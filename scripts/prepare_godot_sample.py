#!/usr/bin/env python3
"""Stage the optional renderer sample without generated files in source folders."""
from pathlib import Path
import json
import shutil

ROOT = Path(__file__).resolve().parents[1]
TARGET = ROOT / "build/godot/project"


def source_files():
    """Return the exact managed resource mapping, shared by staging and checks."""
    result = {}
    for origin, prefix in [(ROOT / "godot/sample", Path()),
                           (ROOT / "resources/sounds", Path("resources/sounds")),
                           (ROOT / "resources/textures", Path("resources/textures")),
                           (ROOT / "resources/fonts", Path("resources/fonts")),
                           (ROOT / "godot/licenses", Path("licenses")),
                           (ROOT / "LICENSES", Path("licenses/LICENSES"))]:
        for path in sorted(origin.rglob("*")):
            if path.is_file():
                if path.is_symlink() or ".godot" in path.parts or "__pycache__" in path.parts:
                    raise RuntimeError(f"Unexpected generated or linked source resource: {path}")
                relative = (prefix / path.relative_to(origin)).as_posix()
                if relative in result:
                    raise RuntimeError(f"Duplicate staged resource mapping: {relative}")
                result[relative] = path
    result["native/libtanks_sample.dylib"] = ROOT / "build/godot/libtanks_sample.dylib"
    for name in ["LICENSE", "ASSET_LICENSES.md", "THIRD_PARTY_NOTICES.md"]:
        result["licenses/" + name] = ROOT / name
    return result


def same_content(source, destination):
    if not destination.is_file() or source.stat().st_size != destination.stat().st_size:
        return False
    with source.open("rb") as original, destination.open("rb") as staged:
        while True:
            block = original.read(1024 * 1024)
            if block != staged.read(1024 * 1024):
                return False
            if not block:
                return True


def main():
    files = source_files()
    missing = [str(source) for source in files.values() if not source.is_file()]
    if missing:
        raise RuntimeError("Missing sample inputs: " + ", ".join(missing))
    if TARGET.is_symlink():
        raise RuntimeError("Staged project must not be a symlink")
    TARGET.mkdir(parents=True, exist_ok=True)
    previous_manifest = ROOT / "build/godot/staged-files.json"
    previous = json.loads(previous_manifest.read_text()) if previous_manifest.exists() else []
    for relative in set(previous) - set(files):
        stale = (TARGET / relative).resolve()
        if not stale.is_relative_to(TARGET):
            raise RuntimeError("Staging manifest path escaped the generated project")
        stale.unlink(missing_ok=True)
        Path(str(stale) + ".import").unlink(missing_ok=True)
        Path(str(stale) + ".uid").unlink(missing_ok=True)
    for relative, source in files.items():
        destination = TARGET / relative
        if source.is_symlink() or any(path.is_symlink() for path in [destination, *destination.parents]
                                      if path.is_relative_to(TARGET)):
            raise RuntimeError(f"Refusing linked staging source/destination: {relative}")
        destination.parent.mkdir(parents=True, exist_ok=True)
        # Compare bytes rather than timestamps: git checkouts can change source
        # mtimes, and an edited resource can retain its original size/mtime.
        if not same_content(source, destination):
            shutil.copy2(source, destination)
    previous_manifest.write_text(json.dumps(sorted(files), indent=2) + "\n")
    print(f"Godot sample staged at {TARGET}")


if __name__ == "__main__":
    main()
