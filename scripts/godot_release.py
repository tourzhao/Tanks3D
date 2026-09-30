#!/usr/bin/env python3
"""Immutable Godot Alpha candidates. Building a candidate never publishes it."""
import argparse
import contextlib
import datetime
import fcntl
import hashlib
import json
import math
import os
from pathlib import Path, PurePosixPath
import plistlib
import re
import shutil
import signal
import stat
import subprocess
import sys
import tempfile
import time
import zipfile

import package_godot_app as package
from validate_media_recording import validate_recording, RecordingValidationError

ROOT = Path(__file__).resolve().parents[1]
SCHEMA = "tanks3d-godot-candidate-v1"
GATES = {
    "clean": ["make", "clean"],
    "tooling": ["make", "test-architecture", "test-godot-release"],
    "debug": ["make", "-j4", "debug"],
    "shared": ["make", "-j4", "test", "test-sanitize", "test-game-session-adapter", "test-game-session-adapter-sanitize"],
    "native": ["make", "-j4", "test-godot-core", "test-godot-core-sanitize", "test-godot-lan-sockets"],
    "frontend": ["make", "test-godot-import", "test-godot-bundle", "test-godot-lan"],
    "coverage": ["make", "-j4", "coverage"],
}


def read_json(path):
    if path.is_symlink() or not path.is_file() or path.stat().st_size > 16 * 1024 * 1024:
        raise RuntimeError(f"Invalid or oversized JSON file: {path}")
    def unique(items):
        result = {}
        for key, value in items:
            if key in result:
                raise RuntimeError(f"Duplicate JSON key: {key}")
            result[key] = value
        return result
    return json.loads(path.read_text(), object_pairs_hook=unique)


def write_json(path, value):
    path.write_text(json.dumps(value, indent=2, sort_keys=True) + "\n")


def digest(path):
    return package.gate.digest(path)


def json_digest(value):
    return hashlib.sha256(json.dumps(value, sort_keys=True, separators=(",", ":")).encode()).hexdigest()


def git(*args, root=ROOT):
    return subprocess.check_output(["git", "-C", str(root), *args], text=True, timeout=60).strip()


def source_files():
    names = git("ls-files", "-z").removesuffix("\0").split("\0")
    result = {}
    for name in names:
        path = ROOT / name
        if not name or path.is_symlink() or not path.is_file():
            raise RuntimeError(f"Source must contain regular tracked files: {name}")
        result[name] = digest(path)
    return result


def source_identity(version, channel):
    if not re.fullmatch(r"alpha\.[1-9][0-9]*", channel):
        raise RuntimeError("Godot channel must be alpha.N")
    identity = package.validate_identity({"version": version, "build": git("rev-list", "--count", "HEAD"),
        "commit": git("rev-parse", "HEAD"), "tag": f"v{version}-godot.{channel}"})
    if git("status", "--porcelain", "--untracked-files=all"):
        raise RuntimeError("Candidate requires a clean worktree, including all new Godot files")
    if git("rev-parse", "--verify", f"refs/tags/{identity['tag']}^{{commit}}") != identity["commit"]:
        raise RuntimeError("The exact immutable Godot release tag must point at HEAD")
    return identity


def checked_file(root, name):
    if not isinstance(name, str):
        raise RuntimeError("Candidate path must be text")
    path = PurePosixPath(name)
    if (not isinstance(name, str) or not name or path.is_absolute() or ".." in path.parts
            or "\\" in name or path.as_posix() != name or any(ord(c) < 32 for c in name)):
        raise RuntimeError(f"Unsafe candidate path: {name}")
    result = root / name
    if any(p.is_symlink() for p in [result, *result.parents] if p.is_relative_to(root)):
        raise RuntimeError(f"Candidate contains a symlink: {name}")
    if not result.is_file():
        raise RuntimeError(f"Missing candidate file: {name}")
    return result


def file_hashes(root):
    return {p.relative_to(root).as_posix(): digest(checked_file(root, p.relative_to(root).as_posix()))
            for p in sorted(root.rglob("*")) if p.is_file() or p.is_symlink()}


def run_gate(command, output, timeout=1800):
    """Bound each build and kill its entire subprocess group on failure/timeout."""
    with output.open("w") as log:
        process = subprocess.Popen(command, cwd=ROOT, stdout=log, stderr=subprocess.STDOUT, start_new_session=True)
        begin = time.monotonic()
        try:
            while process.poll() is None:
                if time.monotonic() - begin > timeout or output.stat().st_size > 64 * 1024 * 1024:
                    raise RuntimeError(f"Gate exceeded its time/output budget: {command}")
                time.sleep(.2)
            if process.returncode:
                raise RuntimeError(f"Gate failed ({process.returncode}): {command}; see {output}")
        finally:
            if process.poll() is None:
                os.killpg(process.pid, signal.SIGTERM)
                try:
                    process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    os.killpg(process.pid, signal.SIGKILL)
                    process.wait(timeout=5)
    return {"argv": command, "exit_code": 0, "log": "gates/" + output.name}


def archive_app(app, archive):
    with zipfile.ZipFile(archive, "w", compression=zipfile.ZIP_DEFLATED) as out:
        for name in file_hashes(app):
            out.write(app / name, app.name + "/" + name)


@contextlib.contextmanager
def extracted_app(archive):
    """Reject traversal, links, duplicates and unreasonable extraction budgets."""
    with tempfile.TemporaryDirectory(prefix="godot-extract-", dir=ROOT / "build") as temp:
        root = Path(temp)
        with zipfile.ZipFile(archive) as source:
            members = source.infolist()
            names = set()
            if len(members) > 4096 or sum(m.file_size for m in members) > 1024 * 1024 * 1024:
                raise RuntimeError("Candidate archive exceeds extraction limits")
            for member in members:
                path = PurePosixPath(member.filename)
                mode = member.external_attr >> 16
                if (member.filename in names or path.is_absolute() or ".." in path.parts
                        or "\\" in member.filename or path.as_posix() != member.filename
                        or not path.parts or path.parts[0] != "Tanks3D-Godot.app"
                        or len(path.parts) < 3 or any(ord(c) < 32 for c in member.filename)
                        or stat.S_IFMT(mode) not in (0, stat.S_IFREG) or member.is_dir()
                        or member.file_size > 512 * 1024 * 1024):
                    raise RuntimeError("Unsafe or unexpected candidate archive entry")
                names.add(member.filename)
            for member in members:
                target = root / member.filename
                target.parent.mkdir(parents=True, exist_ok=True)
                with source.open(member) as data, target.open("wb") as output:
                    shutil.copyfileobj(data, output)
                target.chmod(0o755 if member.external_attr >> 16 & 0o111 else 0o644)
        yield root / "Tanks3D-Godot.app"


def requirements():
    """Retain the established QA scope, with the actual Godot control bindings."""
    old = read_json(ROOT / "docs/release-requirements/macos-alpha-v2.json")
    groups = {mode: old["gameplay_ids"] for mode in ["one_player", "two_player", "ai_as_p2"]}
    groups.update({"base_" + nation: old["base_checks"] for nation in old["base_ids"]})
    groups.update({"pickup_" + item["id"]: item["checks"] for item in old["pickup_requirements"]})
    groups["settlement"] = old["settlement_ids"]
    groups["advanced_settings"] = old["advanced_settings_checks"]
    groups["controls"] = [item for item in old["published_control_checks"]
                          if item not in ["f8_quality_toggle", "n_b_stage_navigation"]] + ["tab_pixel_toggle"]
    groups.update(read_json(ROOT / "docs/release-requirements/godot-alpha-v1.json")["groups"])
    return groups


def init_status(candidate, attestation, output):
    if output.exists():
        raise RuntimeError("Refusing to overwrite an existing QA status")
    write_json(output, {"schema": "tanks3d-godot-release-status-v1",
        "candidate_sha256": attestation["archive_sha256"], "identity": attestation["identity"],
        "requirements_sha256": json_digest(requirements()), "known_issues_reviewed": False,
        "known_issues": [], "observations": {name: {"checks": {c: "NOT_RUN" for c in checks},
            "tester": "", "machine": "", "completed_at_utc": "", "notes": "", "evidence": []}
            for name, checks in requirements().items()}, "approvals": {}})


def verify_candidate(directory, smoke=True):
    if directory.is_symlink():
        raise RuntimeError("Candidate directory must not be a symlink")
    directory = directory.resolve()
    attestation = read_json(checked_file(directory, "attestation.json"))
    if attestation.get("schema") != SCHEMA:
        raise RuntimeError("Unsupported Godot candidate attestation")
    identity = package.validate_identity(attestation["identity"])
    expected = source_identity(identity["version"], identity["tag"].split("-godot.", 1)[1])
    if identity != expected or attestation["requirements_sha256"] != json_digest(requirements()):
        raise RuntimeError("Candidate identity or QA requirements differ from the tagged source")
    initial = file_hashes(directory)
    recorded = attestation["files_sha256"]
    if {k: v for k, v in initial.items() if k != "attestation.json"} != recorded:
        raise RuntimeError("Candidate files differ from the attested hashes")
    if read_json(checked_file(directory, "source-manifest.json")) != source_files():
        raise RuntimeError("Candidate source snapshot differs from the clean tagged source")
    if set(attestation["gates"]) != set(GATES):
        raise RuntimeError("Missing or unexpected required Godot candidate gates")
    for name, command in GATES.items():
        result = attestation["gates"][name]
        if result != {"argv": command, "exit_code": 0, "log": f"gates/{name}.log"}:
            raise RuntimeError(f"Invalid gate result: {name}")
        if not checked_file(directory, result["log"]).stat().st_size:
            raise RuntimeError(f"Missing gate output: {name}")
    archive = checked_file(directory, attestation["archive"])
    if digest(archive) != attestation["archive_sha256"]:
        raise RuntimeError("Candidate ZIP checksum mismatch")
    if checked_file(directory, archive.name + ".sha256").read_text() != f"{digest(archive)}  {archive.name}\n":
        raise RuntimeError("Candidate checksum sidecar mismatch")
    with extracted_app(archive) as app:
        if file_hashes(app) != attestation["app_sha256"]:
            raise RuntimeError("Extracted candidate app differs from its sealed build")
        package.verify(app, identity)
        if smoke:
            package.smoke(app)
    if file_hashes(directory) != initial or source_identity(identity["version"], identity["tag"].split("-godot.")[1]) != identity:
        raise RuntimeError("Candidate or source identity changed during verification")
    return attestation


def build_candidate(version, channel):
    (ROOT / "build").mkdir(exist_ok=True)
    with (ROOT / "build/.godot-candidate.lock").open("w") as lock:
        try:
            fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError as error:
            raise RuntimeError("Another Godot candidate build is active") from error
        identity = source_identity(version, channel)
        sources = source_files()
        parent = ROOT / "build/release/godot"
        parent.mkdir(parents=True, exist_ok=True)
        destination = parent / identity["tag"]
        if destination.exists():
            raise RuntimeError("Candidate already exists; never overwrite an attested tag")
        # Keep gate output under preserved evidence: make clean must not remove it.
        evidence = ROOT / "build/release-evidence"
        evidence.mkdir(exist_ok=True)
        work = Path(tempfile.mkdtemp(prefix="godot-candidate-", dir=evidence))
        pending = work / "candidate"
        (pending / "gates").mkdir(parents=True)
        results = {}
        for name, command in GATES.items():
            print("Godot candidate gate: " + name, flush=True)
            results[name] = run_gate(command, pending / "gates" / (name + ".log"))
        if source_identity(version, channel) != identity or source_files() != sources:
            raise RuntimeError("Source changed during candidate gates")
        for path in package.gate.staging.source_files().values():
            if not path.is_relative_to(ROOT / "build") and path.relative_to(ROOT).as_posix() not in sources:
                raise RuntimeError("An untracked/ignored resource would enter the candidate")
        package.WORK = work / "package"
        package.APP = work / "Tanks3D-Godot.app"
        package.build(ROOT / "build/godot-tools/Godot.app/Contents/MacOS/Godot", ROOT / "build/godot/project", identity)
        manifest = package.verify(package.APP, identity)
        archive = pending / f"Tanks3D-Godot-{version}-{channel}-macos-arm64-macos{manifest['minimum_macos']}.zip"
        archive_app(package.APP, archive)
        (pending / (archive.name + ".sha256")).write_text(f"{digest(archive)}  {archive.name}\n")
        write_json(pending / "source-manifest.json", sources)
        shutil.copy2(package.WORK / "app-receipt.json", pending / "package-receipt.json")
        attestation = {"schema": SCHEMA, "identity": identity, "archive": archive.name,
            "archive_sha256": digest(archive), "requirements_sha256": json_digest(requirements()),
            "gates": results, "app_sha256": file_hashes(package.APP), "files_sha256": file_hashes(pending)}
        write_json(pending / "attestation.json", attestation)
        verify_candidate(pending)
        pending.rename(destination)
        init_status(destination, attestation, work / "qa-status.json")
        print(f"Verified Godot candidate: {destination}\nQA status (not approved): {work / 'qa-status.json'}")
        return destination


def verify_tagged(candidate, status=None):
    """Run the verifier stored in the immutable source tag, never move the tag."""
    data = read_json(candidate / "attestation.json")
    identity = package.validate_identity(data["identity"])
    if git("status", "--porcelain", "--untracked-files=all"):
        raise RuntimeError("Tagged verification requires a clean caller worktree")
    if git("rev-parse", "--verify", f"refs/tags/{identity['tag']}^{{commit}}") != identity["commit"]:
        raise RuntimeError("Candidate source tag was moved or is missing")
    hashes = file_hashes(candidate)
    status_hash = digest(status) if status else None
    with tempfile.TemporaryDirectory(prefix="godot-tagged-") as temporary:
        clone = Path(temporary) / "repo"
        subprocess.run(["git", "clone", "--local", "--no-hardlinks", "--quiet", str(ROOT), str(clone)], check=True, timeout=120)
        subprocess.run(["git", "-C", str(clone), "checkout", "--quiet", "--detach", identity["commit"]], check=True, timeout=60)
        (clone / "build").mkdir(exist_ok=True)
        copied = clone / "build/candidate"
        shutil.copytree(candidate, copied, symlinks=True)
        command = [sys.executable, "-B", str(clone / "scripts/godot_release.py"), "verify", "--candidate", str(copied)]
        if status:
            command += ["--status", str(status.absolute())]
        run_gate(command,
                 ROOT / "build/godot-tagged-verification.log", timeout=600)
    if hashes != file_hashes(candidate) or git("rev-parse", f"refs/tags/{identity['tag']}^{{commit}}") != identity["commit"]:
        raise RuntimeError("Candidate/tag changed during tagged verification")
    if status and digest(status) != status_hash:
        raise RuntimeError("QA status changed during tagged verification")


def nonempty(value):
    return isinstance(value, str) and len(value.strip()) >= 3 and value.upper() not in {"TODO", "TBD", "N/A", "UNKNOWN"}


def timestamp(value):
    try:
        parsed = datetime.datetime.fromisoformat(value.replace("Z", "+00:00"))
        if parsed.utcoffset() != datetime.timedelta(0) or parsed > datetime.datetime.now(datetime.timezone.utc):
            raise ValueError()
        return parsed
    except (ValueError, AttributeError, TypeError) as error:
        raise RuntimeError("QA requires a real, non-future UTC timestamp") from error


def verify_extended_metrics(item, status_path, attestation):
    """Require reviewable measurements as well as the long-session checkboxes."""
    name = item.get("metrics_report")
    references = {entry["path"] for entry in item["evidence"]}
    if name not in references:
        raise RuntimeError("Extended session requires a hashed metrics_report in its evidence")
    report = read_json(checked_file(status_path.parent, name))
    if (report.get("schema") != "tanks3d-godot-session-metrics-v1" or
            report.get("candidate_sha256") != attestation["archive_sha256"]):
        raise RuntimeError("Extended-session metrics are not bound to this candidate")
    thresholds = read_json(ROOT / "docs/release-requirements/macos-alpha-v2.json")["performance_thresholds"]
    limits = {"duration_seconds": (thresholds["minimum_duration_minutes"] * 60, math.inf),
        "stages_completed": (thresholds["minimum_stages_completed"], math.inf),
        "average_fps": (thresholds["minimum_average_fps"], math.inf),
        "one_percent_low_fps": (thresholds["minimum_one_percent_low_fps"], math.inf),
        "memory_growth_bytes": (-math.inf, thresholds["maximum_memory_growth_bytes"]),
        "gameplay_duration_ratio": (thresholds["minimum_gameplay_duration_ratio"], 1.0),
        "focused_duration_ratio": (thresholds["minimum_focused_duration_ratio"], 1.0)}
    for key, (minimum, maximum) in limits.items():
        value = report.get(key)
        if type(value) not in (int, float) or not math.isfinite(value) or not minimum <= value <= maximum:
            raise RuntimeError(f"Extended-session metric fails the acceptance threshold: {key}")
    if (report.get("worst_thermal_state") not in ["nominal", "fair"] or
            not all(nonempty(report.get(key)) for key in ["measurement_tool", "clock", "memory_metric", "method"])):
        raise RuntimeError("Extended session needs acceptable thermal data and a documented measurement method")
    raw_logs = report.get("raw_logs", [])
    if not raw_logs or any(path == name or path not in references or
                           Path(path).suffix.lower() not in [".json", ".jsonl", ".csv", ".log"] for path in raw_logs):
        raise RuntimeError("Extended-session measurements need their hashed raw logs")


def verify_status(status_path, attestation):
    status = read_json(status_path)
    groups = requirements()
    if (status.get("schema") != "tanks3d-godot-release-status-v1" or
            status.get("candidate_sha256") != attestation["archive_sha256"] or
            status.get("identity") != attestation["identity"] or
            status.get("requirements_sha256") != json_digest(groups)):
        raise RuntimeError("QA status is not bound to this exact Godot candidate/contract")
    observations = status.get("observations", {})
    if set(observations) != set(groups):
        raise RuntimeError("Missing or unexpected Godot QA groups")
    last_observation = None
    for name, checks in groups.items():
        item = observations[name]
        if item.get("checks") != dict.fromkeys(checks, "PASS"):
            raise RuntimeError(f"Godot release BLOCKED: {name} has unfinished or failed checks")
        if not all(nonempty(item.get(k)) for k in ["tester", "machine", "notes"]):
            raise RuntimeError(f"Missing human observation details: {name}")
        observed = timestamp(item.get("completed_at_utc"))
        last_observation = max(last_observation, observed) if last_observation else observed
        evidence = item.get("evidence", [])
        if not evidence:
            raise RuntimeError(f"Missing candidate-bound recording/report evidence: {name}")
        recording = False
        for reference in evidence:
            path = checked_file(status_path.parent, reference["path"])
            if path.stat().st_size > 95 * 1024 * 1024 or digest(path) != reference["sha256"]:
                raise RuntimeError("QA evidence size or checksum mismatch")
            if path.suffix.lower() in {".mp4", ".mov", ".m4v"}:
                if path.stat().st_size < 65536:
                    raise RuntimeError("QA recording is too small")
                validate_recording(path, 65536, 95 * 1024 * 1024)
                recording = True
        if not recording:
            raise RuntimeError(f"A real recording is required for {name}")
        if name == "extended_session":
            verify_extended_metrics(item, status_path, attestation)
    if status.get("known_issues_reviewed") is not True or not isinstance(status.get("known_issues"), list):
        raise RuntimeError("Known issues have not been reviewed")
    for issue in status["known_issues"]:
        if (issue.get("severity") not in ["P2", "P3"] or issue.get("decision") != "accepted"
                or not nonempty(issue.get("owner")) or not nonempty(issue.get("reason"))):
            raise RuntimeError("An unresolved/blocking known issue remains")
    content_hash = json_digest({k: v for k, v in status.items() if k != "approvals"})
    if set(status.get("approvals", {})) != {"qa_lead", "release_owner"}:
        raise RuntimeError("Both QA lead and release-owner approval are required")
    for approval in status["approvals"].values():
        if (approval.get("decision") != "approved" or approval.get("report_sha256") != content_hash
                or not nonempty(approval.get("name")) or timestamp(approval.get("at_utc")) < last_observation):
            raise RuntimeError("Approval is missing, stale, or predates its evidence")
    return status


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=["build", "verify", "verify-tagged", "init-status", "verify-ready"])
    parser.add_argument("--version", default=plistlib.loads((ROOT / "macos/Info.plist").read_bytes())["CFBundleShortVersionString"])
    parser.add_argument("--channel", default="alpha.1")
    parser.add_argument("--candidate", type=Path)
    parser.add_argument("--status", type=Path)
    args = parser.parse_args()
    (ROOT / "build").mkdir(exist_ok=True)
    if args.action == "build":
        build_candidate(args.version, args.channel)
        return
    if args.candidate is None:
        parser.error("--candidate is required")
    if args.action in ["init-status", "verify-ready"] and args.status is None:
        parser.error("--status is required")
    if args.candidate.is_symlink() or (args.status and args.status.is_symlink()):
        parser.error("Candidate/status must not be symlinks")
    candidate = args.candidate.absolute()
    if args.action in ["verify-tagged", "verify-ready"]:
        verify_tagged(candidate, args.status if args.action == "verify-ready" else None)
    else:
        attestation = verify_candidate(candidate)
        if args.action == "init-status":
            init_status(candidate, attestation, args.status.absolute())
        elif args.status:
            verify_status(args.status.absolute(), attestation)
    print("Godot release check passed: " + args.action)


if __name__ == "__main__":
    try:
        main()
    except (OSError, RuntimeError, ValueError, KeyError, TypeError, zipfile.BadZipFile,
            RecordingValidationError, subprocess.SubprocessError) as error:
        raise SystemExit(str(error)) from error
