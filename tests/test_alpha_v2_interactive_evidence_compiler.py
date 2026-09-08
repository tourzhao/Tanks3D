#!/usr/bin/env python3
"""Tests for the explicit Alpha-v2 interactive evidence compiler."""

from datetime import datetime, timedelta, timezone
import copy
import importlib.util
import json
import os
from pathlib import Path
import struct
import sys
import tempfile
import unittest
from unittest import mock
import zlib


SCRIPT = (
    Path(__file__).resolve().parents[1]
    / "scripts"
    / "compile_alpha_v2_interactive_evidence.py"
)
SPEC = importlib.util.spec_from_file_location("alpha_v2_evidence_compiler", SCRIPT)
assert SPEC is not None and SPEC.loader is not None
compiler = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = compiler
SPEC.loader.exec_module(compiler)


CANDIDATE_SHA256 = "a" * 64


def write_json(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def png_chunk(kind, payload):
    checksum = zlib.crc32(kind)
    checksum = zlib.crc32(payload, checksum) & 0xFFFFFFFF
    return struct.pack(">I", len(payload)) + kind + payload + struct.pack(">I", checksum)


def write_png(path, width=2, height=2):
    path.parent.mkdir(parents=True, exist_ok=True)
    pixel = bytes((47, 127, 211, 255))
    rows = (b"\x00" + pixel * width) * height
    data = b"\x89PNG\r\n\x1a\n"
    data += png_chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0))
    data += png_chunk(b"IDAT", zlib.compress(rows))
    data += png_chunk(b"IEND", b"")
    path.write_bytes(data)


def write_recording(path, seed=1):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(bytes((seed,)) * compiler.MINIMUM_RECORDING_BYTES)


def result_row(item_id, mode=None):
    row = {
        "id": item_id,
        "status": "NOT_RUN",
        "tester": "",
        "tested_at_utc": None,
        "evidence_ids": [],
        "checks_confirmed": [],
        "notes": "Not run against this candidate.",
    }
    if mode is not None:
        row = {"id": item_id, "mode": mode, **{key: value for key, value in row.items() if key != "id"}}
    return row


class CompilerFixture:
    def __init__(self, testcase):
        self.testcase = testcase
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name)
        self.requirements_path = self.root / "docs" / "release-requirements" / "macos-alpha-v2.json"
        self.status_path = self.root / "docs" / "releases" / "alpha-status.json"
        self.manifest_path = self.root / "observation-plan.json"
        self.output_dir = self.root / "compiled-evidence"
        self.profile = {
            "schema": compiler.REQUIREMENTS_SCHEMA,
            "profile": "macos-alpha-v2",
            "interactive_controls_revision": compiler.INTERACTIVE_CONTROLS_REVISION,
            "interactive_observation_manifest_schema": (
                compiler.OBSERVATION_MANIFEST_SCHEMA
            ),
            "interactive_observation_manifest_keys": sorted(
                compiler.MANIFEST_KEYS
            ),
            "interactive_observation_keys": sorted(
                compiler.OBSERVATION_KEYS
            ),
            "interactive_observation_evidence_ids": list(
                compiler.EVIDENCE_IDS
            ),
            "interactive_supporting_artifact_group_keys": [
                "evidence_id",
                "artifacts",
            ],
            "interactive_supporting_artifact_keys": ["path", "kind"],
            "gameplay_event_log_schema": compiler.EVENT_LOG_SCHEMA,
            "gameplay_event_log_producer": compiler.EVENT_LOG_PRODUCER,
            "gameplay_event_log_keys": [
                "schema",
                "producer",
                compiler.OBSERVATION_MANIFEST_SHA256_KEY,
                "candidate_sha256",
                "tester",
                "machine",
                "started_at_utc",
                "completed_at_utc",
                "events",
            ],
            "published_control_checks": list(
                compiler.PUBLISHED_CONTROL_CHECKS
            ),
            "advanced_settings_checks": list(
                compiler.ADVANCED_SETTINGS_CHECKS
            ),
            "published_control_context_requirements": [
                {
                    "context": item["context"],
                    "coverage_token": item["coverage_token"],
                    "evidence_id": item["evidence_id"],
                    "checks": list(item["checks"]),
                }
                for item in compiler.CONTROL_CONTEXT_REQUIREMENTS
            ],
            "clean_mac_evidence_ids": ["gatekeeper_launch"],
            "clean_mac_minimum_recording_bytes": compiler.MINIMUM_RECORDING_BYTES,
            "clean_mac_maximum_recording_bytes": compiler.MAXIMUM_RECORDING_BYTES,
            "modes": ["one_player", "two_player"],
            "gameplay_ids": ["movement"],
            "base_ids": ["usa"],
            "base_checks": ["wall_damage", "core_loss"],
            "pickup_requirements": [
                {"id": "star", "checks": ["model_3d_visible", "gameplay_effect_matches"]}
            ],
            "settlement_ids": ["basic_tank_ko"],
        }
        self.status = {
            "schema": "tanks3d-release-status-v1",
            "requirements": "docs/release-requirements/macos-alpha-v2.json",
            "release": {"artifact": {"path": "candidate.zip", "sha256": CANDIDATE_SHA256}},
            "gameplay": [result_row("movement", mode) for mode in self.profile["modes"]],
            "bases": [result_row("usa", mode) for mode in self.profile["modes"]],
            "pickups": [result_row("star", mode) for mode in self.profile["modes"]],
            "settlement": [result_row("basic_tank_ko")],
            "published_controls": result_row("published_controls_match"),
            "evidence": [
                {
                    "id": evidence_id,
                    "status": "NOT_RUN",
                    "artifacts": [],
                    "reviewer": "",
                    "reviewed_at_utc": None,
                    "interactive": {
                        "candidate_sha256": "",
                        "tester": "",
                        "machine": "",
                        "tested_at_utc": None,
                        "signature": "",
                        "coverage_refs": {},
                    },
                    "notes": "Evidence not collected.",
                }
                for evidence_id in compiler.EVIDENCE_IDS
            ],
            "untouched_marker": {"value": "must remain byte-for-byte unchanged"},
        }
        write_json(self.requirements_path, self.profile)
        write_json(self.status_path, self.status)
        self.original_status = self.status_path.read_bytes()

    def close(self):
        self.temporary.cleanup()

    def init_plan(self):
        result = compiler.main(
            [
                "init-plan",
                "--requirements",
                str(self.requirements_path),
                "--output",
                str(self.manifest_path),
            ]
        )
        self.testcase.assertEqual(result, 0)
        return json.loads(self.manifest_path.read_text(encoding="utf-8"))

    def valid_manifest(self):
        manifest = self.init_plan()
        manifest.update(
            {
                "candidate_sha256": CANDIDATE_SHA256,
                "tester": "Alice Tester",
                "machine": "Test Mac arm64",
                "tester_signature": "Alice Tester",
                "reviewer": "Bob Reviewer",
                "reviewer_signature": "Bob Reviewer",
                "started_at_utc": "2026-08-08T10:00:00Z",
                "completed_at_utc": "2026-08-08T11:00:00Z",
                "reviewed_at_utc": "2026-08-08T12:00:00Z",
                "review_notes": "Interactive observations and captures reviewed.",
            }
        )
        for index, observation in enumerate(manifest["observations"]):
            observation["result"] = "PASS"
            observation["observed_at_utc"] = "2026-08-08T10:{:02d}:00Z".format(index + 1)
            observation["checks_confirmed"] = list(observation["required_checks"])
            observation["notes"] = "Explicitly exercised {}.".format(observation["coverage_token"])
        for group in manifest["supporting_artifacts"]:
            if group["evidence_id"] in compiler.CONTROL_EVIDENCE_IDS:
                artifact_path = self.root / "captures" / (
                    group["evidence_id"] + ".mp4"
                )
                write_recording(
                    artifact_path,
                    compiler.EVIDENCE_IDS.index(group["evidence_id"]) + 1,
                )
                kind = "recording"
            else:
                artifact_path = self.root / "captures" / (
                    group["evidence_id"] + ".png"
                )
                write_png(artifact_path)
                kind = "png"
            group["artifacts"] = [
                {
                    "path": artifact_path.relative_to(self.root).as_posix(),
                    "kind": kind,
                }
            ]
        write_json(self.manifest_path, manifest)
        return manifest

    def compile(self):
        return compiler.main(
            [
                "compile",
                "--project-root",
                str(self.root),
                "--status",
                self.status_path.relative_to(self.root).as_posix(),
                "--manifest",
                self.manifest_path.relative_to(self.root).as_posix(),
                "--output-dir",
                self.output_dir.relative_to(self.root).as_posix(),
            ]
        )

    def assert_rejected_without_mutation(self):
        self.testcase.assertEqual(self.compile(), 1)
        self.testcase.assertFalse(self.output_dir.exists())
        self.testcase.assertEqual(self.status_path.read_bytes(), self.original_status)


class InteractiveEvidenceCompilerTests(unittest.TestCase):
    def setUp(self):
        self.fixture = CompilerFixture(self)

    def tearDown(self):
        self.fixture.close()

    def test_init_plan_is_all_not_run_and_never_claims_checks(self):
        manifest = self.fixture.init_plan()
        self.assertEqual(manifest["schema"], compiler.OBSERVATION_MANIFEST_SCHEMA)
        self.assertEqual(manifest["event_log_schema"], compiler.EVENT_LOG_SCHEMA)
        self.assertEqual(manifest["event_log_producer"], compiler.EVENT_LOG_PRODUCER)
        self.assertTrue(manifest["observations"])
        self.assertTrue(all(item["result"] == "NOT_RUN" for item in manifest["observations"]))
        self.assertTrue(all(item["checks_confirmed"] == [] for item in manifest["observations"]))
        self.assertTrue(all(item["observed_at_utc"] is None for item in manifest["observations"]))
        self.assertEqual(
            [
                item["coverage_token"]
                for item in manifest["observations"][-3:]
            ],
            ["controls:main_menu", "controls:one_player", "controls:two_player"],
        )
        self.assertEqual(
            [item["evidence_id"] for item in manifest["supporting_artifacts"]],
            list(compiler.EVIDENCE_IDS),
        )
        before = self.fixture.manifest_path.read_bytes()
        self.assertEqual(
            compiler.main(
                [
                    "init-plan",
                    "--requirements",
                    str(self.fixture.requirements_path),
                    "--output",
                    str(self.fixture.manifest_path),
                ]
            ),
            1,
        )
        self.assertEqual(self.fixture.manifest_path.read_bytes(), before)

    def test_success_compiles_bound_pack_and_keeps_original_status_unchanged(self):
        self.fixture.valid_manifest()
        self.assertEqual(self.fixture.compile(), 0)
        self.assertEqual(self.fixture.status_path.read_bytes(), self.fixture.original_status)
        expected = {"observation-manifest.json", "status.next.json"}
        for evidence_id in compiler.EVIDENCE_IDS:
            expected.add(evidence_id + "-events.json")
            expected.add(evidence_id + "-session.json")
        self.assertEqual({path.name for path in self.fixture.output_dir.iterdir()}, expected)

        copied_manifest = (self.fixture.output_dir / "observation-manifest.json").read_bytes()
        manifest_digest = compiler.sha256_bytes(copied_manifest)
        event_log = json.loads(
            (self.fixture.output_dir / "one_player_gameplay-events.json").read_text(encoding="utf-8")
        )
        self.assertEqual(event_log["schema"], compiler.EVENT_LOG_SCHEMA)
        self.assertEqual(event_log["producer"], compiler.EVENT_LOG_PRODUCER)
        self.assertEqual(
            event_log[compiler.OBSERVATION_MANIFEST_SHA256_KEY], manifest_digest
        )
        self.assertEqual(event_log["candidate_sha256"], CANDIDATE_SHA256)
        self.assertTrue(all(event["result"] == "PASS" for event in event_log["events"]))
        self.assertEqual(
            [event["coverage_token"] for event in event_log["events"]],
            ["gameplay:movement:one_player", "controls:one_player"],
        )
        menu_event_log = json.loads(
            (
                self.fixture.output_dir
                / "main_menu_and_advanced_settings-events.json"
            ).read_text(encoding="utf-8")
        )
        self.assertEqual(
            [event["coverage_token"] for event in menu_event_log["events"]],
            ["controls:main_menu"],
        )

        next_status = json.loads(
            (self.fixture.output_dir / "status.next.json").read_text(encoding="utf-8")
        )
        self.assertEqual(next_status["untouched_marker"], self.fixture.status["untouched_marker"])
        for section in ("gameplay", "bases", "pickups", "settlement"):
            self.assertTrue(all(row["status"] == "PASS" for row in next_status[section]))
        controls = next_status["published_controls"]
        self.assertEqual(controls["status"], "PASS")
        self.assertEqual(controls["tester"], "Alice Tester")
        self.assertEqual(controls["tested_at_utc"], "2026-08-08T11:00:00Z")
        self.assertEqual(controls["evidence_ids"], list(compiler.CONTROL_EVIDENCE_IDS))
        self.assertEqual(
            controls["checks_confirmed"],
            list(
                compiler.PUBLISHED_CONTROL_CHECKS
                + compiler.ADVANCED_SETTINGS_CHECKS
            ),
        )
        self.assertEqual(
            controls["notes"],
            "Interactive observations and captures reviewed.",
        )
        self.assertTrue(
            all(item["status"] == "PASS" for item in next_status["evidence"])
        )

    def test_repository_profile_has_exactly_seventy_observations(self):
        profile_path = SCRIPT.parents[1] / "docs/release-requirements/macos-alpha-v2.json"
        _, profile = compiler.load_profile(profile_path)
        plan = compiler.token_plan(profile)
        self.assertEqual(len(plan), 70)
        self.assertEqual(
            [item["coverage_token"] for item in plan[-3:]],
            ["controls:main_menu", "controls:one_player", "controls:two_player"],
        )

    def test_not_run_observation_is_rejected(self):
        manifest = self.fixture.valid_manifest()
        manifest["observations"][0]["result"] = "NOT_RUN"
        write_json(self.fixture.manifest_path, manifest)
        self.fixture.assert_rejected_without_mutation()

    def test_camera_controller_checks_are_required_in_each_applicable_context(self):
        expected_contexts = {
            "controller_menu_navigation_confirm_cancel_reset": {"main_menu"},
            "controller_fire_pause_cancel_no_face_exit": {"one_player", "two_player"},
            "controller_stable_player_assignments": {"one_player", "two_player"},
            "controller_disconnect_reconnect_without_stuck_input": {
                "main_menu", "one_player", "two_player"
            },
            "camera_relative_stick_all_yaw_steps_and_cardinal_keyboard_dpad": {
                "one_player", "two_player"
            },
            "return_menu_held_stick_release_without_dpad": {"one_player", "two_player"},
            "return_menu_held_stick_release_with_dpad_overlap": {"one_player", "two_player"},
            "camera_yaw_range_minus_45_to_plus_45_step_5": {"main_menu"},
            "camera_elevation_range_40_to_70_step_5": {"main_menu"},
            "camera_defaults_yaw_0_elevation_50_and_reset": {"main_menu"},
            "camera_angles_preserved_on_escape_start_restart": {
                "main_menu", "one_player", "two_player"
            },
            "camera_framing_all_angles_and_window_sizes": {"one_player", "two_player"},
            "solo_camera_continuous_follow_and_reset": {"one_player"},
            "coop_camera_midpoint_separation_and_respawn": {"two_player"},
        }
        manifest = self.fixture.valid_manifest()
        control_rows = {
            row["coverage_token"].split(":")[1]: row
            for row in manifest["observations"]
            if row["coverage_token"].startswith("controls:")
        }
        for check, contexts in expected_contexts.items():
            self.assertEqual(
                {name for name, row in control_rows.items() if check in row["required_checks"]},
                contexts,
                check,
            )
            for context in contexts:
                with self.subTest(check=check, context=context):
                    incomplete = copy.deepcopy(manifest)
                    row = next(
                        item for item in incomplete["observations"]
                        if item["coverage_token"] == "controls:" + context
                    )
                    row["checks_confirmed"].remove(check)
                    with self.assertRaisesRegex(compiler.CompileError, "checks_confirmed are not exact"):
                        compiler.validate_manifest(incomplete, self.fixture.profile, CANDIDATE_SHA256)

    def test_legacy_camera_free_plan_cannot_approve_current_controls(self):
        manifest = self.fixture.valid_manifest()
        # Match the historical 13 published and eight Advanced checks. Updating
        # every old row to PASS must not attest the new camera/controller work.
        old_checks = set(compiler.PUBLISHED_CONTROL_CHECKS[:13]) | set(
            compiler.ADVANCED_SETTINGS_CHECKS[:8]
        )
        self.assertEqual(len(old_checks), 21)
        for row in manifest["observations"]:
            if row["coverage_token"].startswith("controls:"):
                row["required_checks"] = [check for check in row["required_checks"] if check in old_checks]
                row["checks_confirmed"] = list(row["required_checks"])
        write_json(self.fixture.manifest_path, manifest)
        self.fixture.assert_rejected_without_mutation()

    def test_pass_without_explicit_checks_is_rejected(self):
        manifest = self.fixture.valid_manifest()
        manifest["observations"][0]["checks_confirmed"] = []
        write_json(self.fixture.manifest_path, manifest)
        self.fixture.assert_rejected_without_mutation()

    def test_missing_coverage_token_is_rejected(self):
        manifest = self.fixture.valid_manifest()
        manifest["observations"].pop()
        write_json(self.fixture.manifest_path, manifest)
        self.fixture.assert_rejected_without_mutation()

    def test_control_context_contract_drift_is_rejected(self):
        mutations = (
            lambda profile: profile.pop("interactive_controls_revision"),
            lambda profile: profile.__setitem__(
                "interactive_controls_revision", "historical-keyboard-only"
            ),
            lambda profile: profile["published_control_context_requirements"][0].__setitem__(
                "coverage_token", "controls:forged"
            ),
            lambda profile: profile["published_control_context_requirements"][1][
                "checks"
            ].reverse(),
            lambda profile: profile["published_control_context_requirements"][2][
                "checks"
            ].pop(),
            lambda profile: profile.__setitem__(
                "clean_mac_evidence_ids", ["main_menu_and_advanced_settings"]
            ),
            lambda profile: profile.__setitem__(
                "clean_mac_maximum_recording_bytes", 100_000_000
            ),
        )
        for mutation in mutations:
            with self.subTest(mutation=mutation):
                fixture = CompilerFixture(self)
                self.addCleanup(fixture.close)
                profile = json.loads(
                    fixture.requirements_path.read_text(encoding="utf-8")
                )
                mutation(profile)
                write_json(fixture.requirements_path, profile)
                self.assertEqual(
                    compiler.main(
                        [
                            "init-plan",
                            "--requirements",
                            str(fixture.requirements_path),
                            "--output",
                            str(fixture.manifest_path),
                        ]
                    ),
                    1,
                )
                self.assertFalse(fixture.manifest_path.exists())

    def test_candidate_mismatch_is_rejected(self):
        manifest = self.fixture.valid_manifest()
        manifest["candidate_sha256"] = "b" * 64
        write_json(self.fixture.manifest_path, manifest)
        self.fixture.assert_rejected_without_mutation()

    def test_existing_published_controls_pass_is_not_replaced(self):
        self.fixture.valid_manifest()
        self.fixture.status["published_controls"]["status"] = "PASS"
        write_json(self.fixture.status_path, self.fixture.status)
        self.fixture.original_status = self.fixture.status_path.read_bytes()
        self.fixture.assert_rejected_without_mutation()

    def test_non_chronological_observation_is_rejected(self):
        manifest = self.fixture.valid_manifest()
        manifest["observations"][0]["observed_at_utc"] = "2026-08-08T09:59:59Z"
        write_json(self.fixture.manifest_path, manifest)
        self.fixture.assert_rejected_without_mutation()

    def test_future_timestamp_is_rejected(self):
        manifest = self.fixture.valid_manifest()
        future = datetime.now(timezone.utc) + timedelta(hours=1)
        manifest["reviewed_at_utc"] = future.strftime("%Y-%m-%dT%H:%M:%SZ")
        write_json(self.fixture.manifest_path, manifest)
        self.fixture.assert_rejected_without_mutation()

    def test_placeholder_identity_is_rejected(self):
        manifest = self.fixture.valid_manifest()
        manifest["tester"] = "TBD"
        write_json(self.fixture.manifest_path, manifest)
        self.fixture.assert_rejected_without_mutation()

    def test_blocking_review_notes_are_rejected(self):
        manifest = self.fixture.valid_manifest()
        manifest["review_notes"] = "Independent review is pending."
        write_json(self.fixture.manifest_path, manifest)
        self.fixture.assert_rejected_without_mutation()

    def test_blocking_observation_notes_are_rejected(self):
        manifest = self.fixture.valid_manifest()
        manifest["observations"][0]["notes"] = "This check was not tested."
        write_json(self.fixture.manifest_path, manifest)
        self.fixture.assert_rejected_without_mutation()

    def test_same_reviewer_and_tester_is_rejected(self):
        manifest = self.fixture.valid_manifest()
        manifest["reviewer"] = "  ALICE TESTER  "
        write_json(self.fixture.manifest_path, manifest)
        self.fixture.assert_rejected_without_mutation()

    def test_symlinked_supporting_artifact_is_rejected(self):
        manifest = self.fixture.valid_manifest()
        first = manifest["supporting_artifacts"][0]["artifacts"][0]
        original = self.fixture.root / first["path"]
        target = self.fixture.root / "real-capture.png"
        target.write_bytes(original.read_bytes())
        original.unlink()
        os.symlink(str(target), str(original))
        write_json(self.fixture.manifest_path, manifest)
        self.fixture.assert_rejected_without_mutation()

    def test_invalid_png_and_recording_size_bounds_are_rejected(self):
        manifest = self.fixture.valid_manifest()
        png_group = next(
            group
            for group in manifest["supporting_artifacts"]
            if group["evidence_id"] == "national_bases"
        )
        first = png_group["artifacts"][0]
        (self.fixture.root / first["path"]).write_bytes(b"not a PNG")
        write_json(self.fixture.manifest_path, manifest)
        self.fixture.assert_rejected_without_mutation()

        other = CompilerFixture(self)
        self.addCleanup(other.close)
        manifest = other.valid_manifest()
        recording = other.root / "captures" / "too-small.mp4"
        recording.write_bytes(b"0" * (compiler.MINIMUM_RECORDING_BYTES - 1))
        manifest["supporting_artifacts"][0]["artifacts"] = [
            {
                "path": recording.relative_to(other.root).as_posix(),
                "kind": "recording",
            }
        ]
        write_json(other.manifest_path, manifest)
        other.assert_rejected_without_mutation()

        oversized = CompilerFixture(self)
        self.addCleanup(oversized.close)
        manifest = oversized.valid_manifest()
        recording = oversized.root / "captures" / "too-large.mp4"
        with recording.open("wb") as stream:
            stream.truncate(compiler.MAXIMUM_RECORDING_BYTES + 1)
        manifest["supporting_artifacts"][0]["artifacts"] = [
            {
                "path": recording.relative_to(oversized.root).as_posix(),
                "kind": "recording",
            }
        ]
        write_json(oversized.manifest_path, manifest)
        oversized.assert_rejected_without_mutation()

    def test_each_control_context_requires_a_real_recording(self):
        for evidence_id in compiler.CONTROL_EVIDENCE_IDS:
            with self.subTest(evidence_id=evidence_id):
                fixture = CompilerFixture(self)
                self.addCleanup(fixture.close)
                manifest = fixture.valid_manifest()
                group = next(
                    item
                    for item in manifest["supporting_artifacts"]
                    if item["evidence_id"] == evidence_id
                )
                image = fixture.root / "captures" / (evidence_id + "-only.png")
                write_png(image)
                group["artifacts"] = [
                    {
                        "path": image.relative_to(fixture.root).as_posix(),
                        "kind": "png",
                    }
                ]
                write_json(fixture.manifest_path, manifest)
                fixture.assert_rejected_without_mutation()

    def test_supporting_and_existing_recordings_are_streamed(self):
        manifest = self.fixture.valid_manifest()
        for index, group in enumerate(manifest["supporting_artifacts"]):
            recording = self.fixture.root / "captures" / "support-{}.mp4".format(index)
            recording.write_bytes(
                bytes((index + 1,)) * compiler.MINIMUM_RECORDING_BYTES
            )
            group["artifacts"] = [
                {
                    "path": recording.relative_to(self.fixture.root).as_posix(),
                    "kind": "recording",
                }
            ]

        existing = self.fixture.root / "captures" / "existing.mp4"
        existing.write_bytes(b"e" * compiler.MINIMUM_RECORDING_BYTES)
        self.fixture.status["evidence"][0]["artifacts"] = [
            {
                "path": existing.relative_to(self.fixture.root).as_posix(),
                "sha256": compiler.sha256_bytes(existing.read_bytes()),
                "kind": "recording",
            }
        ]
        write_json(self.fixture.status_path, self.fixture.status)
        self.fixture.original_status = self.fixture.status_path.read_bytes()
        write_json(self.fixture.manifest_path, manifest)

        original_reader = compiler.read_regular_file

        def reject_buffered_recording(path, label):
            if Path(path).suffix == ".mp4":
                raise AssertionError("recordings must not use the buffering reader")
            return original_reader(path, label)

        with mock.patch.object(
            compiler, "read_regular_file", side_effect=reject_buffered_recording
        ):
            self.assertEqual(self.fixture.compile(), 0)
        self.assertEqual(
            self.fixture.status_path.read_bytes(), self.fixture.original_status
        )

    def test_existing_output_directory_is_never_overwritten(self):
        self.fixture.valid_manifest()
        self.fixture.output_dir.mkdir()
        sentinel = self.fixture.output_dir / "keep.txt"
        sentinel.write_text("keep", encoding="utf-8")
        self.assertEqual(self.fixture.compile(), 1)
        self.assertEqual(sentinel.read_text(encoding="utf-8"), "keep")
        self.assertEqual({path.name for path in self.fixture.output_dir.iterdir()}, {"keep.txt"})
        self.assertEqual(self.fixture.status_path.read_bytes(), self.fixture.original_status)

    def test_output_directory_symlink_is_never_followed(self):
        self.fixture.valid_manifest()
        target = self.fixture.root / "symlink-target"
        target.mkdir()
        sentinel = target / "keep.txt"
        sentinel.write_text("keep", encoding="utf-8")
        os.symlink(str(target), str(self.fixture.output_dir))
        self.assertEqual(self.fixture.compile(), 1)
        self.assertTrue(self.fixture.output_dir.is_symlink())
        self.assertEqual(sentinel.read_text(encoding="utf-8"), "keep")
        self.assertEqual({path.name for path in target.iterdir()}, {"keep.txt"})
        self.assertEqual(self.fixture.status_path.read_bytes(), self.fixture.original_status)

    def test_partial_pack_write_is_removed_through_the_open_directory(self):
        self.fixture.valid_manifest()
        original_write = compiler.os.write
        calls = 0

        def partial_then_fail(descriptor, data):
            nonlocal calls
            calls += 1
            if calls == 1:
                return original_write(descriptor, data[:10])
            raise OSError("forced pack write failure")

        with mock.patch.object(compiler.os, "write", side_effect=partial_then_fail):
            self.assertEqual(self.fixture.compile(), 1)
        self.assertGreaterEqual(calls, 2)
        self.assertFalse(os.path.lexists(str(self.fixture.output_dir)))
        self.assertEqual(
            self.fixture.status_path.read_bytes(), self.fixture.original_status
        )


if __name__ == "__main__":
    unittest.main()
