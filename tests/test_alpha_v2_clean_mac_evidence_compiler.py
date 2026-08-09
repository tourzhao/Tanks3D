#!/usr/bin/env python3
"""Tests for the source-free Alpha-v2 clean-Mac evidence compiler."""

from datetime import datetime, timezone
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import plistlib
import shutil
import sys
import tempfile
import unittest
from unittest import mock

from media_recording_fixture import recording


REPOSITORY_ROOT = Path(__file__).resolve().parents[1]
SCRIPT = REPOSITORY_ROOT / "scripts" / "compile_alpha_v2_clean_mac_evidence.py"
SPEC = importlib.util.spec_from_file_location("alpha_v2_clean_mac_compiler", SCRIPT)
assert SPEC is not None and SPEC.loader is not None
compiler = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = compiler
SPEC.loader.exec_module(compiler)
VERIFIER_SCRIPT = REPOSITORY_ROOT / "scripts" / "verify_release_status.py"
VERIFIER_SPEC = importlib.util.spec_from_file_location(
    "alpha_v2_clean_mac_release_verifier", VERIFIER_SCRIPT
)
assert VERIFIER_SPEC is not None and VERIFIER_SPEC.loader is not None
verifier = importlib.util.module_from_spec(VERIFIER_SPEC)
sys.modules[VERIFIER_SPEC.name] = verifier
VERIFIER_SPEC.loader.exec_module(verifier)


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def write_json(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(
        json.dumps(value, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )


def write_plist(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(plistlib.dumps(value, fmt=plistlib.FMT_XML, sort_keys=False))


def epoch_hex(timestamp):
    parsed = datetime.strptime(timestamp, "%Y-%m-%dT%H:%M:%SZ").replace(
        tzinfo=timezone.utc
    )
    return format(int(parsed.timestamp()), "x")


class CompilerFixture:
    def __init__(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name).resolve()
        self.requirements_path = (
            self.root / "docs/release-requirements/macos-alpha-v2.json"
        )
        self.requirements_path.parent.mkdir(parents=True, exist_ok=True)
        self.requirements_path.write_bytes(
            (
                REPOSITORY_ROOT
                / "docs/release-requirements/macos-alpha-v2.json"
            ).read_bytes()
        )
        self.profile = json.loads(self.requirements_path.read_text(encoding="utf-8"))
        self.filename = (
            "Tanks3D-0.1.0-alpha.4-macos-arm64-macos26.0.zip"
        )
        self.candidate = self.root / "build/release/v0.1.0-alpha.4" / self.filename
        self.candidate.parent.mkdir(parents=True, exist_ok=True)
        self.candidate.write_bytes(b"fixture candidate archive\n")
        self.candidate_sha256 = digest(self.candidate)
        self.status_path = self.root / "docs/releases/v0.1.0-alpha.4-status.json"
        empty_interactive = {
            "candidate_sha256": "",
            "tester": "",
            "machine": "",
            "tested_at_utc": None,
            "signature": "",
            "coverage_refs": {},
        }
        self.status = {
            "requirements": "docs/release-requirements/macos-alpha-v2.json",
            "release": {
                "tag": "v0.1.0-alpha.4",
                "artifact": {
                    "path": self.candidate.relative_to(self.root).as_posix(),
                    "sha256": self.candidate_sha256,
                },
            },
            "clean_mac": {
                "status": "NOT_RUN",
                "tester": "",
                "tested_at_utc": None,
                "evidence_ids": [],
                "checks_confirmed": [],
                "details": {key: "" for key in self.profile["clean_mac_detail_keys"]},
                "notes": "Clean-Mac test not run.",
            },
            "gatekeeper": {
                "status": "BLOCKED",
                "tester": "",
                "tested_at_utc": None,
                "evidence_ids": [],
                "checks_confirmed": [],
                "details": {
                    key: "" for key in self.profile["gatekeeper_detail_keys"]
                },
                "notes": "Gatekeeper test not run.",
            },
            "evidence": [
                {
                    "id": "gatekeeper_launch",
                    "status": "NOT_RUN",
                    "artifacts": [],
                    "reviewer": "",
                    "reviewed_at_utc": None,
                    "interactive": empty_interactive,
                    "notes": "Gatekeeper launch not observed.",
                }
            ],
        }
        write_json(self.status_path, self.status)
        self.original_status = self.status_path.read_bytes()
        self.intake_dir = self.root / "raw-intake"
        self.intake_dir.mkdir()
        self.url = "https://github.com/tourzhao/tanks3d/releases/download/v0.1.0-alpha.4/{}".format(
            self.filename
        )
        self.quarantine_time = "2020-01-01T16:31:30Z"
        self.quarantine_hex = epoch_hex(self.quarantine_time)
        self.where_file = self.intake_dir / "where-froms.hex"
        where_plist = plistlib.dumps([self.url], fmt=plistlib.FMT_BINARY)
        self.where_file.write_text(where_plist.hex() + "\n", encoding="ascii")
        self.plan = {
            "schema": compiler.PLAN_SCHEMA,
            "requirements_profile": compiler.REQUIREMENTS_PROFILE,
            "candidate_tag": "v0.1.0-alpha.4",
            "candidate_filename": self.filename,
            "candidate_sha256": self.candidate_sha256,
            "download_url": self.url,
            "minimum_macos_version": "26.0",
            "collector_sha256": self.profile.get(
                "clean_mac_collector_sha256", "0" * 64
            ),
            "prepared_at_utc": "2020-01-01T16:29:00Z",
            "session_nonce": "1" * 32,
        }
        self.plan_path = self.intake_dir / "clean-mac-plan.plist"
        write_plist(self.plan_path, self.plan)
        command_argv = compiler.expected_argv(self.filename)
        command_outputs = {
            "checksum": (
                0,
                "{}  {}\n".format(self.candidate_sha256, self.filename),
                "",
            ),
            "zip_quarantine": (
                0,
                "0083;{};Safari;fixture\n".format(self.quarantine_hex),
                "",
            ),
            "app_quarantine": (
                0,
                "0083;{};Archive Utility;fixture\n".format(
                    self.quarantine_hex
                ),
                "",
            ),
            "codesign": (0, "", ""),
            "spctl": (
                1,
                "",
                "Tanks3D.app: rejected\nsource=Unnotarized Developer ID\n",
            ),
        }
        commands = []
        for index, command_id in enumerate(compiler.COMMAND_IDS):
            exit_code, stdout, stderr = command_outputs[command_id]
            commands.append(
                {
                    "id": command_id,
                    "argv": command_argv[command_id],
                    "exit_code": exit_code,
                    "stdout": stdout,
                    "stderr": stderr,
                    "started_at_utc": "2020-01-01T16:{:02d}:00Z".format(
                        33 + index
                    ),
                    "completed_at_utc": "2020-01-01T16:{:02d}:30Z".format(
                        33 + index
                    ),
                }
            )
        self.intake = {
            "schema": compiler.INTAKE_SCHEMA,
            "plan_sha256": digest(self.plan_path),
            "session_nonce": self.plan["session_nonce"],
            "collector_sha256": self.plan["collector_sha256"],
            "candidate_filename": self.filename,
            "candidate_sha256": self.candidate_sha256,
            "download_url": self.url,
            "tester": "Clean Mac Tester",
            "tester_signature": "Clean Mac Tester",
            "machine": "MacBookPro18,3 arm64",
            "machine_details": {
                "mac_model": "MacBookPro18,3",
                "chip": "Apple M3 Pro",
                "uname_machine": "arm64",
                "ram": "18 GB",
                "macos_version": "26.5",
                "macos_build": "25F84",
                "clean_machine_method": "Fresh local account with no prior install",
                "prior_app_absent": "yes",
                "prior_approval_absent": "yes",
                "minimum_macos_met": "yes",
                "source_checkout_absent": "yes",
                "homebrew_raylib_unused": "yes",
            },
            "session_started_at_utc": "2020-01-01T16:30:00Z",
            "session_completed_at_utc": "2020-01-01T16:40:00Z",
            "acquisition": {
                "client": "Safari",
                "started_at_utc": "2020-01-01T16:31:00Z",
                "completed_at_utc": "2020-01-01T16:32:00Z",
                "zip_quarantine_agent": "Safari",
                "quarantine_timestamp_utc": self.quarantine_time,
                "where_froms_url": self.url,
                "where_froms_sha256": digest(self.where_file),
            },
            "commands": commands,
            "observations": {
                "first_finder_launch": "Double-clicked Tanks3D.app in Finder",
                "dialog_text": "macOS reported the app could not be verified",
                "documented_launch_path": (
                    "Finder launch, Privacy & Security, Open Anyway, then Open"
                ),
                "main_menu_reached": "yes",
                "signature_preserved": "yes",
                "conclusion": "PASS",
            },
            "notes": "Quarantined Finder launch reached the main menu.",
            "complete": True,
            "test_mode": False,
        }
        self.intake_path = self.intake_dir / "clean-mac-intake.plist"
        self.write_intake()
        self.write_raw_transcripts()
        (self.intake_dir / "COMPLETE").write_bytes(b"")
        self.media = self.root / "gatekeeper-launch.mov"
        self.media.write_bytes(recording())
        self.output_dir = self.root / "docs/assets/releases/v0.1.0-alpha.4/evidence/clean-mac"
        self.output_dir.parent.mkdir(parents=True, exist_ok=True)

    def write_intake(self):
        self.intake["plan_sha256"] = digest(self.plan_path)
        self.intake["acquisition"]["where_froms_sha256"] = digest(self.where_file)
        write_plist(self.intake_path, self.intake)

    def write_raw_transcripts(self):
        for command in self.intake["commands"]:
            prefix = command["id"].replace("_", "-")
            for stream in ("stdout", "stderr"):
                (self.intake_dir / "{}.{}".format(prefix, stream)).write_text(
                    command[stream], encoding="utf-8"
                )

    def write_plan(self):
        write_plist(self.plan_path, self.plan)
        self.write_intake()

    def set_url(self, url):
        self.url = url
        self.plan["download_url"] = url
        self.intake["download_url"] = url
        self.intake["acquisition"]["where_froms_url"] = url
        where_plist = plistlib.dumps([url], fmt=plistlib.FMT_BINARY)
        self.where_file.write_text(where_plist.hex() + "\n", encoding="ascii")
        self.write_plan()

    def compile(self, **overrides):
        values = {
            "reviewer": "Independent Reviewer",
            "reviewer_signature": "Independent Reviewer",
            "reviewed_at_utc": "2020-01-01T16:50:00Z",
            "review_notes": "Raw Safari, command, and launch evidence reviewed.",
            "release_note_wording_verified": "yes",
            "media": str(self.media),
            "output_dir": str(self.output_dir),
        }
        values.update(overrides)
        return compiler.main(
            [
                "--project-root",
                str(self.root),
                "--status",
                str(self.status_path),
                "--intake-dir",
                str(self.intake_dir),
                "--media",
                values["media"],
                "--reviewer",
                values["reviewer"],
                "--reviewer-signature",
                values["reviewer_signature"],
                "--reviewed-at-utc",
                values["reviewed_at_utc"],
                "--review-notes",
                values["review_notes"],
                "--release-note-wording-verified",
                values["release_note_wording_verified"],
                "--output-dir",
                values["output_dir"],
            ]
        )

    def close(self):
        self.temporary.cleanup()


class CleanMacEvidenceCompilerTests(unittest.TestCase):
    def setUp(self):
        self.fixture = CompilerFixture()

    def tearDown(self):
        self.fixture.close()

    def test_compiles_candidate_bound_evidence_without_changing_source(self):
        self.assertEqual(self.fixture.compile(), 0)
        self.assertEqual(self.fixture.status_path.read_bytes(), self.fixture.original_status)
        expected_files = {
            "clean-mac-plan.plist",
            "clean-mac-intake.plist",
            "where-froms.hex",
            "browser-acquisition.json",
            "command-log.json",
            "clean-mac-compiler-receipt.json",
            "gatekeeper-launch-session.json",
            "gatekeeper-launch-recording.mov",
            "status.next.json",
        }
        expected_files.update(compiler.RAW_COMMAND_FILENAMES)
        self.assertEqual(
            {path.name for path in self.fixture.output_dir.iterdir()},
            expected_files,
        )
        status = json.loads(
            (self.fixture.output_dir / "status.next.json").read_text(encoding="utf-8")
        )
        self.assertEqual(status["clean_mac"]["status"], "PASS")
        self.assertEqual(status["gatekeeper"]["status"], "PASS")
        evidence = status["evidence"][0]
        self.assertEqual(evidence["status"], "PASS")
        self.assertEqual(
            set(evidence["interactive"]["coverage_refs"]),
            {"gate:clean_mac", "gate:gatekeeper"},
        )
        self.assertEqual(len(evidence["artifacts"]), 18)
        artifact_map = {
            artifact["path"]: (
                artifact["path"],
                artifact["sha256"],
                artifact["kind"],
                self.fixture.root / artifact["path"],
            )
            for artifact in evidence["artifacts"]
        }
        verifier.validate_clean_mac_command_log(
            status,
            {"gatekeeper_launch": evidence},
            artifact_map,
            self.fixture.filename,
            self.fixture.candidate_sha256,
            "macos-alpha-v2",
        )

    def test_test_mode_and_incomplete_intakes_are_rejected(self):
        for key, value in (("test_mode", True), ("complete", False)):
            with self.subTest(key=key):
                fixture = CompilerFixture()
                try:
                    fixture.intake[key] = value
                    fixture.write_intake()
                    self.assertEqual(fixture.compile(), 1)
                    self.assertFalse(fixture.output_dir.exists())
                finally:
                    fixture.close()

    def test_candidate_plan_and_collector_identity_are_exact(self):
        mutations = (
            lambda fixture: fixture.plan.__setitem__("candidate_sha256", "f" * 64),
            lambda fixture: fixture.plan.__setitem__("collector_sha256", "f" * 64),
            lambda fixture: fixture.intake.__setitem__("session_nonce", "2" * 32),
        )
        for mutation in mutations:
            fixture = CompilerFixture()
            try:
                mutation(fixture)
                fixture.write_plan()
                self.assertEqual(fixture.compile(), 1)
            finally:
                fixture.close()

    def test_safari_quarantine_time_and_where_froms_are_verified(self):
        mutations = (
            lambda fixture: fixture.intake["commands"][1].__setitem__(
                "stdout",
                "0083;{};ManualWriter;fixture\n".format(fixture.quarantine_hex),
            ),
            lambda fixture: fixture.intake["acquisition"].__setitem__(
                "quarantine_timestamp_utc", "2020-01-01T16:30:00Z"
            ),
            lambda fixture: fixture.where_file.write_text(
                plistlib.dumps(
                    ["https://github.com/wrong/file.zip"], fmt=plistlib.FMT_BINARY
                ).hex()
                + "\n",
                encoding="ascii",
            ),
        )
        for mutation in mutations:
            fixture = CompilerFixture()
            try:
                mutation(fixture)
                fixture.write_intake()
                fixture.write_raw_transcripts()
                self.assertEqual(fixture.compile(), 1)
            finally:
                fixture.close()

    def test_command_order_absolute_argv_and_spctl_outcome_are_exact(self):
        mutations = (
            lambda fixture: fixture.intake["commands"][0]["argv"].__setitem__(
                0, "shasum"
            ),
            lambda fixture: fixture.intake["commands"].__setitem__(
                slice(0, 2), list(reversed(fixture.intake["commands"][:2]))
            ),
            lambda fixture: fixture.intake["commands"][4].__setitem__(
                "stdout", "accepted and rejected"
            ),
        )
        for mutation in mutations:
            fixture = CompilerFixture()
            try:
                mutation(fixture)
                fixture.write_intake()
                fixture.write_raw_transcripts()
                self.assertEqual(fixture.compile(), 1)
            finally:
                fixture.close()

    def test_checksum_output_is_exact(self):
        mutations = (
            lambda fixture: fixture.intake["commands"][0].__setitem__(
                "stdout",
                "{}  {}\ntrailing text\n".format(
                    fixture.candidate_sha256, fixture.filename
                ),
            ),
            lambda fixture: fixture.intake["commands"][0].__setitem__(
                "stderr", "unexpected warning\n"
            ),
        )
        for mutation in mutations:
            with self.subTest(mutation=mutation):
                fixture = CompilerFixture()
                try:
                    mutation(fixture)
                    fixture.write_intake()
                    fixture.write_raw_transcripts()
                    self.assertEqual(fixture.compile(), 1)
                finally:
                    fixture.close()

    def test_raw_command_transcripts_must_match_the_intake(self):
        transcript = self.fixture.intake_dir / "spctl.stderr"
        transcript.write_text("substituted assessment\n", encoding="utf-8")
        self.assertEqual(self.fixture.compile(), 1)
        self.assertFalse(self.fixture.output_dir.exists())

    def test_intake_file_set_and_transcript_types_are_fail_closed(self):
        mutations = (
            lambda fixture: (fixture.intake_dir / "checksum.stdout").unlink(),
            lambda fixture: (fixture.intake_dir / "unexpected.txt").write_text(
                "unexpected", encoding="utf-8"
            ),
            lambda fixture: (fixture.intake_dir / "codesign.stderr").write_bytes(
                b"\xff"
            ),
        )
        for mutation in mutations:
            with self.subTest(mutation=mutation):
                fixture = CompilerFixture()
                try:
                    mutation(fixture)
                    self.assertEqual(fixture.compile(), 1)
                    self.assertFalse(fixture.output_dir.exists())
                finally:
                    fixture.close()

    def test_human_review_and_media_are_fail_closed(self):
        self.assertEqual(
            self.fixture.compile(
                reviewer="Clean Mac Tester",
                reviewer_signature="Clean Mac Tester",
            ),
            1,
        )
        fixture = CompilerFixture()
        try:
            fixture.media.write_bytes(b"too small")
            self.assertEqual(fixture.compile(), 1)
        finally:
            fixture.close()
        fixture = CompilerFixture()
        try:
            fixture.media.write_bytes(
                b"\x89PNG\r\n\x1a\n".ljust(64 * 1024, b"0")
            )
            self.assertEqual(fixture.compile(), 1)
        finally:
            fixture.close()
        fixture = CompilerFixture()
        try:
            with fixture.media.open("wb") as stream:
                stream.truncate(compiler.MAXIMUM_MEDIA_BYTES + 1)
            self.assertEqual(fixture.compile(), 1)
        finally:
            fixture.close()

    def test_private_or_placeholder_download_hosts_are_rejected(self):
        for host in (
            "release.local",
            "release.internal",
            "release.lan",
            "release.onion",
            "fixture.github.com",
            "placeholder.github.com",
        ):
            with self.subTest(host=host):
                fixture = CompilerFixture()
                try:
                    fixture.set_url(
                        "https://{}/{}".format(host, fixture.filename)
                    )
                    self.assertEqual(fixture.compile(), 1)
                    self.assertFalse(fixture.output_dir.exists())
                finally:
                    fixture.close()

    def test_existing_output_and_symlinked_intake_file_are_never_followed(self):
        self.fixture.output_dir.mkdir()
        sentinel = self.fixture.output_dir / "keep.txt"
        sentinel.write_text("keep", encoding="utf-8")
        self.assertEqual(self.fixture.compile(), 1)
        self.assertEqual(sentinel.read_text(encoding="utf-8"), "keep")
        fixture = CompilerFixture()
        try:
            original = fixture.intake_path.with_suffix(".real")
            fixture.intake_path.rename(original)
            os.symlink(str(original), str(fixture.intake_path))
            self.assertEqual(fixture.compile(), 1)
        finally:
            fixture.close()

    def test_partial_output_is_rolled_back(self):
        original_write = compiler.shared.os.write
        calls = 0

        def partial_then_fail(descriptor, data):
            nonlocal calls
            calls += 1
            if calls == 1:
                return original_write(descriptor, data[:10])
            raise OSError("forced clean-Mac pack write failure")

        with mock.patch.object(
            compiler.shared.os, "write", side_effect=partial_then_fail
        ):
            self.assertEqual(self.fixture.compile(), 1)
        self.assertFalse(os.path.lexists(str(self.fixture.output_dir)))
        self.assertEqual(self.fixture.status_path.read_bytes(), self.fixture.original_status)

    def test_candidate_change_during_compilation_is_rejected(self):
        original_validate = compiler.validate_source_status

        def validate_then_replace(root, status):
            result = original_validate(root, status)
            self.fixture.candidate.write_bytes(b"replacement candidate\n")
            return result

        with mock.patch.object(
            compiler,
            "validate_source_status",
            side_effect=validate_then_replace,
        ):
            self.assertEqual(self.fixture.compile(), 1)
        self.assertFalse(self.fixture.output_dir.exists())


if __name__ == "__main__":
    unittest.main()
