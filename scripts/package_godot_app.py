#!/usr/bin/env python3
"""Build/check a self-contained arm64 Godot app from cached tools.

Uses the cached official editor executable as a runtime, a PCK containing the
already imported project, and the engine-independent GDExtension. The CLI builds
an ad-hoc signed development app. godot_release.py supplies an immutable source
identity when building a candidate; neither path claims notarization.
No export-template download is needed. Build/import with make godot-sample first.
"""

import argparse
import json
import os
from pathlib import Path, PurePosixPath
import plistlib
import re
import shutil
import subprocess
import sys

SCRIPT_DIRECTORY = Path(__file__).resolve().parent
if str(SCRIPT_DIRECTORY) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIRECTORY))
import test_godot_import as gate

ROOT = SCRIPT_DIRECTORY.parent
WORK = ROOT / "build/godot/package"
APP = ROOT / "build/Tanks3D-Godot.app"
EXECUTABLE = "Tanks3D-Godot"
BUNDLE_ID = "io.github.tourzhao.tanks3d.godot.local"
RELEASE_BUNDLE_ID = "io.github.tourzhao.tanks3d.godot"
SYSTEM_PREFIXES = ("/System/Library/", "/usr/lib/")
# Prefer Apple's GameController path for the original Nintendo Switch Pro.
# Filter this VID/PID from SDL's raw HID backend, not from gamepad input.
# Other controllers (including Joy-Cons) keep Godot/SDL's normal driver choice.
MACOS_RUNTIME_ENV = {"SDL_HIDAPI_IGNORE_DEVICES": "0x057e/0x2009"}
# Development staging retains these standalone diagnostics and historical
# texture. None is loaded by the active frontend or the packaged UI smoke.
# Keep ui_checks, arcade_ui_checks, parity_checks and report_preview in the PCK.
PRODUCTION_EXCLUDED_SOURCES = frozenset({
    "art_review.gd", "art_checks.gd", "audio_checks.gd", "battlefield_camera_checks.gd",
    "coop_camera_checks.gd", "effect_pool_checks.gd", "frame_metrics_checks.gd",
    "frame_trace_checks.gd", "lan_checks.gd", "player_visibility_checks.gd",
    "running_gear_checks.gd", "shell_flight_checks.gd", "resources/textures/urban_masonry.png",
})
PACK_METADATA = {".godot/extension_list.cfg", ".godot/global_script_class_cache.cfg", ".godot/uid_cache.bin"}

PACK_SCRIPT = '''extends SceneTree
func _initialize() -> void:
    var args := OS.get_cmdline_user_args()
    if args.size() != 2:
        push_error("Packing requires a manifest and output path")
        quit(2)
        return
    var files: Array = JSON.parse_string(FileAccess.get_file_as_string(args[0]))
    var packer := PCKPacker.new()
    if packer.pck_start(args[1]) != OK:
        push_error("Could not create project PCK")
        quit(2)
        return
    for entry in files:
        if packer.add_file(entry.target, entry.source) != OK:
            push_error("Could not add pack entry: " + entry.target)
            quit(2)
            return
    if packer.flush() != OK:
        push_error("Could not finish project PCK")
        quit(2)
        return
    print("TANKS_SAMPLE_PACK_CREATED " + str(files.size()))
    quit()
'''


def run(command):
    result = subprocess.run([str(value) for value in command], cwd=ROOT, text=True,
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, check=False, timeout=120)
    if result.returncode:
        raise RuntimeError(f"Command failed: {command[0]}\n{result.stdout}")
    return result.stdout


def linked_libraries(output, own_id=None):
    dependencies = []
    for line in output.splitlines():
        if " (compatibility version " not in line:
            continue
        name = line.strip().split(" (compatibility version ", 1)[0]
        if name != own_id:
            dependencies.append(name)
    return dependencies


def library_dependencies(path):
    ids = run(["otool", "-D", path]).splitlines()[1:]
    return linked_libraries(run(["otool", "-L", path]), ids[0].strip() if ids else None)


def minimum_macos(output):
    versions = []
    for command in re.split(r"^Load command \d+\s*$", output, flags=re.MULTILINE):
        if re.search(r"^\s*cmd LC_BUILD_VERSION\s*$", command, re.MULTILINE):
            field = "minos"
        elif re.search(r"^\s*cmd LC_VERSION_MIN_MACOSX\s*$", command, re.MULTILINE):
            field = "version"
        else:
            continue
        match = re.search(rf"^\s*{field}\s+(\d+\.\d+(?:\.\d+)?)\s*$", command, re.MULTILINE)
        if match:
            versions.append(match[1])
    if not versions:
        raise RuntimeError("Mach-O does not declare a macOS deployment version")
    return max(versions, key=lambda value: tuple(int(part) for part in value.split(".")))


def safe_relative(name):
    value = PurePosixPath(name)
    if value.is_absolute() or ".." in value.parts or "\\" in name:
        raise RuntimeError(f"Unsafe package resource path: {name}")
    return value


def production_sources(managed):
    """Select runtime sources from the full, still-authoritative staging map."""
    return {name for name in managed if name not in PRODUCTION_EXCLUDED_SOURCES
            and not name.startswith(("native/", "licenses/"))}


def imported_source_payloads(sources):
    """Raw audio/grass stay in staging; their validated imports serve the PCK."""
    return {name for name in sources if name == "resources/textures/battlefield_grass.png"
            or (name.startswith("resources/sounds/") and name.endswith(".ogg"))}


def resource_name(value):
    if not isinstance(value, str) or not value.startswith("res://"):
        raise RuntimeError(f"Invalid imported resource reference: {value}")
    name = value.removeprefix("res://")
    if not name or safe_relative(name).as_posix() != name:
        raise RuntimeError(f"Unsafe package resource path: {value}")
    return name


def import_section(text, section, path):
    matches = re.findall(rf"(?ms)^\[{section}\]\s*\n(.*?)(?=^\[|\Z)", text)
    if len(matches) != 1:
        raise RuntimeError(f"Invalid [{section}] in import sidecar: {path}")
    return matches[0]


def import_value(section, key, path):
    matches = list(re.finditer(rf"(?m)^\s*{key}\s*=\s*", section))
    if len(matches) != 1:
        raise RuntimeError(f"Invalid {key} in import sidecar: {path}")
    try:
        return json.JSONDecoder().raw_decode(section[matches[0].end():])[0]
    except ValueError as error:
        raise RuntimeError(f"Invalid {key} in import sidecar: {path}") from error


def imported_resources(project, sources, files):
    """Keep only generated products declared by retained source sidecars."""
    retained = set()
    remapped = set()
    for name in sorted(sources):
        sidecar = name + ".import"
        if sidecar not in files:
            if PurePosixPath(name).suffix in (".png", ".ogg", ".fnt"):
                raise RuntimeError(f"Imported staged asset is missing its sidecar: {name}")
            continue
        path = project / sidecar
        text = path.read_text()
        deps = import_section(text, "deps", path)
        if import_value(deps, "source_file", path) != "res://" + name:
            raise RuntimeError(f"Import sidecar source differs from its staged asset: {path}")
        outputs = import_value(deps, "dest_files", path)
        if not isinstance(outputs, list) or not outputs:
            raise RuntimeError(f"Invalid dest_files in import sidecar: {path}")
        destinations = {resource_name(value) for value in outputs}
        if any(not value.startswith(".godot/imported/") for value in destinations):
            raise RuntimeError(f"Import output is outside the generated resource directory: {path}")
        remap = import_section(text, "remap", path)
        paths = re.findall(r'(?m)^path(?:\.[^\s=]+)?\s*=\s*("[^"\n]*")\s*$', remap)
        if not paths or not {resource_name(json.loads(value)) for value in paths}.issubset(destinations):
            raise RuntimeError(f"Import remap differs from its declared outputs: {path}")
        if not destinations.issubset(files):
            raise RuntimeError(f"Imported staged asset is missing generated outputs: {path}")
        remapped.add(name)
        retained.update(destinations)
        for destination in destinations:
            checksum = str(PurePosixPath(destination).with_suffix(".md5"))
            if checksum in files:
                retained.add(checksum)
    return retained, remapped


def validate_pack_references(project, sources, files, remapped):
    """Static dependencies resolve to packed sources or validated import remaps."""
    available = sources | remapped
    for name in sorted(sources):
        suffix = PurePosixPath(name).suffix
        if suffix == ".gd":
            references = re.findall(r'''\b(?:preload|load)\(\s*(["'])(res://[^"']+)\1\s*\)''',
                                    (project / name).read_text())
            references = [value for _, value in references]
        elif suffix in (".tscn", ".tres", ".godot", ".gdshader"):
            references = re.findall(r'"(res://[^"\n]+)"', (project / name).read_text())
        else:
            continue
        for reference in references:
            if resource_name(reference) not in available:
                raise RuntimeError(f"Production resource {name} references an excluded or missing source: {reference}")
    for name in PACK_METADATA - {".godot/uid_cache.bin"}:
        if name not in files:
            continue
        for reference in re.findall(r'"?(res://[^"\s]+)"?', (project / name).read_text()):
            if resource_name(reference) not in available:
                raise RuntimeError(f"Production metadata {name} references an excluded or missing source: {reference}")
    # ResourceUID caches are kept byte-for-byte. Unused UID entries need no
    # rewriting; real resource dependencies above must resolve in the PCK.


def pack_entries(project, managed=None):
    # Every source is checked against the full development map before applying
    # production exclusions. Unknown files cannot hide in licenses/native or
    # beside a deliberately excluded diagnostic.
    if managed is None:
        managed = {name: gate.digest(source) for name, source in gate.staging.source_files().items()}
    files = {}
    for source in sorted(project.rglob("*")):
        if source.is_symlink():
            raise RuntimeError(f"Staged project contains a symlink: {source}")
        if not source.is_file():
            continue
        relative = source.relative_to(project)
        name = relative.as_posix()
        safe_relative(name)
        if source.name in (".DS_Store", ".gdignore"):
            continue
        if relative.parts[0] != ".godot" and name not in managed:
            original = name[:-7] if name.endswith(".import") else name[:-4] if name.endswith(".uid") else name
            if original == name or original not in managed:
                raise RuntimeError(f"Unmanaged staged resource in the imported project: {relative}")
        files[name] = source
    if not set(managed).issubset(files):
        raise RuntimeError("Staged project is missing managed source resources")
    # Validate the full development map, including raw assets, diagnostics,
    # native code and notices whose payloads are not stored in the PCK.
    source_hashes = {name: gate.digest(files[name]) for name in managed}
    if isinstance(managed, dict):
        for name, digest in source_hashes.items():
            if digest != managed[name]:
                raise RuntimeError(f"Staged resource changed since validation: {name}")
    sources = production_sources(managed)
    imported, remapped = imported_resources(project, sources, files)
    raw_payloads = imported_source_payloads(sources)
    if not raw_payloads.issubset(remapped):
        raise RuntimeError("Raw source payloads require validated import remaps before omission")
    packed_sources = sources - raw_payloads
    validate_pack_references(project, packed_sources, files, remapped)
    names = packed_sources | imported | (PACK_METADATA & files.keys())
    names.update(name for name in files if (name.endswith(".import") and name[:-7] in sources)
                 or (name.endswith(".uid") and name[:-4] in sources))
    entries = []
    for name in sorted(names):
        source = files[name]
        digest = source_hashes[name] if name in source_hashes else gate.digest(source)
        entries.append({"target": "res://" + name, "source": str(source), "sha256": digest})
    required = {"res://project.godot", "res://native.gdextension", "res://.godot/extension_list.cfg"}
    if not required.issubset({entry["target"] for entry in entries}):
        raise RuntimeError("Imported staged project is missing settings or GDExtension discovery data")
    return entries


def expected_files(app):
    result = {}
    for path in sorted(app.rglob("*")):
        if path.is_symlink():
            raise RuntimeError(f"Local app must not contain symlinks: {path}")
        if not path.is_file():
            continue
        name = path.relative_to(app).as_posix()
        # The main Mach-O signature seals Resources, including this manifest.
        # Hashing that signature in the sealed manifest would be circular.
        # codesign verifies the executable, and its exact hash is also retained
        # in the external local receipt after final signing.
        if name in ("Contents/Resources/package-manifest.json", f"Contents/MacOS/{EXECUTABLE}") or name.startswith("Contents/_CodeSignature/"):
            continue
        result[name] = gate.digest(path)
    return result


def check_dependency(name, library, app):
    if name.startswith(SYSTEM_PREFIXES):
        return
    if not name.startswith("@loader_path/"):
        raise RuntimeError(f"Unbundled or ambiguous dependency in {library.name}: {name}")
    relative = name.removeprefix("@loader_path/")
    safe_relative(relative)
    target = (library.parent / relative).resolve()
    if not target.is_relative_to(app.resolve()) or not target.is_file():
        raise RuntimeError(f"Missing bundled dependency in {library.name}: {name}")


def validate_identity(identity):
    if (not isinstance(identity, dict) or set(identity) != {"version", "build", "commit", "tag"}
            or not all(isinstance(v, str) for v in identity.values())):
        raise RuntimeError("Invalid Godot release identity")
    if (not re.fullmatch(r"[0-9]+\.[0-9]+\.[0-9]+", identity["version"])
            or not re.fullmatch(r"[1-9][0-9]*", identity["build"])
            or not re.fullmatch(r"[0-9a-f]{40}", identity["commit"])
            or not re.fullmatch(re.escape("v" + identity["version"]) + r"-godot\.alpha\.[1-9][0-9]*", identity["tag"])):
        raise RuntimeError("Invalid Godot release version, build, commit or tag")
    return identity


def verify(app, identity=None):
    manifest_file = app / "Contents/Resources/package-manifest.json"
    manifest = json.loads(manifest_file.read_text())
    actual = expected_files(app)
    if actual != manifest["files_sha256"]:
        raise RuntimeError("Local app resource manifest does not match its exact files")
    required = {f"Contents/MacOS/{EXECUTABLE}", f"Contents/Resources/{EXECUTABLE}.pck",
                "Contents/Frameworks/libtanks_sample.dylib",
                "Contents/Resources/licenses/LICENSE", "Contents/Resources/licenses/Godot-MIT.txt",
                "Contents/Resources/licenses/ASSET_LICENSES.md", "Contents/Resources/licenses/THIRD_PARTY_NOTICES.md",
                "Contents/Resources/licenses/Godot-COPYRIGHT.txt",
                "Contents/Resources/licenses/godot-cpp-MIT.txt",
                "Contents/Resources/licenses/LICENSES/Zlib-raylib.txt",
                "Contents/Resources/licenses/LICENSES/Raylib-6.0-dependencies.txt"}
    if not required.issubset(set(actual) | {f"Contents/MacOS/{EXECUTABLE}"}) or not (app / f"Contents/MacOS/{EXECUTABLE}").is_file():
        raise RuntimeError("Local app is missing required runtime assets or original license notices")
    frameworks = {path.name for path in (app / "Contents/Frameworks").iterdir()}
    if frameworks != {"libtanks_sample.dylib"}:
        raise RuntimeError("Godot app must contain only its engine-independent extension, with no raylib runtime")
    info = plistlib.loads((app / "Contents/Info.plist").read_bytes())
    embedded = manifest.get("release_identity")
    if embedded is not None:
        validate_identity(embedded)
        if (manifest.get("release_candidate") is not True or
                info.get("Tanks3DLocalDevelopmentBuild") is not False or
                info.get("CFBundleShortVersionString") != embedded["version"] or
                info.get("CFBundleVersion") != embedded["build"] or
                info.get("Tanks3DSourceCommit") != embedded["commit"] or
                info.get("Tanks3DSourceTag") != embedded["tag"]):
            raise RuntimeError("Candidate bundle metadata differs from its release identity")
        notices = {name: path for name, path in gate.staging.source_files().items()
                   if name.startswith("licenses/")}
        bundled = {name.removeprefix("Contents/Resources/") for name in actual
                   if name.startswith("Contents/Resources/licenses/")}
        if set(notices) != bundled or any(gate.digest(app / "Contents/Resources" / name) != gate.digest(path)
                                         for name, path in notices.items()):
            raise RuntimeError("Candidate license notices differ from their tagged originals")
    if identity is not None and embedded != validate_identity(identity):
        raise RuntimeError("Candidate app does not match the attested source identity")
    if embedded is None and manifest.get("release_candidate"):
        raise RuntimeError("Candidate app has no source identity")
    bundle_id = RELEASE_BUNDLE_ID if embedded is not None else BUNDLE_ID
    if info.get("CFBundleExecutable") != EXECUTABLE or info.get("CFBundleIdentifier") != bundle_id:
        raise RuntimeError("Local app identity does not match its runtime and project pack")
    if info.get("LSMinimumSystemVersion") != manifest.get("minimum_macos"):
        raise RuntimeError("Local app deployment target differs from its manifest")
    if not info.get("NSLocalNetworkUsageDescription", "").strip():
        raise RuntimeError("Local app is missing its local-network permission explanation")
    if info.get("LSEnvironment") != MACOS_RUNTIME_ENV:
        raise RuntimeError("Local app controller environment must target only Switch Pro")
    minimum_versions = []
    for binary in [app / f"Contents/MacOS/{EXECUTABLE}", *sorted((app / "Contents/Frameworks").glob("*.dylib"))]:
        if run(["lipo", "-archs", binary]).strip() != "arm64":
            raise RuntimeError(f"Unexpected local app architecture: {binary}")
        for dependency in library_dependencies(binary):
            check_dependency(dependency, binary, app)
        minimum_versions.append(minimum_macos(run(["otool", "-l", binary])))
    actual_minimum = max(minimum_versions, key=lambda value: tuple(int(part) for part in value.split(".")))
    if info["LSMinimumSystemVersion"] != actual_minimum:
        raise RuntimeError("Local app deployment version does not match its actual Mach-O requirements")
    run(["codesign", "--verify", "--deep", "--strict", app])
    return manifest


def smoke(app):
    """Exercise bundle/PCK discovery without --path or a repository cwd."""
    logs = WORK / "packaged-validation"
    logs.mkdir(parents=True, exist_ok=True)
    outputs = {}
    # Direct executable launches bypass LaunchServices' LSEnvironment. Exercise
    # the same defaults Finder uses, while retaining the caller's other env.
    runtime_env = plistlib.loads((app / "Contents/Info.plist").read_bytes())["LSEnvironment"]
    for label, arguments in (
        ("native", ["--fixed-fps", "60", "--", "--demo", "--frames=180", "--seed=20260916"]),
        ("ui", ["--", "--ui-self-test"]),
    ):
        engine_log = logs / f"{label}.engine.log"
        engine_log.unlink(missing_ok=True)
        command = [str(app / f"Contents/MacOS/{EXECUTABLE}"), "--headless",
                   "--log-file", str(engine_log), *arguments]
        result = subprocess.run(command, cwd="/private/tmp", text=True, stdout=subprocess.PIPE,
                                stderr=subprocess.STDOUT, timeout=120, env={**os.environ, **runtime_env})
        (logs / f"{label}.console.log").write_text(result.stdout)
        engine_output = engine_log.read_text(errors="replace") if engine_log.exists() else ""
        gate.validate_output(result.stdout + "\n" + engine_output, result.returncode, f"Packaged {label}")
        outputs[label] = {"command": command, "cwd": "/private/tmp", "runtime_environment": runtime_env,
                          "result": gate.validate_report(result.stdout, 180) if label == "native"
                                    else gate.validate_ui_report(result.stdout)}
    return outputs


def write_receipt(app, checks, identity=None):
    (WORK / "app-receipt.json").write_text(json.dumps({
        "status": "passed", "app": str(APP.relative_to(ROOT)), "release_candidate": identity is not None,
        "release_identity": identity,
        "headless_checks": checks,
        "files_sha256": {path.relative_to(app).as_posix(): gate.digest(path)
                         for path in sorted(app.rglob("*")) if path.is_file()},
    }, indent=2) + "\n")


def build(godot, project, identity=None):
    # Only the candidate builder supplies an identity, after its clean-source
    # and tag checks. A payload alone is never publication approval.
    if identity is not None:
        validate_identity(identity)
    if shutil.disk_usage(ROOT / "build").free < 512 * 1024 * 1024:
        raise RuntimeError("Need at least 512 MiB free to assemble the local Godot app")
    dependencies = gate.validate_dependencies()
    staged = gate.validate_staged(project)
    WORK.mkdir(parents=True, exist_ok=True)
    temporary = WORK / (APP.name + ".partial")
    if temporary.exists():
        shutil.rmtree(temporary)
    macos = temporary / "Contents/MacOS"
    frameworks = temporary / "Contents/Frameworks"
    resources = temporary / "Contents/Resources"
    for directory in (macos, frameworks, resources):
        directory.mkdir(parents=True)
    # A single arm64 slice avoids copying the universal editor's unused half.
    engine = macos / EXECUTABLE
    run(["lipo", godot, "-thin", "arm64", "-output", engine])
    engine.chmod(0o755)
    extension = frameworks / "libtanks_sample.dylib"
    shutil.copy2(project / "native/libtanks_sample.dylib", extension)
    outside = [name for name in library_dependencies(extension) if not name.startswith(SYSTEM_PREFIXES)]
    if outside:
        raise RuntimeError(f"Gameplay extension must be engine-independent; found external dependencies: {outside}")
    run(["install_name_tool", "-id", "@rpath/libtanks_sample.dylib", extension])
    run(["codesign", "--force", "--sign", "-", "--timestamp=none", extension])
    shutil.copytree(project / "licenses", resources / "licenses")
    versions = [minimum_macos(run(["otool", "-l", binary])) for binary in (engine, extension)]
    minimum = max(versions, key=lambda value: tuple(int(part) for part in value.split(".")))
    defaults = plistlib.loads((ROOT / "macos/Info.plist").read_bytes())
    info = {"CFBundleDevelopmentRegion": "en", "CFBundleDisplayName": "Tanks 3D — Godot",
            "CFBundleExecutable": EXECUTABLE, "CFBundleIdentifier": RELEASE_BUNDLE_ID if identity else BUNDLE_ID,
            "CFBundleInfoDictionaryVersion": "6.0", "CFBundleName": "Tanks 3D — Godot",
            "CFBundlePackageType": "APPL",
            "CFBundleShortVersionString": identity["version"] if identity else defaults["CFBundleShortVersionString"],
            "CFBundleVersion": identity["build"] if identity else defaults["CFBundleVersion"],
            "LSMinimumSystemVersion": minimum, "LSApplicationCategoryType": "public.app-category.games",
            "NSHighResolutionCapable": True, "NSPrincipalClass": "NSApplication",
            "NSRequiresAquaSystemAppearance": False, "Tanks3DLocalDevelopmentBuild": identity is None,
            "LSEnvironment": MACOS_RUNTIME_ENV,
            "NSLocalNetworkUsageDescription": plistlib.loads((ROOT / "macos/Info.plist").read_bytes())[
                "NSLocalNetworkUsageDescription"]}
    if identity:
        info.update(Tanks3DSourceCommit=identity["commit"], Tanks3DSourceTag=identity["tag"])
    (temporary / "Contents/Info.plist").write_bytes(plistlib.dumps(info))
    entries = pack_entries(project, staged)
    pack_manifest = WORK / "pack-input.json"
    pack_manifest.write_text(json.dumps(entries, indent=2) + "\n")
    pack_script = WORK / "pack_project.gd"
    pack_script.write_text(PACK_SCRIPT)
    engine_log = WORK / "pack.engine.log"
    engine_log.unlink(missing_ok=True)
    command = [str(godot), "--headless", "--path", str(project), "--log-file", str(engine_log),
               "--script", str(pack_script), "--", str(pack_manifest), str(resources / f"{EXECUTABLE}.pck")]
    result = subprocess.run(command, cwd=ROOT, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=120)
    (WORK / "pack.console.log").write_text(result.stdout)
    gate.validate_output(result.stdout + engine_log.read_text(errors="replace"), result.returncode, "PCK packing")
    if f"TANKS_SAMPLE_PACK_CREATED {len(entries)}" not in result.stdout:
        raise RuntimeError("Godot did not confirm completion of the project pack")
    if gate.validate_staged(project) != staged or any(
            gate.digest(Path(entry["source"])) != entry["sha256"] for entry in entries):
        raise RuntimeError("Project resources changed while packaging; rebuild from a stable staged project")
    manifest = {"schema": "tanks3d-local-godot-app-v1", "release_candidate": identity is not None,
                "release_identity": identity,
                "notarized": False, "architecture": "arm64", "minimum_macos": minimum,
                "dependencies": dependencies, "staged_sha256": staged,
                "engine_source_sha256": gate.digest(godot),
                "runtime_integrity": "codesign resource/code seals; final byte hash in external local receipt",
                "pack_entries_sha256": {entry["target"]: entry["sha256"] for entry in entries},
                "files_sha256": expected_files(temporary)}
    (resources / "package-manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    run(["codesign", "--force", "--sign", "-", "--timestamp=none", temporary])
    verify(temporary, identity)
    backup = WORK / (APP.name + ".previous")
    if backup.exists():
        shutil.rmtree(backup)
    if APP.exists():
        previous_info = plistlib.loads((APP / "Contents/Info.plist").read_bytes())
        if not previous_info.get("Tanks3DLocalDevelopmentBuild"):
            raise RuntimeError("Refusing to replace an app not created by this local packager")
        APP.rename(backup)
    try:
        temporary.rename(APP)
        checks = smoke(APP)
        verify(APP, identity)
        write_receipt(APP, checks, identity)
    except (OSError, RuntimeError, ValueError, subprocess.SubprocessError):
        if APP.exists():
            shutil.rmtree(APP)
        if backup.exists():
            backup.rename(APP)
        raise
    if backup.exists():
        shutil.rmtree(backup)
    label = "Candidate" if identity else "Local"
    print(f"{label} app assembled and verified: {APP}\nOpen with: open {APP}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--verify-only", action="store_true")
    parser.add_argument("--smoke", action="store_true", help="Also rerun the packaged native/UI headless checks when verifying")
    parser.add_argument("--godot", type=Path, default=ROOT / "build/godot-tools/Godot.app/Contents/MacOS/Godot")
    parser.add_argument("--project", type=Path, default=ROOT / "build/godot/project")
    args = parser.parse_args()
    if args.verify_only:
        verify(APP)
        receipt = json.loads((WORK / "app-receipt.json").read_text())
        current = {path.relative_to(APP).as_posix(): gate.digest(path)
                   for path in sorted(APP.rglob("*")) if path.is_file()}
        if current != receipt["files_sha256"]:
            raise RuntimeError("Local app differs from its final signed-build receipt")
        if args.smoke:
            smoke(APP)
        print("Local Godot app files, architecture, signatures and dependency paths passed")
    else:
        project = args.project.resolve()
        if not project.is_relative_to(ROOT / "build"):
            parser.error("The source must be a staged project inside build/")
        build(args.godot.resolve(), project)


if __name__ == "__main__":
    try:
        main()
    except (OSError, RuntimeError, ValueError, subprocess.SubprocessError) as error:
        raise SystemExit(str(error)) from error
