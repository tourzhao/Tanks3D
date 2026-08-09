#!/usr/bin/env python3
"""Run candidate-generated macOS performance QA and publish an audit receipt.

This runner never fabricates telemetry.  It verifies the tagged candidate,
launches the executable extracted from that candidate, and publishes a receipt
only after the candidate exits successfully and its identity-bound v2 telemetry
and stdout markers validate.  The output directory must be an existing, empty,
owner-private directory so every output is created with no-replace semantics.
"""

import argparse
import datetime
import errno
import hashlib
import json
import os
from pathlib import Path
import re
import secrets
import stat
import subprocess
import sys
import tempfile
from dataclasses import dataclass
from typing import Any, Dict, Mapping, Optional, Sequence, Tuple


SCRIPT_DIRECTORY = Path(__file__).resolve().parent
if str(SCRIPT_DIRECTORY) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIRECTORY))

import release_performance_contract as performance_contract  # noqa: E402


TELEMETRY_SCHEMA = performance_contract.PERFORMANCE_LOG_V2_SCHEMA
RECEIPT_SCHEMA = "tanks3d-performance-qa-receipt-v1"
TELEMETRY_FILENAME = "performance-log-v2.json"
STDOUT_FILENAME = "performance-stdout.log"
STDERR_FILENAME = "performance-stderr.log"
RECEIPT_FILENAME = "performance-qa-receipt.json"
START_MARKER = performance_contract.PERFORMANCE_V2_START_MARKER
COMPLETE_MARKER = performance_contract.PERFORMANCE_V2_COMPLETE_MARKER
CAPABILITY_ARGUMENT = "--self-test=release-performance-capabilities"
CAPABILITY_SCHEMA = "tanks3d-release-performance-capabilities-v1"
CAPABILITY_CONTRACT_SHA256 = (
    "5137950da46fa11ee6d5ff60fafe67e83c4c0aacfb5fc83f2b0ce5f74afcfe1c"
)
CAPABILITY_TIMEOUT_SECONDS = 5
PROCESS_TIMEOUT_GRACE_SECONDS = 120
PROCESS_TERMINATE_GRACE_SECONDS = 5
CANDIDATE_ATTESTATION_SCHEMA = "tanks3d-alpha-candidate-v3"
DEFAULT_DURATION_SECONDS = 1801
MAX_DURATION_SECONDS = performance_contract.PERFORMANCE_V2_MAXIMUM_DURATION_SECONDS
MAX_TELEMETRY_BYTES = performance_contract.MAX_TELEMETRY_BYTES
MAX_MARKER_LOG_BYTES = 16 * 1024 * 1024
SHA256_RE = re.compile(r"^[0-9a-f]{64}$")
COMMIT_RE = re.compile(r"^(?:[0-9a-f]{40}|[0-9a-f]{64})$")
TAG_RE = re.compile(r"^[A-Za-z0-9][A-Za-z0-9._-]*$")

RECEIPT_KEYS = {
    "schema",
    "candidate_filename",
    "candidate_sha256",
    "executable_sha256",
    "source_commit",
    "source_tag",
    "session_nonce",
    "argv",
    "pid",
    "started_at_utc",
    "completed_at_utc",
    "exit_code",
    "telemetry",
    "stdout",
    "stderr",
}
FILE_REFERENCE_KEYS = {"path", "sha256"}
PERFORMANCE_BUILD_CONFIG = {
    "performance-capability-schema": CAPABILITY_SCHEMA,
    "performance-telemetry-schema": TELEMETRY_SCHEMA,
    "performance-capability-contract-sha256": CAPABILITY_CONTRACT_SHA256,
}


class RunnerError(Exception):
    """A performance QA precondition or candidate contract failed."""


@dataclass(frozen=True)
class CandidateIdentity:
    candidate_dir: Path
    artifact: Path
    artifact_sha256: str
    source_commit: str
    source_tag: str


def utc_now() -> str:
    return (
        datetime.datetime.now(datetime.timezone.utc)
        .isoformat(timespec="seconds")
        .replace("+00:00", "Z")
    )


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    try:
        with path.open("rb") as stream:
            for chunk in iter(lambda: stream.read(1024 * 1024), b""):
                digest.update(chunk)
    except OSError as exc:
        raise RunnerError("cannot hash {}: {}".format(path, exc))
    return digest.hexdigest()


def open_regular_file_no_follow(
    path: Path,
    label: str,
    flags: int,
    mode: Optional[int] = None,
    directory_fd: Optional[int] = None,
) -> Tuple[int, os.stat_result]:
    no_follow = getattr(os, "O_NOFOLLOW", None)
    if no_follow is None:
        raise RunnerError("O_NOFOLLOW is required to secure the candidate snapshot")
    descriptor = -1
    try:
        open_flags = flags | no_follow | getattr(os, "O_CLOEXEC", 0)
        if mode is None:
            descriptor = (
                os.open(path, open_flags)
                if directory_fd is None
                else os.open(str(path), open_flags, dir_fd=directory_fd)
            )
        else:
            descriptor = (
                os.open(path, open_flags, mode)
                if directory_fd is None
                else os.open(
                    str(path), open_flags, mode, dir_fd=directory_fd
                )
            )
        metadata = os.fstat(descriptor)
        if not stat.S_ISREG(metadata.st_mode):
            raise RunnerError("{} must be a regular file".format(label))
        return descriptor, metadata
    except RunnerError:
        if descriptor >= 0:
            os.close(descriptor)
        raise
    except OSError as exc:
        if descriptor >= 0:
            os.close(descriptor)
        raise RunnerError("cannot securely open {}: {}".format(label, exc))


def sha256_descriptor(descriptor: int, label: str) -> str:
    digest = hashlib.sha256()
    try:
        os.lseek(descriptor, 0, os.SEEK_SET)
        while True:
            chunk = os.read(descriptor, 1024 * 1024)
            if not chunk:
                break
            digest.update(chunk)
    except OSError as exc:
        raise RunnerError("cannot hash {}: {}".format(label, exc))
    return digest.hexdigest()


def read_regular_file_no_follow(
    path: Path,
    label: str,
    directory_fd: Optional[int] = None,
    maximum_bytes: Optional[int] = None,
) -> bytes:
    descriptor = -1
    try:
        descriptor, before = open_regular_file_no_follow(
            path,
            label,
            os.O_RDONLY | getattr(os, "O_NONBLOCK", 0),
            directory_fd=directory_fd,
        )
        if maximum_bytes is not None and before.st_size > maximum_bytes:
            raise RunnerError("{} exceeds the safety limit".format(label))
        total_bytes = 0
        chunks = []
        while total_bytes < before.st_size:
            chunk = os.read(
                descriptor, min(1024 * 1024, before.st_size - total_bytes)
            )
            if not chunk:
                raise RunnerError("{} changed while it was read".format(label))
            total_bytes += len(chunk)
            if maximum_bytes is not None and total_bytes > maximum_bytes:
                raise RunnerError("{} exceeds the safety limit".format(label))
            chunks.append(chunk)
        after = os.fstat(descriptor)
        identity_before = (
            before.st_dev,
            before.st_ino,
            before.st_size,
            before.st_mtime_ns,
            before.st_ctime_ns,
        )
        identity_after = (
            after.st_dev,
            after.st_ino,
            after.st_size,
            after.st_mtime_ns,
            after.st_ctime_ns,
        )
        data = b"".join(chunks)
        if identity_before != identity_after or len(data) != after.st_size:
            raise RunnerError("{} changed while it was read".format(label))
        return data
    except RunnerError:
        raise
    except OSError as exc:
        raise RunnerError("cannot read {}: {}".format(label, exc))
    finally:
        if descriptor >= 0:
            os.close(descriptor)


def sha256_regular_file_no_follow(
    path: Path,
    label: str,
    maximum_bytes: Optional[int] = None,
) -> str:
    descriptor = -1
    try:
        descriptor, before = open_regular_file_no_follow(
            path,
            label,
            os.O_RDONLY | getattr(os, "O_NONBLOCK", 0),
        )
        if maximum_bytes is not None and before.st_size > maximum_bytes:
            raise RunnerError("{} exceeds the safety limit".format(label))
        digest = hashlib.sha256()
        total_bytes = 0
        while total_bytes < before.st_size:
            chunk = os.read(
                descriptor, min(1024 * 1024, before.st_size - total_bytes)
            )
            if not chunk:
                raise RunnerError("{} changed while it was hashed".format(label))
            total_bytes += len(chunk)
            if maximum_bytes is not None and total_bytes > maximum_bytes:
                raise RunnerError("{} exceeds the safety limit".format(label))
            digest.update(chunk)
        after = os.fstat(descriptor)
        identity_before = (
            before.st_dev,
            before.st_ino,
            before.st_size,
            before.st_mtime_ns,
            before.st_ctime_ns,
        )
        identity_after = (
            after.st_dev,
            after.st_ino,
            after.st_size,
            after.st_mtime_ns,
            after.st_ctime_ns,
        )
        if identity_before != identity_after or total_bytes != after.st_size:
            raise RunnerError("{} changed while it was hashed".format(label))
        return digest.hexdigest()
    except RunnerError:
        raise
    except OSError as exc:
        raise RunnerError("cannot hash {}: {}".format(label, exc))
    finally:
        if descriptor >= 0:
            os.close(descriptor)


def snapshot_candidate_archive(
    identity: "CandidateIdentity", destination: Path
) -> int:
    snapshot = destination / "candidate-archive.snapshot.zip"
    source_descriptor = -1
    snapshot_descriptor = -1
    snapshot_unlinked = False
    descriptor_transferred = False
    copied_digest = hashlib.sha256()
    copied_size = 0
    source_before: Optional[os.stat_result] = None
    try:
        source_descriptor, source_before = open_regular_file_no_follow(
            identity.artifact,
            "candidate archive snapshot source",
            os.O_RDONLY | getattr(os, "O_NONBLOCK", 0),
        )
        snapshot_descriptor, snapshot_metadata = open_regular_file_no_follow(
            snapshot,
            "candidate archive snapshot",
            os.O_RDWR | os.O_CREAT | os.O_EXCL,
            0o600,
        )
        if snapshot_metadata.st_nlink != 1:
            raise RunnerError("candidate archive snapshot must have one link")

        while True:
            chunk = os.read(source_descriptor, 1024 * 1024)
            if not chunk:
                break
            copied_digest.update(chunk)
            copied_size += len(chunk)
            offset = 0
            while offset < len(chunk):
                written = os.write(snapshot_descriptor, chunk[offset:])
                if written <= 0:
                    raise RunnerError("candidate archive snapshot write made no progress")
                offset += written

        source_after = os.fstat(source_descriptor)
        stable_fields_before = (
            source_before.st_dev,
            source_before.st_ino,
            source_before.st_size,
            source_before.st_mtime_ns,
            source_before.st_ctime_ns,
        )
        stable_fields_after = (
            source_after.st_dev,
            source_after.st_ino,
            source_after.st_size,
            source_after.st_mtime_ns,
            source_after.st_ctime_ns,
        )
        if stable_fields_before != stable_fields_after or copied_size != source_before.st_size:
            raise RunnerError("candidate archive changed while its snapshot was copied")
        if copied_digest.hexdigest() != identity.artifact_sha256:
            raise RunnerError(
                "candidate archive snapshot digest does not match its attestation"
            )
        os.fchmod(snapshot_descriptor, 0o400)
        os.fsync(snapshot_descriptor)
        os.unlink(snapshot)
        snapshot_unlinked = True
        if os.fstat(snapshot_descriptor).st_nlink != 0:
            raise RunnerError("candidate archive snapshot remained path-addressable")
        if (
            sha256_descriptor(snapshot_descriptor, "candidate archive snapshot")
            != identity.artifact_sha256
        ):
            raise RunnerError(
                "candidate archive snapshot failed its same-descriptor digest check"
            )
        os.lseek(snapshot_descriptor, 0, os.SEEK_SET)
        descriptor_transferred = True
        return snapshot_descriptor
    except RunnerError:
        raise
    except OSError as exc:
        raise RunnerError("cannot copy candidate archive snapshot: {}".format(exc))
    finally:
        if snapshot_descriptor >= 0 and not descriptor_transferred:
            os.close(snapshot_descriptor)
        if source_descriptor >= 0:
            os.close(source_descriptor)
        if not snapshot_unlinked:
            try:
                snapshot.unlink()
            except FileNotFoundError:
                pass
            except OSError:
                pass


def reject_duplicate_pairs(pairs: Sequence[Tuple[str, Any]]) -> Dict[str, Any]:
    result: Dict[str, Any] = {}
    for key, value in pairs:
        if key in result:
            raise RunnerError("duplicate JSON key {!r}".format(key))
        result[key] = value
    return result


def load_strict_json_with_digest(path: Path) -> Tuple[Any, str]:
    data = read_regular_file_no_follow(
        path,
        "candidate performance telemetry",
        maximum_bytes=MAX_TELEMETRY_BYTES,
    )
    if len(data) <= 0:
        raise RunnerError("telemetry is empty")
    try:
        text = data.decode("utf-8")
    except UnicodeError as exc:
        raise RunnerError("cannot read telemetry {}: {}".format(path, exc))
    try:
        value = json.loads(text, object_pairs_hook=reject_duplicate_pairs)
    except RunnerError:
        raise
    except json.JSONDecodeError as exc:
        raise RunnerError("candidate telemetry is invalid JSON: {}".format(exc))
    return value, hashlib.sha256(data).hexdigest()


def load_strict_json(path: Path) -> Any:
    return load_strict_json_with_digest(path)[0]


def require_real_directory(path: Path, label: str) -> Path:
    try:
        metadata = path.lstat()
    except OSError as exc:
        raise RunnerError("{} is not accessible: {}".format(label, exc))
    if stat.S_ISLNK(metadata.st_mode):
        raise RunnerError("{} must not be a symbolic link".format(label))
    if not stat.S_ISDIR(metadata.st_mode):
        raise RunnerError("{} must be a directory".format(label))
    try:
        return path.resolve(strict=True)
    except OSError as exc:
        raise RunnerError("{} cannot be normalized: {}".format(label, exc))


def require_no_symlink_components(root: Path, path: Path, label: str) -> None:
    try:
        relative = path.relative_to(root)
    except ValueError:
        raise RunnerError("{} is outside the project root".format(label))
    current = root
    for part in relative.parts:
        current = current / part
        try:
            metadata = current.lstat()
        except OSError as exc:
            raise RunnerError("{} is not accessible: {}".format(label, exc))
        if stat.S_ISLNK(metadata.st_mode):
            raise RunnerError("{} has a symlinked path component".format(label))


def open_repository_directory_no_follow(root: Path, path: Path, label: str) -> int:
    try:
        parts = path.relative_to(root).parts
    except ValueError:
        raise RunnerError("{} is outside the project root".format(label))
    no_follow = getattr(os, "O_NOFOLLOW", None)
    if no_follow is None:
        raise RunnerError("O_NOFOLLOW is required to open {}".format(label))
    flags = os.O_RDONLY | getattr(os, "O_DIRECTORY", 0) | no_follow
    descriptor = -1
    try:
        descriptor = os.open(root, flags)
        for part in parts:
            next_descriptor = os.open(part, flags, dir_fd=descriptor)
            os.close(descriptor)
            descriptor = next_descriptor
        if not stat.S_ISDIR(os.fstat(descriptor).st_mode):
            raise RunnerError("{} must be a directory".format(label))
        return descriptor
    except RunnerError:
        if descriptor >= 0:
            os.close(descriptor)
        raise
    except OSError as exc:
        if descriptor >= 0:
            os.close(descriptor)
        raise RunnerError("cannot open {} safely: {}".format(label, exc))


def require_private_empty_output(path: Path) -> Path:
    output = require_real_directory(path, "output directory")
    try:
        metadata = output.stat()
    except OSError as exc:
        raise RunnerError("cannot inspect output directory: {}".format(exc))
    if hasattr(os, "getuid") and metadata.st_uid != os.getuid():
        raise RunnerError("output directory must be owned by the current user")
    if stat.S_IMODE(metadata.st_mode) & 0o077:
        raise RunnerError("output directory must be private (mode 0700 or stricter)")
    try:
        with os.scandir(output) as entries:
            if next(entries, None) is not None:
                raise RunnerError("output directory must be empty")
    except OSError as exc:
        raise RunnerError("cannot inspect output directory contents: {}".format(exc))
    return output


def require_regular_non_symlink(path: Path, label: str) -> Path:
    try:
        metadata = path.lstat()
    except OSError as exc:
        raise RunnerError("{} is missing: {}".format(label, exc))
    if stat.S_ISLNK(metadata.st_mode) or not stat.S_ISREG(metadata.st_mode):
        raise RunnerError("{} must be a regular non-symlink file".format(label))
    return path


def parse_attestation(data: bytes) -> Dict[str, str]:
    try:
        lines = data.decode("utf-8").splitlines()
    except UnicodeError as exc:
        raise RunnerError("cannot read attestation: {}".format(exc))
    values: Dict[str, str] = {}
    for line_number, line in enumerate(lines, 1):
        if not line or "=" not in line:
            raise RunnerError("malformed attestation line {}".format(line_number))
        key, value = line.split("=", 1)
        if not key or key in values:
            raise RunnerError("duplicate or empty attestation key on line {}".format(line_number))
        values[key] = value
    required = {
        "schema",
        "source_commit",
        "source_tag",
        "artifact_filename",
        "artifact_sha256",
        "checksum_filename",
        "build_config_filename",
        "build_config_sha256",
        "gate_log_filename",
        "gate_log_sha256",
    }
    missing = sorted(required - set(values))
    if missing:
        raise RunnerError("attestation is missing keys: {}".format(", ".join(missing)))
    if values["schema"] != CANDIDATE_ATTESTATION_SCHEMA:
        raise RunnerError(
            "candidate must use {}".format(CANDIDATE_ATTESTATION_SCHEMA)
        )
    return values


def validate_performance_build_config(data: bytes) -> None:
    try:
        lines = data.decode("utf-8").splitlines()
    except UnicodeError as exc:
        raise RunnerError("cannot read candidate build configuration: {}".format(exc))
    values: Dict[str, str] = {}
    for line_number, line in enumerate(lines, 1):
        if not line or "=" not in line:
            raise RunnerError(
                "malformed candidate build configuration line {}".format(line_number)
            )
        key, value = line.split("=", 1)
        if not key or key in values:
            raise RunnerError(
                "duplicate or empty candidate build configuration key on line {}".format(
                    line_number
                )
            )
        values[key] = value
    for key, expected in PERFORMANCE_BUILD_CONFIG.items():
        if values.get(key) != expected:
            raise RunnerError(
                "candidate build configuration lacks the current {} contract".format(
                    key
                )
            )


def invoke_tagged_verifier(project_root: Path, candidate_dir: Path) -> Dict[str, str]:
    verifier_path = project_root / "scripts" / "verify_tagged_alpha_candidate.sh"
    require_no_symlink_components(
        project_root, verifier_path, "tagged candidate verifier"
    )
    verifier = require_regular_non_symlink(
        verifier_path,
        "tagged candidate verifier",
    )
    verifier_parent_fd = open_repository_directory_no_follow(
        project_root, verifier.parent, "tagged verifier directory"
    )
    try:
        verifier_data = read_regular_file_no_follow(
            Path(verifier.name),
            "tagged candidate verifier",
            directory_fd=verifier_parent_fd,
        )
    finally:
        os.close(verifier_parent_fd)
    try:
        completed = subprocess.run(
            ["sh", "-s", "--", str(project_root), str(candidate_dir)],
            input=verifier_data,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            check=False,
        )
    except OSError as exc:
        raise RunnerError("cannot run tagged candidate verifier: {}".format(exc))
    if completed.returncode != 0:
        diagnostic = (completed.stderr or completed.stdout).decode(
            "utf-8", errors="replace"
        ).strip()
        if len(diagnostic) > 800:
            diagnostic = diagnostic[-800:]
        raise RunnerError(
            "tagged candidate verifier failed (exit {}): {}".format(
                completed.returncode, diagnostic or "no diagnostic"
            )
        )
    try:
        verifier_stdout = completed.stdout.decode("utf-8")
    except UnicodeError as exc:
        raise RunnerError("tagged verifier output is not UTF-8: {}".format(exc))
    prefix = "VERIFIED CANDIDATE FILE SHA256 "
    receipt: Dict[str, str] = {}
    for line in verifier_stdout.splitlines():
        if not line.startswith(prefix):
            continue
        fields = line[len(prefix) :].split(" ", 1)
        if len(fields) != 2:
            raise RunnerError("tagged verifier emitted a malformed candidate receipt")
        digest, name = fields
        if SHA256_RE.fullmatch(digest) is None or TAG_RE.fullmatch(name) is None:
            raise RunnerError("tagged verifier emitted an invalid candidate receipt")
        if name in receipt:
            raise RunnerError("tagged verifier emitted a duplicate candidate receipt")
        receipt[name] = digest
    if len(receipt) != 5:
        raise RunnerError("tagged verifier did not emit an exact five-file receipt")
    return receipt


def parse_candidate(project_root: Path, candidate_argument: Path) -> CandidateIdentity:
    candidate_dir = require_real_directory(candidate_argument, "candidate directory")
    try:
        candidate_dir.relative_to(project_root)
    except ValueError:
        raise RunnerError("candidate directory must be inside the project root")
    verified_receipt = invoke_tagged_verifier(project_root, candidate_dir)
    candidate_fd = open_repository_directory_no_follow(
        project_root, candidate_dir, "candidate directory"
    )
    try:
        try:
            actual_names = os.listdir(candidate_fd)
        except OSError as exc:
            raise RunnerError("cannot enumerate candidate directory: {}".format(exc))
        if len(actual_names) != 5 or len(set(actual_names)) != 5:
            raise RunnerError("candidate directory must contain exactly five files")
        captured = {
            name: read_regular_file_no_follow(
                Path(name),
                "candidate file {!r}".format(name),
                directory_fd=candidate_fd,
            )
            for name in actual_names
        }
    finally:
        os.close(candidate_fd)
    if "attestation.txt" not in captured:
        raise RunnerError("candidate attestation is missing")
    values = parse_attestation(captured["attestation.txt"])
    source_commit = values["source_commit"]
    source_tag = values["source_tag"]
    artifact_sha256 = values["artifact_sha256"]
    if COMMIT_RE.fullmatch(source_commit) is None:
        raise RunnerError("attested source commit is not a full object ID")
    if TAG_RE.fullmatch(source_tag) is None:
        raise RunnerError("attested source tag contains unsafe characters")
    if SHA256_RE.fullmatch(artifact_sha256) is None:
        raise RunnerError("attested artifact digest is not SHA-256")
    expected_candidate = project_root / "build" / "release" / source_tag
    if candidate_dir != expected_candidate:
        raise RunnerError("candidate directory does not match its source tag")

    filenames = {
        "attestation.txt",
        values["artifact_filename"],
        values["checksum_filename"],
        values["build_config_filename"],
        values["gate_log_filename"],
    }
    if len(filenames) != 5:
        raise RunnerError("candidate attestation assigns duplicate filenames")
    if set(captured) != filenames:
        raise RunnerError("candidate directory must contain exactly its five attested files")
    captured_receipt = {
        name: hashlib.sha256(data).hexdigest() for name, data in captured.items()
    }
    if verified_receipt != captured_receipt:
        raise RunnerError(
            "candidate does not match the tagged verifier's private snapshot"
        )

    artifact = candidate_dir / values["artifact_filename"]
    if hashlib.sha256(captured[artifact.name]).hexdigest() != artifact_sha256:
        raise RunnerError("candidate archive digest does not match its attestation")
    build_config_name = values["build_config_filename"]
    gate_log_name = values["gate_log_filename"]
    if hashlib.sha256(captured[build_config_name]).hexdigest() != values[
        "build_config_sha256"
    ]:
        raise RunnerError("build configuration digest does not match its attestation")
    if hashlib.sha256(captured[gate_log_name]).hexdigest() != values["gate_log_sha256"]:
        raise RunnerError("gate log digest does not match its attestation")
    validate_performance_build_config(captured[build_config_name])
    try:
        checksum_text = captured[values["checksum_filename"]].decode("utf-8")
    except UnicodeError as exc:
        raise RunnerError("cannot read candidate checksum: {}".format(exc))
    expected_checksum = "{}  {}\n".format(artifact_sha256, artifact.name)
    if checksum_text != expected_checksum:
        raise RunnerError("candidate checksum does not exactly bind the archive")
    return CandidateIdentity(
        candidate_dir=candidate_dir,
        artifact=artifact,
        artifact_sha256=artifact_sha256,
        source_commit=source_commit,
        source_tag=source_tag,
    )


def extract_candidate(archive_snapshot_descriptor: int, destination: Path) -> Path:
    try:
        snapshot_metadata = os.fstat(archive_snapshot_descriptor)
        if not stat.S_ISREG(snapshot_metadata.st_mode) or snapshot_metadata.st_nlink != 0:
            raise RunnerError(
                "candidate archive snapshot descriptor is not an unlinked regular file"
            )
        os.lseek(archive_snapshot_descriptor, 0, os.SEEK_SET)
    except RunnerError:
        raise
    except OSError as exc:
        raise RunnerError("cannot inspect candidate archive snapshot descriptor: {}".format(exc))
    archive_descriptor_path = "/dev/fd/{}".format(archive_snapshot_descriptor)
    try:
        completed = subprocess.run(
            [
                "/usr/bin/ditto",
                "-x",
                "-k",
                archive_descriptor_path,
                str(destination),
            ],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            check=False,
            pass_fds=(archive_snapshot_descriptor,),
        )
    except OSError as exc:
        raise RunnerError("cannot execute /usr/bin/ditto: {}".format(exc))
    if completed.returncode != 0:
        diagnostic = (completed.stderr or completed.stdout).strip()
        raise RunnerError(
            "candidate extraction failed (exit {}): {}".format(
                completed.returncode, diagnostic or "no diagnostic"
            )
        )
    executable = destination / "Tanks3D.app" / "Contents" / "MacOS" / "Tanks3D"
    require_regular_non_symlink(executable, "extracted candidate executable")
    for parent in (executable.parent, executable.parent.parent, executable.parent.parent.parent):
        if parent.is_symlink():
            raise RunnerError("extracted app path must not traverse symbolic links")
    if not os.access(executable, os.X_OK):
        raise RunnerError("extracted candidate executable is not executable")
    return executable


def expected_performance_capabilities(identity: CandidateIdentity) -> Dict[str, Any]:
    return {
        "schema": CAPABILITY_SCHEMA,
        "telemetry_schema": TELEMETRY_SCHEMA,
        "producer": performance_contract.PERFORMANCE_V2_PRODUCER,
        "quick_start_argument": "--quick-start",
        "log_argument_prefix": "--release-performance-log=",
        "candidate_sha256_argument_prefix": "--release-candidate-sha256=",
        "session_nonce_argument_prefix": "--release-session-nonce=",
        "duration_argument_prefix": "--release-performance-duration-seconds=",
        "start_marker": START_MARKER,
        "complete_marker": COMPLETE_MARKER,
        "default_duration_seconds": DEFAULT_DURATION_SECONDS,
        "maximum_duration_seconds": MAX_DURATION_SECONDS,
        "sample_interval_microseconds": (
            performance_contract.PERFORMANCE_V2_TARGET_INTERVAL_US
        ),
        "clock": performance_contract.PERFORMANCE_V2_CLOCK,
        "memory_metric": performance_contract.PERFORMANCE_V2_MEMORY_METRIC,
        "memory_unit": performance_contract.PERFORMANCE_V2_MEMORY_UNIT,
        "app_states": list(performance_contract.PERFORMANCE_V2_APP_STATES),
        "source_commit": identity.source_commit,
        "source_tag": identity.source_tag,
        "self_check": "PASS",
    }


def probe_performance_capabilities(
    executable: Path, identity: CandidateIdentity
) -> None:
    argv = [str(executable), CAPABILITY_ARGUMENT]
    try:
        completed = subprocess.run(
            argv,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            check=False,
            cwd=str(executable.parent),
            close_fds=True,
            timeout=CAPABILITY_TIMEOUT_SECONDS,
        )
    except subprocess.TimeoutExpired:
        raise RunnerError("candidate performance capability probe timed out")
    except OSError as exc:
        raise RunnerError(
            "cannot run candidate performance capability probe: {}".format(exc)
        )
    if completed.returncode != 0:
        diagnostic = (completed.stderr or completed.stdout).decode(
            "utf-8", errors="replace"
        ).strip()
        raise RunnerError(
            "candidate performance capability probe failed (exit {}): {}".format(
                completed.returncode, diagnostic or "no diagnostic"
            )
        )
    if completed.stderr:
        raise RunnerError("candidate performance capability probe wrote to stderr")
    if len(completed.stdout) == 0 or len(completed.stdout) > 64 * 1024:
        raise RunnerError("candidate performance capability output has an invalid size")
    try:
        text = completed.stdout.decode("utf-8")
        value = json.loads(text, object_pairs_hook=reject_duplicate_pairs)
    except UnicodeError as exc:
        raise RunnerError(
            "candidate performance capability output is not UTF-8: {}".format(exc)
        )
    except RunnerError:
        raise
    except json.JSONDecodeError as exc:
        raise RunnerError(
            "candidate performance capability output is invalid JSON: {}".format(exc)
        )
    expected = expected_performance_capabilities(identity)
    canonical = (json.dumps(expected, indent=2, sort_keys=False) + "\n").encode(
        "utf-8"
    )
    if value != expected or completed.stdout != canonical:
        raise RunnerError(
            "candidate performance capability manifest does not match its identity and contract"
        )


def wait_for_performance_process(process: subprocess.Popen, duration_seconds: int) -> int:
    timeout_seconds = duration_seconds + PROCESS_TIMEOUT_GRACE_SECONDS
    try:
        return process.wait(timeout=timeout_seconds)
    except subprocess.TimeoutExpired:
        try:
            process.terminate()
            process.wait(timeout=PROCESS_TERMINATE_GRACE_SECONDS)
        except (OSError, subprocess.TimeoutExpired):
            try:
                process.kill()
                process.wait(timeout=PROCESS_TERMINATE_GRACE_SECONDS)
            except (OSError, subprocess.TimeoutExpired):
                pass
        raise RunnerError(
            "candidate performance run exceeded its {}-second timeout".format(
                timeout_seconds
            )
        )


def create_output_file(path: Path):
    try:
        descriptor = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
    except OSError as exc:
        raise RunnerError("cannot exclusively create {}: {}".format(path.name, exc))
    return os.fdopen(descriptor, "wb", buffering=0)


def read_marker_log_with_digest(path: Path) -> Tuple[str, str]:
    data = read_regular_file_no_follow(
        path,
        "candidate stdout",
        maximum_bytes=MAX_MARKER_LOG_BYTES,
    )
    try:
        text = data.decode("utf-8")
    except UnicodeError as exc:
        raise RunnerError("candidate stdout is not valid UTF-8: {}".format(exc))
    return text, hashlib.sha256(data).hexdigest()


def validate_markers(stdout_text: str, nonce: str) -> None:
    try:
        performance_contract.validate_performance_markers(stdout_text, nonce)
    except performance_contract.PerformanceContractError as exc:
        raise RunnerError(str(exc))


def validate_telemetry(
    path: Path,
    identity: CandidateIdentity,
    nonce: str,
    duration_seconds: int,
    started_at_utc: str,
    completed_at_utc: str,
) -> Tuple[performance_contract.PerformanceSummary, str]:
    require_regular_non_symlink(path, "candidate performance telemetry")
    value, digest = load_strict_json_with_digest(path)
    try:
        summary = performance_contract.validate_performance_log_v2(
            value,
            performance_contract.RunBinding(
                source_commit=identity.source_commit,
                source_tag=identity.source_tag,
                candidate_sha256=identity.artifact_sha256,
                session_nonce=nonce,
                requested_duration_seconds=duration_seconds,
                receipt_started_at_utc=started_at_utc,
                receipt_completed_at_utc=completed_at_utc,
            ),
        )
    except performance_contract.PerformanceContractError as exc:
        raise RunnerError(str(exc))
    return summary, digest


def file_reference(
    path: Path,
    validated_sha256: Optional[str] = None,
    maximum_bytes: Optional[int] = None,
) -> Dict[str, str]:
    digest = sha256_regular_file_no_follow(
        path,
        path.name,
        maximum_bytes=maximum_bytes,
    )
    if validated_sha256 is not None and digest != validated_sha256:
        raise RunnerError("{} changed after validation".format(path.name))
    return {"path": path.name, "sha256": digest}


def publish_json_no_replace(path: Path, value: Mapping[str, Any]) -> None:
    if set(value) != RECEIPT_KEYS:
        raise RunnerError("internal receipt key set is incomplete")
    for key in ("telemetry", "stdout", "stderr"):
        reference = value[key]
        if not isinstance(reference, dict) or set(reference) != FILE_REFERENCE_KEYS:
            raise RunnerError("internal receipt file reference is malformed")
    encoded = (json.dumps(value, indent=2, sort_keys=False) + "\n").encode("utf-8")
    descriptor = -1
    temporary_path: Optional[Path] = None
    try:
        descriptor, temporary_name = tempfile.mkstemp(
            prefix=".performance-qa-receipt.", dir=str(path.parent)
        )
        temporary_path = Path(temporary_name)
        os.fchmod(descriptor, 0o600)
        offset = 0
        while offset < len(encoded):
            written = os.write(descriptor, encoded[offset:])
            if written <= 0:
                raise RunnerError("receipt write made no progress")
            offset += written
        os.fsync(descriptor)
        os.close(descriptor)
        descriptor = -1
        try:
            os.link(str(temporary_path), str(path))
        except OSError as exc:
            if exc.errno == errno.EEXIST:
                raise RunnerError("receipt already exists; refusing to replace it")
            raise RunnerError("cannot atomically publish receipt: {}".format(exc))
        temporary_path.unlink()
        temporary_path = None
    finally:
        if descriptor >= 0:
            os.close(descriptor)
        if temporary_path is not None:
            try:
                temporary_path.unlink()
            except FileNotFoundError:
                pass


def run_performance_qa(
    project_root_argument: Path,
    candidate_dir_argument: Path,
    output_dir_argument: Path,
    duration_seconds: int = DEFAULT_DURATION_SECONDS,
) -> Path:
    try:
        duration_seconds = performance_contract.validate_duration_seconds(
            duration_seconds
        )
    except performance_contract.PerformanceContractError as exc:
        raise RunnerError(str(exc))
    project_root = require_real_directory(project_root_argument, "project root")
    output_dir = require_private_empty_output(output_dir_argument)
    identity = parse_candidate(project_root, candidate_dir_argument)
    nonce = secrets.token_hex(16)
    if re.fullmatch(r"[0-9a-f]{32}", nonce) is None:
        raise RunnerError("secure nonce generator returned an invalid value")

    telemetry_path = output_dir / TELEMETRY_FILENAME
    stdout_path = output_dir / STDOUT_FILENAME
    stderr_path = output_dir / STDERR_FILENAME
    receipt_path = output_dir / RECEIPT_FILENAME
    receipt: Dict[str, Any]
    with tempfile.TemporaryDirectory(prefix="tanks3d-performance-qa-") as temporary_name:
        temporary_root = Path(temporary_name)
        os.chmod(temporary_root, 0o700)
        archive_snapshot_descriptor = snapshot_candidate_archive(
            identity, temporary_root
        )
        extraction_root = temporary_root / "extracted"
        extraction_root.mkdir(mode=0o700)
        try:
            executable = extract_candidate(
                archive_snapshot_descriptor, extraction_root
            )
        finally:
            os.close(archive_snapshot_descriptor)
        probe_performance_capabilities(executable, identity)
        executable_sha256 = sha256_file(executable)
        argv = [
            str(executable),
            "--quick-start",
            "--release-performance-log={}".format(telemetry_path),
            "--release-candidate-sha256={}".format(identity.artifact_sha256),
            "--release-session-nonce={}".format(nonce),
            "--release-performance-duration-seconds={}".format(duration_seconds),
        ]
        started_at_utc = utc_now()
        try:
            with create_output_file(stdout_path) as stdout_stream, create_output_file(
                stderr_path
            ) as stderr_stream:
                try:
                    process = subprocess.Popen(
                        argv,
                        stdout=stdout_stream,
                        stderr=stderr_stream,
                        cwd=str(executable.parent),
                        close_fds=True,
                    )
                except OSError as exc:
                    raise RunnerError("cannot launch extracted candidate: {}".format(exc))
                pid = process.pid
                exit_code = wait_for_performance_process(
                    process, duration_seconds
                )
        except RunnerError:
            raise
        completed_at_utc = utc_now()
        if not isinstance(pid, int) or pid <= 0:
            raise RunnerError("candidate process did not provide a valid PID")
        if exit_code != 0:
            raise RunnerError("candidate performance run exited {}".format(exit_code))
        stdout_text, stdout_sha256 = read_marker_log_with_digest(stdout_path)
        validate_markers(stdout_text, nonce)
        _, telemetry_sha256 = validate_telemetry(
            telemetry_path,
            identity,
            nonce,
            duration_seconds,
            started_at_utc,
            completed_at_utc,
        )

        receipt = {
            "schema": RECEIPT_SCHEMA,
            "candidate_filename": identity.artifact.name,
            "candidate_sha256": identity.artifact_sha256,
            "executable_sha256": executable_sha256,
            "source_commit": identity.source_commit,
            "source_tag": identity.source_tag,
            "session_nonce": nonce,
            "argv": argv,
            "pid": pid,
            "started_at_utc": started_at_utc,
            "completed_at_utc": completed_at_utc,
            "exit_code": exit_code,
            "telemetry": file_reference(
                telemetry_path,
                telemetry_sha256,
                maximum_bytes=MAX_TELEMETRY_BYTES,
            ),
            "stdout": file_reference(
                stdout_path,
                stdout_sha256,
                maximum_bytes=MAX_MARKER_LOG_BYTES,
            ),
            "stderr": file_reference(stderr_path),
        }
    publish_json_no_replace(receipt_path, receipt)
    return receipt_path


def positive_duration(value: str) -> int:
    try:
        parsed = int(value, 10)
    except ValueError:
        raise argparse.ArgumentTypeError("duration must be a base-10 integer")
    try:
        return performance_contract.validate_duration_seconds(parsed)
    except performance_contract.PerformanceContractError:
        raise argparse.ArgumentTypeError(
            "duration must be between 1 and {} seconds".format(MAX_DURATION_SECONDS)
        )


def build_argument_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--project-root", required=True)
    parser.add_argument("--candidate-dir", required=True)
    parser.add_argument("--output-dir", required=True)
    parser.add_argument(
        "--duration-seconds",
        type=positive_duration,
        default=DEFAULT_DURATION_SECONDS,
    )
    return parser


def main(argv: Optional[Sequence[str]] = None) -> int:
    arguments = build_argument_parser().parse_args(argv)
    try:
        receipt = run_performance_qa(
            Path(arguments.project_root),
            Path(arguments.candidate_dir),
            Path(arguments.output_dir),
            arguments.duration_seconds,
        )
    except RunnerError as exc:
        print("release performance QA failed: {}".format(exc), file=sys.stderr)
        return 1
    print("Release performance QA receipt: {}".format(receipt))
    return 0


if __name__ == "__main__":
    sys.exit(main())
