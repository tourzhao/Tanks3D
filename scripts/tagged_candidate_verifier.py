#!/usr/bin/env python3
"""Bound and parse the tagged Alpha candidate verifier.

Release tools pass verifier source bytes that they have already opened with
their repository-specific no-follow checks.  This module supplies the shared
process contract: a private process group, an execution deadline, bounded and
concurrently drained output pipes, deterministic group cleanup, and an exact
five-file SHA-256 receipt.
"""

import argparse
from dataclasses import dataclass
import errno
import os
from pathlib import Path
import re
import selectors
import signal
import stat
import subprocess
import sys
import tempfile
import threading
import time
from typing import Dict, Mapping, Optional, Sequence, Tuple


DEFAULT_TIMEOUT_SECONDS = 600.0
DEFAULT_STDOUT_LIMIT_BYTES = 1024 * 1024
DEFAULT_STDERR_LIMIT_BYTES = 1024 * 1024
TERMINATE_GRACE_SECONDS = 5.0
KILL_GRACE_SECONDS = 5.0
PIPE_DRAIN_GRACE_SECONDS = 2.0
SUPERVISOR_INTERVAL_SECONDS = 0.1
READ_CHUNK_BYTES = 64 * 1024
MAX_VERIFIER_SOURCE_BYTES = 1024 * 1024
RECEIPT_PREFIX = b"VERIFIED CANDIDATE FILE SHA256 "
SHA256_RE = re.compile(r"^[0-9a-f]{64}$")
TOKEN_RE = re.compile(r"^[A-Za-z0-9][A-Za-z0-9._-]*$")
POPEN_CLASS = subprocess.Popen
SELECTOR_CLASS = selectors.DefaultSelector


class TaggedVerifierError(Exception):
    """The tagged verifier could not be executed or did not prove a candidate."""


@dataclass(frozen=True)
class TaggedVerifierResult:
    """Bounded output and parsed receipt from one successful invocation."""

    stdout: bytes
    stderr: bytes
    receipt: Mapping[str, str]


def _validate_invocation(
    verifier_bytes: bytes,
    timeout_seconds: float,
    stdout_limit: int,
    stderr_limit: int,
) -> None:
    if not isinstance(verifier_bytes, bytes):
        raise TaggedVerifierError("tagged candidate verifier source must be bytes")
    if not verifier_bytes:
        raise TaggedVerifierError("tagged candidate verifier source is empty")
    if len(verifier_bytes) > MAX_VERIFIER_SOURCE_BYTES:
        raise TaggedVerifierError(
            "tagged candidate verifier source exceeds the {}-byte limit".format(
                MAX_VERIFIER_SOURCE_BYTES
            )
        )
    if timeout_seconds <= 0:
        raise TaggedVerifierError("tagged candidate verifier timeout must be positive")
    if stdout_limit <= 0 or stderr_limit <= 0:
        raise TaggedVerifierError("tagged candidate verifier output limits must be positive")


def _process_group_exists(process_group: int) -> bool:
    try:
        os.killpg(process_group, 0)
        return True
    except ProcessLookupError:
        return False
    except PermissionError:
        return True


def _signal_process_group(process_group: int, signal_number: int) -> None:
    try:
        os.killpg(process_group, signal_number)
    except ProcessLookupError:
        pass
    except OSError as exc:
        # Darwin can report EPERM for a just-exited session leader between the
        # existence probe and killpg().  The subsequent bounded group-exit
        # check still fails closed if any live descendant actually remains.
        if exc.errno not in (errno.ESRCH, errno.EPERM):
            raise TaggedVerifierError(
                "cannot signal tagged candidate verifier process group: {}".format(exc)
            )


def _wait_for_group_exit(process: subprocess.Popen, seconds: float) -> bool:
    deadline = time.monotonic() + max(0.0, seconds)
    while True:
        process.poll()
        if process.returncode is not None and not _process_group_exists(process.pid):
            return True
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            return False
        time.sleep(min(SUPERVISOR_INTERVAL_SECONDS, remaining))


def _stop_and_reap(process: subprocess.Popen) -> None:
    """Terminate the entire private group and synchronously reap its leader."""

    cleanup_error: Optional[BaseException] = None
    try:
        if process.poll() is None or _process_group_exists(process.pid):
            _signal_process_group(process.pid, signal.SIGTERM)
            if not _wait_for_group_exit(process, TERMINATE_GRACE_SECONDS):
                _signal_process_group(process.pid, signal.SIGKILL)
                if not _wait_for_group_exit(process, KILL_GRACE_SECONDS):
                    cleanup_error = TaggedVerifierError(
                        "tagged candidate verifier process group survived SIGKILL"
                    )
        try:
            process.wait(timeout=KILL_GRACE_SECONDS)
        except subprocess.TimeoutExpired:
            _signal_process_group(process.pid, signal.SIGKILL)
            try:
                process.wait(timeout=KILL_GRACE_SECONDS)
            except subprocess.TimeoutExpired:
                cleanup_error = TaggedVerifierError(
                    "tagged candidate verifier leader could not be reaped after SIGKILL"
                )
    except BaseException as exc:  # Preserve cleanup failure for the caller.
        cleanup_error = exc
    if cleanup_error is not None:
        if isinstance(cleanup_error, TaggedVerifierError):
            raise cleanup_error
        raise TaggedVerifierError(
            "cannot clean up tagged candidate verifier: {}".format(cleanup_error)
        )


def _close_stream(stream: object) -> None:
    try:
        stream.close()  # type: ignore[attr-defined]
    except (OSError, ValueError):
        pass


def _parse_receipt(stdout: bytes) -> Dict[str, str]:
    try:
        text = stdout.decode("utf-8")
    except UnicodeError as exc:
        raise TaggedVerifierError(
            "tagged verifier output is not UTF-8: {}".format(exc)
        )
    receipt: Dict[str, str] = {}
    prefix = RECEIPT_PREFIX.decode("ascii")
    for line in text.splitlines():
        if not line.startswith(prefix):
            continue
        fields = line[len(prefix) :].split(" ", 1)
        if len(fields) != 2:
            raise TaggedVerifierError(
                "tagged verifier emitted a malformed candidate receipt"
            )
        digest, name = fields
        if SHA256_RE.fullmatch(digest) is None or TOKEN_RE.fullmatch(name) is None:
            raise TaggedVerifierError(
                "tagged verifier emitted an invalid candidate receipt"
            )
        if name in receipt:
            raise TaggedVerifierError(
                "tagged verifier emitted a duplicate candidate receipt"
            )
        receipt[name] = digest
    if len(receipt) != 5:
        raise TaggedVerifierError(
            "tagged verifier did not emit an exact five-file receipt"
        )
    return receipt


def invoke(
    verifier_bytes: bytes,
    project_root: Path,
    candidate_dir: Path,
    *,
    timeout_seconds: float = DEFAULT_TIMEOUT_SECONDS,
    stdout_limit: int = DEFAULT_STDOUT_LIMIT_BYTES,
    stderr_limit: int = DEFAULT_STDERR_LIMIT_BYTES,
) -> TaggedVerifierResult:
    """Execute captured verifier bytes and return its bounded exact receipt."""

    _validate_invocation(
        verifier_bytes, timeout_seconds, stdout_limit, stderr_limit
    )
    received_signal = [0]
    previous_handlers: Dict[int, object] = {}
    managed_signals = (signal.SIGHUP, signal.SIGINT, signal.SIGTERM)

    def record_signal(signal_number: int, _frame: object) -> None:
        received_signal[0] = signal_number

    def interruption_error() -> BaseException:
        if received_signal[0] == signal.SIGINT:
            return KeyboardInterrupt()
        return TaggedVerifierError(
            "tagged candidate verifier interrupted by signal {}".format(
                received_signal[0]
            )
        )

    can_manage_signals = threading.current_thread() is threading.main_thread()
    if can_manage_signals:
        for signal_number in managed_signals:
            previous_handlers[signal_number] = signal.getsignal(signal_number)
            signal.signal(signal_number, record_signal)

    process: Optional[subprocess.Popen] = None
    selector: Optional[selectors.BaseSelector] = None
    streams: Dict[int, Tuple[str, bytearray, int, object]] = {}
    primary_error: Optional[BaseException] = None
    stdout = bytearray()
    stderr = bytearray()
    try:
        with tempfile.TemporaryFile(mode="w+b") as verifier_input:
            verifier_input.write(verifier_bytes)
            verifier_input.flush()
            verifier_input.seek(0)
            if received_signal[0]:
                raise interruption_error()
            try:
                process = POPEN_CLASS(
                    [
                        "/bin/sh",
                        "-s",
                        "--",
                        str(project_root),
                        str(candidate_dir),
                    ],
                    stdin=verifier_input,
                    stdout=subprocess.PIPE,
                    stderr=subprocess.PIPE,
                    bufsize=0,
                    close_fds=True,
                    start_new_session=True,
                )
            except (OSError, subprocess.SubprocessError) as exc:
                raise TaggedVerifierError(
                    "cannot run tagged candidate verifier: {}".format(exc)
                )
            if process.stdout is None or process.stderr is None:
                raise TaggedVerifierError(
                    "tagged candidate verifier did not expose both output pipes"
                )
            selector = SELECTOR_CLASS()
            for label, stream, buffer, limit in (
                ("stdout", process.stdout, stdout, stdout_limit),
                ("stderr", process.stderr, stderr, stderr_limit),
            ):
                descriptor = stream.fileno()
                os.set_blocking(descriptor, False)
                selector.register(descriptor, selectors.EVENT_READ)
                streams[descriptor] = (label, buffer, limit, stream)

            deadline = time.monotonic() + timeout_seconds
            leader_exit_seen_at: Optional[float] = None
            while streams:
                now = time.monotonic()
                if received_signal[0]:
                    raise interruption_error()
                if now >= deadline:
                    raise TaggedVerifierError(
                        "tagged candidate verifier timed out after {:.3g} seconds".format(
                            timeout_seconds
                        )
                    )
                returncode = process.poll()
                if returncode is not None:
                    if leader_exit_seen_at is None:
                        leader_exit_seen_at = now
                    elif now - leader_exit_seen_at >= PIPE_DRAIN_GRACE_SECONDS:
                        raise TaggedVerifierError(
                            "tagged candidate verifier descendants kept output pipes open"
                        )
                wait_seconds = min(
                    SUPERVISOR_INTERVAL_SECONDS, max(0.0, deadline - now)
                )
                try:
                    ready = selector.select(wait_seconds)
                except InterruptedError:
                    continue
                for key, _mask in ready:
                    descriptor = key.fd
                    record = streams.get(descriptor)
                    if record is None:
                        continue
                    label, buffer, limit, stream = record
                    try:
                        chunk = os.read(descriptor, READ_CHUNK_BYTES)
                    except BlockingIOError:
                        continue
                    except OSError as exc:
                        raise TaggedVerifierError(
                            "cannot read tagged candidate verifier {}: {}".format(
                                label, exc
                            )
                        )
                    if not chunk:
                        selector.unregister(descriptor)
                        streams.pop(descriptor, None)
                        _close_stream(stream)
                        continue
                    remaining = limit - len(buffer)
                    if remaining > 0:
                        buffer.extend(chunk[:remaining])
                    if len(chunk) > remaining:
                        raise TaggedVerifierError(
                            "tagged candidate verifier {} exceeded the {}-byte limit".format(
                                label, limit
                            )
                        )

            while True:
                if received_signal[0]:
                    raise interruption_error()
                now = time.monotonic()
                if now >= deadline:
                    raise TaggedVerifierError(
                        "tagged candidate verifier timed out after {:.3g} seconds".format(
                            timeout_seconds
                        )
                    )
                returncode = process.poll()
                if returncode is not None:
                    break
                time.sleep(
                    min(SUPERVISOR_INTERVAL_SECONDS, max(0.0, deadline - now))
                )
            if _process_group_exists(process.pid):
                raise TaggedVerifierError(
                    "tagged candidate verifier left descendant processes running"
                )
            if returncode != 0:
                diagnostic = bytes(stderr or stdout).decode(
                    "utf-8", errors="replace"
                ).strip()
                if len(diagnostic) > 800:
                    diagnostic = diagnostic[-800:]
                raise TaggedVerifierError(
                    "tagged candidate verifier failed (exit {}): {}".format(
                        returncode, diagnostic or "no diagnostic"
                    )
                )
            receipt = _parse_receipt(bytes(stdout))
            return TaggedVerifierResult(
                stdout=bytes(stdout), stderr=bytes(stderr), receipt=receipt
            )
    except TaggedVerifierError as exc:
        primary_error = exc
        raise
    except (OSError, subprocess.SubprocessError) as exc:
        normalized = TaggedVerifierError(
            "tagged candidate verifier supervision failed: {}".format(exc)
        )
        primary_error = normalized
        raise normalized from exc
    except BaseException as exc:
        primary_error = exc
        raise
    finally:
        cleanup_error: Optional[BaseException] = None
        if process is not None:
            try:
                if process.poll() is None or _process_group_exists(process.pid):
                    _stop_and_reap(process)
                else:
                    process.wait(timeout=KILL_GRACE_SECONDS)
            except BaseException as exc:
                cleanup_error = exc
        if selector is not None:
            try:
                selector.close()
            except OSError:
                pass
        for _label, _buffer, _limit, stream in list(streams.values()):
            _close_stream(stream)
        if process is not None:
            if process.stdout is not None:
                _close_stream(process.stdout)
            if process.stderr is not None:
                _close_stream(process.stderr)
        if can_manage_signals:
            for signal_number, previous in previous_handlers.items():
                signal.signal(signal_number, previous)
        if cleanup_error is not None:
            if primary_error is not None:
                raise cleanup_error from primary_error
            raise cleanup_error
        if primary_error is None and received_signal[0]:
            raise interruption_error()


def _read_verifier_source(project_root: Path) -> bytes:
    """Securely read the canonical shell verifier for the Make/CLI entry point."""

    try:
        root_metadata = project_root.lstat()
    except OSError as exc:
        raise TaggedVerifierError("project root is not accessible: {}".format(exc))
    if stat.S_ISLNK(root_metadata.st_mode) or not stat.S_ISDIR(root_metadata.st_mode):
        raise TaggedVerifierError("project root must be a real directory")
    root = project_root.resolve(strict=True)
    directory_flags = os.O_RDONLY | getattr(os, "O_DIRECTORY", 0)
    directory_flags |= getattr(os, "O_NOFOLLOW", 0) | getattr(os, "O_CLOEXEC", 0)
    file_flags = os.O_RDONLY | getattr(os, "O_NONBLOCK", 0)
    file_flags |= getattr(os, "O_NOFOLLOW", 0) | getattr(os, "O_CLOEXEC", 0)
    root_fd = scripts_fd = verifier_fd = -1
    try:
        root_fd = os.open(str(root), directory_flags)
        scripts_fd = os.open("scripts", directory_flags, dir_fd=root_fd)
        verifier_fd = os.open(
            "verify_tagged_alpha_candidate.sh", file_flags, dir_fd=scripts_fd
        )
        before = os.fstat(verifier_fd)
        if not stat.S_ISREG(before.st_mode):
            raise TaggedVerifierError("tagged candidate verifier must be a regular file")
        if before.st_size > MAX_VERIFIER_SOURCE_BYTES:
            raise TaggedVerifierError(
                "tagged candidate verifier source exceeds the {}-byte limit".format(
                    MAX_VERIFIER_SOURCE_BYTES
                )
            )
        chunks = []
        total = 0
        while True:
            chunk = os.read(verifier_fd, READ_CHUNK_BYTES)
            if not chunk:
                break
            total += len(chunk)
            if total > MAX_VERIFIER_SOURCE_BYTES:
                raise TaggedVerifierError(
                    "tagged candidate verifier source exceeds the {}-byte limit".format(
                        MAX_VERIFIER_SOURCE_BYTES
                    )
                )
            chunks.append(chunk)
        after = os.fstat(verifier_fd)
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
            raise TaggedVerifierError(
                "tagged candidate verifier changed while it was read"
            )
        return data
    except TaggedVerifierError:
        raise
    except OSError as exc:
        raise TaggedVerifierError(
            "cannot securely read tagged candidate verifier: {}".format(exc)
        )
    finally:
        for descriptor in (verifier_fd, scripts_fd, root_fd):
            if descriptor >= 0:
                os.close(descriptor)


def main(argv: Optional[Sequence[str]] = None) -> int:
    parser = argparse.ArgumentParser(
        description="Run the tagged Alpha verifier with bounded resources."
    )
    parser.add_argument("--project-root", required=True, type=Path)
    parser.add_argument("--candidate-dir", required=True, type=Path)
    arguments = parser.parse_args(argv)
    try:
        root = arguments.project_root.resolve(strict=True)
        verifier_bytes = _read_verifier_source(arguments.project_root)
        result = invoke(verifier_bytes, root, arguments.candidate_dir)
    except (OSError, TaggedVerifierError) as exc:
        print("error: {}".format(exc), file=sys.stderr)
        return 1
    sys.stdout.buffer.write(result.stdout)
    sys.stdout.buffer.flush()
    if result.stderr:
        sys.stderr.buffer.write(result.stderr)
        sys.stderr.buffer.flush()
    return 0


if __name__ == "__main__":
    sys.exit(main())
