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
import ipaddress
import json
import math
import os
from pathlib import Path
import plistlib
import re
import shlex
import stat
import struct
import subprocess
import sys
from typing import Any, Dict, List, Mapping, Optional, Sequence, Set, Tuple
from urllib.parse import unquote, urlsplit
import zipfile
import zlib
import xml.etree.ElementTree as ET


SCRIPT_DIRECTORY = Path(__file__).resolve().parent
if str(SCRIPT_DIRECTORY) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIRECTORY))

import release_performance_contract as performance_contract  # noqa: E402
import validate_media_recording as recording_validator  # noqa: E402


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
ADVANCED_SETTINGS_CHECKS = [
    "default_hp_3_and_reset_restores_defaults",
    "player_hp_range_1_to_6_step_1",
    "enemy_speed_range_minus_30_to_plus_30_step_5",
    "fire_frequency_range_minus_30_to_plus_30_step_5",
    "spawn_pace_range_minus_30_to_plus_30_step_5",
    "selected_tuning_applies_after_start_and_restart",
    "hp_1_disables_bandage_and_normal_hp_restores_it",
    "escape_preserves_selected_values",
]
PUBLISHED_CONTROL_CONTEXT_REQUIREMENTS = [
    {
        "context": "main_menu",
        "coverage_token": "controls:main_menu",
        "evidence_id": "main_menu_and_advanced_settings",
        "checks": [
            "menu_arrow_or_wasd_navigation",
            "menu_enter_or_space_confirm",
            "q_or_escape_exits_from_setup",
            "default_hp_3_and_reset_restores_defaults",
            "player_hp_range_1_to_6_step_1",
            "enemy_speed_range_minus_30_to_plus_30_step_5",
            "fire_frequency_range_minus_30_to_plus_30_step_5",
            "spawn_pace_range_minus_30_to_plus_30_step_5",
            "escape_preserves_selected_values",
        ],
    },
    {
        "context": "one_player",
        "coverage_token": "controls:one_player",
        "evidence_id": "one_player_gameplay",
        "checks": [
            "player_1_arrow_movement",
            "player_1_fire_bindings",
            "enter_pause_resume",
            "escape_returns_to_setup",
            "r_restarts_stage",
            "f8_quality_toggle",
            "f11_borderless_toggle",
            "n_b_stage_navigation",
            "selected_tuning_applies_after_start_and_restart",
            "hp_1_disables_bandage_and_normal_hp_restores_it",
        ],
    },
    {
        "context": "two_player",
        "coverage_token": "controls:two_player",
        "evidence_id": "two_player_gameplay",
        "checks": [
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
            "selected_tuning_applies_after_start_and_restart",
            "hp_1_disables_bandage_and_normal_hp_restores_it",
        ],
    },
]
CONTROL_EVIDENCE_IDS = [
    item["evidence_id"] for item in PUBLISHED_CONTROL_CONTEXT_REQUIREMENTS
]
V1_CLEAN_MAC_EVIDENCE_IDS = [
    "main_menu_and_advanced_settings",
    "gatekeeper_launch",
]
V2_CLEAN_MAC_EVIDENCE_IDS = ["gatekeeper_launch"]
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
GAMEPLAY_EVENT_LOG_V2_SCHEMA = "tanks3d-gameplay-event-log-v2"
GAMEPLAY_EVENT_LOG_V2_PRODUCER = "Tanks3D Alpha QA Evidence Compiler"
GAMEPLAY_EVENT_LOG_V2_KEYS = [
    "schema",
    "producer",
    "observation_manifest_sha256",
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
OBSERVATION_MANIFEST_SCHEMA = (
    "tanks3d-alpha-v2-interactive-observation-manifest-v1"
)
OBSERVATION_MANIFEST_KEYS = [
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
]
OBSERVATION_KEYS = [
    "coverage_token",
    "evidence_id",
    "required_checks",
    "result",
    "observed_at_utc",
    "checks_confirmed",
    "notes",
]
V1_GAMEPLAY_EVIDENCE_IDS = [
    "one_player_gameplay",
    "two_player_gameplay",
    "national_bases",
    "pickup_and_minimap",
    "settlement_report",
]
OBSERVATION_EVIDENCE_IDS = [
    "main_menu_and_advanced_settings",
] + V1_GAMEPLAY_EVIDENCE_IDS
SUPPORTING_ARTIFACT_GROUP_KEYS = ["evidence_id", "artifacts"]
SUPPORTING_ARTIFACT_KEYS = ["path", "kind"]
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
V1_CLEAN_MAC_DOWNLOAD_CLIENT = "curl"
V1_COMMAND_LOG_COMMAND_IDS = [
    "download",
    "checksum",
    "zip_quarantine",
    "app_quarantine",
    "codesign",
    "spctl",
]
V2_CLEAN_MAC_DOWNLOAD_CLIENT = "Safari"
V2_COMMAND_LOG_COMMAND_IDS = [
    "checksum",
    "zip_quarantine",
    "app_quarantine",
    "codesign",
    "spctl",
]
BROWSER_ACQUISITION_SCHEMA = "tanks3d-browser-acquisition-v1"
BROWSER_ACQUISITION_KEYS = [
    "schema",
    "candidate_sha256",
    "tester",
    "machine",
    "client",
    "url",
    "filename",
    "started_at_utc",
    "completed_at_utc",
    "zip_quarantine_agent",
    "signature",
]
CLEAN_MAC_PLAN_SCHEMA = "tanks3d-clean-mac-plan-v1"
CLEAN_MAC_INTAKE_SCHEMA = "tanks3d-clean-mac-intake-v1"
CLEAN_MAC_PLAN_KEYS = [
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
]
CLEAN_MAC_INTAKE_KEYS = [
    "schema",
    "plan_sha256",
    "session_nonce",
    "collector_sha256",
    "candidate_filename",
    "candidate_sha256",
    "download_url",
    "tester",
    "tester_signature",
    "machine",
    "machine_details",
    "session_started_at_utc",
    "session_completed_at_utc",
    "acquisition",
    "commands",
    "observations",
    "notes",
    "complete",
    "test_mode",
]
CLEAN_MAC_MACHINE_DETAIL_KEYS = [
    "mac_model",
    "chip",
    "uname_machine",
    "ram",
    "macos_version",
    "macos_build",
    "clean_machine_method",
    "prior_app_absent",
    "prior_approval_absent",
    "minimum_macos_met",
    "source_checkout_absent",
    "homebrew_raylib_unused",
]
CLEAN_MAC_ACQUISITION_KEYS = [
    "client",
    "started_at_utc",
    "completed_at_utc",
    "zip_quarantine_agent",
    "quarantine_timestamp_utc",
    "where_froms_url",
    "where_froms_sha256",
]
CLEAN_MAC_OBSERVATION_KEYS = [
    "first_finder_launch",
    "dialog_text",
    "documented_launch_path",
    "main_menu_reached",
    "signature_preserved",
    "conclusion",
]
CLEAN_MAC_COMPILER_RECEIPT_SCHEMA = (
    "tanks3d-clean-mac-compiler-receipt-v1"
)
CLEAN_MAC_COMPILER_RECEIPT_PRODUCER = (
    "Tanks3D Alpha Clean-Mac Evidence Compiler"
)
CLEAN_MAC_COMPILER_RECEIPT_KEYS = [
    "schema",
    "producer",
    "candidate_sha256",
    "session_nonce",
    "collector_sha256",
    "tester",
    "tester_signature",
    "reviewer",
    "reviewer_signature",
    "reviewed_at_utc",
    "review_notes",
    "files",
]
CLEAN_MAC_COMPILER_FILE_REFERENCE_KEYS = ["path", "sha256"]
CLEAN_MAC_COMPILER_FILE_IDS = [
    "plan",
    "intake",
    "where_froms",
    "checksum_stdout",
    "checksum_stderr",
    "zip_quarantine_stdout",
    "zip_quarantine_stderr",
    "app_quarantine_stdout",
    "app_quarantine_stderr",
    "codesign_stdout",
    "codesign_stderr",
    "spctl_stdout",
    "spctl_stderr",
    "browser_acquisition",
    "command_log",
    "media",
]
CLEAN_MAC_SYSTEM_COMMAND_PATHS = {
    "shasum": "/usr/bin/shasum",
    "xattr": "/usr/bin/xattr",
    "codesign": "/usr/bin/codesign",
    "spctl": "/usr/sbin/spctl",
}
CLEAN_MAC_COLLECTOR_SHA256 = (
    "0cfef08c5a016a7b36a9d46757710ec00aad4faa679dc2bb06e1b8d7dfd70b8d"
)
CLEAN_MAC_MEDIA_KIND = "recording"
CLEAN_MAC_MINIMUM_RECORDING_BYTES = 64 * 1024
CLEAN_MAC_MAXIMUM_RECORDING_BYTES = 95_000_000
CLEAN_MAC_RECORDING_EXTENSIONS = [".mov", ".mp4", ".m4v"]
PERFORMANCE_LOG_SCHEMA = "tanks3d-performance-log-v1"
PERFORMANCE_LOG_KEYS = [
    "schema",
    "candidate_sha256",
    "started_at_utc",
    "completed_at_utc",
    "samples",
]
PERFORMANCE_SAMPLE_KEYS = ["elapsed_seconds", "fps", "memory_mb"]
PERFORMANCE_LOG_V2_SCHEMA = performance_contract.PERFORMANCE_LOG_V2_SCHEMA
PERFORMANCE_LOG_V2_KEYS = list(performance_contract.PERFORMANCE_LOG_V2_KEYS)
PERFORMANCE_SAMPLE_V2_KEYS = list(
    performance_contract.PERFORMANCE_SAMPLE_V2_KEYS
)
PERFORMANCE_QA_RECEIPT_SCHEMA = "tanks3d-performance-qa-receipt-v1"
PERFORMANCE_QA_RECEIPT_KEYS = [
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
]
PERFORMANCE_QA_FILE_REFERENCE_KEYS = ["path", "sha256"]
PERFORMANCE_V2_APP_STATES = list(performance_contract.PERFORMANCE_V2_APP_STATES)
PERFORMANCE_V2_PRODUCER = performance_contract.PERFORMANCE_V2_PRODUCER
PERFORMANCE_V2_CLOCK = performance_contract.PERFORMANCE_V2_CLOCK
PERFORMANCE_V2_MEMORY_METRIC = performance_contract.PERFORMANCE_V2_MEMORY_METRIC
PERFORMANCE_V2_MEMORY_UNIT = performance_contract.PERFORMANCE_V2_MEMORY_UNIT
PERFORMANCE_V2_TARGET_INTERVAL_US = (
    performance_contract.PERFORMANCE_V2_TARGET_INTERVAL_US
)
PERFORMANCE_V2_MINIMUM_WINDOW_US = (
    performance_contract.PERFORMANCE_V2_MINIMUM_WINDOW_US
)
PERFORMANCE_V2_MAXIMUM_WINDOW_US = (
    performance_contract.PERFORMANCE_V2_MAXIMUM_WINDOW_US
)
PERFORMANCE_V2_MINIMUM_DURATION_US = 1_800_000_000
PERFORMANCE_V2_MAXIMUM_DURATION_SECONDS = (
    performance_contract.PERFORMANCE_V2_MAXIMUM_DURATION_SECONDS
)
PERFORMANCE_V2_MAXIMUM_MEMORY_GROWTH_BYTES = 256 * 1024 * 1024
PERFORMANCE_V2_MINIMUM_GAMEPLAY_DURATION_RATIO = 0.80
PERFORMANCE_V2_MINIMUM_FOCUSED_DURATION_RATIO = 0.95
PERFORMANCE_V2_EXECUTABLE_MEMBER = (
    "Tanks3D.app/Contents/MacOS/Tanks3D"
)
PERFORMANCE_V2_FILENAMES = {
    "receipt": "performance-qa-receipt.json",
    "telemetry": "performance-log-v2.json",
    "stdout": "performance-stdout.log",
    "stderr": "performance-stderr.log",
}
PERFORMANCE_V2_ARTIFACT_MAXIMUM_BYTES = dict(
    performance_contract.PERFORMANCE_ARTIFACT_MAXIMUM_BYTES
)
PERFORMANCE_V2_LIMITS_BY_FILENAME = {
    PERFORMANCE_V2_FILENAMES[role]: maximum
    for role, maximum in PERFORMANCE_V2_ARTIFACT_MAXIMUM_BYTES.items()
}
PERFORMANCE_V2_START_MARKER = performance_contract.PERFORMANCE_V2_START_MARKER
PERFORMANCE_V2_COMPLETE_MARKER = performance_contract.PERFORMANCE_V2_COMPLETE_MARKER
CURRENT_V2_PERFORMANCE_BUILD_CONFIG = {
    "performance-capability-schema":
        "tanks3d-release-performance-capabilities-v1",
    "performance-telemetry-schema": "tanks3d-performance-log-v2",
    "performance-capability-contract-sha256":
        "5137950da46fa11ee6d5ff60fafe67e83c4c0aacfb5fc83f2b0ce5f74afcfe1c",
}
CURRENT_V2_CANDIDATE_ATTESTATION_SCHEMA = "tanks3d-alpha-candidate-v3"
PERFORMANCE_THRESHOLDS_V2 = {
    "minimum_duration_minutes": 30,
    "minimum_stages_completed": 1,
    "minimum_average_fps": 50,
    "minimum_one_percent_low_fps": 30,
    "target_interval_us": PERFORMANCE_V2_TARGET_INTERVAL_US,
    "minimum_window_duration_us": PERFORMANCE_V2_MINIMUM_WINDOW_US,
    "maximum_window_duration_us": PERFORMANCE_V2_MAXIMUM_WINDOW_US,
    "maximum_memory_growth_bytes": PERFORMANCE_V2_MAXIMUM_MEMORY_GROWTH_BYTES,
    "maximum_memory_growth_mb": 256,
    "minimum_gameplay_duration_ratio": (
        PERFORMANCE_V2_MINIMUM_GAMEPLAY_DURATION_RATIO
    ),
    "minimum_focused_duration_ratio": (
        PERFORMANCE_V2_MINIMUM_FOCUSED_DURATION_RATIO
    ),
}
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

CANONICAL_REQUIREMENTS_V1: Dict[str, Any] = {
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

CANONICAL_REQUIREMENTS_V2: Dict[str, Any] = dict(CANONICAL_REQUIREMENTS_V1)
CANONICAL_REQUIREMENTS_V2.update(
    {
        "schema": "tanks3d-release-requirements-v2",
        "profile": "macos-alpha-v2",
        "advanced_settings_checks": ADVANCED_SETTINGS_CHECKS,
        "published_control_context_requirements": (
            PUBLISHED_CONTROL_CONTEXT_REQUIREMENTS
        ),
        "clean_mac_evidence_ids": V2_CLEAN_MAC_EVIDENCE_IDS,
        "interactive_observation_manifest_schema": OBSERVATION_MANIFEST_SCHEMA,
        "interactive_observation_manifest_keys": OBSERVATION_MANIFEST_KEYS,
        "interactive_observation_keys": OBSERVATION_KEYS,
        "interactive_observation_evidence_ids": OBSERVATION_EVIDENCE_IDS,
        "interactive_supporting_artifact_group_keys": SUPPORTING_ARTIFACT_GROUP_KEYS,
        "interactive_supporting_artifact_keys": SUPPORTING_ARTIFACT_KEYS,
        "gameplay_event_log_schema": GAMEPLAY_EVENT_LOG_V2_SCHEMA,
        "gameplay_event_log_producer": GAMEPLAY_EVENT_LOG_V2_PRODUCER,
        "gameplay_event_log_keys": GAMEPLAY_EVENT_LOG_V2_KEYS,
        "clean_mac_download_client": V2_CLEAN_MAC_DOWNLOAD_CLIENT,
        "browser_acquisition_schema": BROWSER_ACQUISITION_SCHEMA,
        "browser_acquisition_keys": BROWSER_ACQUISITION_KEYS,
        "clean_mac_plan_schema": CLEAN_MAC_PLAN_SCHEMA,
        "clean_mac_intake_schema": CLEAN_MAC_INTAKE_SCHEMA,
        "clean_mac_plan_keys": CLEAN_MAC_PLAN_KEYS,
        "clean_mac_intake_keys": CLEAN_MAC_INTAKE_KEYS,
        "clean_mac_machine_detail_keys": CLEAN_MAC_MACHINE_DETAIL_KEYS,
        "clean_mac_acquisition_keys": CLEAN_MAC_ACQUISITION_KEYS,
        "clean_mac_observation_keys": CLEAN_MAC_OBSERVATION_KEYS,
        "clean_mac_compiler_receipt_schema": (
            CLEAN_MAC_COMPILER_RECEIPT_SCHEMA
        ),
        "clean_mac_compiler_receipt_producer": (
            CLEAN_MAC_COMPILER_RECEIPT_PRODUCER
        ),
        "clean_mac_compiler_receipt_keys": CLEAN_MAC_COMPILER_RECEIPT_KEYS,
        "clean_mac_compiler_file_reference_keys": (
            CLEAN_MAC_COMPILER_FILE_REFERENCE_KEYS
        ),
        "clean_mac_compiler_file_ids": CLEAN_MAC_COMPILER_FILE_IDS,
        "clean_mac_system_command_paths": CLEAN_MAC_SYSTEM_COMMAND_PATHS,
        "clean_mac_collector_sha256": CLEAN_MAC_COLLECTOR_SHA256,
        "clean_mac_media_kind": CLEAN_MAC_MEDIA_KIND,
        "clean_mac_minimum_recording_bytes": (
            CLEAN_MAC_MINIMUM_RECORDING_BYTES
        ),
        "clean_mac_maximum_recording_bytes": (
            CLEAN_MAC_MAXIMUM_RECORDING_BYTES
        ),
        "clean_mac_recording_extensions": CLEAN_MAC_RECORDING_EXTENSIONS,
        "command_log_command_ids": V2_COMMAND_LOG_COMMAND_IDS,
        "performance_thresholds": PERFORMANCE_THRESHOLDS_V2,
        "performance_log_schema": PERFORMANCE_LOG_V2_SCHEMA,
        "performance_log_keys": PERFORMANCE_LOG_V2_KEYS,
        "performance_sample_keys": PERFORMANCE_SAMPLE_V2_KEYS,
        "performance_qa_receipt_schema": PERFORMANCE_QA_RECEIPT_SCHEMA,
        "performance_qa_receipt_keys": PERFORMANCE_QA_RECEIPT_KEYS,
        "performance_qa_file_reference_keys":
            PERFORMANCE_QA_FILE_REFERENCE_KEYS,
        "performance_app_states": PERFORMANCE_V2_APP_STATES,
        "performance_producer": PERFORMANCE_V2_PRODUCER,
        "performance_clock": PERFORMANCE_V2_CLOCK,
        "performance_memory_metric": PERFORMANCE_V2_MEMORY_METRIC,
        "performance_memory_unit": PERFORMANCE_V2_MEMORY_UNIT,
        "performance_executable_member": PERFORMANCE_V2_EXECUTABLE_MEMBER,
        "performance_filenames": PERFORMANCE_V2_FILENAMES,
        "performance_artifact_maximum_bytes": (
            PERFORMANCE_V2_ARTIFACT_MAXIMUM_BYTES
        ),
        "performance_stdout_markers": {
            "start": PERFORMANCE_V2_START_MARKER,
            "complete": PERFORMANCE_V2_COMPLETE_MARKER,
        },
    }
)

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
V2_QUARANTINE_RE = re.compile(
    r"^[0-9A-Fa-f]{4};([0-9A-Fa-f]+);([^;\r\n]+);[^\r\n]*$"
)
CLEAN_MAC_PLIST_DOCTYPE = (
    b'<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" '
    b'"http://www.apple.com/DTDs/PropertyList-1.0.dtd">'
)
MAXIMUM_CLEAN_MAC_PLIST_BYTES = 4 * 1024 * 1024
MAXIMUM_TEXT_EVIDENCE_BYTES = 32 * 1024 * 1024
MAXIMUM_PNG_EVIDENCE_BYTES = 32 * 1024 * 1024
LEGACY_NUMERIC_HOST_RE = re.compile(
    r"^(?:0[xX][0-9A-Fa-f]+|[0-9]+)(?:\.(?:0[xX][0-9A-Fa-f]+|[0-9]+))*$"
)
DNS_LABEL_RE = re.compile(
    r"^[A-Za-z0-9](?:[A-Za-z0-9-]{0,61}[A-Za-z0-9])?$"
)
PNG_KINDS = {"png"}
ARTIFACT_KINDS = {"png", "log", "recording", "report"}
MINIMUM_RECORDING_BYTES = 64 * 1024
MAXIMUM_RECORDING_BYTES = CLEAN_MAC_MAXIMUM_RECORDING_BYTES


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


def validate_requirements(requirements: Any, profile: str) -> None:
    canonical = {
        "macos-alpha-v1": CANONICAL_REQUIREMENTS_V1,
        "macos-alpha-v2": CANONICAL_REQUIREMENTS_V2,
    }.get(profile)
    if canonical is None:
        raise VerificationError("unsupported release requirements profile")
    compare_canonical(requirements, canonical, "requirements")


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
            raise VerificationError(
                "{} contradicts PASS with {!r}".format(context, phrase)
            )


def reject_audio_acceptance_conflict(value: Any, context: str) -> None:
    text = require_string(value, context).strip().lower()
    conflicts = (
        r"\b(?:do|does|did|will)\s+not\s+accept\b",
        r"\b(?:cannot|can't|won't)\s+accept\b",
        r"\b(?:reject|rejects|rejected|decline|declines|declined)\s+"
        r"(?:this\s+)?(?:release|candidate|audio|risk|decision)\b",
        r"\b(?:release|candidate|audio|risk)\s+(?:is|was)\s+not\s+accepted\b",
    )
    if any(re.search(pattern, text) for pattern in conflicts):
        raise VerificationError("{} contradicts the ACCEPT decision".format(context))


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    try:
        with path.open("rb") as stream:
            for chunk in iter(lambda: stream.read(1024 * 1024), b""):
                digest.update(chunk)
    except OSError as exc:
        raise VerificationError("cannot hash {}: {}".format(path, exc))
    return digest.hexdigest()


def open_repository_directory_no_follow(root: Path, path: Path, context: str) -> int:
    try:
        parts = path.relative_to(root).parts
    except ValueError:
        raise VerificationError("{} is outside the project root".format(context))
    flags = os.O_RDONLY | getattr(os, "O_DIRECTORY", 0)
    no_follow = getattr(os, "O_NOFOLLOW", None)
    if no_follow is None:
        raise VerificationError("O_NOFOLLOW is required to open {}".format(context))
    descriptor = -1
    try:
        descriptor = os.open(root, flags | no_follow)
        for part in parts:
            next_descriptor = os.open(
                part, flags | no_follow, dir_fd=descriptor
            )
            os.close(descriptor)
            descriptor = next_descriptor
        if not stat.S_ISDIR(os.fstat(descriptor).st_mode):
            raise VerificationError("{} must be a directory".format(context))
        return descriptor
    except VerificationError:
        if descriptor >= 0:
            os.close(descriptor)
        raise
    except OSError as exc:
        if descriptor >= 0:
            os.close(descriptor)
        raise VerificationError("cannot open {} safely: {}".format(context, exc))


def read_regular_file_no_follow(
    path: Path,
    context: str,
    directory_fd: Optional[int] = None,
    maximum_bytes: Optional[int] = None,
) -> bytes:
    flags = os.O_RDONLY | getattr(os, "O_NONBLOCK", 0)
    no_follow = getattr(os, "O_NOFOLLOW", None)
    if no_follow is None:
        raise VerificationError("O_NOFOLLOW is required to read {}".format(context))
    descriptor = -1
    try:
        descriptor = (
            os.open(path, flags | no_follow)
            if directory_fd is None
            else os.open(str(path), flags | no_follow, dir_fd=directory_fd)
        )
        before = os.fstat(descriptor)
        if not stat.S_ISREG(before.st_mode):
            raise VerificationError("{} must be a regular file".format(context))
        if maximum_bytes is not None and before.st_size > maximum_bytes:
            raise VerificationError(
                "{} exceeds its {}-byte release limit".format(
                    context, maximum_bytes
                )
            )
        chunks = []
        total_bytes = 0
        while True:
            chunk = os.read(descriptor, 1024 * 1024)
            if not chunk:
                break
            total_bytes += len(chunk)
            if maximum_bytes is not None and total_bytes > maximum_bytes:
                raise VerificationError(
                    "{} exceeds its {}-byte release limit".format(
                        context, maximum_bytes
                    )
                )
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
            raise VerificationError("{} changed while it was read".format(context))
        return data
    except VerificationError:
        raise
    except OSError as exc:
        raise VerificationError("cannot read {}: {}".format(context, exc))
    finally:
        if descriptor >= 0:
            os.close(descriptor)


def sha256_regular_file_no_follow(
    path: Path,
    context: str,
    maximum_bytes: int,
) -> str:
    flags = os.O_RDONLY | getattr(os, "O_NONBLOCK", 0)
    no_follow = getattr(os, "O_NOFOLLOW", None)
    if no_follow is None:
        raise VerificationError("O_NOFOLLOW is required to hash {}".format(context))
    descriptor = -1
    try:
        descriptor = os.open(path, flags | no_follow)
        before = os.fstat(descriptor)
        if not stat.S_ISREG(before.st_mode):
            raise VerificationError("{} must be a regular file".format(context))
        if before.st_size > maximum_bytes:
            raise VerificationError(
                "{} exceeds its {}-byte release limit".format(
                    context, maximum_bytes
                )
            )
        digest = hashlib.sha256()
        total_bytes = 0
        while True:
            chunk = os.read(descriptor, 1024 * 1024)
            if not chunk:
                break
            total_bytes += len(chunk)
            if total_bytes > maximum_bytes:
                raise VerificationError(
                    "{} exceeds its {}-byte release limit".format(
                        context, maximum_bytes
                    )
                )
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
            raise VerificationError("{} changed while it was hashed".format(context))
        return digest.hexdigest()
    except VerificationError:
        raise
    except OSError as exc:
        raise VerificationError("cannot hash {}: {}".format(context, exc))
    finally:
        if descriptor >= 0:
            os.close(descriptor)


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


def decode_png_stream(compressed: bytes, context: str, maximum_size: int) -> bytes:
    try:
        decoder = zlib.decompressobj()
        decoded = decoder.decompress(compressed, maximum_size + 1)
        if len(decoded) > maximum_size or decoder.unconsumed_tail:
            raise VerificationError(
                "{} exceeds the maximum decoded PNG size".format(context)
            )
        decoded += decoder.flush(maximum_size + 1 - len(decoded))
    except zlib.error as exc:
        raise VerificationError("{} has an invalid PNG image stream: {}".format(context, exc))
    if len(decoded) > maximum_size:
        raise VerificationError("{} exceeds the maximum decoded PNG size".format(context))
    if not decoder.eof or decoder.unused_data or decoder.unconsumed_tail:
        raise VerificationError("{} has a non-canonical PNG image stream".format(context))
    return decoded


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


def normalized_rgba_digest(
    decoded: bytes, width: int, height: int, context: str
) -> str:
    row_bytes = width * 4
    pixels = bytearray()
    previous = bytearray(row_bytes)
    offset = 0
    for _ in range(height):
        filter_type = decoded[offset]
        offset += 1
        current = bytearray(decoded[offset : offset + row_bytes])
        offset += row_bytes
        if filter_type > 4:
            raise VerificationError(
                "{} has an unsupported PNG row filter".format(context)
            )
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
                    current[index]
                    + paeth_predictor(left, above, upper_left)
                ) & 0xFF
        pixels.extend(current)
        previous = current
    return hashlib.sha256(bytes(pixels)).hexdigest()


def verify_png(
    path: Path, context: str, require_release_size: bool = False
) -> Optional[str]:
    try:
        data = path.read_bytes()
    except OSError as exc:
        raise VerificationError("cannot read {}: {}".format(context, exc))
    if not data.startswith(b"\x89PNG\r\n\x1a\n"):
        raise VerificationError("{} is not a PNG".format(context))
    offset = 8
    chunks: List[bytes] = []
    width = height = None
    image_format = None
    image_data = []
    idat_started = False
    idat_finished = False
    known_critical = {b"IHDR", b"PLTE", b"IDAT", b"IEND"}
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
        if chunk_type not in known_critical and not (chunk_type[0] & 0x20):
            raise VerificationError("{} has an unknown critical PNG chunk".format(context))
        chunks.append(chunk_type)
        if len(chunks) == 1:
            if chunk_type != b"IHDR" or length != 13:
                raise VerificationError("{} has no canonical IHDR".format(context))
            width, height = struct.unpack(">II", payload[:8])
            image_format = struct.unpack(">BBBBB", payload[8:13])
        elif chunk_type == b"IHDR":
            raise VerificationError("{} has more than one IHDR".format(context))
        if chunk_type == b"IDAT":
            if idat_finished:
                raise VerificationError("{} has non-contiguous IDAT chunks".format(context))
            idat_started = True
            image_data.append(payload)
        elif idat_started and chunk_type != b"IEND":
            idat_finished = True
        if chunk_type == b"IEND":
            if length != 0 or end != len(data):
                raise VerificationError("{} has an invalid IEND".format(context))
            offset = end
            break
        offset = end
    if not chunks or chunks[-1] != b"IEND" or not image_data:
        raise VerificationError("{} is an incomplete PNG".format(context))
    if require_release_size and (width, height) != (1280, 720):
        raise VerificationError(
            "{} must be 1280x720, got {}x{}".format(context, width, height)
        )
    if require_release_size:
        if image_format != (8, 6, 0, 0, 0):
            raise VerificationError(
                "{} must be 8-bit RGBA and non-interlaced".format(context)
            )
        row_bytes = width * 4
        expected_size = height * (row_bytes + 1)
        decoded = decode_png_stream(b"".join(image_data), context, expected_size)
        if len(decoded) != expected_size:
            raise VerificationError(
                "{} has an invalid decoded PNG size: expected {}, got {}".format(
                    context, expected_size, len(decoded)
                )
            )
        return normalized_rgba_digest(decoded, width, height, context)
    else:
        decode_png_stream(b"".join(image_data), context, 64 * 1024 * 1024)
    return None


def validate_artifact(
    root: Path,
    value: Any,
    context: str,
    maximum_bytes: Optional[int] = None,
) -> Tuple[str, str, str, Path]:
    artifact = require_object(value, context)
    require_exact_keys(artifact, EVIDENCE_ARTIFACT_KEYS, context)
    kind = require_string(artifact["kind"], "{}.kind".format(context))
    if kind not in ARTIFACT_KINDS:
        raise VerificationError("{}.kind is unsupported".format(context))
    if maximum_bytes is None and kind in {"log", "report"}:
        maximum_bytes = MAXIMUM_TEXT_EVIDENCE_BYTES
    elif maximum_bytes is None and kind == "recording":
        maximum_bytes = MAXIMUM_RECORDING_BYTES
    elif maximum_bytes is None and kind in PNG_KINDS:
        maximum_bytes = MAXIMUM_PNG_EVIDENCE_BYTES
    path = resolve_repository_file(root, artifact["path"], "{}.path".format(context))
    digest = require_sha256(artifact["sha256"], "{}.sha256".format(context))
    if maximum_bytes is None:
        actual = sha256_file(path)
    else:
        actual = sha256_regular_file_no_follow(path, context, maximum_bytes)
    if actual != digest:
        raise VerificationError("{} hash mismatch".format(context))
    if kind in PNG_KINDS:
        verify_png(path, context)
    elif kind == "recording":
        recording_size = path.stat().st_size
        if recording_size < MINIMUM_RECORDING_BYTES:
            raise VerificationError(
                "{} recording must be at least {} bytes".format(
                    context, MINIMUM_RECORDING_BYTES
                )
            )
        if recording_size > MAXIMUM_RECORDING_BYTES:
            raise VerificationError(
                "{} recording must be no larger than {} bytes".format(
                    context, MAXIMUM_RECORDING_BYTES
                )
            )
    return (
        require_string(artifact["path"], "{}.path".format(context)),
        digest,
        kind,
        path,
    )


def load_structured_artifact(
    path: Path,
    context: str,
    maximum_bytes: int = MAXIMUM_TEXT_EVIDENCE_BYTES,
    expected_sha256: Optional[str] = None,
) -> Optional[Mapping[str, Any]]:
    """Return a JSON object for structured evidence, or None for binary/text evidence."""
    if path.suffix.lower() != ".json":
        return None
    raw = read_regular_file_no_follow(
        path,
        context,
        maximum_bytes=maximum_bytes,
    )
    if (
        expected_sha256 is not None
        and hashlib.sha256(raw).hexdigest() != expected_sha256
    ):
        raise VerificationError("{} changed after evidence validation".format(context))
    stripped = raw.lstrip()
    if not stripped.startswith(b"{"):
        return None
    try:
        value = json.loads(raw.decode("utf-8"), object_pairs_hook=_reject_duplicate_pairs)
    except (UnicodeError, json.JSONDecodeError) as exc:
        raise VerificationError("{} is malformed structured JSON: {}".format(context, exc))
    return require_object(value, context)


def parse_clean_mac_plist_node(
    node: ET.Element, context: str, depth: int = 0
) -> Any:
    if depth > 16:
        raise VerificationError("{} is nested too deeply".format(context))
    if node.attrib or (node.tail is not None and node.tail.strip()):
        raise VerificationError(
            "{} contains unsupported XML attributes or text".format(context)
        )
    if node.tag == "string":
        if list(node):
            raise VerificationError("{} string has child elements".format(context))
        return node.text or ""
    if node.tag == "integer":
        if (
            list(node)
            or node.text is None
            or re.fullmatch(r"-?[0-9]+", node.text) is None
        ):
            raise VerificationError("{} has an invalid integer".format(context))
        return int(node.text)
    if node.tag in {"true", "false"}:
        if list(node) or (node.text is not None and node.text.strip()):
            raise VerificationError("{} has an invalid boolean".format(context))
        return node.tag == "true"
    if node.tag == "array":
        if node.text is not None and node.text.strip():
            raise VerificationError("{} array has unexpected text".format(context))
        return [
            parse_clean_mac_plist_node(
                child, "{}[{}]".format(context, index), depth + 1
            )
            for index, child in enumerate(list(node))
        ]
    if node.tag == "dict":
        if node.text is not None and node.text.strip():
            raise VerificationError(
                "{} dictionary has unexpected text".format(context)
            )
        children = list(node)
        if len(children) % 2:
            raise VerificationError(
                "{} dictionary is missing a value".format(context)
            )
        result: Dict[str, Any] = {}
        for index in range(0, len(children), 2):
            key_node = children[index]
            if key_node.tag != "key" or list(key_node) or key_node.attrib:
                raise VerificationError(
                    "{} dictionary has an invalid key".format(context)
                )
            key = key_node.text or ""
            if not key or key in result:
                raise VerificationError(
                    "{} dictionary has a duplicate or empty key".format(context)
                )
            result[key] = parse_clean_mac_plist_node(
                children[index + 1],
                "{}.{}".format(context, key),
                depth + 1,
            )
        return result
    raise VerificationError(
        "{} contains unsupported plist element {!r}".format(context, node.tag)
    )


def load_clean_mac_xml_plist(path: Path, context: str) -> Mapping[str, Any]:
    try:
        if path.stat().st_size > MAXIMUM_CLEAN_MAC_PLIST_BYTES:
            raise VerificationError("{} is unexpectedly large".format(context))
        data = path.read_bytes()
    except OSError as exc:
        raise VerificationError("cannot read {}: {}".format(context, exc))
    if b"<!ENTITY" in data.upper():
        raise VerificationError("{} contains an XML entity declaration".format(context))
    if re.findall(br"<!DOCTYPE[^>]*>", data) != [CLEAN_MAC_PLIST_DOCTYPE]:
        raise VerificationError(
            "{} does not use the canonical Apple plist declaration".format(context)
        )
    try:
        root = ET.fromstring(data)
    except ET.ParseError as exc:
        raise VerificationError("{} is not valid XML: {}".format(context, exc))
    if root.tag != "plist" or root.attrib != {"version": "1.0"}:
        raise VerificationError("{} does not use plist version 1.0".format(context))
    if root.text is not None and root.text.strip():
        raise VerificationError("{} has unexpected root text".format(context))
    children = list(root)
    if len(children) != 1:
        raise VerificationError(
            "{} must contain exactly one root value".format(context)
        )
    return require_object(
        parse_clean_mac_plist_node(children[0], context), context
    )


def validate_clean_mac_where_froms(path: Path, expected_url: str) -> None:
    try:
        if path.stat().st_size > 1024 * 1024:
            raise VerificationError("clean-Mac where-froms is unexpectedly large")
        text = path.read_text(encoding="ascii")
        raw = bytes.fromhex("".join(text.split()))
        values = plistlib.loads(raw)
    except (OSError, UnicodeError, ValueError, plistlib.InvalidFileException) as exc:
        raise VerificationError(
            "clean-Mac where-froms is not a valid encoded plist: {}".format(exc)
        )
    if (
        not isinstance(values, list)
        or not values
        or any(not isinstance(value, str) for value in values)
        or expected_url not in values
    ):
        raise VerificationError(
            "clean-Mac where-froms does not contain the exact download URL"
        )


def require_clean_mac_affirmative(value: Any, context: str) -> str:
    text = require_nonplaceholder(value, context).lower()
    if text not in {"yes", "true", "pass", "verified"}:
        raise VerificationError(
            "{} must explicitly record yes/true/PASS/verified".format(context)
        )
    return text


def validate_clean_mac_raw_records(
    status: Mapping[str, Any],
    receipt: Mapping[str, Any],
    files: Mapping[str, Any],
    artifact_map: Mapping[str, Tuple[str, str, str, Path]],
    artifact_name: str,
    artifact_sha256: str,
    acquisition: Mapping[str, Any],
    log: Mapping[str, Any],
    session: Mapping[str, Any],
    interactive: Mapping[str, Any],
) -> None:
    plan = load_clean_mac_xml_plist(
        artifact_map[files["plan"]["path"]][3], "clean-Mac plan"
    )
    intake = load_clean_mac_xml_plist(
        artifact_map[files["intake"]["path"]][3], "clean-Mac intake"
    )
    require_exact_keys(plan, set(CLEAN_MAC_PLAN_KEYS), "clean-Mac plan")
    require_exact_keys(intake, set(CLEAN_MAC_INTAKE_KEYS), "clean-Mac intake")
    if (
        plan["schema"] != CLEAN_MAC_PLAN_SCHEMA
        or plan["requirements_profile"] != "macos-alpha-v2"
    ):
        raise VerificationError("clean-Mac plan schema/profile is not canonical")
    release = require_object(status["release"], "status.release")
    clean = require_object(status["clean_mac"]["details"], "status.clean_mac.details")
    gate = require_object(status["gatekeeper"]["details"], "status.gatekeeper.details")
    plan_expectations = {
        "candidate_tag": release["tag"],
        "candidate_filename": artifact_name,
        "candidate_sha256": artifact_sha256,
        "download_url": clean["download_url"],
        "collector_sha256": CLEAN_MAC_COLLECTOR_SHA256,
        "session_nonce": receipt["session_nonce"],
    }
    for key, expected in plan_expectations.items():
        if plan[key] != expected:
            raise VerificationError(
                "clean-Mac plan {} is not candidate-bound".format(key)
            )
    deployment = re.search(r"-macos([0-9]+(?:\.[0-9]+)*)\.zip$", artifact_name)
    if deployment is None or plan["minimum_macos_version"] != deployment.group(1):
        raise VerificationError(
            "clean-Mac plan minimum macOS does not match the candidate"
        )
    prepared = timestamp_value(
        require_timestamp(plan["prepared_at_utc"], "clean-Mac plan.prepared_at_utc")
    )
    session_started = timestamp_value(
        require_timestamp(
            session["started_at_utc"], "clean-Mac session.started_at_utc"
        )
    )
    if prepared > session_started:
        raise VerificationError("clean-Mac plan was prepared after the session began")

    if intake["schema"] != CLEAN_MAC_INTAKE_SCHEMA:
        raise VerificationError("clean-Mac intake schema is not canonical")
    intake_expectations = {
        "plan_sha256": files["plan"]["sha256"],
        "session_nonce": receipt["session_nonce"],
        "collector_sha256": CLEAN_MAC_COLLECTOR_SHA256,
        "candidate_filename": artifact_name,
        "candidate_sha256": artifact_sha256,
        "download_url": plan["download_url"],
        "tester": interactive["tester"],
        "tester_signature": interactive["signature"],
        "machine": interactive["machine"],
        "session_started_at_utc": session["started_at_utc"],
        "session_completed_at_utc": session["completed_at_utc"],
    }
    for key, expected in intake_expectations.items():
        if intake[key] != expected:
            raise VerificationError(
                "clean-Mac intake {} does not match its evidence".format(key)
            )
    if intake["complete"] is not True or intake["test_mode"] is not False:
        raise VerificationError(
            "clean-Mac intake must be complete and must not be test mode"
        )

    machine_details = require_object(
        intake["machine_details"], "clean-Mac intake.machine_details"
    )
    require_exact_keys(
        machine_details,
        set(CLEAN_MAC_MACHINE_DETAIL_KEYS),
        "clean-Mac intake.machine_details",
    )
    for key in CLEAN_MAC_MACHINE_DETAIL_KEYS:
        value = require_nonplaceholder(
            machine_details[key], "clean-Mac intake.machine_details.{}".format(key)
        )
        if clean[key] != value:
            raise VerificationError(
                "clean-Mac machine detail {} contradicts status".format(key)
            )
    for key in (
        "prior_app_absent",
        "prior_approval_absent",
        "minimum_macos_met",
        "source_checkout_absent",
        "homebrew_raylib_unused",
    ):
        require_clean_mac_affirmative(
            machine_details[key], "clean-Mac intake.machine_details.{}".format(key)
        )

    raw_acquisition = require_object(
        intake["acquisition"], "clean-Mac intake.acquisition"
    )
    require_exact_keys(
        raw_acquisition,
        set(CLEAN_MAC_ACQUISITION_KEYS),
        "clean-Mac intake.acquisition",
    )
    acquisition_expectations = {
        "client": acquisition["client"],
        "started_at_utc": acquisition["started_at_utc"],
        "completed_at_utc": acquisition["completed_at_utc"],
        "zip_quarantine_agent": acquisition["zip_quarantine_agent"],
        "where_froms_url": plan["download_url"],
        "where_froms_sha256": files["where_froms"]["sha256"],
    }
    for key, expected in acquisition_expectations.items():
        if raw_acquisition[key] != expected:
            raise VerificationError(
                "clean-Mac intake acquisition {} is not exact".format(key)
            )
    require_timestamp(
        raw_acquisition["quarantine_timestamp_utc"],
        "clean-Mac intake.acquisition.quarantine_timestamp_utc",
    )
    validate_clean_mac_where_froms(
        artifact_map[files["where_froms"]["path"]][3], plan["download_url"]
    )

    raw_commands = require_array(intake["commands"], "clean-Mac intake.commands")
    if raw_commands != log["commands"]:
        raise VerificationError(
            "clean-Mac intake commands do not exactly match the command log"
        )
    zip_command = require_object(raw_commands[1], "clean-Mac intake.commands[1]")
    quarantine = V2_QUARANTINE_RE.fullmatch(
        require_string(zip_command["stdout"], "clean-Mac ZIP quarantine stdout").strip()
    )
    if quarantine is None:
        raise VerificationError("clean-Mac ZIP quarantine output is not canonical")
    try:
        quarantine_time = _datetime.datetime.fromtimestamp(
            int(quarantine.group(1), 16), tz=_datetime.timezone.utc
        ).strftime("%Y-%m-%dT%H:%M:%SZ")
    except (ValueError, OverflowError, OSError):
        raise VerificationError("clean-Mac ZIP quarantine timestamp is invalid")
    if raw_acquisition["quarantine_timestamp_utc"] != quarantine_time:
        raise VerificationError(
            "clean-Mac intake quarantine timestamp is not derived from xattr"
        )

    observations = require_object(
        intake["observations"], "clean-Mac intake.observations"
    )
    require_exact_keys(
        observations,
        set(CLEAN_MAC_OBSERVATION_KEYS),
        "clean-Mac intake.observations",
    )
    for key in CLEAN_MAC_OBSERVATION_KEYS:
        if observations[key] != gate[key]:
            raise VerificationError(
                "clean-Mac intake observation {} contradicts status".format(key)
            )
    for key in ("first_finder_launch", "dialog_text", "documented_launch_path"):
        require_nonplaceholder(
            observations[key], "clean-Mac intake.observations.{}".format(key)
        )
        reject_blocking_language(
            observations[key], "clean-Mac intake.observations.{}".format(key)
        )
    require_clean_mac_affirmative(
        observations["main_menu_reached"],
        "clean-Mac intake.observations.main_menu_reached",
    )
    require_clean_mac_affirmative(
        observations["signature_preserved"],
        "clean-Mac intake.observations.signature_preserved",
    )
    if observations["conclusion"] != "PASS":
        raise VerificationError("clean-Mac intake conclusion must be PASS")
    notes = require_nonplaceholder(intake["notes"], "clean-Mac intake.notes")
    reject_blocking_language(notes, "clean-Mac intake.notes")


def validate_clean_mac_recording(path: Path) -> None:
    """Require stable structural ISO-BMFF evidence, not padded arbitrary bytes."""

    try:
        recording_validator.validate_recording(
            path,
            CLEAN_MAC_MINIMUM_RECORDING_BYTES,
            CLEAN_MAC_MAXIMUM_RECORDING_BYTES,
        )
    except recording_validator.RecordingValidationError as exc:
        raise VerificationError(
            "clean-Mac launch recording is not structurally valid: {}".format(exc)
        )


def require_json_number(value: Any, context: str) -> float:
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        raise VerificationError("{} must be a JSON number".format(context))
    number = float(value)
    if not math.isfinite(number):
        raise VerificationError("{} must be finite".format(context))
    return number


def require_json_integer(value: Any, minimum: int, context: str) -> int:
    if isinstance(value, bool) or not isinstance(value, int):
        raise VerificationError("{} must be a raw JSON integer".format(context))
    if value < minimum:
        raise VerificationError("{} must be at least {}".format(context, minimum))
    return value


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
    root: Path,
    values: Any,
    blockers: List[str],
    requirements_profile: str,
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
            maximum_bytes = None
            if (
                requirements_profile == "macos-alpha-v2"
                and evidence_id == "extended_session_metrics"
                and isinstance(artifact, dict)
                and isinstance(artifact.get("path"), str)
            ):
                filename = Path(artifact["path"]).name
                maximum_bytes = PERFORMANCE_V2_LIMITS_BY_FILENAME.get(filename)
            normalized = validate_artifact(
                root,
                artifact,
                artifact_context,
                maximum_bytes=maximum_bytes,
            )
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
    coverage_tokens_by_evidence: Optional[Mapping[str, str]] = None,
) -> None:
    record = require_object(raw_record, context)
    if record["status"] != "PASS":
        return
    tester = require_string(record["tester"], "{}.tester".format(context))
    tested_at = require_timestamp(
        record["tested_at_utc"], "{}.tested_at_utc".format(context)
    )
    evidence_ids = require_array(record["evidence_ids"], "{}.evidence_ids".format(context))
    if coverage_tokens_by_evidence is not None and set(evidence_ids) != set(
        coverage_tokens_by_evidence
    ):
        raise VerificationError(
            "{} interactive coverage mapping is not exact".format(context)
        )
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
        expected_coverage_token = (
            coverage_tokens_by_evidence[evidence_id]
            if coverage_tokens_by_evidence is not None
            else coverage_token
        )
        if expected_coverage_token not in coverage:
            raise VerificationError(
                "{} interactive evidence does not cover {!r}".format(
                    context, expected_coverage_token
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


def dynamic_evidence_tokens(
    status: Mapping[str, Any], requirements_profile: str
) -> Dict[str, Set[str]]:
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
        if requirements_profile == "macos-alpha-v2":
            for context in PUBLISHED_CONTROL_CONTEXT_REQUIREMENTS:
                tokens[context["evidence_id"]].add(context["coverage_token"])
        else:
            for evidence_id in status["published_controls"]["evidence_ids"]:
                tokens[evidence_id].add("controls:published_controls_match")
    for gate_name in ("clean_mac", "gatekeeper", "extended_session"):
        if status[gate_name]["status"] == "PASS":
            for evidence_id in status[gate_name]["evidence_ids"]:
                tokens[evidence_id].add("gate:{}".format(gate_name))
    return tokens


def observation_token_plan() -> List[Dict[str, Any]]:
    plan: List[Dict[str, Any]] = []

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
        plan.append(
            {
                "section": section,
                "id": item_id,
                "mode": mode,
                "coverage_token": token,
                "evidence_id": evidence_id,
                "required_checks": list(checks),
            }
        )

    for gameplay_id in GAMEPLAY_IDS:
        for mode in MODES:
            add(
                "gameplay",
                gameplay_id,
                mode,
                (
                    "one_player_gameplay"
                    if mode == "one_player"
                    else "two_player_gameplay"
                ),
                [gameplay_id],
            )
    for base_id in BASE_IDS:
        for mode in MODES:
            add("base", base_id, mode, "national_bases", BASE_CHECKS)
    for pickup in PICKUP_REQUIREMENTS:
        for mode in MODES:
            add(
                "pickup",
                pickup["id"],
                mode,
                "pickup_and_minimap",
                pickup["checks"],
            )
    for settlement_id in SETTLEMENT_IDS:
        add(
            "settlement",
            settlement_id,
            None,
            "settlement_report",
            [settlement_id],
        )
    for control_context in PUBLISHED_CONTROL_CONTEXT_REQUIREMENTS:
        plan.append(
            {
                "section": "controls",
                "id": control_context["context"],
                "mode": None,
                "coverage_token": control_context["coverage_token"],
                "evidence_id": control_context["evidence_id"],
                "required_checks": list(control_context["checks"]),
            }
        )
    return plan


def observation_status_rows(status: Mapping[str, Any]) -> Dict[str, Mapping[str, Any]]:
    rows: Dict[str, Mapping[str, Any]] = {}
    definitions = (
        ("gameplay", "gameplay", lambda row: "gameplay:{}:{}".format(row["id"], row["mode"])),
        ("bases", "base", lambda row: "base:{}:{}".format(row["id"], row["mode"])),
        ("pickups", "pickup", lambda row: "pickup:{}:{}".format(row["id"], row["mode"])),
        ("settlement", "settlement", lambda row: "settlement:{}".format(row["id"])),
    )
    for status_key, _, token_builder in definitions:
        for raw_row in status[status_key]:
            row = require_object(raw_row, "status.{} observation row".format(status_key))
            token = token_builder(row)
            if token in rows:
                raise VerificationError("status contains duplicate observation token {!r}".format(token))
            rows[token] = row
    return rows


def validate_observation_manifest(
    value: Mapping[str, Any],
    context: str,
    candidate_sha256: str,
    evidence_map: Mapping[str, Mapping[str, Any]],
    artifact_map: Mapping[str, Tuple[str, str, str, Path]],
    status: Optional[Mapping[str, Any]] = None,
    evidence: Optional[Mapping[str, Any]] = None,
    interactive: Optional[Mapping[str, Any]] = None,
    session: Optional[Mapping[str, Any]] = None,
) -> List[Mapping[str, Any]]:
    require_exact_keys(value, set(OBSERVATION_MANIFEST_KEYS), context)
    expected_constants = {
        "schema": OBSERVATION_MANIFEST_SCHEMA,
        "requirements_profile": "macos-alpha-v2",
        "event_log_schema": GAMEPLAY_EVENT_LOG_V2_SCHEMA,
        "event_log_producer": GAMEPLAY_EVENT_LOG_V2_PRODUCER,
    }
    for key, expected in expected_constants.items():
        if value[key] != expected:
            raise VerificationError("{}.{} does not match the Alpha-v2 compiler contract".format(context, key))
    if require_sha256(value["candidate_sha256"], context + ".candidate_sha256") != candidate_sha256:
        raise VerificationError("{} candidate does not match the release artifact".format(context))

    tester = require_nonplaceholder(value["tester"], context + ".tester")
    reviewer = require_nonplaceholder(value["reviewer"], context + ".reviewer")
    for key in (
        "machine",
        "tester_signature",
        "reviewer_signature",
        "review_notes",
    ):
        require_nonplaceholder(value[key], "{}.{}".format(context, key))
    reject_blocking_language(value["review_notes"], context + ".review_notes")
    if tester.strip().casefold() == reviewer.strip().casefold():
        raise VerificationError("{} tester and reviewer must be different people".format(context))

    started_text = require_timestamp(value["started_at_utc"], context + ".started_at_utc")
    completed_text = require_timestamp(value["completed_at_utc"], context + ".completed_at_utc")
    reviewed_text = require_timestamp(value["reviewed_at_utc"], context + ".reviewed_at_utc")
    started = timestamp_value(started_text)
    completed = timestamp_value(completed_text)
    reviewed = timestamp_value(reviewed_text)
    if not started <= completed <= reviewed:
        raise VerificationError("{} timestamps are not chronological".format(context))

    if interactive is not None:
        identity_fields = {
            "candidate_sha256": "candidate_sha256",
            "tester": "tester",
            "machine": "machine",
            "tester_signature": "signature",
            "completed_at_utc": "tested_at_utc",
        }
        for manifest_key, interactive_key in identity_fields.items():
            if value[manifest_key] != interactive[interactive_key]:
                raise VerificationError(
                    "{}.{} does not match interactive evidence".format(
                        context, manifest_key
                    )
                )
    if evidence is not None:
        if value["reviewer"] != evidence["reviewer"]:
            raise VerificationError("{}.reviewer does not match evidence review".format(context))
        if value["reviewed_at_utc"] != evidence["reviewed_at_utc"]:
            raise VerificationError("{}.reviewed_at_utc does not match evidence review".format(context))
        if value["review_notes"] != evidence["notes"]:
            raise VerificationError("{}.review_notes does not match evidence notes".format(context))
    if session is not None:
        if (
            value["started_at_utc"] != session["started_at_utc"]
            or value["completed_at_utc"] != session["completed_at_utc"]
        ):
            raise VerificationError(
                "{} interval does not match its interactive session".format(context)
            )

    support_groups = require_array(
        value["supporting_artifacts"], context + ".supporting_artifacts"
    )
    if len(support_groups) != len(OBSERVATION_EVIDENCE_IDS):
        raise VerificationError(
            "{}.supporting_artifacts must contain the six canonical categories".format(context)
        )
    support_paths: Set[str] = set()
    for index, (raw_group, expected_id) in enumerate(
        zip(support_groups, OBSERVATION_EVIDENCE_IDS)
    ):
        group_context = "{}.supporting_artifacts[{}]".format(context, index)
        group = require_object(raw_group, group_context)
        require_exact_keys(group, set(SUPPORTING_ARTIFACT_GROUP_KEYS), group_context)
        if group["evidence_id"] != expected_id:
            raise VerificationError("{} category order is not canonical".format(group_context))
        artifacts = require_array(group["artifacts"], group_context + ".artifacts")
        if not artifacts:
            raise VerificationError("{} must not be empty".format(group_context))
        group_kinds: Set[str] = set()
        attached = {
            (item["path"], item["kind"])
            for item in evidence_map[expected_id]["artifacts"]
        }
        for artifact_index, raw_artifact in enumerate(artifacts):
            artifact_context = "{}.artifacts[{}]".format(group_context, artifact_index)
            artifact = require_object(raw_artifact, artifact_context)
            require_exact_keys(artifact, set(SUPPORTING_ARTIFACT_KEYS), artifact_context)
            path = require_nonplaceholder(artifact["path"], artifact_context + ".path")
            relative = Path(path)
            if (
                "\\" in path
                or relative.is_absolute()
                or any(part in {"", ".", ".."} for part in relative.parts)
                or relative.as_posix() != path
            ):
                raise VerificationError(
                    "{}.path must be a normalized repository-relative path".format(
                        artifact_context
                    )
                )
            kind = require_string(artifact["kind"], artifact_context + ".kind")
            if kind not in {"png", "recording"}:
                raise VerificationError("{}.kind must be png or recording".format(artifact_context))
            group_kinds.add(kind)
            if path in support_paths:
                raise VerificationError("{} duplicates supporting artifact path {!r}".format(context, path))
            support_paths.add(path)
            if status is not None:
                normalized = artifact_map.get(path)
                if normalized is None or normalized[2] != kind or (path, kind) not in attached:
                    raise VerificationError(
                        "{} is not attached to evidence.{} with matching metadata".format(
                            artifact_context, expected_id
                        )
                    )
        if expected_id in CONTROL_EVIDENCE_IDS and "recording" not in group_kinds:
            raise VerificationError(
                "{} requires a recording for controls verification".format(
                    group_context
                )
            )

    expected_plan = observation_token_plan()
    raw_observations = require_array(value["observations"], context + ".observations")
    if len(raw_observations) != len(expected_plan):
        raise VerificationError("{} observations do not exactly match the token plan".format(context))
    status_rows = observation_status_rows(status) if status is not None else {}
    observations: List[Mapping[str, Any]] = []
    control_observations: List[Mapping[str, Any]] = []
    for index, (raw_observation, expected) in enumerate(
        zip(raw_observations, expected_plan)
    ):
        observation_context = "{}.observations[{}]".format(context, index)
        observation = require_object(raw_observation, observation_context)
        require_exact_keys(observation, set(OBSERVATION_KEYS), observation_context)
        for key in ("coverage_token", "evidence_id", "required_checks"):
            if observation[key] != expected[key]:
                raise VerificationError(
                    "{} does not match the canonical token plan".format(
                        observation_context
                    )
                )
        if observation["result"] != "PASS":
            raise VerificationError("{}.result must be PASS".format(observation_context))
        observed_text = require_timestamp(
            observation["observed_at_utc"], observation_context + ".observed_at_utc"
        )
        observed = timestamp_value(observed_text)
        if not started <= observed <= completed:
            raise VerificationError("{} lies outside the observation session".format(observation_context))
        checks = validate_string_array(
            observation["checks_confirmed"], observation_context + ".checks_confirmed"
        )
        if checks != expected["required_checks"]:
            raise VerificationError("{} checks_confirmed are not exact".format(observation_context))
        require_nonplaceholder(observation["notes"], observation_context + ".notes")
        reject_blocking_language(observation["notes"], observation_context + ".notes")
        if status is not None and expected["section"] == "controls":
            control_observations.append(observation)
        elif status is not None:
            row = status_rows[expected["coverage_token"]]
            if (
                row["status"] != observation["result"]
                or row["tester"] != value["tester"]
                or row["tested_at_utc"] != value["completed_at_utc"]
                or row["evidence_ids"] != [expected["evidence_id"]]
                or row["checks_confirmed"] != observation["checks_confirmed"]
                or row["notes"] != observation["notes"]
            ):
                raise VerificationError(
                    "{} contradicts its release-status row".format(observation_context)
                )
        observations.append(observation)
    if status is not None:
        expected_control_tokens = [
            item["coverage_token"]
            for item in PUBLISHED_CONTROL_CONTEXT_REQUIREMENTS
        ]
        if [item["coverage_token"] for item in control_observations] != (
            expected_control_tokens
        ):
            raise VerificationError(
                "Alpha-v2 controls observations do not exactly match the token plan"
            )
        controls = require_object(
            status["published_controls"], "status.published_controls"
        )
        expected_evidence_ids = [
            item["evidence_id"]
            for item in PUBLISHED_CONTROL_CONTEXT_REQUIREMENTS
        ]
        expected_checks = PUBLISHED_CONTROL_CHECKS + ADVANCED_SETTINGS_CHECKS
        if (
            controls["status"] != "PASS"
            or controls["tester"] != value["tester"]
            or controls["tested_at_utc"] != value["completed_at_utc"]
            or controls["evidence_ids"] != expected_evidence_ids
            or controls["checks_confirmed"] != expected_checks
            or controls["notes"] != value["review_notes"]
        ):
            raise VerificationError(
                "Alpha-v2 controls observations contradict status.published_controls"
            )
    return observations


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


def validate_v2_gameplay_event_log(
    value: Mapping[str, Any],
    context: str,
    evidence_id: str,
    candidate_sha256: str,
    manifest: Mapping[str, Any],
    manifest_sha256: str,
    observations: Sequence[Mapping[str, Any]],
    interactive: Optional[Mapping[str, Any]] = None,
    expected_tokens: Optional[Set[str]] = None,
) -> None:
    require_exact_keys(value, set(GAMEPLAY_EVENT_LOG_V2_KEYS), context)
    if value["schema"] != GAMEPLAY_EVENT_LOG_V2_SCHEMA:
        raise VerificationError("{} does not use the Alpha-v2 event-log schema".format(context))
    if value["producer"] != GAMEPLAY_EVENT_LOG_V2_PRODUCER:
        raise VerificationError("{} was not produced by the Alpha-v2 evidence compiler".format(context))
    if require_sha256(
        value["observation_manifest_sha256"],
        context + ".observation_manifest_sha256",
    ) != manifest_sha256:
        raise VerificationError(
            "{} observation_manifest_sha256 does not match the attached manifest file".format(
                context
            )
        )
    if require_sha256(value["candidate_sha256"], context + ".candidate_sha256") != candidate_sha256:
        raise VerificationError("{} candidate does not match the release artifact".format(context))
    for key in ("tester", "machine", "started_at_utc", "completed_at_utc"):
        if value[key] != manifest[key]:
            raise VerificationError("{}.{} does not match the observation manifest".format(context, key))
    if interactive is not None:
        for key in ("candidate_sha256", "tester", "machine"):
            if value[key] != interactive[key]:
                raise VerificationError("{}.{} does not match interactive evidence".format(context, key))

    started = timestamp_value(
        require_timestamp(value["started_at_utc"], context + ".started_at_utc")
    )
    completed = timestamp_value(
        require_timestamp(value["completed_at_utc"], context + ".completed_at_utc")
    )
    if completed < started:
        raise VerificationError("{} ends before it starts".format(context))
    expected_observations = [
        observation
        for observation in observations
        if observation["evidence_id"] == evidence_id
    ]
    events = require_array(value["events"], context + ".events")
    if len(events) != len(expected_observations):
        raise VerificationError("{} event coverage is not exact".format(context))
    seen: Set[str] = set()
    for index, (raw_event, observation) in enumerate(
        zip(events, expected_observations)
    ):
        event_context = "{}.events[{}]".format(context, index)
        event = require_object(raw_event, event_context)
        require_exact_keys(event, set(GAMEPLAY_EVENT_KEYS), event_context)
        if event["sequence"] != index + 1:
            raise VerificationError("{}.sequence must be contiguous from 1".format(event_context))
        event_time = timestamp_value(
            require_timestamp(event["timestamp_utc"], event_context + ".timestamp_utc")
        )
        if not started <= event_time <= completed:
            raise VerificationError("{} lies outside the event-log session".format(event_context))
        if (
            event["timestamp_utc"] != observation["observed_at_utc"]
            or event["category_id"] != observation["evidence_id"]
            or event["coverage_token"] != observation["coverage_token"]
            or event["result"] != observation["result"]
        ):
            raise VerificationError("{} contradicts its observation manifest entry".format(event_context))
        token = require_nonplaceholder(
            event["coverage_token"], event_context + ".coverage_token"
        )
        if token in seen:
            raise VerificationError("{} duplicates coverage token {!r}".format(context, token))
        seen.add(token)
        if interactive is not None and interactive["coverage_refs"].get(token) != "event:{}".format(index + 1):
            raise VerificationError("{} is not bound to its event sequence".format(event_context))
    if interactive is not None:
        expected_refs = {
            observation["coverage_token"]: "event:{}".format(index + 1)
            for index, observation in enumerate(expected_observations)
        }
        if interactive["coverage_refs"] != expected_refs:
            raise VerificationError(
                "{} interactive coverage references are not exact".format(context)
            )
    if expected_tokens is not None and seen != expected_tokens:
        raise VerificationError("{} event coverage is not exact".format(context))


def validate_structured_interactive_evidence(
    status: Mapping[str, Any],
    evidence_map: Mapping[str, Mapping[str, Any]],
    artifact_map: Mapping[str, Tuple[str, str, str, Path]],
    candidate_sha256: str,
    requirements_profile: str,
) -> None:
    token_map = dynamic_evidence_tokens(status, requirements_profile)
    v1_gameplay_categories = set(V1_GAMEPLAY_EVIDENCE_IDS)
    v2_observation_categories = set(OBSERVATION_EVIDENCE_IDS)
    manifest_binding: Optional[Tuple[str, str]] = None
    for evidence_id, expected_tokens in token_map.items():
        needs_v2_draft_lint = (
            requirements_profile == "macos-alpha-v2"
            and evidence_id in v2_observation_categories
        )
        if not expected_tokens and not needs_v2_draft_lint:
            continue
        evidence = evidence_map[evidence_id]
        structured: List[
            Tuple[Tuple[str, str, str, Path], Mapping[str, Any]]
        ] = []
        for artifact in evidence["artifacts"]:
            normalized = artifact_map[artifact["path"]]
            parsed = load_structured_artifact(
                normalized[3],
                "evidence artifact " + normalized[0],
                expected_sha256=normalized[1],
            )
            if parsed is not None:
                structured.append((normalized, parsed))
            if needs_v2_draft_lint:
                basename = Path(normalized[0]).name
                expected_schema = None
                if basename == "observation-manifest.json":
                    expected_schema = OBSERVATION_MANIFEST_SCHEMA
                elif basename.endswith("-events.json"):
                    expected_schema = GAMEPLAY_EVENT_LOG_V2_SCHEMA
                if expected_schema is not None and (
                    parsed is None or parsed.get("schema") != expected_schema
                ):
                    raise VerificationError(
                        "evidence artifact {} does not contain the expected Alpha-v2 schema".format(
                            normalized[0]
                        )
                    )

        if needs_v2_draft_lint:
            manifests = [
                item
                for item in structured
                if item[1].get("schema") == OBSERVATION_MANIFEST_SCHEMA
            ]
            v2_event_logs = [
                item
                for item in structured
                if item[1].get("schema") == GAMEPLAY_EVENT_LOG_V2_SCHEMA
            ]
            if any(item[0][2] != "report" for item in manifests):
                raise VerificationError("Alpha-v2 observation manifest must be a report artifact")
            if any(item[0][2] != "log" for item in v2_event_logs):
                raise VerificationError("Alpha-v2 gameplay event log must be a log artifact")
            if not expected_tokens and (manifests or v2_event_logs):
                if len(manifests) > 1:
                    raise VerificationError(
                        "evidence.{} may attach at most one Alpha-v2 observation manifest".format(
                            evidence_id
                        )
                    )
                if len(v2_event_logs) > 1:
                    raise VerificationError(
                        "evidence.{} may attach at most one Alpha-v2 gameplay event log".format(
                            evidence_id
                        )
                    )
                if v2_event_logs and len(manifests) != 1:
                    raise VerificationError(
                        "Alpha-v2 gameplay event log requires its observation manifest"
                    )
                if manifests:
                    manifest_artifact, manifest = manifests[0]
                    observations = validate_observation_manifest(
                        manifest,
                        "Alpha-v2 observation manifest",
                        candidate_sha256,
                        evidence_map,
                        artifact_map,
                    )
                    if v2_event_logs:
                        validate_v2_gameplay_event_log(
                            v2_event_logs[0][1],
                            "Alpha-v2 gameplay event log",
                            evidence_id,
                            candidate_sha256,
                            manifest,
                            manifest_artifact[1],
                            observations,
                        )
        if not expected_tokens:
            continue

        interactive = require_object(evidence["interactive"], "interactive evidence")
        if interactive["tester"].strip().casefold() == evidence["reviewer"].strip().casefold():
            raise VerificationError(
                "interactive evidence tester and reviewer must be different people"
            )
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
        if (
            requirements_profile == "macos-alpha-v2"
            and evidence_id == "gatekeeper_launch"
        ):
            hashes = [
                require_sha256(
                    value,
                    "interactive session artifact_sha256s[{}]".format(index),
                )
                for index, value in enumerate(
                    require_array(
                        category["artifact_sha256s"],
                        "interactive session artifact_sha256s",
                    )
                )
            ]
        else:
            hashes = validate_string_array(
                category["artifact_sha256s"],
                "interactive session artifact_sha256s",
            )
        expected_hashes = [
            artifact["sha256"]
            for artifact in evidence["artifacts"]
            if artifact["path"] != session_artifact[0]
        ]
        if hashes != expected_hashes or not hashes:
            raise VerificationError("interactive session artifact hashes are not exact")
        if (
            evidence_id in v1_gameplay_categories
            and requirements_profile == "macos-alpha-v1"
        ):
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
        elif (
            evidence_id in v2_observation_categories
            and requirements_profile == "macos-alpha-v2"
        ):
            manifests = [
                item
                for item in structured
                if item[1].get("schema") == OBSERVATION_MANIFEST_SCHEMA
            ]
            event_logs = [
                item
                for item in structured
                if item[1].get("schema") == GAMEPLAY_EVENT_LOG_V2_SCHEMA
            ]
            if len(manifests) != 1:
                raise VerificationError(
                    "evidence.{} requires exactly one Alpha-v2 observation manifest report".format(
                        evidence_id
                    )
                )
            if len(event_logs) != 1:
                raise VerificationError(
                    "evidence.{} requires exactly one Alpha-v2 compiler event log".format(
                        evidence_id
                    )
                )
            manifest_artifact, manifest = manifests[0]
            binding = (manifest_artifact[0], manifest_artifact[1])
            if manifest_binding is None:
                manifest_binding = binding
            elif manifest_binding != binding:
                raise VerificationError(
                    "all Alpha-v2 observation evidence must share one observation manifest file"
                )
            observations = validate_observation_manifest(
                manifest,
                "Alpha-v2 observation manifest",
                candidate_sha256,
                evidence_map,
                artifact_map,
                status=status,
                evidence=evidence,
                interactive=interactive,
                session=session,
            )
            event_log = event_logs[0][1]
            if (
                event_log.get("started_at_utc") != session["started_at_utc"]
                or event_log.get("completed_at_utc") != session["completed_at_utc"]
            ):
                raise VerificationError(
                    "gameplay event log interval does not match its interactive session"
                )
            manifest_tokens = {
                observation["coverage_token"]
                for observation in observations
                if observation["evidence_id"] == evidence_id
            }
            if expected_tokens != manifest_tokens:
                raise VerificationError(
                    "evidence.{} dynamic coverage does not exactly match the "
                    "Alpha-v2 observation manifest".format(evidence_id)
                )
            validate_v2_gameplay_event_log(
                event_log,
                "Alpha-v2 gameplay event log",
                evidence_id,
                candidate_sha256,
                manifest,
                manifest_artifact[1],
                observations,
                interactive=interactive,
                expected_tokens=manifest_tokens,
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
        parsed = load_structured_artifact(
            normalized[3],
            "evidence artifact " + normalized[0],
            expected_sha256=normalized[1],
        )
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
    requirements_profile: str,
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
    if requirements_profile == "macos-alpha-v1":
        expected_client = V1_CLEAN_MAC_DOWNLOAD_CLIENT
        expected_ids = V1_COMMAND_LOG_COMMAND_IDS
        acquisition = None
    elif requirements_profile == "macos-alpha-v2":
        expected_client = V2_CLEAN_MAC_DOWNLOAD_CLIENT
        expected_ids = V2_COMMAND_LOG_COMMAND_IDS
        acquisitions = [
            value
            for value in values
            if value.get("schema") == BROWSER_ACQUISITION_SCHEMA
        ]
        if len(acquisitions) != 1:
            raise VerificationError(
                "v2 Clean-Mac PASS requires exactly one structured browser acquisition record"
            )
        acquisition = acquisitions[0]
        receipt_items: List[
            Tuple[Tuple[str, str, str, Path], Mapping[str, Any]]
        ] = []
        for artifact in evidence_map["gatekeeper_launch"]["artifacts"]:
            normalized = artifact_map[artifact["path"]]
            parsed = load_structured_artifact(
                normalized[3],
                "evidence artifact " + normalized[0],
                expected_sha256=normalized[1],
            )
            if (
                parsed is not None
                and parsed.get("schema") == CLEAN_MAC_COMPILER_RECEIPT_SCHEMA
            ):
                receipt_items.append((normalized, parsed))
        if len(receipt_items) != 1:
            raise VerificationError(
                "v2 Clean-Mac PASS requires exactly one compiler receipt"
            )
        receipt_artifact, receipt = receipt_items[0]
        if receipt_artifact[2] != "report":
            raise VerificationError("clean-Mac compiler receipt must be a report artifact")
        require_exact_keys(
            receipt,
            set(CLEAN_MAC_COMPILER_RECEIPT_KEYS),
            "clean-Mac compiler receipt",
        )
        if receipt["producer"] != CLEAN_MAC_COMPILER_RECEIPT_PRODUCER:
            raise VerificationError("clean-Mac compiler receipt producer is not canonical")
        if receipt["candidate_sha256"] != artifact_sha256:
            raise VerificationError("clean-Mac compiler receipt candidate does not match")
        expected_collector_sha256 = CANONICAL_REQUIREMENTS_V2.get(
            "clean_mac_collector_sha256"
        )
        if (
            not isinstance(expected_collector_sha256, str)
            or receipt["collector_sha256"] != expected_collector_sha256
        ):
            raise VerificationError(
                "clean-Mac compiler receipt does not bind the canonical collector"
            )
        nonce = require_string(
            receipt["session_nonce"], "clean-Mac compiler receipt.session_nonce"
        )
        if re.fullmatch(r"[0-9a-f]{32}", nonce) is None:
            raise VerificationError("clean-Mac compiler receipt session nonce is invalid")
        receipt_identity = {
            "tester": interactive["tester"],
            "tester_signature": interactive["signature"],
            "reviewer": evidence_map["gatekeeper_launch"]["reviewer"],
            "reviewed_at_utc": evidence_map["gatekeeper_launch"]["reviewed_at_utc"],
            "review_notes": evidence_map["gatekeeper_launch"]["notes"],
        }
        for key, expected in receipt_identity.items():
            if receipt[key] != expected:
                raise VerificationError(
                    "clean-Mac compiler receipt {} does not match evidence".format(
                        key
                    )
                )
        require_nonplaceholder(
            receipt["reviewer_signature"],
            "clean-Mac compiler receipt.reviewer_signature",
        )
        if receipt["reviewer_signature"] != receipt["reviewer"]:
            raise VerificationError(
                "clean-Mac compiler receipt reviewer signature is not exact"
            )
        files = require_object(
            receipt["files"], "clean-Mac compiler receipt.files"
        )
        require_exact_keys(
            files,
            set(CLEAN_MAC_COMPILER_FILE_IDS),
            "clean-Mac compiler receipt.files",
        )
        expected_file_kinds = {
            "plan": "report",
            "intake": "report",
            "where_froms": "log",
            "checksum_stdout": "log",
            "checksum_stderr": "log",
            "zip_quarantine_stdout": "log",
            "zip_quarantine_stderr": "log",
            "app_quarantine_stdout": "log",
            "app_quarantine_stderr": "log",
            "codesign_stdout": "log",
            "codesign_stderr": "log",
            "spctl_stdout": "log",
            "spctl_stderr": "log",
            "browser_acquisition": "report",
            "command_log": "log",
            "media": CLEAN_MAC_MEDIA_KIND,
        }
        gatekeeper_artifact_paths = {
            artifact["path"]
            for artifact in evidence_map["gatekeeper_launch"]["artifacts"]
        }
        seen_receipt_paths: Set[str] = set()
        for file_id in CLEAN_MAC_COMPILER_FILE_IDS:
            reference = require_object(
                files[file_id],
                "clean-Mac compiler receipt.files.{}".format(file_id),
            )
            require_exact_keys(
                reference,
                set(CLEAN_MAC_COMPILER_FILE_REFERENCE_KEYS),
                "clean-Mac compiler receipt.files.{}".format(file_id),
            )
            path = require_string(
                reference["path"],
                "clean-Mac compiler receipt.files.{}.path".format(file_id),
            )
            digest = require_sha256(
                reference["sha256"],
                "clean-Mac compiler receipt.files.{}.sha256".format(file_id),
            )
            if path in seen_receipt_paths:
                raise VerificationError("clean-Mac compiler receipt reuses a file path")
            seen_receipt_paths.add(path)
            attached = artifact_map.get(path)
            if (
                attached is None
                or attached[1] != digest
                or path not in gatekeeper_artifact_paths
            ):
                raise VerificationError(
                    "clean-Mac compiler receipt file is not attached to Gatekeeper evidence with its hash"
                )
            expected_kind = expected_file_kinds.get(file_id)
            if expected_kind is not None and attached[2] != expected_kind:
                raise VerificationError(
                    "clean-Mac compiler receipt file has the wrong artifact kind"
                )
        expected_basenames = {
            "plan": "clean-mac-plan.plist",
            "intake": "clean-mac-intake.plist",
            "where_froms": "where-froms.hex",
            "checksum_stdout": "checksum.stdout",
            "checksum_stderr": "checksum.stderr",
            "zip_quarantine_stdout": "zip-quarantine.stdout",
            "zip_quarantine_stderr": "zip-quarantine.stderr",
            "app_quarantine_stdout": "app-quarantine.stdout",
            "app_quarantine_stderr": "app-quarantine.stderr",
            "codesign_stdout": "codesign.stdout",
            "codesign_stderr": "codesign.stderr",
            "spctl_stdout": "spctl.stdout",
            "spctl_stderr": "spctl.stderr",
            "browser_acquisition": "browser-acquisition.json",
            "command_log": "command-log.json",
        }
        for file_id, basename in expected_basenames.items():
            if Path(files[file_id]["path"]).name != basename:
                raise VerificationError(
                    "clean-Mac compiler receipt uses a noncanonical filename"
                )
        media_basename = Path(files["media"]["path"]).name
        if re.fullmatch(
            r"gatekeeper-launch-recording\.[a-z0-9]{1,8}", media_basename
        ) is None:
            raise VerificationError(
                "clean-Mac compiler receipt uses a noncanonical recording filename"
            )
        validate_clean_mac_recording(
            artifact_map[files["media"]["path"]][3]
        )
        browser_artifact = artifact_map[files["browser_acquisition"]["path"]]
        command_artifact = artifact_map[files["command_log"]["path"]]
        browser_from_receipt = load_structured_artifact(
            browser_artifact[3],
            "clean-Mac compiler receipt browser acquisition",
            expected_sha256=browser_artifact[1],
        )
        command_from_receipt = load_structured_artifact(
            command_artifact[3],
            "clean-Mac compiler receipt command log",
            expected_sha256=command_artifact[1],
        )
        if browser_from_receipt != acquisition or command_from_receipt != log:
            raise VerificationError(
                "clean-Mac compiler receipt does not reference the validated records"
            )
    else:
        raise VerificationError("unsupported clean-Mac command-log profile")
    if len(commands) != len(expected_ids):
        raise VerificationError(
            "command log must contain the {} canonical commands".format(
                len(expected_ids)
            )
        )
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

    if requirements_profile == "macos-alpha-v2":
        for command_id in V2_COMMAND_LOG_COMMAND_IDS:
            for stream in ("stdout", "stderr"):
                file_id = "{}_{}".format(command_id, stream)
                transcript_path = artifact_map[files[file_id]["path"]][3]
                try:
                    if transcript_path.stat().st_size > 4 * 1024 * 1024:
                        raise VerificationError(
                            "clean-Mac raw command transcript is unexpectedly large"
                        )
                    raw_text = transcript_path.read_bytes().decode("utf-8")
                except UnicodeError as exc:
                    raise VerificationError(
                        "clean-Mac raw command transcript is not UTF-8: {}".format(
                            exc
                        )
                    )
                except OSError as exc:
                    raise VerificationError(
                        "cannot read clean-Mac raw command transcript: {}".format(
                            exc
                        )
                    )
                if raw_text != command_map[command_id][stream]:
                    raise VerificationError(
                        "clean-Mac raw {}.{} does not match the command log".format(
                            command_id, stream
                        )
                    )
        validate_clean_mac_raw_records(
            status,
            receipt,
            files,
            artifact_map,
            artifact_name,
            artifact_sha256,
            acquisition,
            log,
            session,
            interactive,
        )

    clean = status["clean_mac"]["details"]
    gate = status["gatekeeper"]["details"]
    url = clean["download_url"].strip()
    try:
        parsed_url = urlsplit(url)
        raw_host = (parsed_url.hostname or "").lower()
        parsed_port = parsed_url.port
    except ValueError:
        raise VerificationError(
            "status.clean_mac.details.download_url must be a non-test HTTPS candidate URL"
        )
    try:
        raw_host.encode("ascii")
        host_is_ascii = True
    except UnicodeEncodeError:
        host_is_ascii = False
    host = raw_host
    dns_labels = host.split(".")
    valid_dns_host = (
        len(host) <= 253
        and len(dns_labels) >= 2
        and all(DNS_LABEL_RE.fullmatch(label) is not None for label in dns_labels)
    )
    try:
        address_literal = ipaddress.ip_address(host)
    except ValueError:
        address_literal = None
    forbidden_hosts = {
        "example",
        "example.com",
        "example.org",
        "example.net",
        "localhost",
        "127.0.0.1",
        "::1",
    }
    forbidden_host_suffixes = (
        ".example",
        ".example.com",
        ".example.org",
        ".example.net",
        ".test",
        ".invalid",
        ".localhost",
        ".local",
        ".internal",
        ".lan",
        ".onion",
    )
    if (
        parsed_url.scheme != "https"
        or not host
        or not host_is_ascii
        or raw_host.endswith(".")
        or not valid_dns_host
        or parsed_port not in (None, 443)
        or address_literal is not None
        or LEGACY_NUMERIC_HOST_RE.fullmatch(host) is not None
        or host in forbidden_hosts
        or host.endswith(forbidden_host_suffixes)
        or any(token in host for token in ("fixture", "placeholder"))
        or parsed_url.username is not None
        or parsed_url.password is not None
        or parsed_url.query
        or parsed_url.fragment
    ):
        raise VerificationError("status.clean_mac.details.download_url must be a non-test HTTPS candidate URL")
    encoded_basename = Path(parsed_url.path).name
    if re.search(r"%(?![0-9A-Fa-f]{2})", encoded_basename):
        raise VerificationError(
            "status.clean_mac.details.download_url has malformed percent encoding"
        )
    try:
        decoded_basename = unquote(
            encoded_basename, encoding="utf-8", errors="strict"
        )
    except UnicodeError as exc:
        raise VerificationError(
            "status.clean_mac.details.download_url filename is not valid UTF-8: {}".format(
                exc
            )
        )
    if decoded_basename != artifact_name:
        raise VerificationError(
            "status.clean_mac.details.download_url must name the candidate artifact"
        )
    if clean["download_client"].strip() != expected_client:
        raise VerificationError(
            "status.clean_mac.details.download_client must be {} for the {} acquisition contract".format(
                expected_client, requirements_profile
            )
        )
    if acquisition is not None:
        require_exact_keys(
            acquisition,
            set(BROWSER_ACQUISITION_KEYS),
            "browser acquisition",
        )
        if acquisition["schema"] != BROWSER_ACQUISITION_SCHEMA:
            raise VerificationError("browser acquisition schema is not canonical")
        for key in ("candidate_sha256", "tester", "machine", "signature"):
            if acquisition[key] != interactive[key]:
                raise VerificationError(
                    "browser acquisition {} does not match interactive evidence".format(
                        key
                    )
                )
        expected_acquisition_values = {
            "client": expected_client,
            "url": url,
            "filename": artifact_name,
            "zip_quarantine_agent": expected_client,
        }
        for key, expected in expected_acquisition_values.items():
            if acquisition[key] != expected:
                raise VerificationError(
                    "browser acquisition {} is not candidate-bound".format(key)
                )
        acquisition_started = timestamp_value(
            require_timestamp(
                acquisition["started_at_utc"],
                "browser acquisition.started_at_utc",
            )
        )
        acquisition_completed = timestamp_value(
            require_timestamp(
                acquisition["completed_at_utc"],
                "browser acquisition.completed_at_utc",
            )
        )
        if not (
            session_started
            <= acquisition_started
            <= acquisition_completed
            <= session_completed
        ):
            raise VerificationError(
                "browser acquisition lies outside the Gatekeeper interactive session"
            )
        checksum_started = timestamp_value(
            command_map["checksum"]["started_at_utc"]
        )
        if acquisition_completed > checksum_started:
            raise VerificationError(
                "browser acquisition must complete before the checksum command"
            )
        for prefix in ("zip", "app"):
            quarantine = gate["{}_quarantine_output".format(prefix)].strip()
            quarantine_match = V2_QUARANTINE_RE.fullmatch(quarantine)
            if quarantine_match is None:
                raise VerificationError(
                    "{} quarantine output is not a canonical v2 quarantine record".format(
                        prefix
                    )
                )
            quarantine_agent = quarantine_match.group(2)
            if (
                prefix == "zip"
                and quarantine_agent != acquisition["zip_quarantine_agent"]
            ):
                raise VerificationError(
                    "ZIP quarantine agent does not match the Safari acquisition"
                )
            if prefix == "app" and quarantine_agent not in {
                "Safari",
                "Archive Utility",
                "Finder",
            }:
                raise VerificationError(
                    "app quarantine agent is not a supported Finder path"
                )
            if prefix == "zip":
                try:
                    quarantine_time = _datetime.datetime.fromtimestamp(
                        int(quarantine_match.group(1), 16),
                        tz=_datetime.timezone.utc,
                    )
                except (ValueError, OverflowError, OSError):
                    raise VerificationError(
                        "ZIP quarantine timestamp is not a valid epoch value"
                    )
                skew = _datetime.timedelta(seconds=5)
                if not (
                    acquisition_started - skew
                    <= quarantine_time
                    <= acquisition_completed + skew
                ):
                    raise VerificationError(
                        "ZIP quarantine timestamp lies outside the Safari acquisition"
                    )
    expected_argv = {
        "checksum": [
            CLEAN_MAC_SYSTEM_COMMAND_PATHS["shasum"],
            "-a",
            "256",
            artifact_name,
        ],
        "zip_quarantine": [
            CLEAN_MAC_SYSTEM_COMMAND_PATHS["xattr"],
            "-p",
            "com.apple.quarantine",
            artifact_name,
        ],
        "app_quarantine": [
            CLEAN_MAC_SYSTEM_COMMAND_PATHS["xattr"],
            "-p",
            "com.apple.quarantine",
            "Tanks3D.app",
        ],
        "codesign": [
            CLEAN_MAC_SYSTEM_COMMAND_PATHS["codesign"],
            "--verify",
            "--deep",
            "--strict",
            "--verbose=4",
            "Tanks3D.app",
        ],
        "spctl": [
            CLEAN_MAC_SYSTEM_COMMAND_PATHS["spctl"],
            "--assess",
            "--type",
            "execute",
            "--verbose=4",
            "Tanks3D.app",
        ],
    }
    if requirements_profile == "macos-alpha-v1":
        expected_argv = {
            "download": [
                "curl",
                "--fail",
                "--location",
                "--output",
                artifact_name,
                url,
            ],
            "checksum": ["shasum", "-a", "256", artifact_name],
            "zip_quarantine": [
                "xattr",
                "-p",
                "com.apple.quarantine",
                artifact_name,
            ],
            "app_quarantine": [
                "xattr",
                "-p",
                "com.apple.quarantine",
                "Tanks3D.app",
            ],
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
    if command_map["checksum"]["exit_code"] != 0:
        raise VerificationError("checksum command must exit 0")
    if (
        requirements_profile == "macos-alpha-v1"
        and command_map["download"]["exit_code"] != 0
    ):
        raise VerificationError("download command must exit 0")
    checksum = command_map["checksum"]
    if requirements_profile == "macos-alpha-v2":
        if (
            checksum["stdout"] != "{}  {}\n".format(artifact_sha256, artifact_name)
            or checksum["stderr"] != ""
        ):
            raise VerificationError(
                "checksum command log does not exactly bind the candidate digest"
            )
    elif artifact_sha256 not in checksum["stdout"] or artifact_name not in checksum["stdout"]:
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
    has_accepted = "accepted" in spctl_text
    has_rejected = "rejected" in spctl_text
    if has_accepted == has_rejected:
        raise VerificationError(
            "spctl command log must record exactly one assessment outcome"
        )
    if (spctl["exit_code"] == 0) != has_accepted:
        raise VerificationError(
            "spctl command log outcome contradicts its exit code"
        )


def validate_performance_log_v1(
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


def candidate_executable_sha256(archive: Path) -> str:
    try:
        with zipfile.ZipFile(archive, "r") as bundle:
            matches = [
                info
                for info in bundle.infolist()
                if info.filename == PERFORMANCE_V2_EXECUTABLE_MEMBER
            ]
            if len(matches) != 1:
                raise VerificationError(
                    "candidate archive must contain exactly one canonical Tanks3D executable"
                )
            member = matches[0]
            if member.is_dir() or member.file_size <= 0 or member.file_size > 512 * 1024 * 1024:
                raise VerificationError(
                    "candidate executable member is empty or unexpectedly large"
                )
            digest = hashlib.sha256()
            with bundle.open(member, "r") as stream:
                for chunk in iter(lambda: stream.read(1024 * 1024), b""):
                    digest.update(chunk)
            return digest.hexdigest()
    except VerificationError:
        raise
    except (OSError, zipfile.BadZipFile, RuntimeError) as exc:
        raise VerificationError(
            "cannot inspect candidate executable: {}".format(exc)
        )


def validate_performance_receipt_reference(
    receipt: Mapping[str, Any],
    key: str,
    expected_filename: str,
    expected_kind: str,
    artifacts_by_name: Mapping[str, Tuple[str, str, str, Path]],
) -> Tuple[str, str, str, Path]:
    context = "performance QA receipt.{}".format(key)
    reference = require_object(receipt[key], context)
    require_exact_keys(
        reference, set(PERFORMANCE_QA_FILE_REFERENCE_KEYS), context
    )
    filename = require_string(reference["path"], context + ".path")
    if filename != expected_filename:
        raise VerificationError(
            "{} must reference the runner's canonical filename".format(context)
        )
    digest = require_sha256(reference["sha256"], context + ".sha256")
    artifact = artifacts_by_name.get(filename)
    if artifact is None:
        raise VerificationError(
            "{} is not an extended_session_metrics evidence artifact".format(context)
        )
    if artifact[1] != digest:
        raise VerificationError("{} hash contradicts its evidence artifact".format(context))
    if artifact[2] != expected_kind:
        raise VerificationError(
            "{} evidence artifact must use kind {}".format(context, expected_kind)
        )
    return artifact


def validate_performance_log_v2(
    status: Mapping[str, Any],
    evidence_map: Mapping[str, Mapping[str, Any]],
    artifact_map: Mapping[str, Tuple[str, str, str, Path]],
    release: Mapping[str, Any],
    candidate_archive: Path,
    candidate_sha256: str,
) -> None:
    if status["extended_session"]["status"] != "PASS":
        return

    evidence = evidence_map["extended_session_metrics"]
    artifacts_by_name: Dict[str, Tuple[str, str, str, Path]] = {}
    structured: List[Tuple[Tuple[str, str, str, Path], Mapping[str, Any]]] = []
    for raw_artifact in evidence["artifacts"]:
        normalized = artifact_map[raw_artifact["path"]]
        filename = normalized[3].name
        if filename in artifacts_by_name:
            raise VerificationError(
                "extended-session evidence has duplicate artifact filename {!r}".format(
                    filename
                )
            )
        artifacts_by_name[filename] = normalized
        parsed = load_structured_artifact(
            normalized[3],
            "evidence artifact " + normalized[0],
            maximum_bytes=PERFORMANCE_V2_LIMITS_BY_FILENAME.get(
                filename, MAXIMUM_TEXT_EVIDENCE_BYTES
            ),
            expected_sha256=normalized[1],
        )
        if parsed is not None:
            structured.append((normalized, parsed))

    receipt_entries = [
        item
        for item in structured
        if item[1].get("schema") == PERFORMANCE_QA_RECEIPT_SCHEMA
    ]
    if len(receipt_entries) != 1:
        raise VerificationError(
            "extended-session PASS under v2 requires exactly one performance QA receipt"
        )
    receipt_artifact, receipt = receipt_entries[0]
    if (
        receipt_artifact[3].name != PERFORMANCE_V2_FILENAMES["receipt"]
        or receipt_artifact[2] != "report"
    ):
        raise VerificationError(
            "performance QA receipt must be the canonical report artifact"
        )
    require_exact_keys(
        receipt, set(PERFORMANCE_QA_RECEIPT_KEYS), "performance QA receipt"
    )

    telemetry_artifact = validate_performance_receipt_reference(
        receipt,
        "telemetry",
        PERFORMANCE_V2_FILENAMES["telemetry"],
        "log",
        artifacts_by_name,
    )
    stdout_artifact = validate_performance_receipt_reference(
        receipt,
        "stdout",
        PERFORMANCE_V2_FILENAMES["stdout"],
        "log",
        artifacts_by_name,
    )
    stderr_artifact = validate_performance_receipt_reference(
        receipt,
        "stderr",
        PERFORMANCE_V2_FILENAMES["stderr"],
        "log",
        artifacts_by_name,
    )

    matching_logs = [
        item
        for item in structured
        if item[1].get("schema") == PERFORMANCE_LOG_V2_SCHEMA
    ]
    if len(matching_logs) != 1 or matching_logs[0][0][3] != telemetry_artifact[3]:
        raise VerificationError(
            "extended-session PASS requires exactly one receipt-bound v2 telemetry log"
        )
    log = matching_logs[0][1]

    if receipt["schema"] != PERFORMANCE_QA_RECEIPT_SCHEMA:
        raise VerificationError("performance QA receipt schema is not canonical")
    artifact_name = candidate_archive.name
    if receipt["candidate_filename"] != artifact_name:
        raise VerificationError(
            "performance QA receipt candidate filename does not match the release"
        )
    if require_sha256(
        receipt["candidate_sha256"],
        "performance QA receipt.candidate_sha256",
    ) != candidate_sha256:
        raise VerificationError("performance QA receipt is not bound to the candidate")
    executable_digest = require_sha256(
        receipt["executable_sha256"],
        "performance QA receipt.executable_sha256",
    )
    if executable_digest != candidate_executable_sha256(candidate_archive):
        raise VerificationError(
            "performance QA receipt executable hash does not match the candidate ZIP"
        )
    for key, expected in (
        ("source_commit", release["source_commit"]),
        ("source_tag", release["tag"]),
    ):
        if require_string(receipt[key], "performance QA receipt." + key) != expected:
            raise VerificationError(
                "performance QA receipt {} does not match the release".format(key)
            )
    nonce = require_string(
        receipt["session_nonce"], "performance QA receipt.session_nonce"
    )
    if re.fullmatch(r"[0-9a-f]{32}", nonce) is None:
        raise VerificationError(
            "performance QA receipt session nonce must be 32 lowercase hex characters"
        )
    require_json_integer(receipt["pid"], 1, "performance QA receipt.pid")
    exit_code = require_json_integer(
        receipt["exit_code"], 0, "performance QA receipt.exit_code"
    )
    if exit_code != 0:
        raise VerificationError("performance QA receipt exit_code must be zero")
    receipt_started_text = require_timestamp(
        receipt["started_at_utc"], "performance QA receipt.started_at_utc"
    )
    receipt_completed_text = require_timestamp(
        receipt["completed_at_utc"], "performance QA receipt.completed_at_utc"
    )
    receipt_started = timestamp_value(receipt_started_text)
    receipt_completed = timestamp_value(receipt_completed_text)
    if receipt_completed < receipt_started:
        raise VerificationError("performance QA receipt ends before it starts")

    argv = parse_command_argv(receipt["argv"], "performance QA receipt.argv")
    if len(argv) != 6:
        raise VerificationError(
            "performance QA receipt argv must contain the runner's six exact arguments"
        )
    executable_argument = argv[0]
    executable_path = Path(executable_argument)
    if (
        "\\" in executable_argument
        or "\r" in executable_argument
        or "\n" in executable_argument
        or not executable_path.is_absolute()
        or executable_path.as_posix() != executable_argument
        or not executable_argument.endswith("/" + PERFORMANCE_V2_EXECUTABLE_MEMBER)
        or not any(
            part.startswith("tanks3d-performance-qa-")
            for part in executable_path.parts
        )
        or any(part in {".", ".."} for part in executable_path.parts)
    ):
        raise VerificationError(
            "performance QA receipt argv[0] is not the runner-extracted candidate executable"
        )
    if argv[1] != "--quick-start":
        raise VerificationError(
            "performance QA receipt argv must start the candidate with --quick-start"
        )
    telemetry_prefix = "--release-performance-log="
    if not argv[2].startswith(telemetry_prefix):
        raise VerificationError("performance QA receipt argv lacks the telemetry path flag")
    telemetry_argument = argv[2][len(telemetry_prefix) :]
    telemetry_path = Path(telemetry_argument)
    if (
        not telemetry_path.is_absolute()
        or telemetry_path.as_posix() != telemetry_argument
        or telemetry_path.name != PERFORMANCE_V2_FILENAMES["telemetry"]
        or "\\" in telemetry_argument
        or "\r" in telemetry_argument
        or "\n" in telemetry_argument
        or any(part in {".", ".."} for part in telemetry_path.parts)
    ):
        raise VerificationError(
            "performance QA receipt telemetry path flag is not canonical"
        )
    expected_candidate_flag = "--release-candidate-sha256={}".format(
        candidate_sha256
    )
    expected_nonce_flag = "--release-session-nonce={}".format(nonce)
    if argv[3] != expected_candidate_flag or argv[4] != expected_nonce_flag:
        raise VerificationError(
            "performance QA receipt argv candidate or nonce flag is not exact"
        )
    duration_prefix = "--release-performance-duration-seconds="
    if not argv[5].startswith(duration_prefix):
        raise VerificationError("performance QA receipt argv lacks the duration flag")
    duration_text = argv[5][len(duration_prefix) :]
    try:
        requested_duration_seconds = int(duration_text, 10)
    except ValueError:
        raise VerificationError(
            "performance QA receipt duration flag must be a base-10 integer"
        )
    if (
        str(requested_duration_seconds) != duration_text
        or requested_duration_seconds < 1800
        or requested_duration_seconds > PERFORMANCE_V2_MAXIMUM_DURATION_SECONDS
    ):
        raise VerificationError(
            "performance QA receipt duration flag is outside the release range"
        )

    stdout_data = read_regular_file_no_follow(
        stdout_artifact[3],
        "performance stdout",
        maximum_bytes=PERFORMANCE_V2_ARTIFACT_MAXIMUM_BYTES["stdout"],
    )
    if hashlib.sha256(stdout_data).hexdigest() != stdout_artifact[1]:
        raise VerificationError("performance stdout changed after evidence validation")
    stderr_data = read_regular_file_no_follow(
        stderr_artifact[3],
        "performance stderr",
        maximum_bytes=PERFORMANCE_V2_ARTIFACT_MAXIMUM_BYTES["stderr"],
    )
    if hashlib.sha256(stderr_data).hexdigest() != stderr_artifact[1]:
        raise VerificationError("performance stderr changed after evidence validation")
    if stderr_data:
        raise VerificationError("performance stderr must be empty")
    try:
        stdout_text = stdout_data.decode("utf-8")
    except UnicodeError as exc:
        raise VerificationError("cannot read performance stdout: {}".format(exc))
    sessions = [
        value
        for _, value in structured
        if value.get("schema") == INTERACTIVE_SESSION_SCHEMA
    ]
    if len(sessions) != 1:
        raise VerificationError(
            "extended-session PASS requires exactly one matching interactive session"
        )
    try:
        performance_contract.validate_performance_markers(stdout_text, nonce)
        summary = performance_contract.validate_performance_log_v2(
            log,
            performance_contract.RunBinding(
                source_commit=release["source_commit"],
                source_tag=release["tag"],
                candidate_sha256=candidate_sha256,
                session_nonce=nonce,
                requested_duration_seconds=requested_duration_seconds,
                receipt_started_at_utc=receipt_started_text,
                receipt_completed_at_utc=receipt_completed_text,
                expected_started_at_utc=sessions[0]["started_at_utc"],
                expected_completed_at_utc=sessions[0]["completed_at_utc"],
            ),
        )
    except performance_contract.PerformanceContractError as exc:
        raise VerificationError(str(exc))
    if summary.wall_duration_us < PERFORMANCE_V2_MINIMUM_DURATION_US:
        raise VerificationError("performance log v2 wall duration is shorter than 30 minutes")
    if (
        summary.gameplay_duration_ratio
        < PERFORMANCE_V2_MINIMUM_GAMEPLAY_DURATION_RATIO
    ):
        raise VerificationError(
            "raw performance samples do not contain enough active gameplay"
        )
    if (
        summary.focused_duration_ratio
        < PERFORMANCE_V2_MINIMUM_FOCUSED_DURATION_RATIO
    ):
        raise VerificationError(
            "raw performance samples do not contain enough focused-window time"
        )
    minimum_stages = PERFORMANCE_THRESHOLDS_V2["minimum_stages_completed"]
    if (
        summary.completed_stages < minimum_stages
        or summary.stage_clear_events != summary.completed_stages
    ):
        raise VerificationError(
            "raw performance samples do not prove a completed stage"
        )
    recorded_stages = require_number_at_least(
        status["extended_session"]["details"]["stages_completed"],
        0,
        "extended-session stages_completed",
    )
    if (
        not recorded_stages.is_integer()
        or int(recorded_stages) != summary.completed_stages
    ):
        raise VerificationError(
            "extended-session stages_completed contradicts raw v2 samples"
        )
    recorded_mode_mix = require_string(
        status["extended_session"]["details"]["mode_mix"],
        "extended-session mode_mix",
    ).strip().lower()
    if recorded_mode_mix != "one-player":
        raise VerificationError(
            "extended-session mode_mix contradicts raw v2 samples"
        )
    if (
        summary.memory_growth_bytes
        > PERFORMANCE_V2_MAXIMUM_MEMORY_GROWTH_BYTES
    ):
        raise VerificationError(
            "raw performance samples exceed the fixed Alpha memory-growth limit"
        )
    details = status["extended_session"]["details"]
    actual_metrics = {
        "duration_minutes": summary.monotonic_duration_us / 60_000_000.0,
        "average_fps": summary.average_fps,
        "minimum_fps": summary.minimum_fps,
        "one_percent_low_fps": summary.one_percent_low_fps,
        "memory_start_mb": summary.memory_start_mb,
        "memory_end_mb": summary.memory_end_mb,
    }
    for key, actual in actual_metrics.items():
        recorded = require_number_at_least(
            details[key], 0, "extended-session " + key
        )
        if abs(recorded - actual) > 0.11:
            raise VerificationError(
                "extended-session {} contradicts raw v2 samples".format(key)
            )
    if require_number_at_least(
        details["sampling_interval_seconds"],
        0,
        "extended-session sampling_interval_seconds",
    ) != 1:
        raise VerificationError(
            "extended-session sampling interval must be the fixed v2 one second"
        )
    fixed_values = {
        "minimum_average_fps": PERFORMANCE_THRESHOLDS_V2["minimum_average_fps"],
        "minimum_one_percent_low_fps": PERFORMANCE_THRESHOLDS_V2[
            "minimum_one_percent_low_fps"
        ],
        "maximum_memory_growth_mb": PERFORMANCE_THRESHOLDS_V2[
            "maximum_memory_growth_mb"
        ],
    }
    for key, expected in fixed_values.items():
        if require_number_at_least(
            details[key], 0, "extended-session " + key
        ) != expected:
            raise VerificationError(
                "extended-session {} must use the fixed Alpha threshold under v2".format(
                    key
                )
            )
    if (
        summary.average_fps < PERFORMANCE_THRESHOLDS_V2["minimum_average_fps"]
        or summary.one_percent_low_fps
        < PERFORMANCE_THRESHOLDS_V2["minimum_one_percent_low_fps"]
    ):
        raise VerificationError("raw performance samples miss the fixed Alpha FPS threshold")


def validate_performance_log(
    status: Mapping[str, Any],
    evidence_map: Mapping[str, Mapping[str, Any]],
    artifact_map: Mapping[str, Tuple[str, str, str, Path]],
    release: Mapping[str, Any],
    candidate_archive: Path,
    candidate_sha256: str,
    requirements_profile: str,
) -> None:
    if requirements_profile == "macos-alpha-v1":
        validate_performance_log_v1(
            status, evidence_map, artifact_map, candidate_sha256
        )
        return
    if requirements_profile == "macos-alpha-v2":
        validate_performance_log_v2(
            status,
            evidence_map,
            artifact_map,
            release,
            candidate_archive,
            candidate_sha256,
        )
        return
    raise VerificationError("unsupported performance evidence profile")


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


def validate_current_v2_candidate_contract(
    attestation_path: Path, build_config_path: Path
) -> None:
    attestation = parse_attestation(attestation_path)
    if attestation.get("schema") != CURRENT_V2_CANDIDATE_ATTESTATION_SCHEMA:
        raise VerificationError(
            "macos-alpha-v2 candidate must use {}".format(
                CURRENT_V2_CANDIDATE_ATTESTATION_SCHEMA
            )
        )
    try:
        lines = build_config_path.read_text(encoding="utf-8").splitlines()
    except (OSError, UnicodeError) as exc:
        raise VerificationError(
            "cannot read candidate build configuration: {}".format(exc)
        )
    values: Dict[str, str] = {}
    for line_number, line in enumerate(lines, 1):
        if not line or "=" not in line:
            raise VerificationError(
                "malformed candidate build configuration line {}".format(
                    line_number
                )
            )
        key, value = line.split("=", 1)
        if not key or key in values:
            raise VerificationError(
                "duplicate or empty candidate build configuration key on line {}".format(
                    line_number
                )
            )
        values[key] = value
    for key, expected in CURRENT_V2_PERFORMANCE_BUILD_CONFIG.items():
        if values.get(key) != expected:
            raise VerificationError(
                "macos-alpha-v2 candidate build configuration lacks the current {} contract".format(
                    key
                )
            )


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


def invoke_tagged_candidate_verifier(
    root: Path,
    candidate_dir: Path,
    file_refs: Mapping[str, Tuple[Path, str]],
) -> None:
    verifier = resolve_repository_file(
        root,
        "scripts/verify_tagged_alpha_candidate.sh",
        "tagged candidate verifier",
    )
    verifier_parent_fd = open_repository_directory_no_follow(
        root, verifier.parent, "tagged verifier directory"
    )
    try:
        verifier_data = read_regular_file_no_follow(
            Path(verifier.name),
            "scripts/verify_tagged_alpha_candidate.sh",
            directory_fd=verifier_parent_fd,
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
        raise VerificationError("cannot run tagged candidate verifier: {}".format(exc))
    if completed.returncode != 0:
        detail = (completed.stderr or completed.stdout).decode(
            "utf-8", errors="replace"
        ).strip()
        if len(detail) > 600:
            detail = detail[-600:]
        raise VerificationError(
            "tagged candidate verifier failed (exit {}): {}".format(
                completed.returncode, detail or "no diagnostic"
            )
        )
    try:
        verifier_stdout = completed.stdout.decode("utf-8")
    except UnicodeError as exc:
        raise VerificationError("tagged verifier output is not UTF-8: {}".format(exc))
    prefix = "VERIFIED CANDIDATE FILE SHA256 "
    receipt: Dict[str, str] = {}
    for line in verifier_stdout.splitlines():
        if not line.startswith(prefix):
            continue
        fields = line[len(prefix) :].split(" ", 1)
        if len(fields) != 2:
            raise VerificationError("tagged verifier emitted a malformed candidate receipt")
        digest, name = fields
        if SHA256_RE.fullmatch(digest) is None or TOKEN_RE.fullmatch(name) is None:
            raise VerificationError("tagged verifier emitted an invalid candidate receipt")
        if name in receipt:
            raise VerificationError("tagged verifier emitted a duplicate candidate receipt")
        receipt[name] = digest
    expected_receipt = {
        file_refs[key][0].name: file_refs[key][1]
        for key in ("artifact", "checksum", "attestation", "gate_log", "build_config")
    }
    if len(receipt) != 5 or receipt != expected_receipt:
        raise VerificationError(
            "tagged verifier receipt does not match the five release candidate files"
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
    release_pixel_digests: Dict[str, str] = {}
    for name in screenshot_names:
        repository_path = "docs/assets/releases/{}/{}".format(tag, name)
        reference = "../assets/releases/{}/{}".format(tag, name)
        if repository_path not in screenshot_artifacts:
            raise VerificationError("release evidence is missing {}".format(repository_path))
        digest = screenshot_artifacts[repository_path][1]
        pixel_digest = verify_png(
            screenshot_artifacts[repository_path][3],
            "release evidence {}".format(name),
            require_release_size=True,
        )
        if pixel_digest is None:
            raise VerificationError(
                "release evidence {} has no normalized pixel digest".format(name)
            )
        previous_name = release_pixel_digests.get(pixel_digest)
        if previous_name is not None:
            raise VerificationError(
                "release screenshots must show seven distinct images; {} and {} "
                "have identical pixels".format(previous_name, name)
            )
        release_pixel_digests[pixel_digest] = name
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
    requirements_profile: str,
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
    if requirements_profile == "macos-alpha-v2":
        require_interactive_coverage(
            status["published_controls"],
            "status.published_controls",
            "controls:published_controls_match",
            candidate_sha256,
            evidence_map,
            {
                item["evidence_id"]: item["coverage_token"]
                for item in PUBLISHED_CONTROL_CONTEXT_REQUIREMENTS
            },
        )
    else:
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
    reject_blocking_language(rationale, "status.audio.rationale")
    reject_audio_acceptance_conflict(rationale, "status.audio.rationale")
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
    requirements_profiles = {
        "docs/release-requirements/macos-alpha-v1.json": "macos-alpha-v1",
        "docs/release-requirements/macos-alpha-v2.json": "macos-alpha-v2",
    }
    requirements_profile = requirements_profiles.get(status["requirements"])
    if requirements_profile is None:
        raise VerificationError("status.requirements must name a canonical Alpha profile")
    requirements_path = resolve_repository_file(root, status["requirements"], "status.requirements")
    validate_requirements(load_json_strict(requirements_path), requirements_profile)

    blockers: List[str] = []
    release, file_refs, candidate_dir = validate_release(root, status["release"])
    report = validate_report(status["report"], blockers)
    evidence_map, artifact_map = validate_evidence(
        root,
        status["evidence"],
        blockers,
        requirements_profile,
    )

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
        PUBLISHED_CONTROL_CHECKS
        + (ADVANCED_SETTINGS_CHECKS if requirements_profile == "macos-alpha-v2" else []),
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
        (
            V2_CLEAN_MAC_EVIDENCE_IDS
            if requirements_profile == "macos-alpha-v2"
            else V1_CLEAN_MAC_EVIDENCE_IDS
        ),
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
    validate_interactive_coverage_matrix(
        status,
        evidence_map,
        file_refs["artifact"][1],
        requirements_profile,
    )
    validate_structured_interactive_evidence(
        status,
        evidence_map,
        artifact_map,
        file_refs["artifact"][1],
        requirements_profile,
    )
    validate_clean_mac_command_log(
        status,
        evidence_map,
        artifact_map,
        file_refs["artifact"][0].name,
        file_refs["artifact"][1],
        requirements_profile,
    )
    validate_performance_log(
        status,
        evidence_map,
        artifact_map,
        release,
        file_refs["artifact"][0],
        file_refs["artifact"][1],
        requirements_profile,
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
    if requirements_profile == "macos-alpha-v2":
        validate_current_v2_candidate_contract(
            file_refs["attestation"][0],
            file_refs["build_config"][0]
        )
    invoke_tagged_candidate_verifier(root, candidate_dir, file_refs)

    if not blockers and requirements_profile == "macos-alpha-v1":
        raise VerificationError(
            "macos-alpha-v1 is superseded and cannot approve a release; "
            "use macos-alpha-v2 candidate-bound performance evidence"
        )
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
