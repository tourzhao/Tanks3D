#!/usr/bin/env python3
"""Fetch hash-pinned free dependencies and build the optional macOS sample SDK.

No system install, global pip change, engine source upload or paid service is used.
Tools, editor settings/cache and compilation outputs stay in build/. Godot's
project user:// location still follows macOS; self-contained mode only affects
the editor's own data, not per-game saves.
"""

import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import platform
import shutil
import subprocess
import sys
import tarfile
import tempfile
import urllib.request
import venv
import zipfile


ROOT = Path(__file__).resolve().parents[1]
CACHE = ROOT / "build/godot-tools"


def sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def fetch(dependency):
    destination = CACHE / dependency["archive"]
    expected = dependency["sha256"]
    if destination.exists():
        if sha256(destination) != expected:
            raise RuntimeError(f"Cached dependency hash mismatch: {destination}; remove it to retry")
        return destination
    partial = destination.with_suffix(destination.suffix + ".partial")
    print(f"Downloading {dependency['name']} {dependency['version']}", flush=True)
    request = urllib.request.Request(dependency["url"], headers={"User-Agent": "Tanks3D-local-sample"})
    try:
        with urllib.request.urlopen(request, timeout=90) as source, partial.open("wb") as output:
            shutil.copyfileobj(source, output)
        if sha256(partial) != expected:
            raise RuntimeError(f"Downloaded dependency hash mismatch: {dependency['name']}")
        partial.replace(destination)
    finally:
        partial.unlink(missing_ok=True)
    return destination


def safe_name(name):
    path = PurePosixPath(name)
    if path.is_absolute() or ".." in path.parts:
        raise RuntimeError(f"Unsafe dependency archive path: {name}")


def unpack(dependency, archive, target, kind):
    marker = CACHE / (target.name + ".sha256")
    if target.exists() and marker.exists() and marker.read_text().strip() == dependency["sha256"]:
        return
    with tempfile.TemporaryDirectory(prefix="extract-", dir=CACHE) as temporary:
        stage = Path(temporary)
        if kind == "zip":
            with zipfile.ZipFile(archive) as package:
                for member in package.infolist():
                    safe_name(member.filename)
            subprocess.run(["ditto", "-x", "-k", str(archive), str(stage)], check=True)
            extracted = stage / "Godot.app"
        else:
            with tarfile.open(archive) as package:
                members = package.getmembers()
                for member in members:
                    safe_name(member.name)
                    if not (member.isfile() or member.isdir()):
                        raise RuntimeError(f"Unsupported dependency archive entry: {member.name}")
                package.extractall(stage, members=members)
            entries = list(stage.iterdir())
            if len(entries) != 1 or not entries[0].is_dir():
                raise RuntimeError("Expected one root in the godot-cpp archive")
            extracted = entries[0]
        if not extracted.is_dir():
            raise RuntimeError(f"Dependency archive is missing {target.name}")
        if target.exists():
            shutil.rmtree(target)
        extracted.rename(target)
        marker.write_text(dependency["sha256"] + "\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--skip-build", action="store_true", help="Prepare tools without compiling C++ bindings")
    parser.add_argument("--jobs", type=int, default=min(os.cpu_count() or 2, 6))
    args = parser.parse_args()
    if sys.platform != "darwin" or platform.machine() != "arm64":
        parser.error("This first sample currently targets macOS Apple Silicon only")
    if args.jobs < 1:
        parser.error("--jobs must be positive")
    CACHE.mkdir(parents=True, exist_ok=True)
    dependencies = json.loads((ROOT / "godot/DEPENDENCIES.json").read_text())["dependencies"]
    archives = {key: fetch(value) for key, value in dependencies.items()}
    unpack(dependencies["godot"], archives["godot"], CACHE / "Godot.app", "zip")
    unpack(dependencies["godot_cpp"], archives["godot_cpp"], CACHE / "godot-cpp", "tar")
    # Godot's macOS EditorPaths checks next to the .app bundle. This keeps
    # editor settings, documentation cache and export-template data in build/.
    # https://docs.godotengine.org/en/stable/classes/class_editorpaths.html
    (CACHE / "_sc_").touch()

    python = CACHE / "venv/bin/python"
    if not python.exists():
        venv.create(CACHE / "venv", with_pip=True)
    installed = subprocess.run(
        [str(python), "-c", "import importlib.metadata; print(importlib.metadata.version('SCons'))"],
        capture_output=True, text=True,
    )
    if installed.returncode != 0 or installed.stdout.strip() != dependencies["scons"]["version"]:
        subprocess.run(
            [str(python), "-m", "pip", "install", "--no-index", "--no-deps", "--no-cache-dir", str(archives["scons"])],
            check=True,
        )
    subprocess.run([str(CACHE / "Godot.app/Contents/MacOS/Godot"), "--version"], check=True)
    subprocess.run([str(python), "-m", "SCons", "--version"], check=True)
    if not args.skip_build:
        subprocess.run(
            [str(python), "-m", "SCons", "-f", "godot/SConstruct", f"-j{args.jobs}"],
            cwd=ROOT, check=True,
        )
    print(f"Godot sample tools ready: {CACHE}")


if __name__ == "__main__":
    try:
        main()
    except (OSError, RuntimeError, subprocess.CalledProcessError) as error:
        raise SystemExit(str(error)) from error
