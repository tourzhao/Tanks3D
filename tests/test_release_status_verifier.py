#!/usr/bin/env python3
"""Contract tests for the structured macOS Alpha release-status gate."""

import hashlib
import json
import os
from pathlib import Path
import plistlib
import re
import shutil
import struct
import subprocess
import sys
import tempfile
import unittest
from unittest import mock
import zipfile
import zlib

from media_recording_fixture import recording as make_recording


REPOSITORY_ROOT = Path(__file__).resolve().parents[1]
VERIFIER = REPOSITORY_ROOT / "scripts" / "verify_release_status.py"
CLEAN_MAC_COMPILER = (
    REPOSITORY_ROOT / "scripts" / "compile_alpha_v2_clean_mac_evidence.py"
)
EVIDENCE_COMPILER = (
    REPOSITORY_ROOT / "scripts" / "compile_alpha_v2_interactive_evidence.py"
)
REQUIREMENTS_V1 = (
    REPOSITORY_ROOT / "docs" / "release-requirements" / "macos-alpha-v1.json"
)
REQUIREMENTS_V2 = (
    REPOSITORY_ROOT / "docs" / "release-requirements" / "macos-alpha-v2.json"
)
STATUS_TEMPLATE = REPOSITORY_ROOT / "docs" / "releases" / "v0.1.0-alpha.3-status.json"
SESSION_START_UTC = "2020-01-01T16:30:00Z"
UTC = "2020-01-01T17:00:00Z"
REVIEW_UTC = "2020-01-01T17:30:00Z"
DECISION_UTC = "2020-01-01T18:00:00Z"
REPORT_UTC = "2020-01-01T19:00:00Z"
QA_APPROVAL_UTC = "2020-01-01T20:00:00Z"
RELEASE_APPROVAL_UTC = "2020-01-01T21:00:00Z"
RELEASE_DATE = "2020-01-01"
PERFORMANCE_NONCE = "1" * 32
PERFORMANCE_EXECUTABLE = b"#!/bin/sh\nexit 0\n"
PERFORMANCE_EXECUTABLE_MEMBER = "Tanks3D.app/Contents/MacOS/Tanks3D"
MINIMUM_RECORDING_BYTES = 64 * 1024
MAXIMUM_RECORDING_BYTES = 95_000_000
OBSERVATION_MANIFEST_SCHEMA = (
    "tanks3d-alpha-v2-interactive-observation-manifest-v1"
)
GAMEPLAY_EVENT_LOG_V2_SCHEMA = "tanks3d-gameplay-event-log-v2"
GAMEPLAY_EVENT_LOG_V2_PRODUCER = "Tanks3D Alpha QA Evidence Compiler"
GAMEPLAY_EVIDENCE_IDS = (
    "one_player_gameplay",
    "two_player_gameplay",
    "national_bases",
    "pickup_and_minimap",
    "settlement_report",
)
OBSERVATION_EVIDENCE_IDS = (
    "main_menu_and_advanced_settings",
) + GAMEPLAY_EVIDENCE_IDS
CONTROL_EVIDENCE_IDS = (
    "main_menu_and_advanced_settings",
    "one_player_gameplay",
    "two_player_gameplay",
)


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def png_chunk(kind, payload):
    checksum = zlib.crc32(kind)
    checksum = zlib.crc32(payload, checksum) & 0xFFFFFFFF
    return struct.pack(">I", len(payload)) + kind + payload + struct.pack(">I", checksum)


def write_png(path, width=1280, height=720):
    path.parent.mkdir(parents=True, exist_ok=True)
    seed = sum(path.name.encode("utf-8")) % 256
    pixel = bytes((seed, (seed * 3) & 0xFF, (seed * 7) & 0xFF, 255))
    rows = (b"\x00" + pixel * width) * height
    data = b"\x89PNG\r\n\x1a\n"
    data += png_chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0))
    data += png_chunk(b"IDAT", zlib.compress(rows, 9))
    data += png_chunk(b"IEND", b"")
    path.write_bytes(data)


class ReleaseFixture:
    def __init__(self, base):
        self.root = Path(base)
        self.status = json.loads(STATUS_TEMPLATE.read_text(encoding="utf-8"))
        self.status_path = self.root / "docs/releases/v0.1.0-alpha.3-status.json"
        requirements_directory = self.root / "docs/release-requirements"
        requirements_directory.mkdir(parents=True, exist_ok=True)
        (requirements_directory / "macos-alpha-v1.json").write_bytes(
            REQUIREMENTS_V1.read_bytes()
        )
        (requirements_directory / "macos-alpha-v2.json").write_bytes(
            REQUIREMENTS_V2.read_bytes()
        )
        self._write_tagged_stub()
        self._write_repository_audio_and_notices()
        self._write_candidate()
        self._write_screenshots()
        self._write_documents()
        self.write_status()
        self.commit_tree()

    def requirements(self):
        return json.loads(
            (self.root / self.status["requirements"]).read_text(encoding="utf-8")
        )

    def _write_tagged_stub(self):
        verifier = self.root / "scripts/verify_tagged_alpha_candidate.sh"
        verifier.parent.mkdir(parents=True, exist_ok=True)
        verifier.write_text(
            "#!/bin/sh\n"
            "if [ -f \"$1/tagged-verifier-must-fail\" ]; then\n"
            "  echo forced tagged verifier failure >&2\n"
            "  exit 9\n"
            "fi\n"
            "for candidate_file in \"$2\"/*; do\n"
            "  name=${candidate_file##*/}\n"
            "  value=$(shasum -a 256 \"$candidate_file\" | awk '{print $1}')\n"
            "  printf 'VERIFIED CANDIDATE FILE SHA256 %s %s\\n' \"$value\" \"$name\"\n"
            "done\n"
            "exit 0\n",
            encoding="utf-8",
        )

    def _write_repository_audio_and_notices(self):
        sound_dir = self.root / "resources/sounds"
        sound_dir.mkdir(parents=True, exist_ok=True)
        for index in range(22):
            (sound_dir / "fixture-cue-{:02d}.ogg".format(index)).write_bytes(
                b"OggS fixture cue " + str(index).encode("ascii") + b"\n"
            )
        notice_files = {
            "ASSET_LICENSES.md": "fixture asset and audio provenance\n",
            "THIRD_PARTY_NOTICES.md": "fixture third-party notices\n",
            "LICENSES/MIT-upstream.txt": "fixture upstream MIT notice\n",
        }
        for name, contents in notice_files.items():
            path = self.root / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(contents, encoding="utf-8")

    def _write_candidate(self):
        release = self.status["release"]
        release["source_commit"] = "a" * 40
        candidate = self.root / release["candidate_dir"]
        candidate.mkdir(parents=True, exist_ok=True)
        artifact_path = candidate / Path(release["artifact"]["path"]).name
        checksum_path = candidate / Path(release["checksum"]["path"]).name
        attestation_path = candidate / "attestation.txt"
        gate_log_path = candidate / "alpha-candidate-gates.log"
        build_config_path = candidate / "build-config.txt"
        with zipfile.ZipFile(artifact_path, "w", compression=zipfile.ZIP_STORED) as bundle:
            bundle.writestr(PERFORMANCE_EXECUTABLE_MEMBER, PERFORMANCE_EXECUTABLE)
            for sound in sorted((self.root / "resources/sounds").glob("*.ogg")):
                bundle.write(
                    sound,
                    "Tanks3D.app/Contents/Resources/sounds/{}".format(sound.name),
                )
        gate_log_path.write_text("all fixture gates pass\n", encoding="utf-8")
        build_config_path.write_text(
            "fixture=true\n"
            "performance-capability-schema=tanks3d-release-performance-capabilities-v1\n"
            "performance-telemetry-schema=tanks3d-performance-log-v2\n"
            "performance-capability-contract-sha256="
            "5137950da46fa11ee6d5ff60fafe67e83c4c0aacfb5fc83f2b0ce5f74afcfe1c\n",
            encoding="utf-8",
        )
        artifact_digest = digest(artifact_path)
        checksum_path.write_text(
            "{}  {}\n".format(artifact_digest, artifact_path.name), encoding="utf-8"
        )
        attestation = {
            "schema": "tanks3d-alpha-candidate-v3",
            "source_commit": release["source_commit"],
            "source_head_at_start": release["source_commit"],
            "source_head_at_finish": release["source_commit"],
            "source_tag": release["tag"],
            "source_tag_commit": release["source_commit"],
            "source_tree": "clean",
            "app_version": release["version"],
            "dist_channel": release["channel"],
            "artifact_filename": artifact_path.name,
            "artifact_sha256": artifact_digest,
            "checksum_filename": checksum_path.name,
            "build_config_filename": build_config_path.name,
            "build_config_sha256": digest(build_config_path),
            "gate_log_filename": gate_log_path.name,
            "gate_log_sha256": digest(gate_log_path),
        }
        attestation_path.write_text(
            "".join("{}={}\n".format(key, value) for key, value in attestation.items()),
            encoding="utf-8",
        )
        paths = {
            "artifact": artifact_path,
            "checksum": checksum_path,
            "attestation": attestation_path,
            "gate_log": gate_log_path,
            "build_config": build_config_path,
        }
        for key, path in paths.items():
            release[key]["path"] = path.relative_to(self.root).as_posix()
            release[key]["sha256"] = digest(path)

    def _write_screenshots(self):
        for evidence in self.status["evidence"]:
            for artifact in evidence["artifacts"]:
                path = self.root / artifact["path"]
                write_png(path)
                artifact["sha256"] = digest(path)

    def _write_documents(self, ready=False):
        release = self.status["release"]
        publication_asset_prefix = "docs/assets/releases/{}/".format(
            release["tag"]
        )
        screenshots = []
        for evidence in self.status["evidence"]:
            for artifact in evidence["artifacts"]:
                relative_asset = artifact["path"].removeprefix(
                    publication_asset_prefix
                )
                if (
                    artifact["kind"] == "png"
                    and artifact["path"].startswith(publication_asset_prefix)
                    and "/" not in relative_asset
                ):
                    screenshots.append(artifact)
        references = [
            "../assets/releases/{}/{}".format(release["tag"], Path(item["path"]).name)
            for item in screenshots
        ]
        identity = "\n".join(
            [
                release["tag"],
                release["source_commit"],
                Path(release["artifact"]["path"]).name,
                release["artifact"]["sha256"],
            ]
        )
        page_path = self.root / "docs/releases/{}.md".format(release["tag"])
        qa_path = self.root / "docs/releases/{}-qa.md".format(release["tag"])
        page_path.parent.mkdir(parents=True, exist_ok=True)
        requirements = self.requirements()
        gate_summary = (
            "## Release gate summary\n\n"
            "| Gate | Status |\n"
            "| --- | --- |\n"
            + "".join(
                "| {} | PASS |\n".format(row)
                for row in requirements["document_gate_rows"]
            )
            + "\n"
        )
        page_status = (
            "**Release status: APPROVED.**\n"
            "Audio decision: **ACCEPT**\n"
            "Known-issues review: **NONE_KNOWN**\n"
            "Gatekeeper conclusion: **PASS**\n"
            + RELEASE_DATE
            + "\n"
            + gate_summary
            if ready
            else "**Release status: BLOCKED.**\n"
        )
        qa_status = (
            "**Overall Alpha gate: PASS.**\n"
            "Selected option: **ACCEPT**\n"
            "Known-issues review: **NONE_KNOWN**\n"
            "Gatekeeper conclusion: **PASS**\n"
            "QA Lead\nRelease Owner\n"
            + RELEASE_DATE
            + "\n"
            + gate_summary
            if ready
            else "**Overall Alpha gate: BLOCKED.**\n"
        )
        page_path.write_text(
            page_status
            + identity
            + "\n"
            + qa_path.name
            + "\n"
            + "\n".join(references)
            + "\n",
            encoding="utf-8",
        )
        qa_path.write_text(
            qa_status
            + identity
            + "\n"
            + "\n".join(
                reference + " " + artifact["sha256"]
                for reference, artifact in zip(references, screenshots)
            )
            + "\n",
            encoding="utf-8",
        )
        self.status["documents"] = {
            "release_page": {
                "path": page_path.relative_to(self.root).as_posix(),
                "sha256": digest(page_path),
            },
            "qa_report": {
                "path": qa_path.relative_to(self.root).as_posix(),
                "sha256": digest(qa_path),
            },
        }

    def write_status(self):
        self.status_path.parent.mkdir(parents=True, exist_ok=True)
        self.status_path.write_text(
            json.dumps(self.status, indent=2, sort_keys=False) + "\n", encoding="utf-8"
        )

    def persistent_evidence_path(self, name):
        path = (
            self.root
            / "docs"
            / "assets"
            / "releases"
            / self.status["release"]["tag"]
            / "evidence"
            / name
        )
        path.parent.mkdir(parents=True, exist_ok=True)
        return path

    def replace_build_config(self, text):
        release = self.status["release"]
        build_config = self.root / release["build_config"]["path"]
        attestation = self.root / release["attestation"]["path"]
        build_config.write_text(text, encoding="utf-8")
        attestation_text = attestation.read_text(encoding="utf-8")
        attestation_text = re.sub(
            r"^build_config_sha256=.*$",
            "build_config_sha256={}".format(digest(build_config)),
            attestation_text,
            flags=re.MULTILINE,
        )
        attestation.write_text(attestation_text, encoding="utf-8")
        release["build_config"]["sha256"] = digest(build_config)
        release["attestation"]["sha256"] = digest(attestation)
        self.write_status()

    def replace_attestation_schema(self, schema):
        release = self.status["release"]
        attestation = self.root / release["attestation"]["path"]
        attestation.write_text(
            re.sub(
                r"^schema=.*$",
                "schema={}".format(schema),
                attestation.read_text(encoding="utf-8"),
                flags=re.MULTILINE,
            ),
            encoding="utf-8",
        )
        release["attestation"]["sha256"] = digest(attestation)
        self.write_status()

    def run(self, allow_blocked=False, status_path=None):
        command = [
            sys.executable,
            str(VERIFIER),
            "--project-root",
            str(self.root),
            "--status",
            str(status_path or self.status_path),
        ]
        if allow_blocked:
            command.insert(2, "--allow-blocked")
        return subprocess.run(
            command,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            env=self.git_test_environment(),
        )

    @staticmethod
    def git_test_environment():
        environment = {
            key: value
            for key, value in os.environ.items()
            if not key.startswith("GIT_")
        }
        environment.update(
            {
                "GIT_CONFIG_NOSYSTEM": "1",
                "GIT_CONFIG_GLOBAL": os.devnull,
                "LC_ALL": "C",
            }
        )
        return environment

    def git(self, *arguments, text=False):
        return subprocess.run(
            [
                "git",
                "-C",
                str(self.root),
                "-c",
                "commit.gpgSign=false",
                "-c",
                "core.hooksPath=/dev/null",
                "-c",
                "core.autocrlf=false",
                "-c",
                "user.name=Release Fixture",
                "-c",
                "user.email=fixture@example.invalid",
            ]
            + list(arguments),
            check=True,
            text=text,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            env=self.git_test_environment(),
        )

    def commit_tree(self):
        if not (self.root / ".git").is_dir():
            self.git("init", "-q")
        self.git("add", "-A")
        self.git(
            "commit",
            "--allow-empty",
            "-q",
            "-m",
            "fixture release evidence",
        )

    def add_evidence_artifact(self, evidence, name, kind="report"):
        path = self.persistent_evidence_path(name)
        if kind == "png":
            write_png(path)
        else:
            path.write_text("fixture evidence for {}\n".format(name), encoding="utf-8")
        evidence["artifacts"] = [
            {
                "path": path.relative_to(self.root).as_posix(),
                "sha256": digest(path),
                "kind": kind,
            }
        ]

    def append_evidence_artifact(self, evidence, path, kind="report"):
        evidence["artifacts"].append(
            {
                "path": path.relative_to(self.root).as_posix(),
                "sha256": digest(path),
                "kind": kind,
            }
        )

    def refresh_evidence_manifest(self, evidence):
        session_artifact = next(
            item for item in evidence["artifacts"] if item["path"].endswith("-session.json")
        )
        session_path = self.root / session_artifact["path"]
        session = json.loads(session_path.read_text(encoding="utf-8"))
        session["categories"][0]["artifact_sha256s"] = [
            item["sha256"]
            for item in evidence["artifacts"]
            if item is not session_artifact
        ]
        session_path.write_text(json.dumps(session, indent=2) + "\n", encoding="utf-8")
        session_artifact["sha256"] = digest(session_path)

    def refresh_artifact(self, evidence, artifact):
        artifact["sha256"] = digest(self.root / artifact["path"])
        changed_artifacts = {artifact["path"]: artifact}
        artifacts_by_name = {
            Path(item["path"]).name: item for item in evidence["artifacts"]
        }
        if Path(artifact["path"]).name == "command-log.json":
            command_log = json.loads(
                (self.root / artifact["path"]).read_text(encoding="utf-8")
            )
            for command in command_log.get("commands", []):
                command_id = command.get("id")
                if not isinstance(command_id, str):
                    continue
                prefix = command_id.replace("_", "-")
                for stream in ("stdout", "stderr"):
                    name = "{}.{}".format(prefix, stream)
                    raw_artifact = artifacts_by_name.get(name)
                    value = command.get(stream)
                    if raw_artifact is None or not isinstance(value, str):
                        continue
                    raw_path = self.root / raw_artifact["path"]
                    raw_path.write_text(value, encoding="utf-8")
                    raw_artifact["sha256"] = digest(raw_path)
                    changed_artifacts[raw_artifact["path"]] = raw_artifact
            intake_artifact = artifacts_by_name.get("clean-mac-intake.plist")
            if intake_artifact is not None:
                intake_path = self.root / intake_artifact["path"]
                intake = plistlib.loads(intake_path.read_bytes())
                intake["commands"] = command_log.get("commands", [])
                intake_path.write_bytes(
                    plistlib.dumps(
                        intake, fmt=plistlib.FMT_XML, sort_keys=False
                    )
                )
                intake_artifact["sha256"] = digest(intake_path)
                changed_artifacts[intake_artifact["path"]] = intake_artifact
        if Path(artifact["path"]).name == "browser-acquisition.json":
            acquisition = json.loads(
                (self.root / artifact["path"]).read_text(encoding="utf-8")
            )
            intake_artifact = artifacts_by_name.get("clean-mac-intake.plist")
            if intake_artifact is not None:
                intake_path = self.root / intake_artifact["path"]
                intake = plistlib.loads(intake_path.read_bytes())
                for key in (
                    "client",
                    "started_at_utc",
                    "completed_at_utc",
                    "zip_quarantine_agent",
                ):
                    intake["acquisition"][key] = acquisition.get(key)
                intake_path.write_bytes(
                    plistlib.dumps(
                        intake, fmt=plistlib.FMT_XML, sort_keys=False
                    )
                )
                intake_artifact["sha256"] = digest(intake_path)
                changed_artifacts[intake_artifact["path"]] = intake_artifact
        receipt_artifact = next(
            (
                item
                for item in evidence["artifacts"]
                if item["path"].endswith("performance-qa-receipt.json")
            ),
            None,
        )
        if receipt_artifact is not None and receipt_artifact is not artifact:
            receipt_path = self.root / receipt_artifact["path"]
            receipt = json.loads(receipt_path.read_text(encoding="utf-8"))
            for key in ("telemetry", "stdout", "stderr"):
                if receipt[key]["path"] == Path(artifact["path"]).name:
                    receipt[key]["sha256"] = artifact["sha256"]
            receipt_path.write_text(
                json.dumps(receipt, indent=2) + "\n", encoding="utf-8"
            )
            receipt_artifact["sha256"] = digest(receipt_path)
        clean_mac_receipt = next(
            (
                item
                for item in evidence["artifacts"]
                if item["path"].endswith("clean-mac-compiler-receipt.json")
            ),
            None,
        )
        if clean_mac_receipt is not None and clean_mac_receipt is not artifact:
            receipt_path = self.root / clean_mac_receipt["path"]
            receipt = json.loads(receipt_path.read_text(encoding="utf-8"))
            for reference in receipt["files"].values():
                changed = changed_artifacts.get(reference["path"])
                if changed is not None:
                    reference["sha256"] = changed["sha256"]
            receipt_path.write_text(
                json.dumps(receipt, indent=2) + "\n", encoding="utf-8"
            )
            clean_mac_receipt["sha256"] = digest(receipt_path)
        self.refresh_evidence_manifest(evidence)

    def performance_artifact(self, filename):
        evidence = next(
            item
            for item in self.status["evidence"]
            if item["id"] == "extended_session_metrics"
        )
        artifact = next(
            item
            for item in evidence["artifacts"]
            if Path(item["path"]).name == filename
        )
        return evidence, artifact, self.root / artifact["path"]

    def mutate_performance_json(self, filename, mutation):
        evidence, artifact, path = self.performance_artifact(filename)
        payload = json.loads(path.read_text(encoding="utf-8"))
        mutation(payload)
        path.write_text(json.dumps(payload) + "\n", encoding="utf-8")
        self.refresh_artifact(evidence, artifact)

    def mutate_observation_manifest(self, mutation):
        evidence_by_id = {item["id"]: item for item in self.status["evidence"]}
        first = evidence_by_id[OBSERVATION_EVIDENCE_IDS[0]]
        manifest_artifact = next(
            item
            for item in first["artifacts"]
            if Path(item["path"]).name == "observation-manifest.json"
        )
        path = self.root / manifest_artifact["path"]
        payload = json.loads(path.read_text(encoding="utf-8"))
        mutation(payload)
        path.write_text(
            json.dumps(payload, indent=2, sort_keys=True) + "\n",
            encoding="utf-8",
        )
        manifest_digest = digest(path)
        for evidence_id in OBSERVATION_EVIDENCE_IDS:
            evidence = evidence_by_id[evidence_id]
            shared = next(
                item
                for item in evidence["artifacts"]
                if Path(item["path"]).name == "observation-manifest.json"
            )
            shared["sha256"] = manifest_digest
            self.refresh_evidence_manifest(evidence)

    def clean_mac_artifact(self, filename):
        evidence = next(
            item
            for item in self.status["evidence"]
            if item["id"] == "gatekeeper_launch"
        )
        artifact = next(
            item
            for item in evidence["artifacts"]
            if Path(item["path"]).name == filename
        )
        return evidence, artifact, self.root / artifact["path"]

    def mutate_clean_mac_plist(self, filename, mutation):
        evidence, artifact, path = self.clean_mac_artifact(filename)
        payload = plistlib.loads(path.read_bytes())
        mutation(payload)
        path.write_bytes(
            plistlib.dumps(payload, fmt=plistlib.FMT_XML, sort_keys=False)
        )
        self.refresh_artifact(evidence, artifact)

    def set_clean_mac_url(self, url):
        self.status["clean_mac"]["details"]["download_url"] = url
        evidence, acquisition_artifact, acquisition_path = self.clean_mac_artifact(
            "browser-acquisition.json"
        )
        acquisition = json.loads(acquisition_path.read_text(encoding="utf-8"))
        acquisition["url"] = url
        acquisition_path.write_text(
            json.dumps(acquisition) + "\n", encoding="utf-8"
        )
        self.refresh_artifact(evidence, acquisition_artifact)

        evidence, where_artifact, where_path = self.clean_mac_artifact(
            "where-froms.hex"
        )
        where_path.write_text(
            plistlib.dumps([url], fmt=plistlib.FMT_BINARY).hex() + "\n",
            encoding="ascii",
        )
        self.refresh_artifact(evidence, where_artifact)
        self.mutate_clean_mac_plist(
            "clean-mac-plan.plist",
            lambda payload: payload.__setitem__("download_url", url),
        )
        _, _, plan_path = self.clean_mac_artifact("clean-mac-plan.plist")

        def update_intake(payload):
            payload["plan_sha256"] = digest(plan_path)
            payload["download_url"] = url
            payload["acquisition"]["where_froms_url"] = url
            payload["acquisition"]["where_froms_sha256"] = digest(where_path)

        self.mutate_clean_mac_plist("clean-mac-intake.plist", update_intake)

    def mutate_gameplay_event_log(self, evidence_id, mutation):
        evidence = next(
            item for item in self.status["evidence"] if item["id"] == evidence_id
        )
        artifact = next(
            item
            for item in evidence["artifacts"]
            if item["path"].endswith("-events.json")
        )
        path = self.root / artifact["path"]
        payload = json.loads(path.read_text(encoding="utf-8"))
        mutation(payload)
        path.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")
        self.refresh_artifact(evidence, artifact)

    def make_all_pass(self, profile="v2"):
        if profile == "v2":
            self.status["requirements"] = (
                "docs/release-requirements/macos-alpha-v2.json"
            )
        elif profile == "v1":
            self.status["requirements"] = (
                "docs/release-requirements/macos-alpha-v1.json"
            )
        else:
            raise ValueError("unsupported fixture profile")
        requirements = self.requirements()
        artifact = self.status["release"]["artifact"]
        candidate_digest = artifact["sha256"]
        fixture_tester = "Fixture Tester"
        evidence_by_id = {item["id"]: item for item in self.status["evidence"]}
        for evidence in self.status["evidence"]:
            if not evidence["artifacts"] and evidence["id"] in {
                "main_menu_and_advanced_settings",
                "gatekeeper_launch",
            }:
                if profile == "v2" and evidence["id"] == "gatekeeper_launch":
                    recording = self.persistent_evidence_path(
                        "gatekeeper-launch-recording.mov"
                    )
                    recording.write_bytes(
                        make_recording(minimum_bytes=MINIMUM_RECORDING_BYTES)
                    )
                    self.append_evidence_artifact(
                        evidence, recording, kind="recording"
                    )
                else:
                    self.add_evidence_artifact(
                        evidence, evidence["id"] + ".png", kind="png"
                    )
            evidence["status"] = "PASS"
            evidence["reviewer"] = "Fixture Reviewer"
            evidence["reviewed_at_utc"] = REVIEW_UTC
            evidence["notes"] = "Interactive evidence and release visuals reviewed."
            evidence["interactive"] = {
                "candidate_sha256": candidate_digest,
                "tester": fixture_tester,
                "machine": "Fixture Mac arm64",
                "tested_at_utc": UTC,
                "signature": fixture_tester,
                "coverage_refs": {},
            }
        if profile == "v2":
            for evidence_id in CONTROL_EVIDENCE_IDS:
                recording = self.persistent_evidence_path(
                    "{}-controls.mp4".format(evidence_id)
                )
                recording.write_bytes(
                    ("fixture controls recording for {}\n".format(evidence_id)).encode(
                        "utf-8"
                    ).ljust(MINIMUM_RECORDING_BYTES, b"0")
                )
                self.append_evidence_artifact(
                    evidence_by_id[evidence_id], recording, kind="recording"
                )

        for record in self.status["gameplay"]:
            record["status"] = "PASS"
            record["tester"] = fixture_tester
            record["tested_at_utc"] = UTC
            evidence_id = (
                "one_player_gameplay"
                if record["mode"] == "one_player"
                else "two_player_gameplay"
            )
            record["evidence_ids"] = [evidence_id]
            record["checks_confirmed"] = [record["id"]]
            record["notes"] = "Interactive gameplay check passed."
            token = "gameplay:{}:{}".format(record["id"], record["mode"])
            evidence_by_id[evidence_id]["interactive"]["coverage_refs"][token] = (
                "00:01:00"
            )
        base_checks = requirements["base_checks"]
        for record in self.status["bases"]:
            record["status"] = "PASS"
            record["tester"] = fixture_tester
            record["tested_at_utc"] = UTC
            record["evidence_ids"] = ["national_bases"]
            record["checks_confirmed"] = list(base_checks)
            record["notes"] = "Interactive national-base check passed."
            token = "base:{}:{}".format(record["id"], record["mode"])
            evidence_by_id["national_bases"]["interactive"]["coverage_refs"][token] = (
                "00:05:00"
            )
        pickup_checks = {
            item["id"]: item["checks"]
            for item in requirements["pickup_requirements"]
        }
        for record in self.status["pickups"]:
            record["status"] = "PASS"
            record["tester"] = fixture_tester
            record["tested_at_utc"] = UTC
            record["evidence_ids"] = ["pickup_and_minimap"]
            record["checks_confirmed"] = list(pickup_checks[record["id"]])
            record["notes"] = "Interactive pickup check passed."
            token = "pickup:{}:{}".format(record["id"], record["mode"])
            evidence_by_id["pickup_and_minimap"]["interactive"]["coverage_refs"][token] = (
                "00:10:00"
            )
        for record in self.status["settlement"]:
            record["status"] = "PASS"
            record["tester"] = fixture_tester
            record["tested_at_utc"] = UTC
            record["evidence_ids"] = ["settlement_report"]
            record["checks_confirmed"] = [record["id"]]
            record["notes"] = "Interactive settlement check passed."
            token = "settlement:{}".format(record["id"])
            evidence_by_id["settlement_report"]["interactive"]["coverage_refs"][token] = (
                "00:15:00"
            )

        controls = self.status["published_controls"]
        controls["status"] = "PASS"
        controls["tester"] = fixture_tester
        controls["tested_at_utc"] = UTC
        controls["evidence_ids"] = [
            "main_menu_and_advanced_settings",
            "one_player_gameplay",
            "two_player_gameplay",
        ]
        controls["checks_confirmed"] = list(requirements["published_control_checks"])
        controls["checks_confirmed"].extend(
            requirements.get("advanced_settings_checks", [])
        )
        controls["notes"] = (
            "Interactive evidence and release visuals reviewed."
            if profile == "v2"
            else "Every published control and advanced setting was exercised."
        )
        if profile == "v2":
            for context in requirements["published_control_context_requirements"]:
                evidence_by_id[context["evidence_id"]]["interactive"][
                    "coverage_refs"
                ][context["coverage_token"]] = "00:20:00"
        else:
            for evidence_id in controls["evidence_ids"]:
                evidence_by_id[evidence_id]["interactive"]["coverage_refs"][
                    "controls:published_controls_match"
                ] = "00:20:00"

        gate_evidence = {
            "clean_mac": (
                list(requirements["clean_mac_evidence_ids"])
                if profile == "v2"
                else ["main_menu_and_advanced_settings", "gatekeeper_launch"]
            ),
            "gatekeeper": ["gatekeeper_launch"],
            "extended_session": ["extended_session_metrics"],
        }
        gate_keys = {
            "clean_mac": requirements["clean_mac_detail_keys"],
            "gatekeeper": requirements["gatekeeper_detail_keys"],
            "extended_session": requirements["extended_session_detail_keys"],
        }
        for gate_name, keys in gate_keys.items():
            gate = self.status[gate_name]
            gate["status"] = "PASS"
            gate["tester"] = fixture_tester
            gate["tested_at_utc"] = UTC
            gate["evidence_ids"] = list(gate_evidence[gate_name])
            gate["checks_confirmed"] = list(keys)
            gate["details"] = {key: "recorded {}".format(key) for key in keys}
            gate["notes"] = "Release gate passed with recorded evidence."
            for evidence_id in gate["evidence_ids"]:
                evidence_by_id[evidence_id]["interactive"]["coverage_refs"][
                    "gate:{}".format(gate_name)
                ] = "00:25:00"
        self.status["clean_mac"]["details"] = {
            "mac_model": "MacBook Pro",
            "chip": "Apple M3 Pro",
            "uname_machine": "arm64",
            "ram": "18 GB",
            "macos_version": "26.5.2",
            "macos_build": "25F84",
            "clean_machine_method": "Fresh local account with no prior install",
            "download_url": "https://github.com/tourzhao/tanks3d/releases/download/v0.1.0-alpha.3/{}".format(
                Path(artifact["path"]).name
            ),
            "download_client": (
                requirements["clean_mac_download_client"]
                if profile == "v2"
                else "curl"
            ),
            "downloaded_artifact_filename": Path(artifact["path"]).name,
            "downloaded_artifact_sha256": candidate_digest,
            "checksum_command": "shasum -a 256 {}".format(
                Path(artifact["path"]).name
            ),
            "checksum_exit_code": "0",
            "prior_app_absent": "yes",
            "prior_approval_absent": "yes",
            "minimum_macos_met": "yes",
            "source_checkout_absent": "yes",
            "homebrew_raylib_unused": "yes",
        }
        self.status["gatekeeper"]["details"] = {
            "zip_quarantine_command": "xattr -p com.apple.quarantine {}".format(
                Path(artifact["path"]).name
            ),
            "zip_quarantine_exit_code": "0",
            "zip_quarantine_output": "0083;fixture;Safari;",
            "app_quarantine_command": "xattr -p com.apple.quarantine Tanks3D.app",
            "app_quarantine_exit_code": "0",
            "app_quarantine_output": "0083;fixture;Archive Utility;",
            "codesign_command": "codesign --verify --deep --strict --verbose=4 Tanks3D.app",
            "codesign_exit_code": "0",
            "spctl_command": "spctl --assess --type execute --verbose=4 Tanks3D.app",
            "spctl_exit_code": "1",
            "first_finder_launch": "Finder launch observed",
            "dialog_text": "Observed dialog recorded verbatim",
            "documented_launch_path": (
                "Finder launch attempt, then System Settings > Privacy & "
                "Security > Open Anyway"
            ),
            "main_menu_reached": "yes",
            "signature_preserved": "yes",
            "conclusion": "PASS",
            "release_note_wording_verified": "yes",
        }
        if profile == "v2":
            quarantine_hex = "5e0cc926"
            self.status["clean_mac"]["details"]["checksum_command"] = (
                "/usr/bin/shasum -a 256 {}".format(Path(artifact["path"]).name)
            )
            self.status["gatekeeper"]["details"].update(
                {
                    "zip_quarantine_command": (
                        "/usr/bin/xattr -p com.apple.quarantine {}".format(
                            Path(artifact["path"]).name
                        )
                    ),
                    "zip_quarantine_output": (
                        "0083;{};Safari;fixture".format(quarantine_hex)
                    ),
                    "app_quarantine_command": (
                        "/usr/bin/xattr -p com.apple.quarantine Tanks3D.app"
                    ),
                    "app_quarantine_output": (
                        "0083;{};Archive Utility;fixture".format(quarantine_hex)
                    ),
                    "codesign_command": (
                        "/usr/bin/codesign --verify --deep --strict "
                        "--verbose=4 Tanks3D.app"
                    ),
                    "spctl_command": (
                        "/usr/sbin/spctl --assess --type execute "
                        "--verbose=4 Tanks3D.app"
                    ),
                }
            )
        self.status["extended_session"]["details"] = {
            "duration_minutes": "30",
            "stages_completed": "2",
            "mode_mix": (
                "one-player"
                if profile == "v2"
                else "one-player and two-player"
            ),
            "measurement_tools": (
                "Candidate telemetry and performance QA runner"
                if profile == "v2"
                else "Activity Monitor and frame telemetry"
            ),
            "sampling_interval_seconds": "1" if profile == "v2" else "5",
            "fps_acceptance_criterion": "average at least 50 FPS and 1% low at least 30 FPS",
            "average_fps": "60",
            "minimum_fps": "60",
            "minimum_average_fps": "50",
            "minimum_one_percent_low_fps": "30",
            "one_percent_low_fps": "60",
            "thermal_state": "nominal",
            "throttling": "none observed",
            "fan_observation": "audible but stable",
            "memory_start_mb": "220",
            "memory_end_mb": "225",
            "memory_growth_observation": "stable after warmup",
            "maximum_memory_growth_mb": "256",
            "rendering_artifacts": "none observed",
            "audio_issues": "none observed",
            "crashes_hangs_or_softlocks": "none observed",
            "crash_count": "0",
            "hang_count": "0",
            "softlock_count": "0",
            "logs_and_capture_locations": (
                "evidence/performance-log-v2.json and "
                "evidence/performance-qa-receipt.json"
                if profile == "v2"
                else "evidence/performance-log.json"
            ),
        }

        artifact_name = Path(artifact["path"]).name
        clean_details = self.status["clean_mac"]["details"]
        gate_details = self.status["gatekeeper"]["details"]
        if profile == "v2":
            acquisition_log = self.persistent_evidence_path(
                "browser-acquisition.json"
            )
            acquisition_log.write_text(
                json.dumps(
                    {
                        "schema": requirements["browser_acquisition_schema"],
                        "candidate_sha256": candidate_digest,
                        "tester": fixture_tester,
                        "machine": "Fixture Mac arm64",
                        "client": requirements["clean_mac_download_client"],
                        "url": clean_details["download_url"],
                        "filename": artifact_name,
                        "started_at_utc": "2020-01-01T16:30:00Z",
                        "completed_at_utc": "2020-01-01T16:31:00Z",
                        "zip_quarantine_agent": "Safari",
                        "signature": fixture_tester,
                    },
                    indent=2,
                )
                + "\n",
                encoding="utf-8",
            )
            self.append_evidence_artifact(
                evidence_by_id["gatekeeper_launch"],
                acquisition_log,
                kind="report",
            )

        command_log = self.persistent_evidence_path("command-log.json")
        command_paths = (
            requirements["clean_mac_system_command_paths"]
            if profile == "v2"
            else {
                "shasum": "shasum",
                "xattr": "xattr",
                "codesign": "codesign",
                "spctl": "spctl",
            }
        )
        command_specs = [
            ("checksum", [command_paths["shasum"], "-a", "256", artifact_name], 0, "{}  {}\n".format(candidate_digest, artifact_name), ""),
            ("zip_quarantine", [command_paths["xattr"], "-p", "com.apple.quarantine", artifact_name], 0, gate_details["zip_quarantine_output"], ""),
            ("app_quarantine", [command_paths["xattr"], "-p", "com.apple.quarantine", "Tanks3D.app"], 0, gate_details["app_quarantine_output"], ""),
            (
                "codesign",
                [
                    command_paths["codesign"],
                    "--verify",
                    "--deep",
                    "--strict",
                    "--verbose=4",
                    "Tanks3D.app",
                ],
                0,
                "",
                "",
            ),
            (
                "spctl",
                [
                    command_paths["spctl"],
                    "--assess",
                    "--type",
                    "execute",
                    "--verbose=4",
                    "Tanks3D.app",
                ],
                1,
                "",
                "Tanks3D.app: rejected\nsource=Unnotarized Developer ID",
            ),
        ]
        if profile == "v1":
            command_specs.insert(
                0,
                (
                    "download",
                    [
                        "curl",
                        "--fail",
                        "--location",
                        "--output",
                        artifact_name,
                        clean_details["download_url"],
                    ],
                    0,
                    "downloaded {}".format(artifact_name),
                    "",
                ),
            )
        command_log.write_text(
            json.dumps(
                {
                    "schema": requirements["command_log_schema"],
                    "candidate_sha256": candidate_digest,
                    "machine": "Fixture Mac arm64",
                    "commands": [
                        {
                            "id": command_id,
                            "argv": argv,
                            "exit_code": exit_code,
                            "stdout": stdout,
                            "stderr": stderr,
                            "started_at_utc": "2020-01-01T16:{:02d}:00Z".format(
                                32 + index
                            ),
                            "completed_at_utc": "2020-01-01T16:{:02d}:00Z".format(
                                33 + index
                            ),
                        }
                        for index, (
                            command_id,
                            argv,
                            exit_code,
                            stdout,
                            stderr,
                        ) in enumerate(command_specs)
                    ],
                },
                indent=2,
            ) + "\n",
            encoding="utf-8",
        )
        self.append_evidence_artifact(evidence_by_id["gatekeeper_launch"], command_log, kind="log")
        raw_command_files = {}
        if profile == "v2":
            for command_id, _, _, stdout, stderr in command_specs:
                prefix = command_id.replace("_", "-")
                for stream, contents in (("stdout", stdout), ("stderr", stderr)):
                    path = self.persistent_evidence_path(
                        "{}.{}".format(prefix, stream)
                    )
                    path.write_text(contents, encoding="utf-8")
                    self.append_evidence_artifact(
                        evidence_by_id["gatekeeper_launch"], path, kind="log"
                    )
                    raw_command_files["{}_{}".format(command_id, stream)] = path
        if profile == "v2":
            gatekeeper_evidence = evidence_by_id["gatekeeper_launch"]
            raw_files = {
                name: self.persistent_evidence_path(name)
                for name in (
                    "clean-mac-plan.plist",
                    "clean-mac-intake.plist",
                    "where-froms.hex",
                )
            }
            plan = {
                "schema": requirements["clean_mac_plan_schema"],
                "requirements_profile": "macos-alpha-v2",
                "candidate_tag": self.status["release"]["tag"],
                "candidate_filename": artifact_name,
                "candidate_sha256": candidate_digest,
                "download_url": clean_details["download_url"],
                "minimum_macos_version": "26.0",
                "collector_sha256": requirements[
                    "clean_mac_collector_sha256"
                ],
                "prepared_at_utc": "2020-01-01T16:29:00Z",
                "session_nonce": "2" * 32,
            }
            raw_files["clean-mac-plan.plist"].write_bytes(
                plistlib.dumps(plan, fmt=plistlib.FMT_XML, sort_keys=False)
            )
            where_plist = plistlib.dumps(
                [clean_details["download_url"]], fmt=plistlib.FMT_BINARY
            )
            raw_files["where-froms.hex"].write_text(
                where_plist.hex() + "\n", encoding="ascii"
            )
            browser = json.loads(acquisition_log.read_text(encoding="utf-8"))
            command_records = json.loads(
                command_log.read_text(encoding="utf-8")
            )["commands"]
            intake = {
                "schema": requirements["clean_mac_intake_schema"],
                "plan_sha256": digest(raw_files["clean-mac-plan.plist"]),
                "session_nonce": plan["session_nonce"],
                "collector_sha256": plan["collector_sha256"],
                "candidate_filename": artifact_name,
                "candidate_sha256": candidate_digest,
                "download_url": clean_details["download_url"],
                "tester": fixture_tester,
                "tester_signature": fixture_tester,
                "machine": "Fixture Mac arm64",
                "machine_details": {
                    key: clean_details[key]
                    for key in requirements["clean_mac_machine_detail_keys"]
                },
                "session_started_at_utc": SESSION_START_UTC,
                "session_completed_at_utc": UTC,
                "acquisition": {
                    "client": browser["client"],
                    "started_at_utc": browser["started_at_utc"],
                    "completed_at_utc": browser["completed_at_utc"],
                    "zip_quarantine_agent": browser[
                        "zip_quarantine_agent"
                    ],
                    "quarantine_timestamp_utc": (
                        "2020-01-01T16:30:30Z"
                    ),
                    "where_froms_url": clean_details["download_url"],
                    "where_froms_sha256": digest(
                        raw_files["where-froms.hex"]
                    ),
                },
                "commands": command_records,
                "observations": {
                    key: gate_details[key]
                    for key in requirements["clean_mac_observation_keys"]
                },
                "notes": "Quarantined Finder launch reached the main menu.",
                "complete": True,
                "test_mode": False,
            }
            raw_files["clean-mac-intake.plist"].write_bytes(
                plistlib.dumps(intake, fmt=plistlib.FMT_XML, sort_keys=False)
            )
            for name, kind in (
                ("clean-mac-plan.plist", "report"),
                ("clean-mac-intake.plist", "report"),
                ("where-froms.hex", "log"),
            ):
                self.append_evidence_artifact(
                    gatekeeper_evidence, raw_files[name], kind=kind
                )
            media_artifact = next(
                item
                for item in gatekeeper_evidence["artifacts"]
                if item["kind"] in {"png", "recording"}
            )
            receipt_files = {
                "plan": {
                    "path": raw_files["clean-mac-plan.plist"].relative_to(
                        self.root
                    ).as_posix(),
                    "sha256": digest(raw_files["clean-mac-plan.plist"]),
                },
                "intake": {
                    "path": raw_files["clean-mac-intake.plist"].relative_to(
                        self.root
                    ).as_posix(),
                    "sha256": digest(raw_files["clean-mac-intake.plist"]),
                },
                "where_froms": {
                    "path": raw_files["where-froms.hex"].relative_to(
                        self.root
                    ).as_posix(),
                    "sha256": digest(raw_files["where-froms.hex"]),
                },
                "browser_acquisition": {
                    "path": acquisition_log.relative_to(self.root).as_posix(),
                    "sha256": digest(acquisition_log),
                },
                "command_log": {
                    "path": command_log.relative_to(self.root).as_posix(),
                    "sha256": digest(command_log),
                },
                "media": {
                    "path": media_artifact["path"],
                    "sha256": media_artifact["sha256"],
                },
            }
            receipt_files.update(
                {
                    file_id: {
                        "path": path.relative_to(self.root).as_posix(),
                        "sha256": digest(path),
                    }
                    for file_id, path in raw_command_files.items()
                }
            )
            receipt_path = self.persistent_evidence_path(
                "clean-mac-compiler-receipt.json"
            )
            receipt_path.write_text(
                json.dumps(
                    {
                        "schema": requirements[
                            "clean_mac_compiler_receipt_schema"
                        ],
                        "producer": requirements[
                            "clean_mac_compiler_receipt_producer"
                        ],
                        "candidate_sha256": candidate_digest,
                        "session_nonce": "2" * 32,
                        "collector_sha256": requirements[
                            "clean_mac_collector_sha256"
                        ],
                        "tester": fixture_tester,
                        "tester_signature": fixture_tester,
                        "reviewer": "Fixture Reviewer",
                        "reviewer_signature": "Fixture Reviewer",
                        "reviewed_at_utc": REVIEW_UTC,
                        "review_notes": (
                            "Interactive evidence and release visuals reviewed."
                        ),
                        "files": receipt_files,
                    },
                    indent=2,
                )
                + "\n",
                encoding="utf-8",
            )
            self.append_evidence_artifact(
                gatekeeper_evidence, receipt_path, kind="report"
            )

        performance_evidence = evidence_by_id["extended_session_metrics"]
        if profile == "v2":
            performance_log = self.persistent_evidence_path(
                "performance-log-v2.json"
            )
            memory_start = 220 * 1048576
            memory_growth = 5 * 1048576
            performance_log.write_text(
                json.dumps(
                    {
                        "schema": requirements["performance_log_schema"],
                        "producer": requirements["performance_producer"],
                        "source_commit": self.status["release"]["source_commit"],
                        "source_tag": self.status["release"]["tag"],
                        "candidate_sha256": candidate_digest,
                        "session_nonce": PERFORMANCE_NONCE,
                        "started_at_utc": SESSION_START_UTC,
                        "completed_at_utc": UTC,
                        "monotonic_duration_us": 1801 * 1000000,
                        "target_interval_us": 1000000,
                        "clock": requirements["performance_clock"],
                        "memory_metric": requirements[
                            "performance_memory_metric"
                        ],
                        "memory_unit": requirements["performance_memory_unit"],
                        "clean_shutdown": True,
                        "samples": [
                            {
                                "sequence": sequence,
                                "elapsed_us": sequence * 1000000,
                                "window_duration_us": 1000000,
                                "rendered_frames": 60,
                                "resident_bytes": memory_start
                                + memory_growth * (sequence - 1) // 1800,
                                "gameplay_duration_us": (
                                    0
                                    if sequence in {900, 1800}
                                    else 1000000
                                ),
                                "focused_duration_us": 1000000,
                                "stage_clear_events": (
                                    1 if sequence in {900, 1800} else 0
                                ),
                                "completed_stages": sequence // 900,
                                "stage_number": min(
                                    2, 1 + (sequence - 1) // 900
                                ),
                                "player_count": 1,
                                "app_state": (
                                    "settlement"
                                    if sequence in {900, 1800}
                                    else "gameplay"
                                ),
                                "window_focused": True,
                            }
                            for sequence in range(1, 1802)
                        ],
                    },
                    separators=(",", ":"),
                )
                + "\n",
                encoding="utf-8",
            )
            stdout_log = self.persistent_evidence_path(
                "performance-stdout.log"
            )
            stdout_log.write_text(
                "fixture candidate boot\n"
                "TANKS3D_PERFORMANCE_START {}\n"
                "fixture candidate running\n"
                "TANKS3D_PERFORMANCE_COMPLETE {}\n".format(
                    PERFORMANCE_NONCE, PERFORMANCE_NONCE
                ),
                encoding="utf-8",
            )
            stderr_log = self.persistent_evidence_path(
                "performance-stderr.log"
            )
            stderr_log.write_text("", encoding="utf-8")
            receipt_path = self.persistent_evidence_path(
                "performance-qa-receipt.json"
            )
            receipt_path.write_text(
                json.dumps(
                    {
                        "schema": requirements[
                            "performance_qa_receipt_schema"
                        ],
                        "candidate_filename": Path(artifact["path"]).name,
                        "candidate_sha256": candidate_digest,
                        "executable_sha256": hashlib.sha256(
                            PERFORMANCE_EXECUTABLE
                        ).hexdigest(),
                        "source_commit": self.status["release"][
                            "source_commit"
                        ],
                        "source_tag": self.status["release"]["tag"],
                        "session_nonce": PERFORMANCE_NONCE,
                        "argv": [
                            "/private/tmp/tanks3d-performance-qa-fixture/"
                            + PERFORMANCE_EXECUTABLE_MEMBER,
                            "--quick-start",
                            "--release-performance-log={}".format(
                                performance_log.resolve()
                            ),
                            "--release-candidate-sha256={}".format(
                                candidate_digest
                            ),
                            "--release-session-nonce={}".format(
                                PERFORMANCE_NONCE
                            ),
                            "--release-performance-duration-seconds=1801",
                        ],
                        "pid": 12345,
                        "started_at_utc": "2020-01-01T16:29:59Z",
                        "completed_at_utc": "2020-01-01T17:00:01Z",
                        "exit_code": 0,
                        "telemetry": {
                            "path": performance_log.name,
                            "sha256": digest(performance_log),
                        },
                        "stdout": {
                            "path": stdout_log.name,
                            "sha256": digest(stdout_log),
                        },
                        "stderr": {
                            "path": stderr_log.name,
                            "sha256": digest(stderr_log),
                        },
                    },
                    indent=2,
                )
                + "\n",
                encoding="utf-8",
            )
            for path, kind in (
                (performance_log, "log"),
                (stdout_log, "log"),
                (stderr_log, "log"),
                (receipt_path, "report"),
            ):
                self.append_evidence_artifact(
                    performance_evidence, path, kind=kind
                )
        else:
            performance_log = self.persistent_evidence_path(
                "performance-log.json"
            )
            performance_log.write_text(
                json.dumps(
                    {
                        "schema": requirements["performance_log_schema"],
                        "candidate_sha256": candidate_digest,
                        "started_at_utc": SESSION_START_UTC,
                        "completed_at_utc": UTC,
                        "samples": [
                            {
                                "elapsed_seconds": elapsed,
                                "fps": 60,
                                "memory_mb": 220 + 5 * elapsed / 1800,
                            }
                            for elapsed in range(0, 1801, 5)
                        ],
                    },
                    separators=(",", ":"),
                )
                + "\n",
                encoding="utf-8",
            )
            self.append_evidence_artifact(
                performance_evidence, performance_log, kind="log"
            )

        manifest_digest = None
        observations = []
        if profile == "v2":
            rows_by_token = {}
            for record in self.status["gameplay"]:
                rows_by_token[
                    "gameplay:{}:{}".format(record["id"], record["mode"])
                ] = record
            for record in self.status["bases"]:
                rows_by_token[
                    "base:{}:{}".format(record["id"], record["mode"])
                ] = record
            for record in self.status["pickups"]:
                rows_by_token[
                    "pickup:{}:{}".format(record["id"], record["mode"])
                ] = record
            for record in self.status["settlement"]:
                rows_by_token["settlement:{}".format(record["id"])] = record

            planned_tokens = []
            for gameplay_id in requirements["gameplay_ids"]:
                for mode in requirements["modes"]:
                    planned_tokens.append(
                        (
                            "gameplay:{}:{}".format(gameplay_id, mode),
                            (
                                "one_player_gameplay"
                                if mode == "one_player"
                                else "two_player_gameplay"
                            ),
                            [gameplay_id],
                        )
                    )
            for base_id in requirements["base_ids"]:
                for mode in requirements["modes"]:
                    planned_tokens.append(
                        (
                            "base:{}:{}".format(base_id, mode),
                            "national_bases",
                            list(requirements["base_checks"]),
                        )
                    )
            for pickup in requirements["pickup_requirements"]:
                for mode in requirements["modes"]:
                    planned_tokens.append(
                        (
                            "pickup:{}:{}".format(pickup["id"], mode),
                            "pickup_and_minimap",
                            list(pickup["checks"]),
                        )
                    )
            for settlement_id in requirements["settlement_ids"]:
                planned_tokens.append(
                    (
                        "settlement:{}".format(settlement_id),
                        "settlement_report",
                        [settlement_id],
                    )
                )
            for control_context in requirements[
                "published_control_context_requirements"
            ]:
                planned_tokens.append(
                    (
                        control_context["coverage_token"],
                        control_context["evidence_id"],
                        list(control_context["checks"]),
                    )
                )
            for token, evidence_id, checks in planned_tokens:
                notes = (
                    controls["notes"]
                    if token.startswith("controls:")
                    else rows_by_token[token]["notes"]
                )
                observations.append(
                    {
                        "coverage_token": token,
                        "evidence_id": evidence_id,
                        "required_checks": checks,
                        "result": "PASS",
                        "observed_at_utc": UTC,
                        "checks_confirmed": checks,
                        "notes": notes,
                    }
                )
            manifest = {
                "schema": OBSERVATION_MANIFEST_SCHEMA,
                "requirements_profile": "macos-alpha-v2",
                "event_log_schema": GAMEPLAY_EVENT_LOG_V2_SCHEMA,
                "event_log_producer": GAMEPLAY_EVENT_LOG_V2_PRODUCER,
                "candidate_sha256": candidate_digest,
                "tester": fixture_tester,
                "machine": "Fixture Mac arm64",
                "tester_signature": fixture_tester,
                "reviewer": "Fixture Reviewer",
                "reviewer_signature": "Fixture Reviewer",
                "started_at_utc": SESSION_START_UTC,
                "completed_at_utc": UTC,
                "reviewed_at_utc": REVIEW_UTC,
                "review_notes": "Interactive evidence and release visuals reviewed.",
                "supporting_artifacts": [
                    {
                        "evidence_id": evidence_id,
                        "artifacts": [
                            {"path": item["path"], "kind": item["kind"]}
                            for item in evidence_by_id[evidence_id]["artifacts"]
                            if item["kind"] in {"png", "recording"}
                        ],
                    }
                    for evidence_id in OBSERVATION_EVIDENCE_IDS
                ],
                "observations": observations,
            }
            manifest_path = self.persistent_evidence_path(
                "observation-manifest.json"
            )
            manifest_path.write_text(
                json.dumps(manifest, indent=2, sort_keys=True) + "\n",
                encoding="utf-8",
            )
            manifest_digest = digest(manifest_path)
            for evidence_id in OBSERVATION_EVIDENCE_IDS:
                self.append_evidence_artifact(
                    evidence_by_id[evidence_id], manifest_path, kind="report"
                )

        observations_by_evidence = {
            evidence_id: [
                item for item in observations if item["evidence_id"] == evidence_id
            ]
            for evidence_id in OBSERVATION_EVIDENCE_IDS
        }
        event_evidence_ids = (
            OBSERVATION_EVIDENCE_IDS
            if profile == "v2"
            else GAMEPLAY_EVIDENCE_IDS
        )
        for evidence_id in event_evidence_ids:
            evidence = evidence_by_id[evidence_id]
            tokens = (
                [item["coverage_token"] for item in observations_by_evidence[evidence_id]]
                if profile == "v2"
                else list(evidence["interactive"]["coverage_refs"])
            )
            for sequence, token in enumerate(tokens, 1):
                evidence["interactive"]["coverage_refs"][token] = "event:{}".format(sequence)
            event_log = self.persistent_evidence_path(
                "{}-events.json".format(evidence_id)
            )
            event_payload = {
                "schema": (
                    GAMEPLAY_EVENT_LOG_V2_SCHEMA
                    if profile == "v2"
                    else requirements["gameplay_event_log_schema"]
                ),
                "producer": (
                    GAMEPLAY_EVENT_LOG_V2_PRODUCER
                    if profile == "v2"
                    else "Tanks3D"
                ),
                "candidate_sha256": candidate_digest,
                "tester": fixture_tester,
                "machine": "Fixture Mac arm64",
                "started_at_utc": SESSION_START_UTC,
                "completed_at_utc": UTC,
                "events": [
                    {
                        "sequence": sequence,
                        "timestamp_utc": UTC,
                        "category_id": evidence_id,
                        "coverage_token": token,
                        "result": "PASS",
                    }
                    for sequence, token in enumerate(tokens, 1)
                ],
            }
            if profile == "v2":
                event_payload["observation_manifest_sha256"] = manifest_digest
            event_log.write_text(
                json.dumps(event_payload, indent=2) + "\n",
                encoding="utf-8",
            )
            self.append_evidence_artifact(evidence, event_log, kind="log")

        for evidence in self.status["evidence"]:
            session_report = self.persistent_evidence_path(
                "{}-session.json".format(evidence["id"])
            )
            session_report.write_text(
                json.dumps(
                    {
                        "schema": requirements["interactive_session_schema"],
                        "candidate_sha256": candidate_digest,
                        "tester": fixture_tester,
                        "machine": "Fixture Mac arm64",
                        "started_at_utc": SESSION_START_UTC,
                        "completed_at_utc": UTC,
                        "signature": fixture_tester,
                        "categories": [
                            {
                                "id": evidence["id"],
                                "tested_at_utc": UTC,
                                "reviewed_at_utc": REVIEW_UTC,
                                "result": "PASS",
                                "coverage_refs": evidence["interactive"]["coverage_refs"],
                                "artifact_sha256s": [item["sha256"] for item in evidence["artifacts"]],
                            }
                        ],
                    },
                    indent=2,
                ) + "\n",
                encoding="utf-8",
            )
            self.append_evidence_artifact(evidence, session_report, kind="report")

        audio_path = self.persistent_evidence_path("audio-decision.txt")
        audio_path.write_text("audio decision evidence\n", encoding="utf-8")
        audio_evidence = []
        for relative in (
            "ASSET_LICENSES.md",
            "THIRD_PARTY_NOTICES.md",
            "LICENSES/MIT-upstream.txt",
        ):
            path = self.root / relative
            audio_evidence.append(
                {
                    "path": relative,
                    "sha256": digest(path),
                    "kind": "report",
                }
            )
        audio_evidence.append(
            {
                "path": audio_path.relative_to(self.root).as_posix(),
                "sha256": digest(audio_path),
                "kind": "report",
            }
        )
        self.status["audio"] = {
            "decision": "ACCEPT",
            "rationale": "Release owner accepts the documented Alpha limitation.",
            "evidence": audio_evidence,
            "checks_confirmed": list(requirements["audio_decision_checks"][0]["checks"]),
            "owner": "Release Owner",
            "authority": "Project release owner",
            "signature": "Release Owner",
            "decided_at_utc": DECISION_UTC,
        }
        self.status["known_issues"] = {
            "status": "PASS",
            "conclusion": "NONE_KNOWN",
            "reviewer": "QA Lead",
            "reviewed_at_utc": DECISION_UTC,
            "signature": "QA Lead",
            "issues": [],
            "notes": "Signed review found no known release issues.",
        }
        self.status["report"] = {
            "qa_owner": "QA Lead",
            "completed_at_utc": REPORT_UTC,
            "release_date": RELEASE_DATE,
        }
        for approval in self.status["approvals"]:
            approval["status"] = "PASS"
            approval["name"] = "QA Lead" if approval["role"] == "qa_lead" else "Release Owner"
            approval["signature"] = approval["name"]
            approval["approved_at_utc"] = (
                QA_APPROVAL_UTC
                if approval["role"] == "qa_lead"
                else RELEASE_APPROVAL_UTC
            )
            approval["evidence_ids"] = list(evidence_by_id.keys())
            approval["notes"] = "Release evidence reviewed and approved."
        self._write_documents(ready=True)
        self.write_status()
        self.commit_tree()


class ReleaseStatusVerifierTests(unittest.TestCase):
    def new_fixture(self):
        temporary = tempfile.TemporaryDirectory(prefix="tanks3d-release-status-test-")
        self.addCleanup(temporary.cleanup)
        return ReleaseFixture(temporary.name)

    def assert_failed(self, result, fragment):
        self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn(fragment, result.stderr)

    def test_valid_blocked_status_passes_only_with_allow_blocked(self):
        fixture = self.new_fixture()
        result = fixture.run(allow_blocked=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("Verified blocked Alpha status", result.stdout)
        self.assertIn("BLOCKER: audio.decision=NONE", result.stdout)

    def test_default_mode_rejects_blocked_status(self):
        fixture = self.new_fixture()
        result = fixture.run()
        self.assert_failed(result, "release is not approved")

    def test_synthetic_all_pass_status_is_accepted(self):
        fixture = self.new_fixture()
        fixture.make_all_pass()
        result = fixture.run()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("all required Alpha gates PASS", result.stdout)

    def test_fixture_git_operations_ignore_global_signing_and_hooks(self):
        with tempfile.TemporaryDirectory(
            prefix="tanks3d-hostile-git-config-"
        ) as temporary:
            configuration_root = Path(temporary)
            hook_directory = configuration_root / "hooks"
            hook_directory.mkdir()
            hook_marker = configuration_root / "hook-ran"
            hook = hook_directory / "pre-commit"
            hook.write_text(
                "#!/bin/sh\nprintf 'unexpected\\n' > {!r}\nexit 97\n".format(
                    str(hook_marker)
                ),
                encoding="utf-8",
            )
            hook.chmod(0o700)
            global_configuration = configuration_root / "global.gitconfig"
            global_configuration.write_text(
                "[commit]\n"
                "\tgpgSign = true\n"
                "[core]\n"
                "\thooksPath = {}\n"
                "\tautocrlf = true\n".format(hook_directory),
                encoding="utf-8",
            )
            with mock.patch.dict(
                os.environ,
                {
                    "GIT_CONFIG_GLOBAL": str(global_configuration),
                    "GIT_CONFIG_NOSYSTEM": "0",
                    "GIT_CONFIG_COUNT": "1",
                    "GIT_CONFIG_KEY_0": "commit.gpgSign",
                    "GIT_CONFIG_VALUE_0": "true",
                },
            ):
                fixture = self.new_fixture()
                fixture.make_all_pass()
                result = fixture.run()
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertFalse(hook_marker.exists())

    def test_final_ready_evidence_must_be_persistent_but_blocked_intake_may_be_ignored(self):
        fixture = self.new_fixture()
        fixture.make_all_pass()
        evidence = fixture.status["evidence"][0]
        session = next(
            artifact
            for artifact in evidence["artifacts"]
            if artifact["path"].endswith("-session.json")
        )
        source = fixture.root / session["path"]
        ignored = (
            fixture.root
            / "build"
            / "release-evidence"
            / fixture.status["release"]["tag"]
            / source.name
        )
        ignored.parent.mkdir(parents=True, exist_ok=True)
        shutil.move(source, ignored)
        session["path"] = ignored.relative_to(fixture.root).as_posix()
        fixture.write_status()

        expected = "PASS evidence main_menu_and_advanced_settings.artifacts["
        self.assert_failed(fixture.run(), expected)
        self.assert_failed(fixture.run(allow_blocked=True), expected)

        blocked = self.new_fixture()
        local_diagnostic = (
            blocked.root
            / "build"
            / "release-evidence"
            / blocked.status["release"]["tag"]
            / "local-diagnostic.log"
        )
        local_diagnostic.parent.mkdir(parents=True, exist_ok=True)
        local_diagnostic.write_text("blocked diagnostic\n", encoding="utf-8")
        blocked.append_evidence_artifact(
            blocked.status["evidence"][-1], local_diagnostic, kind="log"
        )
        blocked.write_status()
        (blocked.root / ".gitignore").write_text("build/\n", encoding="utf-8")
        blocked.commit_tree()
        result = blocked.run(allow_blocked=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("Verified blocked Alpha status", result.stdout)

    def test_final_ready_audio_decision_report_must_be_persistent(self):
        fixture = self.new_fixture()
        fixture.make_all_pass()
        canonical_notices = {
            "ASSET_LICENSES.md",
            "THIRD_PARTY_NOTICES.md",
            "LICENSES/MIT-upstream.txt",
        }
        decision_report = next(
            artifact
            for artifact in fixture.status["audio"]["evidence"]
            if artifact["path"] not in canonical_notices
        )
        source = fixture.root / decision_report["path"]
        ignored = (
            fixture.root
            / "build"
            / "release-evidence"
            / fixture.status["release"]["tag"]
            / source.name
        )
        ignored.parent.mkdir(parents=True, exist_ok=True)
        shutil.move(source, ignored)
        decision_report["path"] = ignored.relative_to(fixture.root).as_posix()
        fixture.write_status()

        expected = "selected status.audio.evidence["
        self.assert_failed(fixture.run(), expected)
        self.assert_failed(fixture.run(allow_blocked=True), expected)

    def test_final_ready_status_must_be_canonical_and_evidence_committed(self):
        fixture = self.new_fixture()
        fixture.make_all_pass()
        draft = (
            fixture.root
            / "build"
            / "release-evidence"
            / fixture.status["release"]["tag"]
            / "status.next.json"
        )
        draft.parent.mkdir(parents=True, exist_ok=True)
        draft.write_text(
            json.dumps(fixture.status, indent=2) + "\n", encoding="utf-8"
        )
        self.assert_failed(
            fixture.run(status_path=draft),
            "release-ready status must be the canonical",
        )

        fixture = self.new_fixture()
        fixture.make_all_pass()
        uncommitted = fixture.persistent_evidence_path(
            "uncommitted-audio-decision-detail.txt"
        )
        uncommitted.write_text("uncommitted evidence\n", encoding="utf-8")
        relative = uncommitted.relative_to(fixture.root).as_posix()
        (fixture.root / ".git/info/exclude").write_text(
            relative + "\n", encoding="utf-8"
        )
        fixture.status["audio"]["evidence"].append(
            {
                "path": relative,
                "sha256": digest(uncommitted),
                "kind": "report",
            }
        )
        fixture.write_status()
        expected = "release evidence is not committed in HEAD: {}".format(relative)
        self.assert_failed(fixture.run(), expected)
        self.assert_failed(fixture.run(allow_blocked=True), expected)

    def test_promoted_status_and_evidence_bytes_must_match_head(self):
        fixture = self.new_fixture()
        fixture.make_all_pass()
        fixture.status_path.write_text(
            fixture.status_path.read_text(encoding="utf-8") + "\n",
            encoding="utf-8",
        )
        status_relative = fixture.status_path.relative_to(fixture.root).as_posix()
        fixture.git(
            "update-index",
            "--skip-worktree",
            "--",
            status_relative,
        )
        clean = fixture.git("status", "--porcelain", text=True)
        self.assertEqual(clean.stdout, "")
        expected = "release evidence bytes do not match HEAD: {}".format(
            status_relative
        )
        self.assert_failed(fixture.run(), expected)
        self.assert_failed(fixture.run(allow_blocked=True), expected)

        fixture = self.new_fixture()
        fixture.make_all_pass()
        canonical_notices = {
            "ASSET_LICENSES.md",
            "THIRD_PARTY_NOTICES.md",
            "LICENSES/MIT-upstream.txt",
        }
        artifact = next(
            item
            for item in fixture.status["audio"]["evidence"]
            if item["path"] not in canonical_notices
        )
        evidence_path = fixture.root / artifact["path"]
        evidence_path.write_text(
            evidence_path.read_text(encoding="utf-8") + "locally smudged\n",
            encoding="utf-8",
        )
        artifact["sha256"] = digest(evidence_path)
        fixture.write_status()
        fixture.git(
            "update-index",
            "--assume-unchanged",
            "--",
            artifact["path"],
        )
        fixture.git("add", "--", status_relative)
        fixture.git(
            "commit",
            "-q",
            "-m",
            "update fixture status only",
        )
        clean = fixture.git("status", "--porcelain", text=True)
        self.assertEqual(clean.stdout, "")
        self.assert_failed(
            fixture.run(),
            "release evidence bytes do not match HEAD: {}".format(
                artifact["path"]
            ),
        )

        fixture = self.new_fixture()
        fixture.make_all_pass()
        artifact = next(
            item
            for item in fixture.status["audio"]["evidence"]
            if item["path"] not in canonical_notices
        )
        evidence_path = fixture.root / artifact["path"]
        evidence_path.write_text(
            evidence_path.read_text(encoding="utf-8")
            + "worktree filter marker\n",
            encoding="utf-8",
        )
        artifact["sha256"] = digest(evidence_path)
        fixture.write_status()
        (fixture.root / ".gitattributes").write_text(
            "{} filter=fixture-smudge\n".format(artifact["path"]),
            encoding="utf-8",
        )
        for key, value in (
            ("filter.fixture-smudge.clean", "sed s/worktree/committed/g"),
            ("filter.fixture-smudge.smudge", "cat"),
            ("filter.fixture-smudge.required", "true"),
        ):
            fixture.git("config", key, value)
        fixture.commit_tree()
        clean = fixture.git("status", "--porcelain", text=True)
        self.assertEqual(clean.stdout, "")
        self.assert_failed(
            fixture.run(),
            "release evidence bytes do not match HEAD: {}".format(
                artifact["path"]
            ),
        )

    def test_v2_status_requires_current_candidate_and_performance_contract(self):
        def make_blocked_v2(candidate_fixture):
            candidate_fixture.make_all_pass()
            candidate_fixture.status["approvals"][1] = {
                "role": "release_owner",
                "status": "BLOCKED",
                "name": "",
                "signature": "",
                "approved_at_utc": None,
                "evidence_ids": [],
                "notes": "Release-owner approval is pending.",
            }
            candidate_fixture.status["report"] = {
                "qa_owner": "",
                "completed_at_utc": None,
                "release_date": None,
            }
            candidate_fixture._write_documents(ready=False)
            candidate_fixture.write_status()
            candidate_fixture.commit_tree()
            result = candidate_fixture.run(allow_blocked=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

        fixture = self.new_fixture()
        fixture.make_all_pass()
        fixture.replace_build_config("fixture=true\n")
        fixture.commit_tree()
        self.assert_failed(
            fixture.run(),
            "macos-alpha-v2 candidate build configuration lacks the current performance-capability-schema contract",
        )

        historical = self.new_fixture()
        make_blocked_v2(historical)
        historical.replace_build_config("fixture=true\n")
        historical.commit_tree()
        self.assert_failed(
            historical.run(allow_blocked=True),
            "macos-alpha-v2 candidate build configuration lacks the current performance-capability-schema contract",
        )

        legacy_candidate = self.new_fixture()
        legacy_candidate.make_all_pass()
        legacy_candidate.replace_attestation_schema("tanks3d-alpha-candidate-v2")
        legacy_candidate.commit_tree()
        self.assert_failed(
            legacy_candidate.run(),
            "macos-alpha-v2 candidate must use tanks3d-alpha-candidate-v3",
        )

        historical_candidate = self.new_fixture()
        make_blocked_v2(historical_candidate)
        historical_candidate.replace_attestation_schema(
            "tanks3d-alpha-candidate-v2"
        )
        historical_candidate.commit_tree()
        self.assert_failed(
            historical_candidate.run(allow_blocked=True),
            "macos-alpha-v2 candidate must use tanks3d-alpha-candidate-v3",
        )

    def test_missing_additional_and_duplicate_keys_are_rejected(self):
        fixture = self.new_fixture()
        del fixture.status["audio"]
        fixture.write_status()
        self.assert_failed(fixture.run(allow_blocked=True), "invalid keys")

        fixture = self.new_fixture()
        fixture.status["clean_mac"]["details"]["invented_check"] = "PASS"
        fixture.write_status()
        self.assert_failed(fixture.run(allow_blocked=True), "invalid keys")

        fixture = self.new_fixture()
        raw = fixture.status_path.read_text(encoding="utf-8")
        fixture.status_path.write_text(
            raw.replace("{\n", '{\n  "schema": "duplicate",\n', 1), encoding="utf-8"
        )
        self.assert_failed(fixture.run(allow_blocked=True), "duplicate JSON key")

    def test_tampered_requirements_profile_is_rejected(self):
        fixture = self.new_fixture()
        path = fixture.root / fixture.status["requirements"]
        requirements = json.loads(path.read_text(encoding="utf-8"))
        requirements["gameplay_ids"].pop()
        path.write_text(json.dumps(requirements), encoding="utf-8")
        self.assert_failed(fixture.run(allow_blocked=True), "exactly 15 entries")

        fixture = self.new_fixture()
        fixture.status["requirements"] = (
            "docs/release-requirements/macos-alpha-v2.json"
        )
        fixture.write_status()
        path = fixture.root / fixture.status["requirements"]
        requirements = json.loads(path.read_text(encoding="utf-8"))
        requirements["clean_mac_download_client"] = "curl"
        path.write_text(json.dumps(requirements), encoding="utf-8")
        self.assert_failed(
            fixture.run(allow_blocked=True),
            "requirements.clean_mac_download_client does not match",
        )

        for mutation, expected in (
            ("missing", "invalid keys (missing stderr)"),
            ("extra", "invalid keys (unexpected diagnostic)"),
            ("weakened", ".stderr does not match the canonical profile"),
        ):
            with self.subTest(performance_limits=mutation):
                fixture = self.new_fixture()
                fixture.status["requirements"] = (
                    "docs/release-requirements/macos-alpha-v2.json"
                )
                fixture.write_status()
                path = fixture.root / fixture.status["requirements"]
                requirements = json.loads(path.read_text(encoding="utf-8"))
                limits = requirements["performance_artifact_maximum_bytes"]
                if mutation == "missing":
                    limits.pop("stderr")
                elif mutation == "extra":
                    limits["diagnostic"] = 1
                else:
                    limits["stderr"] = 1024
                path.write_text(json.dumps(requirements), encoding="utf-8")
                self.assert_failed(
                    fixture.run(allow_blocked=True),
                    expected,
                )

    def test_candidate_hash_and_attestation_identity_are_rejected(self):
        fixture = self.new_fixture()
        artifact = fixture.root / fixture.status["release"]["artifact"]["path"]
        artifact.write_bytes(artifact.read_bytes() + b"tampered")
        self.assert_failed(fixture.run(allow_blocked=True), "hash mismatch")

        fixture = self.new_fixture()
        fixture.status["release"]["source_commit"] = "b" * 40
        fixture.write_status()
        self.assert_failed(
            fixture.run(allow_blocked=True), "attestation source_commit does not match"
        )

    def test_png_dimensions_are_enforced_even_when_hash_matches(self):
        fixture = self.new_fixture()
        artifact = fixture.status["evidence"][1]["artifacts"][0]
        path = fixture.root / artifact["path"]
        write_png(path, width=640, height=720)
        artifact["sha256"] = digest(path)
        fixture.write_status()
        self.assert_failed(fixture.run(allow_blocked=True), "must be 1280x720")

    def test_png_image_stream_is_decoded_even_when_crc_and_hash_match(self):
        fixture = self.new_fixture()
        artifact = fixture.status["evidence"][1]["artifacts"][0]
        path = fixture.root / artifact["path"]
        data = b"\x89PNG\r\n\x1a\n"
        data += png_chunk(
            b"IHDR", struct.pack(">IIBBBBB", 1280, 720, 8, 6, 0, 0, 0)
        )
        data += png_chunk(b"IDAT", b"not-a-zlib-stream")
        data += png_chunk(b"IEND", b"")
        path.write_bytes(data)
        artifact["sha256"] = digest(path)
        fixture.write_status()
        self.assert_failed(
            fixture.run(allow_blocked=True), "invalid PNG image stream"
        )

    def test_png_decode_is_bounded_before_size_validation(self):
        fixture = self.new_fixture()
        artifact = fixture.status["evidence"][1]["artifacts"][0]
        path = fixture.root / artifact["path"]
        expected_size = 720 * (1280 * 4 + 1)
        data = b"\x89PNG\r\n\x1a\n"
        data += png_chunk(
            b"IHDR", struct.pack(">IIBBBBB", 1280, 720, 8, 6, 0, 0, 0)
        )
        data += png_chunk(b"IDAT", zlib.compress(b"\x00" * (expected_size + 1), 9))
        data += png_chunk(b"IEND", b"")
        path.write_bytes(data)
        artifact["sha256"] = digest(path)
        fixture.write_status()
        self.assert_failed(
            fixture.run(allow_blocked=True), "exceeds the maximum decoded PNG size"
        )

    def test_png_rejects_non_contiguous_data_and_unknown_critical_chunks(self):
        for mutation, fragment in (
            ("split_idat", "non-contiguous IDAT chunks"),
            ("critical", "unknown critical PNG chunk"),
        ):
            with self.subTest(mutation=mutation):
                fixture = self.new_fixture()
                artifact = fixture.status["evidence"][1]["artifacts"][0]
                path = fixture.root / artifact["path"]
                if mutation == "split_idat":
                    pixel = bytes((7, 21, 49, 255))
                    compressed = zlib.compress(
                        (b"\x00" + pixel * 1280) * 720, 9
                    )
                    midpoint = len(compressed) // 2
                    data = b"\x89PNG\r\n\x1a\n"
                    data += png_chunk(
                        b"IHDR",
                        struct.pack(">IIBBBBB", 1280, 720, 8, 6, 0, 0, 0),
                    )
                    data += png_chunk(b"IDAT", compressed[:midpoint])
                    data += png_chunk(b"tEXt", b"separator")
                    data += png_chunk(b"IDAT", compressed[midpoint:])
                    data += png_chunk(b"IEND", b"")
                else:
                    original = path.read_bytes()
                    data = original[:33] + png_chunk(b"ABCD", b"") + original[33:]
                path.write_bytes(data)
                artifact["sha256"] = digest(path)
                fixture.write_status()
                self.assert_failed(fixture.run(allow_blocked=True), fragment)

    def test_pass_record_requires_tester_time_evidence_and_exact_checks(self):
        fixture = self.new_fixture()
        record = fixture.status["gameplay"][0]
        record["status"] = "PASS"
        record["tester"] = "Tester"
        record["tested_at_utc"] = UTC
        record["checks_confirmed"] = [record["id"]]
        fixture.write_status()
        self.assert_failed(fixture.run(allow_blocked=True), "PASS requires evidence_ids")

        fixture = self.new_fixture()
        record = fixture.status["bases"][0]
        record["status"] = "PASS"
        record["tester"] = "Tester"
        record["tested_at_utc"] = UTC
        record["evidence_ids"] = ["national_bases"]
        record["checks_confirmed"] = ["wall_damage"]
        fixture.write_status()
        self.assert_failed(fixture.run(allow_blocked=True), "checks_confirmed are not exact")

    def test_pass_sections_require_their_exact_evidence_classes(self):
        mutations = [
            (lambda status: status["gameplay"][0], ["two_player_gameplay"]),
            (lambda status: status["bases"][0], ["one_player_gameplay"]),
            (lambda status: status["pickups"][0], ["settlement_report"]),
            (lambda status: status["settlement"][0], ["pickup_and_minimap"]),
            (lambda status: status["clean_mac"], ["main_menu_and_advanced_settings"]),
            (lambda status: status["gatekeeper"], ["main_menu_and_advanced_settings"]),
            (lambda status: status["extended_session"], ["gatekeeper_launch"]),
        ]
        for select_record, wrong_evidence in mutations:
            fixture = self.new_fixture()
            fixture.make_all_pass()
            select_record(fixture.status)["evidence_ids"] = wrong_evidence
            fixture.write_status()
            self.assert_failed(fixture.run(), "PASS evidence_ids are not exact")

        fixture = self.new_fixture()
        fixture.make_all_pass()
        fixture.status["approvals"][0]["evidence_ids"].pop()
        fixture.write_status()
        self.assert_failed(fixture.run(), "must reference all eight evidence IDs")

    def test_pass_gate_details_must_prove_minimum_semantics(self):
        mutations = [
            (
                lambda status: status["clean_mac"]["details"].__setitem__(
                    "uname_machine", "x86_64"
                ),
                "must be arm64",
            ),
            (
                lambda status: status["clean_mac"]["details"].__setitem__(
                    "prior_approval_absent", "no"
                ),
                "must explicitly record",
            ),
            (
                lambda status: status["gatekeeper"]["details"].__setitem__(
                    "codesign_exit_code", "1"
                ),
                "codesign_exit_code must be 0",
            ),
            (
                lambda status: status["gatekeeper"]["details"].__setitem__(
                    "main_menu_reached", "no"
                ),
                "must explicitly record",
            ),
            (
                lambda status: status["extended_session"]["details"].__setitem__(
                    "duration_minutes", "29.9"
                ),
                "must be at least 30.0",
            ),
            (
                lambda status: status["extended_session"]["details"].__setitem__(
                    "stages_completed", "0"
                ),
                "must be at least 1.0",
            ),
        ]
        for mutate, expected_error in mutations:
            fixture = self.new_fixture()
            fixture.make_all_pass()
            mutate(fixture.status)
            fixture.write_status()
            self.assert_failed(fixture.run(), expected_error)

    def test_audio_decision_and_both_approvals_are_mandatory(self):
        fixture = self.new_fixture()
        fixture.make_all_pass()
        fixture.status["audio"]["evidence"] = []
        fixture.write_status()
        self.assert_failed(fixture.run(), "must hash the asset, third-party, and MIT notices")

        fixture = self.new_fixture()
        fixture.make_all_pass()
        fixture.status["approvals"][1]["status"] = "BLOCKED"
        fixture.write_status()
        self.assert_failed(fixture.run(), "blocked release page lacks")

    def test_release_page_and_qa_binding_cannot_be_removed(self):
        fixture = self.new_fixture()
        page_ref = fixture.status["documents"]["release_page"]
        page = fixture.root / page_ref["path"]
        artifact_hash = fixture.status["release"]["artifact"]["sha256"]
        page.write_text(
            page.read_text(encoding="utf-8").replace(artifact_hash, "hash removed"),
            encoding="utf-8",
        )
        page_ref["sha256"] = digest(page)
        fixture.write_status()
        self.assert_failed(fixture.run(allow_blocked=True), "release page does not reference")

    def test_release_pngs_cannot_be_swapped_between_evidence_categories(self):
        fixture = self.new_fixture()
        first = fixture.status["evidence"][1]["artifacts"]
        second = fixture.status["evidence"][2]["artifacts"]
        fixture.status["evidence"][1]["artifacts"] = second
        fixture.status["evidence"][2]["artifacts"] = first
        fixture.write_status()
        self.assert_failed(fixture.run(allow_blocked=True), "must contain exactly these release PNGs")

    def test_release_pngs_must_have_seven_distinct_pixel_images(self):
        fixture = self.new_fixture()
        fixture.make_all_pass()
        publication_prefix = "docs/assets/releases/{}/".format(
            fixture.status["release"]["tag"]
        )
        release_pngs = []
        for evidence in fixture.status["evidence"]:
            for artifact in evidence["artifacts"]:
                relative_asset = artifact["path"].removeprefix(
                    publication_prefix
                )
                if (
                    artifact["kind"] == "png"
                    and artifact["path"].startswith(publication_prefix)
                    and "/" not in relative_asset
                ):
                    release_pngs.append((evidence, artifact))
        self.assertEqual(len(release_pngs), 7)
        source = (fixture.root / release_pngs[0][1]["path"]).read_bytes()
        touched = {}
        for index, (evidence, artifact) in enumerate(release_pngs):
            path = fixture.root / artifact["path"]
            path.write_bytes(
                source[:-12]
                + png_chunk(
                    b"tEXt", "same-pixels-{}".format(index).encode("ascii")
                )
                + source[-12:]
            )
            artifact["sha256"] = digest(path)
            touched[evidence["id"]] = evidence
        self.assertEqual(
            len({artifact["sha256"] for _, artifact in release_pngs}), 7
        )
        for evidence in touched.values():
            fixture.refresh_evidence_manifest(evidence)
        fixture._write_documents(ready=True)
        fixture.write_status()
        self.assert_failed(
            fixture.run(), "release screenshots must show seven distinct images"
        )

    def test_status_symlink_and_non_normalized_evidence_path_are_rejected(self):
        fixture = self.new_fixture()
        status_link = fixture.status_path.with_name("status-link.json")
        status_link.symlink_to(fixture.status_path.name)
        self.assert_failed(
            fixture.run(allow_blocked=True, status_path=status_link),
            "must not traverse a symbolic link",
        )

        fixture = self.new_fixture()
        artifact = fixture.status["evidence"][1]["artifacts"][0]
        artifact["path"] = artifact["path"].replace("/one-player.png", "//one-player.png")
        fixture.write_status()
        self.assert_failed(
            fixture.run(allow_blocked=True), "must be a normalized repository-relative path"
        )

    def test_alpha_profile_rejects_non_alpha_channel(self):
        fixture = self.new_fixture()
        fixture.status["release"]["channel"] = "beta.1"
        fixture.write_status()
        self.assert_failed(fixture.run(allow_blocked=True), "must use alpha.N")

    def test_published_controls_and_advanced_settings_require_every_check(self):
        fixture = self.new_fixture()
        fixture.make_all_pass()
        fixture.status["published_controls"]["checks_confirmed"].pop()
        fixture.write_status()
        self.assert_failed(fixture.run(), "checks_confirmed are not exact")

    def test_ready_documents_cannot_retain_blocked_status(self):
        fixture = self.new_fixture()
        fixture.make_all_pass()
        page_ref = fixture.status["documents"]["release_page"]
        page = fixture.root / page_ref["path"]
        page.write_text(
            page.read_text(encoding="utf-8").replace(
                "**Release status: APPROVED.**", "**Release status: BLOCKED.**"
            ),
            encoding="utf-8",
        )
        page_ref["sha256"] = digest(page)
        fixture.write_status()
        self.assert_failed(fixture.run(), "one canonical APPROVED marker")

    def test_dynamic_pass_requires_candidate_bound_interactive_evidence(self):
        fixture = self.new_fixture()
        fixture.make_all_pass()
        menu_evidence = fixture.status["evidence"][0]
        menu_evidence["artifacts"] = [
            artifact
            for artifact in menu_evidence["artifacts"]
            if artifact["kind"] == "report"
        ]
        fixture.write_status()
        self.assert_failed(fixture.run(), "PASS requires one of png, recording")

        fixture = self.new_fixture()
        fixture.make_all_pass()
        evidence = fixture.status["evidence"][1]
        evidence["artifacts"] = [
            artifact for artifact in evidence["artifacts"] if artifact["kind"] == "png"
        ]
        fixture.write_status()
        self.assert_failed(fixture.run(), "requires a recording, log, or signed report")

        fixture = self.new_fixture()
        fixture.make_all_pass()
        fixture.status["evidence"][1]["interactive"]["candidate_sha256"] = "0" * 64
        fixture.write_status()
        self.assert_failed(fixture.run(), "is not bound to the candidate")

        fixture = self.new_fixture()
        fixture.make_all_pass()
        coverage = fixture.status["evidence"][1]["interactive"]["coverage_refs"]
        del coverage["gameplay:start_and_control:one_player"]
        fixture.write_status()
        self.assert_failed(fixture.run(), "does not cover")

        fixture = self.new_fixture()
        fixture.make_all_pass()
        fixture.status["evidence"][1]["notes"] = (
            "Rendering evidence only; not interactive."
        )
        fixture.write_status()
        self.assert_failed(fixture.run(), "contradicts PASS")

    def test_visual_evidence_rejects_empty_or_too_small_recordings(self):
        for size in (0, MINIMUM_RECORDING_BYTES - 1):
            fixture = self.new_fixture()
            fixture.make_all_pass()
            evidence = fixture.status["evidence"][0]
            evidence["artifacts"] = [
                artifact
                for artifact in evidence["artifacts"]
                if artifact["kind"] != "png"
            ]
            recording = fixture.persistent_evidence_path(
                "menu-{}.mp4".format(size)
            )
            recording.write_bytes(b"0" * size)
            fixture.append_evidence_artifact(
                evidence, recording, kind="recording"
            )
            fixture.refresh_evidence_manifest(evidence)
            fixture.write_status()
            self.assert_failed(
                fixture.run(), "recording must be at least {} bytes".format(
                    MINIMUM_RECORDING_BYTES
                )
            )

        fixture = self.new_fixture()
        fixture.make_all_pass()
        evidence = fixture.status["evidence"][0]
        evidence["artifacts"] = [
            artifact
            for artifact in evidence["artifacts"]
            if artifact["kind"] != "png"
        ]
        recording = fixture.persistent_evidence_path("menu-too-large.mp4")
        with recording.open("wb") as stream:
            stream.truncate(MAXIMUM_RECORDING_BYTES + 1)
        fixture.append_evidence_artifact(evidence, recording, kind="recording")
        fixture.refresh_evidence_manifest(evidence)
        fixture.write_status()
        self.assert_failed(
            fixture.run(), "exceeds its {}-byte release limit".format(
                MAXIMUM_RECORDING_BYTES
            )
        )

    def test_pass_language_and_audio_acceptance_cannot_contradict_status(self):
        note_cases = (
            (
                lambda status: status["gameplay"][0],
                "Required gameplay was not tested.",
                "not tested",
            ),
            (
                lambda status: status["evidence"][1],
                "Required gameplay was not exercised.",
                "not exercised",
            ),
            (
                lambda status: status["approvals"][0],
                "Final approval was skipped.",
                "skipped",
            ),
            (
                lambda status: status["known_issues"],
                "Known-issue review remains pending.",
                "pending",
            ),
        )
        for select_record, notes, expected in note_cases:
            fixture = self.new_fixture()
            fixture.make_all_pass()
            select_record(fixture.status)["notes"] = notes
            fixture.write_status()
            self.assert_failed(fixture.run(), expected)

        fixture = self.new_fixture()
        fixture.make_all_pass()
        fixture.status["audio"]["rationale"] = (
            "I reject this release and do not accept the documented audio risk."
        )
        fixture.write_status()
        self.assert_failed(fixture.run(), "contradicts the ACCEPT decision")

    def test_clean_mac_download_and_quarantine_are_candidate_bound(self):
        mutations = [
            (
                lambda status: status["clean_mac"]["details"].__setitem__(
                    "downloaded_artifact_sha256", "0" * 64
                ),
                "does not match the candidate",
            ),
            (
                lambda status: status["clean_mac"]["details"].__setitem__(
                    "checksum_exit_code", "1"
                ),
                "checksum_exit_code must be 0",
            ),
            (
                lambda status: status["gatekeeper"]["details"].__setitem__(
                    "zip_quarantine_output", "arbitrary quarantine value"
                ),
                "is not a quarantine record",
            ),
            (
                lambda status: status["gatekeeper"]["details"].__setitem__(
                    "app_quarantine_exit_code", "1"
                ),
                "app_quarantine_exit_code must be 0",
            ),
        ]
        for mutate, expected in mutations:
            fixture = self.new_fixture()
            fixture.make_all_pass()
            mutate(fixture.status)
            fixture.write_status()
            self.assert_failed(fixture.run(), expected)

    def test_extended_session_must_meet_declared_stability_criteria(self):
        mutations = [
            ("average_fps", "49", "average FPS misses"),
            ("one_percent_low_fps", "29", "1% low FPS misses"),
            ("average_fps", "inf", "must be finite"),
            ("crash_count", "1", "must be integer zero"),
            ("throttling", "severe", "records a release failure"),
            ("memory_end_mb", "400", "contradicts raw v2 samples"),
        ]
        for key, value, expected in mutations:
            fixture = self.new_fixture()
            fixture.make_all_pass()
            fixture.status["extended_session"]["details"][key] = value
            fixture.write_status()
            self.assert_failed(fixture.run(), expected)

    def test_audio_decision_is_specific_and_candidate_bound(self):
        fixture = self.new_fixture()
        fixture.make_all_pass()
        fixture.status["audio"]["decision"] = "REPLACE"
        fixture.write_status()
        self.assert_failed(fixture.run(), "REPLACE cannot approve this candidate")

        fixture = self.new_fixture()
        fixture.make_all_pass()
        fixture.status["audio"]["checks_confirmed"] = []
        fixture.write_status()
        self.assert_failed(fixture.run(), "checks_confirmed do not match")

        fixture = self.new_fixture()
        fixture.make_all_pass()
        sound = next((fixture.root / "resources/sounds").glob("*.ogg"))
        sound.write_bytes(sound.read_bytes() + b"tampered")
        fixture.write_status()
        self.assert_failed(fixture.run(), "candidate audio differs")

        fixture = self.new_fixture()
        fixture.make_all_pass()
        fixture.status["audio"]["owner"] = "Different Owner"
        fixture.write_status()
        self.assert_failed(fixture.run(), "must be the release owner")

    def test_known_issue_review_and_approval_chronology_are_enforced(self):
        fixture = self.new_fixture()
        fixture.make_all_pass()
        fixture.status["known_issues"]["conclusion"] = "NONE"
        fixture.write_status()
        self.assert_failed(fixture.run(), "requires a signed conclusion")

        fixture = self.new_fixture()
        fixture.make_all_pass()
        fixture.status["approvals"][0]["approved_at_utc"] = UTC
        fixture.write_status()
        self.assert_failed(fixture.run(), "approval predates required evidence")

        fixture = self.new_fixture()
        fixture.make_all_pass()
        known = fixture.status["known_issues"]
        known["conclusion"] = "RECORDED"
        known["issues"] = [
            {
                "id": "ISSUE-1",
                "severity": "LOW",
                "summary": "Minor fixture limitation",
                "reproduction": "Run the fixture once",
                "impact": "Cosmetic only",
                "workaround": "Ignore the fixture marker",
                "release_decision": "ACCEPT_FOR_ALPHA",
                "owner": "Release Owner",
                "evidence_ids": ["one_player_gameplay"],
            }
        ]
        known["notes"] = "ISSUE-1 is accepted for this Alpha."
        for key in ("release_page", "qa_report"):
            reference = fixture.status["documents"][key]
            path = fixture.root / reference["path"]
            text = path.read_text(encoding="utf-8")
            text = text.replace("**NONE_KNOWN**", "**RECORDED**")
            path.write_text(text + "ISSUE-1\n", encoding="utf-8")
            reference["sha256"] = digest(path)
        fixture.write_status()
        fixture.commit_tree()
        result = fixture.run()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_tagged_candidate_verifier_failure_is_fatal_in_allow_blocked_mode(self):
        fixture = self.new_fixture()
        (fixture.root / "tagged-verifier-must-fail").write_text("fail\n", encoding="utf-8")
        self.assert_failed(fixture.run(allow_blocked=True), "tagged candidate verifier failed")

    def test_tagged_candidate_receipt_must_match_all_five_files(self):
        fixture = self.new_fixture()
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
            "done\n",
            encoding="utf-8",
        )
        self.assert_failed(
            fixture.run(allow_blocked=True),
            "receipt does not match the five release candidate files",
        )

    def test_tagged_verifier_rejects_a_symlinked_scripts_ancestor(self):
        fixture = self.new_fixture()
        scripts = fixture.root / "scripts"
        moved = fixture.root / "scripts-real"
        scripts.rename(moved)
        scripts.symlink_to(moved, target_is_directory=True)
        self.assert_failed(
            fixture.run(allow_blocked=True), "must not traverse a symbolic link"
        )

    def test_structured_session_and_candidate_event_log_are_mandatory(self):
        fixture = self.new_fixture()
        fixture.make_all_pass()
        evidence = fixture.status["evidence"][1]
        session = next(item for item in evidence["artifacts"] if item["path"].endswith("-session.json"))
        path = fixture.root / session["path"]
        path.write_text("candidate signed session passed\n", encoding="utf-8")
        session["sha256"] = digest(path)
        fixture.write_status()
        self.assert_failed(fixture.run(), "exactly one structured interactive session")

        fixture = self.new_fixture()
        fixture.make_all_pass()
        evidence = fixture.status["evidence"][1]
        event = next(item for item in evidence["artifacts"] if item["path"].endswith("-events.json"))
        path = fixture.root / event["path"]
        payload = json.loads(path.read_text(encoding="utf-8"))
        payload["producer"] = "Fixture"
        path.write_text(json.dumps(payload) + "\n", encoding="utf-8")
        fixture.refresh_artifact(evidence, event)
        fixture.write_status()
        self.assert_failed(
            fixture.run(), "was not produced by the Alpha-v2 evidence compiler"
        )

        fixture = self.new_fixture()
        fixture.make_all_pass()
        evidence = fixture.status["evidence"][1]
        event = next(
            item
            for item in evidence["artifacts"]
            if item["path"].endswith("-events.json")
        )
        evidence["artifacts"].remove(event)
        fake_recording = fixture.persistent_evidence_path("fake.mp4")
        fake_recording.write_bytes(b"\0" * 65536)
        fixture.append_evidence_artifact(
            evidence, fake_recording, kind="recording"
        )
        fixture.refresh_evidence_manifest(evidence)
        fixture.write_status()
        self.assert_failed(
            fixture.run(), "requires exactly one Alpha-v2 compiler event log"
        )

        fixture = self.new_fixture()
        fixture.make_all_pass()
        evidence = fixture.status["evidence"][1]
        evidence["reviewer"] = evidence["interactive"]["tester"]
        fixture.write_status()
        self.assert_failed(
            fixture.run(), "tester and reviewer must be different people"
        )

    def test_v2_controls_require_exact_manifest_observations_and_aggregation(self):
        fixture = self.new_fixture()
        fixture.make_all_pass()
        fixture.mutate_observation_manifest(
            lambda payload: payload["observations"].pop()
        )
        fixture.write_status()
        self.assert_failed(
            fixture.run(), "observations do not exactly match the token plan"
        )

        fixture = self.new_fixture()
        fixture.make_all_pass()

        def change_control_checks(payload):
            observation = next(
                item
                for item in payload["observations"]
                if item["coverage_token"] == "controls:main_menu"
            )
            observation["required_checks"] = observation["required_checks"][:-1]

        fixture.mutate_observation_manifest(change_control_checks)
        fixture.write_status()
        self.assert_failed(fixture.run(), "does not match the canonical token plan")

        fixture = self.new_fixture()
        fixture.make_all_pass()
        fixture.status["published_controls"]["notes"] = (
            "A different non-blocking controls summary."
        )
        fixture.write_status()
        self.assert_failed(
            fixture.run(),
            "controls observations contradict status.published_controls",
        )

    def test_v2_main_menu_requires_manifest_event_session_and_compiler_producer(self):
        removals = (
            (
                lambda item: Path(item["path"]).name == "observation-manifest.json",
                "requires exactly one Alpha-v2 observation manifest report",
            ),
            (
                lambda item: item["path"].endswith(
                    "main_menu_and_advanced_settings-events.json"
                ),
                "requires exactly one Alpha-v2 compiler event log",
            ),
            (
                lambda item: item["path"].endswith(
                    "main_menu_and_advanced_settings-session.json"
                ),
                "requires exactly one structured interactive session report",
            ),
        )
        for predicate, expected in removals:
            with self.subTest(expected=expected):
                fixture = self.new_fixture()
                fixture.make_all_pass()
                evidence = fixture.status["evidence"][0]
                removed = next(
                    item for item in evidence["artifacts"] if predicate(item)
                )
                evidence["artifacts"].remove(removed)
                if not removed["path"].endswith("-session.json"):
                    fixture.refresh_evidence_manifest(evidence)
                fixture.write_status()
                self.assert_failed(fixture.run(), expected)

        fixture = self.new_fixture()
        fixture.make_all_pass()
        fixture.mutate_gameplay_event_log(
            "main_menu_and_advanced_settings",
            lambda payload: payload.__setitem__("producer", "Fixture"),
        )
        fixture.write_status()
        self.assert_failed(
            fixture.run(), "was not produced by the Alpha-v2 evidence compiler"
        )

    def test_v2_controls_reject_extra_interactive_coverage_reference(self):
        fixture = self.new_fixture()
        fixture.make_all_pass()
        main_coverage = fixture.status["evidence"][0]["interactive"][
            "coverage_refs"
        ]
        del main_coverage["controls:main_menu"]
        main_coverage["controls:forged"] = "event:1"
        fixture.write_status()
        self.assert_failed(
            fixture.run(), "interactive evidence does not cover 'controls:main_menu'"
        )

        fixture = self.new_fixture()
        fixture.make_all_pass()
        evidence = fixture.status["evidence"][0]
        evidence["interactive"]["coverage_refs"]["controls:forged"] = "event:2"
        session_artifact = next(
            item
            for item in evidence["artifacts"]
            if item["path"].endswith("-session.json")
        )
        session_path = fixture.root / session_artifact["path"]
        session = json.loads(session_path.read_text(encoding="utf-8"))
        session["categories"][0]["coverage_refs"] = evidence["interactive"][
            "coverage_refs"
        ]
        session_path.write_text(
            json.dumps(session, indent=2) + "\n", encoding="utf-8"
        )
        session_artifact["sha256"] = digest(session_path)
        fixture.write_status()
        self.assert_failed(
            fixture.run(), "interactive coverage references are not exact"
        )

    def test_v2_control_supporting_groups_each_require_a_recording(self):
        for evidence_id in CONTROL_EVIDENCE_IDS:
            with self.subTest(evidence_id=evidence_id):
                fixture = self.new_fixture()
                fixture.make_all_pass()

                def remove_recording(payload):
                    group = next(
                        item
                        for item in payload["supporting_artifacts"]
                        if item["evidence_id"] == evidence_id
                    )
                    group["artifacts"] = [
                        item
                        for item in group["artifacts"]
                        if item["kind"] != "recording"
                    ]

                fixture.mutate_observation_manifest(remove_recording)
                fixture.write_status()
                self.assert_failed(
                    fixture.run(), "requires a recording for controls verification"
                )

    def test_compiler_output_is_accepted_end_to_end_and_producer_drift_is_rejected(self):
        fixture = self.new_fixture()
        fixture.status["requirements"] = (
            "docs/release-requirements/macos-alpha-v2.json"
        )
        for evidence in fixture.status["evidence"]:
            if evidence["id"] in OBSERVATION_EVIDENCE_IDS:
                evidence["status"] = "NOT_RUN"
            if evidence["id"] in CONTROL_EVIDENCE_IDS:
                recording = fixture.persistent_evidence_path(
                    "compiler-{}-controls.mp4".format(evidence["id"])
                )
                recording.write_bytes(b"1" * MINIMUM_RECORDING_BYTES)
                fixture.append_evidence_artifact(
                    evidence, recording, kind="recording"
                )
        fixture.write_status()

        plan_path = fixture.root / "observation-plan.json"
        output_dir = fixture.persistent_evidence_path(
            "interactive-compiled"
        )
        init_result = subprocess.run(
            [
                sys.executable,
                str(EVIDENCE_COMPILER),
                "init-plan",
                "--requirements",
                str(fixture.root / fixture.status["requirements"]),
                "--output",
                str(plan_path),
            ],
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )
        self.assertEqual(
            init_result.returncode, 0, init_result.stdout + init_result.stderr
        )

        plan = json.loads(plan_path.read_text(encoding="utf-8"))
        self.assertEqual(len(plan["observations"]), 70)
        self.assertEqual(
            [
                item["coverage_token"]
                for item in plan["observations"][-3:]
            ],
            ["controls:main_menu", "controls:one_player", "controls:two_player"],
        )
        plan.update(
            {
                "candidate_sha256": fixture.status["release"]["artifact"][
                    "sha256"
                ],
                "tester": "Compiler Fixture Tester",
                "machine": "Compiler Fixture Mac arm64",
                "tester_signature": "Compiler Fixture Tester",
                "reviewer": "Compiler Fixture Reviewer",
                "reviewer_signature": "Compiler Fixture Reviewer",
                "started_at_utc": SESSION_START_UTC,
                "completed_at_utc": UTC,
                "reviewed_at_utc": REVIEW_UTC,
                "review_notes": "Compiled interactive evidence reviewed.",
            }
        )
        for observation in plan["observations"]:
            observation["result"] = "PASS"
            observation["observed_at_utc"] = UTC
            observation["checks_confirmed"] = list(
                observation["required_checks"]
            )
            observation["notes"] = "Explicitly exercised {}.".format(
                observation["coverage_token"]
            )
        evidence_by_id = {
            item["id"]: item for item in fixture.status["evidence"]
        }
        for support_group in plan["supporting_artifacts"]:
            support_group["artifacts"] = [
                {"path": artifact["path"], "kind": artifact["kind"]}
                for artifact in evidence_by_id[support_group["evidence_id"]][
                    "artifacts"
                ]
                if artifact["kind"] in {"png", "recording"}
            ]
            self.assertTrue(support_group["artifacts"])
            if support_group["evidence_id"] in CONTROL_EVIDENCE_IDS:
                self.assertTrue(
                    any(
                        item["kind"] == "recording"
                        for item in support_group["artifacts"]
                    )
                )
        plan_path.write_text(
            json.dumps(plan, indent=2, sort_keys=True) + "\n",
            encoding="utf-8",
        )

        compile_result = subprocess.run(
            [
                sys.executable,
                str(EVIDENCE_COMPILER),
                "compile",
                "--project-root",
                str(fixture.root),
                "--status",
                fixture.status_path.relative_to(fixture.root).as_posix(),
                "--manifest",
                plan_path.relative_to(fixture.root).as_posix(),
                "--output-dir",
                output_dir.relative_to(fixture.root).as_posix(),
            ],
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )
        self.assertEqual(
            compile_result.returncode,
            0,
            compile_result.stdout + compile_result.stderr,
        )
        next_status_path = output_dir / "status.next.json"
        fixture.commit_tree()
        valid_result = fixture.run(
            allow_blocked=True, status_path=next_status_path
        )
        self.assertEqual(
            valid_result.returncode, 0, valid_result.stdout + valid_result.stderr
        )
        self.assertIn("Verified blocked Alpha status", valid_result.stdout)

        next_status = json.loads(next_status_path.read_text(encoding="utf-8"))
        self.assertEqual(next_status["published_controls"]["status"], "PASS")
        self.assertEqual(
            next_status["published_controls"]["evidence_ids"],
            list(CONTROL_EVIDENCE_IDS),
        )
        self.assertEqual(
            len(next_status["published_controls"]["checks_confirmed"]), 21
        )
        one_player = next(
            item
            for item in next_status["evidence"]
            if item["id"] == "one_player_gameplay"
        )
        event_artifact = next(
            item
            for item in one_player["artifacts"]
            if item["path"].endswith("-events.json")
        )
        event_path = fixture.root / event_artifact["path"]
        event_log = json.loads(event_path.read_text(encoding="utf-8"))
        event_log["producer"] = "Drifted Producer"
        event_path.write_text(
            json.dumps(event_log, indent=2, sort_keys=True) + "\n",
            encoding="utf-8",
        )
        event_artifact["sha256"] = digest(event_path)
        session_artifact = next(
            item
            for item in one_player["artifacts"]
            if item["path"].endswith("-session.json")
        )
        session_path = fixture.root / session_artifact["path"]
        session = json.loads(session_path.read_text(encoding="utf-8"))
        session["categories"][0]["artifact_sha256s"] = [
            item["sha256"]
            for item in one_player["artifacts"]
            if item is not session_artifact
        ]
        session_path.write_text(
            json.dumps(session, indent=2, sort_keys=True) + "\n",
            encoding="utf-8",
        )
        session_artifact["sha256"] = digest(session_path)
        next_status_path.write_text(
            json.dumps(next_status, indent=2, sort_keys=True) + "\n",
            encoding="utf-8",
        )
        self.assert_failed(
            fixture.run(allow_blocked=True, status_path=next_status_path),
            "was not produced by the Alpha-v2 evidence compiler",
        )

    def test_v2_event_log_is_exact_and_bound_to_the_manifest_file(self):
        for mutation, expected in (
            (
                lambda payload: payload.pop("observation_manifest_sha256"),
                "invalid keys (missing observation_manifest_sha256)",
            ),
            (
                lambda payload: payload.__setitem__(
                    "observation_manifest_sha256", "0" * 64
                ),
                "does not match the attached manifest file",
            ),
            (
                lambda payload: payload["events"].pop(),
                "event coverage is not exact",
            ),
            (
                lambda payload: payload["events"][0].__setitem__(
                    "timestamp_utc", "2020-01-01T16:45:00Z"
                ),
                "contradicts its observation manifest entry",
            ),
        ):
            with self.subTest(expected=expected):
                fixture = self.new_fixture()
                fixture.make_all_pass()
                fixture.mutate_gameplay_event_log(
                    "one_player_gameplay", mutation
                )
                fixture.write_status()
                self.assert_failed(fixture.run(), expected)

    def test_v2_manifest_is_required_and_matches_all_release_identities(self):
        fixture = self.new_fixture()
        fixture.make_all_pass()
        evidence = next(
            item
            for item in fixture.status["evidence"]
            if item["id"] == "one_player_gameplay"
        )
        manifest = next(
            item
            for item in evidence["artifacts"]
            if Path(item["path"]).name == "observation-manifest.json"
        )
        evidence["artifacts"].remove(manifest)
        fixture.refresh_evidence_manifest(evidence)
        fixture.write_status()
        self.assert_failed(
            fixture.run(),
            "requires exactly one Alpha-v2 observation manifest report",
        )

        mutations = (
            (
                lambda payload: payload.__setitem__("candidate_sha256", "b" * 64),
                "candidate does not match the release artifact",
            ),
            (
                lambda payload: payload.__setitem__("tester", "Different Tester"),
                ".tester does not match interactive evidence",
            ),
            (
                lambda payload: payload.__setitem__("machine", "Different Mac"),
                ".machine does not match interactive evidence",
            ),
            (
                lambda payload: payload.__setitem__(
                    "started_at_utc", "2020-01-01T16:31:00Z"
                ),
                "interval does not match its interactive session",
            ),
            (
                lambda payload: payload.__setitem__("reviewer", "Other Reviewer"),
                ".reviewer does not match evidence review",
            ),
            (
                lambda payload: payload.__setitem__(
                    "reviewed_at_utc", "2020-01-01T17:31:00Z"
                ),
                ".reviewed_at_utc does not match evidence review",
            ),
        )
        for mutation, expected in mutations:
            with self.subTest(expected=expected):
                fixture = self.new_fixture()
                fixture.make_all_pass()
                fixture.mutate_observation_manifest(mutation)
                fixture.write_status()
                self.assert_failed(fixture.run(), expected)

    def test_v2_manifest_observations_are_exact_and_status_bound(self):
        mutations = (
            (
                lambda payload: payload["observations"][0].__setitem__(
                    "coverage_token", "gameplay:invented:one_player"
                ),
                "does not match the canonical token plan",
            ),
            (
                lambda payload: payload["observations"][0].__setitem__(
                    "result", "FAIL"
                ),
                ".result must be PASS",
            ),
            (
                lambda payload: payload["observations"][0].__setitem__(
                    "checks_confirmed", []
                ),
                "checks_confirmed are not exact",
            ),
            (
                lambda payload: payload["observations"][0].__setitem__(
                    "notes", ""
                ),
                ".notes is missing or a placeholder",
            ),
            (
                lambda payload: payload["observations"][0].__setitem__(
                    "notes", "Different verified observation."
                ),
                "contradicts its release-status row",
            ),
        )
        for mutation, expected in mutations:
            with self.subTest(expected=expected):
                fixture = self.new_fixture()
                fixture.make_all_pass()
                fixture.mutate_observation_manifest(mutation)
                fixture.write_status()
                self.assert_failed(fixture.run(), expected)

    def test_blocked_v2_status_lints_attached_known_structured_artifacts(self):
        for name, kind in (
            ("observation-manifest.json", "report"),
            ("one_player_gameplay-events.json", "log"),
        ):
            with self.subTest(name=name):
                fixture = self.new_fixture()
                fixture.status["requirements"] = (
                    "docs/release-requirements/macos-alpha-v2.json"
                )
                evidence = next(
                    item
                    for item in fixture.status["evidence"]
                    if item["id"] == "one_player_gameplay"
                )
                path = fixture.root / "evidence" / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text("malformed Alpha-v2 evidence\n", encoding="utf-8")
                fixture.append_evidence_artifact(evidence, path, kind=kind)
                fixture.write_status()
                self.assert_failed(
                    fixture.run(allow_blocked=True),
                    "does not contain the expected Alpha-v2 schema",
                )

    def test_v2_command_log_uses_safari_and_five_post_download_commands(self):
        fixture = self.new_fixture()
        fixture.make_all_pass()
        self.assertEqual(
            fixture.status["clean_mac"]["details"]["download_client"],
            "Safari",
        )
        evidence = fixture.status["evidence"][6]
        command = next(
            item
            for item in evidence["artifacts"]
            if item["path"].endswith("command-log.json")
        )
        payload = json.loads(
            (fixture.root / command["path"]).read_text(encoding="utf-8")
        )
        self.assertEqual(
            [record["id"] for record in payload["commands"]],
            fixture.requirements()["command_log_command_ids"],
        )
        result = fixture.run()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_v2_clean_mac_receipt_and_raw_transcripts_are_fail_closed(self):
        fixture = self.new_fixture()
        fixture.make_all_pass()
        evidence = next(
            item
            for item in fixture.status["evidence"]
            if item["id"] == "gatekeeper_launch"
        )
        receipt = next(
            item
            for item in evidence["artifacts"]
            if item["path"].endswith("clean-mac-compiler-receipt.json")
        )
        evidence["artifacts"].remove(receipt)
        fixture.refresh_evidence_manifest(evidence)
        fixture.write_status()
        self.assert_failed(fixture.run(), "exactly one compiler receipt")

        fixture = self.new_fixture()
        fixture.make_all_pass()
        evidence = next(
            item
            for item in fixture.status["evidence"]
            if item["id"] == "gatekeeper_launch"
        )
        receipt = next(
            item
            for item in evidence["artifacts"]
            if item["path"].endswith("clean-mac-compiler-receipt.json")
        )
        receipt_path = fixture.root / receipt["path"]
        payload = json.loads(receipt_path.read_text(encoding="utf-8"))
        payload["collector_sha256"] = "f" * 64
        receipt_path.write_text(json.dumps(payload) + "\n", encoding="utf-8")
        fixture.refresh_artifact(evidence, receipt)
        fixture.write_status()
        self.assert_failed(fixture.run(), "canonical collector")

        fixture = self.new_fixture()
        fixture.make_all_pass()
        evidence_by_id = {
            item["id"]: item for item in fixture.status["evidence"]
        }
        evidence = evidence_by_id["gatekeeper_launch"]
        receipt = next(
            item
            for item in evidence["artifacts"]
            if item["path"].endswith("clean-mac-compiler-receipt.json")
        )
        source = next(
            item
            for item in evidence["artifacts"]
            if Path(item["path"]).name == "checksum.stdout"
        )
        foreign = fixture.root / "evidence/foreign/checksum.stdout"
        foreign.parent.mkdir(parents=True, exist_ok=True)
        foreign.write_bytes((fixture.root / source["path"]).read_bytes())
        fixture.append_evidence_artifact(
            evidence_by_id["main_menu_and_advanced_settings"],
            foreign,
            kind="log",
        )
        fixture.refresh_evidence_manifest(
            evidence_by_id["main_menu_and_advanced_settings"]
        )
        receipt_path = fixture.root / receipt["path"]
        payload = json.loads(receipt_path.read_text(encoding="utf-8"))
        payload["files"]["checksum_stdout"] = {
            "path": foreign.relative_to(fixture.root).as_posix(),
            "sha256": digest(foreign),
        }
        receipt_path.write_text(json.dumps(payload) + "\n", encoding="utf-8")
        fixture.refresh_artifact(evidence, receipt)
        fixture.write_status()
        self.assert_failed(
            fixture.run(), "not attached to Gatekeeper evidence"
        )

        fixture = self.new_fixture()
        fixture.make_all_pass()
        evidence = next(
            item
            for item in fixture.status["evidence"]
            if item["id"] == "gatekeeper_launch"
        )
        transcript = next(
            item
            for item in evidence["artifacts"]
            if Path(item["path"]).name == "spctl.stderr"
        )
        (fixture.root / transcript["path"]).write_text(
            "substituted assessment\n", encoding="utf-8"
        )
        fixture.refresh_artifact(evidence, transcript)
        fixture.write_status()
        self.assert_failed(
            fixture.run(), "raw spctl.stderr does not match the command log"
        )

    def test_v2_clean_mac_raw_plan_intake_and_origin_are_semantic(self):
        fixture = self.new_fixture()
        fixture.make_all_pass()
        fixture.mutate_clean_mac_plist(
            "clean-mac-plan.plist",
            lambda payload: payload.__setitem__(
                "candidate_sha256", "f" * 64
            ),
        )
        fixture.write_status()
        self.assert_failed(fixture.run(), "plan candidate_sha256 is not candidate-bound")

        fixture = self.new_fixture()
        fixture.make_all_pass()
        fixture.mutate_clean_mac_plist(
            "clean-mac-intake.plist",
            lambda payload: payload.__setitem__("test_mode", True),
        )
        fixture.write_status()
        self.assert_failed(
            fixture.run(), "must be complete and must not be test mode"
        )

        fixture = self.new_fixture()
        fixture.make_all_pass()
        evidence, where_artifact, where_path = fixture.clean_mac_artifact(
            "where-froms.hex"
        )
        wrong_origin = plistlib.dumps(
            ["https://github.com/wrong/candidate.zip"],
            fmt=plistlib.FMT_BINARY,
        )
        where_path.write_text(wrong_origin.hex() + "\n", encoding="ascii")
        fixture.refresh_artifact(evidence, where_artifact)
        fixture.mutate_clean_mac_plist(
            "clean-mac-intake.plist",
            lambda payload: payload["acquisition"].__setitem__(
                "where_froms_sha256", digest(where_path)
            ),
        )
        fixture.write_status()
        self.assert_failed(
            fixture.run(), "where-froms does not contain the exact download URL"
        )

        fixture = self.new_fixture()
        fixture.make_all_pass()
        evidence, plan_artifact, plan_path = fixture.clean_mac_artifact(
            "clean-mac-plan.plist"
        )
        plan_path.write_text("not a plist\n", encoding="utf-8")
        fixture.refresh_artifact(evidence, plan_artifact)
        fixture.write_status()
        self.assert_failed(
            fixture.run(), "does not use the canonical Apple plist declaration"
        )

    def test_v2_clean_mac_rejects_untrusted_app_quarantine_agent(self):
        fixture = self.new_fixture()
        fixture.make_all_pass()
        evidence, command, command_path = fixture.clean_mac_artifact(
            "command-log.json"
        )
        quarantine = "0083;5e0cc926;ManualWriter;fixture"
        fixture.status["gatekeeper"]["details"][
            "app_quarantine_output"
        ] = quarantine
        command_payload = json.loads(command_path.read_text(encoding="utf-8"))
        for record in command_payload["commands"]:
            if record["id"] == "app_quarantine":
                record["stdout"] = quarantine
        command_path.write_text(
            json.dumps(command_payload) + "\n", encoding="utf-8"
        )
        fixture.refresh_artifact(evidence, command)
        fixture.mutate_clean_mac_plist(
            "clean-mac-intake.plist",
            lambda payload: payload.__setitem__(
                "commands", command_payload["commands"]
            ),
        )
        fixture.write_status()
        self.assert_failed(
            fixture.run(), "app quarantine agent is not a supported Finder path"
        )

    def test_clean_mac_compiler_output_passes_the_full_verifier(self):
        fixture = self.new_fixture()
        fixture.make_all_pass()
        requirements = fixture.requirements()
        gatekeeper_evidence = next(
            item
            for item in fixture.status["evidence"]
            if item["id"] == "gatekeeper_launch"
        )
        artifact_paths = {
            Path(item["path"]).name: fixture.root / item["path"]
            for item in gatekeeper_evidence["artifacts"]
        }
        media_path = artifact_paths["gatekeeper-launch-recording.mov"]
        intake_dir = fixture.root / "returned-clean-mac-intake"
        intake_dir.mkdir()
        intake_names = [
            "clean-mac-plan.plist",
            "clean-mac-intake.plist",
            "where-froms.hex",
            "checksum.stdout",
            "checksum.stderr",
            "zip-quarantine.stdout",
            "zip-quarantine.stderr",
            "app-quarantine.stdout",
            "app-quarantine.stderr",
            "codesign.stdout",
            "codesign.stderr",
            "spctl.stdout",
            "spctl.stderr",
        ]
        for name in intake_names:
            shutil.copyfile(artifact_paths[name], intake_dir / name)
        (intake_dir / "COMPLETE").write_bytes(b"")

        for gate_name, initial_status in (
            ("clean_mac", "NOT_RUN"),
            ("gatekeeper", "BLOCKED"),
        ):
            gate = fixture.status[gate_name]
            gate["status"] = initial_status
            gate["tester"] = ""
            gate["tested_at_utc"] = None
            gate["evidence_ids"] = []
            gate["checks_confirmed"] = []
            detail_key = "{}_detail_keys".format(gate_name)
            gate["details"] = {key: "" for key in requirements[detail_key]}
            gate["notes"] = "Clean-Mac evidence has not been compiled."
        gatekeeper_evidence["status"] = "NOT_RUN"
        gatekeeper_evidence["artifacts"] = []
        gatekeeper_evidence["reviewer"] = ""
        gatekeeper_evidence["reviewed_at_utc"] = None
        gatekeeper_evidence["interactive"] = {
            "candidate_sha256": "",
            "tester": "",
            "machine": "",
            "tested_at_utc": None,
            "signature": "",
            "coverage_refs": {},
        }
        gatekeeper_evidence["notes"] = "Gatekeeper launch not observed."
        fixture.write_status()

        output_dir = fixture.persistent_evidence_path("clean-mac-compiled")
        command = [
            sys.executable,
            str(CLEAN_MAC_COMPILER),
            "--project-root",
            str(fixture.root.resolve()),
            "--status",
            str(fixture.status_path.resolve()),
            "--intake-dir",
            str(intake_dir.resolve()),
            "--media",
            str(media_path.resolve()),
            "--reviewer",
            "Independent Clean Reviewer",
            "--reviewer-signature",
            "Independent Clean Reviewer",
            "--reviewed-at-utc",
            REVIEW_UTC,
            "--review-notes",
            "Raw Safari, command, and launch evidence reviewed.",
            "--release-note-wording-verified",
            "yes",
            "--output-dir",
            str(output_dir.resolve()),
        ]
        compiled = subprocess.run(
            command,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )
        self.assertEqual(
            compiled.returncode, 0, compiled.stdout + compiled.stderr
        )
        next_status = output_dir / "status.next.json"
        fixture.commit_tree()
        self.assert_failed(
            fixture.run(status_path=next_status),
            "release-ready status must be the canonical",
        )
        shutil.copyfile(next_status, fixture.status_path)
        fixture.commit_tree()
        result = fixture.run()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_v2_clean_mac_requires_continuous_recording_container(self):
        fixture = self.new_fixture()
        fixture.make_all_pass()
        evidence = next(
            item
            for item in fixture.status["evidence"]
            if item["id"] == "gatekeeper_launch"
        )
        recording = next(
            item
            for item in evidence["artifacts"]
            if Path(item["path"]).name.startswith(
                "gatekeeper-launch-recording."
            )
        )
        (fixture.root / recording["path"]).write_bytes(
            b"padded static evidence".ljust(MINIMUM_RECORDING_BYTES, b"0")
        )
        fixture.refresh_artifact(evidence, recording)
        fixture.write_status()
        self.assert_failed(fixture.run(), "is not structurally valid")

    def test_v2_clean_mac_rejects_url_checksum_and_quarantine_drift(self):
        fixture = self.new_fixture()
        fixture.make_all_pass()
        fixture.set_clean_mac_url(
            fixture.status["clean_mac"]["details"]["download_url"]
            + "?token=secret"
        )
        fixture.write_status()
        self.assert_failed(fixture.run(), "non-test HTTPS candidate URL")

        fixture = self.new_fixture()
        fixture.make_all_pass()
        evidence = next(
            item
            for item in fixture.status["evidence"]
            if item["id"] == "gatekeeper_launch"
        )
        command = next(
            item
            for item in evidence["artifacts"]
            if item["path"].endswith("command-log.json")
        )
        command_path = fixture.root / command["path"]
        payload = json.loads(command_path.read_text(encoding="utf-8"))
        payload["commands"][0]["stdout"] += "trailing text\n"
        command_path.write_text(json.dumps(payload) + "\n", encoding="utf-8")
        fixture.refresh_artifact(evidence, command)
        fixture.write_status()
        self.assert_failed(fixture.run(), "does not exactly bind the candidate")

        for quarantine, expected in (
            (
                "0083;not-hex;Safari;fixture",
                "ZIP quarantine output is not canonical",
            ),
            (
                "0083;00000001;Safari;fixture",
                "lies outside the Safari acquisition",
            ),
        ):
            with self.subTest(quarantine=quarantine):
                fixture = self.new_fixture()
                fixture.make_all_pass()
                evidence = next(
                    item
                    for item in fixture.status["evidence"]
                    if item["id"] == "gatekeeper_launch"
                )
                fixture.status["gatekeeper"]["details"][
                    "zip_quarantine_output"
                ] = quarantine
                command = next(
                    item
                    for item in evidence["artifacts"]
                    if item["path"].endswith("command-log.json")
                )
                command_path = fixture.root / command["path"]
                payload = json.loads(
                    command_path.read_text(encoding="utf-8")
                )
                for record in payload["commands"]:
                    if record["id"] == "zip_quarantine":
                        record["stdout"] = quarantine
                command_path.write_text(
                    json.dumps(payload) + "\n", encoding="utf-8"
                )
                fixture.refresh_artifact(evidence, command)
                if quarantine.startswith("0083;00000001;"):
                    fixture.mutate_clean_mac_plist(
                        "clean-mac-intake.plist",
                        lambda payload: payload["acquisition"].__setitem__(
                            "quarantine_timestamp_utc",
                            "1970-01-01T00:00:01Z",
                        ),
                    )
                fixture.write_status()
                self.assert_failed(fixture.run(), expected)

    def test_command_log_rejects_test_url_wrong_args_and_false_spctl_outcome(self):
        fixture = self.new_fixture()
        fixture.make_all_pass()
        artifact_name = fixture.status["clean_mac"]["details"]["downloaded_artifact_filename"]
        fixture.set_clean_mac_url("https://example.com/" + artifact_name)
        fixture.write_status()
        self.assert_failed(fixture.run(), "non-test HTTPS candidate URL")

        fixture = self.new_fixture()
        fixture.make_all_pass()
        fixture.set_clean_mac_url(
            "https://download.example.com/" + artifact_name
        )
        fixture.write_status()
        self.assert_failed(fixture.run(), "non-test HTTPS candidate URL")

        for host in (
            "download.example.com.",
            "github.com..",
            "localhost.",
            "127.1",
            "2130706433",
            "0x7f000001",
            "download。example。com",
            "not a host",
            "github..com",
            "-github.com",
            "github-.com",
            "github_com",
            "github",
            "github.com:abc",
            "github.com:99999",
            "github.com:444",
            "release.local",
            "release.internal",
            "release.lan",
            "release.onion",
            "fixture.github.com",
            "placeholder.github.com",
        ):
            with self.subTest(forbidden_download_host=host):
                fixture = self.new_fixture()
                fixture.make_all_pass()
                fixture.set_clean_mac_url(
                    "https://{}/{}".format(host, artifact_name)
                )
                fixture.write_status()
                self.assert_failed(
                    fixture.run(), "non-test HTTPS candidate URL"
                )

        fixture = self.new_fixture()
        fixture.make_all_pass()
        fixture.status["clean_mac"]["details"]["download_client"] = "curl"
        fixture.write_status()
        self.assert_failed(
            fixture.run(), "download_client must be Safari"
        )

        fixture = self.new_fixture()
        fixture.make_all_pass()
        evidence = fixture.status["evidence"][6]
        command = next(
            item
            for item in evidence["artifacts"]
            if item["path"].endswith("command-log.json")
        )
        path = fixture.root / command["path"]
        payload = json.loads(path.read_text(encoding="utf-8"))
        payload["commands"].insert(
            0,
            {
                "id": "download",
                "argv": [
                    "curl",
                    "--fail",
                    "--location",
                    "--output",
                    artifact_name,
                    fixture.status["clean_mac"]["details"]["download_url"],
                ],
                "exit_code": 0,
                "stdout": "downloaded {}".format(artifact_name),
                "stderr": "",
                "started_at_utc": "2020-01-01T16:29:00Z",
                "completed_at_utc": "2020-01-01T16:30:00Z",
            },
        )
        path.write_text(json.dumps(payload) + "\n", encoding="utf-8")
        fixture.refresh_artifact(evidence, command)
        fixture.write_status()
        self.assert_failed(
            fixture.run(), "must contain the 5 canonical commands"
        )

        fixture = self.new_fixture()
        fixture.make_all_pass()
        evidence = fixture.status["evidence"][6]
        command = next(
            item
            for item in evidence["artifacts"]
            if item["path"].endswith("command-log.json")
        )
        path = fixture.root / command["path"]
        payload = json.loads(path.read_text(encoding="utf-8"))
        payload["commands"][0], payload["commands"][1] = (
            payload["commands"][1],
            payload["commands"][0],
        )
        path.write_text(json.dumps(payload) + "\n", encoding="utf-8")
        fixture.refresh_artifact(evidence, command)
        fixture.write_status()
        self.assert_failed(fixture.run(), "command order is not canonical")

        fixture = self.new_fixture()
        fixture.make_all_pass()
        evidence = fixture.status["evidence"][6]
        acquisition = next(
            item
            for item in evidence["artifacts"]
            if item["path"].endswith("browser-acquisition.json")
        )
        evidence["artifacts"].remove(acquisition)
        fixture.refresh_evidence_manifest(evidence)
        fixture.write_status()
        self.assert_failed(
            fixture.run(), "requires exactly one structured browser acquisition"
        )

        fixture = self.new_fixture()
        fixture.make_all_pass()
        evidence = fixture.status["evidence"][6]
        acquisition = next(
            item
            for item in evidence["artifacts"]
            if item["path"].endswith("browser-acquisition.json")
        )
        path = fixture.root / acquisition["path"]
        payload = json.loads(path.read_text(encoding="utf-8"))
        payload["completed_at_utc"] = "2020-01-01T16:33:00Z"
        path.write_text(json.dumps(payload) + "\n", encoding="utf-8")
        fixture.refresh_artifact(evidence, acquisition)
        fixture.write_status()
        self.assert_failed(
            fixture.run(), "must complete before the checksum command"
        )

        fixture = self.new_fixture()
        fixture.make_all_pass()
        gate = fixture.status["gatekeeper"]["details"]
        gate["zip_quarantine_output"] = (
            "0083;5e0cc926;ManualWriter;fixture"
        )
        evidence = fixture.status["evidence"][6]
        command = next(
            item
            for item in evidence["artifacts"]
            if item["path"].endswith("command-log.json")
        )
        path = fixture.root / command["path"]
        payload = json.loads(path.read_text(encoding="utf-8"))
        for record in payload["commands"]:
            if record["id"] == "zip_quarantine":
                record["stdout"] = gate["zip_quarantine_output"]
        path.write_text(json.dumps(payload) + "\n", encoding="utf-8")
        fixture.refresh_artifact(evidence, command)
        fixture.write_status()
        self.assert_failed(
            fixture.run(), "ZIP quarantine agent does not match the Safari acquisition"
        )

        fixture = self.new_fixture()
        fixture.make_all_pass()
        fixture.status["gatekeeper"]["details"]["codesign_command"] = (
            "codesign --verify Tanks3D.app"
        )
        fixture.write_status()
        self.assert_failed(fixture.run(), "exact codesign arguments")

        fixture = self.new_fixture()
        fixture.make_all_pass()
        evidence = fixture.status["evidence"][6]
        command = next(item for item in evidence["artifacts"] if item["path"].endswith("command-log.json"))
        path = fixture.root / command["path"]
        payload = json.loads(path.read_text(encoding="utf-8"))
        payload["commands"][-1]["stderr"] = "assessment unavailable"
        path.write_text(json.dumps(payload) + "\n", encoding="utf-8")
        fixture.refresh_artifact(evidence, command)
        fixture.write_status()
        self.assert_failed(
            fixture.run(), "must record exactly one assessment outcome"
        )

        fixture = self.new_fixture()
        fixture.make_all_pass()
        evidence = fixture.status["evidence"][6]
        command = next(
            item
            for item in evidence["artifacts"]
            if item["path"].endswith("command-log.json")
        )
        path = fixture.root / command["path"]
        payload = json.loads(path.read_text(encoding="utf-8"))
        for index, record in enumerate(payload["commands"]):
            minute = 35 - index
            record["started_at_utc"] = "2020-01-01T16:{:02d}:00Z".format(
                minute
            )
            record["completed_at_utc"] = "2020-01-01T16:{:02d}:00Z".format(
                minute + 1
            )
        path.write_text(json.dumps(payload) + "\n", encoding="utf-8")
        fixture.refresh_artifact(evidence, command)
        fixture.write_status()
        self.assert_failed(
            fixture.run(), "do not follow canonical command order"
        )

        fixture = self.new_fixture()
        fixture.make_all_pass()
        evidence = fixture.status["evidence"][6]
        command = next(
            item
            for item in evidence["artifacts"]
            if item["path"].endswith("command-log.json")
        )
        path = fixture.root / command["path"]
        payload = json.loads(path.read_text(encoding="utf-8"))
        for record in payload["commands"]:
            record["started_at_utc"] = "2019-12-31T16:30:00Z"
            record["completed_at_utc"] = "2019-12-31T17:00:00Z"
        path.write_text(json.dumps(payload) + "\n", encoding="utf-8")
        fixture.refresh_artifact(evidence, command)
        fixture.write_status()
        self.assert_failed(
            fixture.run(), "outside the Gatekeeper interactive session"
        )

    def test_raw_performance_log_and_fixed_thresholds_are_enforced(self):
        fixture = self.new_fixture()
        fixture.make_all_pass()
        fixture.status["extended_session"]["details"]["minimum_average_fps"] = "1"
        fixture.write_status()
        self.assert_failed(fixture.run(), "fixed Alpha threshold")

        fixture = self.new_fixture()
        fixture.make_all_pass()
        evidence = fixture.status["evidence"][7]
        performance = next(
            item
            for item in evidence["artifacts"]
            if item["path"].endswith("performance-log-v2.json")
        )
        path = fixture.root / performance["path"]
        payload = json.loads(path.read_text(encoding="utf-8"))
        payload["samples"] = payload["samples"][:5]
        path.write_text(json.dumps(payload) + "\n", encoding="utf-8")
        fixture.refresh_artifact(evidence, performance)
        fixture.write_status()
        self.assert_failed(fixture.run(), "insufficient raw sampling coverage")

        fixture = self.new_fixture()
        fixture.make_all_pass()
        evidence = fixture.status["evidence"][7]
        performance = next(
            item
            for item in evidence["artifacts"]
            if item["path"].endswith("performance-log-v2.json")
        )
        path = fixture.root / performance["path"]
        payload = json.loads(path.read_text(encoding="utf-8"))
        payload["started_at_utc"] = "2019-12-31T16:30:00Z"
        payload["completed_at_utc"] = "2019-12-31T17:00:00Z"
        path.write_text(json.dumps(payload) + "\n", encoding="utf-8")
        fixture.refresh_artifact(evidence, performance)
        fixture.write_status()
        self.assert_failed(
            fixture.run(), "interval does not match its interactive session"
        )

    def test_ready_v1_profile_is_explicitly_superseded(self):
        fixture = self.new_fixture()
        fixture.make_all_pass(profile="v1")
        evidence = next(
            item
            for item in fixture.status["evidence"]
            if item["id"] == "one_player_gameplay"
        )
        event = next(
            item
            for item in evidence["artifacts"]
            if item["path"].endswith("-events.json")
        )
        payload = json.loads(
            (fixture.root / event["path"]).read_text(encoding="utf-8")
        )
        self.assertEqual(payload["schema"], "tanks3d-gameplay-event-log-v1")
        self.assertEqual(payload["producer"], "Tanks3D")
        self.assertNotIn("observation_manifest_sha256", payload)
        self.assert_failed(fixture.run(), "macos-alpha-v1 is superseded")

    def test_v2_samples_reject_raw_metric_interval_and_continuity_tampering(self):
        mutations = [
            (
                lambda payload: payload["samples"][0].__setitem__(
                    "resident_bytes", 220.5
                ),
                "must be a raw JSON integer",
            ),
            (
                lambda payload: payload["samples"][0].__setitem__(
                    "window_duration_us", 500000
                ),
                "outside 0.75-1.25 seconds",
            ),
            (
                lambda payload: payload["samples"].pop(100),
                "sample sequence must be contiguous",
            ),
            (
                lambda payload: [
                    sample.__setitem__("rendered_frames", 40)
                    for sample in payload["samples"][:901]
                ],
                "average_fps contradicts raw v2 samples",
            ),
            (
                lambda payload: payload["samples"][100].__setitem__(
                    "resident_bytes", 500 * 1048576
                ),
                "memory-growth limit",
            ),
            (
                lambda payload: payload["samples"][0].__setitem__(
                    "app_state", "paused"
                ),
                "sample app_state is not canonical",
            ),
        ]
        for mutation, expected in mutations:
            with self.subTest(expected=expected):
                fixture = self.new_fixture()
                fixture.make_all_pass()
                fixture.mutate_performance_json(
                    "performance-log-v2.json", mutation
                )
                fixture.write_status()
                self.assert_failed(fixture.run(), expected)

    def test_v2_samples_prove_gameplay_focus_stage_completion_and_mode_mix(self):
        mutations = [
            (
                lambda payload: [
                    sample.update(
                        {"gameplay_duration_us": 0, "app_state": "settlement"}
                    )
                    for sample in payload["samples"]
                ],
                "enough active gameplay",
            ),
            (
                lambda payload: [
                    sample.update(
                        {"focused_duration_us": 0, "window_focused": False}
                    )
                    for sample in payload["samples"][:100]
                ],
                "enough focused-window time",
            ),
            (
                lambda payload: [
                    (
                        sample.__setitem__("completed_stages", 0),
                        sample.__setitem__("stage_clear_events", 0),
                    )
                    for sample in payload["samples"]
                ],
                "do not prove a completed stage",
            ),
            (
                lambda payload: payload["samples"][1000].__setitem__(
                    "completed_stages", 0
                ),
                "completed_stages contradicts its clear events",
            ),
            (
                lambda payload: payload["samples"][0].__setitem__(
                    "gameplay_duration_us", 1000001
                ),
                "beyond its sample window",
            ),
            (
                lambda payload: [
                    (
                        sample.__setitem__("app_state", "settlement"),
                        sample.__setitem__(
                            "gameplay_duration_us",
                            sample["window_duration_us"],
                        ),
                    )
                    for sample in payload["samples"]
                ],
                "non-gameplay state contradicts a full gameplay window",
            ),
            (
                lambda payload: payload["samples"][-1].__setitem__(
                    "completed_stages", 3
                ),
                "completed_stages contradicts its clear events",
            ),
            (
                lambda payload: payload["samples"][899].__setitem__(
                    "stage_clear_events", 2
                ),
                "more than one cleared stage",
            ),
            (
                lambda payload: [
                    sample.__setitem__("player_count", 2)
                    for sample in payload["samples"]
                ],
                "requires one-player samples",
            ),
            (
                lambda payload: payload["samples"][0].update(
                    {"window_focused": False, "focused_duration_us": 1000000}
                ),
                "unfocused endpoint contradicts a fully focused window",
            ),
        ]
        for mutation, expected in mutations:
            with self.subTest(expected=expected):
                fixture = self.new_fixture()
                fixture.make_all_pass()
                fixture.mutate_performance_json(
                    "performance-log-v2.json", mutation
                )
                fixture.write_status()
                self.assert_failed(fixture.run(), expected)

        fixture = self.new_fixture()
        fixture.make_all_pass()
        fixture.status["extended_session"]["details"]["stages_completed"] = "1"
        fixture.write_status()
        self.assert_failed(
            fixture.run(), "stages_completed contradicts raw v2 samples"
        )

        fixture = self.new_fixture()
        fixture.make_all_pass()
        fixture.status["extended_session"]["details"]["mode_mix"] = "two-player"
        fixture.write_status()
        self.assert_failed(fixture.run(), "mode_mix contradicts raw v2 samples")

    def test_v2_telemetry_identity_and_shutdown_are_candidate_bound(self):
        mutations = [
            (
                lambda payload: payload.__setitem__("source_commit", "b" * 40),
                "source_commit does not match its candidate receipt",
            ),
            (
                lambda payload: payload.__setitem__("source_tag", "v9.9.9"),
                "source_tag does not match its candidate receipt",
            ),
            (
                lambda payload: payload.__setitem__("session_nonce", "2" * 32),
                "session_nonce does not match its candidate receipt",
            ),
            (
                lambda payload: payload.__setitem__("clean_shutdown", False),
                "requires clean_shutdown=true",
            ),
        ]
        for mutation, expected in mutations:
            with self.subTest(expected=expected):
                fixture = self.new_fixture()
                fixture.make_all_pass()
                fixture.mutate_performance_json(
                    "performance-log-v2.json", mutation
                )
                fixture.write_status()
                self.assert_failed(fixture.run(), expected)

    def test_v2_receipt_references_and_executable_hash_are_enforced(self):
        mutations = [
            (
                lambda payload: payload["telemetry"].__setitem__(
                    "sha256", "0" * 64
                ),
                "hash contradicts its evidence artifact",
            ),
            (
                lambda payload: payload["telemetry"].__setitem__(
                    "path", "../performance-log-v2.json"
                ),
                "canonical filename",
            ),
            (
                lambda payload: payload.__setitem__("executable_sha256", "0" * 64),
                "executable hash does not match the candidate ZIP",
            ),
            (
                lambda payload: payload.__setitem__(
                    "started_at_utc", "2020-01-01T16:30:01Z"
                ),
                "timestamps are not nested chronologically",
            ),
        ]
        for mutation, expected in mutations:
            with self.subTest(expected=expected):
                fixture = self.new_fixture()
                fixture.make_all_pass()
                fixture.mutate_performance_json(
                    "performance-qa-receipt.json", mutation
                )
                fixture.write_status()
                self.assert_failed(fixture.run(), expected)

    def test_v2_runner_argv_and_stdout_markers_are_exact(self):
        fixture = self.new_fixture()
        fixture.make_all_pass()
        fixture.mutate_performance_json(
            "performance-qa-receipt.json",
            lambda payload: payload["argv"].append("--release-screenshot"),
        )
        fixture.write_status()
        self.assert_failed(fixture.run(), "runner's six exact arguments")

        fixture = self.new_fixture()
        fixture.make_all_pass()
        fixture.mutate_performance_json(
            "performance-qa-receipt.json",
            lambda payload: payload["argv"].__setitem__(1, "--showcase"),
        )
        fixture.write_status()
        self.assert_failed(fixture.run(), "start the candidate with --quick-start")

        fixture = self.new_fixture()
        fixture.make_all_pass()
        fixture.mutate_performance_json(
            "performance-qa-receipt.json",
            lambda payload: payload["argv"].__setitem__(
                2,
                "--release-performance-log=/private/tmp/evidence/../"
                "performance-log-v2.json",
            ),
        )
        fixture.write_status()
        self.assert_failed(fixture.run(), "telemetry path flag is not canonical")

        fixture = self.new_fixture()
        fixture.make_all_pass()
        fixture.mutate_performance_json(
            "performance-qa-receipt.json",
            lambda payload: payload["argv"].__setitem__(
                3, "--release-candidate-sha256={}".format("0" * 64)
            ),
        )
        fixture.write_status()
        self.assert_failed(fixture.run(), "candidate or nonce flag is not exact")

        fixture = self.new_fixture()
        fixture.make_all_pass()
        fixture.mutate_performance_json(
            "performance-qa-receipt.json",
            lambda payload: payload["argv"].__setitem__(
                5, "--release-performance-duration-seconds=14401"
            ),
        )
        fixture.write_status()
        self.assert_failed(fixture.run(), "duration flag is outside the release range")

        fixture = self.new_fixture()
        fixture.make_all_pass()
        evidence, artifact, path = fixture.performance_artifact(
            "performance-stdout.log"
        )
        path.write_text(
            path.read_text(encoding="utf-8")
            + "TANKS3D_PERFORMANCE_START {}\n".format(PERFORMANCE_NONCE),
            encoding="utf-8",
        )
        fixture.refresh_artifact(evidence, artifact)
        fixture.write_status()
        self.assert_failed(fixture.run(), "exactly one ordered matching START/COMPLETE")

    def test_v2_performance_artifact_limits_run_before_hashing(self):
        limits = {
            "performance-qa-receipt.json": 64 * 1024,
            "performance-log-v2.json": 32 * 1024 * 1024,
            "performance-stdout.log": 16 * 1024 * 1024,
            "performance-stderr.log": 0,
        }
        for filename, maximum in limits.items():
            with self.subTest(filename=filename):
                fixture = self.new_fixture()
                fixture.make_all_pass()
                _, _, path = fixture.performance_artifact(filename)
                with path.open("wb") as stream:
                    stream.truncate(maximum + 1)
                fixture.write_status()
                result = fixture.run()
                self.assert_failed(
                    result,
                    "exceeds its {}-byte release limit".format(maximum),
                )
                self.assertNotIn("hash mismatch", result.stderr)

    def test_v2_resigned_nonempty_stderr_is_still_rejected(self):
        fixture = self.new_fixture()
        fixture.make_all_pass()
        evidence, artifact, path = fixture.performance_artifact(
            "performance-stderr.log"
        )
        path.write_bytes(b"warning hidden behind refreshed hashes\n")
        fixture.refresh_artifact(evidence, artifact)
        fixture.write_status()
        self.assert_failed(
            fixture.run(),
            "exceeds its 0-byte release limit",
        )

    def test_renamed_large_performance_log_is_bounded_before_hashing(self):
        fixture = self.new_fixture()
        fixture.make_all_pass()
        evidence = next(
            item
            for item in fixture.status["evidence"]
            if item["id"] == "extended_session_metrics"
        )
        path = fixture.persistent_evidence_path(
            "renamed-performance-output.log"
        )
        with path.open("wb") as stream:
            stream.truncate(32 * 1024 * 1024 + 1)
        fixture.append_evidence_artifact(evidence, path, kind="log")
        fixture.write_status()
        result = fixture.run()
        self.assert_failed(result, "exceeds its 33554432-byte release limit")
        self.assertNotIn("hash mismatch", result.stderr)

    def test_both_ready_documents_reject_contradictory_gate_status(self):
        contradictions = [
            (
                "release_page",
                "Clean Mac validation and Gatekeeper assessment are still "
                "outstanding.\n",
            ),
            (
                "qa_report",
                "Hardware audio has never been listened to, and performance "
                "verification is missing.\n",
            ),
            ("release_page", "Clean Mac validation is incomplete.\n"),
            ("qa_report", "Audio verification has yet to occur.\n"),
        ]
        for document_key, contradiction in contradictions:
            fixture = self.new_fixture()
            fixture.make_all_pass()
            reference = fixture.status["documents"][document_key]
            path = fixture.root / reference["path"]
            path.write_text(
                path.read_text(encoding="utf-8") + contradiction,
                encoding="utf-8",
            )
            reference["sha256"] = digest(path)
            fixture.write_status()
            self.assert_failed(
                fixture.run(), "contradictory release-status assertion"
            )

    def test_audio_confirm_requires_an_externally_trusted_signature_profile(self):
        fixture = self.new_fixture()
        fixture.make_all_pass()
        requirements = fixture.requirements()
        audio = fixture.status["audio"]
        independent = audio["evidence"][-1]
        confirmation_path = fixture.persistent_evidence_path(
            "rights-confirmation.json"
        )
        confirmation_path.write_text(
            json.dumps(
                {
                    "schema": "self-asserted-rights-confirmation",
                    "candidate_sha256": fixture.status["release"]["artifact"]["sha256"],
                    "rights_holder_name": "Fixture Rights Holder",
                    "rights_holder_authority": "Authorized audio licensor",
                    "permission_scope": "Noncommercial Alpha distribution of the 22 audio cues",
                    "confirmation_method": "Signed email retained as hashed evidence",
                    "confirmed_at_utc": REVIEW_UTC,
                    "expires_at_utc": None,
                    "signature": "Fixture Rights Holder",
                    "evidence_sha256s": [independent["sha256"]],
                },
                indent=2,
            ) + "\n",
            encoding="utf-8",
        )
        audio["evidence"].append(
            {
                "path": confirmation_path.relative_to(fixture.root).as_posix(),
                "sha256": digest(confirmation_path),
                "kind": "report",
            }
        )
        audio["decision"] = "CONFIRM"
        audio["authority"] = "Authorized audio licensor"
        audio["checks_confirmed"] = requirements["audio_decision_checks"][1]["checks"]
        for document_key in ("release_page", "qa_report"):
            reference = fixture.status["documents"][document_key]
            path = fixture.root / reference["path"]
            path.write_text(path.read_text(encoding="utf-8").replace("ACCEPT", "CONFIRM"), encoding="utf-8")
            reference["sha256"] = digest(path)
        fixture.write_status()
        self.assert_failed(
            fixture.run(),
            "cannot approve under the v1 profile without an externally trusted",
        )

    def test_future_and_ordered_approval_timestamps_are_rejected(self):
        fixture = self.new_fixture()
        fixture.make_all_pass()
        fixture.status["report"]["completed_at_utc"] = "2099-01-01T00:00:00Z"
        fixture.write_status()
        self.assert_failed(fixture.run(), "must not be in the future")

        fixture = self.new_fixture()
        fixture.make_all_pass()
        fixture.status["approvals"][1]["approved_at_utc"] = QA_APPROVAL_UTC
        fixture.write_status()
        self.assert_failed(fixture.run(), "must follow QA approval")

        fixture = self.new_fixture()
        fixture.make_all_pass()
        fixture.status["approvals"][1]["name"] = "QA Lead"
        fixture.status["approvals"][1]["signature"] = "QA Lead"
        fixture.status["audio"]["owner"] = "QA Lead"
        fixture.status["audio"]["signature"] = "QA Lead"
        fixture.write_status()
        self.assert_failed(
            fixture.run(), "qa_lead and release_owner approvals must be independent"
        )


if __name__ == "__main__":
    unittest.main()
