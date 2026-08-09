#!/usr/bin/env python3
"""Focused, dependency-free tests for the release performance QA runner."""

import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import stat
import subprocess
import sys
import tempfile
import unittest
from unittest import mock
import zipfile


sys.dont_write_bytecode = True
REPOSITORY_ROOT = Path(__file__).resolve().parents[1]
RUNNER_PATH = REPOSITORY_ROOT / "scripts" / "run_release_performance_qa.py"
CAPABILITY_GOLDEN = (
    REPOSITORY_ROOT / "tests" / "expected_release_performance_capabilities.json"
)
SPEC = importlib.util.spec_from_file_location("run_release_performance_qa", RUNNER_PATH)
if SPEC is None or SPEC.loader is None:
    raise RuntimeError("cannot load performance QA runner")
RUNNER = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = RUNNER
SPEC.loader.exec_module(RUNNER)


SOURCE_COMMIT = "a" * 40
SOURCE_TAG = "v0.1.0-alpha.4"
NONCE = "0123456789abcdef0123456789abcdef"
STARTED = "2026-08-08T20:00:00Z"
COMPLETED = "2026-08-08T20:30:01Z"


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


class FakeProcess:
    def __init__(
        self, exit_code=0, pid=4242, hangs=False, ignores_terminate=False
    ):
        self.pid = pid
        self._exit_code = exit_code
        self.hangs = hangs
        self.ignores_terminate = ignores_terminate
        self.terminated = False
        self.killed = False
        self.wait_timeouts = []

    def wait(self, timeout=None):
        self.wait_timeouts.append(timeout)
        if self.hangs and not self.killed and (
            not self.terminated or self.ignores_terminate
        ):
            raise subprocess.TimeoutExpired("fixture candidate", timeout)
        if self.terminated and not self.ignores_terminate:
            return -15
        if self.killed:
            return -9
        return self._exit_code

    def terminate(self):
        self.terminated = True

    def kill(self):
        self.killed = True


class RunnerFixture:
    def __init__(self, base):
        self.root = Path(base).resolve()
        self.output = self.root / "private-output"
        self.output.mkdir(mode=0o700)
        os.chmod(self.output, 0o700)
        self.candidate = self.root / "build" / "release" / SOURCE_TAG
        self.candidate.mkdir(parents=True)
        self.calls = []
        self.verify_returncode = 0
        self.verifier_receipt_mode = "valid"
        self.extract_returncode = 0
        self.process_returncode = 0
        self.process_hangs = False
        self.process_ignores_terminate = False
        self.capability_mode = "valid"
        self.marker_mode = "valid"
        self.telemetry_mode = "valid"
        self.receipt_race = False
        self.replace_artifact_before_extract = False
        self.replace_snapshot_path_before_extract = False
        self.extracted_archive_argument = None
        self.extracted_archive_bytes = None
        self.snapshot_descriptor = None
        self.snapshot_descriptor_link_count = None
        self.snapshot_path_existed_at_extract = None
        self.snapshot_pass_fds = None
        self.verifier_input = None
        self.last_executable = None
        self.last_process = None
        self._write_verifier()
        self._write_candidate()

    def _write_verifier(self):
        verifier = self.root / "scripts" / "verify_tagged_alpha_candidate.sh"
        verifier.parent.mkdir(parents=True)
        verifier.write_text("#!/bin/sh\nexit 0\n", encoding="utf-8")
        verifier.chmod(0o755)

    def _write_candidate(self):
        artifact = self.candidate / "Tanks3D-0.1.0-alpha.4-macos-arm64-macos26.0.zip"
        checksum = self.candidate / (artifact.name + ".sha256")
        gate_log = self.candidate / "alpha-candidate-gates.log"
        build_config = self.candidate / "build-config.txt"
        attestation = self.candidate / "attestation.txt"
        artifact.write_bytes(b"fixture archive bytes\n")
        gate_log.write_text("fixture gates pass\n", encoding="utf-8")
        build_config.write_text(
            "fixture=true\n"
            "performance-capability-schema={}\n"
            "performance-telemetry-schema={}\n"
            "performance-capability-contract-sha256={}\n".format(
                RUNNER.CAPABILITY_SCHEMA,
                RUNNER.TELEMETRY_SCHEMA,
                RUNNER.CAPABILITY_CONTRACT_SHA256,
            ),
            encoding="utf-8",
        )
        artifact_sha = digest(artifact)
        checksum.write_text("{}  {}\n".format(artifact_sha, artifact.name), encoding="utf-8")
        values = {
            "schema": RUNNER.CANDIDATE_ATTESTATION_SCHEMA,
            "source_commit": SOURCE_COMMIT,
            "source_tag": SOURCE_TAG,
            "artifact_filename": artifact.name,
            "artifact_sha256": artifact_sha,
            "checksum_filename": checksum.name,
            "build_config_filename": build_config.name,
            "build_config_sha256": digest(build_config),
            "gate_log_filename": gate_log.name,
            "gate_log_sha256": digest(gate_log),
        }
        attestation.write_text(
            "".join("{}={}\n".format(key, value) for key, value in values.items()),
            encoding="utf-8",
        )
        self.artifact = artifact
        self.artifact_sha256 = artifact_sha

    def fake_run(self, argv, **kwargs):
        self.calls.append(list(argv))
        if argv[0] == "sh":
            self.verifier_input = kwargs.get("input")
            receipt_lines = []
            candidate_files = sorted(self.candidate.iterdir())
            for index, candidate_file in enumerate(candidate_files):
                if self.verifier_receipt_mode == "missing" and index == len(candidate_files) - 1:
                    continue
                value = digest(candidate_file)
                if self.verifier_receipt_mode == "wrong" and index == 0:
                    value = "0" * 64
                receipt_lines.append(
                    "VERIFIED CANDIDATE FILE SHA256 {} {}\n".format(
                        value, candidate_file.name
                    )
                )
            if self.verifier_receipt_mode == "duplicate":
                receipt_lines.append(receipt_lines[0])
            receipt = "".join(receipt_lines).encode("utf-8")
            return subprocess.CompletedProcess(
                argv,
                self.verify_returncode,
                stdout=receipt if self.verify_returncode == 0 else b"",
                stderr=b"forced verifier failure\n" if self.verify_returncode else b"",
            )
        if argv[0] == "/usr/bin/ditto":
            archive_argument = argv[-2]
            self.extracted_archive_argument = archive_argument
            self.snapshot_pass_fds = kwargs.get("pass_fds")
            if not self.snapshot_pass_fds or len(self.snapshot_pass_fds) != 1:
                raise AssertionError("ditto did not receive exactly one snapshot descriptor")
            self.snapshot_descriptor = self.snapshot_pass_fds[0]
            self.snapshot_descriptor_link_count = os.fstat(
                self.snapshot_descriptor
            ).st_nlink
            destination = Path(argv[-1])
            snapshot_path = destination.parent / "candidate-archive.snapshot.zip"
            self.snapshot_path_existed_at_extract = snapshot_path.exists()
            if self.replace_artifact_before_extract:
                original = self.artifact.read_bytes()
                self.artifact.write_bytes(b"racing replacement archive\n")
                self.artifact.write_bytes(original)
            if self.replace_snapshot_path_before_extract:
                snapshot_path.write_bytes(b"replacement at the removed snapshot path\n")
            self.extracted_archive_bytes = Path(archive_argument).read_bytes()
            if self.extract_returncode == 0:
                executable = destination / "Tanks3D.app/Contents/MacOS/Tanks3D"
                executable.parent.mkdir(parents=True)
                executable.write_bytes(b"exact extracted candidate executable\n")
                executable.chmod(0o755)
                self.last_executable = executable
            return subprocess.CompletedProcess(
                argv,
                self.extract_returncode,
                stdout="",
                stderr="forced extraction failure\n" if self.extract_returncode else "",
            )
        if len(argv) == 2 and argv[1] == RUNNER.CAPABILITY_ARGUMENT:
            if self.capability_mode == "timeout":
                raise subprocess.TimeoutExpired(argv, kwargs.get("timeout"))
            identity = RUNNER.CandidateIdentity(
                candidate_dir=self.candidate,
                artifact=self.artifact,
                artifact_sha256=self.artifact_sha256,
                source_commit=SOURCE_COMMIT,
                source_tag=SOURCE_TAG,
            )
            capability = json.loads(CAPABILITY_GOLDEN.read_text(encoding="utf-8"))
            capability["source_commit"] = identity.source_commit
            capability["source_tag"] = identity.source_tag
            if self.capability_mode == "wrong_schema":
                capability["schema"] = "tanks3d-release-performance-capabilities-v0"
            elif self.capability_mode == "wrong_identity":
                capability["source_commit"] = "0" * 40
            elif self.capability_mode == "extra_key":
                capability["unexpected"] = True
            if self.capability_mode == "duplicate_key":
                stdout = b'{"schema":"first","schema":"second"}\n'
            elif self.capability_mode == "invalid_utf8":
                stdout = b"\xff\n"
            else:
                stdout = (
                    json.dumps(capability, indent=2, sort_keys=False) + "\n"
                ).encode("utf-8")
            stderr = (
                b"unexpected capability diagnostic\n"
                if self.capability_mode == "stderr"
                else b""
            )
            return subprocess.CompletedProcess(
                argv,
                2 if self.capability_mode == "nonzero" else 0,
                stdout=stdout,
                stderr=stderr,
            )
        raise AssertionError("unexpected subprocess.run argv: {!r}".format(argv))

    @staticmethod
    def argument(argv, prefix):
        return next(value[len(prefix) :] for value in argv if value.startswith(prefix))

    def fake_popen(self, argv, stdout, stderr, cwd, close_fds):
        del cwd, close_fds
        self.calls.append(list(argv))
        nonce = self.argument(argv, "--release-session-nonce=")
        telemetry_path = Path(self.argument(argv, "--release-performance-log="))
        candidate_sha = self.argument(argv, "--release-candidate-sha256=")
        if self.marker_mode == "valid":
            stdout.write(
                (
                    "{} {}\n{} {}\n".format(
                        RUNNER.START_MARKER,
                        nonce,
                        RUNNER.COMPLETE_MARKER,
                        nonce,
                    )
                ).encode("utf-8")
            )
        elif self.marker_mode == "missing_complete":
            stdout.write("{} {}\n".format(RUNNER.START_MARKER, nonce).encode("utf-8"))
        elif self.marker_mode == "wrong_nonce":
            stdout.write(
                "{} wrong\n{} wrong\n".format(
                    RUNNER.START_MARKER, RUNNER.COMPLETE_MARKER
                ).encode("utf-8")
            )
        stderr.write(b"fixture candidate stderr\n")

        telemetry = {
            "schema": RUNNER.TELEMETRY_SCHEMA,
            "candidate_sha256": candidate_sha,
            "session_nonce": nonce,
            "source_commit": SOURCE_COMMIT,
            "source_tag": SOURCE_TAG,
            "clean_shutdown": True,
            "samples": [],
        }
        if self.telemetry_mode == "wrong_candidate":
            telemetry["candidate_sha256"] = "0" * 64
        elif self.telemetry_mode == "unclean":
            telemetry["clean_shutdown"] = False
        if self.telemetry_mode == "duplicate_key":
            telemetry_path.write_text(
                '{"schema":"tanks3d-performance-log-v2",'
                '"schema":"duplicate"}\n',
                encoding="utf-8",
            )
        elif self.telemetry_mode != "missing":
            telemetry_path.write_text(json.dumps(telemetry) + "\n", encoding="utf-8")
        if self.receipt_race:
            (self.output / RUNNER.RECEIPT_FILENAME).write_text(
                "sentinel receipt must survive\n", encoding="utf-8"
            )
        self.last_process = FakeProcess(
            self.process_returncode,
            hangs=self.process_hangs,
            ignores_terminate=self.process_ignores_terminate,
        )
        return self.last_process

    def patches(self):
        return (
            mock.patch.object(RUNNER.subprocess, "run", side_effect=self.fake_run),
            mock.patch.object(RUNNER.subprocess, "Popen", side_effect=self.fake_popen),
            mock.patch.object(RUNNER.secrets, "token_hex", return_value=NONCE),
            mock.patch.object(RUNNER, "utc_now", side_effect=[STARTED, COMPLETED]),
        )

    def run(self, duration=RUNNER.DEFAULT_DURATION_SECONDS):
        patches = self.patches()
        with patches[0], patches[1], patches[2], patches[3]:
            return RUNNER.run_performance_qa(
                self.root, self.candidate, self.output, duration
            )


class ReleasePerformanceRunnerTests(unittest.TestCase):
    def new_fixture(self):
        temporary = tempfile.TemporaryDirectory(prefix="tanks3d-performance-runner-test-")
        self.addCleanup(temporary.cleanup)
        return RunnerFixture(temporary.name)

    def assert_runner_error(self, fixture, fragment):
        with self.assertRaisesRegex(RUNNER.RunnerError, fragment):
            fixture.run(duration=2)
        self.assertFalse((fixture.output / RUNNER.RECEIPT_FILENAME).exists())

    def test_python_capability_contract_exactly_specializes_repository_golden(self):
        fixture = self.new_fixture()
        identity = RUNNER.CandidateIdentity(
            candidate_dir=fixture.candidate,
            artifact=fixture.artifact,
            artifact_sha256=fixture.artifact_sha256,
            source_commit=SOURCE_COMMIT,
            source_tag=SOURCE_TAG,
        )
        golden = json.loads(CAPABILITY_GOLDEN.read_text(encoding="utf-8"))
        self.assertEqual(digest(CAPABILITY_GOLDEN), RUNNER.CAPABILITY_CONTRACT_SHA256)
        golden["source_commit"] = SOURCE_COMMIT
        golden["source_tag"] = SOURCE_TAG
        actual = RUNNER.expected_performance_capabilities(identity)
        self.assertEqual(actual, golden)
        self.assertEqual(
            (json.dumps(actual, indent=2, sort_keys=False) + "\n").encode("utf-8"),
            (json.dumps(golden, indent=2, sort_keys=False) + "\n").encode("utf-8"),
        )

    def test_success_binds_candidate_process_nonce_time_and_three_outputs(self):
        fixture = self.new_fixture()
        receipt_path = fixture.run()
        self.assertEqual(receipt_path, fixture.output / RUNNER.RECEIPT_FILENAME)
        receipt = json.loads(receipt_path.read_text(encoding="utf-8"))
        self.assertEqual(set(receipt), RUNNER.RECEIPT_KEYS)
        self.assertEqual(receipt["schema"], RUNNER.RECEIPT_SCHEMA)
        self.assertEqual(receipt["candidate_filename"], fixture.artifact.name)
        self.assertEqual(receipt["candidate_sha256"], fixture.artifact_sha256)
        self.assertEqual(receipt["source_commit"], SOURCE_COMMIT)
        self.assertEqual(receipt["source_tag"], SOURCE_TAG)
        self.assertEqual(receipt["session_nonce"], NONCE)
        self.assertEqual(receipt["pid"], 4242)
        self.assertEqual(receipt["started_at_utc"], STARTED)
        self.assertEqual(receipt["completed_at_utc"], COMPLETED)
        self.assertEqual(receipt["exit_code"], 0)
        self.assertIn("--quick-start", receipt["argv"])
        self.assertIn(
            "--release-performance-duration-seconds=1801", receipt["argv"]
        )
        self.assertEqual(len(receipt["executable_sha256"]), 64)
        for key in ("telemetry", "stdout", "stderr"):
            self.assertEqual(set(receipt[key]), RUNNER.FILE_REFERENCE_KEYS)
            output = fixture.output / receipt[key]["path"]
            self.assertTrue(output.is_file())
            self.assertEqual(receipt[key]["sha256"], digest(output))
        self.assertEqual(
            sorted(path.name for path in fixture.output.iterdir()),
            sorted(
                [
                    RUNNER.TELEMETRY_FILENAME,
                    RUNNER.STDOUT_FILENAME,
                    RUNNER.STDERR_FILENAME,
                    RUNNER.RECEIPT_FILENAME,
                ]
            ),
        )
        self.assertEqual(fixture.calls[0][0], "sh")
        self.assertEqual(
            fixture.verifier_input,
            (fixture.root / "scripts/verify_tagged_alpha_candidate.sh").read_bytes(),
        )
        self.assertEqual(fixture.calls[1][0], "/usr/bin/ditto")
        self.assertEqual(
            fixture.calls[2],
            [str(fixture.last_executable), RUNNER.CAPABILITY_ARGUMENT],
        )
        self.assertIn("--release-performance-log=", " ".join(fixture.calls[3]))
        self.assertEqual(
            fixture.extracted_archive_argument,
            "/dev/fd/{}".format(fixture.snapshot_descriptor),
        )
        self.assertEqual(fixture.snapshot_pass_fds, (fixture.snapshot_descriptor,))
        self.assertEqual(fixture.snapshot_descriptor_link_count, 0)
        self.assertFalse(fixture.snapshot_path_existed_at_extract)
        self.assertEqual(
            fixture.extracted_archive_bytes, fixture.artifact.read_bytes()
        )
        with self.assertRaises(OSError):
            os.fstat(fixture.snapshot_descriptor)
        self.assertIsNotNone(fixture.last_executable)
        self.assertFalse(fixture.last_executable.exists())
        self.assertEqual(
            fixture.last_process.wait_timeouts,
            [RUNNER.DEFAULT_DURATION_SECONDS + RUNNER.PROCESS_TIMEOUT_GRACE_SECONDS],
        )

    def test_archive_replacement_after_snapshot_cannot_change_extracted_bytes(self):
        fixture = self.new_fixture()
        original = fixture.artifact.read_bytes()
        fixture.replace_artifact_before_extract = True

        receipt_path = fixture.run(duration=2)
        receipt = json.loads(receipt_path.read_text(encoding="utf-8"))
        self.assertEqual(fixture.artifact.read_bytes(), original)
        self.assertEqual(fixture.extracted_archive_bytes, original)
        self.assertTrue(fixture.extracted_archive_argument.startswith("/dev/fd/"))
        self.assertEqual(receipt["candidate_sha256"], hashlib.sha256(original).hexdigest())

    def test_removed_snapshot_path_can_be_replaced_without_changing_fd_bytes(self):
        fixture = self.new_fixture()
        original = fixture.artifact.read_bytes()
        fixture.replace_snapshot_path_before_extract = True

        fixture.run(duration=2)
        self.assertFalse(fixture.snapshot_path_existed_at_extract)
        self.assertEqual(fixture.snapshot_descriptor_link_count, 0)
        self.assertEqual(fixture.extracted_archive_bytes, original)
        with self.assertRaises(OSError):
            os.fstat(fixture.snapshot_descriptor)

    @unittest.skipUnless(
        sys.platform == "darwin" and Path("/usr/bin/ditto").is_file(),
        "real ditto descriptor extraction requires macOS",
    )
    def test_real_ditto_extracts_from_unlinked_snapshot_descriptor(self):
        with tempfile.TemporaryDirectory(
            prefix="tanks3d-performance-ditto-fd-test-"
        ) as temporary_name:
            root = Path(temporary_name)
            source_archive = root / "source.zip"
            member = "Tanks3D.app/Contents/MacOS/Tanks3D"
            member_info = zipfile.ZipInfo(member)
            member_info.create_system = 3
            member_info.external_attr = 0o100755 << 16
            payload = b"#!/bin/sh\nexit 0\n"
            with zipfile.ZipFile(source_archive, "w") as archive:
                archive.writestr(member_info, payload)
            identity = RUNNER.CandidateIdentity(
                candidate_dir=root,
                artifact=source_archive,
                artifact_sha256=digest(source_archive),
                source_commit=SOURCE_COMMIT,
                source_tag=SOURCE_TAG,
            )
            snapshot_root = root / "snapshot"
            snapshot_root.mkdir(mode=0o700)
            extraction_root = root / "extracted"
            extraction_root.mkdir(mode=0o700)
            descriptor = RUNNER.snapshot_candidate_archive(identity, snapshot_root)
            try:
                self.assertFalse(
                    (snapshot_root / "candidate-archive.snapshot.zip").exists()
                )
                self.assertEqual(os.fstat(descriptor).st_nlink, 0)
                executable = RUNNER.extract_candidate(descriptor, extraction_root)
                self.assertEqual(executable.read_bytes(), payload)
            finally:
                os.close(descriptor)

    def test_archive_symlink_race_is_rejected_by_snapshot_open(self):
        fixture = self.new_fixture()
        replacement = fixture.root / "replacement.zip"
        replacement.write_bytes(fixture.artifact.read_bytes())
        real_parse_candidate = RUNNER.parse_candidate
        swapped = False

        def parse_then_swap(project_root, candidate_dir):
            nonlocal swapped
            result = real_parse_candidate(project_root, candidate_dir)
            if not swapped:
                fixture.artifact.unlink()
                fixture.artifact.symlink_to(replacement)
                swapped = True
            return result

        patches = fixture.patches()
        with patches[0], patches[1], patches[2], patches[3], mock.patch.object(
            RUNNER, "parse_candidate", side_effect=parse_then_swap
        ):
            with self.assertRaisesRegex(RUNNER.RunnerError, "securely open.*snapshot source"):
                RUNNER.run_performance_qa(
                    fixture.root, fixture.candidate, fixture.output, 2
                )
        self.assertTrue(swapped)
        self.assertEqual(list(fixture.output.iterdir()), [])

    def test_archive_regular_replacement_before_snapshot_fails_digest_binding(self):
        fixture = self.new_fixture()
        real_parse_candidate = RUNNER.parse_candidate
        replaced = False

        def parse_then_replace(project_root, candidate_dir):
            nonlocal replaced
            result = real_parse_candidate(project_root, candidate_dir)
            if not replaced:
                fixture.artifact.write_bytes(b"different regular archive after validation\n")
                replaced = True
            return result

        patches = fixture.patches()
        with patches[0], patches[1], patches[2], patches[3], mock.patch.object(
            RUNNER, "parse_candidate", side_effect=parse_then_replace
        ), mock.patch.object(RUNNER.subprocess, "Popen") as popen:
            with self.assertRaisesRegex(
                RUNNER.RunnerError, "snapshot digest does not match"
            ):
                RUNNER.run_performance_qa(
                    fixture.root, fixture.candidate, fixture.output, 2
                )
            popen.assert_not_called()
        self.assertTrue(replaced)
        self.assertEqual(list(fixture.output.iterdir()), [])

    def test_verifier_and_extraction_failures_never_launch_or_publish(self):
        fixture = self.new_fixture()
        fixture.verify_returncode = 9
        with mock.patch.object(RUNNER.subprocess, "Popen") as popen:
            with self.assertRaisesRegex(RUNNER.RunnerError, "tagged candidate verifier failed"):
                fixture.run(duration=2)
            popen.assert_not_called()
        self.assertEqual(list(fixture.output.iterdir()), [])

        fixture = self.new_fixture()
        fixture.extract_returncode = 4
        with mock.patch.object(RUNNER.subprocess, "Popen") as popen:
            with self.assertRaisesRegex(RUNNER.RunnerError, "candidate extraction failed"):
                fixture.run(duration=2)
            popen.assert_not_called()
        self.assertIsNotNone(fixture.snapshot_descriptor)
        with self.assertRaises(OSError):
            os.fstat(fixture.snapshot_descriptor)
        self.assertEqual(list(fixture.output.iterdir()), [])

    def test_legacy_build_config_is_rejected_before_snapshot_or_launch(self):
        fixture = self.new_fixture()
        build_config = fixture.candidate / "build-config.txt"
        build_config.write_text("fixture=true\n", encoding="utf-8")
        attestation = fixture.candidate / "attestation.txt"
        text = attestation.read_text(encoding="utf-8")
        text = re.sub(
            r"^build_config_sha256=.*$",
            "build_config_sha256={}".format(digest(build_config)),
            text,
            flags=re.MULTILINE,
        )
        attestation.write_text(text, encoding="utf-8")

        self.assert_runner_error(fixture, "current performance-capability-schema contract")
        self.assertEqual(len(fixture.calls), 1)
        self.assertIsNone(fixture.last_process)
        self.assertEqual(list(fixture.output.iterdir()), [])

    def test_legacy_candidate_attestation_is_rejected_before_snapshot_or_launch(self):
        fixture = self.new_fixture()
        attestation = fixture.candidate / "attestation.txt"
        text = attestation.read_text(encoding="utf-8").replace(
            "schema=tanks3d-alpha-candidate-v3",
            "schema=tanks3d-alpha-candidate-v2",
            1,
        )
        attestation.write_text(text, encoding="utf-8")

        self.assert_runner_error(fixture, "tanks3d-alpha-candidate-v3")
        self.assertEqual(len(fixture.calls), 1)
        self.assertIsNone(fixture.last_process)
        self.assertEqual(list(fixture.output.iterdir()), [])

    def test_capability_probe_failures_precede_long_run_and_leave_no_outputs(self):
        cases = (
            ("nonzero", "probe failed"),
            ("timeout", "probe timed out"),
            ("stderr", "wrote to stderr"),
            ("wrong_schema", "does not match"),
            ("wrong_identity", "does not match"),
            ("extra_key", "does not match"),
            ("duplicate_key", "duplicate JSON key"),
            ("invalid_utf8", "not UTF-8"),
        )
        for mode, fragment in cases:
            with self.subTest(mode=mode):
                fixture = self.new_fixture()
                fixture.capability_mode = mode
                self.assert_runner_error(fixture, fragment)
                self.assertIsNone(fixture.last_process)
                self.assertEqual(list(fixture.output.iterdir()), [])

    def test_long_run_timeout_terminates_process_and_never_publishes_receipt(self):
        fixture = self.new_fixture()
        fixture.process_hangs = True
        self.assert_runner_error(fixture, "exceeded its 122-second timeout")
        self.assertIsNotNone(fixture.last_process)
        self.assertTrue(fixture.last_process.terminated)
        self.assertFalse(fixture.last_process.killed)
        self.assertEqual(fixture.last_process.wait_timeouts, [122, 5])
        self.assertFalse((fixture.output / RUNNER.RECEIPT_FILENAME).exists())

    def test_long_run_timeout_kills_process_that_ignores_termination(self):
        fixture = self.new_fixture()
        fixture.process_hangs = True
        fixture.process_ignores_terminate = True
        self.assert_runner_error(fixture, "exceeded its 122-second timeout")
        self.assertIsNotNone(fixture.last_process)
        self.assertTrue(fixture.last_process.terminated)
        self.assertTrue(fixture.last_process.killed)
        self.assertEqual(fixture.last_process.wait_timeouts, [122, 5, 5])
        self.assertFalse((fixture.output / RUNNER.RECEIPT_FILENAME).exists())

    def test_verifier_receipt_must_be_exact_and_match_the_captured_candidate(self):
        cases = (
            ("wrong", "does not match the tagged verifier's private snapshot"),
            ("missing", "exact five-file receipt"),
            ("duplicate", "duplicate candidate receipt"),
        )
        for mode, fragment in cases:
            with self.subTest(mode=mode):
                fixture = self.new_fixture()
                fixture.verifier_receipt_mode = mode
                with mock.patch.object(RUNNER.subprocess, "Popen") as popen:
                    self.assert_runner_error(fixture, fragment)
                    popen.assert_not_called()

    def test_verifier_executes_the_bytes_that_were_securely_read(self):
        fixture = self.new_fixture()
        verifier = fixture.root / "scripts/verify_tagged_alpha_candidate.sh"
        verifier.write_text("#!/bin/sh\nexit 91\n", encoding="utf-8")
        original_read = RUNNER.read_regular_file_no_follow

        def swap_after_read(path, label, directory_fd=None):
            data = original_read(path, label, directory_fd=directory_fd)
            if Path(path).name == verifier.name and label == "tagged candidate verifier":
                verifier.write_text("#!/bin/sh\nexit 0\n", encoding="utf-8")
            return data

        with mock.patch.object(
            RUNNER, "read_regular_file_no_follow", side_effect=swap_after_read
        ):
            with self.assertRaisesRegex(
                RUNNER.RunnerError, r"tagged candidate verifier failed \(exit 91\)"
            ):
                RUNNER.invoke_tagged_verifier(fixture.root, fixture.candidate)

    def test_verifier_rejects_a_symlinked_scripts_ancestor(self):
        fixture = self.new_fixture()
        scripts = fixture.root / "scripts"
        moved = fixture.root / "scripts-real"
        scripts.rename(moved)
        scripts.symlink_to(moved, target_is_directory=True)
        with self.assertRaisesRegex(
            RUNNER.RunnerError, "symlinked path component"
        ):
            RUNNER.invoke_tagged_verifier(fixture.root, fixture.candidate)

    def test_process_marker_and_telemetry_failures_do_not_forge_receipt(self):
        cases = [
            ("process", "valid", "valid", 7, "exited 7"),
            ("marker", "missing_complete", "valid", 0, "START/COMPLETE"),
            ("nonce_marker", "wrong_nonce", "valid", 0, "START/COMPLETE"),
            ("missing_telemetry", "valid", "missing", 0, "telemetry.*missing"),
            ("identity", "valid", "wrong_candidate", 0, "does not match"),
            ("unclean", "valid", "unclean", 0, "clean_shutdown=true"),
            ("duplicate", "valid", "duplicate_key", 0, "duplicate JSON key"),
        ]
        for _, marker_mode, telemetry_mode, returncode, expected in cases:
            fixture = self.new_fixture()
            fixture.marker_mode = marker_mode
            fixture.telemetry_mode = telemetry_mode
            fixture.process_returncode = returncode
            self.assert_runner_error(fixture, expected)

    def test_output_must_be_existing_empty_owned_private_and_not_symlink(self):
        fixture = self.new_fixture()
        fixture.output.chmod(0o755)
        with self.assertRaisesRegex(RUNNER.RunnerError, "must be private"):
            fixture.run(duration=2)

        fixture = self.new_fixture()
        (fixture.output / "existing.txt").write_text("occupied\n", encoding="utf-8")
        with self.assertRaisesRegex(RUNNER.RunnerError, "must be empty"):
            fixture.run(duration=2)

        fixture = self.new_fixture()
        real_output = fixture.output
        output_link = fixture.root / "output-link"
        output_link.symlink_to(real_output, target_is_directory=True)
        patches = fixture.patches()
        with patches[0], patches[1], patches[2], patches[3]:
            with self.assertRaisesRegex(RUNNER.RunnerError, "must not be a symbolic link"):
                RUNNER.run_performance_qa(
                    fixture.root, fixture.candidate, output_link, 2
                )
        self.assertEqual(list(real_output.iterdir()), [])

        fixture = self.new_fixture()
        missing = fixture.root / "missing-output"
        patches = fixture.patches()
        with patches[0], patches[1], patches[2], patches[3]:
            with self.assertRaisesRegex(RUNNER.RunnerError, "not accessible"):
                RUNNER.run_performance_qa(
                    fixture.root, fixture.candidate, missing, 2
                )

    def test_candidate_symlink_and_hash_tampering_are_rejected_after_verification(self):
        fixture = self.new_fixture()
        candidate_link = fixture.root / "candidate-link"
        candidate_link.symlink_to(fixture.candidate, target_is_directory=True)
        patches = fixture.patches()
        with patches[0], patches[1], patches[2], patches[3]:
            with self.assertRaisesRegex(RUNNER.RunnerError, "must not be a symbolic link"):
                RUNNER.run_performance_qa(
                    fixture.root, candidate_link, fixture.output, 2
                )

        fixture = self.new_fixture()
        fixture.artifact.write_bytes(fixture.artifact.read_bytes() + b"tampered")
        with self.assertRaisesRegex(RUNNER.RunnerError, "archive digest"):
            fixture.run(duration=2)
        self.assertEqual(list(fixture.output.iterdir()), [])

    def test_receipt_publish_is_atomic_and_refuses_a_racing_destination(self):
        fixture = self.new_fixture()
        fixture.receipt_race = True
        with self.assertRaisesRegex(RUNNER.RunnerError, "refusing to replace"):
            fixture.run(duration=2)
        receipt = fixture.output / RUNNER.RECEIPT_FILENAME
        self.assertEqual(receipt.read_text(encoding="utf-8"), "sentinel receipt must survive\n")
        self.assertEqual(
            list(fixture.output.glob(".performance-qa-receipt.*")), []
        )

    def test_duration_and_nonce_are_strict(self):
        fixture = self.new_fixture()
        patches = fixture.patches()
        with patches[0], patches[1], patches[2], patches[3]:
            with self.assertRaisesRegex(RUNNER.RunnerError, "between 1"):
                RUNNER.run_performance_qa(
                    fixture.root, fixture.candidate, fixture.output, 0
                )

        fixture = self.new_fixture()
        patches = fixture.patches()
        with patches[0], patches[1], patches[2], patches[3]:
            with self.assertRaisesRegex(RUNNER.RunnerError, "between 1"):
                RUNNER.run_performance_qa(
                    fixture.root,
                    fixture.candidate,
                    fixture.output,
                    4 * 60 * 60 + 1,
                )

        fixture = self.new_fixture()
        with mock.patch.object(RUNNER.subprocess, "run", side_effect=fixture.fake_run), mock.patch.object(
            RUNNER.subprocess, "Popen", side_effect=fixture.fake_popen
        ), mock.patch.object(RUNNER.secrets, "token_hex", return_value="not-a-nonce"), mock.patch.object(
            RUNNER, "utc_now", side_effect=[STARTED, COMPLETED]
        ):
            with self.assertRaisesRegex(RUNNER.RunnerError, "nonce generator"):
                RUNNER.run_performance_qa(
                    fixture.root, fixture.candidate, fixture.output, 2
                )


if __name__ == "__main__":
    unittest.main()
