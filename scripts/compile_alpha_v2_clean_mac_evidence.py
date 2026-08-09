#!/usr/bin/env python3
"""Compile a source-free clean-Mac intake into Alpha-v2 release evidence.

The clean Mac collector deliberately records raw facts only.  This compiler
rechecks those facts against the immutable candidate and canonical v2 profile,
requires an independent review and real launch media, and publishes a new
evidence directory plus ``status.next.json`` without changing the source
status.  It never drives Safari, Finder, Gatekeeper, or the candidate app.
"""

import argparse
import copy
from datetime import datetime, timedelta, timezone
import hashlib
import ipaddress
import json
import os
from pathlib import Path, PurePosixPath
import plistlib
import re
import shlex
import stat
import sys
from typing import Any, Dict, List, Mapping, NamedTuple, Optional, Sequence, Tuple
from urllib.parse import unquote, urlsplit
import xml.etree.ElementTree as ET


SCRIPT_DIRECTORY = Path(__file__).resolve().parent
if str(SCRIPT_DIRECTORY) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIRECTORY))

import compile_alpha_v2_interactive_evidence as shared  # noqa: E402
import validate_media_recording as recording_validator  # noqa: E402


CompileError = shared.CompileError
PLAN_SCHEMA = "tanks3d-clean-mac-plan-v1"
INTAKE_SCHEMA = "tanks3d-clean-mac-intake-v1"
RECEIPT_SCHEMA = "tanks3d-clean-mac-compiler-receipt-v1"
RECEIPT_PRODUCER = "Tanks3D Alpha Clean-Mac Evidence Compiler"
BROWSER_ACQUISITION_SCHEMA = "tanks3d-browser-acquisition-v1"
COMMAND_LOG_SCHEMA = "tanks3d-command-log-v1"
INTERACTIVE_SESSION_SCHEMA = "tanks3d-interactive-session-v1"
REQUIREMENTS_PROFILE = "macos-alpha-v2"
EVIDENCE_ID = "gatekeeper_launch"
MINIMUM_RECORDING_BYTES = 64 * 1024
MAXIMUM_PLIST_BYTES = 4 * 1024 * 1024
MAXIMUM_WHERE_FROMS_BYTES = 1024 * 1024
MAXIMUM_MEDIA_BYTES = shared.MAXIMUM_RECORDING_BYTES
TIMESTAMP_SKEW = timedelta(seconds=5)
DNS_LABEL_RE = re.compile(
    r"^[A-Za-z0-9](?:[A-Za-z0-9-]{0,61}[A-Za-z0-9])?$"
)
LEGACY_NUMERIC_HOST_RE = re.compile(
    r"^(?:0[xX][0-9A-Fa-f]+|[0-9]+)"
    r"(?:\.(?:0[xX][0-9A-Fa-f]+|[0-9]+))*$"
)

PLAN_KEYS = {
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
}
INTAKE_KEYS = {
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
}
MACHINE_DETAIL_KEYS = {
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
}
ACQUISITION_KEYS = {
    "client",
    "started_at_utc",
    "completed_at_utc",
    "zip_quarantine_agent",
    "quarantine_timestamp_utc",
    "where_froms_url",
    "where_froms_sha256",
}
OBSERVATION_KEYS = {
    "first_finder_launch",
    "dialog_text",
    "documented_launch_path",
    "main_menu_reached",
    "signature_preserved",
    "conclusion",
}
COMMAND_KEYS = {
    "id",
    "argv",
    "exit_code",
    "stdout",
    "stderr",
    "started_at_utc",
    "completed_at_utc",
}
COMMAND_IDS = (
    "checksum",
    "zip_quarantine",
    "app_quarantine",
    "codesign",
    "spctl",
)
SYSTEM_COMMAND_PATHS = {
    "shasum": "/usr/bin/shasum",
    "xattr": "/usr/bin/xattr",
    "codesign": "/usr/bin/codesign",
    "spctl": "/usr/sbin/spctl",
}
RECEIPT_KEYS = {
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
}
RECEIPT_FILE_IDS = (
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
)
RAW_COMMAND_FILENAMES = tuple(
    "{}.{}".format(command_id.replace("_", "-"), stream)
    for command_id in COMMAND_IDS
    for stream in ("stdout", "stderr")
)
FILE_REFERENCE_KEYS = {"path", "sha256"}
SAFE_FILENAME_RE = re.compile(r"^[A-Za-z0-9][A-Za-z0-9._-]*$")
NONCE_RE = re.compile(r"^[0-9a-f]{32}$")
QUARANTINE_RE = re.compile(r"^[0-9A-Fa-f]{4};([0-9A-Fa-f]+);([^;\r\n]+);[^\r\n]*$")
CANONICAL_PLIST_DOCTYPE = (
    b'<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" '
    b'"http://www.apple.com/DTDs/PropertyList-1.0.dtd">'
)


class InputFile(NamedTuple):
    path: Path
    snapshot: shared.FileSnapshot
    data: bytes


def require_bool(value: Any, label: str) -> bool:
    if not isinstance(value, bool):
        raise CompileError("{} must be a boolean".format(label))
    return value


def require_integer(value: Any, label: str) -> int:
    if isinstance(value, bool) or not isinstance(value, int):
        raise CompileError("{} must be an integer".format(label))
    return value


def require_array(value: Any, label: str) -> List[Any]:
    if not isinstance(value, list):
        raise CompileError("{} must be an array".format(label))
    return list(value)


def affirmative(value: Any, label: str) -> str:
    text = shared.require_string(value, label).strip().lower()
    if text not in {"yes", "true", "pass", "verified"}:
        raise CompileError("{} must explicitly record yes/true/PASS/verified".format(label))
    return text


def parse_plist_node(node: ET.Element, label: str, depth: int = 0) -> Any:
    if depth > 16:
        raise CompileError("{} is nested too deeply".format(label))
    if node.attrib or (node.tail is not None and node.tail.strip()):
        raise CompileError("{} contains unsupported XML attributes or text".format(label))
    tag = node.tag
    if tag == "string":
        if list(node):
            raise CompileError("{} string contains child elements".format(label))
        return node.text or ""
    if tag == "integer":
        if list(node) or node.text is None or re.fullmatch(r"-?[0-9]+", node.text) is None:
            raise CompileError("{} contains an invalid integer".format(label))
        return int(node.text)
    if tag in {"true", "false"}:
        if list(node) or (node.text is not None and node.text.strip()):
            raise CompileError("{} contains an invalid boolean".format(label))
        return tag == "true"
    if tag == "array":
        if node.text is not None and node.text.strip():
            raise CompileError("{} array contains unexpected text".format(label))
        return [
            parse_plist_node(child, "{}[{}]".format(label, index), depth + 1)
            for index, child in enumerate(list(node))
        ]
    if tag == "dict":
        if node.text is not None and node.text.strip():
            raise CompileError("{} dictionary contains unexpected text".format(label))
        children = list(node)
        if len(children) % 2:
            raise CompileError("{} dictionary is missing a value".format(label))
        result: Dict[str, Any] = {}
        for index in range(0, len(children), 2):
            key_node = children[index]
            if key_node.tag != "key" or list(key_node) or key_node.attrib:
                raise CompileError("{} dictionary has an invalid key".format(label))
            key = key_node.text or ""
            if not key or key in result:
                raise CompileError("{} dictionary has a duplicate or empty key".format(label))
            result[key] = parse_plist_node(
                children[index + 1], "{}.{}".format(label, key), depth + 1
            )
        return result
    raise CompileError("{} contains unsupported plist element {!r}".format(label, tag))


def parse_strict_xml_plist(data: bytes, label: str) -> Mapping[str, Any]:
    if len(data) > MAXIMUM_PLIST_BYTES:
        raise CompileError("{} exceeds {} bytes".format(label, MAXIMUM_PLIST_BYTES))
    if b"<!ENTITY" in data.upper():
        raise CompileError("{} contains an XML entity declaration".format(label))
    doctypes = re.findall(br"<!DOCTYPE[^>]*>", data)
    if doctypes != [CANONICAL_PLIST_DOCTYPE]:
        raise CompileError("{} does not use the canonical Apple plist declaration".format(label))
    try:
        root = ET.fromstring(data)
    except ET.ParseError as exc:
        raise CompileError("{} is not valid XML: {}".format(label, exc))
    if root.tag != "plist" or root.attrib != {"version": "1.0"}:
        raise CompileError("{} does not use plist version 1.0".format(label))
    if root.text is not None and root.text.strip():
        raise CompileError("{} contains unexpected root text".format(label))
    children = list(root)
    if len(children) != 1:
        raise CompileError("{} must contain exactly one root value".format(label))
    value = parse_plist_node(children[0], label)
    return shared.require_object(value, label)


def read_input_file(
    directory: Path, name: str, label: str, maximum: int
) -> InputFile:
    path = directory / name
    snapshot, data = shared.snapshot_regular_file(
        path, label, collect=True, maximum_collected_bytes=maximum
    )
    if data is None:
        raise CompileError("internal read failure for {}".format(label))
    return InputFile(path, snapshot, data)


def validate_public_download_url(url: Any, filename: str) -> str:
    text = shared.require_string(url, "download_url", True)
    if text != text.strip():
        raise CompileError("download_url must not have surrounding whitespace")
    try:
        parsed = urlsplit(text)
        host = (parsed.hostname or "").lower()
        port = parsed.port
    except ValueError as exc:
        raise CompileError("download_url is not a valid URL: {}".format(exc))
    try:
        host.encode("ascii")
    except UnicodeEncodeError:
        raise CompileError("download_url host must be ASCII")
    labels = host.split(".")
    try:
        address = ipaddress.ip_address(host)
    except ValueError:
        address = None
    forbidden_exact = {
        "example",
        "localhost",
        "example.com",
        "example.org",
        "example.net",
    }
    forbidden_suffixes = (
        ".example",
        ".example.com",
        ".example.net",
        ".example.org",
        ".test",
        ".invalid",
        ".localhost",
        ".local",
        ".internal",
        ".lan",
        ".onion",
    )
    if (
        parsed.scheme != "https"
        or not host
        or host.endswith(".")
        or len(host) > 253
        or len(labels) < 2
        or any(DNS_LABEL_RE.fullmatch(label) is None for label in labels)
        or port not in (None, 443)
        or address is not None
        or LEGACY_NUMERIC_HOST_RE.fullmatch(host) is not None
        or host in forbidden_exact
        or host.endswith(forbidden_suffixes)
        or any(word in host for word in ("fixture", "placeholder"))
        or parsed.username is not None
        or parsed.password is not None
        or parsed.query
        or parsed.fragment
    ):
        raise CompileError("download_url must be a public, token-free HTTPS candidate URL")
    encoded_basename = PurePosixPath(parsed.path).name
    if re.search(r"%(?![0-9A-Fa-f]{2})", encoded_basename):
        raise CompileError("download_url has malformed percent encoding")
    try:
        decoded_basename = unquote(
            encoded_basename, encoding="utf-8", errors="strict"
        )
    except UnicodeError as exc:
        raise CompileError(
            "download_url filename is not valid UTF-8: {}".format(exc)
        )
    if decoded_basename != filename:
        raise CompileError("download_url basename does not match the candidate")
    return text


def version_tuple(value: Any, label: str) -> Tuple[int, ...]:
    text = shared.require_string(value, label, True)
    if re.fullmatch(r"[0-9]+(?:\.[0-9]+)*", text) is None:
        raise CompileError("{} must be a dotted numeric version".format(label))
    components = [int(component) for component in text.split(".")]
    while len(components) > 1 and components[-1] == 0:
        components.pop()
    return tuple(components)


def expected_argv(filename: str) -> Dict[str, List[str]]:
    return {
        "checksum": [SYSTEM_COMMAND_PATHS["shasum"], "-a", "256", filename],
        "zip_quarantine": [
            SYSTEM_COMMAND_PATHS["xattr"],
            "-p",
            "com.apple.quarantine",
            filename,
        ],
        "app_quarantine": [
            SYSTEM_COMMAND_PATHS["xattr"],
            "-p",
            "com.apple.quarantine",
            "Tanks3D.app",
        ],
        "codesign": [
            SYSTEM_COMMAND_PATHS["codesign"],
            "--verify",
            "--deep",
            "--strict",
            "--verbose=4",
            "Tanks3D.app",
        ],
        "spctl": [
            SYSTEM_COMMAND_PATHS["spctl"],
            "--assess",
            "--type",
            "execute",
            "--verbose=4",
            "Tanks3D.app",
        ],
    }


def validate_quarantine(value: Any, label: str) -> Tuple[str, int, str]:
    text = shared.require_string(value, label, True).strip()
    match = QUARANTINE_RE.fullmatch(text)
    if match is None:
        raise CompileError("{} is not a canonical quarantine record".format(label))
    try:
        seconds = int(match.group(1), 16)
    except ValueError:
        raise CompileError("{} has an invalid quarantine timestamp".format(label))
    return text, seconds, match.group(2)


def parse_where_froms(data: bytes, expected_url: str) -> None:
    try:
        text = data.decode("ascii")
        raw = bytes.fromhex("".join(text.split()))
        values = plistlib.loads(raw)
    except (UnicodeError, ValueError, plistlib.InvalidFileException) as exc:
        raise CompileError("where-froms.hex is not a valid encoded plist: {}".format(exc))
    if (
        not isinstance(values, list)
        or not values
        or any(not isinstance(item, str) for item in values)
        or expected_url not in values
    ):
        raise CompileError("where-froms metadata does not contain the exact download URL")


def validate_profile(profile: Mapping[str, Any]) -> str:
    expected = {
        "clean_mac_plan_schema": PLAN_SCHEMA,
        "clean_mac_intake_schema": INTAKE_SCHEMA,
        "clean_mac_compiler_receipt_schema": RECEIPT_SCHEMA,
        "clean_mac_compiler_receipt_producer": RECEIPT_PRODUCER,
    }
    for key, value in expected.items():
        if profile.get(key) != value:
            raise CompileError("requirements profile.{} does not match the compiler".format(key))
    profile_key_sets = {
        "clean_mac_plan_keys": PLAN_KEYS,
        "clean_mac_intake_keys": INTAKE_KEYS,
        "clean_mac_machine_detail_keys": MACHINE_DETAIL_KEYS,
        "clean_mac_acquisition_keys": ACQUISITION_KEYS,
        "clean_mac_observation_keys": OBSERVATION_KEYS,
    }
    for key, expected_keys in profile_key_sets.items():
        values = profile.get(key)
        if not isinstance(values, list) or set(values) != expected_keys:
            raise CompileError(
                "requirements profile.{} does not match the compiler".format(key)
            )
    collector = shared.require_string(
        profile.get("clean_mac_collector_sha256"),
        "requirements profile.clean_mac_collector_sha256",
        True,
    )
    if shared.SHA256_RE.fullmatch(collector) is None:
        raise CompileError("requirements profile collector SHA-256 is invalid")
    if profile.get("clean_mac_system_command_paths") != SYSTEM_COMMAND_PATHS:
        raise CompileError("requirements profile system command paths do not match")
    if set(profile.get("clean_mac_compiler_receipt_keys", [])) != RECEIPT_KEYS:
        raise CompileError("requirements profile receipt keys do not match")
    if profile.get("clean_mac_compiler_file_ids") != list(RECEIPT_FILE_IDS):
        raise CompileError("requirements profile receipt file IDs do not match")
    if set(profile.get("clean_mac_compiler_file_reference_keys", [])) != FILE_REFERENCE_KEYS:
        raise CompileError("requirements profile receipt file-reference keys do not match")
    if profile.get("clean_mac_media_kind") != "recording":
        raise CompileError("requirements profile clean-Mac media kind does not match")
    if profile.get("clean_mac_minimum_recording_bytes") != MINIMUM_RECORDING_BYTES:
        raise CompileError(
            "requirements profile clean-Mac recording minimum does not match"
        )
    if profile.get("clean_mac_maximum_recording_bytes") != MAXIMUM_MEDIA_BYTES:
        raise CompileError(
            "requirements profile clean-Mac recording maximum does not match"
        )
    if profile.get("clean_mac_recording_extensions") != [
        ".mov",
        ".mp4",
        ".m4v",
    ]:
        raise CompileError(
            "requirements profile clean-Mac recording extensions do not match"
        )
    return collector


def artifact_record(path: str, digest: str, kind: str) -> Dict[str, str]:
    return {"path": path, "sha256": digest, "kind": kind}


def file_reference(record: Mapping[str, str]) -> Dict[str, str]:
    return {"path": record["path"], "sha256": record["sha256"]}


def output_relative(root: Path, output: Path) -> Tuple[Path, str]:
    return shared.path_inside_root(root, output, "output directory")


def validate_source_status(
    root: Path, status: Mapping[str, Any]
) -> Tuple[
    Mapping[str, Any],
    str,
    str,
    str,
    Path,
    shared.FileSnapshot,
]:
    _, profile = shared.resolve_status_requirements(root, status)
    collector_sha256 = validate_profile(profile)
    release = shared.require_object(status.get("release"), "status.release")
    tag = shared.require_string(release.get("tag"), "status.release.tag", True)
    artifact = shared.require_object(release.get("artifact"), "status.release.artifact")
    artifact_path, candidate_path = shared.repository_path(
        root, artifact.get("path"), "status.release.artifact.path"
    )
    candidate_snapshot, _ = shared.snapshot_regular_file(
        candidate_path, "status.release.artifact.path"
    )
    candidate_sha256 = shared.require_string(
        artifact.get("sha256"), "status.release.artifact.sha256", True
    )
    if (
        shared.SHA256_RE.fullmatch(candidate_sha256) is None
        or candidate_snapshot.sha256 != candidate_sha256
    ):
        raise CompileError("status release artifact hash does not match the candidate")
    filename = candidate_path.name
    if artifact_path != artifact.get("path") or SAFE_FILENAME_RE.fullmatch(filename) is None:
        raise CompileError("status release artifact filename is not safe")
    for gate_name in ("clean_mac", "gatekeeper"):
        gate = shared.require_object(status.get(gate_name), "status.{}".format(gate_name))
        if gate.get("status") == "PASS":
            raise CompileError("status.{} is already PASS".format(gate_name))
        if gate.get("evidence_ids") or gate.get("checks_confirmed"):
            raise CompileError("status.{} already contains evidence claims".format(gate_name))
    evidence_values = status.get("evidence")
    if not isinstance(evidence_values, list):
        raise CompileError("status.evidence must be an array")
    evidence = None
    for raw in evidence_values:
        record = shared.require_object(raw, "status evidence")
        if record.get("id") == EVIDENCE_ID:
            evidence = record
            break
    if evidence is None:
        raise CompileError("status is missing gatekeeper_launch evidence")
    interactive = shared.require_object(evidence.get("interactive"), "gatekeeper interactive evidence")
    populated_interactive = any(
        value is not None and value != "" and value != {}
        for value in interactive.values()
    )
    if (
        evidence.get("status") == "PASS"
        or evidence.get("artifacts")
        or evidence.get("reviewer")
        or evidence.get("reviewed_at_utc") is not None
        or populated_interactive
    ):
        raise CompileError("gatekeeper_launch evidence is already populated")
    return (
        profile,
        tag,
        filename,
        candidate_sha256,
        candidate_path,
        candidate_snapshot,
    )


def copy_snapshot_at(
    directory_fd: int,
    name: str,
    source: Path,
    original: shared.FileSnapshot,
) -> None:
    source_fd = -1
    destination_fd = -1
    try:
        flags = os.O_RDONLY | getattr(os, "O_NONBLOCK", 0)
        if hasattr(os, "O_NOFOLLOW"):
            flags |= os.O_NOFOLLOW
        source_fd = os.open(str(source), flags)
        before = os.fstat(source_fd)
        if not stat.S_ISREG(before.st_mode):
            raise CompileError("media input must remain a regular file")
        output_flags = os.O_WRONLY | os.O_CREAT | os.O_EXCL
        if hasattr(os, "O_NOFOLLOW"):
            output_flags |= os.O_NOFOLLOW
        destination_fd = os.open(name, output_flags, 0o600, dir_fd=directory_fd)
        digest = hashlib.sha256()
        total = 0
        while True:
            chunk = os.read(source_fd, 1024 * 1024)
            if not chunk:
                break
            digest.update(chunk)
            total += len(chunk)
            view = memoryview(chunk)
            while view:
                written = os.write(destination_fd, view)
                if written <= 0:
                    raise CompileError("short write while copying launch media")
                view = view[written:]
        after = os.fstat(source_fd)
        current = shared.FileSnapshot(
            digest.hexdigest(),
            total,
            after.st_dev,
            after.st_ino,
            after.st_mtime_ns,
            after.st_ctime_ns,
        )
        if current != original:
            raise CompileError("launch media changed while it was copied")
        os.fsync(destination_fd)
    except Exception:
        if destination_fd >= 0:
            os.close(destination_fd)
            destination_fd = -1
        try:
            os.unlink(name, dir_fd=directory_fd)
        except OSError:
            pass
        raise
    finally:
        if destination_fd >= 0:
            os.close(destination_fd)
        if source_fd >= 0:
            os.close(source_fd)


def command_compile(args: argparse.Namespace) -> None:
    root = shared.require_real_directory(Path(args.project_root), "project root")
    status_path = Path(args.status)
    if not status_path.is_absolute():
        status_path = root / status_path
    shared.require_no_symlink_components(root, status_path, "source status")
    status_raw, status_value = shared.load_json(status_path, "source status")
    status_source = shared.require_object(status_value, "source status")
    (
        profile,
        tag,
        filename,
        candidate_sha256,
        candidate_path,
        candidate_snapshot,
    ) = validate_source_status(root, status_source)

    intake_dir = shared.require_real_directory(Path(args.intake_dir), "intake directory")
    try:
        entries = set(os.listdir(intake_dir))
    except OSError as exc:
        raise CompileError("cannot list intake directory: {}".format(exc))
    expected_entries = {
        "clean-mac-plan.plist",
        "clean-mac-intake.plist",
        "where-froms.hex",
        "COMPLETE",
        *RAW_COMMAND_FILENAMES,
    }
    if entries != expected_entries:
        raise CompileError(
            "intake directory does not contain the 14 canonical files"
        )
    plan_file = read_input_file(
        intake_dir, "clean-mac-plan.plist", "clean-Mac plan", MAXIMUM_PLIST_BYTES
    )
    intake_file = read_input_file(
        intake_dir, "clean-mac-intake.plist", "clean-Mac intake", MAXIMUM_PLIST_BYTES
    )
    where_file = read_input_file(
        intake_dir, "where-froms.hex", "where-froms metadata", MAXIMUM_WHERE_FROMS_BYTES
    )
    complete_file = read_input_file(
        intake_dir, "COMPLETE", "intake completion marker", 1
    )
    raw_command_files = {
        name: read_input_file(
            intake_dir,
            name,
            "raw command transcript {}".format(name),
            MAXIMUM_PLIST_BYTES,
        )
        for name in RAW_COMMAND_FILENAMES
    }
    if complete_file.data:
        raise CompileError("intake completion marker must be empty")

    plan = parse_strict_xml_plist(plan_file.data, "clean-Mac plan")
    shared.require_exact_keys(plan, PLAN_KEYS, "clean-Mac plan")
    intake = parse_strict_xml_plist(intake_file.data, "clean-Mac intake")
    shared.require_exact_keys(intake, INTAKE_KEYS, "clean-Mac intake")
    if plan["schema"] != PLAN_SCHEMA or plan["requirements_profile"] != REQUIREMENTS_PROFILE:
        raise CompileError("clean-Mac plan schema/profile is not canonical")
    if intake["schema"] != INTAKE_SCHEMA:
        raise CompileError("clean-Mac intake schema is not canonical")
    collector_sha256 = validate_profile(profile)
    for label, value in (
        ("plan collector", plan["collector_sha256"]),
        ("intake collector", intake["collector_sha256"]),
    ):
        if value != collector_sha256:
            raise CompileError("{} SHA-256 does not match the canonical collector".format(label))
    nonce = shared.require_string(plan["session_nonce"], "plan session_nonce", True)
    if NONCE_RE.fullmatch(nonce) is None or intake["session_nonce"] != nonce:
        raise CompileError("clean-Mac session nonce is invalid or inconsistent")
    if intake["plan_sha256"] != plan_file.snapshot.sha256:
        raise CompileError("intake plan SHA-256 does not match the copied plan")
    if (
        plan["candidate_tag"] != tag
        or plan["candidate_filename"] != filename
        or plan["candidate_sha256"] != candidate_sha256
    ):
        raise CompileError("clean-Mac plan is not bound to the status candidate")
    deployment_match = re.search(
        r"-macos([0-9]+(?:\.[0-9]+)*)\.zip$", filename
    )
    if (
        deployment_match is None
        or plan["minimum_macos_version"] != deployment_match.group(1)
    ):
        raise CompileError(
            "clean-Mac plan deployment target does not match the candidate filename"
        )
    for key in ("candidate_filename", "candidate_sha256", "download_url"):
        if intake[key] != plan[key]:
            raise CompileError("intake {} does not match the plan".format(key))
    url = validate_public_download_url(plan["download_url"], filename)
    prepared = shared.parse_timestamp(plan["prepared_at_utc"], "plan prepared_at_utc")

    if require_bool(intake["test_mode"], "intake test_mode"):
        raise CompileError("test-mode clean-Mac intake cannot become release evidence")
    if not require_bool(intake["complete"], "intake complete"):
        raise CompileError("clean-Mac intake is incomplete")
    tester = shared.require_nonplaceholder(intake["tester"], "intake tester")
    signature = shared.require_nonplaceholder(
        intake["tester_signature"], "intake tester_signature"
    )
    if tester != signature:
        raise CompileError("tester signature must exactly match the tester name")
    machine = shared.require_nonplaceholder(intake["machine"], "intake machine")
    started = shared.parse_timestamp(
        intake["session_started_at_utc"], "intake session_started_at_utc"
    )
    completed = shared.parse_timestamp(
        intake["session_completed_at_utc"], "intake session_completed_at_utc"
    )
    if not prepared <= started <= completed:
        raise CompileError("plan and intake session timestamps are not chronological")

    machine_details = shared.require_object(
        intake["machine_details"], "intake machine_details"
    )
    shared.require_exact_keys(machine_details, MACHINE_DETAIL_KEYS, "intake machine_details")
    for key in MACHINE_DETAIL_KEYS:
        shared.require_nonplaceholder(machine_details[key], "machine_details.{}".format(key))
    if machine_details["uname_machine"].strip() != "arm64":
        raise CompileError("clean-Mac intake must come from arm64")
    if "apple" not in machine_details["chip"].strip().lower():
        raise CompileError("clean-Mac intake chip must identify Apple Silicon")
    for key in (
        "prior_app_absent",
        "prior_approval_absent",
        "minimum_macos_met",
        "source_checkout_absent",
        "homebrew_raylib_unused",
    ):
        affirmative(machine_details[key], "machine_details.{}".format(key))
    if version_tuple(machine_details["macos_version"], "machine macOS version") < version_tuple(
        plan["minimum_macos_version"], "plan minimum macOS version"
    ):
        raise CompileError("clean Mac is older than the candidate deployment target")

    acquisition = shared.require_object(intake["acquisition"], "intake acquisition")
    shared.require_exact_keys(acquisition, ACQUISITION_KEYS, "intake acquisition")
    if acquisition["client"] != "Safari" or acquisition["zip_quarantine_agent"] != "Safari":
        raise CompileError("clean-Mac acquisition must be performed by Safari")
    acquisition_started = shared.parse_timestamp(
        acquisition["started_at_utc"], "acquisition started_at_utc"
    )
    acquisition_completed = shared.parse_timestamp(
        acquisition["completed_at_utc"], "acquisition completed_at_utc"
    )
    if not started <= acquisition_started <= acquisition_completed <= completed:
        raise CompileError("Safari acquisition lies outside the clean-Mac session")
    if acquisition["where_froms_url"] != url:
        raise CompileError("where-froms URL does not match the candidate URL")
    if acquisition["where_froms_sha256"] != where_file.snapshot.sha256:
        raise CompileError("where-froms SHA-256 does not match its raw file")
    parse_where_froms(where_file.data, url)

    commands = require_array(intake["commands"], "intake commands")
    if len(commands) != len(COMMAND_IDS):
        raise CompileError("intake must contain the five canonical commands")
    canonical_argv = expected_argv(filename)
    validated_commands: List[Dict[str, Any]] = []
    previous_completed: Optional[datetime] = None
    for index, (raw, command_id) in enumerate(zip(commands, COMMAND_IDS)):
        label = "intake commands[{}]".format(index)
        command = shared.require_object(raw, label)
        shared.require_exact_keys(command, COMMAND_KEYS, label)
        if command["id"] != command_id:
            raise CompileError("intake command order is not canonical")
        argv = shared.require_string_list(command["argv"], label + ".argv")
        if argv != canonical_argv[command_id]:
            raise CompileError("{} argv is not the absolute canonical command".format(command_id))
        exit_code = require_integer(command["exit_code"], label + ".exit_code")
        stdout = shared.require_string(command["stdout"], label + ".stdout")
        stderr = shared.require_string(command["stderr"], label + ".stderr")
        command_started = shared.parse_timestamp(
            command["started_at_utc"], label + ".started_at_utc"
        )
        command_completed = shared.parse_timestamp(
            command["completed_at_utc"], label + ".completed_at_utc"
        )
        if command_completed < command_started:
            raise CompileError("{} ends before it starts".format(label))
        if previous_completed is not None and command_started < previous_completed:
            raise CompileError("clean-Mac commands overlap or are out of order")
        if command_started < started or command_completed > completed:
            raise CompileError("{} lies outside the clean-Mac session".format(label))
        previous_completed = command_completed
        validated_commands.append(
            {
                "id": command_id,
                "argv": argv,
                "exit_code": exit_code,
                "stdout": stdout,
                "stderr": stderr,
                "started_at_utc": command["started_at_utc"],
                "completed_at_utc": command["completed_at_utc"],
            }
        )
        raw_prefix = command_id.replace("_", "-")
        for stream, expected_text in (("stdout", stdout), ("stderr", stderr)):
            raw_name = "{}.{}".format(raw_prefix, stream)
            try:
                raw_text = raw_command_files[raw_name].data.decode("utf-8")
            except UnicodeError as exc:
                raise CompileError(
                    "{} is not UTF-8: {}".format(raw_name, exc)
                )
            if raw_text != expected_text:
                raise CompileError(
                    "{} does not exactly match the intake command record".format(
                        raw_name
                    )
                )
    if acquisition_completed > shared.parse_timestamp(
        validated_commands[0]["started_at_utc"], "checksum started_at_utc"
    ):
        raise CompileError("Safari acquisition must finish before checksum")
    checksum = validated_commands[0]
    if (
        checksum["exit_code"] != 0
        or checksum["stdout"] != "{}  {}\n".format(candidate_sha256, filename)
        or checksum["stderr"] != ""
    ):
        raise CompileError("checksum command does not prove the candidate digest")
    zip_quarantine, quarantine_seconds, quarantine_agent = validate_quarantine(
        validated_commands[1]["stdout"], "ZIP quarantine output"
    )
    app_quarantine, _, app_agent = validate_quarantine(
        validated_commands[2]["stdout"], "app quarantine output"
    )
    if validated_commands[1]["exit_code"] != 0 or quarantine_agent != "Safari":
        raise CompileError("ZIP quarantine was not preserved from Safari")
    if validated_commands[2]["exit_code"] != 0 or app_agent not in {
        "Safari",
        "Archive Utility",
        "Finder",
    }:
        raise CompileError("app quarantine was not preserved by a supported Finder path")
    if validated_commands[3]["exit_code"] != 0:
        raise CompileError("codesign verification did not succeed")
    spctl = validated_commands[4]
    spctl_text = (spctl["stdout"] + "\n" + spctl["stderr"]).lower()
    has_accepted = "accepted" in spctl_text
    has_rejected = "rejected" in spctl_text
    if has_accepted == has_rejected:
        raise CompileError("spctl output must record exactly one assessment outcome")
    if (spctl["exit_code"] == 0) != has_accepted:
        raise CompileError("spctl exit code contradicts its assessment outcome")
    quarantine_time = datetime.fromtimestamp(quarantine_seconds, timezone.utc)
    if not (
        acquisition_started - TIMESTAMP_SKEW
        <= quarantine_time
        <= acquisition_completed + TIMESTAMP_SKEW
    ):
        raise CompileError("Safari quarantine timestamp lies outside acquisition")
    expected_quarantine_text = quarantine_time.strftime("%Y-%m-%dT%H:%M:%SZ")
    if acquisition["quarantine_timestamp_utc"] != expected_quarantine_text:
        raise CompileError("acquisition quarantine timestamp is not derived from xattr")

    observations = shared.require_object(intake["observations"], "intake observations")
    shared.require_exact_keys(observations, OBSERVATION_KEYS, "intake observations")
    for key in ("first_finder_launch", "dialog_text", "documented_launch_path"):
        shared.require_nonplaceholder(observations[key], "observations.{}".format(key))
        shared.reject_blocking_language(observations[key], "observations.{}".format(key))
    affirmative(observations["main_menu_reached"], "observations.main_menu_reached")
    affirmative(observations["signature_preserved"], "observations.signature_preserved")
    if shared.require_string(observations["conclusion"], "observations.conclusion").strip().upper() != "PASS":
        raise CompileError("clean-Mac human observation conclusion must be PASS")
    intake_notes = shared.require_nonplaceholder(intake["notes"], "intake notes")
    shared.reject_blocking_language(intake_notes, "intake notes")

    reviewer = shared.require_nonplaceholder(args.reviewer, "reviewer")
    reviewer_signature = shared.require_nonplaceholder(
        args.reviewer_signature, "reviewer signature"
    )
    if reviewer != reviewer_signature:
        raise CompileError("reviewer signature must exactly match reviewer name")
    if reviewer.casefold() == tester.casefold():
        raise CompileError("tester and reviewer must be different people")
    reviewed = shared.parse_timestamp(args.reviewed_at_utc, "reviewed_at_utc")
    if reviewed < completed:
        raise CompileError("review must finish after the clean-Mac session")
    review_notes = shared.require_nonplaceholder(args.review_notes, "review notes")
    shared.reject_blocking_language(review_notes, "review notes")
    affirmative(
        args.release_note_wording_verified,
        "release-note wording verification",
    )

    media_path = Path(args.media)
    media_snapshot, _ = shared.snapshot_regular_file(
        media_path, "Gatekeeper launch media"
    )
    if media_snapshot.size > MAXIMUM_MEDIA_BYTES:
        raise CompileError(
            "Gatekeeper launch media exceeds {} bytes".format(MAXIMUM_MEDIA_BYTES)
        )
    if media_snapshot.size < MINIMUM_RECORDING_BYTES:
        raise CompileError(
            "Gatekeeper launch recording must be at least {} bytes".format(
                MINIMUM_RECORDING_BYTES
            )
        )
    media_kind = "recording"
    try:
        suffix = recording_validator.validate_recording(
            media_path,
            MINIMUM_RECORDING_BYTES,
            MAXIMUM_MEDIA_BYTES,
            (
                media_snapshot.device,
                media_snapshot.inode,
                media_snapshot.size,
                media_snapshot.mtime_ns,
                media_snapshot.ctime_ns,
            ),
        )
    except recording_validator.RecordingValidationError as exc:
        raise CompileError(
            "Gatekeeper launch recording is not structurally valid: {}".format(exc)
        )
    media_name = "gatekeeper-launch-recording" + suffix

    output_dir, output_rel = output_relative(root, Path(args.output_dir))
    if os.path.lexists(str(output_dir)):
        raise CompileError("output directory already exists: {}".format(output_dir))
    shared.require_no_symlink_components(root, output_dir.parent, "output parent")
    shared.require_real_directory(output_dir.parent, "output parent")

    def path_for(name: str) -> str:
        return "{}/{}".format(output_rel, name)

    browser = {
        "schema": BROWSER_ACQUISITION_SCHEMA,
        "candidate_sha256": candidate_sha256,
        "tester": tester,
        "machine": machine,
        "client": "Safari",
        "url": url,
        "filename": filename,
        "started_at_utc": acquisition["started_at_utc"],
        "completed_at_utc": acquisition["completed_at_utc"],
        "zip_quarantine_agent": "Safari",
        "signature": signature,
    }
    command_log = {
        "schema": COMMAND_LOG_SCHEMA,
        "candidate_sha256": candidate_sha256,
        "machine": machine,
        "commands": validated_commands,
    }
    browser_data = shared.json_bytes(browser)
    command_data = shared.json_bytes(command_log)
    plan_record = artifact_record(
        path_for("clean-mac-plan.plist"), plan_file.snapshot.sha256, "report"
    )
    intake_record = artifact_record(
        path_for("clean-mac-intake.plist"), intake_file.snapshot.sha256, "report"
    )
    where_record = artifact_record(
        path_for("where-froms.hex"), where_file.snapshot.sha256, "log"
    )
    raw_command_records = {
        "{}_{}".format(command_id, stream): artifact_record(
            path_for("{}.{}".format(command_id.replace("_", "-"), stream)),
            raw_command_files[
                "{}.{}".format(command_id.replace("_", "-"), stream)
            ].snapshot.sha256,
            "log",
        )
        for command_id in COMMAND_IDS
        for stream in ("stdout", "stderr")
    }
    browser_record = artifact_record(
        path_for("browser-acquisition.json"), shared.sha256_bytes(browser_data), "report"
    )
    command_record = artifact_record(
        path_for("command-log.json"), shared.sha256_bytes(command_data), "log"
    )
    media_record = artifact_record(
        path_for(media_name), media_snapshot.sha256, media_kind
    )
    file_records = {
        "plan": plan_record,
        "intake": intake_record,
        "where_froms": where_record,
        **raw_command_records,
        "browser_acquisition": browser_record,
        "command_log": command_record,
        "media": media_record,
    }
    receipt = {
        "schema": RECEIPT_SCHEMA,
        "producer": RECEIPT_PRODUCER,
        "candidate_sha256": candidate_sha256,
        "session_nonce": nonce,
        "collector_sha256": collector_sha256,
        "tester": tester,
        "tester_signature": signature,
        "reviewer": reviewer,
        "reviewer_signature": reviewer_signature,
        "reviewed_at_utc": args.reviewed_at_utc,
        "review_notes": review_notes,
        "files": {
            file_id: file_reference(file_records[file_id])
            for file_id in RECEIPT_FILE_IDS
        },
    }
    receipt_data = shared.json_bytes(receipt)
    receipt_record = artifact_record(
        path_for("clean-mac-compiler-receipt.json"),
        shared.sha256_bytes(receipt_data),
        "report",
    )
    without_session = [
        plan_record,
        intake_record,
        where_record,
        *[
            raw_command_records[file_id]
            for file_id in RECEIPT_FILE_IDS
            if file_id in raw_command_records
        ],
        browser_record,
        command_record,
        receipt_record,
        media_record,
    ]
    coverage_refs = {
        "gate:clean_mac": "collector:clean-mac-intake",
        "gate:gatekeeper": "collector:gatekeeper-launch",
    }
    session = {
        "schema": INTERACTIVE_SESSION_SCHEMA,
        "candidate_sha256": candidate_sha256,
        "tester": tester,
        "machine": machine,
        "started_at_utc": intake["session_started_at_utc"],
        "completed_at_utc": intake["session_completed_at_utc"],
        "signature": signature,
        "categories": [
            {
                "id": EVIDENCE_ID,
                "tested_at_utc": intake["session_completed_at_utc"],
                "reviewed_at_utc": args.reviewed_at_utc,
                "result": "PASS",
                "coverage_refs": coverage_refs,
                "artifact_sha256s": [item["sha256"] for item in without_session],
            }
        ],
    }
    session_data = shared.json_bytes(session)
    session_record = artifact_record(
        path_for("gatekeeper-launch-session.json"),
        shared.sha256_bytes(session_data),
        "report",
    )

    next_status: Dict[str, Any] = copy.deepcopy(status_source)
    clean_details = {
        "mac_model": machine_details["mac_model"],
        "chip": machine_details["chip"],
        "uname_machine": machine_details["uname_machine"],
        "ram": machine_details["ram"],
        "macos_version": machine_details["macos_version"],
        "macos_build": machine_details["macos_build"],
        "clean_machine_method": machine_details["clean_machine_method"],
        "download_url": url,
        "download_client": "Safari",
        "downloaded_artifact_filename": filename,
        "downloaded_artifact_sha256": candidate_sha256,
        "checksum_command": shlex.join(canonical_argv["checksum"]),
        "checksum_exit_code": str(checksum["exit_code"]),
        "prior_app_absent": machine_details["prior_app_absent"],
        "prior_approval_absent": machine_details["prior_approval_absent"],
        "minimum_macos_met": machine_details["minimum_macos_met"],
        "source_checkout_absent": machine_details["source_checkout_absent"],
        "homebrew_raylib_unused": machine_details["homebrew_raylib_unused"],
    }
    gate_details = {
        "zip_quarantine_command": shlex.join(canonical_argv["zip_quarantine"]),
        "zip_quarantine_exit_code": str(validated_commands[1]["exit_code"]),
        "zip_quarantine_output": zip_quarantine,
        "app_quarantine_command": shlex.join(canonical_argv["app_quarantine"]),
        "app_quarantine_exit_code": str(validated_commands[2]["exit_code"]),
        "app_quarantine_output": app_quarantine,
        "codesign_command": shlex.join(canonical_argv["codesign"]),
        "codesign_exit_code": str(validated_commands[3]["exit_code"]),
        "spctl_command": shlex.join(canonical_argv["spctl"]),
        "spctl_exit_code": str(spctl["exit_code"]),
        "first_finder_launch": observations["first_finder_launch"],
        "dialog_text": observations["dialog_text"],
        "documented_launch_path": observations["documented_launch_path"],
        "main_menu_reached": observations["main_menu_reached"],
        "signature_preserved": observations["signature_preserved"],
        "conclusion": "PASS",
        "release_note_wording_verified": args.release_note_wording_verified,
    }
    for gate_name, details in (("clean_mac", clean_details), ("gatekeeper", gate_details)):
        gate = next_status[gate_name]
        gate["status"] = "PASS"
        gate["tester"] = tester
        gate["tested_at_utc"] = intake["session_completed_at_utc"]
        gate["evidence_ids"] = [EVIDENCE_ID]
        gate["checks_confirmed"] = list(details)
        gate["details"] = details
        gate["notes"] = review_notes
    evidence = next(
        item for item in next_status["evidence"] if item["id"] == EVIDENCE_ID
    )
    evidence["status"] = "PASS"
    evidence["artifacts"] = without_session + [session_record]
    evidence["reviewer"] = reviewer
    evidence["reviewed_at_utc"] = args.reviewed_at_utc
    evidence["interactive"] = {
        "candidate_sha256": candidate_sha256,
        "tester": tester,
        "machine": machine,
        "tested_at_utc": intake["session_completed_at_utc"],
        "signature": signature,
        "coverage_refs": coverage_refs,
    }
    evidence["notes"] = review_notes
    status_data = shared.json_bytes(next_status)

    small_outputs = {
        "clean-mac-plan.plist": plan_file.data,
        "clean-mac-intake.plist": intake_file.data,
        "where-froms.hex": where_file.data,
        **{
            name: raw_file.data
            for name, raw_file in raw_command_files.items()
        },
        "browser-acquisition.json": browser_data,
        "command-log.json": command_data,
        "clean-mac-compiler-receipt.json": receipt_data,
        "gatekeeper-launch-session.json": session_data,
        "status.next.json": status_data,
    }
    if shared.read_regular_file(status_path, "source status") != status_raw:
        raise CompileError("source status changed during compilation")
    for current_file in (
        plan_file,
        intake_file,
        where_file,
        complete_file,
        *raw_command_files.values(),
    ):
        current, _ = shared.snapshot_regular_file(current_file.path, "intake input")
        if current != current_file.snapshot:
            raise CompileError("clean-Mac intake changed during compilation")
    current_media, _ = shared.snapshot_regular_file(media_path, "Gatekeeper launch media")
    if current_media != media_snapshot:
        raise CompileError("Gatekeeper launch media changed during compilation")
    current_candidate, _ = shared.snapshot_regular_file(
        candidate_path, "status.release.artifact.path"
    )
    if current_candidate != candidate_snapshot:
        raise CompileError("release candidate changed during compilation")
    try:
        current_entries = set(os.listdir(intake_dir))
    except OSError as exc:
        raise CompileError("cannot relist intake directory: {}".format(exc))
    if current_entries != expected_entries:
        raise CompileError("intake directory changed during compilation")

    parent_fd: Optional[int] = None
    directory_fd: Optional[int] = None
    created_identity: Optional[Tuple[int, int]] = None
    created_names: List[str] = []
    try:
        directory_flags = os.O_RDONLY | getattr(os, "O_DIRECTORY", 0)
        if hasattr(os, "O_NOFOLLOW"):
            directory_flags |= os.O_NOFOLLOW
        parent_fd = os.open(str(output_dir.parent), directory_flags)
        os.mkdir(output_dir.name, 0o700, dir_fd=parent_fd)
        created = os.stat(output_dir.name, dir_fd=parent_fd, follow_symlinks=False)
        if not stat.S_ISDIR(created.st_mode):
            raise CompileError("created output path is not a directory")
        created_identity = (created.st_dev, created.st_ino)
        directory_fd = os.open(output_dir.name, directory_flags, dir_fd=parent_fd)
        opened = os.fstat(directory_fd)
        if (opened.st_dev, opened.st_ino) != created_identity:
            raise CompileError("output directory was replaced while opening it")
        for name, data in small_outputs.items():
            shared.exclusive_write_at(directory_fd, name, data)
            created_names.append(name)
        copy_snapshot_at(
            directory_fd, media_name, media_path, media_snapshot
        )
        created_names.append(media_name)
        os.fsync(directory_fd)
        print("Compiled clean-Mac evidence pack: {}".format(output_dir))
        print("Next status (source unchanged): {}".format(output_dir / "status.next.json"))
    except Exception:
        if directory_fd is not None:
            for name in reversed(created_names):
                try:
                    os.unlink(name, dir_fd=directory_fd)
                except OSError:
                    pass
        if parent_fd is not None and created_identity is not None:
            try:
                current = os.stat(
                    output_dir.name, dir_fd=parent_fd, follow_symlinks=False
                )
                if stat.S_ISDIR(current.st_mode) and (
                    current.st_dev,
                    current.st_ino,
                ) == created_identity:
                    os.rmdir(output_dir.name, dir_fd=parent_fd)
            except OSError:
                pass
        raise
    finally:
        if directory_fd is not None:
            os.close(directory_fd)
        if parent_fd is not None:
            os.close(parent_fd)


def parser() -> argparse.ArgumentParser:
    result = argparse.ArgumentParser(description=__doc__)
    result.add_argument("--project-root", required=True)
    result.add_argument("--status", required=True)
    result.add_argument("--intake-dir", required=True)
    result.add_argument("--media", required=True)
    result.add_argument("--reviewer", required=True)
    result.add_argument("--reviewer-signature", required=True)
    result.add_argument("--reviewed-at-utc", required=True)
    result.add_argument("--review-notes", required=True)
    result.add_argument("--release-note-wording-verified", required=True)
    result.add_argument("--output-dir", required=True)
    return result


def main(argv: Optional[Sequence[str]] = None) -> int:
    args = parser().parse_args(argv)
    try:
        command_compile(args)
    except (CompileError, OSError, OverflowError) as exc:
        print("error: {}".format(exc), file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
