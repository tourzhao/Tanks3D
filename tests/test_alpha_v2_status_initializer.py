#!/usr/bin/env python3
"""Contract tests for the one-shot Alpha-v2 BLOCKED baseline initializer."""

import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import stat
import struct
import subprocess
import sys
import tempfile
import unittest
from unittest import mock
import zlib


REPOSITORY_ROOT = Path(__file__).resolve().parents[1]
INITIALIZER = REPOSITORY_ROOT / "scripts/init_alpha_v2_status.py"
STATUS_VERIFIER = REPOSITORY_ROOT / "scripts/verify_release_status.py"
REQUIREMENTS = (
    REPOSITORY_ROOT / "docs/release-requirements/macos-alpha-v2.json"
)
TAG = "v0.2.0-alpha.4"
VERSION = "0.2.0"
CHANNEL = "alpha.4"
COMMIT = "a" * 40
SCREENSHOT_NAMES = (
    "one-player.png",
    "two-player.png",
    "base-usa.png",
    "base-ussr.png",
    "base-germany.png",
    "bonuses.png",
    "settlement.png",
)


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def png_chunk(kind, payload):
    checksum = zlib.crc32(kind)
    checksum = zlib.crc32(payload, checksum) & 0xFFFFFFFF
    return struct.pack(">I", len(payload)) + kind + payload + struct.pack(">I", checksum)


def png_bytes(seed, width=1280, height=720):
    pixel = bytes((seed & 0xFF, (seed * 3) & 0xFF, (seed * 7) & 0xFF, 255))
    row = pixel * width
    rows = (b"\x00" + row) * height
    result = b"\x89PNG\r\n\x1a\n"
    result += png_chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0))
    result += png_chunk(b"IDAT", zlib.compress(rows, 9))
    result += png_chunk(b"IEND", b"")
    return result


def load_initializer_module():
    specification = importlib.util.spec_from_file_location(
        "tanks3d_alpha_v2_initializer", INITIALIZER
    )
    module = importlib.util.module_from_spec(specification)
    specification.loader.exec_module(module)
    return module


INITIALIZER_MODULE = load_initializer_module()


class InitializerFixture:
    def __init__(self, base):
        self.root = Path(base)
        self.candidate = self.root / "build/release" / TAG
        self.screenshots = self.root / "build/release-evidence" / TAG / "screenshots"
        self.asset_destination = self.root / "docs/assets/releases" / TAG
        self.release_page = self.root / "docs/releases" / "{}.md".format(TAG)
        self.qa_report = self.root / "docs/releases" / "{}-qa.md".format(TAG)
        self.status_path = self.root / "docs/releases" / "{}-status.json".format(TAG)
        self._write_layout()
        self._write_candidate()
        self._write_screenshots()

    def _write_layout(self):
        (self.root / "scripts").mkdir(parents=True)
        (self.root / "docs/release-requirements").mkdir(parents=True)
        (self.root / "docs/assets/releases/v0.1.0-alpha.3").mkdir(parents=True)
        (self.root / "docs/releases").mkdir(parents=True)
        (self.root / "docs/release-requirements/macos-alpha-v2.json").write_bytes(
            REQUIREMENTS.read_bytes()
        )
        verifier = self.root / "scripts/verify_tagged_alpha_candidate.sh"
        verifier.write_text(
            "#!/bin/sh\n"
            "for candidate_file in \"$2\"/*; do\n"
            "  name=${candidate_file##*/}\n"
            "  value=$(shasum -a 256 \"$candidate_file\" | awk '{print $1}')\n"
            "  printf 'VERIFIED CANDIDATE FILE SHA256 %s %s\\n' \"$value\" \"$name\"\n"
            "done\n"
            "exit 0\n",
            encoding="utf-8",
        )

    def _write_candidate(self):
        self.candidate.mkdir(parents=True)
        artifact_name = "Tanks3D-0.2.0-alpha.4-macos-arm64-macos13.0.zip"
        artifact = self.candidate / artifact_name
        checksum = self.candidate / (artifact_name + ".sha256")
        gate_log = self.candidate / "alpha-candidate-gates.log"
        build_config = self.candidate / "build-config.txt"
        attestation = self.candidate / "attestation.txt"
        artifact.write_bytes(b"fixture candidate ZIP\n")
        gate_log.write_text("fixture gates\n", encoding="utf-8")
        build_config.write_text(
            "fixture=true\n"
            "performance-capability-schema=tanks3d-release-performance-capabilities-v1\n"
            "performance-telemetry-schema=tanks3d-performance-log-v2\n"
            "performance-capability-contract-sha256="
            "5137950da46fa11ee6d5ff60fafe67e83c4c0aacfb5fc83f2b0ce5f74afcfe1c\n",
            encoding="utf-8",
        )
        artifact_hash = digest(artifact)
        checksum.write_text(
            "{}  {}\n".format(artifact_hash, artifact_name), encoding="utf-8"
        )
        values = {
            "schema": "tanks3d-alpha-candidate-v3",
            "source_commit": COMMIT,
            "source_head_at_start": COMMIT,
            "source_head_at_finish": COMMIT,
            "source_tag": TAG,
            "source_tag_commit": COMMIT,
            "source_tree": "clean",
            "app_version": VERSION,
            "dist_channel": CHANNEL,
            "dist_arch": "arm64",
            "dist_macos_min": "13.0",
            "artifact_filename": artifact_name,
            "artifact_sha256": artifact_hash,
            "checksum_filename": checksum.name,
            "build_config_filename": build_config.name,
            "build_config_sha256": digest(build_config),
            "gate_log_filename": gate_log.name,
            "gate_log_sha256": digest(gate_log),
            "gate_clean": "PASS",
            "gate_test_alpha_candidate": "PASS",
            "gate_debug": "PASS",
            "gate_test_architecture": "PASS",
            "gate_test": "PASS",
            "gate_test_sanitize": "PASS",
            "gate_coverage": "PASS",
            "gate_test_dist": "PASS",
        }
        attestation.write_text(
            "".join("{}={}\n".format(key, value) for key, value in values.items()),
            encoding="utf-8",
        )

    def _write_screenshots(self):
        self.screenshots.mkdir(parents=True)
        for index, name in enumerate(SCREENSHOT_NAMES, 1):
            (self.screenshots / name).write_bytes(png_bytes(index * 17))

    def run(self, screenshot_dir=None):
        return subprocess.run(
            [
                sys.executable,
                "-B",
                str(INITIALIZER),
                "--project-root",
                str(self.root),
                "--candidate-dir",
                str(self.candidate),
                "--screenshot-input-dir",
                str(screenshot_dir or self.screenshots),
            ],
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            check=False,
            timeout=15,
        )

    def replace_build_config(self, text):
        build_config = self.candidate / "build-config.txt"
        build_config.write_text(text, encoding="utf-8")
        attestation = self.candidate / "attestation.txt"
        attestation_text = attestation.read_text(encoding="utf-8")
        attestation_text = re.sub(
            r"^build_config_sha256=.*$",
            "build_config_sha256={}".format(digest(build_config)),
            attestation_text,
            flags=re.MULTILINE,
        )
        attestation.write_text(attestation_text, encoding="utf-8")

    def assert_no_outputs(self, testcase):
        testcase.assertFalse(self.asset_destination.exists())
        testcase.assertFalse(self.release_page.exists())
        testcase.assertFalse(self.qa_report.exists())
        testcase.assertFalse(self.status_path.exists())


class AlphaV2StatusInitializerTests(unittest.TestCase):
    def fixture(self):
        temporary = tempfile.TemporaryDirectory(prefix="tanks3d-v2-init-test-")
        self.addCleanup(temporary.cleanup)
        return InitializerFixture(temporary.name)

    def assert_failed(self, fixture, result, fragment):
        self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn(fragment, result.stderr)
        fixture.assert_no_outputs(self)

    def test_success_creates_honest_baseline_accepted_by_blocked_verifier(self):
        fixture = self.fixture()
        result = fixture.run()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertTrue(fixture.status_path.is_file())
        status = json.loads(fixture.status_path.read_text(encoding="utf-8"))
        self.assertEqual(status["requirements"], "docs/release-requirements/macos-alpha-v2.json")
        self.assertEqual(status["release"]["tag"], TAG)
        self.assertEqual(status["release"]["source_commit"], COMMIT)
        for section in ("gameplay", "bases", "pickups", "settlement", "evidence"):
            self.assertEqual(
                {item["status"] for item in status[section]},
                {"NOT_RUN"},
                section,
            )
        self.assertEqual(status["published_controls"]["status"], "NOT_RUN")
        self.assertEqual(status["clean_mac"]["status"], "NOT_RUN")
        self.assertEqual(status["gatekeeper"]["status"], "BLOCKED")
        self.assertEqual(status["extended_session"]["status"], "NOT_RUN")
        self.assertEqual(status["known_issues"]["status"], "NOT_RUN")
        self.assertIsNone(status["report"]["completed_at_utc"])
        self.assertIsNone(status["report"]["release_date"])
        self.assertEqual(status["audio"]["decision"], "NONE")
        self.assertEqual([item["status"] for item in status["approvals"]], ["BLOCKED", "BLOCKED"])
        for name in SCREENSHOT_NAMES:
            copied = fixture.asset_destination / name
            self.assertEqual(copied.read_bytes(), (fixture.screenshots / name).read_bytes())
        page = fixture.release_page.read_text(encoding="utf-8")
        qa = fixture.qa_report.read_text(encoding="utf-8")
        self.assertIn("**Release status: BLOCKED.**", page)
        self.assertIn("**Overall Alpha gate: BLOCKED.**", qa)
        self.assertIn("Author: **tourzhao**", page)
        self.assertIn("Gatekeeper conclusion: **BLOCKED**", page)
        self.assertIn(
            "[PolyForm Noncommercial License 1.0.0](../../LICENSE)", page
        )
        self.assertIn("statically links raylib 6.0 and its embedded dependencies", page)
        self.assertIn("[`LICENSES/`](../../LICENSES/)", page)
        self.assertIn(
            "Required notice: `Required Notice: Copyright (c) 2026 tourzhao.`",
            page,
        )
        self.assertIn("Selected option: **NONE — BLOCKED**", qa)
        self.assertIn(fixture.qa_report.name, page)
        requirements = json.loads(REQUIREMENTS.read_text(encoding="utf-8"))
        self.assertEqual(
            requirements["clean_mac_evidence_ids"], ["gatekeeper_launch"]
        )
        self.assertEqual(
            requirements["interactive_observation_evidence_ids"],
            [
                "main_menu_and_advanced_settings",
                "one_player_gameplay",
                "two_player_gameplay",
                "national_bases",
                "pickup_and_minimap",
                "settlement_report",
            ],
        )
        self.assertEqual(
            [
                (
                    item["context"],
                    item["coverage_token"],
                    item["evidence_id"],
                )
                for item in requirements[
                    "published_control_context_requirements"
                ]
            ],
            [
                (
                    "main_menu",
                    "controls:main_menu",
                    "main_menu_and_advanced_settings",
                ),
                ("one_player", "controls:one_player", "one_player_gameplay"),
                ("two_player", "controls:two_player", "two_player_gameplay"),
            ],
        )
        self.assertEqual(
            set().union(
                *(
                    set(item["checks"])
                    for item in requirements[
                        "published_control_context_requirements"
                    ]
                )
            ),
            set(
                requirements["published_control_checks"]
                + requirements["advanced_settings_checks"]
            ),
        )
        for row in requirements["document_gate_rows"]:
            gate_row = "| {} | BLOCKED |".format(row)
            self.assertIn(gate_row, page)
            self.assertIn(gate_row, qa)
        for name in SCREENSHOT_NAMES:
            reference = "../assets/releases/{}/{}".format(TAG, name)
            self.assertIn(reference, page)
            self.assertIn(reference, qa)
            self.assertIn(digest(fixture.asset_destination / name), qa)

        verification = subprocess.run(
            [
                sys.executable,
                "-B",
                str(STATUS_VERIFIER),
                "--allow-blocked",
                "--project-root",
                str(fixture.root),
                "--status",
                str(fixture.status_path),
            ],
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            check=False,
        )
        self.assertEqual(
            verification.returncode, 0, verification.stdout + verification.stderr
        )
        self.assertIn("Verified blocked Alpha status", verification.stdout)

    def test_refuses_missing_or_extra_screenshot_before_writing(self):
        for mutation in ("missing", "extra"):
            with self.subTest(mutation=mutation):
                fixture = self.fixture()
                if mutation == "missing":
                    (fixture.screenshots / SCREENSHOT_NAMES[0]).unlink()
                else:
                    (fixture.screenshots / "extra.png").write_bytes(png_bytes(233))
                self.assert_failed(fixture, fixture.run(), "exactly seven canonical PNGs")

    def test_refuses_wrong_size_crc_stream_and_symlink_screenshots(self):
        mutations = (
            "size",
            "crc",
            "stream",
            "split_idat",
            "critical",
            "symlink",
            "fifo",
        )
        for mutation in mutations:
            with self.subTest(mutation=mutation):
                fixture = self.fixture()
                target = fixture.screenshots / SCREENSHOT_NAMES[0]
                if mutation == "size":
                    target.write_bytes(png_bytes(8, width=640, height=360))
                    fragment = "must be 1280x720"
                elif mutation == "crc":
                    data = bytearray(target.read_bytes())
                    data[-5] ^= 0x01
                    target.write_bytes(data)
                    fragment = "invalid PNG chunk CRC"
                elif mutation == "stream":
                    data = b"\x89PNG\r\n\x1a\n"
                    data += png_chunk(
                        b"IHDR", struct.pack(">IIBBBBB", 1280, 720, 8, 6, 0, 0, 0)
                    )
                    data += png_chunk(b"IDAT", b"not-a-zlib-stream")
                    data += png_chunk(b"IEND", b"")
                    target.write_bytes(data)
                    fragment = "invalid PNG image stream"
                elif mutation == "split_idat":
                    pixel = bytes((8, 24, 56, 255))
                    rows = (b"\x00" + pixel * 1280) * 720
                    compressed = zlib.compress(rows, 9)
                    midpoint = len(compressed) // 2
                    data = b"\x89PNG\r\n\x1a\n"
                    data += png_chunk(
                        b"IHDR", struct.pack(">IIBBBBB", 1280, 720, 8, 6, 0, 0, 0)
                    )
                    data += png_chunk(b"IDAT", compressed[:midpoint])
                    data += png_chunk(b"tEXt", b"separator")
                    data += png_chunk(b"IDAT", compressed[midpoint:])
                    data += png_chunk(b"IEND", b"")
                    target.write_bytes(data)
                    fragment = "non-contiguous IDAT chunks"
                elif mutation == "critical":
                    data = target.read_bytes()
                    target.write_bytes(data[:33] + png_chunk(b"ABCD", b"") + data[33:])
                    fragment = "unknown critical PNG chunk"
                elif mutation == "symlink":
                    replacement = fixture.screenshots.parent / "replacement.png"
                    replacement.write_bytes(png_bytes(244))
                    target.unlink()
                    target.symlink_to(replacement)
                    fragment = "cannot open screenshot"
                else:
                    target.unlink()
                    os.mkfifo(target)
                    fragment = "regular non-symlink file"
                self.assert_failed(fixture, fixture.run(), fragment)

    def test_refuses_duplicate_images(self):
        fixture = self.fixture()
        (fixture.screenshots / SCREENSHOT_NAMES[1]).write_bytes(
            (fixture.screenshots / SCREENSHOT_NAMES[0]).read_bytes()
        )
        self.assert_failed(fixture, fixture.run(), "must show seven distinct images")

    def test_refuses_candidate_tamper_and_any_profile_drift(self):
        for mutation in (
            "candidate",
            "profile_schema",
            "profile_semantics",
            "profile_control_context",
        ):
            with self.subTest(mutation=mutation):
                fixture = self.fixture()
                if mutation == "candidate":
                    artifact = next(fixture.candidate.glob("*.zip"))
                    artifact.write_bytes(b"tampered\n")
                    fragment = "artifact_sha256 does not match"
                elif mutation == "profile_schema":
                    profile_path = fixture.root / "docs/release-requirements/macos-alpha-v2.json"
                    profile = json.loads(profile_path.read_text(encoding="utf-8"))
                    profile["schema"] = "tanks3d-release-requirements-v1"
                    profile_path.write_text(json.dumps(profile), encoding="utf-8")
                    fragment = "does not match the canonical contract"
                elif mutation == "profile_semantics":
                    profile_path = fixture.root / "docs/release-requirements/macos-alpha-v2.json"
                    profile = json.loads(profile_path.read_text(encoding="utf-8"))
                    profile["gameplay_ids"][0] = "forged_gameplay_check"
                    profile_path.write_text(json.dumps(profile), encoding="utf-8")
                    fragment = "does not match the canonical contract"
                else:
                    profile_path = fixture.root / "docs/release-requirements/macos-alpha-v2.json"
                    profile = json.loads(profile_path.read_text(encoding="utf-8"))
                    profile["published_control_context_requirements"][0][
                        "coverage_token"
                    ] = "controls:forged"
                    profile_path.write_text(json.dumps(profile), encoding="utf-8")
                    fragment = "does not match the canonical contract"
                self.assert_failed(fixture, fixture.run(), fragment)

    def test_refuses_legacy_or_drifted_performance_capability_contract(self):
        cases = (
            ("fixture=true\n", "performance-capability-schema"),
            (
                "fixture=true\n"
                "performance-capability-schema=tanks3d-release-performance-capabilities-v0\n"
                "performance-telemetry-schema=tanks3d-performance-log-v2\n"
                "performance-capability-contract-sha256="
                "5137950da46fa11ee6d5ff60fafe67e83c4c0aacfb5fc83f2b0ce5f74afcfe1c\n",
                "performance-capability-schema",
            ),
        )
        for build_config, fragment in cases:
            with self.subTest(build_config=build_config):
                fixture = self.fixture()
                fixture.replace_build_config(build_config)
                self.assert_failed(fixture, fixture.run(), fragment)

    def test_refuses_legacy_candidate_attestation_before_writing(self):
        fixture = self.fixture()
        attestation = fixture.candidate / "attestation.txt"
        attestation.write_text(
            attestation.read_text(encoding="utf-8").replace(
                "schema=tanks3d-alpha-candidate-v3",
                "schema=tanks3d-alpha-candidate-v2",
                1,
            ),
            encoding="utf-8",
        )
        self.assert_failed(
            fixture,
            fixture.run(),
            "candidate must use tanks3d-alpha-candidate-v3",
        )

    def test_refuses_tagged_verifier_failure(self):
        fixture = self.fixture()
        verifier = fixture.root / "scripts/verify_tagged_alpha_candidate.sh"
        verifier.write_text("#!/bin/sh\necho rejected >&2\nexit 9\n", encoding="utf-8")
        self.assert_failed(fixture, fixture.run(), "tagged candidate verifier failed")

    def test_executes_the_captured_tagged_verifier_bytes(self):
        fixture = self.fixture()
        verifier = fixture.root / "scripts/verify_tagged_alpha_candidate.sh"
        verifier.write_text("#!/bin/sh\nexit 91\n", encoding="utf-8")
        original_read = INITIALIZER_MODULE.read_regular_file_at

        def swap_after_read(directory_fd, name, label):
            data = original_read(directory_fd, name, label)
            if name == verifier.name and label == "tagged candidate verifier":
                verifier.write_text(
                    "#!/bin/sh\n"
                    "for candidate_file in \"$2\"/*; do\n"
                    "  name=${candidate_file##*/}\n"
                    "  value=$(shasum -a 256 \"$candidate_file\" | awk '{print $1}')\n"
                    "  printf 'VERIFIED CANDIDATE FILE SHA256 %s %s\\n' \"$value\" \"$name\"\n"
                    "done\n"
                    "exit 0\n",
                    encoding="utf-8",
                )
            return data

        with mock.patch.object(
            INITIALIZER_MODULE, "read_regular_file_at", side_effect=swap_after_read
        ):
            with self.assertRaisesRegex(
                INITIALIZER_MODULE.InitError,
                r"tagged candidate verifier failed \(exit 91\)",
            ):
                INITIALIZER_MODULE.run_tagged_verifier(
                    fixture.root, fixture.candidate
                )

    def test_refuses_candidate_changed_by_verifier(self):
        fixture = self.fixture()
        verifier = fixture.root / "scripts/verify_tagged_alpha_candidate.sh"
        verifier.write_text(
            "#!/bin/sh\n"
            "printf 'changed\\n' >> \"$2/build-config.txt\"\n"
            "for candidate_file in \"$2\"/*; do\n"
            "  name=${candidate_file##*/}\n"
            "  value=$(shasum -a 256 \"$candidate_file\" | awk '{print $1}')\n"
            "  printf 'VERIFIED CANDIDATE FILE SHA256 %s %s\\n' \"$value\" \"$name\"\n"
            "done\n"
            "exit 0\n",
            encoding="utf-8",
        )
        self.assert_failed(
            fixture, fixture.run(), "candidate changed while the tagged verifier was running"
        )

    def test_refuses_receipt_for_a_different_private_snapshot(self):
        fixture = self.fixture()
        verifier = fixture.root / "scripts/verify_tagged_alpha_candidate.sh"
        verifier.write_text(
            "#!/bin/sh\n"
            "wrong="
            + "0" * 64
            + "\n"
            "first=yes\n"
            "for candidate_file in \"$2\"/*; do\n"
            "  name=${candidate_file##*/}\n"
            "  value=$(shasum -a 256 \"$candidate_file\" | awk '{print $1}')\n"
            "  if [ \"$first\" = yes ]; then value=$wrong; first=no; fi\n"
            "  printf 'VERIFIED CANDIDATE FILE SHA256 %s %s\\n' \"$value\" \"$name\"\n"
            "done\n"
            "exit 0\n",
            encoding="utf-8",
        )
        self.assert_failed(
            fixture,
            fixture.run(),
            "does not match the tagged verifier's private snapshot",
        )

    def test_refuses_symlinked_repository_ancestors(self):
        for ancestor in ("docs", "release-evidence", "scripts"):
            with self.subTest(ancestor=ancestor):
                fixture = self.fixture()
                if ancestor == "docs":
                    original = fixture.root / "docs"
                    moved = fixture.root / "docs-real"
                elif ancestor == "release-evidence":
                    original = fixture.root / "build/release-evidence"
                    moved = fixture.root / "build/release-evidence-real"
                else:
                    original = fixture.root / "scripts"
                    moved = fixture.root / "scripts-real"
                original.rename(moved)
                original.symlink_to(moved, target_is_directory=True)
                result = fixture.run()
                self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
                self.assertIn("symlinked path component", result.stderr)

    def test_refuses_wrong_input_path_and_existing_or_partial_destinations(self):
        fixture = self.fixture()
        wrong = fixture.root / "screenshots"
        wrong.mkdir()
        result = fixture.run(screenshot_dir=wrong)
        self.assert_failed(fixture, result, "screenshot input directory must be")

        for destination in ("asset", "page", "qa", "status"):
            with self.subTest(destination=destination):
                fixture = self.fixture()
                if destination == "asset":
                    fixture.asset_destination.mkdir()
                else:
                    path = {
                        "page": fixture.release_page,
                        "qa": fixture.qa_report,
                        "status": fixture.status_path,
                    }[destination]
                    path.write_text("pre-existing\n", encoding="utf-8")
                result = fixture.run()
                self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
                self.assertIn("release destination already exists", result.stderr)
                if destination == "asset":
                    self.assertEqual(list(fixture.asset_destination.iterdir()), [])
                else:
                    self.assertEqual(path.read_text(encoding="utf-8"), "pre-existing\n")
                    self.assertFalse(fixture.asset_destination.exists())

    def test_publish_failure_rolls_back_only_created_outputs(self):
        fixture = self.fixture()
        staging_parent = fixture.root / "build/staging-test"
        (staging_parent / "assets").mkdir(parents=True)
        (staging_parent / "documents").mkdir()
        for name in SCREENSHOT_NAMES:
            (staging_parent / "assets" / name).write_bytes(png_bytes(55))
        for name in ("{}.md".format(TAG), "{}-qa.md".format(TAG), "{}-status.json".format(TAG)):
            (staging_parent / "documents" / name).write_text("staged\n", encoding="utf-8")

        original_write = INITIALIZER_MODULE.write_published_file
        calls = {"count": 0}

        def fail_after_two(directory_fd, name, data, created_files):
            calls["count"] += 1
            if calls["count"] == 3:
                raise OSError("injected link failure")
            return original_write(directory_fd, name, data, created_files)

        with mock.patch.object(
            INITIALIZER_MODULE, "write_published_file", side_effect=fail_after_two
        ):
            with self.assertRaisesRegex(INITIALIZER_MODULE.InitError, "cannot publish"):
                INITIALIZER_MODULE.publish_outputs(
                    fixture.root,
                    staging_parent,
                    fixture.asset_destination,
                    [fixture.release_page, fixture.qa_report, fixture.status_path],
                    TAG,
                )
        fixture.assert_no_outputs(self)

    def test_keyboard_interrupt_also_rolls_back_created_outputs(self):
        fixture = self.fixture()
        staging_parent = fixture.root / "build/staging-interrupt-test"
        (staging_parent / "assets").mkdir(parents=True)
        (staging_parent / "documents").mkdir()
        for index, name in enumerate(SCREENSHOT_NAMES, 1):
            (staging_parent / "assets" / name).write_bytes(png_bytes(index))
        for name in (
            "{}.md".format(TAG),
            "{}-qa.md".format(TAG),
            "{}-status.json".format(TAG),
        ):
            (staging_parent / "documents" / name).write_text(
                "staged\n", encoding="utf-8"
            )

        original_write = INITIALIZER_MODULE.write_published_file
        calls = {"count": 0}

        def interrupt_after_two(directory_fd, name, data, created_files):
            calls["count"] += 1
            if calls["count"] == 3:
                raise KeyboardInterrupt()
            return original_write(directory_fd, name, data, created_files)

        with mock.patch.object(
            INITIALIZER_MODULE,
            "write_published_file",
            side_effect=interrupt_after_two,
        ):
            with self.assertRaises(KeyboardInterrupt):
                INITIALIZER_MODULE.publish_outputs(
                    fixture.root,
                    staging_parent,
                    fixture.asset_destination,
                    [fixture.release_page, fixture.qa_report, fixture.status_path],
                    TAG,
                )
        fixture.assert_no_outputs(self)

    def test_rollback_temporarily_restores_write_permission_without_changing_mode(self):
        fixture = self.fixture()
        staging_parent = fixture.root / "build/staging-permission-test"
        (staging_parent / "assets").mkdir(parents=True)
        (staging_parent / "documents").mkdir()
        for index, name in enumerate(SCREENSHOT_NAMES, 1):
            (staging_parent / "assets" / name).write_bytes(png_bytes(index))
        for name in (
            "{}.md".format(TAG),
            "{}-qa.md".format(TAG),
            "{}-status.json".format(TAG),
        ):
            (staging_parent / "documents" / name).write_text(
                "staged\n", encoding="utf-8"
            )

        document_parent = fixture.release_page.parent

        def fail_after_publication():
            os.chmod(document_parent, 0o555)
            raise INITIALIZER_MODULE.InitError("injected final-check failure")

        try:
            with self.assertRaisesRegex(
                INITIALIZER_MODULE.InitError, "injected final-check failure"
            ):
                INITIALIZER_MODULE.publish_outputs(
                    fixture.root,
                    staging_parent,
                    fixture.asset_destination,
                    [fixture.release_page, fixture.qa_report, fixture.status_path],
                    TAG,
                    final_check=fail_after_publication,
                )
            fixture.assert_no_outputs(self)
            self.assertEqual(stat.S_IMODE(document_parent.stat().st_mode), 0o555)
        finally:
            os.chmod(document_parent, 0o755)

    def test_candidate_mutation_during_publication_rolls_back(self):
        fixture = self.fixture()
        original_write = INITIALIZER_MODULE.write_published_file
        calls = {"count": 0}

        def mutate_candidate(directory_fd, name, data, created_files):
            original_write(directory_fd, name, data, created_files)
            calls["count"] += 1
            if calls["count"] == 1:
                artifact = next(fixture.candidate.glob("*.zip"))
                artifact.write_bytes(artifact.read_bytes() + b"changed")

        with mock.patch.object(
            INITIALIZER_MODULE,
            "write_published_file",
            side_effect=mutate_candidate,
        ):
            with self.assertRaisesRegex(
                INITIALIZER_MODULE.InitError, "candidate changed after tagged verification"
            ):
                INITIALIZER_MODULE.initialize(
                    fixture.root, fixture.candidate, fixture.screenshots
                )
        fixture.assert_no_outputs(self)


if __name__ == "__main__":
    unittest.main()
