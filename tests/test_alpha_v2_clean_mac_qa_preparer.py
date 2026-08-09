#!/usr/bin/env python3
"""Contract tests for the source-free Alpha-v2 clean-Mac QA kit preparer."""

import hashlib
import importlib.util
import json
import os
from pathlib import Path
import plistlib
import re
import stat
import subprocess
import sys
import tempfile
import unittest
from unittest import mock


sys.dont_write_bytecode = True
REPOSITORY_ROOT = Path(__file__).resolve().parents[1]
PREPARER_PATH = REPOSITORY_ROOT / "scripts/prepare_alpha_v2_clean_mac_qa.py"
SPEC = importlib.util.spec_from_file_location(
    "prepare_alpha_v2_clean_mac_qa", PREPARER_PATH
)
if SPEC is None or SPEC.loader is None:
    raise RuntimeError("cannot load clean-Mac QA preparer")
PREPARER = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = PREPARER
SPEC.loader.exec_module(PREPARER)


VERSION = "0.2.0"
CHANNEL = "alpha.4"
TAG = "v0.2.0-alpha.4"
COMMIT = "a" * 40
ARTIFACT_NAME = "Tanks3D-0.2.0-alpha.4-macos-arm64-macos13.0.zip"
COLLECTOR_BYTES = (
    b"#!/bin/sh\nset -eu\nprintf '%s\\n' 'fixture clean-Mac collector'\n"
)


def digest_bytes(data):
    return hashlib.sha256(data).hexdigest()


def digest_path(path):
    return digest_bytes(path.read_bytes())


class PreparerFixture:
    def __init__(self, base):
        self.root = Path(base).resolve()
        self.candidate = self.root / "build/release" / TAG
        self.status_path = self.root / "docs/releases" / (
            TAG + "-status.json"
        )
        self.profile_path = (
            self.root / "docs/release-requirements/macos-alpha-v2.json"
        )
        self.collector_path = (
            self.root / "scripts/collect_alpha_v2_clean_mac_qa.sh"
        )
        self.output_parent = self.root / "portable-kits"
        self.output = self.output_parent / "clean-mac-qa"
        self.output_parent.mkdir(parents=True)
        self._write_collector()
        self._write_candidate()
        self._write_profile()
        self._write_status()
        self.url = "https://github.com/tourzhao/tanks3d/releases/download/{}/{}".format(
            TAG, ARTIFACT_NAME
        )

    def _write_collector(self):
        self.collector_path.parent.mkdir(parents=True, exist_ok=True)
        self.collector_path.write_bytes(COLLECTOR_BYTES)
        self.collector_path.chmod(0o755)

    def _write_candidate(self):
        self.candidate.mkdir(parents=True)
        self.artifact = self.candidate / ARTIFACT_NAME
        self.artifact.write_bytes(b"immutable candidate ZIP fixture\n")
        checksum = self.candidate / (ARTIFACT_NAME + ".sha256")
        checksum.write_text(
            "{}  {}\n".format(digest_path(self.artifact), ARTIFACT_NAME),
            encoding="ascii",
        )
        (self.candidate / "attestation.txt").write_text(
            "fixture attestation\n", encoding="utf-8"
        )
        (self.candidate / "alpha-candidate-gates.log").write_text(
            "fixture gates\n", encoding="utf-8"
        )
        (self.candidate / "build-config.txt").write_text(
            "fixture=true\n", encoding="utf-8"
        )

    def _write_profile(self):
        self.profile_path.parent.mkdir(parents=True, exist_ok=True)
        self.profile = {
            "schema": "tanks3d-release-requirements-v2",
            "profile": "macos-alpha-v2",
            "clean_mac_plan_schema": "tanks3d-clean-mac-plan-v1",
            "clean_mac_download_client": "Safari",
            "clean_mac_collector_sha256": digest_path(self.collector_path),
        }
        self.write_profile()

    def write_profile(self):
        self.profile_path.write_text(
            json.dumps(self.profile, indent=2) + "\n", encoding="utf-8"
        )

    @staticmethod
    def _reference(relative, path):
        return {"path": relative, "sha256": digest_path(path)}

    def _write_status(self):
        prefix = "build/release/{}".format(TAG)
        self.status = {
            "schema": "tanks3d-release-status-v1",
            "requirements": "docs/release-requirements/macos-alpha-v2.json",
            "release": {
                "version": VERSION,
                "channel": CHANNEL,
                "tag": TAG,
                "source_commit": COMMIT,
                "candidate_dir": prefix,
                "artifact": self._reference(
                    prefix + "/" + ARTIFACT_NAME, self.artifact
                ),
                "checksum": self._reference(
                    prefix + "/" + ARTIFACT_NAME + ".sha256",
                    self.candidate / (ARTIFACT_NAME + ".sha256"),
                ),
                "attestation": self._reference(
                    prefix + "/attestation.txt",
                    self.candidate / "attestation.txt",
                ),
                "gate_log": self._reference(
                    prefix + "/alpha-candidate-gates.log",
                    self.candidate / "alpha-candidate-gates.log",
                ),
                "build_config": self._reference(
                    prefix + "/build-config.txt",
                    self.candidate / "build-config.txt",
                ),
            },
            "report": {},
            "documents": {},
            "gameplay": [],
            "bases": [],
            "pickups": [],
            "settlement": [],
            "published_controls": {},
            "clean_mac": {},
            "gatekeeper": {},
            "extended_session": {},
            "evidence": [],
            "known_issues": {},
            "audio": {},
            "approvals": [],
        }
        self.write_status()

    def write_status(self):
        self.status_path.parent.mkdir(parents=True, exist_ok=True)
        self.status_path.write_text(
            json.dumps(self.status, indent=2) + "\n", encoding="utf-8"
        )

    def run(self, url=None, output=None, candidate=None, status=None):
        return subprocess.run(
            [
                sys.executable,
                "-B",
                str(PREPARER_PATH),
                "--project-root",
                str(self.root),
                "--candidate-dir",
                str(candidate or self.candidate),
                "--status-file",
                str(status or self.status_path),
                "--download-url",
                self.url if url is None else url,
                "--output-dir",
                str(output or self.output),
            ],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            check=False,
            timeout=15,
        )


class AlphaV2CleanMacQAPreparerTests(unittest.TestCase):
    def fixture(self):
        temporary = tempfile.TemporaryDirectory(prefix="tanks3d-clean-mac-kit-")
        self.addCleanup(temporary.cleanup)
        return PreparerFixture(temporary.name)

    def assert_failed(self, fixture, result, fragment, output=None):
        self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn(fragment, result.stderr)
        self.assertFalse((output or fixture.output).exists())

    def test_success_creates_exact_private_source_free_kit(self):
        fixture = self.fixture()
        tracked_before = {
            path: path.read_bytes()
            for path in (
                fixture.status_path,
                fixture.profile_path,
                fixture.collector_path,
                fixture.artifact,
            )
        }
        result = fixture.run()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(stat.S_IMODE(fixture.output.stat().st_mode), 0o700)
        self.assertEqual(
            sorted(path.name for path in fixture.output.iterdir()),
            sorted(
                [
                    "START_HERE.command",
                    "clean-mac-plan.plist",
                    "README.txt",
                    "kit-manifest.json",
                ]
            ),
        )

        start = fixture.output / "START_HERE.command"
        self.assertEqual(start.read_bytes(), COLLECTOR_BYTES)
        self.assertEqual(stat.S_IMODE(start.stat().st_mode), 0o700)
        for name in ("clean-mac-plan.plist", "README.txt", "kit-manifest.json"):
            self.assertEqual(
                stat.S_IMODE((fixture.output / name).stat().st_mode), 0o600
            )

        plan_path = fixture.output / "clean-mac-plan.plist"
        self.assertTrue(plan_path.read_bytes().startswith(b"<?xml"))
        plan = plistlib.loads(plan_path.read_bytes())
        self.assertEqual(tuple(plan), PREPARER.PLAN_KEYS)
        self.assertEqual(plan["schema"], "tanks3d-clean-mac-plan-v1")
        self.assertEqual(plan["requirements_profile"], "macos-alpha-v2")
        self.assertEqual(plan["candidate_tag"], TAG)
        self.assertEqual(plan["candidate_filename"], ARTIFACT_NAME)
        self.assertEqual(plan["candidate_sha256"], digest_path(fixture.artifact))
        self.assertEqual(plan["download_url"], fixture.url)
        self.assertEqual(plan["minimum_macos_version"], "13.0")
        self.assertEqual(plan["collector_sha256"], digest_bytes(COLLECTOR_BYTES))
        self.assertRegex(plan["prepared_at_utc"], r"^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}Z$")
        self.assertRegex(plan["session_nonce"], r"^[0-9a-f]{32}$")

        manifest = json.loads(
            (fixture.output / "kit-manifest.json").read_text(encoding="utf-8")
        )
        self.assertEqual(
            set(manifest), {"schema", "candidate", "files"}
        )
        self.assertEqual(manifest["schema"], "tanks3d-clean-mac-kit-manifest-v1")
        self.assertEqual(
            manifest["candidate"],
            {"filename": ARTIFACT_NAME, "sha256": digest_path(fixture.artifact)},
        )
        self.assertEqual(
            [item["path"] for item in manifest["files"]],
            list(PREPARER.KIT_CONTENT_FILENAMES),
        )
        for item in manifest["files"]:
            self.assertEqual(
                set(item), {"path", "sha256"}
            )
            self.assertEqual(
                item["sha256"], digest_path(fixture.output / item["path"])
            )
        self.assertNotIn(
            "kit-manifest.json", [item["path"] for item in manifest["files"]]
        )

        readme = (fixture.output / "README.txt").read_text(encoding="utf-8")
        self.assertIn("./START_HERE.command /Users/tester/Desktop/", readme)
        self.assertIn("output path must not already exist", readme)
        self.assertIn("Before opening Safari", readme)
        self.assertIn("continuous system", readme)
        self.assertIn("MOV, MP4, or M4V", readme)
        self.assertIn("at least 64 KiB and no more", readme)
        self.assertIn("Return the untouched collector output", readme)
        self.assertIn("it is not a PASS result", readme)
        self.assertNotIn("Double-click", readme)

        self.assertFalse(any(path.suffix == ".zip" for path in fixture.output.rglob("*")))
        self.assertFalse(any(path.suffix in {".cpp", ".h", ".py"} for path in fixture.output.rglob("*")))
        combined = b"".join(
            path.read_bytes() for path in fixture.output.iterdir() if path.is_file()
        )
        self.assertNotIn(fixture.artifact.read_bytes(), combined)
        for path, before in tracked_before.items():
            self.assertEqual(path.read_bytes(), before, path)

    def test_url_policy_accepts_only_public_query_free_candidate_url(self):
        invalid_urls = (
            "http://github.com/" + ARTIFACT_NAME,
            "https://user@github.com/" + ARTIFACT_NAME,
            "https://user:secret@github.com/" + ARTIFACT_NAME,
            "https://github.com/" + ARTIFACT_NAME + "?token=secret",
            "https://github.com/" + ARTIFACT_NAME + "?",
            "https://github.com/" + ARTIFACT_NAME + "#fragment",
            "https://github.com/" + ARTIFACT_NAME + "#",
            "https://127.0.0.1/" + ARTIFACT_NAME,
            "https://[::1]/" + ARTIFACT_NAME,
            "https://2130706433/" + ARTIFACT_NAME,
            "https://0x7f000001/" + ARTIFACT_NAME,
            "https://localhost/" + ARTIFACT_NAME,
            "https://github/" + ARTIFACT_NAME,
            "https://example.com/" + ARTIFACT_NAME,
            "https://download.example.com/" + ARTIFACT_NAME,
            "https://release.invalid/" + ARTIFACT_NAME,
            "https://release.test/" + ARTIFACT_NAME,
            "https://release.local/" + ARTIFACT_NAME,
            "https://release.internal/" + ARTIFACT_NAME,
            "https://release.onion/" + ARTIFACT_NAME,
            "https://github.com:444/" + ARTIFACT_NAME,
            "https://github.com:abc/" + ARTIFACT_NAME,
            "https://github.com./" + ARTIFACT_NAME,
            "https://github..com/" + ARTIFACT_NAME,
            "https://-github.com/" + ARTIFACT_NAME,
            "https://github-.com/" + ARTIFACT_NAME,
            "https://github_com/" + ARTIFACT_NAME,
            "https://placeholder.github.com/" + ARTIFACT_NAME,
            "https://github。com/" + ARTIFACT_NAME,
            "https://github.com/not-" + ARTIFACT_NAME,
            "https://github.com/%2F" + ARTIFACT_NAME,
            "https://github.com/%ZZ" + ARTIFACT_NAME,
        )
        for index, url in enumerate(invalid_urls):
            with self.subTest(url=url):
                fixture = self.fixture()
                output = fixture.output_parent / "invalid-{}".format(index)
                result = fixture.run(url=url, output=output)
                self.assertNotEqual(
                    result.returncode, 0, result.stdout + result.stderr
                )
                self.assertFalse(output.exists())

        for index, url in enumerate(
            (
                "https://github.com/releases/" + ARTIFACT_NAME,
                "https://GITHUB.COM:443/releases/" + ARTIFACT_NAME,
                "https://github.com/releases/%54" + ARTIFACT_NAME[1:],
            )
        ):
            with self.subTest(valid_url=url):
                fixture = self.fixture()
                output = fixture.output_parent / "valid-{}".format(index)
                result = fixture.run(url=url, output=output)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_rejects_wrong_or_noncanonical_status(self):
        mutations = (
            lambda status: status.__setitem__("schema", "wrong-status-schema"),
            lambda status: status.__setitem__(
                "requirements", "docs/release-requirements/macos-alpha-v1.json"
            ),
            lambda status: status.__setitem__("invented", True),
            lambda status: status["release"].__setitem__("tag", "v0.2.0-alpha.5"),
            lambda status: status["release"].__setitem__(
                "candidate_dir", "build/release/elsewhere"
            ),
            lambda status: status["release"]["artifact"].__setitem__(
                "unexpected", "value"
            ),
        )
        for mutation in mutations:
            with self.subTest(mutation=mutation):
                fixture = self.fixture()
                mutation(fixture.status)
                fixture.write_status()
                result = fixture.run()
                self.assertNotEqual(
                    result.returncode, 0, result.stdout + result.stderr
                )
                self.assertFalse(fixture.output.exists())

    def test_rejects_candidate_mismatch_hash_extra_zip_and_symlink(self):
        fixture = self.fixture()
        fixture.status["release"]["artifact"]["sha256"] = "0" * 64
        fixture.write_status()
        self.assert_failed(fixture, fixture.run(), "SHA-256 does not match")

        fixture = self.fixture()
        (fixture.candidate / "unexpected.zip").write_bytes(b"other archive")
        self.assert_failed(
            fixture,
            fixture.run(),
            "exactly the status-bound ZIP",
        )

        fixture = self.fixture()
        wrong = fixture.root / "build/release/wrong-candidate"
        wrong.mkdir()
        self.assert_failed(
            fixture,
            fixture.run(candidate=wrong),
            "does not match release status",
        )

        fixture = self.fixture()
        archive_bytes = fixture.artifact.read_bytes()
        target = fixture.root / "archive-target.zip"
        target.write_bytes(archive_bytes)
        fixture.artifact.unlink()
        fixture.artifact.symlink_to(target)
        self.assert_failed(
            fixture, fixture.run(), "symlinked path component"
        )

    def test_profile_must_bind_exact_fixed_collector(self):
        fixture = self.fixture()
        fixture.profile["clean_mac_plan_schema"] = "wrong-plan-schema"
        fixture.write_profile()
        self.assert_failed(fixture, fixture.run(), "plan schema does not match")

        fixture = self.fixture()
        fixture.profile["clean_mac_download_client"] = "curl"
        fixture.write_profile()
        self.assert_failed(fixture, fixture.run(), "client must be Safari")

        fixture = self.fixture()
        fixture.profile.pop("clean_mac_collector_sha256")
        fixture.write_profile()
        self.assert_failed(
            fixture, fixture.run(), "clean_mac_collector_sha256"
        )

        fixture = self.fixture()
        fixture.profile["clean_mac_collector_sha256"] = "0" * 64
        fixture.write_profile()
        self.assert_failed(
            fixture, fixture.run(), "collector SHA-256 does not match"
        )

        fixture = self.fixture()
        fixture.collector_path.write_bytes(COLLECTOR_BYTES + b"# mutation\n")
        self.assert_failed(
            fixture, fixture.run(), "collector SHA-256 does not match"
        )

    def test_rejects_symlinked_status_and_collector(self):
        fixture = self.fixture()
        status_target = fixture.root / "status-target.json"
        fixture.status_path.replace(status_target)
        fixture.status_path.symlink_to(status_target)
        self.assert_failed(fixture, fixture.run(), "symlinked path component")

        fixture = self.fixture()
        collector_target = fixture.root / "collector-target.sh"
        fixture.collector_path.replace(collector_target)
        fixture.collector_path.symlink_to(collector_target)
        self.assert_failed(fixture, fixture.run(), "symlinked path component")

    def test_toctou_input_change_rolls_back_complete_kit(self):
        fixture = self.fixture()
        original_verify = PREPARER.verify_inputs_unchanged

        def race(state):
            fixture.collector_path.write_bytes(COLLECTOR_BYTES + b"# raced\n")
            original_verify(state)

        with mock.patch.object(PREPARER, "verify_inputs_unchanged", side_effect=race):
            with self.assertRaisesRegex(PREPARER.PrepareError, "changed while"):
                PREPARER.prepare(
                    fixture.root,
                    fixture.candidate,
                    fixture.status_path,
                    fixture.url,
                    fixture.output,
                )
        self.assertFalse(fixture.output.exists())

    def test_existing_output_is_never_modified(self):
        fixture = self.fixture()
        fixture.output.mkdir(mode=0o700)
        marker = fixture.output / "owned-by-user.txt"
        marker.write_text("preserve me\n", encoding="utf-8")
        result = fixture.run()
        self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("already exists", result.stderr)
        self.assertEqual(marker.read_text(encoding="utf-8"), "preserve me\n")
        self.assertEqual(sorted(path.name for path in fixture.output.iterdir()), [marker.name])

    def test_forced_publication_failure_rolls_back_complete_kit(self):
        fixture = self.fixture()
        original_write = PREPARER.write_file_at
        calls = {"count": 0}

        def failing_write(*args, **kwargs):
            calls["count"] += 1
            if calls["count"] == 3:
                raise OSError("forced publication failure")
            return original_write(*args, **kwargs)

        with mock.patch.object(PREPARER, "write_file_at", side_effect=failing_write):
            with self.assertRaisesRegex(PREPARER.PrepareError, "publication failed"):
                PREPARER.prepare(
                    fixture.root,
                    fixture.candidate,
                    fixture.status_path,
                    fixture.url,
                    fixture.output,
                )
        self.assertFalse(fixture.output.exists())

    def test_repository_profile_hashes_the_fixed_collector(self):
        collector = REPOSITORY_ROOT / "scripts/collect_alpha_v2_clean_mac_qa.sh"
        profile_path = (
            REPOSITORY_ROOT / "docs/release-requirements/macos-alpha-v2.json"
        )
        self.assertTrue(collector.is_file(), "fixed clean-Mac collector is missing")
        profile = json.loads(profile_path.read_text(encoding="utf-8"))
        self.assertEqual(
            profile.get("clean_mac_collector_sha256"), digest_path(collector)
        )


if __name__ == "__main__":
    unittest.main()
