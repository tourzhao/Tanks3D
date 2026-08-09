#!/usr/bin/env python3
"""Create a no-overwrite, honestly BLOCKED Alpha-v2 release baseline.

This helper never records human QA as complete.  It verifies one immutable
candidate, validates seven publication image inputs, and creates the versioned
release page, QA report, status JSON, and copied images without overwriting an
existing destination.  Ordinary failures receive best-effort rollback; an
abruptly terminated process may leave files that must be reviewed and removed
manually.  Generated files must be reviewed and committed before the
clean-worktree release-status verifier can run.
"""

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import stat
import struct
import subprocess
import sys
import tempfile
from typing import Any, Callable, Dict, Iterable, List, Mapping, Optional, Sequence, Tuple
import zlib


STATUS_SCHEMA = "tanks3d-release-status-v1"
REQUIREMENTS_PATH = Path("docs/release-requirements/macos-alpha-v2.json")
REQUIREMENTS_SCHEMA = "tanks3d-release-requirements-v2"
REQUIREMENTS_PROFILE = "macos-alpha-v2"
CANONICAL_REQUIREMENTS_DIGEST = (
    "7a0f5eaac3e8ec0bc4726cc7084a16e34f548735381c397f3c152a503d2cabf9"
)
SCREENSHOT_NAMES = (
    "one-player.png",
    "two-player.png",
    "base-usa.png",
    "base-ussr.png",
    "base-germany.png",
    "bonuses.png",
    "settlement.png",
)
SCREENSHOT_MEMBERSHIP = {
    "one_player_gameplay": ("one-player.png",),
    "two_player_gameplay": ("two-player.png",),
    "national_bases": ("base-usa.png", "base-ussr.png", "base-germany.png"),
    "pickup_and_minimap": ("bonuses.png",),
    "settlement_report": ("settlement.png",),
}
EMPTY_EVIDENCE_IDS = {
    "main_menu_and_advanced_settings",
    "gatekeeper_launch",
    "extended_session_metrics",
}
GATE_KEYS = (
    "gate_clean",
    "gate_test_alpha_candidate",
    "gate_debug",
    "gate_test_architecture",
    "gate_test",
    "gate_test_sanitize",
    "gate_coverage",
    "gate_test_dist",
)
ATTESTATION_KEYS = {
    "schema",
    "source_commit",
    "source_head_at_start",
    "source_head_at_finish",
    "source_tag",
    "source_tag_commit",
    "source_tree",
    "app_version",
    "dist_channel",
    "dist_arch",
    "dist_macos_min",
    "artifact_filename",
    "artifact_sha256",
    "checksum_filename",
    "build_config_filename",
    "build_config_sha256",
    "gate_log_filename",
    "gate_log_sha256",
    *GATE_KEYS,
}
VERSION_RE = re.compile(
    r"^(?:0|[1-9][0-9]*)\.(?:0|[1-9][0-9]*)\.(?:0|[1-9][0-9]*)$"
)
CHANNEL_RE = re.compile(r"^alpha\.(?:0|[1-9][0-9]*)$")
COMMIT_RE = re.compile(r"^(?:[0-9a-f]{40}|[0-9a-f]{64})$")
SHA256_RE = re.compile(r"^[0-9a-f]{64}$")
TOKEN_RE = re.compile(r"^[A-Za-z0-9][A-Za-z0-9._-]*$")


class InitError(Exception):
    """A release baseline cannot be created safely."""


def reject_duplicate_pairs(pairs: Sequence[Tuple[str, Any]]) -> Dict[str, Any]:
    result: Dict[str, Any] = {}
    for key, value in pairs:
        if key in result:
            raise InitError("duplicate JSON key: {!r}".format(key))
        result[key] = value
    return result


def load_json_strict(path: Path, label: str) -> Any:
    data = read_regular_file(path, label)
    try:
        return json.loads(data.decode("utf-8"), object_pairs_hook=reject_duplicate_pairs)
    except (UnicodeError, json.JSONDecodeError) as exc:
        raise InitError("{} is not valid JSON: {}".format(label, exc))


def require_object(value: Any, label: str) -> Mapping[str, Any]:
    if not isinstance(value, dict):
        raise InitError("{} must be an object".format(label))
    return value


def require_string_list(value: Any, label: str) -> List[str]:
    if not isinstance(value, list) or any(not isinstance(item, str) for item in value):
        raise InitError("{} must be an array of strings".format(label))
    if len(value) != len(set(value)):
        raise InitError("{} contains duplicate values".format(label))
    return list(value)


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def read_regular_file(path: Path, label: str) -> bytes:
    flags = os.O_RDONLY
    if hasattr(os, "O_NONBLOCK"):
        flags |= os.O_NONBLOCK
    if hasattr(os, "O_NOFOLLOW"):
        flags |= os.O_NOFOLLOW
    try:
        descriptor = os.open(str(path), flags)
    except OSError as exc:
        raise InitError("cannot open {}: {}".format(label, exc))
    try:
        metadata = os.fstat(descriptor)
        if not stat.S_ISREG(metadata.st_mode):
            raise InitError("{} must be a regular non-symlink file".format(label))
        chunks = []
        while True:
            chunk = os.read(descriptor, 1024 * 1024)
            if not chunk:
                break
            chunks.append(chunk)
        final_metadata = os.fstat(descriptor)
        if (
            metadata.st_dev,
            metadata.st_ino,
            metadata.st_size,
            metadata.st_mtime_ns,
            metadata.st_ctime_ns,
        ) != (
            final_metadata.st_dev,
            final_metadata.st_ino,
            final_metadata.st_size,
            final_metadata.st_mtime_ns,
            final_metadata.st_ctime_ns,
        ):
            raise InitError("{} changed while it was read".format(label))
        data = b"".join(chunks)
        if len(data) != final_metadata.st_size:
            raise InitError("{} changed size while it was read".format(label))
        return data
    finally:
        os.close(descriptor)


def require_real_directory(path: Path, label: str) -> Path:
    try:
        metadata = path.lstat()
    except OSError as exc:
        raise InitError("{} is not accessible: {}".format(label, exc))
    if stat.S_ISLNK(metadata.st_mode) or not stat.S_ISDIR(metadata.st_mode):
        raise InitError("{} must be a real directory, not a symlink".format(label))
    return path.resolve(strict=True)


def require_no_symlink_components(root: Path, path: Path, label: str) -> None:
    """Reject every repository-relative symlink in an existing path."""
    try:
        relative = path.relative_to(root)
    except ValueError:
        raise InitError("{} is outside the project root".format(label))
    current = root
    for part in relative.parts:
        current = current / part
        try:
            metadata = current.lstat()
        except OSError as exc:
            raise InitError("{} is not accessible: {}".format(label, exc))
        if stat.S_ISLNK(metadata.st_mode):
            raise InitError("{} has a symlinked path component".format(label))


def require_canonical_path(actual: Path, expected: Path, label: str) -> None:
    try:
        actual_resolved = actual.resolve(strict=False)
        expected_resolved = expected.resolve(strict=False)
    except OSError as exc:
        raise InitError("cannot resolve {}: {}".format(label, exc))
    if actual_resolved != expected_resolved:
        raise InitError("{} must be {}".format(label, expected))


def parse_attestation(data: bytes) -> Dict[str, str]:
    try:
        lines = data.decode("utf-8").splitlines()
    except UnicodeError as exc:
        raise InitError("attestation.txt is not UTF-8: {}".format(exc))
    result: Dict[str, str] = {}
    for number, line in enumerate(lines, 1):
        if not line or "=" not in line:
            raise InitError("malformed attestation line {}".format(number))
        key, value = line.split("=", 1)
        if not key or key in result:
            raise InitError("duplicate or empty attestation key on line {}".format(number))
        result[key] = value
    actual = set(result)
    if actual != ATTESTATION_KEYS:
        missing = sorted(ATTESTATION_KEYS - actual)
        extra = sorted(actual - ATTESTATION_KEYS)
        details = []
        if missing:
            details.append("missing {}".format(", ".join(missing)))
        if extra:
            details.append("unexpected {}".format(", ".join(extra)))
        raise InitError("attestation has invalid keys ({})".format("; ".join(details)))
    return result


def run_tagged_verifier(root: Path, candidate_dir: Path) -> Dict[str, str]:
    verifier = root / "scripts/verify_tagged_alpha_candidate.sh"
    require_no_symlink_components(root, verifier, "tagged candidate verifier")
    verifier_parent_fd = open_repository_directory_no_follow(
        root, verifier.parent, "tagged verifier directory"
    )
    try:
        verifier_data = read_regular_file_at(
            verifier_parent_fd, verifier.name, "tagged candidate verifier"
        )
    finally:
        os.close(verifier_parent_fd)
    try:
        completed = subprocess.run(
            ["sh", "-s", "--", str(root), str(candidate_dir)],
            input=verifier_data,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            check=False,
        )
    except OSError as exc:
        raise InitError("cannot run tagged candidate verifier: {}".format(exc))
    if completed.returncode != 0:
        detail = (completed.stderr or completed.stdout).decode(
            "utf-8", errors="replace"
        ).strip()
        if len(detail) > 800:
            detail = detail[-800:]
        raise InitError(
            "tagged candidate verifier failed (exit {}): {}".format(
                completed.returncode, detail or "no diagnostic"
            )
        )
    try:
        verifier_stdout = completed.stdout.decode("utf-8")
    except UnicodeError as exc:
        raise InitError("tagged verifier output is not UTF-8: {}".format(exc))
    receipt: Dict[str, str] = {}
    prefix = "VERIFIED CANDIDATE FILE SHA256 "
    for line in verifier_stdout.splitlines():
        if not line.startswith(prefix):
            continue
        fields = line[len(prefix) :].split(" ", 1)
        if len(fields) != 2:
            raise InitError("tagged verifier emitted a malformed candidate receipt")
        digest, name = fields
        if SHA256_RE.fullmatch(digest) is None or TOKEN_RE.fullmatch(name) is None:
            raise InitError("tagged verifier emitted an invalid candidate receipt")
        if name in receipt:
            raise InitError("tagged verifier emitted a duplicate candidate receipt")
        receipt[name] = digest
    if len(receipt) != 5:
        raise InitError("tagged verifier did not emit an exact five-file receipt")
    return receipt


def capture_candidate_files(candidate_dir: Path) -> Dict[str, bytes]:
    try:
        entries = set(os.listdir(str(candidate_dir)))
    except OSError as exc:
        raise InitError("cannot list candidate directory: {}".format(exc))
    fixed = {"attestation.txt", "alpha-candidate-gates.log", "build-config.txt"}
    zip_names = [name for name in entries if name.endswith(".zip")]
    checksum_names = [name for name in entries if name.endswith(".zip.sha256")]
    if (
        len(entries) != 5
        or not fixed.issubset(entries)
        or len(zip_names) != 1
        or checksum_names != [zip_names[0] + ".sha256"]
    ):
        raise InitError("candidate directory must contain exactly its five canonical files")
    return {
        name: read_regular_file(
            candidate_dir / name, "candidate file {}".format(name)
        )
        for name in entries
    }


def validate_candidate(root: Path, candidate_argument: Path) -> Dict[str, Any]:
    candidate_dir = require_real_directory(candidate_argument, "candidate directory")
    release_root = require_real_directory(root / "build" / "release", "release directory")
    require_no_symlink_components(root, candidate_dir, "candidate directory")
    if candidate_dir.parent != release_root:
        raise InitError("candidate directory must be directly under build/release")

    before_verification = capture_candidate_files(candidate_dir)
    verified_receipt = run_tagged_verifier(root, candidate_dir)
    after_verification = capture_candidate_files(candidate_dir)
    if before_verification != after_verification:
        raise InitError("candidate changed while the tagged verifier was running")
    captured_receipt = {
        name: sha256_bytes(data) for name, data in after_verification.items()
    }
    if verified_receipt != captured_receipt:
        raise InitError("candidate does not match the tagged verifier's private snapshot")

    attestation_data = after_verification["attestation.txt"]
    attestation = parse_attestation(attestation_data)
    version = attestation["app_version"]
    channel = attestation["dist_channel"]
    tag = attestation["source_tag"]
    commit = attestation["source_commit"]
    if VERSION_RE.fullmatch(version) is None:
        raise InitError("attested app_version is not canonical")
    if CHANNEL_RE.fullmatch(channel) is None:
        raise InitError("attested dist_channel is not canonical")
    if tag != "v{}-{}".format(version, channel) or TOKEN_RE.fullmatch(tag) is None:
        raise InitError("attested source_tag does not match version and channel")
    if candidate_dir != release_root / tag:
        raise InitError("candidate directory name does not match the attested tag")
    if COMMIT_RE.fullmatch(commit) is None:
        raise InitError("attested source_commit is not a full lowercase object ID")
    for key in ("source_head_at_start", "source_head_at_finish", "source_tag_commit"):
        if attestation[key] != commit:
            raise InitError("attestation {} does not match source_commit".format(key))
    if attestation["schema"] != "tanks3d-alpha-candidate-v2":
        raise InitError("candidate must use tanks3d-alpha-candidate-v2")
    if attestation["source_tree"] != "clean":
        raise InitError("candidate was not built from a clean tree")
    for key in GATE_KEYS:
        if attestation[key] != "PASS":
            raise InitError("attestation {} is not PASS".format(key))

    for key in ("dist_arch", "dist_macos_min"):
        if TOKEN_RE.fullmatch(attestation[key]) is None:
            raise InitError("attestation {} contains an unsafe value".format(key))
    expected_artifact = "Tanks3D-{}-{}-macos-{}-macos{}.zip".format(
        version,
        channel,
        attestation["dist_arch"],
        attestation["dist_macos_min"],
    )
    filenames = {
        "artifact": expected_artifact,
        "checksum": expected_artifact + ".sha256",
        "attestation": "attestation.txt",
        "gate_log": "alpha-candidate-gates.log",
        "build_config": "build-config.txt",
    }
    expected_attestation_names = {
        "artifact_filename": filenames["artifact"],
        "checksum_filename": filenames["checksum"],
        "build_config_filename": filenames["build_config"],
        "gate_log_filename": filenames["gate_log"],
    }
    for key, expected in expected_attestation_names.items():
        if attestation[key] != expected:
            raise InitError("attestation {} is not canonical".format(key))

    if set(after_verification) != set(filenames.values()):
        raise InitError("candidate directory must contain exactly its five canonical files")

    contents = {
        key: after_verification[name]
        for key, name in filenames.items()
    }
    digests = {key: sha256_bytes(data) for key, data in contents.items()}
    for key, digest_key in (
        ("artifact", "artifact_sha256"),
        ("build_config", "build_config_sha256"),
        ("gate_log", "gate_log_sha256"),
    ):
        value = attestation[digest_key]
        if SHA256_RE.fullmatch(value) is None or value != digests[key]:
            raise InitError("attestation {} does not match the candidate".format(digest_key))
    expected_checksum = "{}  {}\n".format(
        digests["artifact"], filenames["artifact"]
    ).encode("ascii")
    if contents["checksum"] != expected_checksum:
        raise InitError("candidate checksum file is not canonical")

    return {
        "dir": candidate_dir,
        "version": version,
        "channel": channel,
        "tag": tag,
        "commit": commit,
        "filenames": filenames,
        "digests": digests,
        "attestation": attestation,
        "snapshot": after_verification,
    }


def paeth_predictor(left: int, above: int, upper_left: int) -> int:
    estimate = left + above - upper_left
    left_distance = abs(estimate - left)
    above_distance = abs(estimate - above)
    upper_left_distance = abs(estimate - upper_left)
    if left_distance <= above_distance and left_distance <= upper_left_distance:
        return left
    if above_distance <= upper_left_distance:
        return above
    return upper_left


def normalized_rgba_hash(compressed: bytes, width: int, height: int, label: str) -> str:
    row_bytes = width * 4
    expected_size = height * (row_bytes + 1)
    try:
        decoder = zlib.decompressobj()
        raw = decoder.decompress(compressed, expected_size + 1)
        if len(raw) > expected_size or decoder.unconsumed_tail:
            raise InitError("{} exceeds its canonical decoded PNG size".format(label))
        raw += decoder.flush(expected_size + 1 - len(raw))
    except zlib.error as exc:
        raise InitError("{} has an invalid PNG image stream: {}".format(label, exc))
    if not decoder.eof or decoder.unused_data or decoder.unconsumed_tail:
        raise InitError("{} has a non-canonical PNG image stream".format(label))
    if len(raw) != expected_size:
        raise InitError(
            "{} has an invalid decoded PNG size: expected {}, got {}".format(
                label, expected_size, len(raw)
            )
        )
    pixels = bytearray()
    previous = bytearray(row_bytes)
    offset = 0
    for _ in range(height):
        filter_type = raw[offset]
        offset += 1
        current = bytearray(raw[offset : offset + row_bytes])
        offset += row_bytes
        if filter_type > 4:
            raise InitError("{} has an unsupported PNG row filter".format(label))
        for index in range(row_bytes):
            left = current[index - 4] if index >= 4 else 0
            above = previous[index]
            upper_left = previous[index - 4] if index >= 4 else 0
            if filter_type == 1:
                current[index] = (current[index] + left) & 0xFF
            elif filter_type == 2:
                current[index] = (current[index] + above) & 0xFF
            elif filter_type == 3:
                current[index] = (current[index] + ((left + above) // 2)) & 0xFF
            elif filter_type == 4:
                current[index] = (
                    current[index] + paeth_predictor(left, above, upper_left)
                ) & 0xFF
        pixels.extend(current)
        previous = current
    return sha256_bytes(bytes(pixels))


def validate_png(data: bytes, label: str) -> str:
    if not data.startswith(b"\x89PNG\r\n\x1a\n"):
        raise InitError("{} is not a PNG".format(label))
    offset = 8
    chunks: List[bytes] = []
    width = height = None
    image_data = []
    image_format = None
    idat_started = False
    idat_finished = False
    known_critical = {b"IHDR", b"PLTE", b"IDAT", b"IEND"}
    while offset < len(data):
        if len(data) - offset < 12:
            raise InitError("{} has a truncated PNG chunk".format(label))
        length = struct.unpack(">I", data[offset : offset + 4])[0]
        chunk_type = data[offset + 4 : offset + 8]
        end = offset + 12 + length
        if end > len(data):
            raise InitError("{} has a truncated PNG payload".format(label))
        payload = data[offset + 8 : offset + 8 + length]
        recorded_crc = struct.unpack(">I", data[offset + 8 + length : end])[0]
        actual_crc = zlib.crc32(chunk_type)
        actual_crc = zlib.crc32(payload, actual_crc) & 0xFFFFFFFF
        if recorded_crc != actual_crc:
            raise InitError("{} has an invalid PNG chunk CRC".format(label))
        if chunk_type not in known_critical and not (chunk_type[0] & 0x20):
            raise InitError("{} has an unknown critical PNG chunk".format(label))
        chunks.append(chunk_type)
        if len(chunks) == 1:
            if chunk_type != b"IHDR" or length != 13:
                raise InitError("{} has no canonical IHDR".format(label))
            width, height = struct.unpack(">II", payload[:8])
            image_format = struct.unpack(">BBBBB", payload[8:13])
        elif chunk_type == b"IHDR":
            raise InitError("{} has more than one IHDR".format(label))
        if chunk_type == b"IDAT":
            if idat_finished:
                raise InitError("{} has non-contiguous IDAT chunks".format(label))
            idat_started = True
            image_data.append(payload)
        elif idat_started and chunk_type != b"IEND":
            idat_finished = True
        if chunk_type == b"IEND":
            if length != 0 or end != len(data):
                raise InitError("{} has an invalid IEND".format(label))
            offset = end
            break
        offset = end
    if not chunks or chunks[-1] != b"IEND" or not image_data:
        raise InitError("{} is an incomplete PNG".format(label))
    if (width, height) != (1280, 720):
        raise InitError("{} must be 1280x720, got {}x{}".format(label, width, height))
    if image_format != (8, 6, 0, 0, 0):
        raise InitError("{} must be 8-bit RGBA and non-interlaced".format(label))
    return normalized_rgba_hash(b"".join(image_data), width, height, label)


def validate_screenshots(
    root: Path, tag: str, screenshot_argument: Path
) -> Dict[str, bytes]:
    expected = root / "build" / "release-evidence" / tag / "screenshots"
    require_canonical_path(screenshot_argument, expected, "screenshot input directory")
    require_no_symlink_components(root, expected, "screenshot input directory")
    screenshot_dir = require_real_directory(screenshot_argument, "screenshot input directory")
    if screenshot_dir != expected.resolve(strict=True):
        raise InitError("screenshot input directory resolves outside its canonical path")
    try:
        entries = set(os.listdir(str(screenshot_dir)))
    except OSError as exc:
        raise InitError("cannot list screenshot input directory: {}".format(exc))
    if entries != set(SCREENSHOT_NAMES):
        raise InitError("screenshot input directory must contain exactly seven canonical PNGs")

    screenshots: Dict[str, bytes] = {}
    seen_pixels = set()
    for name in SCREENSHOT_NAMES:
        data = read_regular_file(screenshot_dir / name, "screenshot {}".format(name))
        pixel_digest = validate_png(data, "screenshot {}".format(name))
        if pixel_digest in seen_pixels:
            raise InitError("release screenshots must show seven distinct images")
        seen_pixels.add(pixel_digest)
        screenshots[name] = data
    return screenshots


def validate_requirements(root: Path) -> Mapping[str, Any]:
    profile_path = root / REQUIREMENTS_PATH
    require_no_symlink_components(root, profile_path, "v2 requirements profile")
    profile = require_object(
        load_json_strict(profile_path, "v2 requirements profile"),
        "v2 requirements profile",
    )
    canonical_bytes = json.dumps(
        profile, sort_keys=True, separators=(",", ":"), ensure_ascii=True
    ).encode("utf-8")
    if sha256_bytes(canonical_bytes) != CANONICAL_REQUIREMENTS_DIGEST:
        raise InitError("v2 requirements profile does not match the canonical contract")
    if profile.get("schema") != REQUIREMENTS_SCHEMA:
        raise InitError("requirements profile is not v2")
    if profile.get("profile") != REQUIREMENTS_PROFILE:
        raise InitError("requirements profile name is not macos-alpha-v2")
    if profile.get("status_values") != ["PASS", "FAIL", "BLOCKED", "NOT_RUN"]:
        raise InitError("requirements status values are not canonical")
    required_arrays = (
        "modes",
        "gameplay_ids",
        "base_ids",
        "base_checks",
        "settlement_ids",
        "published_control_checks",
        "clean_mac_detail_keys",
        "gatekeeper_detail_keys",
        "extended_session_detail_keys",
        "interactive_evidence_keys",
        "evidence_ids",
        "document_gate_rows",
        "approval_roles",
    )
    for key in required_arrays:
        require_string_list(profile.get(key), "requirements.{}".format(key))
    if profile["modes"] != ["one_player", "two_player"]:
        raise InitError("requirements modes are not canonical")
    if profile["approval_roles"] != ["qa_lead", "release_owner"]:
        raise InitError("requirements approval roles are not canonical")
    pickup_requirements = profile.get("pickup_requirements")
    if not isinstance(pickup_requirements, list) or not pickup_requirements:
        raise InitError("requirements.pickup_requirements must be a non-empty array")
    pickup_ids = []
    for index, item in enumerate(pickup_requirements):
        record = require_object(item, "requirements.pickup_requirements[{}]".format(index))
        if set(record) != {"id", "checks"} or not isinstance(record["id"], str):
            raise InitError("pickup requirement has invalid keys")
        require_string_list(record["checks"], "pickup requirement checks")
        pickup_ids.append(record["id"])
    if len(pickup_ids) != len(set(pickup_ids)):
        raise InitError("requirements pickup IDs are not unique")
    expected_evidence = [
        "main_menu_and_advanced_settings",
        "one_player_gameplay",
        "two_player_gameplay",
        "national_bases",
        "pickup_and_minimap",
        "settlement_report",
        "gatekeeper_launch",
        "extended_session_metrics",
    ]
    if profile["evidence_ids"] != expected_evidence:
        raise InitError("requirements evidence IDs are not canonical")
    return profile


def matrix_record(item_id: str, mode: Optional[str] = None) -> Dict[str, Any]:
    record: Dict[str, Any] = {
        "id": item_id,
        "status": "NOT_RUN",
        "tester": "",
        "tested_at_utc": None,
        "evidence_ids": [],
        "checks_confirmed": [],
        "notes": "Not run against this candidate.",
    }
    if mode is not None:
        without_id = {key: value for key, value in record.items() if key != "id"}
        record = {"id": item_id, "mode": mode, **without_id}
    return record


def empty_interactive() -> Dict[str, Any]:
    return {
        "candidate_sha256": "",
        "tester": "",
        "machine": "",
        "tested_at_utc": None,
        "signature": "",
        "coverage_refs": {},
    }


def evidence_records(
    profile: Mapping[str, Any], tag: str, screenshot_hashes: Mapping[str, str]
) -> List[Dict[str, Any]]:
    records = []
    for evidence_id in profile["evidence_ids"]:
        names = SCREENSHOT_MEMBERSHIP.get(evidence_id, ())
        artifacts = [
            {
                "path": "docs/assets/releases/{}/{}".format(tag, name),
                "sha256": screenshot_hashes[name],
                "kind": "png",
            }
            for name in names
        ]
        if evidence_id in EMPTY_EVIDENCE_IDS and artifacts:
            raise InitError("internal evidence membership error")
        records.append(
            {
                "id": evidence_id,
                "status": "NOT_RUN",
                "artifacts": artifacts,
                "interactive": empty_interactive(),
                "reviewer": "",
                "reviewed_at_utc": None,
                "notes": "Publication image is unreviewed; interactive evidence is not recorded."
                if artifacts
                else "Evidence has not been collected for this candidate.",
            }
        )
    return records


def render_release_page(
    candidate: Mapping[str, Any],
    profile: Mapping[str, Any],
    screenshot_hashes: Mapping[str, str],
) -> str:
    tag = candidate["tag"]
    artifact = candidate["filenames"]["artifact"]
    artifact_sha = candidate["digests"]["artifact"]
    commit = candidate["commit"]
    if set(screenshot_hashes) != set(SCREENSHOT_NAMES):
        raise InitError("release page screenshot set is not canonical")
    gate_rows = "\n".join(
        "| {} | BLOCKED |".format(row) for row in profile["document_gate_rows"]
    )
    return """# Tanks 3D {tag}

_Classic four-direction tank combat in a fixed-camera isometric 3D battlefield._

![One-player gameplay](../assets/releases/{tag}/one-player.png)

> **Release status: BLOCKED.** This candidate-specific draft is not approved
> for publication. The images and automated candidate are bound below, while
> clean-Mac, interactive, performance, audio-risk, and approval gates remain.

See the [{tag} QA report]({tag}-qa.md) and
[{tag} machine-readable status]({tag}-status.json).

## Game overview

Tanks 3D is a non-commercial, isometric arcade tank game for Apple Silicon
macOS. It keeps four-direction movement, destructible defenses, opposing-shell
cancellation, local two-player co-op, national vehicle progression, 3D pickups,
a minimap, configurable HP and difficulty, and a classified end-stage report.

- 35 deterministic stages with one-player and shared-camera two-player modes.
- United States, Soviet, and German light-to-super-heavy vehicle lines.
- Destructible national objectives, temporary Shovel steel, HP/respawn, and
  direct-fire streaks.
- Nine pickups, including conditional Bandage healing and Star upgrades.
- Fixed 45-degree local camera plus a global minimap.

![Two-player gameplay](../assets/releases/{tag}/two-player.png)

## National bases

| United States | Soviet Union | Germany |
| --- | --- | --- |
| ![United States base](../assets/releases/{tag}/base-usa.png) | ![Soviet base](../assets/releases/{tag}/base-ussr.png) | ![German base](../assets/releases/{tag}/base-germany.png) |

## Pickups and settlement

![3D pickups](../assets/releases/{tag}/bonuses.png)

![Classified settlement](../assets/releases/{tag}/settlement.png)

## Controls

| Context | Controls |
| --- | --- |
| Menus | Arrow keys or `WASD`; `Enter` or `Space` confirms |
| Player 1 | Arrow keys move; `Space`, Right Option, or Right Control fires |
| Player 2 | `WASD` moves; `F`, Left Option, or Left Control fires |
| Battle | `Enter` pauses; `Esc` returns to setup; `R` restarts |
| Display/stage | `F8` quality; `F11` borderless; `N`/`B` stage |
| Exit | `Esc` or `Q` from the setup screen |

Controls remain unapproved until the linked exact-candidate matrix is signed.

## Candidate identity

- Release date: NOT RECORDED — BLOCKED
- Artifact: `{artifact}`
- SHA-256: `{artifact_sha}`
- Source commit: `{commit}`
- Source tag: `{tag}`
- Architecture/minimum OS: `{arch}` / macOS `{macos_min}`
- Signing: ad-hoc integrity signature; not Developer ID signed or notarized

Publish the ZIP only with its checksum, `attestation.txt`,
`alpha-candidate-gates.log`, and `build-config.txt`, after the final no-exception
release gate passes.

After downloading, verify the exact artifact before opening it:

```sh
shasum -a 256 {artifact}
```

The result must equal `{artifact_sha}`.

## Release gate summary

| Gate | Status |
| --- | --- |
{gate_rows}

Gatekeeper conclusion: **BLOCKED**

Known-issues review: **NONE — BLOCKED**

Audio decision: **NONE — BLOCKED**

## Alpha limitations and licensing

This draft remains blocked until quarantined-download Gatekeeper behavior,
one-/two-player QA, hardware audio, a 30-minute performance session, known
issues, and independent approvals are recorded. Project-owned code and assets
use the PolyForm Noncommercial License 1.0.0. Third-party materials retain their
own terms in `NOTICE`, `THIRD_PARTY_NOTICES.md`, and `ASSET_LICENSES.md`.
This independent fan project is not affiliated with or endorsed by any
publisher, manufacturer, government, or other rights holder.

## Author

Author: **tourzhao**

Required notice: `Copyright (c) 2026 tourzhao.`
""".format(
        tag=tag,
        artifact=artifact,
        artifact_sha=artifact_sha,
        commit=commit,
        arch=candidate["attestation"]["dist_arch"],
        macos_min=candidate["attestation"]["dist_macos_min"],
        gate_rows=gate_rows,
    )


def render_qa_report(
    candidate: Mapping[str, Any],
    profile: Mapping[str, Any],
    screenshot_hashes: Mapping[str, str],
) -> str:
    tag = candidate["tag"]
    filenames = candidate["filenames"]
    digests = candidate["digests"]
    lines = [
        "# Tanks 3D {} Alpha QA Report".format(tag),
        "",
        "> **Overall Alpha gate: BLOCKED.** This baseline records no human PASS",
        "> result. Replace NOT RUN only with exact-candidate evidence and accountable",
        "> signatures, then pass the no-exception verifier before publication.",
        "",
        "## Candidate identity",
        "",
        "| Field | Value |",
        "| --- | --- |",
        "| Tag | `{}` |".format(tag),
        "| Planned release date | NOT RECORDED — BLOCKED |",
        "| Source commit | `{}` |".format(candidate["commit"]),
        "| Candidate directory | `build/release/{}` |".format(tag),
        "| Artifact | `{}` |".format(filenames["artifact"]),
        "| Artifact SHA-256 | `{}` |".format(digests["artifact"]),
        "| Checksum | `{}` / `{}` |".format(filenames["checksum"], digests["checksum"]),
        "| Attestation | `attestation.txt` / `{}` |".format(digests["attestation"]),
        "| Gate log | `alpha-candidate-gates.log` / `{}` |".format(digests["gate_log"]),
        "| Build config | `build-config.txt` / `{}` |".format(digests["build_config"]),
        "",
        "## Publication images",
        "",
        "These files passed structural/hash checks only; that is not an interactive",
        "or visual-review PASS.",
        "",
        "| Image | SHA-256 | Review |",
        "| --- | --- | --- |",
    ]
    for name in SCREENSHOT_NAMES:
        lines.append(
            "| [![]({reference})]({reference}) | `{digest}` | NOT RUN |".format(
                reference="../assets/releases/{}/{}".format(tag, name),
                digest=screenshot_hashes[name],
            )
        )
    lines.extend(
        [
            "",
            "## One-player and two-player gameplay matrix",
            "",
            "| Scenario ID | One player | Two players |",
            "| --- | --- | --- |",
        ]
    )
    for item_id in profile["gameplay_ids"]:
        lines.append("| `{}` | NOT RUN | NOT RUN |".format(item_id))
    lines.extend(
        [
            "",
            "## National bases",
            "",
            "Every base must cover wall damage/breach, Shovel material and protection,",
            "collision, core loss, visibility, and emblem orientation in both modes.",
            "",
            "| Base ID | One player | Two players |",
            "| --- | --- | --- |",
        ]
    )
    for item_id in profile["base_ids"]:
        lines.append("| `{}` | NOT RUN | NOT RUN |".format(item_id))
    lines.extend(
        [
            "",
            "## Pickups",
            "",
            "| Pickup/check ID | One player | Two players |",
            "| --- | --- | --- |",
        ]
    )
    for item in profile["pickup_requirements"]:
        lines.append("| `{}` | NOT RUN | NOT RUN |".format(item["id"]))
    lines.extend(
        [
            "",
            "## Settlement and controls",
            "",
            "| Settlement ID | Result |",
            "| --- | --- |",
        ]
    )
    for item_id in profile["settlement_ids"]:
        lines.append("| `{}` | NOT RUN |".format(item_id))
    lines.extend(
        [
            "",
            "Published control bindings: **NOT RUN — BLOCKED**.",
            "",
            "## Clean Mac, Gatekeeper, and extended session",
            "",
            "- Clean Apple-Silicon Mac downloaded-artifact test: NOT RUN — BLOCKED",
            "- Quarantine/Gatekeeper first launch: NOT RUN — BLOCKED",
            "- Gatekeeper conclusion: **BLOCKED**",
            "- Candidate-generated 30-minute performance run: NOT RUN — BLOCKED",
            "- Hardware audio review: NOT RUN — BLOCKED",
            "",
            "The performance run must meet the v2 thresholds, remain focused and in",
            "active gameplay for the required ratios, and clear at least one stage.",
            "",
            "## Release gate summary",
            "",
            "| Gate | Status |",
            "| --- | --- |",
        ]
    )
    for row in profile["document_gate_rows"]:
        lines.append("| {} | BLOCKED |".format(row))
    lines.extend(
        [
            "",
            "## Decisions and approval",
            "",
            "- Known-issues review: **NONE — BLOCKED**",
            "- Selected option: **NONE — BLOCKED**",
            "- Inherited 22-sound decision: **NONE — BLOCKED**",
            "- QA lead: NOT SIGNED — BLOCKED",
            "- Release owner: NOT SIGNED — BLOCKED",
            "",
            "After completing and signing the structured evidence, update this report",
            "and the release page together, recompute their hashes in the status JSON,",
            "and run `make verify-alpha-release-ready DIST_CHANNEL={}`.".format(
                candidate["channel"]
            ),
            "",
        ]
    )
    return "\n".join(lines)


def build_status(
    candidate: Mapping[str, Any],
    profile: Mapping[str, Any],
    screenshot_hashes: Mapping[str, str],
    page_hash: str,
    qa_hash: str,
) -> Dict[str, Any]:
    tag = candidate["tag"]
    filenames = candidate["filenames"]
    digests = candidate["digests"]
    candidate_prefix = "build/release/{}/".format(tag)
    release = {
        "version": candidate["version"],
        "channel": candidate["channel"],
        "tag": tag,
        "source_commit": candidate["commit"],
        "candidate_dir": "build/release/{}".format(tag),
    }
    for key in ("artifact", "checksum", "attestation", "gate_log", "build_config"):
        release[key] = {
            "path": candidate_prefix + filenames[key],
            "sha256": digests[key],
        }
    gameplay = [
        matrix_record(item_id, mode)
        for item_id in profile["gameplay_ids"]
        for mode in profile["modes"]
    ]
    bases = [
        matrix_record(item_id, mode)
        for item_id in profile["base_ids"]
        for mode in profile["modes"]
    ]
    pickup_ids = [item["id"] for item in profile["pickup_requirements"]]
    pickups = [
        matrix_record(item_id, mode)
        for item_id in pickup_ids
        for mode in profile["modes"]
    ]
    settlement = [matrix_record(item_id) for item_id in profile["settlement_ids"]]

    def blocked_gate(
        detail_keys: Iterable[str], notes: str, status_value: str = "NOT_RUN"
    ) -> Dict[str, Any]:
        return {
            "status": status_value,
            "tester": "",
            "tested_at_utc": None,
            "evidence_ids": [],
            "checks_confirmed": [],
            "details": {key: "" for key in detail_keys},
            "notes": notes,
        }

    return {
        "schema": STATUS_SCHEMA,
        "requirements": REQUIREMENTS_PATH.as_posix(),
        "release": release,
        "documents": {
            "release_page": {
                "path": "docs/releases/{}.md".format(tag),
                "sha256": page_hash,
            },
            "qa_report": {
                "path": "docs/releases/{}-qa.md".format(tag),
                "sha256": qa_hash,
            },
        },
        "report": {"qa_owner": "", "completed_at_utc": None, "release_date": None},
        "gameplay": gameplay,
        "bases": bases,
        "pickups": pickups,
        "settlement": settlement,
        "published_controls": {
            **matrix_record("published_controls_match"),
            "notes": "Published controls have not been verified against this candidate.",
        },
        "clean_mac": blocked_gate(
            profile["clean_mac_detail_keys"],
            "A source-free clean-Mac download test has not been run.",
        ),
        "gatekeeper": blocked_gate(
            profile["gatekeeper_detail_keys"],
            "Quarantined first-launch behavior has not been tested.",
            "BLOCKED",
        ),
        "extended_session": blocked_gate(
            profile["extended_session_detail_keys"],
            "Candidate-generated thirty-minute telemetry has not been collected.",
        ),
        "evidence": evidence_records(profile, tag, screenshot_hashes),
        "known_issues": {
            "status": "NOT_RUN",
            "conclusion": "NONE",
            "reviewer": "",
            "reviewed_at_utc": None,
            "signature": "",
            "issues": [],
            "notes": "Known-issue review has not been completed.",
        },
        "audio": {
            "decision": "NONE",
            "checks_confirmed": [],
            "rationale": "",
            "evidence": [],
            "owner": "",
            "authority": "",
            "signature": "",
            "decided_at_utc": None,
        },
        "approvals": [
            {
                "role": role,
                "status": "BLOCKED",
                "name": "",
                "signature": "",
                "approved_at_utc": None,
                "evidence_ids": [],
                "notes": "Required manual evidence and decisions are incomplete.",
            }
            for role in profile["approval_roles"]
        ],
    }


def write_staged_file(path: Path, data: bytes) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    flags = os.O_WRONLY | os.O_CREAT | os.O_EXCL
    descriptor = os.open(str(path), flags, 0o644)
    try:
        view = memoryview(data)
        while view:
            written = os.write(descriptor, view)
            if written <= 0:
                raise InitError("short write while staging {}".format(path.name))
            view = view[written:]
        os.fsync(descriptor)
    finally:
        os.close(descriptor)


def canonical_output_paths(root: Path, tag: str) -> Tuple[Path, List[Path]]:
    require_no_symlink_components(
        root, root / "docs" / "assets" / "releases", "release asset directory"
    )
    asset_parent = require_real_directory(
        root / "docs" / "assets" / "releases", "release asset directory"
    )
    require_no_symlink_components(
        root, root / "docs" / "releases", "release document directory"
    )
    document_parent = require_real_directory(
        root / "docs" / "releases", "release document directory"
    )
    asset_dir = asset_parent / tag
    documents = [
        document_parent / "{}.md".format(tag),
        document_parent / "{}-qa.md".format(tag),
        document_parent / "{}-status.json".format(tag),
    ]
    return asset_dir, documents


def ensure_outputs_absent(root: Path, tag: str) -> Tuple[Path, List[Path]]:
    asset_dir, documents = canonical_output_paths(root, tag)
    for path in [asset_dir] + documents:
        if os.path.lexists(str(path)):
            raise InitError("release destination already exists: {}".format(path))
    return asset_dir, documents


def open_repository_directory_no_follow(root: Path, path: Path, label: str) -> int:
    try:
        parts = path.relative_to(root).parts
    except ValueError:
        raise InitError("{} is outside the project root".format(label))
    flags = os.O_RDONLY
    if hasattr(os, "O_DIRECTORY"):
        flags |= os.O_DIRECTORY
    if hasattr(os, "O_NOFOLLOW"):
        flags |= os.O_NOFOLLOW
    try:
        descriptor = os.open(str(root), flags)
        for part in parts:
            next_descriptor = os.open(part, flags, dir_fd=descriptor)
            os.close(descriptor)
            descriptor = next_descriptor
    except OSError as exc:
        if "descriptor" in locals():
            os.close(descriptor)
        raise InitError("cannot open {} safely: {}".format(label, exc))
    if not stat.S_ISDIR(os.fstat(descriptor).st_mode):
        os.close(descriptor)
        raise InitError("{} is not a directory".format(label))
    return descriptor


def read_regular_file_at(directory_fd: int, name: str, label: str) -> bytes:
    flags = os.O_RDONLY | getattr(os, "O_NONBLOCK", 0) | getattr(os, "O_NOFOLLOW", 0)
    try:
        descriptor = os.open(name, flags, dir_fd=directory_fd)
    except OSError as exc:
        raise InitError("cannot open {}: {}".format(label, exc))
    try:
        metadata = os.fstat(descriptor)
        if not stat.S_ISREG(metadata.st_mode):
            raise InitError("{} must be a regular non-symlink file".format(label))
        chunks = []
        while True:
            chunk = os.read(descriptor, 1024 * 1024)
            if not chunk:
                break
            chunks.append(chunk)
        final_metadata = os.fstat(descriptor)
        if (
            metadata.st_dev,
            metadata.st_ino,
            metadata.st_size,
            metadata.st_mtime_ns,
            metadata.st_ctime_ns,
        ) != (
            final_metadata.st_dev,
            final_metadata.st_ino,
            final_metadata.st_size,
            final_metadata.st_mtime_ns,
            final_metadata.st_ctime_ns,
        ):
            raise InitError("{} changed while it was read".format(label))
        data = b"".join(chunks)
        if len(data) != final_metadata.st_size:
            raise InitError("{} changed size while it was read".format(label))
        return data
    finally:
        os.close(descriptor)


def require_same_directory_identity(
    root: Path, path: Path, original_fd: int, label: str
) -> None:
    fresh_fd = open_repository_directory_no_follow(root, path, label)
    try:
        original = os.fstat(original_fd)
        fresh = os.fstat(fresh_fd)
        if (original.st_dev, original.st_ino) != (fresh.st_dev, fresh.st_ino):
            raise InitError("{} changed during publication".format(label))
    finally:
        os.close(fresh_fd)


def write_published_file(
    directory_fd: int,
    name: str,
    data: bytes,
    created_files: List[Tuple[int, str, int, int]],
) -> None:
    flags = os.O_WRONLY | os.O_CREAT | os.O_EXCL
    if hasattr(os, "O_NOFOLLOW"):
        flags |= os.O_NOFOLLOW
    descriptor = None
    created = False
    created_metadata: Optional[os.stat_result] = None
    try:
        descriptor = os.open(name, flags, 0o644, dir_fd=directory_fd)
        created = True
        created_metadata = os.fstat(descriptor)
        created_files.append(
            (
                directory_fd,
                name,
                created_metadata.st_dev,
                created_metadata.st_ino,
            )
        )
        view = memoryview(data)
        while view:
            written = os.write(descriptor, view)
            if written <= 0:
                raise InitError("short write while publishing {}".format(name))
            view = view[written:]
        os.fsync(descriptor)
    except BaseException:
        if descriptor is not None and created_metadata is None:
            try:
                created_metadata = os.fstat(descriptor)
            except OSError:
                pass
        if descriptor is not None:
            os.close(descriptor)
            descriptor = None
        if created and created_metadata is not None:
            try:
                current = os.stat(name, dir_fd=directory_fd, follow_symlinks=False)
                if (
                    current.st_dev == created_metadata.st_dev
                    and current.st_ino == created_metadata.st_ino
                ):
                    os.unlink(name, dir_fd=directory_fd)
            except OSError:
                pass
        raise
    finally:
        if descriptor is not None:
            os.close(descriptor)


def rollback_created_files(
    created_files: Sequence[Tuple[int, str, int, int]]
) -> List[str]:
    """Delete only this invocation's inodes, preserving concurrent directory modes."""
    original_modes: Dict[int, int] = {}
    elevated = set()
    errors = []
    for directory_fd, _, _, _ in created_files:
        if directory_fd not in original_modes:
            try:
                original_modes[directory_fd] = stat.S_IMODE(
                    os.fstat(directory_fd).st_mode
                )
            except OSError as exc:
                errors.append("cannot inspect rollback directory: {}".format(exc))
    for directory_fd, name, device, inode in reversed(created_files):
        try:
            metadata = os.stat(name, dir_fd=directory_fd, follow_symlinks=False)
            if not (
                stat.S_ISREG(metadata.st_mode)
                and metadata.st_dev == device
                and metadata.st_ino == inode
            ):
                errors.append("{} no longer identifies the created file".format(name))
                continue
            try:
                os.unlink(name, dir_fd=directory_fd)
            except PermissionError:
                if directory_fd not in original_modes:
                    raise
                os.fchmod(directory_fd, original_modes[directory_fd] | 0o300)
                elevated.add(directory_fd)
                current = os.stat(name, dir_fd=directory_fd, follow_symlinks=False)
                if not (
                    stat.S_ISREG(current.st_mode)
                    and current.st_dev == device
                    and current.st_ino == inode
                ):
                    raise InitError(
                        "{} changed while rollback permission was restored".format(name)
                    )
                os.unlink(name, dir_fd=directory_fd)
        except FileNotFoundError:
            continue
        except OSError as exc:
            errors.append("cannot remove {}: {}".format(name, exc))
        except InitError as exc:
            errors.append(str(exc))
    for directory_fd in set(original_modes):
        try:
            if directory_fd in elevated:
                os.fchmod(directory_fd, original_modes[directory_fd])
            os.fsync(directory_fd)
        except OSError as exc:
            errors.append("cannot finalize rollback directory: {}".format(exc))
    return errors


def publish_outputs(
    root: Path,
    staging: Path,
    asset_dir: Path,
    document_paths: Sequence[Path],
    tag: str,
    final_check: Optional[Callable[[], None]] = None,
) -> None:
    screenshot_data = {
        name: read_regular_file(staging / "assets" / name, "staged {}".format(name))
        for name in SCREENSHOT_NAMES
    }
    document_names = (
        "{}.md".format(tag),
        "{}-qa.md".format(tag),
        "{}-status.json".format(tag),
    )
    document_data = {
        name: read_regular_file(
            staging / "documents" / name, "staged document {}".format(name)
        )
        for name in document_names
    }
    asset_parent_fd = open_repository_directory_no_follow(
        root, asset_dir.parent, "release asset directory"
    )
    try:
        document_parent_fd = open_repository_directory_no_follow(
            root, document_paths[0].parent, "release document directory"
        )
    except BaseException:
        os.close(asset_parent_fd)
        raise
    asset_fd: Optional[int] = None
    created_asset_dir = False
    created_files: List[Tuple[int, str, int, int]] = []
    caught: Optional[BaseException] = None
    cleanup_errors: List[str] = []
    try:
        os.mkdir(asset_dir.name, 0o700, dir_fd=asset_parent_fd)
        created_asset_dir = True
        asset_fd = os.open(
            asset_dir.name,
            os.O_RDONLY
            | getattr(os, "O_DIRECTORY", 0)
            | getattr(os, "O_NOFOLLOW", 0),
            dir_fd=asset_parent_fd,
        )
        for name in SCREENSHOT_NAMES:
            write_published_file(
                asset_fd, name, screenshot_data[name], created_files
            )
        for name in document_names:
            write_published_file(
                document_parent_fd, name, document_data[name], created_files
            )
        os.fchmod(asset_fd, 0o755)
        os.fsync(asset_fd)
        os.fsync(asset_parent_fd)
        os.fsync(document_parent_fd)

        require_same_directory_identity(
            root, asset_dir.parent, asset_parent_fd, "release asset directory"
        )
        require_same_directory_identity(
            root,
            document_paths[0].parent,
            document_parent_fd,
            "release document directory",
        )
        asset_metadata = os.stat(
            asset_dir.name, dir_fd=asset_parent_fd, follow_symlinks=False
        )
        if (
            asset_metadata.st_dev,
            asset_metadata.st_ino,
        ) != (
            os.fstat(asset_fd).st_dev,
            os.fstat(asset_fd).st_ino,
        ):
            raise InitError("release asset directory changed during publication")
        for name, expected in screenshot_data.items():
            if read_regular_file_at(
                asset_fd, name, "published {}".format(name)
            ) != expected:
                raise InitError("published screenshot changed during publication")
        for name in document_names:
            if read_regular_file_at(
                document_parent_fd, name, "published {}".format(name)
            ) != document_data[name]:
                raise InitError("published release document changed during publication")
        if final_check is not None:
            final_check()
    except BaseException as exc:
        caught = exc
        if asset_fd is not None:
            try:
                os.fchmod(asset_fd, 0o700)
            except OSError:
                pass
        cleanup_errors.extend(rollback_created_files(created_files))
        if asset_fd is not None:
            try:
                os.fsync(asset_fd)
            except OSError:
                pass
        if created_asset_dir:
            try:
                os.rmdir(asset_dir.name, dir_fd=asset_parent_fd)
            except OSError as cleanup_exc:
                cleanup_errors.append(
                    "cannot remove release asset directory: {}".format(cleanup_exc)
                )
    finally:
        if asset_fd is not None:
            os.close(asset_fd)
        os.close(document_parent_fd)
        os.close(asset_parent_fd)
    if caught is not None:
        if cleanup_errors:
            raise InitError(
                "publication failed and rollback is incomplete ({}); original error: {}".format(
                    "; ".join(cleanup_errors), caught
                )
            ) from caught
        if isinstance(caught, (KeyboardInterrupt, SystemExit, InitError)):
            raise caught
        raise InitError(
            "cannot publish release baseline without replacement: {}".format(caught)
        )


def initialize(root_argument: Path, candidate_argument: Path, screenshot_argument: Path) -> str:
    root = require_real_directory(root_argument, "project root")
    if root != root_argument.resolve(strict=True):
        raise InitError("project root must resolve to itself")
    candidate = validate_candidate(root, candidate_argument)
    profile = validate_requirements(root)
    screenshots = validate_screenshots(root, candidate["tag"], screenshot_argument)
    screenshot_hashes = {name: sha256_bytes(data) for name, data in screenshots.items()}

    page = render_release_page(candidate, profile, screenshot_hashes).encode("utf-8")
    qa = render_qa_report(candidate, profile, screenshot_hashes).encode("utf-8")
    status = build_status(
        candidate,
        profile,
        screenshot_hashes,
        sha256_bytes(page),
        sha256_bytes(qa),
    )
    status_data = (json.dumps(status, indent=2, ensure_ascii=False) + "\n").encode("utf-8")
    asset_dir, document_paths = ensure_outputs_absent(root, candidate["tag"])

    def require_candidate_snapshot() -> None:
        if capture_candidate_files(candidate["dir"]) != candidate["snapshot"]:
            raise InitError("candidate changed after tagged verification")

    build_dir = require_real_directory(root / "build", "build directory")
    require_no_symlink_components(root, build_dir, "build directory")
    with tempfile.TemporaryDirectory(prefix=".alpha-v2-status-", dir=str(build_dir)) as temporary:
        staging = Path(temporary)
        for name, data in screenshots.items():
            write_staged_file(staging / "assets" / name, data)
        write_staged_file(staging / "documents" / "{}.md".format(candidate["tag"]), page)
        write_staged_file(
            staging / "documents" / "{}-qa.md".format(candidate["tag"]), qa
        )
        write_staged_file(
            staging / "documents" / "{}-status.json".format(candidate["tag"]),
            status_data,
        )
        require_candidate_snapshot()
        publish_outputs(
            root,
            staging,
            asset_dir,
            document_paths,
            candidate["tag"],
            final_check=require_candidate_snapshot,
        )

    return candidate["tag"]


def parse_arguments(argv: Optional[Sequence[str]] = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--project-root", required=True, type=Path)
    parser.add_argument("--candidate-dir", required=True, type=Path)
    parser.add_argument("--screenshot-input-dir", required=True, type=Path)
    return parser.parse_args(argv)


def main(argv: Optional[Sequence[str]] = None) -> int:
    arguments = parse_arguments(argv)
    try:
        tag = initialize(
            arguments.project_root,
            arguments.candidate_dir,
            arguments.screenshot_input_dir,
        )
    except InitError as exc:
        print("Alpha v2 baseline initialization failed: {}".format(exc), file=sys.stderr)
        return 1
    print("Created honest BLOCKED Alpha v2 baseline for {}.".format(tag))
    print("Commit and review the generated files before running the evidence verifier.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
