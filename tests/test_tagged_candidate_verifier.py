#!/usr/bin/env python3
"""Lifecycle and receipt tests for the bounded tagged-candidate verifier."""

import os
from pathlib import Path
import selectors
import signal
import subprocess
import sys
import tempfile
import time
import unittest
from unittest import mock


sys.dont_write_bytecode = True
REPOSITORY_ROOT = Path(__file__).resolve().parents[1]
SCRIPT_DIRECTORY = REPOSITORY_ROOT / "scripts"
if str(SCRIPT_DIRECTORY) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIRECTORY))

import tagged_candidate_verifier as VERIFIER  # noqa: E402


NAMES = (
    "Tanks3D.zip",
    "Tanks3D.zip.sha256",
    "alpha-candidate-gates.log",
    "attestation.txt",
    "build-config.txt",
)


def receipt_bytes(names=NAMES):
    return b"".join(
        "VERIFIED CANDIDATE FILE SHA256 {} {}\n".format(
            format(index + 1, "064x"), name
        ).encode("ascii")
        for index, name in enumerate(names)
    )


def literal_printf(payload, descriptor=None):
    escaped = payload.replace(b"'", b"'\\''")
    suffix = b"" if descriptor is None else b" >&" + str(descriptor).encode("ascii")
    return b"printf '%s' '" + escaped + b"'" + suffix + b"\n"


def pid_exists(pid):
    try:
        os.kill(pid, 0)
        return True
    except ProcessLookupError:
        return False


def wait_for_pid_exit(test_case, pid, timeout=2.0):
    deadline = time.monotonic() + timeout
    while pid_exists(pid) and time.monotonic() < deadline:
        time.sleep(0.01)
    test_case.assertFalse(pid_exists(pid), "process {} was not reaped".format(pid))


class InterruptingSelector:
    """Real registrations with one synthetic supervisor interruption."""

    def __init__(self):
        self.inner = selectors.DefaultSelector()

    def register(self, *args, **kwargs):
        return self.inner.register(*args, **kwargs)

    def unregister(self, *args, **kwargs):
        return self.inner.unregister(*args, **kwargs)

    def select(self, _timeout=None):
        raise KeyboardInterrupt()

    def close(self):
        self.inner.close()


class TaggedCandidateVerifierTests(unittest.TestCase):
    def invoke(self, script, **kwargs):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            candidate = root / "candidate"
            candidate.mkdir()
            return VERIFIER.invoke(script, root, candidate, **kwargs)

    def test_success_returns_exact_receipt_and_bounded_output(self):
        diagnostic = b"tagged verifier fixture\n"
        script = literal_printf(diagnostic) + literal_printf(receipt_bytes())
        result = self.invoke(script)
        self.assertEqual(result.stdout, diagnostic + receipt_bytes())
        self.assertEqual(result.stderr, b"")
        self.assertEqual(set(result.receipt), set(NAMES))

    def test_stdout_and_stderr_accept_exact_limit_and_reject_one_more_byte(self):
        receipt = receipt_bytes()
        stderr = b"diagnostic-at-the-exact-limit"
        script = literal_printf(receipt) + literal_printf(stderr, descriptor=2)
        result = self.invoke(
            script,
            stdout_limit=len(receipt),
            stderr_limit=len(stderr),
        )
        self.assertEqual(result.stderr, stderr)

        with self.assertRaisesRegex(
            VERIFIER.TaggedVerifierError, "stdout exceeded.*-byte limit"
        ):
            self.invoke(
                literal_printf(b"X" + receipt),
                stdout_limit=len(receipt),
                stderr_limit=1024,
            )
        with self.assertRaisesRegex(
            VERIFIER.TaggedVerifierError, "stderr exceeded.*-byte limit"
        ):
            self.invoke(
                literal_printf(receipt)
                + literal_printf(stderr + b"X", descriptor=2),
                stdout_limit=len(receipt),
                stderr_limit=len(stderr),
            )

    def test_concurrent_pipe_output_cannot_deadlock_or_escape_limits(self):
        chunk = b"0123456789abcdef" * 64
        script = (
            b"i=0\n"
            b"while [ \"$i\" -lt 128 ]; do\n"
            + literal_printf(chunk)
            + literal_printf(chunk, descriptor=2)
            + b"i=$((i + 1))\n"
            b"done\n"
            b"printf '\\n'\n"
            + literal_printf(receipt_bytes())
        )
        result = self.invoke(
            script,
            timeout_seconds=3,
            stdout_limit=256 * 1024,
            stderr_limit=256 * 1024,
        )
        self.assertGreater(len(result.stdout), 128 * 1024)
        self.assertEqual(len(result.stderr), 128 * 1024)

        flood = b"while :; do printf '0123456789abcdef'; done\n"
        started = time.monotonic()
        with mock.patch.object(VERIFIER, "TERMINATE_GRACE_SECONDS", 0.1), mock.patch.object(
            VERIFIER, "KILL_GRACE_SECONDS", 0.2
        ):
            with self.assertRaisesRegex(
                VERIFIER.TaggedVerifierError, "stdout exceeded the 4096-byte limit"
            ):
                self.invoke(
                    flood,
                    timeout_seconds=2,
                    stdout_limit=4096,
                    stderr_limit=4096,
                )
        self.assertLess(time.monotonic() - started, 1.5)

    def test_timeout_kills_and_reaps_the_private_process_group(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            candidate = root / "candidate"
            candidate.mkdir()
            pid_path = candidate / "leader.pid"
            script = (
                b"printf '%s\\n' \"$$\" > \"$2/leader.pid\"\n"
                b"trap '' TERM\n"
                b"while :; do sleep 1; done\n"
            )
            with mock.patch.object(
                VERIFIER, "TERMINATE_GRACE_SECONDS", 0.1
            ), mock.patch.object(VERIFIER, "KILL_GRACE_SECONDS", 0.2):
                with self.assertRaisesRegex(
                    VERIFIER.TaggedVerifierError, "timed out after 0.2 seconds"
                ):
                    VERIFIER.invoke(
                        script,
                        root,
                        candidate,
                        timeout_seconds=0.2,
                        stdout_limit=4096,
                        stderr_limit=4096,
                    )
            self.assertTrue(pid_path.is_file())
            wait_for_pid_exit(self, int(pid_path.read_text(encoding="ascii")))

    def test_exited_leader_cannot_leave_a_pipe_holding_descendant(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            candidate = root / "candidate"
            candidate.mkdir()
            child_path = candidate / "child.pid"
            script = (
                b"/bin/sh -c 'printf \"%s\\n\" \"$$\" > \"$1\"; "
                b"trap \"\" TERM; while :; do sleep 1; done' child "
                b"\"$2/child.pid\" &\n"
                b"exit 0\n"
            )
            with mock.patch.object(
                VERIFIER, "PIPE_DRAIN_GRACE_SECONDS", 0.1
            ), mock.patch.object(
                VERIFIER, "TERMINATE_GRACE_SECONDS", 0.1
            ), mock.patch.object(VERIFIER, "KILL_GRACE_SECONDS", 0.2):
                with self.assertRaisesRegex(
                    VERIFIER.TaggedVerifierError,
                    "descendants kept output pipes open",
                ):
                    VERIFIER.invoke(
                        script,
                        root,
                        candidate,
                        timeout_seconds=2,
                        stdout_limit=4096,
                        stderr_limit=4096,
                    )
            self.assertTrue(child_path.is_file())
            wait_for_pid_exit(self, int(child_path.read_text(encoding="ascii")))

    def test_keyboard_interrupt_cleans_up_then_propagates(self):
        real_popen = VERIFIER.POPEN_CLASS
        processes = []

        def recording_popen(*args, **kwargs):
            process = real_popen(*args, **kwargs)
            processes.append(process)
            return process

        with mock.patch.object(
            VERIFIER, "POPEN_CLASS", side_effect=recording_popen
        ), mock.patch.object(
            VERIFIER, "SELECTOR_CLASS", InterruptingSelector
        ), mock.patch.object(
            VERIFIER, "TERMINATE_GRACE_SECONDS", 0.1
        ), mock.patch.object(VERIFIER, "KILL_GRACE_SECONDS", 0.2):
            with self.assertRaises(KeyboardInterrupt):
                self.invoke(b"trap '' TERM\nwhile :; do sleep 1; done\n")
        self.assertEqual(len(processes), 1)
        self.assertIsNotNone(processes[0].returncode)
        wait_for_pid_exit(self, processes[0].pid)

    def test_sigterm_is_converted_only_after_the_child_group_is_reaped(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            candidate = root / "candidate"
            candidate.mkdir()
            pid_path = candidate / "leader.pid"
            verifier_script = (
                b"printf '%s\\n' \"$$\" > \"$2/leader.pid\"\n"
                b"trap '' TERM\n"
                b"while :; do sleep 1; done\n"
            )
            child_code = (
                "import pathlib,sys\n"
                "sys.path.insert(0, {!r})\n"
                "import tagged_candidate_verifier as v\n"
                "v.TERMINATE_GRACE_SECONDS = 0.2\n"
                "v.KILL_GRACE_SECONDS = 0.3\n"
                "try:\n"
                " v.invoke({!r}, pathlib.Path({!r}), pathlib.Path({!r}), "
                "timeout_seconds=10)\n"
                "except v.TaggedVerifierError as exc:\n"
                " print(str(exc), file=sys.stderr)\n"
                " raise SystemExit(3)\n"
            ).format(
                str(SCRIPT_DIRECTORY),
                verifier_script,
                str(root),
                str(candidate),
            )
            supervisor = subprocess.Popen(
                [sys.executable, "-B", "-c", child_code],
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
            )
            try:
                deadline = time.monotonic() + 3
                while not pid_path.is_file() and time.monotonic() < deadline:
                    time.sleep(0.01)
                self.assertTrue(pid_path.is_file())
                os.kill(supervisor.pid, signal.SIGTERM)
                stdout, stderr = supervisor.communicate(timeout=4)
            finally:
                if supervisor.poll() is None:
                    supervisor.kill()
                    supervisor.communicate(timeout=2)
            self.assertEqual(supervisor.returncode, 3, stdout + stderr)
            self.assertIn("interrupted by signal {}".format(signal.SIGTERM), stderr)
            wait_for_pid_exit(self, int(pid_path.read_text(encoding="ascii")))

    def test_sigterm_interrupts_after_both_output_pipes_have_closed(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            candidate = root / "candidate"
            candidate.mkdir()
            pid_path = candidate / "leader.pid"
            verifier_script = (
                b"printf '%s\\n' \"$$\" > \"$2/leader.pid\"\n"
                b"exec >/dev/null 2>/dev/null\n"
                b"trap '' TERM\n"
                b"sleep 5\n"
            )
            child_code = (
                "import pathlib,sys\n"
                "sys.path.insert(0, {!r})\n"
                "import tagged_candidate_verifier as v\n"
                "v.TERMINATE_GRACE_SECONDS = 0.2\n"
                "v.KILL_GRACE_SECONDS = 0.3\n"
                "try:\n"
                " v.invoke({!r}, pathlib.Path({!r}), pathlib.Path({!r}), "
                "timeout_seconds=10)\n"
                "except v.TaggedVerifierError as exc:\n"
                " print(str(exc), file=sys.stderr)\n"
                " raise SystemExit(3)\n"
            ).format(
                str(SCRIPT_DIRECTORY),
                verifier_script,
                str(root),
                str(candidate),
            )
            supervisor = subprocess.Popen(
                [sys.executable, "-B", "-c", child_code],
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
            )
            try:
                deadline = time.monotonic() + 3
                while not pid_path.is_file() and time.monotonic() < deadline:
                    time.sleep(0.01)
                self.assertTrue(pid_path.is_file())
                started = time.monotonic()
                os.kill(supervisor.pid, signal.SIGTERM)
                stdout, stderr = supervisor.communicate(timeout=2)
                elapsed = time.monotonic() - started
            finally:
                if supervisor.poll() is None:
                    supervisor.kill()
                    supervisor.communicate(timeout=2)
            self.assertEqual(supervisor.returncode, 3, stdout + stderr)
            self.assertLess(elapsed, 1.5)
            self.assertIn("interrupted by signal {}".format(signal.SIGTERM), stderr)
            wait_for_pid_exit(self, int(pid_path.read_text(encoding="ascii")))

    def test_signal_after_pipe_drain_cannot_yield_a_receipt(self):
        original_parse = VERIFIER._parse_receipt
        previous_term = signal.getsignal(signal.SIGTERM)

        def parse_then_signal(stdout):
            receipt = original_parse(stdout)
            os.kill(os.getpid(), signal.SIGTERM)
            return receipt

        with mock.patch.object(VERIFIER, "_parse_receipt", parse_then_signal):
            with self.assertRaisesRegex(
                VERIFIER.TaggedVerifierError,
                "interrupted by signal {}".format(signal.SIGTERM),
            ):
                self.invoke(literal_printf(receipt_bytes()))
        self.assertIs(signal.getsignal(signal.SIGTERM), previous_term)

        def parse_then_interrupt(stdout):
            receipt = original_parse(stdout)
            os.kill(os.getpid(), signal.SIGINT)
            return receipt

        with mock.patch.object(VERIFIER, "_parse_receipt", parse_then_interrupt):
            with self.assertRaises(KeyboardInterrupt):
                self.invoke(literal_printf(receipt_bytes()))

    def test_nonzero_and_receipt_failures_are_fail_closed(self):
        diagnostic = b"A" * 900 + b"TAIL\n"
        with self.assertRaises(VERIFIER.TaggedVerifierError) as failure:
            self.invoke(literal_printf(diagnostic, descriptor=2) + b"exit 9\n")
        message = str(failure.exception)
        self.assertIn("failed (exit 9)", message)
        self.assertTrue(message.endswith("TAIL"))
        self.assertLessEqual(len(message), 900)

        cases = (
            (receipt_bytes(NAMES[:4]), "exact five-file receipt"),
            (receipt_bytes(NAMES + ("extra.txt",)), "exact five-file receipt"),
            (receipt_bytes() + receipt_bytes(NAMES[:1]), "duplicate candidate receipt"),
            (b"VERIFIED CANDIDATE FILE SHA256 bad name\n", "invalid candidate receipt"),
            (b"printf '\\377'\n" + literal_printf(receipt_bytes()), "not UTF-8"),
        )
        for script_output, expected in cases:
            with self.subTest(expected=expected):
                script = (
                    script_output
                    if script_output.startswith(b"printf ")
                    else literal_printf(script_output)
                )
                with self.assertRaisesRegex(VERIFIER.TaggedVerifierError, expected):
                    self.invoke(script)

    def test_spawn_failure_is_normalized(self):
        with mock.patch.object(
            VERIFIER, "POPEN_CLASS", side_effect=OSError("fixture spawn failure")
        ):
            with self.assertRaisesRegex(
                VERIFIER.TaggedVerifierError,
                "cannot run tagged candidate verifier: fixture spawn failure",
            ):
                self.invoke(literal_printf(receipt_bytes()))

        real_popen = VERIFIER.POPEN_CLASS
        processes = []

        def recording_popen(*args, **kwargs):
            process = real_popen(*args, **kwargs)
            processes.append(process)
            return process

        with mock.patch.object(
            VERIFIER, "POPEN_CLASS", side_effect=recording_popen
        ), mock.patch.object(
            VERIFIER, "SELECTOR_CLASS", side_effect=OSError("fixture selector failure")
        ):
            with self.assertRaisesRegex(
                VERIFIER.TaggedVerifierError,
                "supervision failed: fixture selector failure",
            ):
                self.invoke(b"while :; do sleep 1; done\n")
        self.assertEqual(len(processes), 1)
        self.assertIsNotNone(processes[0].returncode)

    def test_cli_reads_the_canonical_source_without_following_scripts_symlinks(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            scripts = root / "scripts"
            candidate = root / "candidate"
            scripts.mkdir()
            candidate.mkdir()
            verifier_path = scripts / "verify_tagged_alpha_candidate.sh"
            verifier_path.write_bytes(literal_printf(receipt_bytes()))
            command = [
                sys.executable,
                "-B",
                str(SCRIPT_DIRECTORY / "tagged_candidate_verifier.py"),
                "--project-root",
                str(root),
                "--candidate-dir",
                str(candidate),
            ]
            completed = subprocess.run(
                command,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                check=False,
                timeout=3,
            )
            self.assertEqual(
                completed.returncode, 0, completed.stdout + completed.stderr
            )
            self.assertEqual(completed.stdout, receipt_bytes())

            moved = root / "scripts-real"
            scripts.rename(moved)
            scripts.symlink_to(moved, target_is_directory=True)
            rejected = subprocess.run(
                command,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                check=False,
                timeout=3,
            )
            self.assertNotEqual(rejected.returncode, 0)
            self.assertIn(b"securely read tagged candidate verifier", rejected.stderr)


if __name__ == "__main__":
    unittest.main()
