#!/usr/bin/env python3
"""Prepare a source-free, no-overwrite Alpha-v2 clean-Mac QA kit.

The kit contains only the reviewed collector, an identity-bound XML plan,
operator instructions, and a manifest.  It never copies the candidate archive
or modifies release status.  Inputs are read without following symlinks and
revalidated after publication; ordinary failures receive best-effort rollback.
"""

import argparse
import datetime
from dataclasses import dataclass
import hashlib
import ipaddress
import json
import os
from pathlib import Path, PurePosixPath
import plistlib
import re
import secrets
import stat
import sys
from typing import Any, Dict, List, Mapping, Optional, Sequence, Tuple
from urllib.parse import unquote, urlsplit


STATUS_SCHEMA = "tanks3d-release-status-v1"
REQUIREMENTS_RELATIVE = Path("docs/release-requirements/macos-alpha-v2.json")
REQUIREMENTS_REFERENCE = REQUIREMENTS_RELATIVE.as_posix()
REQUIREMENTS_SCHEMA = "tanks3d-release-requirements-v2"
REQUIREMENTS_PROFILE = "macos-alpha-v2"
COLLECTOR_RELATIVE = Path("scripts/collect_alpha_v2_clean_mac_qa.sh")
PLAN_SCHEMA = "tanks3d-clean-mac-plan-v1"
MANIFEST_SCHEMA = "tanks3d-clean-mac-kit-manifest-v1"
PLAN_FILENAME = "clean-mac-plan.plist"
COLLECTOR_FILENAME = "START_HERE.command"
README_FILENAME = "README.txt"
MANIFEST_FILENAME = "kit-manifest.json"
KIT_CONTENT_FILENAMES = (
    COLLECTOR_FILENAME,
    PLAN_FILENAME,
    README_FILENAME,
)
PLAN_KEYS = (
    "schema",
    "requirements_profile",
    "candidate_tag",
    "candidate_filename",
    "candidate_sha256",
    "download_url",
    "minimum_macos_version",
    "collector_sha256",
    "prepared_at_utc",
    "session_nonce",
)
STATUS_ROOT_KEYS = {
    "schema",
    "requirements",
    "release",
    "report",
    "documents",
    "gameplay",
    "bases",
    "pickups",
    "settlement",
    "published_controls",
    "clean_mac",
    "gatekeeper",
    "extended_session",
    "evidence",
    "known_issues",
    "audio",
    "approvals",
}
RELEASE_KEYS = {
    "version",
    "channel",
    "tag",
    "source_commit",
    "candidate_dir",
    "artifact",
    "checksum",
    "attestation",
    "gate_log",
    "build_config",
}
FILE_REFERENCE_KEYS = {"path", "sha256"}
VERSION_RE = re.compile(
    r"^(?:0|[1-9][0-9]*)\.(?:0|[1-9][0-9]*)\.(?:0|[1-9][0-9]*)$"
)
CHANNEL_RE = re.compile(r"^alpha\.(?:0|[1-9][0-9]*)$")
COMMIT_RE = re.compile(r"^(?:[0-9a-f]{40}|[0-9a-f]{64})$")
SHA256_RE = re.compile(r"^[0-9a-f]{64}$")
NONCE_RE = re.compile(r"^[0-9a-f]{32}$")
DNS_LABEL_RE = re.compile(
    r"^[A-Za-z0-9](?:[A-Za-z0-9-]{0,61}[A-Za-z0-9])?$"
)
LEGACY_NUMERIC_HOST_RE = re.compile(
    r"^(?:0[xX][0-9A-Fa-f]+|[0-9]+)"
    r"(?:\.(?:0[xX][0-9A-Fa-f]+|[0-9]+))*$"
)
MAX_STATUS_BYTES = 32 * 1024 * 1024
MAX_PROFILE_BYTES = 4 * 1024 * 1024
MAX_COLLECTOR_BYTES = 4 * 1024 * 1024


class PrepareError(Exception):
    """The clean-Mac QA kit cannot be prepared safely."""


@dataclass(frozen=True)
class FileSnapshot:
    path: Path
    device: int
    inode: int
    size: int
    mtime_ns: int
    ctime_ns: int
    sha256: str
    data: Optional[bytes]

    @property
    def identity(self) -> Tuple[int, int, int, int, int, str]:
        return (
            self.device,
            self.inode,
            self.size,
            self.mtime_ns,
            self.ctime_ns,
            self.sha256,
        )


@dataclass(frozen=True)
class DirectorySnapshot:
    path: Path
    device: int
    inode: int
    names: Tuple[str, ...]


@dataclass(frozen=True)
class CandidateIdentity:
    tag: str
    filename: str
    sha256: str
    minimum_macos_version: str
    directory: Path
    directory_snapshot: DirectorySnapshot
    archive_snapshot: FileSnapshot


@dataclass(frozen=True)
class InputState:
    status: FileSnapshot
    profile: FileSnapshot
    collector: FileSnapshot
    candidate: CandidateIdentity


def reject_duplicate_pairs(pairs: Sequence[Tuple[str, Any]]) -> Dict[str, Any]:
    result: Dict[str, Any] = {}
    for key, value in pairs:
        if key in result:
            raise PrepareError("duplicate JSON key: {!r}".format(key))
        result[key] = value
    return result


def require_object(value: Any, label: str) -> Mapping[str, Any]:
    if not isinstance(value, dict):
        raise PrepareError("{} must be an object".format(label))
    return value


def require_exact_keys(
    value: Mapping[str, Any], expected: Sequence[str], label: str
) -> None:
    actual = set(value)
    expected_set = set(expected)
    if actual != expected_set:
        missing = sorted(expected_set - actual)
        extra = sorted(actual - expected_set)
        details = []
        if missing:
            details.append("missing {}".format(", ".join(missing)))
        if extra:
            details.append("unexpected {}".format(", ".join(extra)))
        raise PrepareError(
            "{} has invalid keys ({})".format(label, "; ".join(details))
        )


def require_string(value: Any, label: str) -> str:
    if not isinstance(value, str) or not value:
        raise PrepareError("{} must be a non-empty string".format(label))
    return value


def require_sha256(value: Any, label: str) -> str:
    digest = require_string(value, label)
    if SHA256_RE.fullmatch(digest) is None:
        raise PrepareError("{} must be a lowercase SHA-256 digest".format(label))
    return digest


def parse_json(snapshot: FileSnapshot, label: str) -> Any:
    if snapshot.data is None:
        raise PrepareError("internal error: {} was not retained".format(label))
    try:
        return json.loads(
            snapshot.data.decode("utf-8"), object_pairs_hook=reject_duplicate_pairs
        )
    except (UnicodeError, json.JSONDecodeError) as exc:
        raise PrepareError("{} is not valid JSON: {}".format(label, exc))


def utc_now() -> str:
    return datetime.datetime.now(datetime.timezone.utc).strftime(
        "%Y-%m-%dT%H:%M:%SZ"
    )


def require_real_root(path: Path) -> Path:
    absolute = Path(os.path.abspath(str(path)))
    try:
        metadata = os.lstat(absolute)
        resolved = absolute.resolve(strict=True)
    except OSError as exc:
        raise PrepareError("project root is not accessible: {}".format(exc))
    if stat.S_ISLNK(metadata.st_mode) or not stat.S_ISDIR(metadata.st_mode):
        raise PrepareError("project root must be a real directory")
    if resolved != absolute:
        raise PrepareError("project root must resolve to itself")
    return resolved


def require_no_symlink_components(root: Path, path: Path, label: str) -> None:
    try:
        relative = path.relative_to(root)
    except ValueError:
        raise PrepareError("{} is outside the project root".format(label))
    current = root
    for part in relative.parts:
        current = current / part
        try:
            metadata = os.lstat(current)
        except OSError as exc:
            raise PrepareError("{} is not accessible: {}".format(label, exc))
        if stat.S_ISLNK(metadata.st_mode):
            raise PrepareError("{} has a symlinked path component".format(label))


def argument_inside_root(root: Path, argument: Path, label: str) -> Path:
    candidate = argument if argument.is_absolute() else root / argument
    absolute = Path(os.path.abspath(str(candidate)))
    try:
        absolute.relative_to(root)
    except ValueError:
        raise PrepareError("{} must be inside the project root".format(label))
    require_no_symlink_components(root, absolute, label)
    return absolute


def snapshot_regular_file(
    path: Path,
    label: str,
    *,
    retain: bool,
    maximum_bytes: Optional[int] = None,
) -> FileSnapshot:
    no_follow = getattr(os, "O_NOFOLLOW", None)
    if no_follow is None:
        raise PrepareError("O_NOFOLLOW is required to read {}".format(label))
    descriptor = -1
    try:
        descriptor = os.open(
            str(path),
            os.O_RDONLY
            | getattr(os, "O_NONBLOCK", 0)
            | getattr(os, "O_CLOEXEC", 0)
            | no_follow,
        )
        before = os.fstat(descriptor)
        if not stat.S_ISREG(before.st_mode):
            raise PrepareError("{} must be a regular non-symlink file".format(label))
        if maximum_bytes is not None and before.st_size > maximum_bytes:
            raise PrepareError("{} is unexpectedly large".format(label))
        digest = hashlib.sha256()
        chunks: Optional[List[bytes]] = [] if retain else None
        consumed = 0
        while True:
            chunk = os.read(descriptor, 1024 * 1024)
            if not chunk:
                break
            consumed += len(chunk)
            digest.update(chunk)
            if chunks is not None:
                chunks.append(chunk)
        after = os.fstat(descriptor)
        before_identity = (
            before.st_dev,
            before.st_ino,
            before.st_size,
            before.st_mtime_ns,
            before.st_ctime_ns,
        )
        after_identity = (
            after.st_dev,
            after.st_ino,
            after.st_size,
            after.st_mtime_ns,
            after.st_ctime_ns,
        )
        if before_identity != after_identity or consumed != after.st_size:
            raise PrepareError("{} changed while it was read".format(label))
        return FileSnapshot(
            path=path,
            device=after.st_dev,
            inode=after.st_ino,
            size=after.st_size,
            mtime_ns=after.st_mtime_ns,
            ctime_ns=after.st_ctime_ns,
            sha256=digest.hexdigest(),
            data=b"".join(chunks) if chunks is not None else None,
        )
    except PrepareError:
        raise
    except OSError as exc:
        raise PrepareError("cannot read {}: {}".format(label, exc))
    finally:
        if descriptor >= 0:
            os.close(descriptor)


def snapshot_directory(path: Path, label: str) -> DirectorySnapshot:
    no_follow = getattr(os, "O_NOFOLLOW", None)
    if no_follow is None:
        raise PrepareError("O_NOFOLLOW is required to inspect {}".format(label))
    descriptor = -1
    try:
        descriptor = os.open(
            str(path),
            os.O_RDONLY
            | getattr(os, "O_DIRECTORY", 0)
            | getattr(os, "O_CLOEXEC", 0)
            | no_follow,
        )
        before = os.fstat(descriptor)
        if not stat.S_ISDIR(before.st_mode):
            raise PrepareError("{} must be a real directory".format(label))
        names = tuple(sorted(os.listdir(descriptor)))
        after = os.fstat(descriptor)
        if (before.st_dev, before.st_ino) != (after.st_dev, after.st_ino):
            raise PrepareError("{} changed while it was inspected".format(label))
        return DirectorySnapshot(path, after.st_dev, after.st_ino, names)
    except PrepareError:
        raise
    except OSError as exc:
        raise PrepareError("cannot inspect {}: {}".format(label, exc))
    finally:
        if descriptor >= 0:
            os.close(descriptor)


def normalized_repository_path(value: Any, label: str) -> str:
    text = require_string(value, label)
    pure = PurePosixPath(text)
    if (
        pure.is_absolute()
        or text != pure.as_posix()
        or "\\" in text
        or any(part in {"", ".", ".."} for part in pure.parts)
    ):
        raise PrepareError(
            "{} must be a normalized repository-relative path".format(label)
        )
    return text


def validate_file_reference(value: Any, label: str) -> Tuple[str, str]:
    reference = require_object(value, label)
    require_exact_keys(reference, FILE_REFERENCE_KEYS, label)
    path = normalized_repository_path(reference["path"], label + ".path")
    digest = require_sha256(reference["sha256"], label + ".sha256")
    return path, digest


def validate_status(
    root: Path, snapshot: FileSnapshot, candidate_argument: Path
) -> CandidateIdentity:
    status = require_object(parse_json(snapshot, "release status"), "release status")
    require_exact_keys(status, STATUS_ROOT_KEYS, "release status")
    if status.get("schema") != STATUS_SCHEMA:
        raise PrepareError("release status has the wrong schema")
    if status.get("requirements") != REQUIREMENTS_REFERENCE:
        raise PrepareError("release status must use the macos-alpha-v2 profile")

    release = require_object(status.get("release"), "release status.release")
    require_exact_keys(release, RELEASE_KEYS, "release status.release")
    version = require_string(release["version"], "release status.release.version")
    channel = require_string(release["channel"], "release status.release.channel")
    tag = require_string(release["tag"], "release status.release.tag")
    commit = require_string(
        release["source_commit"], "release status.release.source_commit"
    )
    if VERSION_RE.fullmatch(version) is None:
        raise PrepareError("release version is not canonical")
    if CHANNEL_RE.fullmatch(channel) is None:
        raise PrepareError("release channel is not canonical")
    if tag != "v{}-{}".format(version, channel):
        raise PrepareError("release tag does not match version and channel")
    if COMMIT_RE.fullmatch(commit) is None:
        raise PrepareError("release source_commit is not a full lowercase object ID")

    expected_candidate_relative = "build/release/{}".format(tag)
    candidate_relative = normalized_repository_path(
        release["candidate_dir"], "release status.release.candidate_dir"
    )
    if candidate_relative != expected_candidate_relative:
        raise PrepareError("release candidate_dir does not match the tag")
    expected_candidate = root.joinpath(*PurePosixPath(candidate_relative).parts)
    if candidate_argument != expected_candidate:
        raise PrepareError("candidate directory does not match release status")
    directory_snapshot = snapshot_directory(candidate_argument, "candidate directory")

    references: Dict[str, Tuple[str, str]] = {}
    for key in ("artifact", "checksum", "attestation", "gate_log", "build_config"):
        references[key] = validate_file_reference(
            release[key], "release status.release.{}".format(key)
        )
    artifact_path, artifact_sha256 = references["artifact"]
    artifact_pure = PurePosixPath(artifact_path)
    if artifact_pure.parent.as_posix() != candidate_relative:
        raise PrepareError("release artifact is outside candidate_dir")
    filename = artifact_pure.name

    filename_re = re.compile(
        r"^Tanks3D-{}-{}-macos-[A-Za-z0-9._-]+-macos"
        r"(?P<minimum>\d+(?:\.\d+)*)\.zip$".format(
            re.escape(version), re.escape(channel)
        )
    )
    filename_match = filename_re.fullmatch(filename)
    if filename_match is None:
        raise PrepareError("candidate filename does not match release identity")
    minimum_macos = filename_match.group("minimum")
    if any(
        component != "0" and component.startswith("0")
        for component in minimum_macos.split(".")
    ):
        raise PrepareError("candidate minimum macOS version is not canonical")

    zip_names = [name for name in directory_snapshot.names if name.endswith(".zip")]
    if zip_names != [filename]:
        raise PrepareError(
            "candidate directory must contain exactly the status-bound ZIP"
        )
    archive_path = candidate_argument / filename
    require_no_symlink_components(root, archive_path, "candidate archive")
    archive_snapshot = snapshot_regular_file(
        archive_path, "candidate archive", retain=False
    )
    if archive_snapshot.sha256 != artifact_sha256:
        raise PrepareError("candidate archive SHA-256 does not match release status")

    expected_reference_names = {
        "checksum": filename + ".sha256",
        "attestation": "attestation.txt",
        "gate_log": "alpha-candidate-gates.log",
        "build_config": "build-config.txt",
    }
    for key, expected_name in expected_reference_names.items():
        reference_path = PurePosixPath(references[key][0])
        if (
            reference_path.parent.as_posix() != candidate_relative
            or reference_path.name != expected_name
        ):
            raise PrepareError(
                "release {} reference is not canonical".format(key)
            )

    return CandidateIdentity(
        tag=tag,
        filename=filename,
        sha256=artifact_sha256,
        minimum_macos_version=minimum_macos,
        directory=candidate_argument,
        directory_snapshot=directory_snapshot,
        archive_snapshot=archive_snapshot,
    )


def validate_profile(snapshot: FileSnapshot, collector_sha256: str) -> None:
    profile = require_object(
        parse_json(snapshot, "requirements profile"), "requirements profile"
    )
    if profile.get("schema") != REQUIREMENTS_SCHEMA:
        raise PrepareError("requirements profile has the wrong schema")
    if profile.get("profile") != REQUIREMENTS_PROFILE:
        raise PrepareError("requirements profile is not macos-alpha-v2")
    if profile.get("clean_mac_plan_schema") != PLAN_SCHEMA:
        raise PrepareError("requirements profile clean-Mac plan schema does not match")
    if profile.get("clean_mac_download_client") != "Safari":
        raise PrepareError("requirements profile clean-Mac client must be Safari")
    expected = require_sha256(
        profile.get("clean_mac_collector_sha256"),
        "requirements profile.clean_mac_collector_sha256",
    )
    if expected != collector_sha256:
        raise PrepareError(
            "requirements profile clean-Mac collector SHA-256 does not match"
        )


def validate_download_url(value: str, candidate_filename: str) -> str:
    if value != value.strip() or not value:
        raise PrepareError("download URL must be a non-empty unpadded string")
    try:
        parsed = urlsplit(value)
        hostname = parsed.hostname or ""
        port = parsed.port
    except ValueError as exc:
        raise PrepareError("download URL is malformed: {}".format(exc))
    try:
        hostname.encode("ascii")
    except UnicodeEncodeError:
        raise PrepareError("download URL host must be ASCII")
    host = hostname.lower()
    labels = host.split(".")
    try:
        address = ipaddress.ip_address(host)
    except ValueError:
        address = None
    forbidden_exact = {
        "example",
        "example.com",
        "example.net",
        "example.org",
        "localhost",
    }
    forbidden_suffixes = (
        ".example",
        ".example.com",
        ".example.net",
        ".example.org",
        ".test",
        ".invalid",
        ".localhost",
        ".local",
        ".internal",
        ".lan",
        ".onion",
    )
    if (
        parsed.scheme != "https"
        or not host
        or hostname != host
        or host.endswith(".")
        or len(host) > 253
        or len(labels) < 2
        or any(DNS_LABEL_RE.fullmatch(label) is None for label in labels)
        or port not in (None, 443)
        or address is not None
        or LEGACY_NUMERIC_HOST_RE.fullmatch(host) is not None
        or host in forbidden_exact
        or host.endswith(forbidden_suffixes)
        or any(word in host for word in ("fixture", "placeholder"))
        or parsed.username is not None
        or parsed.password is not None
        or "?" in value
        or "#" in value
    ):
        raise PrepareError(
            "download URL must be a public ASCII HTTPS candidate URL on port 443 "
            "without credentials, query, or fragment"
        )
    encoded_basename = PurePosixPath(parsed.path).name
    if re.search(r"%(?![0-9A-Fa-f]{2})", encoded_basename):
        raise PrepareError("download URL has malformed percent encoding")
    try:
        decoded_basename = unquote(encoded_basename, encoding="utf-8", errors="strict")
    except UnicodeError as exc:
        raise PrepareError("download URL filename is not valid UTF-8: {}".format(exc))
    if decoded_basename != candidate_filename:
        raise PrepareError(
            "download URL basename does not match the candidate filename"
        )
    return value


def read_input_state(
    root: Path, candidate_dir: Path, status_file: Path
) -> InputState:
    status_snapshot = snapshot_regular_file(
        status_file,
        "release status",
        retain=True,
        maximum_bytes=MAX_STATUS_BYTES,
    )
    candidate = validate_status(root, status_snapshot, candidate_dir)
    profile_path = root / REQUIREMENTS_RELATIVE
    collector_path = root / COLLECTOR_RELATIVE
    require_no_symlink_components(root, profile_path, "requirements profile")
    require_no_symlink_components(root, collector_path, "clean-Mac collector")
    profile_snapshot = snapshot_regular_file(
        profile_path,
        "requirements profile",
        retain=True,
        maximum_bytes=MAX_PROFILE_BYTES,
    )
    collector_snapshot = snapshot_regular_file(
        collector_path,
        "clean-Mac collector",
        retain=True,
        maximum_bytes=MAX_COLLECTOR_BYTES,
    )
    validate_profile(profile_snapshot, collector_snapshot.sha256)
    return InputState(
        status=status_snapshot,
        profile=profile_snapshot,
        collector=collector_snapshot,
        candidate=candidate,
    )


def verify_file_unchanged(snapshot: FileSnapshot, label: str) -> None:
    current = snapshot_regular_file(
        snapshot.path,
        label,
        retain=False,
        maximum_bytes=(
            MAX_STATUS_BYTES
            if label == "release status"
            else MAX_PROFILE_BYTES
            if label == "requirements profile"
            else MAX_COLLECTOR_BYTES
            if label == "clean-Mac collector"
            else None
        ),
    )
    if current.identity != snapshot.identity:
        raise PrepareError("{} changed while the kit was prepared".format(label))


def verify_inputs_unchanged(state: InputState) -> None:
    verify_file_unchanged(state.status, "release status")
    verify_file_unchanged(state.profile, "requirements profile")
    verify_file_unchanged(state.collector, "clean-Mac collector")
    current_directory = snapshot_directory(
        state.candidate.directory, "candidate directory"
    )
    original_directory = state.candidate.directory_snapshot
    if (
        current_directory.device,
        current_directory.inode,
        current_directory.names,
    ) != (
        original_directory.device,
        original_directory.inode,
        original_directory.names,
    ):
        raise PrepareError("candidate directory changed while the kit was prepared")
    verify_file_unchanged(state.candidate.archive_snapshot, "candidate archive")


def render_readme(plan: Mapping[str, str]) -> bytes:
    text = """Tanks3D Alpha-v2 Clean-Mac QA Kit
=====================================

Candidate tag: {candidate_tag}
Candidate file: {candidate_filename}
Expected SHA-256: {candidate_sha256}
Minimum macOS: {minimum_macos_version}
Download URL: {download_url}

This kit intentionally contains neither the candidate ZIP nor a source checkout.
Use it only on the clean Apple-Silicon Mac assigned to source-free QA.

1. Confirm that Tanks3D has never been installed or approved on this account.
2. Move this entire four-file kit to the test Mac without adding the candidate.
3. Before opening Safari or running the collector, start one continuous system
   recording or external-camera recording. It must show the Safari download,
   Finder extraction, first launch, actual Gatekeeper dialogs and launch path,
   and the Tanks3D main menu. A static PNG is not sufficient. Preserve the
   original MOV, MP4, or M4V file; it must be at least 64 KiB and no more
   than 95 MB so it remains acceptable to the repository-side compiler and
   ordinary GitHub Git storage.
4. In Terminal, change to the kit directory and run the collector with one new,
   absolute evidence-output path whose parent already exists, for example:

       ./START_HERE.command /Users/tester/Desktop/tanks3d-clean-mac-evidence

   The output path must not already exist. Follow every prompt exactly.
5. Download only the URL above in Safari. Never use curl, add quarantine
   metadata, remove quarantine metadata, or extract the ZIP in Terminal.
6. Keep the original ZIP. Let Finder/Archive Utility expand it when instructed.
7. Return the untouched collector output and original continuous recording for
   independent repository-side review. The empty COMPLETE marker means only
   that collection finished; it is not a PASS result.

The collector stopping with an error is a blocked QA result, not permission to
edit this plan or manufacture evidence. The kit itself never approves a release.
""".format(**plan)
    return text.encode("utf-8")


def build_kit_contents(
    state: InputState, download_url: str
) -> Dict[str, bytes]:
    if state.collector.data is None:
        raise PrepareError("internal error: collector bytes were not retained")
    prepared_at = utc_now()
    nonce = secrets.token_hex(16)
    if NONCE_RE.fullmatch(nonce) is None:
        raise PrepareError("secure nonce generator did not return 32 lowercase hex digits")
    plan: Dict[str, str] = {
        "schema": PLAN_SCHEMA,
        "requirements_profile": REQUIREMENTS_PROFILE,
        "candidate_tag": state.candidate.tag,
        "candidate_filename": state.candidate.filename,
        "candidate_sha256": state.candidate.sha256,
        "download_url": download_url,
        "minimum_macos_version": state.candidate.minimum_macos_version,
        "collector_sha256": state.collector.sha256,
        "prepared_at_utc": prepared_at,
        "session_nonce": nonce,
    }
    if tuple(plan) != PLAN_KEYS:
        raise PrepareError("internal error: clean-Mac plan keys are not canonical")
    plan_data = plistlib.dumps(plan, fmt=plistlib.FMT_XML, sort_keys=False)
    readme_data = render_readme(plan)
    contents = {
        COLLECTOR_FILENAME: state.collector.data,
        PLAN_FILENAME: plan_data,
        README_FILENAME: readme_data,
    }
    manifest = {
        "schema": MANIFEST_SCHEMA,
        "candidate": {
            "filename": state.candidate.filename,
            "sha256": state.candidate.sha256,
        },
        "files": [
            {
                "path": filename,
                "sha256": hashlib.sha256(contents[filename]).hexdigest(),
            }
            for filename in KIT_CONTENT_FILENAMES
        ],
    }
    contents[MANIFEST_FILENAME] = (
        json.dumps(manifest, indent=2, ensure_ascii=True) + "\n"
    ).encode("utf-8")
    return contents


def open_directory(path: Path, label: str) -> int:
    no_follow = getattr(os, "O_NOFOLLOW", None)
    if no_follow is None:
        raise PrepareError("O_NOFOLLOW is required to open {}".format(label))
    try:
        descriptor = os.open(
            str(path),
            os.O_RDONLY
            | getattr(os, "O_DIRECTORY", 0)
            | getattr(os, "O_CLOEXEC", 0)
            | no_follow,
        )
    except OSError as exc:
        raise PrepareError("cannot open {}: {}".format(label, exc))
    if not stat.S_ISDIR(os.fstat(descriptor).st_mode):
        os.close(descriptor)
        raise PrepareError("{} must be a real directory".format(label))
    return descriptor


def write_file_at(
    directory_fd: int,
    name: str,
    data: bytes,
    mode: int,
    created: List[Tuple[str, int, int]],
) -> None:
    flags = os.O_WRONLY | os.O_CREAT | os.O_EXCL | getattr(os, "O_CLOEXEC", 0)
    if hasattr(os, "O_NOFOLLOW"):
        flags |= os.O_NOFOLLOW
    descriptor = -1
    metadata: Optional[os.stat_result] = None
    try:
        descriptor = os.open(name, flags, mode, dir_fd=directory_fd)
        metadata = os.fstat(descriptor)
        created.append((name, metadata.st_dev, metadata.st_ino))
        view = memoryview(data)
        while view:
            written = os.write(descriptor, view)
            if written <= 0:
                raise PrepareError("short write while publishing {}".format(name))
            view = view[written:]
        os.fchmod(descriptor, mode)
        os.fsync(descriptor)
    except PrepareError:
        raise
    except OSError as exc:
        raise PrepareError("cannot publish {}: {}".format(name, exc))
    finally:
        if descriptor >= 0:
            os.close(descriptor)


def read_file_at(directory_fd: int, name: str, label: str) -> bytes:
    no_follow = getattr(os, "O_NOFOLLOW", None)
    if no_follow is None:
        raise PrepareError("O_NOFOLLOW is required to verify {}".format(label))
    descriptor = -1
    try:
        descriptor = os.open(
            name,
            os.O_RDONLY
            | getattr(os, "O_NONBLOCK", 0)
            | getattr(os, "O_CLOEXEC", 0)
            | no_follow,
            dir_fd=directory_fd,
        )
        before = os.fstat(descriptor)
        if not stat.S_ISREG(before.st_mode):
            raise PrepareError("{} is not a regular file".format(label))
        chunks = []
        while True:
            chunk = os.read(descriptor, 1024 * 1024)
            if not chunk:
                break
            chunks.append(chunk)
        after = os.fstat(descriptor)
        if (
            before.st_dev,
            before.st_ino,
            before.st_size,
            before.st_mtime_ns,
            before.st_ctime_ns,
        ) != (
            after.st_dev,
            after.st_ino,
            after.st_size,
            after.st_mtime_ns,
            after.st_ctime_ns,
        ):
            raise PrepareError("{} changed while it was verified".format(label))
        data = b"".join(chunks)
        if len(data) != after.st_size:
            raise PrepareError("{} changed size while it was verified".format(label))
        return data
    except PrepareError:
        raise
    except OSError as exc:
        raise PrepareError("cannot verify {}: {}".format(label, exc))
    finally:
        if descriptor >= 0:
            os.close(descriptor)


def rollback_files(
    directory_fd: int, created: Sequence[Tuple[str, int, int]]
) -> List[str]:
    errors = []
    for name, device, inode in reversed(created):
        try:
            current = os.stat(name, dir_fd=directory_fd, follow_symlinks=False)
            if (
                not stat.S_ISREG(current.st_mode)
                or (current.st_dev, current.st_ino) != (device, inode)
            ):
                errors.append("{} no longer identifies the published file".format(name))
                continue
            os.unlink(name, dir_fd=directory_fd)
        except FileNotFoundError:
            continue
        except OSError as exc:
            errors.append("cannot remove {}: {}".format(name, exc))
    try:
        os.fsync(directory_fd)
    except OSError as exc:
        errors.append("cannot fsync rollback directory: {}".format(exc))
    return errors


def publish_kit(
    output_dir: Path,
    contents: Mapping[str, bytes],
    final_check: Any,
) -> None:
    absolute_output = Path(os.path.abspath(str(output_dir)))
    parent = absolute_output.parent
    try:
        parent = parent.resolve(strict=True)
    except OSError as exc:
        raise PrepareError("output parent is not accessible: {}".format(exc))
    output_name = absolute_output.name
    if not output_name or output_name in {".", ".."}:
        raise PrepareError("output directory name is invalid")
    parent_fd = open_directory(parent, "output parent")
    directory_fd = -1
    created_directory = False
    directory_identity: Optional[Tuple[int, int]] = None
    created_files: List[Tuple[str, int, int]] = []
    caught: Optional[BaseException] = None
    cleanup_errors: List[str] = []
    try:
        try:
            os.mkdir(output_name, 0o700, dir_fd=parent_fd)
        except FileExistsError:
            raise PrepareError("output directory already exists")
        created_directory = True
        created_metadata = os.stat(
            output_name, dir_fd=parent_fd, follow_symlinks=False
        )
        if not stat.S_ISDIR(created_metadata.st_mode):
            raise PrepareError("created output path is not a directory")
        directory_identity = (created_metadata.st_dev, created_metadata.st_ino)
        no_follow = getattr(os, "O_NOFOLLOW", None)
        if no_follow is None:
            raise PrepareError("O_NOFOLLOW is required to publish the kit")
        directory_fd = os.open(
            output_name,
            os.O_RDONLY
            | getattr(os, "O_DIRECTORY", 0)
            | getattr(os, "O_CLOEXEC", 0)
            | no_follow,
            dir_fd=parent_fd,
        )
        metadata = os.fstat(directory_fd)
        if (metadata.st_dev, metadata.st_ino) != directory_identity:
            raise PrepareError("created output directory changed before opening")
        os.fchmod(directory_fd, 0o700)
        for filename in (
            COLLECTOR_FILENAME,
            PLAN_FILENAME,
            README_FILENAME,
            MANIFEST_FILENAME,
        ):
            write_file_at(
                directory_fd,
                filename,
                contents[filename],
                0o700 if filename == COLLECTOR_FILENAME else 0o600,
                created_files,
            )
        os.fsync(directory_fd)
        os.fsync(parent_fd)
        for filename, expected in contents.items():
            if read_file_at(
                directory_fd, filename, "published {}".format(filename)
            ) != expected:
                raise PrepareError("published {} changed".format(filename))
        final_check()
        current = os.stat(output_name, dir_fd=parent_fd, follow_symlinks=False)
        if (
            not stat.S_ISDIR(current.st_mode)
            or (current.st_dev, current.st_ino) != directory_identity
            or stat.S_IMODE(current.st_mode) != 0o700
        ):
            raise PrepareError("published kit directory identity or mode changed")
    except BaseException as exc:
        caught = exc
        if directory_fd >= 0:
            cleanup_errors.extend(rollback_files(directory_fd, created_files))
        if created_directory and directory_identity is not None:
            try:
                current = os.stat(
                    output_name, dir_fd=parent_fd, follow_symlinks=False
                )
                if (
                    stat.S_ISDIR(current.st_mode)
                    and (current.st_dev, current.st_ino) == directory_identity
                ):
                    os.rmdir(output_name, dir_fd=parent_fd)
                else:
                    cleanup_errors.append(
                        "output directory no longer identifies this invocation"
                    )
            except FileNotFoundError:
                pass
            except OSError as cleanup_exc:
                cleanup_errors.append(
                    "cannot remove output directory: {}".format(cleanup_exc)
                )
        try:
            os.fsync(parent_fd)
        except OSError as cleanup_exc:
            cleanup_errors.append(
                "cannot fsync output parent after rollback: {}".format(cleanup_exc)
            )
    finally:
        if directory_fd >= 0:
            os.close(directory_fd)
        os.close(parent_fd)
    if caught is not None:
        if cleanup_errors:
            raise PrepareError(
                "kit publication failed and rollback is incomplete ({}); original "
                "error: {}".format("; ".join(cleanup_errors), caught)
            ) from caught
        if isinstance(caught, PrepareError):
            raise caught
        raise PrepareError("kit publication failed: {}".format(caught)) from caught


def prepare(
    project_root: Path,
    candidate_dir: Path,
    status_file: Path,
    download_url: str,
    output_dir: Path,
) -> Path:
    root = require_real_root(project_root)
    candidate = argument_inside_root(root, candidate_dir, "candidate directory")
    status = argument_inside_root(root, status_file, "release status")
    state = read_input_state(root, candidate, status)
    canonical_url = validate_download_url(download_url, state.candidate.filename)
    contents = build_kit_contents(state, canonical_url)
    publish_kit(output_dir, contents, lambda: verify_inputs_unchanged(state))
    return Path(os.path.abspath(str(output_dir)))


def parse_arguments(argv: Optional[Sequence[str]] = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--project-root", required=True, type=Path)
    parser.add_argument("--candidate-dir", required=True, type=Path)
    parser.add_argument("--status-file", required=True, type=Path)
    parser.add_argument("--download-url", required=True)
    parser.add_argument("--output-dir", required=True, type=Path)
    return parser.parse_args(argv)


def main(argv: Optional[Sequence[str]] = None) -> int:
    arguments = parse_arguments(argv)
    try:
        output = prepare(
            arguments.project_root,
            arguments.candidate_dir,
            arguments.status_file,
            arguments.download_url,
            arguments.output_dir,
        )
    except PrepareError as exc:
        print("Alpha-v2 clean-Mac QA kit preparation failed: {}".format(exc), file=sys.stderr)
        return 1
    print("Prepared source-free Alpha-v2 clean-Mac QA kit: {}".format(output))
    print("Transfer only this four-file kit to the clean test Mac; it is not approval.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
