"""Release boundaries: source identity, immutable packages and honest QA status."""
import contextlib
import importlib.util
import json
from pathlib import Path
import stat
import subprocess
import sys
import tempfile
import unittest
from unittest import mock
import warnings
import zipfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
SPEC = importlib.util.spec_from_file_location("godot_release", ROOT / "scripts/godot_release.py")
RELEASE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(RELEASE)


class GodotReleaseTests(unittest.TestCase):
    def setUp(self):
        (ROOT / "build/tests").mkdir(parents=True, exist_ok=True)
        self.temp = tempfile.TemporaryDirectory(prefix="godot-release-", dir=ROOT / "build/tests")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        (self.root / "build").mkdir()
        self.identity = {"version": "0.2.0", "build": "7", "commit": "a" * 40, "tag": "v0.2.0-godot.alpha.1"}

    def test_identity_rejects_legacy_tags_invalid_values_and_types(self):
        for key, value in [("tag", "v0.2.0-alpha.1"), ("tag", "v0.1.0-godot.alpha.1"),
                           ("version", "../bad"), ("build", "0"), ("build", 1), ("commit", "short")]:
            with self.subTest(key=key, value=value), self.assertRaises(RuntimeError):
                RELEASE.package.validate_identity(dict(self.identity, **{key: value}))

    def test_clean_source_requires_all_new_files_committed_and_exact_tag(self):
        def git(*args, root=None):
            return subprocess.check_output(["git", "-C", str(self.root), *args], text=True).strip()
        subprocess.run(["git", "init", "-q", str(self.root)], check=True)
        (self.root / "source.txt").write_text("original")
        git("add", "source.txt")
        git("-c", "user.name=QA Fixture", "-c", "user.email=fixture@invalid", "commit", "-qm", "Fixture")
        git("tag", "v0.2.0-godot.alpha.1")
        with mock.patch.object(RELEASE, "git", side_effect=git):
            self.assertEqual(RELEASE.source_identity("0.2.0", "alpha.1")["commit"], git("rev-parse", "HEAD"))
            with mock.patch.object(RELEASE, "ROOT", self.root):
                self.assertEqual(RELEASE.source_files(), {"source.txt": RELEASE.digest(self.root / "source.txt")})
            (self.root / "new.gd").write_text("uncommitted")
            with self.assertRaisesRegex(RuntimeError, "clean worktree"):
                RELEASE.source_identity("0.2.0", "alpha.1")
            (self.root / "new.gd").unlink()
            (self.root / "source.txt").write_text("changed")
            with self.assertRaisesRegex(RuntimeError, "clean worktree"):
                RELEASE.source_identity("0.2.0", "alpha.1")
            git("add", "source.txt")
            git("-c", "user.name=QA Fixture", "-c", "user.email=fixture@invalid", "commit", "-qm", "Next")
            with self.assertRaisesRegex(RuntimeError, "tag must point"):
                RELEASE.source_identity("0.2.0", "alpha.1")

    def candidate(self):
        candidate = self.root / "candidate"
        (candidate / "gates").mkdir(parents=True)
        app = self.root / "Tanks3D-Godot.app"
        binary = app / "Contents/MacOS/Tanks3D-Godot"
        binary.parent.mkdir(parents=True)
        binary.write_bytes(b"test fixture only")
        binary.chmod(0o755)
        archive = candidate / "candidate.zip"
        RELEASE.archive_app(app, archive)
        (candidate / "candidate.zip.sha256").write_text(f"{RELEASE.digest(archive)}  candidate.zip\n")
        RELEASE.write_json(candidate / "source-manifest.json", {"source": "sha"})
        gates = {}
        for name, argv in RELEASE.GATES.items():
            (candidate / "gates" / (name + ".log")).write_text("fixture gate output\n")
            gates[name] = {"argv": argv, "exit_code": 0, "log": "gates/" + name + ".log"}
        data = {"schema": RELEASE.SCHEMA, "identity": self.identity, "gates": gates,
                "requirements_sha256": RELEASE.json_digest(RELEASE.requirements()),
                "archive": archive.name, "archive_sha256": RELEASE.digest(archive),
                "app_sha256": RELEASE.file_hashes(app), "files_sha256": RELEASE.file_hashes(candidate)}
        RELEASE.write_json(candidate / "attestation.json", data)
        return candidate, data

    @contextlib.contextmanager
    def verifier_fixture(self):
        with mock.patch.object(RELEASE, "source_identity", return_value=self.identity), \
                mock.patch.object(RELEASE, "source_files", return_value={"source": "sha"}), \
                mock.patch.object(RELEASE.package, "verify") as verify:
            yield verify

    def test_candidate_hashes_identity_and_actual_extracted_app_are_checked(self):
        candidate, _ = self.candidate()
        with self.verifier_fixture() as verify:
            RELEASE.verify_candidate(candidate, smoke=False)
            verify.assert_called_once()
            self.assertEqual(verify.call_args.args[1], self.identity)

    def test_modified_and_extra_candidate_files_fail(self):
        candidate, _ = self.candidate()
        with self.verifier_fixture():
            (candidate / "extra").write_text("not attested")
            with self.assertRaisesRegex(RuntimeError, "attested hashes"):
                RELEASE.verify_candidate(candidate, smoke=False)
            (candidate / "extra").unlink()
            (candidate / "candidate.zip").write_bytes(b"modified")
            with self.assertRaisesRegex(RuntimeError, "attested hashes"):
                RELEASE.verify_candidate(candidate, smoke=False)

    def test_missing_failed_and_changed_gate_contracts_fail(self):
        candidate, data = self.candidate()
        for change in ["missing", "failed", "changed"]:
            altered = json.loads(json.dumps(data))
            if change == "missing": altered["gates"].pop("native")
            elif change == "failed": altered["gates"]["native"]["exit_code"] = 1
            else: altered["gates"]["native"]["argv"] = ["true"]
            RELEASE.write_json(candidate / "attestation.json", altered)
            with self.verifier_fixture(), self.assertRaisesRegex(RuntimeError, "gates|gate result"):
                RELEASE.verify_candidate(candidate, smoke=False)

    def test_wrong_source_or_rewritten_checksum_is_rejected(self):
        candidate, data = self.candidate()
        RELEASE.write_json(candidate / "source-manifest.json", {"other-source": "sha"})
        data["files_sha256"]["source-manifest.json"] = RELEASE.digest(candidate / "source-manifest.json")
        RELEASE.write_json(candidate / "attestation.json", data)
        with self.verifier_fixture(), self.assertRaisesRegex(RuntimeError, "source snapshot"):
            RELEASE.verify_candidate(candidate, smoke=False)

    def test_extraction_rejects_traversal_symlink_duplicate_and_extra_roots(self):
        for name, mode, duplicate in [("../escape", stat.S_IFREG, False),
                ("Tanks3D-Godot.app/Contents/link", stat.S_IFLNK, False),
                ("Other.app/Contents/file", stat.S_IFREG, False),
                ("/absolute", stat.S_IFREG, False),
                ("Tanks3D-Godot.app/Contents/file", stat.S_IFREG, True)]:
            archive = self.root / "bad.zip"
            with zipfile.ZipFile(archive, "w") as z:
                member = zipfile.ZipInfo(name)
                member.external_attr = (mode | 0o644) << 16
                z.writestr(member, "bad")
                if duplicate:
                    with warnings.catch_warnings():
                        warnings.simplefilter("ignore", UserWarning)
                        z.writestr(member, "duplicate")
            with self.assertRaisesRegex(RuntimeError, "archive entry"):
                with RELEASE.extracted_app(archive): pass

    def test_candidate_and_qa_symlinks_cannot_escape_evidence_root(self):
        (self.root / "linked").symlink_to(ROOT / "README.md")
        with self.assertRaisesRegex(RuntimeError, "symlink"):
            RELEASE.checked_file(self.root, "linked")
        for name in ["../README.md", "/tmp/file", "./file", "x\\y", "x\ny"]:
            with self.assertRaisesRegex(RuntimeError, "Unsafe"):
                RELEASE.checked_file(self.root, name)
        candidate, _ = self.candidate()
        (self.root / "candidate-link").symlink_to(candidate)
        with self.assertRaisesRegex(RuntimeError, "symlink"):
            RELEASE.verify_candidate(self.root / "candidate-link")

    def test_duplicate_json_key_is_rejected(self):
        path = self.root / "bad.json"
        path.write_text('{"status":"FAIL","status":"PASS"}')
        with self.assertRaisesRegex(RuntimeError, "Duplicate JSON"):
            RELEASE.read_json(path)

    def test_failed_gate_propagates_and_cannot_be_attested(self):
        with self.assertRaisesRegex(RuntimeError, "Gate failed"):
            RELEASE.run_gate([sys.executable, "-c", "raise SystemExit(3)"], self.root / "failed.log")

    def test_hanging_gate_is_bounded(self):
        with self.assertRaisesRegex(RuntimeError, "time/output budget"):
            RELEASE.run_gate([sys.executable, "-c", "import time; time.sleep(30)"], self.root / "timeout.log", timeout=.1)

    def test_failed_build_keeps_logs_without_installing_candidate(self):
        def fail(command, output):
            output.write_text("fixture build failure\n")
            raise RuntimeError("Fixture gate failed")
        with mock.patch.object(RELEASE, "ROOT", self.root), self.verifier_fixture(), \
                mock.patch.object(RELEASE, "run_gate", side_effect=fail):
            with self.assertRaisesRegex(RuntimeError, "gate failed"):
                RELEASE.build_candidate("0.2.0", "alpha.1")
        self.assertFalse((self.root / "build/release/godot" / self.identity["tag"]).exists())
        self.assertEqual(len(list((self.root / "build/release-evidence").rglob("clean.log"))), 1)

    def test_existing_candidate_cannot_be_rebuilt(self):
        directory = self.root / "build/release/godot" / self.identity["tag"]
        directory.mkdir(parents=True)
        (directory / "preserve").write_text("existing evidence")
        with mock.patch.object(RELEASE, "ROOT", self.root), self.verifier_fixture(), \
                mock.patch.object(RELEASE, "run_gate") as gate:
            with self.assertRaisesRegex(RuntimeError, "never overwrite"):
                RELEASE.build_candidate("0.2.0", "alpha.1")
            gate.assert_not_called()
        self.assertEqual((directory / "preserve").read_text(), "existing evidence")

    def test_extended_session_requires_numeric_metrics_and_raw_logs(self):
        report = {"schema": "tanks3d-godot-session-metrics-v1", "candidate_sha256": "b" * 64,
                  "duration_seconds": 1800, "stages_completed": 1, "average_fps": 60,
                  "one_percent_low_fps": 40, "memory_growth_bytes": 1024,
                  "gameplay_duration_ratio": .9, "focused_duration_ratio": 1.0,
                  "worst_thermal_state": "nominal", "measurement_tool": "Fixture tool",
                  "clock": "monotonic", "memory_metric": "resident bytes", "method": "Fixture method",
                  "raw_logs": ["raw.log"]}
        item = {"metrics_report": "metrics.json", "evidence": [{"path": "metrics.json"}, {"path": "raw.log"}]}
        path = self.root / "metrics.json"
        data = {"archive_sha256": "b" * 64}
        RELEASE.write_json(path, report)
        RELEASE.verify_extended_metrics(item, self.root / "qa.json", data)
        for key, value in [("duration_seconds", 1799), ("stages_completed", 0), ("average_fps", float("nan")),
                ("one_percent_low_fps", 29), ("memory_growth_bytes", 300 * 1024 * 1024),
                ("gameplay_duration_ratio", .5), ("focused_duration_ratio", False),
                ("worst_thermal_state", "serious"), ("raw_logs", [])]:
            RELEASE.write_json(path, dict(report, **{key: value}))
            with self.subTest(key=key), self.assertRaisesRegex(RuntimeError, "Extended.session"):
                RELEASE.verify_extended_metrics(item, self.root / "qa.json", data)

    def test_initial_qa_is_blocked_and_cannot_be_overwritten(self):
        candidate, data = self.candidate()
        status = self.root / "qa.json"
        RELEASE.init_status(candidate, data, status)
        with self.assertRaisesRegex(RuntimeError, "BLOCKED"):
            RELEASE.verify_status(status, data)
        with self.assertRaisesRegex(RuntimeError, "overwrite"):
            RELEASE.init_status(candidate, data, status)
        record = RELEASE.read_json(status)
        record["candidate_sha256"] = "0" * 64
        RELEASE.write_json(status, record)
        with self.assertRaisesRegex(RuntimeError, "exact Godot candidate"):
            RELEASE.verify_status(status, data)

    def test_real_checklist_keeps_rule_hardware_and_extended_session_scope(self):
        groups = RELEASE.requirements()
        self.assertIn("fire_hits_and_shell_cancellation", groups["two_player"])
        self.assertIn("pickup_bandage_disabled_at_max_hp_1", groups)
        self.assertIn("controller_disconnect_reconnect_without_stuck_input", groups["controls"])
        self.assertIn("tab_pixel_toggle", groups["controls"])
        self.assertNotIn("f8_quality_toggle", groups["controls"])
        self.assertIn("at_least_30_minutes", groups["extended_session"])

    def test_checkboxes_alone_cannot_approve_release(self):
        candidate, data = self.candidate()
        status = self.root / "qa.json"
        RELEASE.init_status(candidate, data, status)
        document = RELEASE.read_json(status)
        for item in document["observations"].values():
            item["checks"] = dict.fromkeys(item["checks"], "PASS")
        RELEASE.write_json(status, document)
        with self.assertRaisesRegex(RuntimeError, "human observation"):
            RELEASE.verify_status(status, data)

    def test_stale_approval_and_missing_video_are_rejected(self):
        groups = {"one_player": ["controls"]}
        video = self.root / "fixture.mp4"
        video.write_bytes(b"fixture only\0" * 6000)
        data = {"archive_sha256": "b" * 64, "identity": self.identity}
        document = {"schema": "tanks3d-godot-release-status-v1", "identity": self.identity,
                    "candidate_sha256": data["archive_sha256"], "requirements_sha256": RELEASE.json_digest(groups),
                    "known_issues_reviewed": True, "known_issues": [], "approvals": {}, "observations": {
                        "one_player": {"checks": {"controls": "PASS"}, "tester": "Fixture Tester", "machine": "Fixture M2",
                            "notes": "Fixture observation", "completed_at_utc": "2026-09-01T12:00:00Z",
                            "evidence": [{"path": video.name, "sha256": RELEASE.digest(video)}]}}}
        status = self.root / "qa.json"
        with mock.patch.object(RELEASE, "requirements", return_value=groups), \
                mock.patch.object(RELEASE, "validate_recording"):
            RELEASE.write_json(status, document)
            with self.assertRaisesRegex(RuntimeError, "Both QA"):
                RELEASE.verify_status(status, data)
            approved_hash = RELEASE.json_digest({k: v for k, v in document.items() if k != "approvals"})
            document["approvals"] = {role: {"name": "Fixture Reviewer", "decision": "approved",
                "at_utc": "2026-09-02T12:00:00Z", "report_sha256": approved_hash} for role in ["qa_lead", "release_owner"]}
            RELEASE.write_json(status, document)
            RELEASE.verify_status(status, data)
            document["observations"]["one_player"]["notes"] = "Changed after approval"
            RELEASE.write_json(status, document)
            with self.assertRaisesRegex(RuntimeError, "stale"):
                RELEASE.verify_status(status, data)
            document["observations"]["one_player"]["evidence"] = []
            RELEASE.write_json(status, document)
            with self.assertRaisesRegex(RuntimeError, "evidence"):
                RELEASE.verify_status(status, data)


if __name__ == "__main__":
    unittest.main()
