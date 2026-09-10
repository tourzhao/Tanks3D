#!/usr/bin/env python3
"""Read an immutable alpha.5 candidate; write an unsigned audio inventory.

This helper never builds, extracts, modifies the candidate, or records approval.
Run only after the candidate builder has finished publishing its directory.
"""

import argparse
import datetime
import hashlib
import io
import json
from pathlib import Path
import re
import subprocess
import sys
import zipfile


TAG = "v0.1.0-alpha.5"
COMMIT = "cc5f2a7eec073accf37c2085c10bf155061acef2"
SOUND_PREFIX = "Tanks3D.app/Contents/Resources/sounds/"
NOTICE_PREFIX = "Tanks3D.app/Contents/Resources/licenses/"
MAX_ARCHIVE_BYTES = 512 * 1024 * 1024


def require(condition, message):
    if not condition:
        raise ValueError(message)


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def read_regular(path, maximum=MAX_ARCHIVE_BYTES):
    require(path.is_file() and not path.is_symlink(), "Not a regular file: " + str(path))
    require(path.stat().st_size <= maximum, "Unexpected file size: " + str(path))
    data = path.read_bytes()
    require(len(data) <= maximum, "Unexpected file size: " + str(path))
    return data


def git_bytes(root, *args):
    return subprocess.run(
        ["git", "-C", str(root), *args], check=True, stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    ).stdout


def source_blob(root, relative):
    return git_bytes(root, "show", COMMIT + ":" + relative)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--output-dir", type=Path,
        default=Path(__file__).resolve().parent / "audio-review",
        help="New directory for inventory.json and review.md (must not exist).",
    )
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[3]
    candidate = root / "build" / "release" / TAG
    output = args.output_dir.absolute()
    evidence_root = root / "build" / "release-evidence" / TAG
    require(output.resolve().is_relative_to(evidence_root.resolve()),
            "Output must remain under this candidate's release-evidence directory")
    require(not output.exists() and not output.is_symlink(),
            "Output already exists; keep prior evidence immutable: " + str(output))
    require(candidate.is_dir() and not candidate.is_symlink(),
            "Finished immutable candidate is not present: " + str(candidate))

    # Reuse the committed release validator's audio contract, without invoking
    # the build/tagged verifier or creating an official release status.
    sys.dont_write_bytecode = True
    sys.path.insert(0, str(root / "scripts"))
    import verify_release_status as gate

    require(read_regular(root / "scripts/verify_release_status.py") ==
            source_blob(root, "scripts/verify_release_status.py"),
            "Release verifier differs from the pinned source snapshot")
    require(git_bytes(root, "rev-parse", "refs/tags/" + TAG + "^{commit}").decode().strip()
            == COMMIT, "Tag no longer resolves to the pinned candidate source")
    attestation_path = candidate / "attestation.txt"
    attestation_bytes = read_regular(attestation_path, 1024 * 1024)
    attestation = gate.parse_attestation(attestation_path)
    expected = {
        "schema": "tanks3d-alpha-candidate-v3",
        "source_commit": COMMIT,
        "source_head_at_start": COMMIT,
        "source_head_at_finish": COMMIT,
        "source_tag": TAG,
        "source_tag_commit": COMMIT,
        "source_tree": "clean",
        "app_version": "0.1.0",
        "dist_channel": "alpha.5",
        "dist_arch": "arm64",
        "dist_macos_min": "26.0",
        "build_config_filename": "build-config.txt",
        "gate_log_filename": "alpha-candidate-gates.log",
    }
    for key, value in expected.items():
        require(attestation.get(key) == value, "Unexpected attestation " + key)
    for key in (
        "gate_clean", "gate_test_alpha_candidate", "gate_debug",
        "gate_test_architecture", "gate_test", "gate_test_sanitize",
        "gate_coverage", "gate_test_dist",
    ):
        require(attestation.get(key) == "PASS", "Candidate machine gate incomplete: " + key)

    archive_name = "Tanks3D-0.1.0-alpha.5-macos-arm64-macos26.0.zip"
    require(attestation.get("artifact_filename") == archive_name,
            "Unexpected candidate archive filename")
    require(attestation.get("checksum_filename") == archive_name + ".sha256",
            "Unexpected candidate checksum filename")
    archive = candidate / archive_name
    # One byte snapshot binds the inventory to precisely the ZIP digest below.
    archive_bytes = read_regular(archive)
    archive_hash = sha256(archive_bytes)
    require(archive_hash == attestation.get("artifact_sha256"),
            "Candidate ZIP SHA-256 differs from its attestation")
    checksum_bytes = read_regular(candidate / (archive_name + ".sha256"), 4096)
    require(checksum_bytes.decode("utf-8").strip() == archive_hash + "  " + archive_name,
            "Candidate checksum does not match the attested ZIP")
    bound_files = {}
    for field in ("build_config", "gate_log"):
        name = attestation[field + "_filename"]
        data = read_regular(candidate / name, 32 * 1024 * 1024)
        require(sha256(data) == attestation[field + "_sha256"],
                "Attested auxiliary file changed: " + name)
        bound_files[field] = {"path": str((candidate / name).relative_to(root)),
                              "sha256": sha256(data), "size_bytes": len(data)}
    gate.validate_current_v2_candidate_contract(
        attestation_path, candidate / "build-config.txt")
    gate.verify_candidate_inherited_audio(root, archive)

    canonical_notices = gate.REPOSITORY_AUDIO_NOTICES
    require(len(canonical_notices) == 4 and
            "LICENSES/MIT-JustoSenka-BattleCity.txt" in canonical_notices,
            "Unexpected repository audio-notice contract")
    source_doc = source_blob(root, "ASSET_LICENSES.md").decode("utf-8")
    retained_commit = "f59aea31638117e20bc03276026bdbb9f8828b47"
    musical_commit = "3a07004ba8e53baea74ff70d2ecc22b017eb9b20"
    for claim in (
        "krystiankaluzny/Tanks@" + retained_commit,
        "JustoSenka/BattleCity@" + musical_commit,
        "Twenty files retain", "Redas Jefisovas", "Justas Glodenis",
    ):
        require(claim in source_doc, "Pinned provenance text changed: " + claim)
    replacement_rows = re.findall(
        r"^\| `(?P<upstream>Assets/Audio/[^`]+\.ogg)` \| "
        r"`(?P<local>resources/sounds/[^`]+\.ogg)` \| "
        r"(?P<duration>[0-9.]+) s \| `(?P<digest>[0-9a-f]{64})` \|$",
        source_doc, re.MULTILINE,
    )
    replacements = {local: {"upstream_path": upstream,
                            "documented_duration_seconds": float(duration),
                            "documented_sha256": digest}
                    for upstream, local, duration, digest in replacement_rows}
    require(set(replacements) == {"resources/sounds/stage_start_up.ogg",
                                 "resources/sounds/game_over.ogg"},
            "Expected exactly the two documented replacement cue mappings")
    source_paths = git_bytes(root, "ls-tree", "-r", "--name-only", COMMIT,
                             "--", "resources/sounds").decode().splitlines()
    source_paths = sorted(path for path in source_paths if path.endswith(".ogg"))
    require(len(source_paths) == 22, "Pinned source must contain exactly 22 OGG files")
    audio_rows = []
    notice_rows = []
    with zipfile.ZipFile(io.BytesIO(archive_bytes)) as bundle:
        infos = bundle.infolist()
        names = [info.filename for info in infos]
        require(len(names) == len(set(names)), "Duplicate ZIP archive paths")
        audio_names = sorted(name for name in names
                             if name.startswith(SOUND_PREFIX) and name.endswith(".ogg"))
        expected_audio = [SOUND_PREFIX + Path(path).name for path in source_paths]
        require(audio_names == sorted(expected_audio), "Unexpected candidate OGG set")
        for local in source_paths:
            archive_path = SOUND_PREFIX + Path(local).name
            require(bundle.getinfo(archive_path).file_size <= 20 * 1024 * 1024,
                    "Audio member is unexpectedly large: " + archive_path)
            data = bundle.read(archive_path)
            require(data == source_blob(root, local) == read_regular(root / local),
                    "Audio differs between candidate, pinned source and checkout: " + local)
            replacement = replacements.get(local)
            if replacement:
                require(sha256(data) == replacement["documented_sha256"],
                        "Replacement cue does not match its documented digest: " + local)
            row = {
                "archive_path": archive_path, "repository_path": local,
                "sha256": sha256(data), "size_bytes": len(data),
                "source_id": "justosenka-battlecity" if replacement else "krystiankaluzny-tanks",
                "upstream_path": replacement["upstream_path"] if replacement else local,
                "matches_pinned_source_and_checkout": True,
            }
            if replacement:
                row["documented_duration_seconds"] = replacement["documented_duration_seconds"]
            audio_rows.append(row)
        for local in canonical_notices:
            archive_path = NOTICE_PREFIX + local
            require(bundle.getinfo(archive_path).file_size <= 2 * 1024 * 1024,
                    "Notice member is unexpectedly large: " + archive_path)
            data = bundle.read(archive_path)
            require(data == source_blob(root, local) == read_regular(root / local),
                    "Notice differs between candidate, pinned source and checkout: " + local)
            notice_rows.append({"repository_path": local, "archive_path": archive_path,
                                "sha256": sha256(data), "size_bytes": len(data),
                                "byte_identical_to_pinned_source_and_checkout": True})
    require(read_regular(attestation_path, 1024 * 1024) == attestation_bytes,
            "Attestation changed during inspection")

    sources = [
        {"id": "krystiankaluzny-tanks", "repository": "krystiankaluzny/Tanks",
         "commit": retained_commit, "audio_file_count": 20,
         "url": "https://github.com/krystiankaluzny/Tanks/tree/" + retained_commit + "/resources/sounds",
         "repository_notice": "LICENSES/MIT-upstream.txt",
         "repository_provenance_claim": "Twenty unchanged, same-name OGG files; history credits Redas Jefisovas (holoflash) with the effect-sound set and refinements; OGG conversion through upstream pull request 35."},
        {"id": "justosenka-battlecity", "repository": "JustoSenka/BattleCity",
         "commit": musical_commit, "audio_file_count": 2,
         "url": "https://github.com/JustoSenka/BattleCity/tree/" + musical_commit + "/Assets/Audio",
         "repository_notice": "LICENSES/MIT-JustoSenka-BattleCity.txt",
         "repository_provenance_claim": "Two unchanged, renamed musical cues; repository declares MIT, copyright 2019 Justas Glodenis; 48,000 Hz mono; no trimming, resampling or transcoding."},
    ]
    for source in sources:
        require(sum(row["source_id"] == source["id"] for row in audio_rows)
                == source["audio_file_count"], "Source file count mismatch")
    packet = {
        "schema": "tanks3d-candidate-audio-inventory-v1",
        "generated_at_utc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
        "purpose": "Mechanical candidate inventory and repository provenance claims for owner review; not release approval or a rights determination.",
        "candidate": {"tag": TAG, "source_commit": COMMIT,
                      "archive_path": str(archive.relative_to(root)),
                      "archive_sha256": archive_hash, "archive_size_bytes": len(archive_bytes),
                      "attestation_path": str(attestation_path.relative_to(root)),
                      "attestation_sha256": sha256(attestation_bytes),
                      "checksum_sha256": sha256(checksum_bytes), **bound_files},
        "provenance_document": "ASSET_LICENSES.md#runtime-audio",
        "sources": sources,
        "scope_limit": "Records repository provenance and license declarations only. No independent confirmation of rights in original Battle City music or individual recordings; no listening review performed by this helper.",
        "audio_files": audio_rows,
        "canonical_notices": notice_rows,
        "owner_decision": {"status": "PENDING", "owner_review_completed": False},
        "generator": {"path": str(Path(__file__).resolve().relative_to(root)),
                      "sha256": sha256(read_regular(Path(__file__)))},
    }
    lines = [
        "# Candidate audio review inventory", "",
        "**Owner decision: PENDING.** This packet contains mechanical checks and the repository's provenance claims. It does not record a completed listening review, rights determination or release approval.", "",
        "- Candidate: `" + TAG + "`",
        "- Source: `" + COMMIT + "`",
        "- ZIP: `" + str(archive.relative_to(root)) + "`",
        "- Verified ZIP SHA-256: `" + archive_hash + "`",
        "- Attestation SHA-256: `" + sha256(attestation_bytes) + "`",
        "- Exactly 22 OGG members match the pinned source and current checkout byte-for-byte.",
        "- All four canonical notices match their archive counterparts, pinned source and current checkout byte-for-byte.", "",
        "## Recorded sources", "",
        "Twenty files retain their original paths and bytes from `krystiankaluzny/Tanks@" + retained_commit + "`. ASSET_LICENSES credits Redas Jefisovas (`holoflash`) for the effect-sound set and refinements, and upstream PR 35 for OGG conversion. Notice: `LICENSES/MIT-upstream.txt`.", "",
        "Two cues come from `JustoSenka/BattleCity@" + musical_commit + "`: `Assets/Audio/levelstarting.ogg` maps to `stage_start_up.ogg`; `Assets/Audio/gameover.ogg` maps to `game_over.ogg`. ASSET_LICENSES records unchanged 48,000 Hz mono recordings and the repository's MIT declaration, copyright 2019 Justas Glodenis. Notice: `LICENSES/MIT-JustoSenka-BattleCity.txt`.", "",
        "These are repository claims, not independent confirmation of rights in original Battle City music or each recording. Owner listening, provenance review and any release decision remain pending.", "",
        "## Exact candidate audio members", "",
        "Source key: **T** = krystiankaluzny/Tanks; **J** = JustoSenka/BattleCity. Full upstream mappings are in `inventory.json`.", "",
        "| Archive path | Source | Bytes | SHA-256 |",
        "| --- | --- | ---: | --- |",
    ]
    for row in audio_rows:
        lines.append("| `{}` | {} | {} | `{}` |".format(
            row["archive_path"], "J" if row["source_id"] == "justosenka-battlecity" else "T",
            row["size_bytes"], row["sha256"]))
    lines += ["", "## Canonical notice counterparts", "",
              "| Repository path | Archive path | Bytes | SHA-256 |",
              "| --- | --- | ---: | --- |"]
    for row in notice_rows:
        lines.append("| `{}` | `{}` | {} | `{}` |".format(
            row["repository_path"], row["archive_path"], row["size_bytes"], row["sha256"]))
    lines += ["", "The candidate ZIP was inspected in memory without extraction or modification. This packet is separate from the formal release status and leaves its owner decision unchanged.", ""]
    output.mkdir(parents=True, exist_ok=False)
    (output / "inventory.json").write_text(json.dumps(packet, indent=2) + "\n", encoding="utf-8")
    (output / "review.md").write_text("\n".join(lines), encoding="utf-8")
    print("Prepared candidate-bound inventory with 22 audio files and four notices:")
    print(output / "inventory.json")
    print(output / "review.md")
    print("Owner decision remains PENDING.")


if __name__ == "__main__":
    try:
        main()
    except (ValueError, OSError, subprocess.CalledProcessError, zipfile.BadZipFile,
            KeyError, RuntimeError) as exc:
        print("Audio inventory not generated: " + str(exc), file=sys.stderr)
        sys.exit(1)
