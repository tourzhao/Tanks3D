#!/usr/bin/env python3
"""Validate staged Godot resources, script import and a real headless game run.

Godot's editor can return success after a script parse error, so exit status alone
is insufficient. This check examines both console output and the engine log,
then requires an actual completed sample report from the native-core game.
It does not validate GPU rendering, input devices or performance.
"""

import argparse
import hashlib
import json
import math
from pathlib import Path
import re
import subprocess
import sys

SCRIPT_DIRECTORY = Path(__file__).resolve().parent
if str(SCRIPT_DIRECTORY) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIRECTORY))
import prepare_godot_sample as staging

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_PROJECT = ROOT / "build/godot/project"
ANSI = re.compile(r"\x1b\[[0-?]*[ -/]*[@-~]")
ERROR_LINE = re.compile(r"^\s*(?:(?:SCRIPT |SHADER )?ERROR|FATAL(?: ERROR)?|PANIC|CRASH):", re.MULTILINE)
NATIVE_FAILURE = re.compile(
    r"(?:Failed to load GDExtension|No GDExtension library found|"
    r"Cannot open (?:dynamic library|GDExtension)|Symbol not found|"
    r"Library not loaded|Segmentation fault|AddressSanitizer|UndefinedBehaviorSanitizer)",
    re.IGNORECASE,
)


def digest(path):
    value = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            value.update(block)
    return value.hexdigest()


def validate_output(text, returncode, label):
    clean = ANSI.sub("", text)
    failures = [line.strip() for line in clean.splitlines() if ERROR_LINE.search(line) or NATIVE_FAILURE.search(line)]
    if returncode != 0 or failures:
        detail = "\n".join(failures[:12]) or "No error text; inspect the captured console log."
        raise RuntimeError(f"{label} failed (exit {returncode}):\n{detail}")
    return clean


def validate_report(text, frames):
    if "TANKS_SAMPLE_READY " not in text:
        raise RuntimeError("Headless game did not initialize the native sample")
    reports = re.findall(r"^TANKS_SAMPLE_REPORT (.+)$", text, flags=re.MULTILINE)
    if len(reports) != 1:
        raise RuntimeError("Expected exactly one completed headless sample report")
    report = json.loads(reports[0])
    if report.get("frames") != frames or report.get("demo_fixed_step") is not True:
        raise RuntimeError("Headless sample did not complete its requested fixed-step input tape")
    if not isinstance(report.get("final_digest"), str) or not report["final_digest"]:
        raise RuntimeError("Headless sample report is missing the native simulation digest")
    return report


def validate_ui_report(text):
    reports = re.findall(r"^TANKS_UI_CHECKS_PASSED (.+)$", text, flags=re.MULTILINE)
    required = {"settings", "nations", "two-player", "camera", "quick-pause", "pixel", "menu",
                "restart", "native-report", "gui-accept-press-hold-release", "keyboard-fire-locations",
                "focus-clear", "background-gui", "focus-lifecycle", "controller-menu", "frame-input", "enter-start", "render-cache", "game-over", "record",
                "record-timeout", "session-record", "audio-resources", "native-audio", "coop-camera", "player-visibility",
                "running-gear-lifecycle", "native-fire-effects", "arcade-hud", "raylib-ui-parity"}
    if len(reports) != 1 or not required.issubset(set(reports[0].split("/"))):
        raise RuntimeError("UI integration did not confirm all required native-core checks")
    return reports[0].split("/")


def validate_art_report(text):
    reports = re.findall(r"^TANKS_ART_CHECKS_PASSED (.+)$", text, flags=re.MULTILINE)
    if len(reports) != 1:
        raise RuntimeError("Expected exactly one completed structural art report")
    report = json.loads(reports[0])
    required = {"geometry", "winding", "budgets", "cache", "material-isolation",
                "identity", "national-color", "enemy-status-patches", "muzzle", "footprints", "motion", "effects", "rng"}
    counts = {"vehicles": 24, "national_color_cases": 192, "brick_cases": 512, "pickups": 9,
              "base_walls": 10, "bases": 3, "forest_cases": 676, "motion_cases": 24}
    if (report.get("status") != "passed" or report.get("counts") != counts
            or type(report["counts"]["national_color_cases"]) is not int
            or not required.issubset(set(report.get("checks", [])))
            or not isinstance(report.get("meshes_checked"), int) or report["meshes_checked"] <= 0):
        raise RuntimeError("Structural art checks did not complete every required asset contract")
    return report


def validate_trace_report(text):
    reports = re.findall(r"^TANKS_FRAME_TRACE_PASSED (.+)$", text, flags=re.MULTILINE)
    required = {"prior-callback", "all-phases", "bounds", "ownership", "counter-reset",
                "finalization", "window-draw-coverage", "window-transition-bounds"}
    if len(reports) != 1 or not required.issubset(set(reports[0].split("/"))):
        raise RuntimeError("Slow-frame trace did not preserve attribution, phases and bounded window evidence")
    return reports[0].split("/")


def validate_coop_camera_report(text):
    reports = re.findall(r"^TANKS_COOP_CAMERA_PASSED(?:[ \t]+(.*))?$", text, flags=re.MULTILINE)
    if len(reports) != 1:
        raise RuntimeError("Expected exactly one completed co-op camera report")
    match = re.fullmatch(r"([a-z0-9/-]+) cases=([0-9]+)", reports[0])
    required = {"projected-bounds", "road-margin", "minimum-fit", "orbit",
                "solo-nearby", "order", "lag", "aspect"}
    checks = match[1].split("/") if match else []
    if (not match or match[2] != "135" or set(checks) != required or len(checks) != len(required)):
        raise RuntimeError("Co-op camera did not complete all projection contracts and exactly 135 cases")
    return {"checks": checks, "cases": 135}


def validate_battlefield_camera_report(text):
    reports = re.findall(r"^TANKS_BATTLEFIELD_CAMERA_PASSED(?:[ \t]+(.*))?$", text, flags=re.MULTILINE)
    if len(reports) != 1:
        raise RuntimeError("Expected exactly one completed battlefield camera report")
    try:
        report = json.loads(reports[0])
    except json.JSONDecodeError as error:
        raise RuntimeError("Battlefield camera report must contain valid JSON") from error
    required = {"only-translation", "player-envelope", "road-margin", "map-coverage", "centering",
                "angles", "order", "continuity", "lag", "near-depth", "no-players"}
    if not isinstance(report, dict):
        raise RuntimeError("Battlefield camera report must be a JSON object")
    checks = report.get("checks")
    if (report.get("status") != "passed" or type(report.get("cases")) is not int or report["cases"] != 1809
            or not isinstance(checks, list) or not all(isinstance(value, str) for value in checks)
            or len(checks) != len(required) or set(checks) != required):
        raise RuntimeError("Battlefield camera did not complete exactly 1809 cases and all 11 unique contracts")
    return report


def validate_running_gear_report(text):
    reports = re.findall(r"^TANKS_RUNNING_GEAR_PASSED(?:[ \t]+(.*))?$", text, flags=re.MULTILINE)
    if len(reports) != 1:
        raise RuntimeError("Expected exactly one completed running gear report")
    try:
        report = json.loads(reports[0])
    except json.JSONDecodeError as error:
        raise RuntimeError("Running gear report must contain valid JSON") from error
    required = {"displacement", "blocked", "cardinals", "reverse", "turn", "lateral", "pause", "freeze",
                "creating", "teleport", "respawn", "isolation", "shared-mesh", "bounds", "budgets",
                "ghost-shadows", "boat", "reset", "catchup", "rng", "historical-chassis"}
    if not isinstance(report, dict):
        raise RuntimeError("Running gear report must be a JSON object")
    checks = report.get("checks")
    counts = {"models": 24, "cases": 493, "geometry_frames": 768}
    maximum = report.get("maximum_triangles")
    if (report.get("status") != "passed"
            or any(type(report.get(key)) is not int or report[key] != value for key, value in counts.items())
            or type(maximum) is not int or not 0 < maximum <= 4000
            or not isinstance(checks, list) or not all(isinstance(value, str) for value in checks)
            or len(checks) != len(required) or set(checks) != required):
        raise RuntimeError("Running gear did not complete 24 models, 493 cases, 768 geometry frames and all 21 contracts")
    return report


def validate_audio_report(text, require_mixer=False):
    reports = re.findall(r"^TANKS_AUDIO_CHECKS_PASSED (.+)$", text, flags=re.MULTILINE)
    if len(reports) != 1:
        raise RuntimeError("Expected exactly one completed audio resource report")
    try:
        report = json.loads(reports[0])
    except json.JSONDecodeError as error:
        raise RuntimeError("Audio report must contain valid JSON") from error
    if not isinstance(report, dict) or type(report.get("resources")) is not int or report["resources"] != 22:
        raise RuntimeError("Audio resource checks did not validate every native cue")
    checks = report.get("checks")
    required = {"resources", "lazy-pool", "reuse", "voice-limit", "single", "priority",
                "engine-exclusive", "disabled", "zero-gain"}
    if (not isinstance(checks, list) or not all(isinstance(value, str) for value in checks)
            or len(checks) != len(set(checks)) or not required.issubset(set(checks))):
        raise RuntimeError("Audio checks did not complete every lazy voice-pool contract")
    if require_mixer:
        required_mixer = {"decoded-mixer", "routing", "overlap-limit", "zero-mute", "volume-restore", "stop"}
        mixed = report.get("mixed_cues")
        expected = {path.stem for path in (ROOT / "resources/sounds").glob("*.ogg")}
        if (not required_mixer.issubset(set(checks)) or not isinstance(mixed, dict)
                or len(expected) != 22 or set(mixed) != expected
                or not all(type(value) in (int, float) and math.isfinite(value) and value > 0.00001
                           for value in mixed.values())):
            raise RuntimeError("Audio mixer did not confirm decoded samples and routing for all 22 recordings")
    return report


def validate_player_visibility_report(text):
    reports = re.findall(r"^TANKS_PLAYER_VISIBILITY_PASSED(?:[ \t]+(.*))?$", text, flags=re.MULTILINE)
    if len(reports) != 1:
        raise RuntimeError("Expected exactly one completed player visibility report")
    try:
        report = json.loads(reports[0])
    except json.JSONDecodeError as error:
        raise RuntimeError("Player visibility report must contain valid JSON") from error
    required = {"brick-mask", "occluder-height", "forest-cover", "player-only", "rigid-follow",
                "material-isolation", "source-unchanged", "lifetime", "bounded-cache", "pause", "rng"}
    if not isinstance(report, dict):
        raise RuntimeError("Player visibility report must be a JSON object")
    checks = report.get("checks")
    if (report.get("status") != "passed" or type(report.get("cases")) is not int or report["cases"] != 30
            or not isinstance(checks, list) or not all(isinstance(value, str) for value in checks)
            or len(checks) != len(required) or set(checks) != required):
        raise RuntimeError("Player visibility did not complete exactly 30 cases and all 11 unique contracts")
    return report


def validate_effect_pool_report(text):
    reports = re.findall(r"^TANKS_EFFECT_POOL_CHECKS_PASSED (.+)$", text, flags=re.MULTILINE)
    if len(reports) != 1:
        raise RuntimeError("Expected exactly one completed effect pool report")
    report = json.loads(reports[0])
    required = {"lazy", "admission", "reuse", "reset", "visibility", "material-isolation",
                "shared-mesh", "lifecycle", "integration", "rng"}
    expected = {"live_limit": 48, "retained_limit": 96, "warmed_created": 96,
                "final_created": 96, "retained_peak": 96, "cycles": 192}
    if (report.get("status") != "passed"
            or any(report.get(key) != value for key, value in expected.items())
            or not required.issubset(set(report.get("checks", [])))
            or not isinstance(report.get("reused"), int) or report["reused"] < 192 * 48):
        raise RuntimeError("Effect pool did not preserve bounds, reuse and visual lifecycle contracts")
    return report


def validate_dependencies():
    manifest = json.loads((ROOT / "godot/DEPENDENCIES.json").read_text())
    verified = {}
    for key, entry in manifest["dependencies"].items():
        archive = ROOT / "build/godot-tools" / entry["archive"]
        if not archive.is_file() or digest(archive) != entry["sha256"]:
            raise RuntimeError(f"Dependency archive missing or modified: {archive}")
        if not (ROOT / entry["license_file"]).is_file():
            raise RuntimeError(f"Missing license record for {key}")
        verified[key] = {"version": entry["version"], "sha256": entry["sha256"]}
    return verified


def validate_staged(project):
    result = {}
    for relative, source in staging.source_files().items():
        destination = project / relative
        if not source.is_file() or not destination.is_file() or digest(source) != digest(destination):
            raise RuntimeError(f"Staged resource missing or stale: {destination}")
        result[str(destination.relative_to(project))] = digest(destination)
    return result


def scene_probe(output):
    """Load every declared scene through Godot, without instantiating gameplay."""
    scenes = sorted("res://" + path.relative_to(ROOT / "godot/sample").as_posix()
                    for path in (ROOT / "godot/sample").rglob("*.tscn"))
    if not scenes:
        raise RuntimeError("The sample has no scene files")
    output.write_text("extends SceneTree\nfunc _initialize() -> void:\n"
                      "    var failures: int = 0\n"
                      f"    for path in {json.dumps(scenes)}:\n"
                      "        if not (ResourceLoader.load(path) is PackedScene):\n"
                      "            push_error(\"Scene load failed: \" + path)\n"
                      "            failures += 1\n"
                      f"    print(\"TANKS_SAMPLE_SCENES_CHECKED {len(scenes)}\")\n"
                      "    quit(1 if failures else 0)\n")
    return scenes


def run_check(godot, project, logs, label, arguments, timeout, headless=True, cwd=None):
    engine_log = logs / f"{label}.engine.log"
    console_log = logs / f"{label}.console.log"
    engine_log.unlink(missing_ok=True)
    command = [str(godot), *(["--headless"] if headless else []),
               *(["--path", str(project)] if project is not None else []),
               "--log-file", str(engine_log), *arguments]
    try:
        result = subprocess.run(command, cwd=ROOT if cwd is None else cwd, text=True, stdout=subprocess.PIPE,
                                stderr=subprocess.STDOUT, timeout=timeout)
    except subprocess.TimeoutExpired as error:
        output = error.stdout or b""
        console_log.write_text(output.decode(errors="replace") if isinstance(output, bytes) else output)
        raise RuntimeError(f"{label} timed out after {timeout}s; see {console_log}") from error
    console_log.write_text(result.stdout)
    engine_text = engine_log.read_text(errors="replace") if engine_log.exists() else ""
    # Inspect both channels; the report is read only from stdout to avoid duplicates.
    validate_output(result.stdout + "\n" + engine_text, result.returncode, label)
    print(f"PASS {label} ({console_log.relative_to(ROOT)})", flush=True)
    return ANSI.sub("", result.stdout)


def imported_files(project, staged):
    """Fingerprint import products without editor layouts or GPU caches."""
    metadata = [project / ".godot" / name for name in
                ("extension_list.cfg", "global_script_class_cache.cfg", "uid_cache.bin")]
    imported = project / ".godot/imported"
    if not imported.is_dir() or any(not path.is_file() for path in metadata):
        return None
    products = metadata + [path for path in imported.rglob("*") if path.is_file()]
    for name in staged:
        products += [path for suffix in (".import", ".uid")
                     if (path := project / (name + suffix)).exists()]
    result = {}
    for path in products:
        if any(part.is_symlink() for part in [path, *path.parents] if part.is_relative_to(project)):
            raise RuntimeError(f"Linked import product: {path}")
        result[path.relative_to(project).as_posix()] = digest(path)
    return result


def prepare_import(godot, project, logs, timeout, dependencies, staged, force=False):
    """Reuse only an import whose engine, staged inputs and products still match."""
    receipt = logs / "prepare-result.json"
    if receipt.is_symlink():
        raise RuntimeError("Import preparation receipt must not be a symlink")
    expected = {"schema": "tanks3d-godot-preparation-v2", "scope": "staging/import",
                "dependencies": dependencies, "godot_sha256": digest(godot), "staged_sha256": staged}
    products = imported_files(project, staged)
    try:
        previous = json.loads(receipt.read_text())
    except (OSError, ValueError):
        previous = None
    if not force and products is not None and previous == expected | {"imported_sha256": products}:
        print("PASS cached import (staged inputs and import products unchanged)", flush=True)
        return
    receipt.unlink(missing_ok=True)
    # Godot's editor scan trusts mtimes, while staging intentionally preserves
    # them. Rescan content on cache misses (including old v1 receipts, which
    # could attest a changed source alongside its stale imported resource).
    # Keep importer settings, resource UIDs and compiled products intact.
    editor = project / ".godot/editor"
    if any(path.is_symlink() for path in [editor, *editor.parents] if path.is_relative_to(project)):
        raise RuntimeError(f"Linked editor cache directory: {editor}")
    for cache in editor.glob("filesystem_cache*"):
        if cache.is_symlink():
            raise RuntimeError(f"Linked editor filesystem cache: {cache}")
        cache.unlink()
    run_check(godot, project, logs, "import", ["--editor", "--import", "--quit"], timeout)
    products = imported_files(project, staged)
    if products is None:
        raise RuntimeError("Godot import did not produce required discovery and resource metadata")
    receipt.write_text(json.dumps(expected | {"imported_sha256": products}, indent=2) + "\n")


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--godot", type=Path, default=ROOT / "build/godot-tools/Godot.app/Contents/MacOS/Godot")
    parser.add_argument("--project", type=Path, default=DEFAULT_PROJECT)
    parser.add_argument("--log-dir", type=Path, default=ROOT / "build/godot/validation")
    parser.add_argument("--skip-stage", action="store_true")
    modes = parser.add_mutually_exclusive_group()
    modes.add_argument("--prepare-only", action="store_true",
                       help="Stage resources and import changed inputs without running regression checks")
    modes.add_argument("--import-only", action="store_true",
                       help="Run all import/presentation contracts without native/UI gameplay smoke checks")
    parser.add_argument("--frames", type=int, default=180)
    parser.add_argument("--timeout", type=int, default=120)
    args = parser.parse_args(argv)
    project, logs = args.project.resolve(), args.log_dir.resolve()
    if not project.is_relative_to(ROOT / "build") or not logs.is_relative_to(ROOT / "build"):
        parser.error("The staged project and logs must be inside this repository's build/ directory")
    if not args.skip_stage and project != DEFAULT_PROJECT:
        parser.error("Custom project path requires --skip-stage")
    if args.frames < 1 or args.timeout < 1:
        parser.error("Frames and timeout must be positive")
    logs.mkdir(parents=True, exist_ok=True)
    receipt = logs / "result.json"
    if not args.prepare_only:
        receipt.unlink(missing_ok=True)
    if not args.skip_stage:
        subprocess.run([sys.executable, str(ROOT / "scripts/prepare_godot_sample.py")], cwd=ROOT, check=True)
    verified_dependencies = validate_dependencies()
    staged = validate_staged(project)
    prepare_import(args.godot, project, logs, args.timeout, verified_dependencies, staged,
                   force=not args.prepare_only)
    if args.prepare_only:
        print(f"Godot project prepared: {project.relative_to(ROOT)}", flush=True)
        return
    for source in sorted((ROOT / "godot/sample").rglob("*.gd")):
        relative = source.relative_to(ROOT / "godot/sample")
        run_check(args.godot, project, logs, "parse-" + relative.with_suffix("").as_posix().replace("/", "__"),
                  ["--check-only", "--script", "res://" + relative.as_posix()], args.timeout)
    probe = logs / "check_scenes.gd"
    scenes = scene_probe(probe)
    run_check(args.godot, project, logs, "scenes", ["--script", str(probe)], args.timeout)
    output = run_check(args.godot, project, logs, "art-contracts",
                       ["--script", "res://art_checks.gd"], args.timeout)
    art_checks = validate_art_report(output)
    output = run_check(args.godot, project, logs, "shell-flight",
                       ["--script", "res://shell_flight_checks.gd"], args.timeout)
    if "TANKS_SHELL_FLIGHT_CHECKS_PASSED 12-models/four-directions/owner-lifecycle/native-impact" not in output:
        raise RuntimeError("Visual shell emergence did not complete its native-flight contract")
    output = run_check(args.godot, project, logs, "coop-camera",
                       ["--script", "res://coop_camera_checks.gd"], args.timeout)
    coop_camera_checks = validate_coop_camera_report(output)
    output = run_check(args.godot, project, logs, "battlefield-camera",
                       ["--script", "res://battlefield_camera_checks.gd"], args.timeout)
    battlefield_camera_checks = validate_battlefield_camera_report(output)
    output = run_check(args.godot, project, logs, "player-visibility",
                       ["--script", "res://player_visibility_checks.gd"], args.timeout)
    player_visibility_checks = validate_player_visibility_report(output)
    output = run_check(args.godot, project, logs, "running-gear",
                       ["--script", "res://running_gear_checks.gd"], args.timeout)
    running_gear_checks = validate_running_gear_report(output)
    output = run_check(args.godot, project, logs, "effect-pool",
                       ["--script", "res://effect_pool_checks.gd"], args.timeout)
    effect_pool_checks = validate_effect_pool_report(output)
    output = run_check(args.godot, project, logs, "frame-metrics",
                       ["--script", "res://frame_metrics_checks.gd"], args.timeout)
    if "TANKS_FRAME_METRICS_PASSED warmup/full-run/windows/bounds/overflow" not in output:
        raise RuntimeError("Bounded timing collection did not complete its long-run contract")
    output = run_check(args.godot, project, logs, "frame-trace",
                       ["--script", "res://frame_trace_checks.gd"], args.timeout)
    validate_trace_report(output)
    output = run_check(args.godot, project, logs, "audio-resources",
                       ["--script", "res://audio_checks.gd"], args.timeout)
    audio_checks = validate_audio_report(output)
    report = None
    ui_checks = None
    if not args.import_only:
        output = run_check(args.godot, project, logs, "smoke",
                           ["--fixed-fps", "60", "--", "--demo", f"--frames={args.frames}", "--seed=20260916"],
                           args.timeout)
        report = validate_report(output, args.frames)
        output = run_check(args.godot, project, logs, "ui-smoke",
                           ["--", "--ui-self-test"], args.timeout)
        ui_checks = validate_ui_report(output)
    receipt.write_text(json.dumps({
        "status": "passed", "scope": "import/parse/art/camera" if args.import_only else "import/parse/art/camera/headless native sample",
        "dependencies": verified_dependencies, "staged_sha256": staged, "scenes": scenes,
        "headless_report": report, "ui_checks": ui_checks, "art_checks": art_checks,
        "audio_checks": audio_checks,
        "effect_pool_checks": effect_pool_checks,
        "coop_camera_checks": coop_camera_checks,
        "battlefield_camera_checks": battlefield_camera_checks,
        "player_visibility_checks": player_visibility_checks,
        "running_gear_checks": running_gear_checks,
        "limitations": ["Headless dummy renderer does not validate GPU rendering or performance", "No physical input QA"],
    }, indent=2) + "\n")
    print(f"Godot validation passed; receipt: {receipt.relative_to(ROOT)}")


if __name__ == "__main__":
    try:
        main()
    except (OSError, RuntimeError, ValueError, subprocess.CalledProcessError) as error:
        raise SystemExit(str(error)) from error
