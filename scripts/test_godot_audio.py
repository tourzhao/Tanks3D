#!/usr/bin/env python3
"""Check the real Godot audio mixer through a muted downstream bus."""

import argparse
import json
from pathlib import Path

import test_godot_import as gate
import package_godot_app as package

ROOT = Path(__file__).resolve().parents[1]
PROJECT = ROOT / "build/godot/project"
LOGS = ROOT / "build/godot/audio-validation"


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--packaged", action="store_true",
                        help="Decode the standalone app's PCK from outside the source project")
    args = parser.parse_args(argv)
    logs = ROOT / "build/godot/audio-package-validation" if args.packaged else LOGS
    logs.mkdir(parents=True, exist_ok=True)
    receipt = logs / "result.json"
    receipt.unlink(missing_ok=True)
    dependencies = gate.validate_dependencies()
    if args.packaged:
        manifest = package.verify(package.APP)
        staged = manifest["staged_sha256"]
        # Only this test script is external. Every res:// lookup resolves in the
        # app's automatically discovered PCK, with no --path or repository cwd.
        script = logs / "audio_checks.gd"
        script.write_bytes((ROOT / "godot/sample/audio_checks.gd").read_bytes())
        script_sha256 = gate.digest(script)
        godot = package.APP / "Contents/MacOS" / package.EXECUTABLE
        project = None
        cwd = "/private/tmp"
        script_name = str(script)
    else:
        staged = gate.validate_staged(PROJECT)
        godot = ROOT / "build/godot-tools/Godot.app/Contents/MacOS/Godot"
        project = PROJECT
        cwd = ROOT
        script_name = "res://audio_checks.gd"
    output = gate.run_check(
        godot, project, logs,
        "mixer", ["--rendering-method", "mobile", "--rendering-driver", "metal",
                  "--script", script_name, "--", "--capture-audio",
                  *(["--require-import-only"] if args.packaged else [])],
        180, headless=False, cwd=cwd,
    )
    report = gate.validate_audio_report(output, require_mixer=True)
    if args.packaged:
        if "import-only" not in report["checks"]:
            raise RuntimeError("Packaged mixer did not prove that raw source assets are absent")
        if package.verify(package.APP) != manifest:
            raise RuntimeError("Packaged runtime changed during mixer validation")
        if gate.digest(script) != script_sha256:
            raise RuntimeError("Packaged mixer test script changed during validation")
    elif gate.validate_staged(PROJECT) != staged:
        raise RuntimeError("Staged audio resources changed during mixer validation")
    receipt.write_text(json.dumps({
        "status": "passed", "scope": "decoded audio mixer with muted downstream output",
        "dependencies": dependencies, "staged_sha256": staged, "audio_checks": report,
        "resource_source": "standalone app PCK" if args.packaged else "staged project",
        **({"pack_entries_sha256": manifest["pack_entries_sha256"], "test_script_sha256": script_sha256}
           if args.packaged else {}),
        "limitations": ["Does not verify speaker/headphone audibility or physical device latency"],
    }, indent=2) + "\n")
    print(f"Godot muted-mixer checks passed; receipt: {receipt.relative_to(ROOT)}")


if __name__ == "__main__":
    try:
        main()
    except (OSError, RuntimeError, ValueError) as error:
        raise SystemExit(str(error)) from error
