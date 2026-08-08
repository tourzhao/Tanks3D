#!/usr/bin/env python3
"""Verify the immutable and human evidence for a Tanks 3D macOS Alpha.

The requirements document is deliberately checked against a compiled-in
profile.  Editing both JSON files therefore cannot silently remove a release
gate.  ``--allow-blocked`` is for honest work-in-progress reports: it relaxes
only the final all-PASS decision, never schema, identity, hash, or evidence
validation.
"""

import argparse
import datetime as _datetime
import hashlib
import json
import math
from pathlib import Path
import re
import shlex
import struct
import subprocess
import sys
from typing import Any, Dict, List, Mapping, Optional, Sequence, Set, Tuple
from urllib.parse import unquote, urlsplit
import zipfile
import zlib


class VerificationError(Exception):
    """A status or evidence invariant was violated."""


STATUS_VALUES = ["PASS", "FAIL", "BLOCKED", "NOT_RUN"]
MODES = ["one_player", "two_player"]
GAMEPLAY_IDS = [
    "start_and_control",
    "movement_and_collision",
    "fire_hits_and_shell_cancellation",
    "pause_and_resume",
    "escape_returns_to_setup",
    "hp_death_and_respawn",
    "streak_increment_and_reset",
    "base_breach_and_core_loss",
    "classified_ko_rows_and_totals",
    "grenade_excluded_from_direct_ko",
    "settlement_transition",
    "fullscreen_toggle",
    "resize_hud_camera_and_minimap",
    "audio_cues_and_volume",
    "complete_stage_stability",
]
BASE_IDS = ["usa", "ussr", "germany"]
BASE_CHECKS = [
    "wall_damage",
    "wall_breach",
    "shovel_steel_material",
    "shovel_steel_protection",
    "tank_collision",
    "core_loss",
    "visibility",
    "emblem_orientation",
]
POSITIVE_PICKUP_CHECKS = [
    "model_3d_visible",
    "classic_icon_blinks",
    "minimap_star_blinks",
    "collection_audio_plays",
    "score_delta_300",
    "gameplay_effect_matches",
]
ABSENT_PICKUP_CHECKS = [
    "pickup_absent",
    "no_pickup_visual_or_marker",
    "no_collection_audio",
    "no_score_delta",
    "spawn_restriction_matches",
]
LIFETIME_PICKUP_CHECKS = [
    "model_3d_visible",
    "classic_icon_blinks",
    "minimap_star_blinks",
    "no_collection_audio_before_collection",
    "no_score_delta_before_collection",
    "lifetime_12_5_seconds",
]
PICKUP_REQUIREMENTS = [
    {"id": pickup_id, "checks": list(POSITIVE_PICKUP_CHECKS)}
    for pickup_id in (
        "grenade",
        "helmet",
        "clock",
        "shovel",
        "tank",
        "star",
        "gun",
        "boat",
        "bandage_heal",
    )
] + [
    {"id": "bandage_absent_at_full_hp", "checks": list(ABSENT_PICKUP_CHECKS)},
    {
        "id": "bandage_disabled_at_max_hp_1",
        "checks": list(ABSENT_PICKUP_CHECKS),
    },
    {"id": "pickup_lifetime_12_5_seconds", "checks": list(LIFETIME_PICKUP_CHECKS)},
]
SETTLEMENT_IDS = [
    "basic_tank_ko",
    "fast_tank_ko",
    "power_tank_ko",
    "armor_tank_ko",
    "ko_total",
    "score_points",
    "grenade_exclusion",
]
REPORT_METADATA_KEYS = ["qa_owner", "completed_at_utc", "release_date"]
PUBLISHED_CONTROL_CHECKS = [
    "menu_arrow_or_wasd_navigation",
    "menu_enter_or_space_confirm",
    "player_1_arrow_movement",
    "player_1_fire_bindings",
    "player_2_wasd_movement",
    "player_2_fire_bindings",
    "enter_pause_resume",
    "escape_returns_to_setup",
    "r_restarts_stage",
    "f8_quality_toggle",
    "f11_borderless_toggle",
    "n_b_stage_navigation",
    "q_or_escape_exits_from_setup",
]
INTERACTIVE_EVIDENCE_KEYS = [
    "candidate_sha256",
    "tester",
    "machine",
    "tested_at_utc",
    "signature",
    "coverage_refs",
]
KNOWN_ISSUE_CONCLUSIONS = ["NONE", "NONE_KNOWN", "RECORDED"]
KNOWN_ISSUE_SEVERITIES = ["LOW", "MEDIUM", "HIGH", "BLOCKER"]
KNOWN_ISSUE_DECISIONS = ["ACCEPT_FOR_ALPHA", "FIX_BEFORE_RELEASE"]
KNOWN_ISSUE_FIELDS = [
    "id",
    "severity",
    "summary",
    "reproduction",
    "impact",
    "workaround",
    "release_decision",
    "owner",
    "evidence_ids",
]
AUDIO_BASE_CHECKS = [
    "asset_licenses_reviewed",
    "third_party_notices_reviewed",
    "archive_license_bundle_reviewed",
    "all_22_candidate_ogg_reviewed",
    "chain_of_title_limitation_understood",
]
AUDIO_DECISION_CHECKS = [
    {"decision": "ACCEPT", "checks": list(AUDIO_BASE_CHECKS)},
    {
        "decision": "CONFIRM",
        "checks": list(AUDIO_BASE_CHECKS) + ["rights_holder_confirmation_reviewed"],
    },
    {
        "decision": "REPLACE",
        "checks": [
            "replacement_sources_reviewed",
            "replacement_licenses_reviewed",
            "replacement_candidate_audio_manifest_verified",
        ],
    },
]
APPROVABLE_AUDIO_DECISIONS = ["ACCEPT"]
CLEAN_MAC_DETAIL_KEYS = [
    "mac_model",
    "chip",
    "uname_machine",
    "ram",
    "macos_version",
    "macos_build",
    "clean_machine_method",
    "download_url",
    "download_client",
    "downloaded_artifact_filename",
    "downloaded_artifact_sha256",
    "checksum_command",
    "checksum_exit_code",
    "prior_app_absent",
    "prior_approval_absent",
    "minimum_macos_met",
    "source_checkout_absent",
    "homebrew_raylib_unused",
]
GATEKEEPER_DETAIL_KEYS = [
    "zip_quarantine_command",
    "zip_quarantine_exit_code",
    "zip_quarantine_output",
    "app_quarantine_command",
    "app_quarantine_exit_code",
    "app_quarantine_output",
    "codesign_command",
    "codesign_exit_code",
    "spctl_command",
    "spctl_exit_code",
    "first_finder_launch",
    "dialog_text",
    "documented_launch_path",
    "main_menu_reached",
    "signature_preserved",
    "conclusion",
    "release_note_wording_verified",
]
EXTENDED_SESSION_DETAIL_KEYS = [
    "duration_minutes",
    "stages_completed",
    "mode_mix",
    "measurement_tools",
    "sampling_interval_seconds",
    "fps_acceptance_criterion",
    "average_fps",
    "minimum_fps",
    "minimum_average_fps",
    "minimum_one_percent_low_fps",
    "one_percent_low_fps",
    "thermal_state",
    "throttling",
    "fan_observation",
    "memory_start_mb",
    "memory_end_mb",
    "memory_growth_observation",
    "maximum_memory_growth_mb",
    "rendering_artifacts",
    "audio_issues",
    "crashes_hangs_or_softlocks",
    "crash_count",
    "hang_count",
    "softlock_count",
    "logs_and_capture_locations",
]
EVIDENCE_IDS = [
    "main_menu_and_advanced_settings",
    "one_player_gameplay",
    "two_player_gameplay",
    "national_bases",
    "pickup_and_minimap",
    "settlement_report",
    "gatekeeper_launch",
    "extended_session_metrics",
]
AUDIO_DECISIONS = ["NONE", "ACCEPT", "CONFIRM", "REPLACE"]
APPROVAL_ROLES = ["qa_lead", "release_owner"]
PERFORMANCE_THRESHOLDS = {
    "minimum_duration_minutes": 30,
    "minimum_stages_completed": 1,
    "minimum_average_fps": 50,
    "minimum_one_percent_low_fps": 30,
    "minimum_sampling_interval_seconds": 0.25,
    "maximum_sampling_interval_seconds": 5,
    "maximum_memory_growth_mb": 256,
    "minimum_sampling_coverage_ratio": 0.9,
}
INTERACTIVE_SESSION_SCHEMA = "tanks3d-interactive-session-v1"
INTERACTIVE_SESSION_KEYS = [
    "schema",
    "candidate_sha256",
    "tester",
    "machine",
    "started_at_utc",
    "completed_at_utc",
    "signature",
    "categories",
]
INTERACTIVE_CATEGORY_KEYS = [
    "id",
    "tested_at_utc",
    "reviewed_at_utc",
    "result",
    "coverage_refs",
    "artifact_sha256s",
]
GAMEPLAY_EVENT_LOG_SCHEMA = "tanks3d-gameplay-event-log-v1"
GAMEPLAY_EVENT_LOG_KEYS = [
    "schema",
    "producer",
    "candidate_sha256",
    "tester",
    "machine",
    "started_at_utc",
    "completed_at_utc",
    "events",
]
GAMEPLAY_EVENT_KEYS = [
    "sequence",
    "timestamp_utc",
    "category_id",
    "coverage_token",
    "result",
]
COMMAND_LOG_SCHEMA = "tanks3d-command-log-v1"
COMMAND_LOG_KEYS = ["schema", "candidate_sha256", "machine", "commands"]
COMMAND_RESULT_KEYS = [
    "id",
    "argv",
    "exit_code",
    "stdout",
    "stderr",
    "started_at_utc",
    "completed_at_utc",
]
PERFORMANCE_LOG_SCHEMA = "tanks3d-performance-log-v1"
PERFORMANCE_LOG_KEYS = [
    "schema",
    "candidate_sha256",
    "started_at_utc",
    "completed_at_utc",
    "samples",
]
PERFORMANCE_SAMPLE_KEYS = ["elapsed_seconds", "fps", "memory_mb"]
DOCUMENT_GATE_ROWS = [
    "Gameplay matrix",
    "National bases",
    "Pickups",
    "Settlement",
    "Published controls",
    "Clean Mac",
    "Gatekeeper",
    "Extended session",
    "Evidence manifest",
    "Known issues",
    "Audio decision",
    "QA approval",
    "Release-owner approval",
]

CANONICAL_REQUIREMENTS: Dict[str, Any] = {
    "schema": "tanks3d-release-requirements-v1",
    "profile": "macos-alpha-v1",
    "status_values": STATUS_VALUES,
    "modes": MODES,
    "gameplay_ids": GAMEPLAY_IDS,
    "base_ids": BASE_IDS,
    "base_checks": BASE_CHECKS,
    "pickup_requirements": PICKUP_REQUIREMENTS,
    "settlement_ids": SETTLEMENT_IDS,
    "report_metadata_keys": REPORT_METADATA_KEYS,
    "published_control_checks": PUBLISHED_CONTROL_CHECKS,
    "interactive_evidence_keys": INTERACTIVE_EVIDENCE_KEYS,
    "known_issue_conclusions": KNOWN_ISSUE_CONCLUSIONS,
    "known_issue_severities": KNOWN_ISSUE_SEVERITIES,
    "known_issue_decisions": KNOWN_ISSUE_DECISIONS,
    "known_issue_fields": KNOWN_ISSUE_FIELDS,
    "clean_mac_detail_keys": CLEAN_MAC_DETAIL_KEYS,
    "gatekeeper_detail_keys": GATEKEEPER_DETAIL_KEYS,
    "extended_session_detail_keys": EXTENDED_SESSION_DETAIL_KEYS,
    "evidence_ids": EVIDENCE_IDS,
    "audio_decisions": AUDIO_DECISIONS,
    "audio_decision_checks": AUDIO_DECISION_CHECKS,
    "approvable_audio_decisions": APPROVABLE_AUDIO_DECISIONS,
    "approval_roles": APPROVAL_ROLES,
    "performance_thresholds": PERFORMANCE_THRESHOLDS,
    "interactive_session_schema": INTERACTIVE_SESSION_SCHEMA,
    "interactive_session_keys": INTERACTIVE_SESSION_KEYS,
    "interactive_category_keys": INTERACTIVE_CATEGORY_KEYS,
    "gameplay_event_log_schema": GAMEPLAY_EVENT_LOG_SCHEMA,
    "gameplay_event_log_keys": GAMEPLAY_EVENT_LOG_KEYS,
    "gameplay_event_keys": GAMEPLAY_EVENT_KEYS,
    "command_log_schema": COMMAND_LOG_SCHEMA,
    "command_log_keys": COMMAND_LOG_KEYS,
    "command_result_keys": COMMAND_RESULT_KEYS,
    "performance_log_schema": PERFORMANCE_LOG_SCHEMA,
    "performance_log_keys": PERFORMANCE_LOG_KEYS,
    "performance_sample_keys": PERFORMANCE_SAMPLE_KEYS,
    "document_gate_rows": DOCUMENT_GATE_ROWS,
}

ROOT_KEYS = {
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
REPORT_KEYS = set(REPORT_METADATA_KEYS)
FILE_REF_KEYS = {"path", "sha256"}
DOCUMENT_KEYS = {"release_page", "qa_report"}
RESULT_KEYS = {
    "id",
    "mode",
    "status",
    "tester",
    "tested_at_utc",
    "evidence_ids",
    "checks_confirmed",
    "notes",
}
SETTLEMENT_RESULT_KEYS = RESULT_KEYS - {"mode"}
GATE_KEYS = {
    "status",
    "tester",
    "tested_at_utc",
    "evidence_ids",
    "checks_confirmed",
    "details",
    "notes",
}
EVIDENCE_KEYS = {
    "id",
    "status",
    "artifacts",
    "reviewer",
    "reviewed_at_utc",
    "interactive",
    "notes",
}
INTERACTIVE_EVIDENCE_KEY_SET = set(INTERACTIVE_EVIDENCE_KEYS)
EVIDENCE_ARTIFACT_KEYS = {"path", "sha256", "kind"}
AUDIO_KEYS = {
    "decision",
    "rationale",
    "evidence",
    "checks_confirmed",
    "owner",
    "authority",
    "signature",
    "decided_at_utc",
}
KNOWN_ISSUES_KEYS = {
    "status",
    "conclusion",
    "reviewer",
    "reviewed_at_utc",
    "signature",
    "issues",
    "notes",
}
KNOWN_ISSUE_KEY_SET = set(KNOWN_ISSUE_FIELDS)
APPROVAL_KEYS = {
    "role",
    "status",
    "name",
    "signature",
    "approved_at_utc",
    "evidence_ids",
    "notes",
}
SHA256_RE = re.compile(r"^[0-9a-f]{64}$")
COMMIT_RE = re.compile(r"^(?:[0-9a-f]{40}|[0-9a-f]{64})$")
TOKEN_RE = re.compile(r"^[A-Za-z0-9][A-Za-z0-9._-]*$")
VERSION_RE = re.compile(r"^(?:0|[1-9][0-9]*)\.(?:0|[1-9][0-9]*)\.(?:0|[1-9][0-9]*)$")
ALPHA_CHANNEL_RE = re.compile(r"^alpha\.(?:0|[1-9][0-9]*)$")
ISSUE_ID_RE = re.compile(r"^[A-Z][A-Z0-9]*(?:-[A-Z0-9]+)+$")
UTC_RE = re.compile(r"^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}Z$")
DATE_RE = re.compile(r"^\d{4}-\d{2}-\d{2}$")
QUARANTINE_RE = re.compile(r"^[0-9A-Fa-f]{4};[^;\r\n]+;[^;\r\n]+(?:;[^\r\n]*)?$")
PNG_KINDS = {"png"}
ARTIFACT_KINDS = {"png", "log", "recording", "report"}


def _reject_duplicate_pairs(pairs: Sequence[Tuple[str, Any]]) -> Dict[str, Any]:
    result: Dict[str, Any] = {}
    for key, value in pairs:
        if key in result:
            raise VerificationError("duplicate JSON key: {!r}".format(key))
        result[key] = value
    return result


def load_json_strict(path: Path) -> Any:
    try:
        text = path.read_text(encoding="utf-8")
    except (OSError, UnicodeError) as exc:
        raise VerificationError("cannot read JSON file {}: {}".format(path, exc))
    try:
        return json.loads(text, object_pairs_hook=_reject_duplicate_pairs)
    except VerificationError:
        raise
    except (json.JSONDecodeError, UnicodeError) as exc:
        raise VerificationError("invalid JSON in {}: {}".format(path, exc))


def require_object(value: Any, context: str) -> Mapping[str, Any]:
    if not isinstance(value, dict):
        raise VerificationError("{} must be an object".format(context))
    return value


def require_array(value: Any, context: str) -> List[Any]:
    if not isinstance(value, list):
        raise VerificationError("{} must be an array".format(context))
    return value


def require_string(value: Any, context: str) -> str:
    if not isinstance(value, str):
        raise VerificationError("{} must be a string".format(context))
    return value


def require_exact_keys(value: Mapping[str, Any], expected: Set[str], context: str) -> None:
    actual = set(value.keys())
    if actual != expected:
        missing = sorted(expected - actual)
        extra = sorted(actual - expected)
        details = []
        if missing:
            details.append("missing {}".format(", ".join(missing)))
        if extra:
            details.append("unexpected {}".format(", ".join(extra)))
        raise VerificationError("{} has invalid keys ({})".format(context, "; ".join(details)))


def compare_canonical(actual: Any, expected: Any, context: str) -> None:
    """Recursively require the exact compiled-in requirements profile."""
    if isinstance(expected, dict):
        obj = require_object(actual, context)
        require_exact_keys(obj, set(expected.keys()), context)
        for key in expected:
            compare_canonical(obj[key], expected[key], "{}.{}".format(context, key))
        return
    if isinstance(expected, list):
        values = require_array(actual, context)
        if len(values) != len(expected):
            raise VerificationError(
                "{} must contain exactly {} entries".format(context, len(expected))
            )
        for index, (actual_item, expected_item) in enumerate(zip(values, expected)):
            compare_canonical(
                actual_item, expected_item, "{}[{}]".format(context, index)
            )
        return
    if actual != expected or type(actual) is not type(expected):
        raise VerificationError("{} does not match the canonical profile".format(context))


def validate_requirements(requirements: Any) -> None:
    compare_canonical(requirements, CANONICAL_REQUIREMENTS, "requirements")


def require_sha256(value: Any, context: str) -> str:
    digest = require_string(value, context)
    if SHA256_RE.fullmatch(digest) is None:
        raise VerificationError("{} is not a lowercase SHA-256 digest".format(context))
    return digest


def require_timestamp(value: Any, context: str) -> str:
    timestamp = require_string(value, context)
    if UTC_RE.fullmatch(timestamp) is None:
        raise VerificationError("{} must use YYYY-MM-DDTHH:MM:SSZ".format(context))
    try:
        parsed = _datetime.datetime.strptime(timestamp, "%Y-%m-%dT%H:%M:%SZ").replace(
            tzinfo=_datetime.timezone.utc
        )
    except ValueError as exc:
        raise VerificationError("{} is not a real UTC timestamp: {}".format(context, exc))
    if parsed > _datetime.datetime.now(_datetime.timezone.utc) + _datetime.timedelta(
        minutes=5
    ):
        raise VerificationError("{} must not be in the future".format(context))
    return timestamp


def require_nullable_timestamp(value: Any, context: str) -> Optional[str]:
    if value is None:
        return None
    return require_timestamp(value, context)


def require_nonplaceholder(value: Any, context: str) -> str:
    text = require_string(value, context).strip()
    upper = text.upper()
    if not text or upper in {"NONE", "N/A", "NA", "TBD"} or upper.startswith("NOT "):
        raise VerificationError("{} is missing or a placeholder".format(context))
    return text


def reject_blocking_language(value: Any, context: str) -> None:
    text = require_string(value, context).strip().lower()
    forbidden = (
        "blocked",
        "not run",
        "not recorded",
        "not checked",
        "not interactive",
        "not a live",
        "showcase only",
        "rendering evidence only",
    )
    for phrase in forbidden:
        if phrase in text:
            raise VerificationError(
                "{} contradicts PASS with {!r}".format(context, phrase)
            )


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    try:
        with path.open("rb") as stream:
            for chunk in iter(lambda: stream.read(1024 * 1024), b""):
                digest.update(chunk)
    except OSError as exc:
        raise VerificationError("cannot hash {}: {}".format(path, exc))
    return digest.hexdigest()


def resolve_repository_file(root: Path, relative: Any, context: str) -> Path:
    location = require_string(relative, context)
    if not location or "\\" in location:
        raise VerificationError("{} must be a non-empty POSIX repository path".format(context))
    relative_path = Path(location)
    if relative_path.is_absolute() or any(part in {"", ".", ".."} for part in relative_path.parts):
        raise VerificationError("{} must be a normalized repository-relative path".format(context))
    if relative_path.as_posix() != location:
        raise VerificationError("{} must be a normalized repository-relative path".format(context))
    candidate = root / relative_path
    try:
        resolved = candidate.resolve(strict=True)
        resolved.relative_to(root)
    except (OSError, ValueError) as exc:
        raise VerificationError("{} is missing or escapes the project root: {}".format(context, exc))
    current = root
    for part in relative_path.parts:
        current = current / part
        if current.is_symlink():
            raise VerificationError("{} must not traverse a symbolic link".format(context))
    if not resolved.is_file():
        raise VerificationError("{} must identify a regular file".format(context))
    return resolved


def resolve_repository_directory(root: Path, relative: Any, context: str) -> Path:
    location = require_string(relative, context)
    if not location or "\\" in location:
        raise VerificationError("{} must be a non-empty POSIX repository path".format(context))
    relative_path = Path(location)
    if relative_path.is_absolute() or any(part in {"", ".", ".."} for part in relative_path.parts):
        raise VerificationError("{} must be a normalized repository-relative path".format(context))
    if relative_path.as_posix() != location:
        raise VerificationError("{} must be a normalized repository-relative path".format(context))
    candidate = root / relative_path
    try:
        resolved = candidate.resolve(strict=True)
        resolved.relative_to(root)
    except (OSError, ValueError) as exc:
        raise VerificationError("{} is missing or escapes the project root: {}".format(context, exc))
    current = root
    for part in relative_path.parts:
        current = current / part
        if current.is_symlink():
            raise VerificationError("{} must not traverse a symbolic link".format(context))
    if not resolved.is_dir():
        raise VerificationError("{} must identify a directory".format(context))
    return resolved


def verify_file_reference(root: Path, value: Any, context: str) -> Tuple[Path, str]:
    reference = require_object(value, context)
    require_exact_keys(reference, FILE_REF_KEYS, context)
    path = resolve_repository_file(root, reference["path"], "{}.path".format(context))
    expected = require_sha256(reference["sha256"], "{}.sha256".format(context))
    actual = sha256_file(path)
    if actual != expected:
        raise VerificationError(
            "{} hash mismatch: expected {}, got {}".format(context, expected, actual)
        )
    return path, expected


def verify_png(path: Path, context: str, require_release_size: bool = False) -> None:
    try:
        data = path.read_bytes()
    except OSError as exc:
        raise VerificationError("cannot read {}: {}".format(context, exc))
    if not data.startswith(b"\x89PNG\r\n\x1a\n"):
        raise VerificationError("{} is not a PNG".format(context))
    offset = 8
    chunks: List[bytes] = []
    width = height = None
    saw_idat = False
    while offset < len(data):
        if len(data) - offset < 12:
            raise VerificationError("{} has a truncated PNG chunk".format(context))
        length = struct.unpack(">I", data[offset : offset + 4])[0]
        chunk_type = data[offset + 4 : offset + 8]
        end = offset + 12 + length
        if end > len(data):
            raise VerificationError("{} has a truncated PNG payload".format(context))
        payload = data[offset + 8 : offset + 8 + length]
        recorded_crc = struct.unpack(">I", data[offset + 8 + length : end])[0]
        actual_crc = zlib.crc32(chunk_type)
        actual_crc = zlib.crc32(payload, actual_crc) & 0xFFFFFFFF
        if recorded_crc != actual_crc:
            raise VerificationError("{} has an invalid PNG chunk CRC".format(context))
        chunks.append(chunk_type)
        if len(chunks) == 1:
            if chunk_type != b"IHDR" or length != 13:
                raise VerificationError("{} has no canonical IHDR".format(context))
            width, height = struct.unpack(">II", payload[:8])
        if chunk_type == b"IDAT":
            saw_idat = True
        if chunk_type == b"IEND":
            if length != 0 or end != len(data):
                raise VerificationError("{} has an invalid IEND".format(context))
            offset = end
            break
        offset = end
    if not chunks or chunks[-1] != b"IEND" or not saw_idat:
        raise VerificationError("{} is an incomplete PNG".format(context))
    if require_release_size and (width, height) != (1280, 720):
        raise VerificationError(
            "{} must be 1280x720, got {}x{}".format(context, width, height)
        )


def validate_artifact(root: Path, value: Any, context: str) -> Tuple[str, str, str, Path]:
    artifact = require_object(value, context)
    require_exact_keys(artifact, EVIDENCE_ARTIFACT_KEYS, context)
    kind = require_string(artifact["kind"], "{}.kind".format(context))
    if kind not in ARTIFACT_KINDS:
        raise VerificationError("{}.kind is unsupported".format(context))
    path = resolve_repository_file(root, artifact["path"], "{}.path".format(context))
    digest = require_sha256(artifact["sha256"], "{}.sha256".format(context))
    actual = sha256_file(path)
    if actual != digest:
        raise VerificationError("{} hash mismatch".format(context))
    if kind in PNG_KINDS:
        verify_png(path, context)
    return (
        require_string(artifact["path"], "{}.path".format(context)),
        digest,
        kind,
        path,
    )


def load_structured_artifact(path: Path, context: str) -> Optional[Mapping[str, Any]]:
    """Return a JSON object for structured evidence, or None for binary/text evidence."""
    try:
        if path.stat().st_size > 32 * 1024 * 1024:
            raise VerificationError("{} is unexpectedly large".format(context))
        raw = path.read_bytes()
    except OSError as exc:
        raise VerificationError("cannot read {}: {}".format(context, exc))
    stripped = raw.lstrip()
    if not stripped.startswith(b"{"):
        return None
    try:
        value = json.loads(raw.decode("utf-8"), object_pairs_hook=_reject_duplicate_pairs)
    except (UnicodeError, json.JSONDecodeError) as exc:
        raise VerificationError("{} is malformed structured JSON: {}".format(context, exc))
    return require_object(value, context)


def require_json_number(value: Any, context: str) -> float:
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        raise VerificationError("{} must be a JSON number".format(context))
    number = float(value)
    if not math.isfinite(number):
        raise VerificationError("{} must be finite".format(context))
    return number


def validate_status_value(value: Any, context: str) -> str:
    status = require_string(value, context)
    if status not in STATUS_VALUES:
        raise VerificationError("{} has unknown status {!r}".format(context, status))
    return status


def validate_string_array(value: Any, context: str) -> List[str]:
    values = require_array(value, context)
    result = []
    seen: Set[str] = set()
    for index, item in enumerate(values):
        text = require_string(item, "{}[{}]".format(context, index))
        if text in seen:
            raise VerificationError("{} contains duplicate {!r}".format(context, text))
        seen.add(text)
        result.append(text)
    return result


def validate_interactive_evidence(value: Any, context: str) -> None:
    interactive = require_object(value, context)
    require_exact_keys(interactive, INTERACTIVE_EVIDENCE_KEY_SET, context)
    candidate_sha256 = require_string(
        interactive["candidate_sha256"], "{}.candidate_sha256".format(context)
    )
    tester = require_string(interactive["tester"], "{}.tester".format(context))
    machine = require_string(interactive["machine"], "{}.machine".format(context))
    tested_at = require_nullable_timestamp(
        interactive["tested_at_utc"], "{}.tested_at_utc".format(context)
    )
    signature = require_string(
        interactive["signature"], "{}.signature".format(context)
    )
    coverage = require_object(
        interactive["coverage_refs"], "{}.coverage_refs".format(context)
    )
    for token, reference in coverage.items():
        require_nonplaceholder(token, "{}.coverage_refs key".format(context))
        require_nonplaceholder(
            reference, "{}.coverage_refs.{!r}".format(context, token)
        )
    populated = any((candidate_sha256, tester, machine, signature, coverage)) or (
        tested_at is not None
    )
    if not populated:
        return
    require_sha256(candidate_sha256, "{}.candidate_sha256".format(context))
    require_nonplaceholder(tester, "{}.tester".format(context))
    require_nonplaceholder(machine, "{}.machine".format(context))
    require_nonplaceholder(signature, "{}.signature".format(context))
    if tested_at is None:
        raise VerificationError("{} requires tested_at_utc when populated".format(context))
    if not coverage:
        raise VerificationError("{} requires coverage_refs when populated".format(context))


def validate_evidence(
    root: Path, values: Any, blockers: List[str]
) -> Tuple[
    Dict[str, Mapping[str, Any]],
    Dict[str, Tuple[str, str, str, Path]],
]:
    records = require_array(values, "status.evidence")
    if len(records) != len(EVIDENCE_IDS):
        raise VerificationError("status.evidence must contain exactly eight categories")
    evidence_map: Dict[str, Mapping[str, Any]] = {}
    artifact_map: Dict[str, Tuple[str, str, str, Path]] = {}
    resolved_artifacts: Dict[Path, Tuple[str, str]] = {}
    actual_ids: List[str] = []
    for index, raw_record in enumerate(records):
        context = "status.evidence[{}]".format(index)
        record = require_object(raw_record, context)
        require_exact_keys(record, EVIDENCE_KEYS, context)
        evidence_id = require_string(record["id"], "{}.id".format(context))
        actual_ids.append(evidence_id)
        status = validate_status_value(record["status"], "{}.status".format(context))
        artifacts = require_array(record["artifacts"], "{}.artifacts".format(context))
        reviewer = require_string(record["reviewer"], "{}.reviewer".format(context))
        reviewed_at = require_nullable_timestamp(
            record["reviewed_at_utc"], "{}.reviewed_at_utc".format(context)
        )
        validate_interactive_evidence(
            record["interactive"], "{}.interactive".format(context)
        )
        require_string(record["notes"], "{}.notes".format(context))
        if status == "PASS":
            require_nonplaceholder(reviewer, "{}.reviewer".format(context))
            if reviewed_at is None:
                raise VerificationError("{} PASS requires reviewed_at_utc".format(context))
            if not artifacts:
                raise VerificationError("{} PASS requires at least one artifact".format(context))
            required_kinds = {
                "main_menu_and_advanced_settings": {"png", "recording"},
                "gatekeeper_launch": {"png", "recording"},
                "extended_session_metrics": {"log", "report"},
            }.get(evidence_id)
            if required_kinds is not None and not any(
                artifact["kind"] in required_kinds for artifact in artifacts
            ):
                raise VerificationError(
                    "{} PASS requires one of {}".format(
                        context, ", ".join(sorted(required_kinds))
                    )
                )
        for artifact_index, artifact in enumerate(artifacts):
            artifact_context = "{}.artifacts[{}]".format(context, artifact_index)
            normalized = validate_artifact(root, artifact, artifact_context)
            path = normalized[0]
            resolved_path = normalized[3]
            previous = resolved_artifacts.get(resolved_path)
            metadata = (normalized[1], normalized[2])
            if previous is not None and previous != metadata:
                raise VerificationError(
                    "shared evidence artifact has inconsistent metadata: {}".format(path)
                )
            resolved_artifacts[resolved_path] = metadata
            artifact_map[path] = normalized
        if evidence_id in evidence_map:
            raise VerificationError("duplicate evidence id {!r}".format(evidence_id))
        evidence_map[evidence_id] = record
        if status != "PASS":
            blockers.append("evidence.{}={}".format(evidence_id, status))
    if actual_ids != EVIDENCE_IDS:
        raise VerificationError("status.evidence IDs/order do not match the fixed profile")
    return evidence_map, artifact_map


def validate_result_record(
    raw_record: Any,
    context: str,
    expected_id: str,
    expected_mode: Optional[str],
    expected_checks: Sequence[str],
    expected_evidence_ids: Sequence[str],
    evidence_map: Mapping[str, Mapping[str, Any]],
    blockers: List[str],
) -> None:
    record = require_object(raw_record, context)
    expected_keys = RESULT_KEYS if expected_mode is not None else SETTLEMENT_RESULT_KEYS
    require_exact_keys(record, expected_keys, context)
    if require_string(record["id"], "{}.id".format(context)) != expected_id:
        raise VerificationError("{}.id does not match the fixed profile".format(context))
    if expected_mode is not None:
        if require_string(record["mode"], "{}.mode".format(context)) != expected_mode:
            raise VerificationError("{}.mode does not match the fixed profile".format(context))
    status = validate_status_value(record["status"], "{}.status".format(context))
    tester = require_string(record["tester"], "{}.tester".format(context))
    tested_at = require_nullable_timestamp(
        record["tested_at_utc"], "{}.tested_at_utc".format(context)
    )
    evidence_ids = validate_string_array(
        record["evidence_ids"], "{}.evidence_ids".format(context)
    )
    checks = validate_string_array(
        record["checks_confirmed"], "{}.checks_confirmed".format(context)
    )
    notes = require_string(record["notes"], "{}.notes".format(context))
    for evidence_id in evidence_ids:
        if evidence_id not in evidence_map:
            raise VerificationError("{} references unknown evidence {!r}".format(context, evidence_id))
    if status == "PASS":
        require_nonplaceholder(tester, "{}.tester".format(context))
        if tested_at is None:
            raise VerificationError("{} PASS requires tested_at_utc".format(context))
        if not evidence_ids:
            raise VerificationError("{} PASS requires evidence_ids".format(context))
        for evidence_id in evidence_ids:
            if evidence_map[evidence_id]["status"] != "PASS":
                raise VerificationError(
                    "{} PASS references non-PASS evidence {!r}".format(context, evidence_id)
                )
        if evidence_ids != list(expected_evidence_ids):
            raise VerificationError("{} PASS evidence_ids are not exact".format(context))
        if checks != list(expected_checks):
            raise VerificationError("{} PASS checks_confirmed are not exact".format(context))
        reject_blocking_language(notes, "{}.notes".format(context))
    elif checks:
        raise VerificationError("{} non-PASS checks_confirmed must be empty".format(context))
    if status != "PASS":
        label = expected_id if expected_mode is None else "{}.{}".format(expected_id, expected_mode)
        blockers.append("{}={}".format(label, status))


def validate_matrix(
    values: Any,
    context: str,
    definitions: Sequence[Tuple[str, Optional[str], Sequence[str], Sequence[str]]],
    evidence_map: Mapping[str, Mapping[str, Any]],
    blockers: List[str],
) -> None:
    records = require_array(values, context)
    if len(records) != len(definitions):
        raise VerificationError("{} must contain exactly {} rows".format(context, len(definitions)))
    for index, (record, definition) in enumerate(zip(records, definitions)):
        expected_id, expected_mode, expected_checks, expected_evidence_ids = definition
        validate_result_record(
            record,
            "{}[{}]".format(context, index),
            expected_id,
            expected_mode,
            expected_checks,
            expected_evidence_ids,
            evidence_map,
            blockers,
        )


def validate_gate(
    raw_gate: Any,
    context: str,
    detail_keys: Sequence[str],
    expected_evidence_ids: Sequence[str],
    evidence_map: Mapping[str, Mapping[str, Any]],
    blockers: List[str],
) -> None:
    gate = require_object(raw_gate, context)
    require_exact_keys(gate, GATE_KEYS, context)
    status = validate_status_value(gate["status"], "{}.status".format(context))
    tester = require_string(gate["tester"], "{}.tester".format(context))
    tested_at = require_nullable_timestamp(
        gate["tested_at_utc"], "{}.tested_at_utc".format(context)
    )
    evidence_ids = validate_string_array(gate["evidence_ids"], "{}.evidence_ids".format(context))
    checks = validate_string_array(
        gate["checks_confirmed"], "{}.checks_confirmed".format(context)
    )
    details = require_object(gate["details"], "{}.details".format(context))
    require_exact_keys(details, set(detail_keys), "{}.details".format(context))
    for detail_key in detail_keys:
        require_string(details[detail_key], "{}.details.{}".format(context, detail_key))
    notes = require_string(gate["notes"], "{}.notes".format(context))
    for evidence_id in evidence_ids:
        if evidence_id not in evidence_map:
            raise VerificationError("{} references unknown evidence {!r}".format(context, evidence_id))
    if status == "PASS":
        require_nonplaceholder(tester, "{}.tester".format(context))
        if tested_at is None:
            raise VerificationError("{} PASS requires tested_at_utc".format(context))
        if not evidence_ids:
            raise VerificationError("{} PASS requires evidence_ids".format(context))
        for evidence_id in evidence_ids:
            if evidence_map[evidence_id]["status"] != "PASS":
                raise VerificationError(
                    "{} PASS references non-PASS evidence {!r}".format(context, evidence_id)
                )
        if evidence_ids != list(expected_evidence_ids):
            raise VerificationError("{} PASS evidence_ids are not exact".format(context))
        if checks != list(detail_keys):
            raise VerificationError("{} PASS checks_confirmed are not exact".format(context))
        for detail_key in detail_keys:
            require_nonplaceholder(
                details[detail_key], "{}.details.{}".format(context, detail_key)
            )
        reject_blocking_language(notes, "{}.notes".format(context))
    elif checks:
        raise VerificationError("{} non-PASS checks_confirmed must be empty".format(context))
    if status != "PASS":
        blockers.append("{}={}".format(context.removeprefix("status."), status))


def require_interactive_coverage(
    raw_record: Any,
    context: str,
    coverage_token: str,
    candidate_sha256: str,
    evidence_map: Mapping[str, Mapping[str, Any]],
) -> None:
    record = require_object(raw_record, context)
    if record["status"] != "PASS":
        return
    tester = require_string(record["tester"], "{}.tester".format(context))
    tested_at = require_timestamp(
        record["tested_at_utc"], "{}.tested_at_utc".format(context)
    )
    evidence_ids = require_array(record["evidence_ids"], "{}.evidence_ids".format(context))
    for evidence_id in evidence_ids:
        evidence = evidence_map[evidence_id]
        evidence_context = "status.evidence.{}".format(evidence_id)
        interactive = require_object(
            evidence["interactive"], "{}.interactive".format(evidence_context)
        )
        if interactive["candidate_sha256"] != candidate_sha256:
            raise VerificationError(
                "{} interactive evidence is not bound to the candidate".format(context)
            )
        if interactive["tester"].strip() != tester.strip():
            raise VerificationError(
                "{} tester does not match its interactive evidence".format(context)
            )
        if interactive["tested_at_utc"] != tested_at:
            raise VerificationError(
                "{} tested_at_utc does not match its interactive evidence".format(
                    context
                )
            )
        require_nonplaceholder(
            interactive["machine"], "{}.interactive.machine".format(evidence_context)
        )
        require_nonplaceholder(
            interactive["signature"],
            "{}.interactive.signature".format(evidence_context),
        )
        coverage = require_object(
            interactive["coverage_refs"],
            "{}.interactive.coverage_refs".format(evidence_context),
        )
        if coverage_token not in coverage:
            raise VerificationError(
                "{} interactive evidence does not cover {!r}".format(
                    context, coverage_token
                )
            )
        if not any(
            artifact["kind"] in {"recording", "log", "report"}
            for artifact in evidence["artifacts"]
        ):
            raise VerificationError(
                "{} requires a recording, log, or signed report".format(context)
            )
        reject_blocking_language(
            evidence["notes"], "{}.notes".format(evidence_context)
        )


def dynamic_evidence_tokens(status: Mapping[str, Any]) -> Dict[str, Set[str]]:
    tokens: Dict[str, Set[str]] = {evidence_id: set() for evidence_id in EVIDENCE_IDS}
    definitions = (
        ("gameplay", lambda row: "gameplay:{}:{}".format(row["id"], row["mode"])),
        ("bases", lambda row: "base:{}:{}".format(row["id"], row["mode"])),
        ("pickups", lambda row: "pickup:{}:{}".format(row["id"], row["mode"])),
        ("settlement", lambda row: "settlement:{}".format(row["id"])),
    )
    for section, token_builder in definitions:
        for row in status[section]:
            if row["status"] == "PASS":
                for evidence_id in row["evidence_ids"]:
                    tokens[evidence_id].add(token_builder(row))
    if status["published_controls"]["status"] == "PASS":
        for evidence_id in status["published_controls"]["evidence_ids"]:
            tokens[evidence_id].add("controls:published_controls_match")
    for gate_name in ("clean_mac", "gatekeeper", "extended_session"):
        if status[gate_name]["status"] == "PASS":
            for evidence_id in status[gate_name]["evidence_ids"]:
                tokens[evidence_id].add("gate:{}".format(gate_name))
    return tokens


def validate_gameplay_event_log(
    value: Mapping[str, Any],
    context: str,
    evidence_id: str,
    interactive: Mapping[str, Any],
    expected_tokens: Set[str],
) -> None:
    require_exact_keys(value, set(GAMEPLAY_EVENT_LOG_KEYS), context)
    if value["schema"] != GAMEPLAY_EVENT_LOG_SCHEMA or value["producer"] != "Tanks3D":
        raise VerificationError(
            "{} is not a candidate-bound Tanks3D QA event log".format(context)
        )
    for key in ("candidate_sha256", "tester", "machine"):
        if value[key] != interactive[key]:
            raise VerificationError("{}.{} does not match interactive evidence".format(context, key))
    started = timestamp_value(require_timestamp(value["started_at_utc"], context + ".started_at_utc"))
    completed = timestamp_value(require_timestamp(value["completed_at_utc"], context + ".completed_at_utc"))
    if completed < started:
        raise VerificationError("{} ends before it starts".format(context))
    events = require_array(value["events"], context + ".events")
    seen: Set[str] = set()
    for index, raw_event in enumerate(events):
        event_context = "{}.events[{}]".format(context, index)
        event = require_object(raw_event, event_context)
        require_exact_keys(event, set(GAMEPLAY_EVENT_KEYS), event_context)
        if event["sequence"] != index + 1:
            raise VerificationError("{}.sequence must be contiguous from 1".format(event_context))
        event_time = timestamp_value(require_timestamp(event["timestamp_utc"], event_context + ".timestamp_utc"))
        if not (started <= event_time <= completed):
            raise VerificationError("{} lies outside the event-log session".format(event_context))
        if event["category_id"] != evidence_id or event["result"] != "PASS":
            raise VerificationError("{} has the wrong category or result".format(event_context))
        token = require_nonplaceholder(event["coverage_token"], event_context + ".coverage_token")
        if token in seen:
            raise VerificationError("{} duplicates coverage token {!r}".format(context, token))
        seen.add(token)
        if interactive["coverage_refs"].get(token) != "event:{}".format(index + 1):
            raise VerificationError("{} is not bound to its event sequence".format(event_context))
    if seen != expected_tokens:
        raise VerificationError("{} event coverage is not exact".format(context))


def validate_structured_interactive_evidence(
    status: Mapping[str, Any],
    evidence_map: Mapping[str, Mapping[str, Any]],
    artifact_map: Mapping[str, Tuple[str, str, str, Path]],
    candidate_sha256: str,
) -> None:
    token_map = dynamic_evidence_tokens(status)
    gameplay_categories = {
        "one_player_gameplay",
        "two_player_gameplay",
        "national_bases",
        "pickup_and_minimap",
        "settlement_report",
    }
    for evidence_id, expected_tokens in token_map.items():
        if not expected_tokens:
            continue
        evidence = evidence_map[evidence_id]
        interactive = require_object(evidence["interactive"], "interactive evidence")
        if interactive["tester"].strip().casefold() == evidence["reviewer"].strip().casefold():
            raise VerificationError(
                "interactive evidence tester and reviewer must be different people"
            )
        structured = []
        for artifact in evidence["artifacts"]:
            normalized = artifact_map[artifact["path"]]
            parsed = load_structured_artifact(normalized[3], "evidence artifact " + normalized[0])
            if parsed is not None:
                structured.append((normalized, parsed))
        sessions = [item for item in structured if item[1].get("schema") == INTERACTIVE_SESSION_SCHEMA]
        if len(sessions) != 1:
            raise VerificationError(
                "evidence.{} requires exactly one structured interactive session report".format(evidence_id)
            )
        session_artifact, session = sessions[0]
        require_exact_keys(session, set(INTERACTIVE_SESSION_KEYS), "interactive session")
        for key in ("candidate_sha256", "tester", "machine", "signature"):
            if session[key] != interactive[key]:
                raise VerificationError("interactive session {} does not match evidence".format(key))
        started = timestamp_value(require_timestamp(session["started_at_utc"], "interactive session.started_at_utc"))
        completed = timestamp_value(require_timestamp(session["completed_at_utc"], "interactive session.completed_at_utc"))
        tested = timestamp_value(interactive["tested_at_utc"])
        reviewed = timestamp_value(evidence["reviewed_at_utc"])
        if not (started <= tested <= completed <= reviewed):
            raise VerificationError("interactive session timestamps are not chronological")
        categories = require_array(session["categories"], "interactive session.categories")
        matching = []
        for index, raw_category in enumerate(categories):
            category = require_object(raw_category, "interactive session.categories[{}]".format(index))
            require_exact_keys(category, set(INTERACTIVE_CATEGORY_KEYS), "interactive session category")
            if category["id"] == evidence_id:
                matching.append(category)
        if len(matching) != 1:
            raise VerificationError("interactive session must contain exactly one category record")
        category = matching[0]
        if (
            category["tested_at_utc"] != interactive["tested_at_utc"]
            or category["reviewed_at_utc"] != evidence["reviewed_at_utc"]
            or category["result"] != "PASS"
            or category["coverage_refs"] != interactive["coverage_refs"]
        ):
            raise VerificationError("interactive session category contradicts release status")
        hashes = validate_string_array(category["artifact_sha256s"], "interactive session artifact_sha256s")
        expected_hashes = [
            artifact["sha256"]
            for artifact in evidence["artifacts"]
            if artifact["path"] != session_artifact[0]
        ]
        if hashes != expected_hashes or not hashes:
            raise VerificationError("interactive session artifact hashes are not exact")
        if evidence_id in gameplay_categories:
            event_logs = [item for item in structured if item[1].get("schema") == GAMEPLAY_EVENT_LOG_SCHEMA]
            if len(event_logs) != 1:
                raise VerificationError(
                    "evidence.{} requires exactly one candidate-bound signed QA event log".format(
                        evidence_id
                    )
                )
            if (
                event_logs[0][1].get("started_at_utc") != session["started_at_utc"]
                or event_logs[0][1].get("completed_at_utc")
                != session["completed_at_utc"]
            ):
                raise VerificationError(
                    "gameplay event log interval does not match its interactive session"
                )
            validate_gameplay_event_log(
                event_logs[0][1],
                "gameplay event log",
                evidence_id,
                interactive,
                expected_tokens,
            )


def require_affirmative(value: Any, context: str) -> None:
    text = require_string(value, context).strip().lower()
    if text not in {"yes", "true", "pass", "verified"}:
        raise VerificationError("{} must explicitly record yes/true/PASS/verified".format(context))


def require_number_at_least(value: Any, minimum: float, context: str) -> float:
    text = require_string(value, context).strip()
    try:
        number = float(text)
    except ValueError:
        raise VerificationError("{} must be numeric".format(context))
    if not math.isfinite(number):
        raise VerificationError("{} must be finite".format(context))
    if not (number >= minimum):
        raise VerificationError("{} must be at least {}".format(context, minimum))
    return number


def parse_version(value: str, context: str) -> Tuple[int, ...]:
    if re.fullmatch(r"\d+(?:\.\d+)*", value) is None:
        raise VerificationError("{} must be a dotted numeric version".format(context))
    return tuple(int(component) for component in value.split("."))


def structured_artifacts_for_evidence(
    evidence: Mapping[str, Any],
    artifact_map: Mapping[str, Tuple[str, str, str, Path]],
) -> List[Mapping[str, Any]]:
    result = []
    for artifact in evidence["artifacts"]:
        normalized = artifact_map[artifact["path"]]
        parsed = load_structured_artifact(normalized[3], "evidence artifact " + normalized[0])
        if parsed is not None:
            result.append(parsed)
    return result


def parse_command_argv(value: Any, context: str) -> List[str]:
    values = require_array(value, context)
    result = [require_nonplaceholder(item, "{}[{}]".format(context, index)) for index, item in enumerate(values)]
    if not result:
        raise VerificationError("{} must not be empty".format(context))
    return result


def validate_clean_mac_command_log(
    status: Mapping[str, Any],
    evidence_map: Mapping[str, Mapping[str, Any]],
    artifact_map: Mapping[str, Tuple[str, str, str, Path]],
    artifact_name: str,
    artifact_sha256: str,
) -> None:
    if status["clean_mac"]["status"] != "PASS" and status["gatekeeper"]["status"] != "PASS":
        return
    values = structured_artifacts_for_evidence(evidence_map["gatekeeper_launch"], artifact_map)
    logs = [value for value in values if value.get("schema") == COMMAND_LOG_SCHEMA]
    if len(logs) != 1:
        raise VerificationError("Gatekeeper PASS requires exactly one structured command log")
    log = logs[0]
    require_exact_keys(log, set(COMMAND_LOG_KEYS), "command log")
    interactive = evidence_map["gatekeeper_launch"]["interactive"]
    if log["candidate_sha256"] != artifact_sha256 or log["machine"] != interactive["machine"]:
        raise VerificationError("command log is not bound to the candidate and machine")
    commands = require_array(log["commands"], "command log.commands")
    sessions = [
        value for value in values if value.get("schema") == INTERACTIVE_SESSION_SCHEMA
    ]
    if len(sessions) != 1:
        raise VerificationError(
            "Gatekeeper PASS requires exactly one interactive session for its command log"
        )
    session = sessions[0]
    session_started = timestamp_value(
        require_timestamp(
            session["started_at_utc"], "Gatekeeper interactive session.started_at_utc"
        )
    )
    session_completed = timestamp_value(
        require_timestamp(
            session["completed_at_utc"],
            "Gatekeeper interactive session.completed_at_utc",
        )
    )
    expected_ids = ["download", "checksum", "zip_quarantine", "app_quarantine", "codesign", "spctl"]
    if len(commands) != len(expected_ids):
        raise VerificationError("command log must contain the six canonical commands")
    command_map: Dict[str, Mapping[str, Any]] = {}
    previous_completed: Optional[_datetime.datetime] = None
    for index, raw_command in enumerate(commands):
        context = "command log.commands[{}]".format(index)
        command = require_object(raw_command, context)
        require_exact_keys(command, set(COMMAND_RESULT_KEYS), context)
        if command["id"] != expected_ids[index]:
            raise VerificationError("command log command order is not canonical")
        argv = parse_command_argv(command["argv"], context + ".argv")
        if isinstance(command["exit_code"], bool) or not isinstance(command["exit_code"], int):
            raise VerificationError(context + ".exit_code must be an integer")
        require_string(command["stdout"], context + ".stdout")
        require_string(command["stderr"], context + ".stderr")
        started = timestamp_value(require_timestamp(command["started_at_utc"], context + ".started_at_utc"))
        completed = timestamp_value(require_timestamp(command["completed_at_utc"], context + ".completed_at_utc"))
        if completed < started:
            raise VerificationError(context + " ends before it starts")
        if previous_completed is not None and started < previous_completed:
            raise VerificationError(
                "command log timestamps do not follow canonical command order"
            )
        if started < session_started or completed > session_completed:
            raise VerificationError(
                context + " lies outside the Gatekeeper interactive session"
            )
        previous_completed = completed
        command_map[command["id"]] = dict(command, argv=argv)

    clean = status["clean_mac"]["details"]
    gate = status["gatekeeper"]["details"]
    url = clean["download_url"].strip()
    parsed_url = urlsplit(url)
    host = (parsed_url.hostname or "").lower()
    forbidden_hosts = {"example.com", "example.org", "example.net", "localhost", "127.0.0.1", "::1"}
    if (
        parsed_url.scheme != "https"
        or not host
        or host in forbidden_hosts
        or host.endswith((".test", ".invalid", ".localhost"))
        or any(token in host for token in ("fixture", "placeholder"))
        or parsed_url.username is not None
        or parsed_url.fragment
        or unquote(Path(parsed_url.path).name) != artifact_name
    ):
        raise VerificationError("status.clean_mac.details.download_url must be a non-test HTTPS candidate URL")
    if clean["download_client"].strip() != "curl":
        raise VerificationError(
            "status.clean_mac.details.download_client must be curl for reproducible command evidence"
        )
    expected_argv = {
        "download": ["curl", "--fail", "--location", "--output", artifact_name, url],
        "checksum": ["shasum", "-a", "256", artifact_name],
        "zip_quarantine": ["xattr", "-p", "com.apple.quarantine", artifact_name],
        "app_quarantine": ["xattr", "-p", "com.apple.quarantine", "Tanks3D.app"],
        "codesign": [
            "codesign",
            "--verify",
            "--deep",
            "--strict",
            "--verbose=4",
            "Tanks3D.app",
        ],
        "spctl": [
            "spctl",
            "--assess",
            "--type",
            "execute",
            "--verbose=4",
            "Tanks3D.app",
        ],
    }
    detail_commands = {
        "checksum": clean["checksum_command"],
        "zip_quarantine": gate["zip_quarantine_command"],
        "app_quarantine": gate["app_quarantine_command"],
        "codesign": gate["codesign_command"],
        "spctl": gate["spctl_command"],
    }
    for command_id, expected in expected_argv.items():
        command = command_map[command_id]
        if command["argv"] != expected:
            raise VerificationError("{} command arguments are not candidate-bound".format(command_id))
        if command_id in detail_commands:
            try:
                detail_argv = shlex.split(detail_commands[command_id])
            except ValueError as exc:
                raise VerificationError("malformed {} command: {}".format(command_id, exc))
            if detail_argv != expected:
                raise VerificationError("status details do not record exact {} arguments".format(command_id))
    if command_map["download"]["exit_code"] != 0 or command_map["checksum"]["exit_code"] != 0:
        raise VerificationError("download and checksum commands must exit 0")
    if artifact_sha256 not in command_map["checksum"]["stdout"] or artifact_name not in command_map["checksum"]["stdout"]:
        raise VerificationError("checksum command log does not bind the candidate digest")
    for command_id, output_key, exit_key in (
        ("zip_quarantine", "zip_quarantine_output", "zip_quarantine_exit_code"),
        ("app_quarantine", "app_quarantine_output", "app_quarantine_exit_code"),
        ("codesign", None, "codesign_exit_code"),
        ("spctl", None, "spctl_exit_code"),
    ):
        command = command_map[command_id]
        if str(command["exit_code"]) != gate[exit_key].strip():
            raise VerificationError("{} exit code contradicts command log".format(command_id))
        if output_key and command["stdout"].strip() != gate[output_key].strip():
            raise VerificationError("{} output contradicts command log".format(command_id))
    spctl = command_map["spctl"]
    spctl_text = (spctl["stdout"] + "\n" + spctl["stderr"]).lower()
    expected_word = "accepted" if spctl["exit_code"] == 0 else "rejected"
    if expected_word not in spctl_text:
        raise VerificationError("spctl command log does not record its assessment outcome")


def validate_performance_log(
    status: Mapping[str, Any],
    evidence_map: Mapping[str, Mapping[str, Any]],
    artifact_map: Mapping[str, Tuple[str, str, str, Path]],
    candidate_sha256: str,
) -> None:
    if status["extended_session"]["status"] != "PASS":
        return
    values = structured_artifacts_for_evidence(evidence_map["extended_session_metrics"], artifact_map)
    logs = [value for value in values if value.get("schema") == PERFORMANCE_LOG_SCHEMA]
    if len(logs) != 1:
        raise VerificationError("extended-session PASS requires exactly one raw performance log")
    log = logs[0]
    require_exact_keys(log, set(PERFORMANCE_LOG_KEYS), "performance log")
    if log["candidate_sha256"] != candidate_sha256:
        raise VerificationError("performance log is not bound to the candidate")
    started = timestamp_value(require_timestamp(log["started_at_utc"], "performance log.started_at_utc"))
    completed = timestamp_value(require_timestamp(log["completed_at_utc"], "performance log.completed_at_utc"))
    sessions = [
        value for value in values if value.get("schema") == INTERACTIVE_SESSION_SCHEMA
    ]
    if len(sessions) != 1:
        raise VerificationError(
            "extended-session PASS requires exactly one matching interactive session"
        )
    if (
        log["started_at_utc"] != sessions[0]["started_at_utc"]
        or log["completed_at_utc"] != sessions[0]["completed_at_utc"]
    ):
        raise VerificationError(
            "performance log interval does not match its interactive session"
        )
    duration_seconds = (completed - started).total_seconds()
    details = status["extended_session"]["details"]
    interval = require_number_at_least(details["sampling_interval_seconds"], 0, "sampling interval")
    if not (PERFORMANCE_THRESHOLDS["minimum_sampling_interval_seconds"] <= interval <= PERFORMANCE_THRESHOLDS["maximum_sampling_interval_seconds"]):
        raise VerificationError("extended-session sampling interval is outside the fixed Alpha range")
    samples = require_array(log["samples"], "performance log.samples")
    expected_count = duration_seconds / interval + 1
    if len(samples) < math.floor(expected_count * PERFORMANCE_THRESHOLDS["minimum_sampling_coverage_ratio"]):
        raise VerificationError("performance log has insufficient raw sampling coverage")
    elapsed_values: List[float] = []
    fps_values: List[float] = []
    memory_values: List[float] = []
    for index, raw_sample in enumerate(samples):
        context = "performance log.samples[{}]".format(index)
        sample = require_object(raw_sample, context)
        require_exact_keys(sample, set(PERFORMANCE_SAMPLE_KEYS), context)
        elapsed = require_json_number(sample["elapsed_seconds"], context + ".elapsed_seconds")
        fps = require_json_number(sample["fps"], context + ".fps")
        memory = require_json_number(sample["memory_mb"], context + ".memory_mb")
        if elapsed < 0 or fps <= 0 or fps > 1000 or memory <= 0 or memory > 262144:
            raise VerificationError(context + " contains implausible values")
        if elapsed_values and not (elapsed_values[-1] < elapsed <= elapsed_values[-1] + interval * 1.25):
            raise VerificationError("performance log sample spacing is invalid")
        elapsed_values.append(elapsed)
        fps_values.append(fps)
        memory_values.append(memory)
    if not elapsed_values or elapsed_values[0] > interval or abs(elapsed_values[-1] - duration_seconds) > interval:
        raise VerificationError("performance log does not span the measured session")
    average = sum(fps_values) / len(fps_values)
    low_count = max(1, math.ceil(len(fps_values) * 0.01))
    one_percent_low = sum(sorted(fps_values)[:low_count]) / low_count
    actual_metrics = {
        "duration_minutes": duration_seconds / 60,
        "average_fps": average,
        "minimum_fps": min(fps_values),
        "one_percent_low_fps": one_percent_low,
        "memory_start_mb": memory_values[0],
        "memory_end_mb": memory_values[-1],
    }
    for key, actual in actual_metrics.items():
        recorded = require_number_at_least(details[key], 0, "extended-session " + key)
        if abs(recorded - actual) > 0.11:
            raise VerificationError("extended-session {} contradicts raw samples".format(key))
    fixed_values = {
        "minimum_average_fps": PERFORMANCE_THRESHOLDS["minimum_average_fps"],
        "minimum_one_percent_low_fps": PERFORMANCE_THRESHOLDS["minimum_one_percent_low_fps"],
        "maximum_memory_growth_mb": PERFORMANCE_THRESHOLDS["maximum_memory_growth_mb"],
    }
    for key, expected in fixed_values.items():
        if require_number_at_least(details[key], 0, "extended-session " + key) != expected:
            raise VerificationError("extended-session {} must use the fixed Alpha threshold".format(key))
    if duration_seconds / 60 < PERFORMANCE_THRESHOLDS["minimum_duration_minutes"]:
        raise VerificationError("performance log is shorter than the fixed Alpha duration")
    if average < PERFORMANCE_THRESHOLDS["minimum_average_fps"] or one_percent_low < PERFORMANCE_THRESHOLDS["minimum_one_percent_low_fps"]:
        raise VerificationError("raw performance samples miss the fixed Alpha FPS threshold")
    if max(memory_values) - memory_values[0] > PERFORMANCE_THRESHOLDS["maximum_memory_growth_mb"]:
        raise VerificationError("raw performance samples exceed the fixed Alpha memory-growth limit")


def validate_pass_gate_semantics(
    status: Mapping[str, Any],
    release: Mapping[str, Any],
    artifact_name: str,
    artifact_sha256: str,
) -> None:
    clean = require_object(status["clean_mac"], "status.clean_mac")
    if clean["status"] == "PASS":
        details = require_object(clean["details"], "status.clean_mac.details")
        if details["uname_machine"].strip() != "arm64":
            raise VerificationError("status.clean_mac.details.uname_machine must be arm64")
        if "apple" not in details["chip"].strip().lower():
            raise VerificationError("status.clean_mac.details.chip must identify Apple Silicon")
        for key in (
            "prior_app_absent",
            "prior_approval_absent",
            "minimum_macos_met",
            "source_checkout_absent",
            "homebrew_raylib_unused",
        ):
            require_affirmative(details[key], "status.clean_mac.details.{}".format(key))
        if details["downloaded_artifact_filename"].strip() != artifact_name:
            raise VerificationError(
                "status.clean_mac.details.downloaded_artifact_filename does not "
                "match the candidate"
            )
        if details["downloaded_artifact_sha256"].strip() != artifact_sha256:
            raise VerificationError(
                "status.clean_mac.details.downloaded_artifact_sha256 does not "
                "match the candidate"
            )
        if details["checksum_exit_code"].strip() != "0":
            raise VerificationError(
                "status.clean_mac.details.checksum_exit_code must be 0"
            )
        checksum_command = details["checksum_command"].strip()
        if "shasum" not in checksum_command or artifact_name not in checksum_command:
            raise VerificationError(
                "status.clean_mac.details.checksum_command must verify the candidate"
            )
        download_url = details["download_url"].strip().lower()
        if not download_url.startswith("https://") or ".invalid" in download_url:
            raise VerificationError(
                "status.clean_mac.details.download_url must be a real HTTPS URL"
            )
        minimum_match = re.search(r"-macos(\d+(?:\.\d+)*)\.zip$", artifact_name)
        if minimum_match is None:
            raise VerificationError("artifact filename has no deployment target")
        installed = parse_version(
            details["macos_version"].strip(), "status.clean_mac.details.macos_version"
        )
        minimum = parse_version(minimum_match.group(1), "artifact deployment target")
        width = max(len(installed), len(minimum))
        if installed + (0,) * (width - len(installed)) < minimum + (0,) * (
            width - len(minimum)
        ):
            raise VerificationError("clean Mac is older than the artifact deployment target")

    gatekeeper = require_object(status["gatekeeper"], "status.gatekeeper")
    if gatekeeper["status"] == "PASS":
        details = require_object(gatekeeper["details"], "status.gatekeeper.details")
        for prefix in ("zip", "app"):
            if details["{}_quarantine_exit_code".format(prefix)].strip() != "0":
                raise VerificationError(
                    "status.gatekeeper.details.{}_quarantine_exit_code must be 0".format(
                        prefix
                    )
                )
            command = details["{}_quarantine_command".format(prefix)].strip()
            if "xattr" not in command or "com.apple.quarantine" not in command:
                raise VerificationError(
                    "status.gatekeeper.details.{}_quarantine_command must read "
                    "com.apple.quarantine".format(prefix)
                )
            output = details["{}_quarantine_output".format(prefix)].strip()
            if QUARANTINE_RE.fullmatch(output) is None:
                raise VerificationError(
                    "status.gatekeeper.details.{}_quarantine_output is not a "
                    "quarantine record".format(prefix)
                )
        if details["codesign_exit_code"].strip() != "0":
            raise VerificationError("status.gatekeeper.details.codesign_exit_code must be 0")
        for key in (
            "main_menu_reached",
            "signature_preserved",
            "release_note_wording_verified",
        ):
            require_affirmative(details[key], "status.gatekeeper.details.{}".format(key))
        if details["conclusion"].strip().upper() != "PASS":
            raise VerificationError("status.gatekeeper.details.conclusion must be PASS")
        if not any(
            evidence_id == "gatekeeper_launch"
            for evidence_id in gatekeeper["evidence_ids"]
        ):
            raise VerificationError(
                "status.gatekeeper must reference gatekeeper_launch evidence"
            )

    extended = require_object(status["extended_session"], "status.extended_session")
    if extended["status"] == "PASS":
        details = require_object(extended["details"], "status.extended_session.details")
        require_number_at_least(
            details["duration_minutes"], 30.0, "status.extended_session.details.duration_minutes"
        )
        stages = require_number_at_least(
            details["stages_completed"], 1.0, "status.extended_session.details.stages_completed"
        )
        if not stages.is_integer():
            raise VerificationError("status.extended_session.details.stages_completed must be an integer")
        require_number_at_least(
            details["sampling_interval_seconds"],
            0.001,
            "status.extended_session.details.sampling_interval_seconds",
        )
        average_minimum = require_number_at_least(
            details["minimum_average_fps"],
            1.0,
            "status.extended_session.details.minimum_average_fps",
        )
        low_minimum = require_number_at_least(
            details["minimum_one_percent_low_fps"],
            1.0,
            "status.extended_session.details.minimum_one_percent_low_fps",
        )
        average = require_number_at_least(
            details["average_fps"], 0.001, "status.extended_session.details.average_fps"
        )
        minimum = require_number_at_least(
            details["minimum_fps"], 0.0, "status.extended_session.details.minimum_fps"
        )
        one_percent_low = require_number_at_least(
            details["one_percent_low_fps"],
            0.001,
            "status.extended_session.details.one_percent_low_fps",
        )
        if average < average_minimum:
            raise VerificationError("extended-session average FPS misses its criterion")
        if one_percent_low < low_minimum:
            raise VerificationError("extended-session 1% low FPS misses its criterion")
        if not (minimum <= one_percent_low <= average):
            raise VerificationError(
                "extended-session FPS metrics must satisfy minimum <= 1% low <= average"
            )
        memory_start = require_number_at_least(
            details["memory_start_mb"],
            0.0,
            "status.extended_session.details.memory_start_mb",
        )
        memory_end = require_number_at_least(
            details["memory_end_mb"],
            0.0,
            "status.extended_session.details.memory_end_mb",
        )
        memory_limit = require_number_at_least(
            details["maximum_memory_growth_mb"],
            0.0,
            "status.extended_session.details.maximum_memory_growth_mb",
        )
        if memory_end - memory_start > memory_limit:
            raise VerificationError("extended-session memory growth exceeds its criterion")
        for key in ("crash_count", "hang_count", "softlock_count"):
            count = require_number_at_least(
                details[key], 0.0, "status.extended_session.details.{}".format(key)
            )
            if not count.is_integer() or count != 0:
                raise VerificationError(
                    "status.extended_session.details.{} must be integer zero".format(key)
                )
        if details["thermal_state"].strip().lower() not in {"nominal", "fair"}:
            raise VerificationError(
                "status.extended_session.details.thermal_state must be nominal or fair"
            )
        expected_none = {
            "throttling": {"none", "none observed", "no"},
            "rendering_artifacts": {"none", "none observed"},
            "audio_issues": {"none", "none observed"},
            "crashes_hangs_or_softlocks": {"none", "none observed"},
        }
        for key, accepted in expected_none.items():
            if details[key].strip().lower() not in accepted:
                raise VerificationError(
                    "status.extended_session.details.{} records a release failure".format(
                        key
                    )
                )


def parse_attestation(path: Path) -> Dict[str, str]:
    try:
        lines = path.read_text(encoding="utf-8").splitlines()
    except (OSError, UnicodeError) as exc:
        raise VerificationError("cannot read attestation: {}".format(exc))
    result: Dict[str, str] = {}
    for line_number, line in enumerate(lines, 1):
        if not line or "=" not in line:
            raise VerificationError("malformed attestation line {}".format(line_number))
        key, value = line.split("=", 1)
        if not key or key in result:
            raise VerificationError("duplicate or empty attestation key on line {}".format(line_number))
        result[key] = value
    return result


def validate_release(
    root: Path, raw_release: Any
) -> Tuple[Mapping[str, Any], Dict[str, Tuple[Path, str]], Path]:
    release = require_object(raw_release, "status.release")
    require_exact_keys(release, RELEASE_KEYS, "status.release")
    version = require_string(release["version"], "status.release.version")
    channel = require_string(release["channel"], "status.release.channel")
    tag = require_string(release["tag"], "status.release.tag")
    commit = require_string(release["source_commit"], "status.release.source_commit")
    if VERSION_RE.fullmatch(version) is None:
        raise VerificationError("release version must use X.Y.Z without leading zeroes")
    if ALPHA_CHANNEL_RE.fullmatch(channel) is None:
        raise VerificationError("release channel must use alpha.N")
    if tag != "v{}-{}".format(version, channel) or TOKEN_RE.fullmatch(tag) is None:
        raise VerificationError("release tag does not match version and channel")
    if COMMIT_RE.fullmatch(commit) is None:
        raise VerificationError("status.release.source_commit is not a full object ID")
    expected_candidate = "build/release/{}".format(tag)
    if release["candidate_dir"] != expected_candidate:
        raise VerificationError("status.release.candidate_dir does not match the tag")
    candidate_dir = resolve_repository_directory(
        root, release["candidate_dir"], "status.release.candidate_dir"
    )
    file_refs: Dict[str, Tuple[Path, str]] = {}
    for key in ("artifact", "checksum", "attestation", "gate_log", "build_config"):
        file_refs[key] = verify_file_reference(
            root, release[key], "status.release.{}".format(key)
        )
        expected_parent = candidate_dir
        if file_refs[key][0].parent != expected_parent:
            raise VerificationError("status.release.{} is outside candidate_dir".format(key))
    artifact_name = file_refs["artifact"][0].name
    expected_prefix = "Tanks3D-{}-{}-macos-".format(version, channel)
    if not artifact_name.startswith(expected_prefix) or not artifact_name.endswith(".zip"):
        raise VerificationError("artifact filename does not match release identity")
    if file_refs["checksum"][0].name != artifact_name + ".sha256":
        raise VerificationError("checksum filename does not belong to the artifact")
    if file_refs["attestation"][0].name != "attestation.txt":
        raise VerificationError("attestation filename is not canonical")
    if file_refs["gate_log"][0].name != "alpha-candidate-gates.log":
        raise VerificationError("gate log filename is not canonical")
    if file_refs["build_config"][0].name != "build-config.txt":
        raise VerificationError("build config filename is not canonical")

    attestation = parse_attestation(file_refs["attestation"][0])
    expected_attestation = {
        "source_commit": commit,
        "source_head_at_start": commit,
        "source_head_at_finish": commit,
        "source_tag": tag,
        "source_tag_commit": commit,
        "source_tree": "clean",
        "app_version": version,
        "dist_channel": channel,
        "artifact_filename": artifact_name,
        "artifact_sha256": file_refs["artifact"][1],
        "checksum_filename": file_refs["checksum"][0].name,
        "build_config_filename": file_refs["build_config"][0].name,
        "build_config_sha256": file_refs["build_config"][1],
        "gate_log_filename": file_refs["gate_log"][0].name,
        "gate_log_sha256": file_refs["gate_log"][1],
    }
    for key, expected in expected_attestation.items():
        if attestation.get(key) != expected:
            raise VerificationError("attestation {} does not match status.release".format(key))
    return release, file_refs, candidate_dir


def invoke_tagged_candidate_verifier(root: Path, candidate_dir: Path) -> None:
    verifier = root / "scripts" / "verify_tagged_alpha_candidate.sh"
    if not verifier.is_file() or verifier.is_symlink():
        raise VerificationError("scripts/verify_tagged_alpha_candidate.sh is missing")
    try:
        completed = subprocess.run(
            ["sh", str(verifier), str(root), str(candidate_dir)],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            check=False,
        )
    except OSError as exc:
        raise VerificationError("cannot run tagged candidate verifier: {}".format(exc))
    if completed.returncode != 0:
        detail = (completed.stderr or completed.stdout).strip()
        if len(detail) > 600:
            detail = detail[-600:]
        raise VerificationError(
            "tagged candidate verifier failed (exit {}): {}".format(
                completed.returncode, detail or "no diagnostic"
            )
        )


def validate_canonical_gate_table(text: str, label: str) -> None:
    heading = "## Release gate summary"
    if text.count(heading) != 1:
        raise VerificationError("{} must contain one canonical release gate summary".format(label))
    tail = text.split(heading, 1)[1].lstrip("\r\n")
    lines = tail.splitlines()
    expected = ["| Gate | Status |", "| --- | --- |"] + [
        "| {} | PASS |".format(row) for row in DOCUMENT_GATE_ROWS
    ]
    if lines[: len(expected)] != expected:
        raise VerificationError("{} release gate summary is not canonical".format(label))
    if len(lines) > len(expected) and lines[len(expected)].lstrip().startswith("|"):
        raise VerificationError("{} release gate summary has an extra row".format(label))


def validate_documents(
    root: Path,
    raw_documents: Any,
    release: Mapping[str, Any],
    file_refs: Mapping[str, Tuple[Path, str]],
    screenshot_artifacts: Mapping[str, Tuple[str, str, str, Path]],
    evidence_map: Mapping[str, Mapping[str, Any]],
    release_ready: bool,
    audio_decision: str,
    known_issues: Mapping[str, Any],
    approvals: Sequence[Mapping[str, Any]],
    release_date: Optional[str],
) -> None:
    documents = require_object(raw_documents, "status.documents")
    require_exact_keys(documents, DOCUMENT_KEYS, "status.documents")
    page, _ = verify_file_reference(root, documents["release_page"], "status.documents.release_page")
    qa, _ = verify_file_reference(root, documents["qa_report"], "status.documents.qa_report")
    expected_page = root / "docs" / "releases" / "{}.md".format(release["tag"])
    expected_qa = root / "docs" / "releases" / "{}-qa.md".format(release["tag"])
    if page != expected_page or qa != expected_qa:
        raise VerificationError("release page or QA report path does not match the release tag")
    try:
        page_text = page.read_text(encoding="utf-8")
        qa_text = qa.read_text(encoding="utf-8")
    except (OSError, UnicodeError) as exc:
        raise VerificationError("cannot read release documents: {}".format(exc))
    identity_tokens = [
        release["tag"],
        release["source_commit"],
        file_refs["artifact"][0].name,
        file_refs["artifact"][1],
    ]
    for label, text in (("release page", page_text), ("QA report", qa_text)):
        for token in identity_tokens:
            if token not in text:
                raise VerificationError("{} does not reference {!r}".format(label, token))
    if qa.name not in page_text:
        raise VerificationError("release page does not link the QA report")

    if release_ready:
        validate_canonical_gate_table(page_text, "release page")
        validate_canonical_gate_table(qa_text, "QA report")
        if page_text.count("**Release status: APPROVED.**") != 1:
            raise VerificationError("release page must contain one canonical APPROVED marker")
        if qa_text.count("**Overall Alpha gate: PASS.**") != 1:
            raise VerificationError("QA report must contain one canonical PASS marker")
        required_markers = (
            ("release page", page_text, "**Release status: APPROVED.**"),
            ("QA report", qa_text, "**Overall Alpha gate: PASS.**"),
            (
                "release page",
                page_text,
                "Audio decision: **{}**".format(audio_decision),
            ),
            (
                "QA report",
                qa_text,
                "Selected option: **{}**".format(audio_decision),
            ),
            (
                "release page",
                page_text,
                "Known-issues review: **{}**".format(known_issues["conclusion"]),
            ),
            (
                "QA report",
                qa_text,
                "Known-issues review: **{}**".format(known_issues["conclusion"]),
            ),
            ("release page", page_text, "Gatekeeper conclusion: **PASS**"),
            ("QA report", qa_text, "Gatekeeper conclusion: **PASS**"),
        )
        for label, text, marker in required_markers:
            if marker not in text:
                raise VerificationError("{} is missing {!r}".format(label, marker))
        forbidden_markers = (
            "Overall Alpha gate: BLOCKED",
            "Release status: BLOCKED",
            "Public release remains blocked",
            "NOT RUN — BLOCKED",
            "NOT RECORDED — BLOCKED",
            "NOT SIGNED",
            "NONE — BLOCKED",
            "Selected option: **NONE",
        )
        for label, text in (("release page", page_text), ("QA report", qa_text)):
            for marker in forbidden_markers:
                if marker in text:
                    raise VerificationError(
                        "{} retains blocked marker {!r}".format(label, marker)
                    )
            if re.search(r"(?i)\b(?:BLOCKED|NOT[ _-]RUN|FAIL(?:ED)?)\b", text):
                raise VerificationError("{} contains a contradictory release status".format(label))
            gate_subject = (
                r"(?:Gatekeeper|clean[- ]Mac|downloaded[- ]quarantine|interactive|"
                r"gameplay|listening|audio|spctl|performance|validation|"
                r"verification|manual QA|approval)"
            )
            incomplete = (
                r"(?:not yet|await(?:s|ing)?|pending|outstanding|missing|"
                r"incomplete|undone|unverified|untested|deferred|skipped|"
                r"forthcoming|TBD|TODO|has yet to|have yet to|"
                r"still\s+(?:outstanding|open|pending|required|needed)|"
                r"never\s+(?:been\s+)?(?:tested|verified|listened(?:\s+to)?|"
                r"run|completed)|remain(?:s|ing)?\s+"
                r"(?:undone|incomplete|unverified|untested)|did not (?:pass|run|"
                r"complete|verify)|not (?:documented|verified|tested|recorded)|"
                r"(?:scheduled|planned)\s+for\s+(?:later|a future)|"
                r"to be (?:done|run|conducted|completed)\s+(?:later|in the future))"
            )
            positive = (
                r"(?:PASS(?:ED)?|verified|completed|observed|tested|ACCEPT(?:ED)?|"
                r"approved|successful|reached|recorded)"
            )
            for line in text.splitlines():
                if any(issue["id"] in line for issue in known_issues["issues"]):
                    continue
                if line.lstrip().startswith("#"):
                    continue
                if re.search(r"(?i)\b{}\b".format(gate_subject), line) and (
                    re.search(r"(?i)\b{}\b".format(incomplete), line)
                    or re.search(r"(?i)\b{}\b".format(positive), line) is None
                ):
                    raise VerificationError(
                        "{} contains a contradictory release-status assertion".format(
                            label
                        )
                    )
        for issue in known_issues["issues"]:
            issue_id = issue["id"]
            if issue_id not in page_text or issue_id not in qa_text:
                raise VerificationError(
                    "release documents do not both disclose known issue {}".format(
                        issue_id
                    )
                )
        for approval in approvals:
            if approval["name"] not in qa_text:
                raise VerificationError(
                    "QA report does not identify {} approval".format(approval["role"])
                )
        if release_date is None or release_date not in page_text or release_date not in qa_text:
            raise VerificationError("release documents do not both record release_date")
    else:
        if "**Release status: BLOCKED.**" not in page_text:
            raise VerificationError("blocked release page lacks its canonical status marker")
        if "**Overall Alpha gate: BLOCKED.**" not in qa_text:
            raise VerificationError("blocked QA report lacks its canonical status marker")

    tag = release["tag"]
    screenshot_names = [
        "one-player.png",
        "two-player.png",
        "base-usa.png",
        "base-ussr.png",
        "base-germany.png",
        "bonuses.png",
        "settlement.png",
    ]
    asset_prefix = "docs/assets/releases/{}/".format(tag)
    expected_png_membership = {
        "one_player_gameplay": {asset_prefix + "one-player.png"},
        "two_player_gameplay": {asset_prefix + "two-player.png"},
        "national_bases": {
            asset_prefix + "base-usa.png",
            asset_prefix + "base-ussr.png",
            asset_prefix + "base-germany.png",
        },
        "pickup_and_minimap": {asset_prefix + "bonuses.png"},
        "settlement_report": {asset_prefix + "settlement.png"},
    }
    for evidence_id, expected_names in expected_png_membership.items():
        record = evidence_map[evidence_id]
        actual_names = {
            artifact["path"]
            for artifact in record["artifacts"]
            if artifact["kind"] == "png"
        }
        if actual_names != expected_names:
            raise VerificationError(
                "evidence.{} must contain exactly these release PNGs: {}".format(
                    evidence_id, ", ".join(sorted(expected_names))
                )
            )
    for name in screenshot_names:
        repository_path = "docs/assets/releases/{}/{}".format(tag, name)
        reference = "../assets/releases/{}/{}".format(tag, name)
        if repository_path not in screenshot_artifacts:
            raise VerificationError("release evidence is missing {}".format(repository_path))
        digest = screenshot_artifacts[repository_path][1]
        verify_png(
            screenshot_artifacts[repository_path][3],
            "release evidence {}".format(name),
            require_release_size=True,
        )
        if reference not in page_text or reference not in qa_text:
            raise VerificationError("release documents do not both reference {}".format(name))
        if digest not in qa_text:
            raise VerificationError("QA report does not bind the hash for {}".format(name))


def validate_report(raw_report: Any, blockers: List[str]) -> Mapping[str, Any]:
    report = require_object(raw_report, "status.report")
    require_exact_keys(report, REPORT_KEYS, "status.report")
    qa_owner = require_string(report["qa_owner"], "status.report.qa_owner")
    completed_at = require_nullable_timestamp(
        report["completed_at_utc"], "status.report.completed_at_utc"
    )
    release_date = report["release_date"]
    if release_date is not None:
        release_date = require_string(release_date, "status.report.release_date")
        if DATE_RE.fullmatch(release_date) is None:
            raise VerificationError("status.report.release_date must use YYYY-MM-DD")
        try:
            _datetime.datetime.strptime(release_date, "%Y-%m-%d")
        except ValueError as exc:
            raise VerificationError(
                "status.report.release_date is not a real date: {}".format(exc)
            )
    if not qa_owner and completed_at is None and release_date is None:
        blockers.append("report.metadata=NOT_RUN")
        return report
    require_nonplaceholder(qa_owner, "status.report.qa_owner")
    if completed_at is None or release_date is None:
        raise VerificationError("status.report metadata must be completed atomically")
    return report


def validate_known_issues(
    raw_known_issues: Any,
    evidence_map: Mapping[str, Mapping[str, Any]],
    blockers: List[str],
) -> Mapping[str, Any]:
    known = require_object(raw_known_issues, "status.known_issues")
    require_exact_keys(known, KNOWN_ISSUES_KEYS, "status.known_issues")
    status_value = validate_status_value(known["status"], "status.known_issues.status")
    conclusion = require_string(
        known["conclusion"], "status.known_issues.conclusion"
    )
    if conclusion not in KNOWN_ISSUE_CONCLUSIONS:
        raise VerificationError("status.known_issues.conclusion is invalid")
    reviewer = require_string(known["reviewer"], "status.known_issues.reviewer")
    reviewed_at = require_nullable_timestamp(
        known["reviewed_at_utc"], "status.known_issues.reviewed_at_utc"
    )
    signature = require_string(known["signature"], "status.known_issues.signature")
    notes = require_string(known["notes"], "status.known_issues.notes")
    issues = require_array(known["issues"], "status.known_issues.issues")
    seen_ids: Set[str] = set()
    for index, raw_issue in enumerate(issues):
        context = "status.known_issues.issues[{}]".format(index)
        issue = require_object(raw_issue, context)
        require_exact_keys(issue, KNOWN_ISSUE_KEY_SET, context)
        issue_id = require_string(issue["id"], "{}.id".format(context))
        if ISSUE_ID_RE.fullmatch(issue_id) is None or issue_id in seen_ids:
            raise VerificationError("{}.id is invalid or duplicated".format(context))
        seen_ids.add(issue_id)
        severity = require_string(issue["severity"], "{}.severity".format(context))
        if severity not in KNOWN_ISSUE_SEVERITIES:
            raise VerificationError("{}.severity is invalid".format(context))
        decision = require_string(
            issue["release_decision"], "{}.release_decision".format(context)
        )
        if decision not in KNOWN_ISSUE_DECISIONS:
            raise VerificationError("{}.release_decision is invalid".format(context))
        for key in ("summary", "reproduction", "impact", "workaround", "owner"):
            require_nonplaceholder(issue[key], "{}.{}".format(context, key))
        evidence_ids = validate_string_array(
            issue["evidence_ids"], "{}.evidence_ids".format(context)
        )
        if not evidence_ids:
            raise VerificationError("{} requires evidence_ids".format(context))
        for evidence_id in evidence_ids:
            if evidence_id not in evidence_map:
                raise VerificationError(
                    "{} references unknown evidence {!r}".format(context, evidence_id)
                )
            if evidence_map[evidence_id]["status"] != "PASS":
                raise VerificationError(
                    "{} references non-PASS evidence {!r}".format(context, evidence_id)
                )
        if severity == "BLOCKER" or decision == "FIX_BEFORE_RELEASE":
            blockers.append("known_issue.{}=UNRESOLVED".format(issue_id))

    if status_value == "PASS":
        require_nonplaceholder(reviewer, "status.known_issues.reviewer")
        require_nonplaceholder(signature, "status.known_issues.signature")
        require_nonplaceholder(notes, "status.known_issues.notes")
        reject_blocking_language(notes, "status.known_issues.notes")
        if reviewed_at is None:
            raise VerificationError("known-issues PASS requires reviewed_at_utc")
        if conclusion == "NONE":
            raise VerificationError("known-issues PASS requires a signed conclusion")
        if conclusion == "NONE_KNOWN" and issues:
            raise VerificationError("NONE_KNOWN must have an empty issue list")
        if conclusion == "RECORDED" and not issues:
            raise VerificationError("RECORDED requires at least one issue")
    else:
        blockers.append("known_issues={}".format(status_value))
        if conclusion == "NONE" and any((reviewer, signature, issues)):
            raise VerificationError("known-issues NONE must not carry a partial review")
    return known


def validate_interactive_coverage_matrix(
    status: Mapping[str, Any],
    evidence_map: Mapping[str, Mapping[str, Any]],
    candidate_sha256: str,
) -> None:
    section_tokens = (
        ("gameplay", lambda row: "gameplay:{}:{}".format(row["id"], row["mode"])),
        ("bases", lambda row: "base:{}:{}".format(row["id"], row["mode"])),
        ("pickups", lambda row: "pickup:{}:{}".format(row["id"], row["mode"])),
        ("settlement", lambda row: "settlement:{}".format(row["id"])),
    )
    for section, token_builder in section_tokens:
        for index, row in enumerate(status[section]):
            require_interactive_coverage(
                row,
                "status.{}[{}]".format(section, index),
                token_builder(row),
                candidate_sha256,
                evidence_map,
            )
    require_interactive_coverage(
        status["published_controls"],
        "status.published_controls",
        "controls:published_controls_match",
        candidate_sha256,
        evidence_map,
    )
    for gate_name in ("clean_mac", "gatekeeper", "extended_session"):
        require_interactive_coverage(
            status[gate_name],
            "status.{}".format(gate_name),
            "gate:{}".format(gate_name),
            candidate_sha256,
            evidence_map,
        )


def verify_candidate_inherited_audio(root: Path, archive: Path) -> None:
    source_dir = root / "resources" / "sounds"
    source_files = sorted(source_dir.glob("*.ogg"))
    if len(source_files) != 22 or any(path.is_symlink() for path in source_files):
        raise VerificationError("repository audio set is not exactly 22 regular OGG files")
    expected = {
        "Tanks3D.app/Contents/Resources/sounds/{}".format(path.name): sha256_file(path)
        for path in source_files
    }
    try:
        with zipfile.ZipFile(archive, "r") as bundle:
            members = [
                info
                for info in bundle.infolist()
                if info.filename.startswith(
                    "Tanks3D.app/Contents/Resources/sounds/"
                )
                and info.filename.endswith(".ogg")
            ]
            names = [info.filename for info in members]
            if len(names) != 22 or len(set(names)) != 22 or set(names) != set(expected):
                raise VerificationError(
                    "candidate archive does not contain the documented 22-sound set"
                )
            for info in members:
                if info.file_size > 20 * 1024 * 1024:
                    raise VerificationError("candidate audio member is unexpectedly large")
                digest = hashlib.sha256(bundle.read(info)).hexdigest()
                if digest != expected[info.filename]:
                    raise VerificationError(
                        "candidate audio differs from documented inherited file {}".format(
                            Path(info.filename).name
                        )
                    )
    except (OSError, zipfile.BadZipFile, RuntimeError) as exc:
        raise VerificationError("cannot inspect candidate audio: {}".format(exc))


def validate_audio(
    root: Path,
    raw_audio: Any,
    candidate_archive: Path,
    blockers: List[str],
) -> Mapping[str, Any]:
    audio = require_object(raw_audio, "status.audio")
    require_exact_keys(audio, AUDIO_KEYS, "status.audio")
    decision = require_string(audio["decision"], "status.audio.decision")
    if decision not in AUDIO_DECISIONS:
        raise VerificationError("status.audio.decision is not a permitted single choice")
    rationale = require_string(audio["rationale"], "status.audio.rationale")
    owner = require_string(audio["owner"], "status.audio.owner")
    authority = require_string(audio["authority"], "status.audio.authority")
    signature = require_string(audio["signature"], "status.audio.signature")
    decided_at = require_nullable_timestamp(audio["decided_at_utc"], "status.audio.decided_at_utc")
    checks = validate_string_array(
        audio["checks_confirmed"], "status.audio.checks_confirmed"
    )
    artifacts = require_array(audio["evidence"], "status.audio.evidence")
    audio_paths: Set[Path] = set()
    normalized_artifacts: List[Tuple[str, str, str, Path]] = []
    for index, artifact in enumerate(artifacts):
        normalized = validate_artifact(root, artifact, "status.audio.evidence[{}]".format(index))
        if normalized[3] in audio_paths:
            raise VerificationError("status.audio.evidence contains a duplicate path")
        audio_paths.add(normalized[3])
        normalized_artifacts.append(normalized)
    if decision == "NONE":
        if (
            any((rationale, owner, authority, signature, checks))
            or decided_at is not None
            or artifacts
        ):
            raise VerificationError("audio NONE must not carry a partial decision")
        blockers.append("audio.decision=NONE")
        return audio
    if decision == "REPLACE":
        raise VerificationError(
            "audio REPLACE cannot approve this candidate; build a new candidate and "
            "add replacement-manifest verification"
        )
    if decision == "CONFIRM":
        raise VerificationError(
            "audio CONFIRM cannot approve under the v1 profile without an "
            "externally trusted cryptographic rights-holder signature; use ACCEPT "
            "for an explicit risk decision or add a new trusted-signature profile"
        )
    require_nonplaceholder(rationale, "status.audio.rationale")
    require_nonplaceholder(owner, "status.audio.owner")
    require_nonplaceholder(authority, "status.audio.authority")
    require_nonplaceholder(signature, "status.audio.signature")
    if decided_at is None:
        raise VerificationError("selected audio decision requires decided_at_utc")
    expected_checks = next(
        item["checks"] for item in AUDIO_DECISION_CHECKS if item["decision"] == decision
    )
    if checks != expected_checks:
        raise VerificationError(
            "status.audio.checks_confirmed do not match the selected decision"
        )
    required_paths = {
        (root / "ASSET_LICENSES.md").resolve(),
        (root / "THIRD_PARTY_NOTICES.md").resolve(),
        (root / "LICENSES" / "MIT-upstream.txt").resolve(),
    }
    if not required_paths.issubset(audio_paths):
        raise VerificationError(
            "selected audio decision must hash the asset, third-party, and MIT notices"
        )
    decision_reports = [
        item
        for item in normalized_artifacts
        if item[3] not in required_paths and item[2] in {"report", "log"}
    ]
    if not decision_reports:
        raise VerificationError("selected audio decision requires a hashed decision report")
    verify_candidate_inherited_audio(root, candidate_archive)
    return audio


def validate_approvals(
    raw_approvals: Any,
    evidence_map: Mapping[str, Mapping[str, Any]],
    blockers: List[str],
) -> List[Mapping[str, Any]]:
    approvals = require_array(raw_approvals, "status.approvals")
    if len(approvals) != len(APPROVAL_ROLES):
        raise VerificationError("status.approvals must contain exactly two roles")
    actual_roles = []
    for index, raw_approval in enumerate(approvals):
        context = "status.approvals[{}]".format(index)
        approval = require_object(raw_approval, context)
        require_exact_keys(approval, APPROVAL_KEYS, context)
        role = require_string(approval["role"], "{}.role".format(context))
        actual_roles.append(role)
        status = validate_status_value(approval["status"], "{}.status".format(context))
        name = require_string(approval["name"], "{}.name".format(context))
        signature = require_string(approval["signature"], "{}.signature".format(context))
        approved_at = require_nullable_timestamp(
            approval["approved_at_utc"], "{}.approved_at_utc".format(context)
        )
        evidence_ids = validate_string_array(
            approval["evidence_ids"], "{}.evidence_ids".format(context)
        )
        notes = require_string(approval["notes"], "{}.notes".format(context))
        for evidence_id in evidence_ids:
            if evidence_id not in evidence_map:
                raise VerificationError("{} references unknown evidence {!r}".format(context, evidence_id))
        if status == "PASS":
            require_nonplaceholder(name, "{}.name".format(context))
            require_nonplaceholder(signature, "{}.signature".format(context))
            if approved_at is None:
                raise VerificationError("{} PASS requires approved_at_utc".format(context))
            if not evidence_ids:
                raise VerificationError("{} PASS requires evidence_ids".format(context))
            for evidence_id in evidence_ids:
                if evidence_map[evidence_id]["status"] != "PASS":
                    raise VerificationError(
                        "{} PASS references non-PASS evidence {!r}".format(context, evidence_id)
                    )
            if evidence_ids != EVIDENCE_IDS:
                raise VerificationError(
                    "{} PASS must reference all eight evidence IDs in profile order".format(
                        context
                    )
                )
            reject_blocking_language(notes, "{}.notes".format(context))
        else:
            blockers.append("approval.{}={}".format(role, status))
    if actual_roles != APPROVAL_ROLES:
        raise VerificationError("status.approvals roles/order do not match the fixed profile")
    return [require_object(item, "status.approvals") for item in approvals]


def timestamp_value(value: str) -> _datetime.datetime:
    return _datetime.datetime.strptime(value, "%Y-%m-%dT%H:%M:%SZ").replace(
        tzinfo=_datetime.timezone.utc
    )


def validate_chronology_and_ownership(
    status: Mapping[str, Any],
    report: Mapping[str, Any],
    known_issues: Mapping[str, Any],
    audio: Mapping[str, Any],
    approvals: Sequence[Mapping[str, Any]],
) -> None:
    approval_by_role = {approval["role"]: approval for approval in approvals}
    qa_approval = approval_by_role["qa_lead"]
    release_approval = approval_by_role["release_owner"]
    if qa_approval["status"] == "PASS" and release_approval["status"] == "PASS":
        if qa_approval["name"].strip().casefold() == release_approval["name"].strip().casefold():
            raise VerificationError(
                "qa_lead and release_owner approvals must be independent"
            )
    if known_issues["status"] == "PASS" and qa_approval["status"] == "PASS":
        if known_issues["reviewer"].strip() != qa_approval["name"].strip():
            raise VerificationError("known-issues reviewer must be the QA lead")
        if known_issues["signature"].strip() != qa_approval["signature"].strip():
            raise VerificationError("known-issues signature must match QA approval")
    if report["completed_at_utc"] is not None and qa_approval["status"] == "PASS":
        if report["qa_owner"].strip() != qa_approval["name"].strip():
            raise VerificationError("status.report.qa_owner must be the QA lead")
    if audio["decision"] != "NONE" and release_approval["status"] == "PASS":
        if audio["owner"].strip() != release_approval["name"].strip():
            raise VerificationError("audio decision owner must be the release owner")
        if audio["signature"].strip() != release_approval["signature"].strip():
            raise VerificationError("audio signature must match release-owner approval")

    tested_times: List[_datetime.datetime] = []
    for section in ("gameplay", "bases", "pickups", "settlement"):
        for record in status[section]:
            if record["status"] == "PASS":
                tested_times.append(timestamp_value(record["tested_at_utc"]))
    if status["published_controls"]["status"] == "PASS":
        tested_times.append(
            timestamp_value(status["published_controls"]["tested_at_utc"])
        )
    for section in ("clean_mac", "gatekeeper", "extended_session"):
        if status[section]["status"] == "PASS":
            tested_times.append(timestamp_value(status[section]["tested_at_utc"]))
    evidence_review_times: List[_datetime.datetime] = []
    for evidence in status["evidence"]:
        if evidence["status"] == "PASS":
            reviewed = timestamp_value(evidence["reviewed_at_utc"])
            evidence_review_times.append(reviewed)
            interactive_tested = evidence["interactive"]["tested_at_utc"]
            if interactive_tested is not None and reviewed < timestamp_value(interactive_tested):
                raise VerificationError("evidence review predates interactive testing")
    prerequisite_times = tested_times + evidence_review_times
    if known_issues["status"] == "PASS":
        known_time = timestamp_value(known_issues["reviewed_at_utc"])
        if evidence_review_times and known_time < max(evidence_review_times):
            raise VerificationError("known-issues review predates evidence review")
        prerequisite_times.append(known_time)
    if audio["decision"] != "NONE":
        prerequisite_times.append(timestamp_value(audio["decided_at_utc"]))

    report_completed = report["completed_at_utc"]
    if report_completed is not None:
        report_time = timestamp_value(report_completed)
        if prerequisite_times and report_time < max(prerequisite_times):
            raise VerificationError("QA report completion predates required evidence")
        prerequisite_times.append(report_time)
    if qa_approval["status"] == "PASS":
        qa_time = timestamp_value(qa_approval["approved_at_utc"])
        if prerequisite_times and qa_time < max(prerequisite_times):
            raise VerificationError("qa_lead approval predates required evidence")
        prerequisite_times.append(qa_time)
    if release_approval["status"] == "PASS":
        release_time = timestamp_value(release_approval["approved_at_utc"])
        if qa_approval["status"] != "PASS" or release_time <= timestamp_value(
            qa_approval["approved_at_utc"]
        ):
            raise VerificationError("release-owner approval must follow QA approval")
        if prerequisite_times and release_time < max(prerequisite_times):
            raise VerificationError("release_owner approval predates required evidence")
    if report["release_date"] is not None and all(
        approval["status"] == "PASS" for approval in approvals
    ):
        release_day = _datetime.datetime.strptime(
            report["release_date"], "%Y-%m-%d"
        ).date()
        latest_approval_day = max(
            timestamp_value(approval["approved_at_utc"]).date()
            for approval in approvals
        )
        if release_day < latest_approval_day:
            raise VerificationError("release_date predates final approval")
        if release_day > _datetime.datetime.now(_datetime.timezone.utc).date():
            raise VerificationError("release_date must not be in the future")


def verify_release_status(root: Path, status_path: Path, allow_blocked: bool) -> List[str]:
    try:
        root = root.resolve(strict=True)
    except OSError as exc:
        raise VerificationError("project root is inaccessible: {}".format(exc))
    if not root.is_dir():
        raise VerificationError("project root is not a directory")
    try:
        if status_path.is_absolute():
            normalized_parent = status_path.parent.resolve(strict=True)
            status_relative = (normalized_parent / status_path.name).relative_to(root).as_posix()
        else:
            status_relative = status_path.as_posix()
    except (OSError, ValueError) as exc:
        raise VerificationError("status file is outside the project root: {}".format(exc))
    status_path = resolve_repository_file(root, status_relative, "status file")
    status = require_object(load_json_strict(status_path), "status")
    require_exact_keys(status, ROOT_KEYS, "status")
    if status["schema"] != "tanks3d-release-status-v1":
        raise VerificationError("unsupported release status schema")
    expected_requirements_path = "docs/release-requirements/macos-alpha-v1.json"
    if status["requirements"] != expected_requirements_path:
        raise VerificationError("status.requirements must name the canonical profile")
    requirements_path = resolve_repository_file(root, status["requirements"], "status.requirements")
    validate_requirements(load_json_strict(requirements_path))

    blockers: List[str] = []
    release, file_refs, candidate_dir = validate_release(root, status["release"])
    report = validate_report(status["report"], blockers)
    evidence_map, artifact_map = validate_evidence(root, status["evidence"], blockers)

    gameplay_definitions = [
        (
            gameplay_id,
            mode,
            [gameplay_id],
            ["one_player_gameplay" if mode == "one_player" else "two_player_gameplay"],
        )
        for gameplay_id in GAMEPLAY_IDS
        for mode in MODES
    ]
    validate_matrix(
        status["gameplay"],
        "status.gameplay",
        gameplay_definitions,
        evidence_map,
        blockers,
    )
    base_definitions = [
        (base_id, mode, BASE_CHECKS, ["national_bases"])
        for base_id in BASE_IDS
        for mode in MODES
    ]
    validate_matrix(
        status["bases"], "status.bases", base_definitions, evidence_map, blockers
    )
    pickup_definitions = [
        (definition["id"], mode, definition["checks"], ["pickup_and_minimap"])
        for definition in PICKUP_REQUIREMENTS
        for mode in MODES
    ]
    validate_matrix(
        status["pickups"],
        "status.pickups",
        pickup_definitions,
        evidence_map,
        blockers,
    )
    settlement_definitions = [
        (settlement_id, None, [settlement_id], ["settlement_report"])
        for settlement_id in SETTLEMENT_IDS
    ]
    validate_matrix(
        status["settlement"],
        "status.settlement",
        settlement_definitions,
        evidence_map,
        blockers,
    )
    validate_result_record(
        status["published_controls"],
        "status.published_controls",
        "published_controls_match",
        None,
        PUBLISHED_CONTROL_CHECKS,
        [
            "main_menu_and_advanced_settings",
            "one_player_gameplay",
            "two_player_gameplay",
        ],
        evidence_map,
        blockers,
    )
    validate_gate(
        status["clean_mac"],
        "status.clean_mac",
        CLEAN_MAC_DETAIL_KEYS,
        ["main_menu_and_advanced_settings", "gatekeeper_launch"],
        evidence_map,
        blockers,
    )
    validate_gate(
        status["gatekeeper"],
        "status.gatekeeper",
        GATEKEEPER_DETAIL_KEYS,
        ["gatekeeper_launch"],
        evidence_map,
        blockers,
    )
    validate_gate(
        status["extended_session"],
        "status.extended_session",
        EXTENDED_SESSION_DETAIL_KEYS,
        ["extended_session_metrics"],
        evidence_map,
        blockers,
    )
    validate_pass_gate_semantics(
        status,
        release,
        file_refs["artifact"][0].name,
        file_refs["artifact"][1],
    )
    validate_interactive_coverage_matrix(status, evidence_map, file_refs["artifact"][1])
    validate_structured_interactive_evidence(
        status, evidence_map, artifact_map, file_refs["artifact"][1]
    )
    validate_clean_mac_command_log(
        status,
        evidence_map,
        artifact_map,
        file_refs["artifact"][0].name,
        file_refs["artifact"][1],
    )
    validate_performance_log(
        status, evidence_map, artifact_map, file_refs["artifact"][1]
    )
    known_issues = validate_known_issues(
        status["known_issues"], evidence_map, blockers
    )
    audio = validate_audio(root, status["audio"], file_refs["artifact"][0], blockers)
    approvals = validate_approvals(status["approvals"], evidence_map, blockers)
    validate_chronology_and_ownership(status, report, known_issues, audio, approvals)
    validate_documents(
        root,
        status["documents"],
        release,
        file_refs,
        artifact_map,
        evidence_map,
        not blockers,
        audio["decision"],
        known_issues,
        approvals,
        report["release_date"],
    )
    invoke_tagged_candidate_verifier(root, candidate_dir)

    if blockers and not allow_blocked:
        raise VerificationError(
            "release is not approved; {} blocker(s): {}".format(
                len(blockers), ", ".join(blockers)
            )
        )
    return blockers


def build_argument_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--allow-blocked",
        action="store_true",
        help="validate an incomplete status but return success after listing blockers",
    )
    parser.add_argument(
        "--project-root",
        default=".",
        help="repository root (default: current directory)",
    )
    parser.add_argument(
        "--status",
        default="docs/releases/v0.1.0-alpha.3-status.json",
        help="status JSON path, relative to the project root by default",
    )
    return parser


def main(argv: Optional[Sequence[str]] = None) -> int:
    arguments = build_argument_parser().parse_args(argv)
    root = Path(arguments.project_root)
    status = Path(arguments.status)
    try:
        blockers = verify_release_status(root, status, arguments.allow_blocked)
    except VerificationError as exc:
        print("release status verification failed: {}".format(exc), file=sys.stderr)
        return 1
    if blockers:
        print("Verified blocked Alpha status; release is NOT approved.")
        for blocker in blockers:
            print("BLOCKER: {}".format(blocker))
    else:
        print("Verified release status: all required Alpha gates PASS.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
