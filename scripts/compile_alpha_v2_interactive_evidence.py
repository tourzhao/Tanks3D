#!/usr/bin/env python3
"""Compile explicit Alpha-v2 observations into candidate-bound QA evidence.

The two commands deliberately separate planning from attestation.  ``init-plan``
creates a no-overwrite manifest whose observations are all ``NOT_RUN``.
``compile`` accepts only a fully and explicitly completed manifest, then writes
a new evidence pack and ``status.next.json`` without changing the source status.
"""

import argparse
import copy
from datetime import datetime, timedelta, timezone
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import stat
import struct
import sys
from typing import Any, Dict, List, Mapping, NamedTuple, Optional, Sequence, Tuple
import zlib


OBSERVATION_MANIFEST_SCHEMA = (
    "tanks3d-alpha-v2-interactive-observation-manifest-v1"
)
INTERACTIVE_SESSION_SCHEMA = "tanks3d-interactive-session-v1"
EVENT_LOG_SCHEMA = "tanks3d-gameplay-event-log-v2"
EVENT_LOG_PRODUCER = "Tanks3D Alpha QA Evidence Compiler"
OBSERVATION_MANIFEST_SHA256_KEY = "observation_manifest_sha256"
REQUIREMENTS_SCHEMA = "tanks3d-release-requirements-v2"
EVIDENCE_IDS = (
    "one_player_gameplay",
    "two_player_gameplay",
    "national_bases",
    "pickup_and_minimap",
    "settlement_report",
)
MANIFEST_KEYS = {
    "schema",
    "requirements_profile",
    "event_log_schema",
    "event_log_producer",
    "candidate_sha256",
    "tester",
    "machine",
    "tester_signature",
    "reviewer",
    "reviewer_signature",
    "started_at_utc",
    "completed_at_utc",
    "reviewed_at_utc",
    "review_notes",
    "supporting_artifacts",
    "observations",
}
OBSERVATION_KEYS = {
    "coverage_token",
    "evidence_id",
    "required_checks",
    "result",
    "observed_at_utc",
    "checks_confirmed",
    "notes",
}
SHA256_RE = re.compile(r"^[0-9a-f]{64}$")
TIMESTAMP_RE = re.compile(r"^[0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9]{2}:[0-9]{2}:[0-9]{2}Z$")
MINIMUM_RECORDING_BYTES = 64 * 1024
MAXIMUM_PNG_FILE_BYTES = 64 * 1024 * 1024
MAXIMUM_DECODED_PNG_BYTES = 64 * 1024 * 1024
FUTURE_TIMESTAMP_TOLERANCE = timedelta(minutes=5)


class CompileError(Exception):
    """The observation manifest cannot be compiled safely."""


class FileSnapshot(NamedTuple):
    """Stable identity and streamed content digest for an input artifact."""

    sha256: str
    size: int
    device: int
    inode: int
    mtime_ns: int
    ctime_ns: int


def reject_duplicate_pairs(pairs: Sequence[Tuple[str, Any]]) -> Dict[str, Any]:
    result: Dict[str, Any] = {}
    for key, value in pairs:
        if key in result:
            raise CompileError("duplicate JSON key: {!r}".format(key))
        result[key] = value
    return result


def snapshot_regular_file(
    path: Path,
    label: str,
    collect: bool = False,
    maximum_collected_bytes: Optional[int] = None,
) -> Tuple[FileSnapshot, Optional[bytes]]:
    """Hash a stable regular file, optionally retaining a bounded byte copy."""

    flags = os.O_RDONLY
    if hasattr(os, "O_NONBLOCK"):
        flags |= os.O_NONBLOCK
    if hasattr(os, "O_NOFOLLOW"):
        flags |= os.O_NOFOLLOW
    try:
        descriptor = os.open(str(path), flags)
    except OSError as exc:
        raise CompileError("cannot open {}: {}".format(label, exc))
    try:
        before = os.fstat(descriptor)
        if not stat.S_ISREG(before.st_mode):
            raise CompileError("{} must be a regular non-symlink file".format(label))
        digest = hashlib.sha256()
        collected = bytearray() if collect else None
        total = 0
        while True:
            chunk = os.read(descriptor, 1024 * 1024)
            if not chunk:
                break
            total += len(chunk)
            if (
                maximum_collected_bytes is not None
                and total > maximum_collected_bytes
            ):
                raise CompileError(
                    "{} exceeds the maximum supported size of {} bytes".format(
                        label, maximum_collected_bytes
                    )
                )
            digest.update(chunk)
            if collected is not None:
                collected.extend(chunk)
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
        if identity_before != identity_after or total != after.st_size:
            raise CompileError("{} changed while it was read".format(label))
        snapshot = FileSnapshot(
            digest.hexdigest(),
            after.st_size,
            after.st_dev,
            after.st_ino,
            after.st_mtime_ns,
            after.st_ctime_ns,
        )
        return snapshot, bytes(collected) if collected is not None else None
    finally:
        os.close(descriptor)


def read_regular_file(path: Path, label: str) -> bytes:
    _, data = snapshot_regular_file(path, label, collect=True)
    if data is None:
        raise CompileError("internal read failure for {}".format(label))
    return data


def parse_json(data: bytes, label: str) -> Any:
    try:
        return json.loads(data.decode("utf-8"), object_pairs_hook=reject_duplicate_pairs)
    except (UnicodeError, json.JSONDecodeError) as exc:
        raise CompileError("{} is not valid JSON: {}".format(label, exc))


def load_json(path: Path, label: str) -> Tuple[bytes, Any]:
    data = read_regular_file(path, label)
    return data, parse_json(data, label)


def require_object(value: Any, label: str) -> Mapping[str, Any]:
    if not isinstance(value, dict):
        raise CompileError("{} must be an object".format(label))
    return value


def require_exact_keys(value: Mapping[str, Any], keys: set, label: str) -> None:
    actual = set(value)
    if actual != keys:
        missing = sorted(keys - actual)
        extra = sorted(actual - keys)
        details = []
        if missing:
            details.append("missing {}".format(", ".join(missing)))
        if extra:
            details.append("unexpected {}".format(", ".join(extra)))
        raise CompileError("{} has invalid keys ({})".format(label, "; ".join(details)))


def require_string(value: Any, label: str, nonempty: bool = False) -> str:
    if not isinstance(value, str):
        raise CompileError("{} must be a string".format(label))
    if nonempty and not value.strip():
        raise CompileError("{} must be non-empty".format(label))
    return value


def require_nonplaceholder(value: Any, label: str) -> str:
    text = require_string(value, label).strip()
    upper = text.upper()
    if not text or upper in {"NONE", "N/A", "NA", "TBD"} or upper.startswith("NOT "):
        raise CompileError("{} is missing or a placeholder".format(label))
    return text


def reject_blocking_language(value: Any, label: str) -> None:
    text = require_string(value, label).strip().lower()
    forbidden = (
        "blocked",
        "not run",
        "not recorded",
        "not checked",
        "not tested",
        "not exercised",
        "not interactive",
        "not a live",
        "skipped",
        "pending",
        "showcase only",
        "rendering evidence only",
    )
    for phrase in forbidden:
        if re.search(r"(?<!\w){}(?!\w)".format(re.escape(phrase)), text):
            raise CompileError("{} contradicts PASS with {!r}".format(label, phrase))


def require_string_list(value: Any, label: str) -> List[str]:
    if not isinstance(value, list) or any(not isinstance(item, str) for item in value):
        raise CompileError("{} must be an array of strings".format(label))
    if len(value) != len(set(value)):
        raise CompileError("{} contains duplicates".format(label))
    return list(value)


def parse_timestamp(value: Any, label: str) -> datetime:
    text = require_string(value, label)
    if not TIMESTAMP_RE.fullmatch(text):
        raise CompileError("{} must use strict UTC YYYY-MM-DDTHH:MM:SSZ".format(label))
    try:
        parsed = datetime.strptime(text, "%Y-%m-%dT%H:%M:%SZ")
    except ValueError as exc:
        raise CompileError("{} is invalid: {}".format(label, exc))
    result = parsed.replace(tzinfo=timezone.utc)
    if result > datetime.now(timezone.utc) + FUTURE_TIMESTAMP_TOLERANCE:
        raise CompileError("{} must not be in the future".format(label))
    return result


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def json_bytes(value: Any) -> bytes:
    return (json.dumps(value, indent=2, sort_keys=True) + "\n").encode("utf-8")


def decode_png_stream(compressed: bytes, label: str, maximum_size: int) -> bytes:
    try:
        decoder = zlib.decompressobj()
        decoded = decoder.decompress(compressed, maximum_size + 1)
        if len(decoded) > maximum_size or decoder.unconsumed_tail:
            raise CompileError("{} exceeds the maximum decoded PNG size".format(label))
        decoded += decoder.flush(maximum_size + 1 - len(decoded))
    except zlib.error as exc:
        raise CompileError("{} has an invalid PNG image stream: {}".format(label, exc))
    if len(decoded) > maximum_size:
        raise CompileError("{} exceeds the maximum decoded PNG size".format(label))
    if not decoder.eof or decoder.unused_data or decoder.unconsumed_tail:
        raise CompileError("{} has a non-canonical PNG image stream".format(label))
    return decoded


def verify_png_data(data: bytes, label: str) -> None:
    """Apply the verifier's complete non-release PNG validation contract."""

    if not data.startswith(b"\x89PNG\r\n\x1a\n"):
        raise CompileError("{} is not a PNG".format(label))
    offset = 8
    chunks: List[bytes] = []
    image_data: List[bytes] = []
    idat_started = False
    idat_finished = False
    known_critical = {b"IHDR", b"PLTE", b"IDAT", b"IEND"}
    while offset < len(data):
        if len(data) - offset < 12:
            raise CompileError("{} has a truncated PNG chunk".format(label))
        length = struct.unpack(">I", data[offset : offset + 4])[0]
        chunk_type = data[offset + 4 : offset + 8]
        end = offset + 12 + length
        if end > len(data):
            raise CompileError("{} has a truncated PNG payload".format(label))
        payload = data[offset + 8 : offset + 8 + length]
        recorded_crc = struct.unpack(">I", data[offset + 8 + length : end])[0]
        actual_crc = zlib.crc32(chunk_type)
        actual_crc = zlib.crc32(payload, actual_crc) & 0xFFFFFFFF
        if recorded_crc != actual_crc:
            raise CompileError("{} has an invalid PNG chunk CRC".format(label))
        if len(chunk_type) != 4 or (
            chunk_type not in known_critical and not (chunk_type[0] & 0x20)
        ):
            raise CompileError("{} has an unknown critical PNG chunk".format(label))
        chunks.append(chunk_type)
        if len(chunks) == 1:
            if chunk_type != b"IHDR" or length != 13:
                raise CompileError("{} has no canonical IHDR".format(label))
            width, height = struct.unpack(">II", payload[:8])
            if width == 0 or height == 0:
                raise CompileError("{} has zero PNG dimensions".format(label))
        elif chunk_type == b"IHDR":
            raise CompileError("{} has more than one IHDR".format(label))
        if chunk_type == b"IDAT":
            if idat_finished:
                raise CompileError("{} has non-contiguous IDAT chunks".format(label))
            idat_started = True
            image_data.append(payload)
        elif idat_started and chunk_type != b"IEND":
            idat_finished = True
        if chunk_type == b"IEND":
            if length != 0 or end != len(data):
                raise CompileError("{} has an invalid IEND".format(label))
            offset = end
            break
        offset = end
    if not chunks or chunks[-1] != b"IEND" or not image_data:
        raise CompileError("{} is an incomplete PNG".format(label))
    decode_png_stream(b"".join(image_data), label, MAXIMUM_DECODED_PNG_BYTES)


def require_real_directory(path: Path, label: str) -> Path:
    try:
        metadata = path.lstat()
    except OSError as exc:
        raise CompileError("{} is not accessible: {}".format(label, exc))
    if stat.S_ISLNK(metadata.st_mode) or not stat.S_ISDIR(metadata.st_mode):
        raise CompileError("{} must be a real directory".format(label))
    return path.resolve(strict=True)


def require_no_symlink_components(root: Path, path: Path, label: str) -> None:
    try:
        relative = path.relative_to(root)
    except ValueError:
        raise CompileError("{} is outside the project root".format(label))
    current = root
    for part in relative.parts:
        current = current / part
        try:
            metadata = current.lstat()
        except OSError as exc:
            raise CompileError("{} is not accessible: {}".format(label, exc))
        if stat.S_ISLNK(metadata.st_mode):
            raise CompileError("{} has a symlinked path component".format(label))


def repository_path(root: Path, raw_path: Any, label: str) -> Tuple[str, Path]:
    text = require_string(raw_path, label, True)
    pure = PurePosixPath(text)
    if (
        pure.is_absolute()
        or text != pure.as_posix()
        or "\\" in text
        or any(part in {"", ".", ".."} for part in pure.parts)
    ):
        raise CompileError("{} must be a normalized repository-relative path".format(label))
    path = root.joinpath(*pure.parts)
    require_no_symlink_components(root, path, label)
    return text, path


def repository_file(root: Path, raw_path: Any, label: str) -> Tuple[str, Path, bytes]:
    text, path = repository_path(root, raw_path, label)
    return text, path, read_regular_file(path, label)


def path_inside_root(root: Path, path: Path, label: str) -> Tuple[Path, str]:
    absolute = path if path.is_absolute() else root / path
    try:
        relative = absolute.relative_to(root)
    except ValueError:
        raise CompileError("{} must be inside the project root".format(label))
    pure = PurePosixPath(*relative.parts)
    if not relative.parts or any(part in {"", ".", ".."} for part in relative.parts):
        raise CompileError("{} must name a repository subdirectory".format(label))
    return absolute, pure.as_posix()


def load_profile(requirements_path: Path) -> Tuple[bytes, Mapping[str, Any]]:
    raw, value = load_json(requirements_path, "requirements profile")
    profile = require_object(value, "requirements profile")
    if profile.get("schema") != REQUIREMENTS_SCHEMA:
        raise CompileError("requirements profile has the wrong schema")
    if profile.get("profile") != "macos-alpha-v2":
        raise CompileError("requirements profile is not macos-alpha-v2")
    expected_strings = {
        "interactive_observation_manifest_schema": OBSERVATION_MANIFEST_SCHEMA,
        "gameplay_event_log_schema": EVENT_LOG_SCHEMA,
        "gameplay_event_log_producer": EVENT_LOG_PRODUCER,
    }
    for key, expected in expected_strings.items():
        if profile.get(key) != expected:
            raise CompileError(
                "requirements profile.{} does not match the compiler".format(key)
            )
    expected_lists = {
        "interactive_observation_manifest_keys": list(MANIFEST_KEYS),
        "interactive_observation_keys": list(OBSERVATION_KEYS),
        "interactive_observation_evidence_ids": list(EVIDENCE_IDS),
        "interactive_supporting_artifact_group_keys": [
            "evidence_id",
            "artifacts",
        ],
        "interactive_supporting_artifact_keys": ["path", "kind"],
        "gameplay_event_log_keys": [
            "schema",
            "producer",
            OBSERVATION_MANIFEST_SHA256_KEY,
            "candidate_sha256",
            "tester",
            "machine",
            "started_at_utc",
            "completed_at_utc",
            "events",
        ],
    }
    for key, expected in expected_lists.items():
        actual = require_string_list(
            profile.get(key), "requirements profile.{}".format(key)
        )
        if key in {
            "interactive_observation_manifest_keys",
            "interactive_observation_keys",
        }:
            if set(actual) != set(expected):
                raise CompileError(
                    "requirements profile.{} does not match the compiler".format(
                        key
                    )
                )
        elif actual != expected:
            raise CompileError(
                "requirements profile.{} does not match the compiler".format(key)
            )
    for key in ("modes", "gameplay_ids", "base_ids", "base_checks", "settlement_ids"):
        require_string_list(profile.get(key), "requirements profile.{}".format(key))
    pickups = profile.get("pickup_requirements")
    if not isinstance(pickups, list):
        raise CompileError("requirements profile.pickup_requirements must be an array")
    for index, raw_pickup in enumerate(pickups):
        pickup = require_object(raw_pickup, "pickup requirement")
        require_string(pickup.get("id"), "pickup requirement id", True)
        require_string_list(pickup.get("checks"), "pickup requirement checks")
    if profile["modes"] != ["one_player", "two_player"]:
        raise CompileError("requirements profile modes are not canonical")
    return raw, profile


def token_plan(profile: Mapping[str, Any]) -> List[Dict[str, Any]]:
    result: List[Dict[str, Any]] = []

    def add(
        section: str,
        item_id: str,
        mode: Optional[str],
        evidence_id: str,
        checks: Sequence[str],
    ) -> None:
        token = "{}:{}".format(section, item_id)
        if mode is not None:
            token += ":{}".format(mode)
        result.append(
            {
                "section": section,
                "id": item_id,
                "mode": mode,
                "coverage_token": token,
                "evidence_id": evidence_id,
                "required_checks": list(checks),
            }
        )

    for item_id in profile["gameplay_ids"]:
        for mode in profile["modes"]:
            evidence_id = (
                "one_player_gameplay" if mode == "one_player" else "two_player_gameplay"
            )
            add("gameplay", item_id, mode, evidence_id, [item_id])
    for item_id in profile["base_ids"]:
        for mode in profile["modes"]:
            add("base", item_id, mode, "national_bases", profile["base_checks"])
    for pickup in profile["pickup_requirements"]:
        for mode in profile["modes"]:
            add("pickup", pickup["id"], mode, "pickup_and_minimap", pickup["checks"])
    for item_id in profile["settlement_ids"]:
        add("settlement", item_id, None, "settlement_report", [item_id])
    tokens = [item["coverage_token"] for item in result]
    if len(tokens) != len(set(tokens)):
        raise CompileError("requirements profile produces duplicate coverage tokens")
    return result


def initial_manifest(profile: Mapping[str, Any]) -> Dict[str, Any]:
    observations = []
    for planned in token_plan(profile):
        observations.append(
            {
                "coverage_token": planned["coverage_token"],
                "evidence_id": planned["evidence_id"],
                "required_checks": planned["required_checks"],
                "result": "NOT_RUN",
                "observed_at_utc": None,
                "checks_confirmed": [],
                "notes": "",
            }
        )
    return {
        "schema": OBSERVATION_MANIFEST_SCHEMA,
        "requirements_profile": profile["profile"],
        "event_log_schema": EVENT_LOG_SCHEMA,
        "event_log_producer": EVENT_LOG_PRODUCER,
        "candidate_sha256": "",
        "tester": "",
        "machine": "",
        "tester_signature": "",
        "reviewer": "",
        "reviewer_signature": "",
        "started_at_utc": None,
        "completed_at_utc": None,
        "reviewed_at_utc": None,
        "review_notes": "",
        "supporting_artifacts": [
            {"evidence_id": evidence_id, "artifacts": []}
            for evidence_id in EVIDENCE_IDS
        ],
        "observations": observations,
    }


def exclusive_write(path: Path, data: bytes, mode: int = 0o600) -> None:
    flags = os.O_WRONLY | os.O_CREAT | os.O_EXCL
    if hasattr(os, "O_NOFOLLOW"):
        flags |= os.O_NOFOLLOW
    descriptor = os.open(str(path), flags, mode)
    try:
        view = memoryview(data)
        while view:
            written = os.write(descriptor, view)
            if written <= 0:
                raise CompileError("short write to {}".format(path))
            view = view[written:]
        os.fsync(descriptor)
    finally:
        os.close(descriptor)


def exclusive_write_at(directory_fd: int, name: str, data: bytes) -> None:
    if not name or "/" in name or name in {".", ".."}:
        raise CompileError("invalid evidence-pack filename: {!r}".format(name))
    flags = os.O_WRONLY | os.O_CREAT | os.O_EXCL
    if hasattr(os, "O_NOFOLLOW"):
        flags |= os.O_NOFOLLOW
    descriptor = os.open(name, flags, 0o600, dir_fd=directory_fd)
    try:
        view = memoryview(data)
        while view:
            written = os.write(descriptor, view)
            if written <= 0:
                raise CompileError("short write to evidence-pack file {}".format(name))
            view = view[written:]
        os.fsync(descriptor)
    except Exception:
        os.close(descriptor)
        descriptor = -1
        try:
            os.unlink(name, dir_fd=directory_fd)
        except OSError:
            pass
        raise
    finally:
        if descriptor >= 0:
            os.close(descriptor)


def command_init_plan(args: argparse.Namespace) -> None:
    output = Path(args.output)
    if os.path.lexists(str(output)):
        raise CompileError("output already exists: {}".format(output))
    require_real_directory(output.parent, "output parent")
    _, profile = load_profile(Path(args.requirements))
    exclusive_write(output, json_bytes(initial_manifest(profile)))
    print("Initialized NOT_RUN observation plan: {}".format(output))


def resolve_status_requirements(
    root: Path, status: Mapping[str, Any]
) -> Tuple[bytes, Mapping[str, Any]]:
    requirements = status.get("requirements")
    _, path, raw = repository_file(root, requirements, "status.requirements")
    profile = require_object(parse_json(raw, "status.requirements"), "status.requirements")
    if profile.get("schema") != REQUIREMENTS_SCHEMA:
        raise CompileError("status requirements have the wrong schema")
    # Run the full lightweight profile validation through its canonical path.
    checked_raw, checked_profile = load_profile(path)
    if raw != checked_raw:
        raise CompileError("status requirements changed while it was read")
    return checked_raw, checked_profile


def validate_manifest(
    manifest_value: Any,
    profile: Mapping[str, Any],
    candidate_sha256: str,
) -> Tuple[Mapping[str, Any], List[Dict[str, Any]]]:
    manifest = require_object(manifest_value, "observation manifest")
    require_exact_keys(manifest, MANIFEST_KEYS, "observation manifest")
    expected_constants = {
        "schema": OBSERVATION_MANIFEST_SCHEMA,
        "requirements_profile": profile["profile"],
        "event_log_schema": EVENT_LOG_SCHEMA,
        "event_log_producer": EVENT_LOG_PRODUCER,
    }
    for key, expected in expected_constants.items():
        if manifest[key] != expected:
            raise CompileError("observation manifest.{} does not match {}".format(key, expected))
    candidate = require_string(manifest["candidate_sha256"], "candidate_sha256", True)
    if not SHA256_RE.fullmatch(candidate) or candidate != candidate_sha256:
        raise CompileError("observation manifest candidate does not match the status artifact")
    tester = require_nonplaceholder(manifest["tester"], "tester")
    reviewer = require_nonplaceholder(manifest["reviewer"], "reviewer")
    for key in (
        "machine",
        "tester_signature",
        "reviewer_signature",
        "review_notes",
    ):
        require_nonplaceholder(manifest[key], key)
    reject_blocking_language(manifest["review_notes"], "review_notes")
    if tester.strip().casefold() == reviewer.strip().casefold():
        raise CompileError("tester and reviewer must be different people")
    started = parse_timestamp(manifest["started_at_utc"], "started_at_utc")
    completed = parse_timestamp(manifest["completed_at_utc"], "completed_at_utc")
    reviewed = parse_timestamp(manifest["reviewed_at_utc"], "reviewed_at_utc")
    if not started <= completed <= reviewed:
        raise CompileError("manifest timestamps are not chronological")

    planned = token_plan(profile)
    observations = manifest["observations"]
    if not isinstance(observations, list) or len(observations) != len(planned):
        raise CompileError("manifest observations do not exactly match the token plan")
    validated: List[Dict[str, Any]] = []
    for index, (raw_observation, expected) in enumerate(zip(observations, planned)):
        label = "observations[{}]".format(index)
        observation = require_object(raw_observation, label)
        require_exact_keys(observation, OBSERVATION_KEYS, label)
        for key in ("coverage_token", "evidence_id", "required_checks"):
            if observation[key] != expected[key]:
                raise CompileError("{} does not match the canonical token plan".format(label))
        if observation["result"] != "PASS":
            raise CompileError("{} must explicitly record PASS".format(label))
        observed = parse_timestamp(observation["observed_at_utc"], label + ".observed_at_utc")
        if not started <= observed <= completed:
            raise CompileError("{} lies outside the observation session".format(label))
        checks = require_string_list(observation["checks_confirmed"], label + ".checks_confirmed")
        if checks != expected["required_checks"]:
            raise CompileError("{} checks_confirmed are not exact".format(label))
        require_nonplaceholder(observation["notes"], label + ".notes")
        reject_blocking_language(observation["notes"], label + ".notes")
        validated.append({**expected, "observed_at_utc": observation["observed_at_utc"], "notes": observation["notes"]})
    return manifest, validated


def snapshot_artifact(
    root: Path, raw_path: Any, kind: str, label: str
) -> Tuple[str, FileSnapshot]:
    path_text, path = repository_path(root, raw_path, label + ".path")
    collect_png = kind == "png"
    snapshot, data = snapshot_regular_file(
        path,
        label,
        collect=collect_png,
        maximum_collected_bytes=(
            MAXIMUM_PNG_FILE_BYTES if collect_png else None
        ),
    )
    if kind == "png":
        if data is None:
            raise CompileError("internal PNG read failure for {}".format(label))
        verify_png_data(data, label)
    elif kind == "recording" and snapshot.size < MINIMUM_RECORDING_BYTES:
        raise CompileError(
            "{} recording must be at least {} bytes".format(
                label, MINIMUM_RECORDING_BYTES
            )
        )
    return path_text, snapshot


def artifact_record(
    root: Path, raw: Any, label: str
) -> Tuple[Dict[str, str], FileSnapshot]:
    artifact = require_object(raw, label)
    require_exact_keys(artifact, {"path", "kind"}, label)
    kind = require_string(artifact["kind"], label + ".kind")
    if kind not in {"png", "recording"}:
        raise CompileError("{} kind must be png or recording".format(label))
    path, snapshot = snapshot_artifact(root, artifact["path"], kind, label)
    return {
        "path": path,
        "sha256": snapshot.sha256,
        "kind": kind,
    }, snapshot


def existing_artifacts(
    root: Path, evidence: Mapping[str, Any], evidence_id: str
) -> Tuple[List[Dict[str, str]], Dict[str, FileSnapshot]]:
    raw_artifacts = evidence.get("artifacts")
    if not isinstance(raw_artifacts, list):
        raise CompileError("evidence.{}.artifacts must be an array".format(evidence_id))
    result = []
    snapshots: Dict[str, FileSnapshot] = {}
    for index, raw in enumerate(raw_artifacts):
        label = "evidence.{}.artifacts[{}]".format(evidence_id, index)
        item = require_object(raw, label)
        require_exact_keys(item, {"path", "sha256", "kind"}, label)
        digest = require_string(item["sha256"], label + ".sha256")
        kind = require_string(item["kind"], label + ".kind")
        if kind not in {"png", "recording", "log", "report"}:
            raise CompileError("{} has an unsupported kind".format(label))
        path, snapshot = snapshot_artifact(root, item["path"], kind, label)
        if not SHA256_RE.fullmatch(digest) or snapshot.sha256 != digest:
            raise CompileError("{} hash mismatch".format(label))
        previous = snapshots.get(path)
        if previous is not None and previous != snapshot:
            raise CompileError("existing artifact changed while compiling: {}".format(path))
        snapshots[path] = snapshot
        result.append({"path": path, "sha256": digest, "kind": kind})
    return result, snapshots


def support_artifacts(
    root: Path, manifest: Mapping[str, Any]
) -> Tuple[Dict[str, List[Dict[str, str]]], Dict[str, FileSnapshot]]:
    raw_groups = manifest["supporting_artifacts"]
    if not isinstance(raw_groups, list) or len(raw_groups) != len(EVIDENCE_IDS):
        raise CompileError("supporting_artifacts must contain the five canonical categories")
    groups: Dict[str, List[Dict[str, str]]] = {}
    snapshots: Dict[str, FileSnapshot] = {}
    for index, (raw_group, expected_id) in enumerate(zip(raw_groups, EVIDENCE_IDS)):
        group = require_object(raw_group, "supporting_artifacts[{}]".format(index))
        require_exact_keys(group, {"evidence_id", "artifacts"}, "supporting artifact group")
        if group["evidence_id"] != expected_id:
            raise CompileError("supporting artifact category order is not canonical")
        if not isinstance(group["artifacts"], list) or not group["artifacts"]:
            raise CompileError("supporting artifact category {} is empty".format(expected_id))
        records = []
        for artifact_index, raw_artifact in enumerate(group["artifacts"]):
            label = "supporting_artifacts[{}].artifacts[{}]".format(index, artifact_index)
            record, snapshot = artifact_record(root, raw_artifact, label)
            if record["path"] in snapshots:
                raise CompileError("supporting artifact path is duplicated: {}".format(record["path"]))
            snapshots[record["path"]] = snapshot
            records.append(record)
        groups[expected_id] = records
    return groups, snapshots


def merge_snapshots(
    destination: Dict[str, FileSnapshot],
    additions: Mapping[str, FileSnapshot],
) -> None:
    for path, snapshot in additions.items():
        previous = destination.get(path)
        if previous is not None and previous != snapshot:
            raise CompileError("artifact changed while compiling: {}".format(path))
        destination[path] = snapshot


def merge_artifacts(*groups: Sequence[Dict[str, str]]) -> List[Dict[str, str]]:
    result: List[Dict[str, str]] = []
    by_path: Dict[str, Dict[str, str]] = {}
    for group in groups:
        for item in group:
            previous = by_path.get(item["path"])
            if previous is not None:
                if previous != item:
                    raise CompileError("artifact metadata conflicts for {}".format(item["path"]))
                continue
            copied = dict(item)
            by_path[item["path"]] = copied
            result.append(copied)
    return result


def update_status_rows(
    status: Dict[str, Any], observations: Sequence[Dict[str, Any]], manifest: Mapping[str, Any]
) -> None:
    section_names = {
        "gameplay": "gameplay",
        "base": "bases",
        "pickup": "pickups",
        "settlement": "settlement",
    }
    maps: Dict[str, Dict[Tuple[str, Optional[str]], Dict[str, Any]]] = {}
    for section, status_key in section_names.items():
        rows = status.get(status_key)
        if not isinstance(rows, list):
            raise CompileError("status.{} must be an array".format(status_key))
        mapped: Dict[Tuple[str, Optional[str]], Dict[str, Any]] = {}
        for raw in rows:
            row = require_object(raw, "status row")
            key = (row.get("id"), row.get("mode"))
            if key in mapped:
                raise CompileError("status.{} contains duplicate rows".format(status_key))
            mapped[key] = row  # type: ignore[assignment]
        maps[section] = mapped

    used: Dict[str, set] = {key: set() for key in maps}
    for observation in observations:
        section = observation["section"]
        key = (observation["id"], observation["mode"])
        row = maps[section].get(key)
        if row is None:
            raise CompileError("status is missing coverage row {}".format(observation["coverage_token"]))
        if row.get("status") == "PASS":
            raise CompileError("status row is already PASS: {}".format(observation["coverage_token"]))
        row["status"] = "PASS"
        row["tester"] = manifest["tester"]
        row["tested_at_utc"] = manifest["completed_at_utc"]
        row["evidence_ids"] = [observation["evidence_id"]]
        row["checks_confirmed"] = list(observation["required_checks"])
        row["notes"] = observation["notes"]
        used[section].add(key)
    for section, mapped in maps.items():
        if set(mapped) != used[section]:
            raise CompileError("status.{} does not exactly match the token plan".format(section_names[section]))


def evidence_map(status: Mapping[str, Any]) -> Dict[str, Dict[str, Any]]:
    values = status.get("evidence")
    if not isinstance(values, list):
        raise CompileError("status.evidence must be an array")
    result: Dict[str, Dict[str, Any]] = {}
    for raw in values:
        record = require_object(raw, "status evidence")
        evidence_id = record.get("id")
        if isinstance(evidence_id, str):
            if evidence_id in result:
                raise CompileError("status.evidence has duplicate IDs")
            result[evidence_id] = record  # type: ignore[assignment]
    for evidence_id in EVIDENCE_IDS:
        if evidence_id not in result:
            raise CompileError("status is missing evidence {}".format(evidence_id))
        if result[evidence_id].get("status") == "PASS":
            raise CompileError("status evidence is already PASS: {}".format(evidence_id))
    return result


def command_compile(args: argparse.Namespace) -> None:
    root = require_real_directory(Path(args.project_root), "project root")
    status_path = Path(args.status)
    if not status_path.is_absolute():
        status_path = root / status_path
    manifest_path = Path(args.manifest)
    if not manifest_path.is_absolute():
        manifest_path = root / manifest_path
    require_no_symlink_components(root, status_path, "source status")
    require_no_symlink_components(root, manifest_path, "observation manifest")
    status_raw, status_value = load_json(status_path, "source status")
    manifest_raw, manifest_value = load_json(manifest_path, "observation manifest")
    status_source = require_object(status_value, "source status")
    _, profile = resolve_status_requirements(root, status_source)
    release = require_object(status_source.get("release"), "status.release")
    artifact = require_object(release.get("artifact"), "status.release.artifact")
    candidate_sha256 = require_string(artifact.get("sha256"), "status.release.artifact.sha256")
    if not SHA256_RE.fullmatch(candidate_sha256):
        raise CompileError("status release artifact has an invalid SHA-256")
    manifest, observations = validate_manifest(manifest_value, profile, candidate_sha256)
    supports, input_snapshots = support_artifacts(root, manifest)

    output_dir, output_rel = path_inside_root(root, Path(args.output_dir), "output directory")
    if os.path.lexists(str(output_dir)):
        raise CompileError("output directory already exists: {}".format(output_dir))
    require_no_symlink_components(root, output_dir.parent, "output parent")
    require_real_directory(output_dir.parent, "output parent")

    next_status: Dict[str, Any] = copy.deepcopy(status_source)
    update_status_rows(next_status, observations, manifest)
    evidence = evidence_map(next_status)
    canonical_manifest = json_bytes(manifest)
    manifest_digest = sha256_bytes(canonical_manifest)
    manifest_name = "observation-manifest.json"
    manifest_record = {
        "path": "{}/{}".format(output_rel, manifest_name),
        "sha256": manifest_digest,
        "kind": "report",
    }
    output_files: Dict[str, bytes] = {manifest_name: canonical_manifest}

    observations_by_evidence = {
        evidence_id: [item for item in observations if item["evidence_id"] == evidence_id]
        for evidence_id in EVIDENCE_IDS
    }
    for evidence_id in EVIDENCE_IDS:
        record = evidence[evidence_id]
        current, current_snapshots = existing_artifacts(root, record, evidence_id)
        merge_snapshots(input_snapshots, current_snapshots)
        event_name = "{}-events.json".format(evidence_id)
        event_path = "{}/{}".format(output_rel, event_name)
        coverage_refs = {
            item["coverage_token"]: "event:{}".format(index + 1)
            for index, item in enumerate(observations_by_evidence[evidence_id])
        }
        event_log = {
            "schema": EVENT_LOG_SCHEMA,
            "producer": EVENT_LOG_PRODUCER,
            OBSERVATION_MANIFEST_SHA256_KEY: manifest_digest,
            "candidate_sha256": candidate_sha256,
            "tester": manifest["tester"],
            "machine": manifest["machine"],
            "started_at_utc": manifest["started_at_utc"],
            "completed_at_utc": manifest["completed_at_utc"],
            "events": [
                {
                    "sequence": index + 1,
                    "timestamp_utc": item["observed_at_utc"],
                    "category_id": evidence_id,
                    "coverage_token": item["coverage_token"],
                    "result": "PASS",
                }
                for index, item in enumerate(observations_by_evidence[evidence_id])
            ],
        }
        event_data = json_bytes(event_log)
        output_files[event_name] = event_data
        event_record = {
            "path": event_path,
            "sha256": sha256_bytes(event_data),
            "kind": "log",
        }
        without_session = merge_artifacts(
            current, supports[evidence_id], [manifest_record], [event_record]
        )
        session_name = "{}-session.json".format(evidence_id)
        session_path = "{}/{}".format(output_rel, session_name)
        session = {
            "schema": INTERACTIVE_SESSION_SCHEMA,
            "candidate_sha256": candidate_sha256,
            "tester": manifest["tester"],
            "machine": manifest["machine"],
            "started_at_utc": manifest["started_at_utc"],
            "completed_at_utc": manifest["completed_at_utc"],
            "signature": manifest["tester_signature"],
            "categories": [
                {
                    "id": evidence_id,
                    "tested_at_utc": manifest["completed_at_utc"],
                    "reviewed_at_utc": manifest["reviewed_at_utc"],
                    "result": "PASS",
                    "coverage_refs": coverage_refs,
                    "artifact_sha256s": [item["sha256"] for item in without_session],
                }
            ],
        }
        session_data = json_bytes(session)
        output_files[session_name] = session_data
        session_record = {
            "path": session_path,
            "sha256": sha256_bytes(session_data),
            "kind": "report",
        }
        record["status"] = "PASS"
        record["artifacts"] = without_session + [session_record]
        record["reviewer"] = manifest["reviewer"]
        record["reviewed_at_utc"] = manifest["reviewed_at_utc"]
        record["interactive"] = {
            "candidate_sha256": candidate_sha256,
            "tester": manifest["tester"],
            "machine": manifest["machine"],
            "tested_at_utc": manifest["completed_at_utc"],
            "signature": manifest["tester_signature"],
            "coverage_refs": coverage_refs,
        }
        record["notes"] = manifest["review_notes"]

    output_files["status.next.json"] = json_bytes(next_status)

    # Close time-of-check/time-of-use windows before publishing any output.
    if read_regular_file(status_path, "source status") != status_raw:
        raise CompileError("source status changed during compilation")
    if read_regular_file(manifest_path, "observation manifest") != manifest_raw:
        raise CompileError("observation manifest changed during compilation")
    for raw_path, original in input_snapshots.items():
        _, path = repository_path(root, raw_path, "input artifact")
        current, _ = snapshot_regular_file(path, "input artifact")
        if current != original:
            raise CompileError("input artifact changed during compilation: {}".format(raw_path))

    parent_fd: Optional[int] = None
    directory_fd: Optional[int] = None
    created_identity: Optional[Tuple[int, int]] = None
    created_names: List[str] = []
    try:
        flags = os.O_RDONLY
        if hasattr(os, "O_DIRECTORY"):
            flags |= os.O_DIRECTORY
        if hasattr(os, "O_NOFOLLOW"):
            flags |= os.O_NOFOLLOW
        parent_fd = os.open(str(output_dir.parent), flags)
        os.mkdir(output_dir.name, 0o700, dir_fd=parent_fd)
        created_metadata = os.stat(
            output_dir.name, dir_fd=parent_fd, follow_symlinks=False
        )
        if not stat.S_ISDIR(created_metadata.st_mode):
            raise CompileError("created output path is not a directory")
        created_identity = (created_metadata.st_dev, created_metadata.st_ino)
        directory_fd = os.open(output_dir.name, flags, dir_fd=parent_fd)
        opened_metadata = os.fstat(directory_fd)
        if (opened_metadata.st_dev, opened_metadata.st_ino) != created_identity:
            raise CompileError("output directory was replaced while opening it")
        for name, data in output_files.items():
            exclusive_write_at(directory_fd, name, data)
            created_names.append(name)
        os.fsync(directory_fd)
        print("Compiled interactive evidence pack: {}".format(output_dir))
        print("Next status (source unchanged): {}".format(output_dir / "status.next.json"))
    except Exception:
        if directory_fd is not None:
            for name in reversed(created_names):
                try:
                    os.unlink(name, dir_fd=directory_fd)
                except OSError:
                    pass
        if parent_fd is not None and created_identity is not None:
            try:
                current_metadata = os.stat(
                    output_dir.name, dir_fd=parent_fd, follow_symlinks=False
                )
                if (
                    stat.S_ISDIR(current_metadata.st_mode)
                    and (current_metadata.st_dev, current_metadata.st_ino)
                    == created_identity
                ):
                    os.rmdir(output_dir.name, dir_fd=parent_fd)
            except OSError:
                pass
        raise
    finally:
        if directory_fd is not None:
            os.close(directory_fd)
        if parent_fd is not None:
            os.close(parent_fd)


def parser() -> argparse.ArgumentParser:
    result = argparse.ArgumentParser(description=__doc__)
    subparsers = result.add_subparsers(dest="command", required=True)
    init = subparsers.add_parser("init-plan", help="create an all-NOT_RUN observation plan")
    init.add_argument("--requirements", required=True)
    init.add_argument("--output", required=True)
    init.set_defaults(function=command_init_plan)
    compile_parser = subparsers.add_parser("compile", help="compile an explicitly completed plan")
    compile_parser.add_argument("--project-root", required=True)
    compile_parser.add_argument("--status", required=True)
    compile_parser.add_argument("--manifest", required=True)
    compile_parser.add_argument("--output-dir", required=True)
    compile_parser.set_defaults(function=command_compile)
    return result


def main(argv: Optional[Sequence[str]] = None) -> int:
    args = parser().parse_args(argv)
    try:
        args.function(args)
    except (CompileError, OSError) as exc:
        print("error: {}".format(exc), file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
