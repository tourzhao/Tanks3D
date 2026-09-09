# Development Guide

This page is for contributors. Players should start with the
[project README](../README.md).

## Current main

`main` includes the arcade model upgrade: 12 player tank models with substantial
turrets and short thick cannons, four enemy roles, remodeled buildings and
ruins, three compact national headquarters, vegetation and painted lighting.
The terrain follows the original 35 Battle City stages. Each headquarters fits
the original two-by-two base and eight-cell Π enclosure without clearing
surrounding map tiles. See the
[current main preview](../README.md#current-main-preview) and
[art direction](ART_DIRECTION.md).

The published
[Alpha 4 download](https://github.com/tourzhao/Tanks3D/releases/tag/v0.1.0-alpha.4)
predates this artwork and the adjustable camera. Build the current source to
try these changes; the versioned Alpha 4 screenshots document that release.

## Build and run

Tanks 3D requires macOS, a C++17 compiler, Homebrew, and raylib 6.0.
From the repository root:

```sh
brew install raylib
make run-app
```

`make run-app` builds `build/Tanks3D` and `build/Tanks3D.app`, then opens the
application bundle. Use `make all` to build without launching, or `make run`
to launch the executable from the terminal. If raylib is outside Homebrew's
active prefix, pass `RAYLIB_PREFIX=/path/to/raylib` to `make`.

In **Advanced Settings**, **View Horizontal** selects -45° to +45° and
**View Elevation** selects 40° to 70°, both in 5° steps. The default is 0°
horizontal and 50° elevation. Tank movement remains in four map directions;
the gamepad left stick follows the selected view. The shared co-op camera
tracks both players and expands to keep them visible.

For a quick two-player stage 10 preview after building:

```sh
./build/Tanks3D --stage=10 --camera-yaw=45 --camera-elevation=70 --quick-start-2p
```

## Test

```sh
make clean
make test
make test-sanitize
```

Run a clean full suite after gameplay, collision, resource, or build changes,
and sanitizers before submitting refactors. Focused targets include
`test-unit`, `test-session`, `test-assets`, `test-bundle` and
`test-release-status`; `make coverage` reports production coverage. Visual
changes also require checking menus, one/two-player controls, pause, pickups,
stage completion and the battle report.

Keep generated screenshots, binaries and reports under `build/`. `make clean`
preserves `build/release/` and `build/release-evidence/`; other build outputs
are disposable.

## Project map

- `src/core/` contains shared, raylib-free values and settings.
- `src/game/` contains gameplay rules and systems.
- `src/app/` and `src/audio/` contain presentation and audio boundaries.
- `src/wwii_tank_model.h`, `src/environment_assets.h` and `src/base_model.h`
  define the procedural vehicle, battlefield and headquarters geometry.
- `src/main.cpp` integrates rendering, cameras and menus; `src/post_process.h`
  applies the final image treatment.
- `resources/` contains runtime models, textures and sounds; the original
  35 Battle City stage layouts and their loading logic live in `src/game/`.
  Stage selection wraps after 35. Procedural art decorates the selected layout
  without replacing its roads, obstacles or base footprint.
- `tests/` contains deterministic unit, integration, and release-tooling checks.
- `macos/` contains application-bundle metadata.

## Reference documents

- [Repository guidelines](../AGENTS.md) — style, naming, tests, and pull requests.
- [Architecture](ARCHITECTURE.md) — module boundaries and current risks.
- [Art direction](ART_DIRECTION.md) — current models, materials and visual
  contracts.
- [Gameplay invariants](GAMEPLAY_INVARIANTS.md) — rules that refactors must keep.
- [Refactoring plan](REFACTORING_PLAN.md) — incremental modernization sequence.
- [Coverage baseline](COVERAGE_BASELINE.md) — deterministic coverage priorities.
- [Release checklist](RELEASE_CHECKLIST.md) — packaging, QA, evidence, and approval.

`make dist` creates and verifies a self-contained macOS Alpha archive. Follow
the release checklist before publishing, and never move an attested release tag
to accommodate later documentation changes.

Project-owned contributions must remain compatible with the
[PolyForm Noncommercial License 1.0.0](../LICENSE). Record the source, author,
license, and exact in-game mapping for every new asset; see
[asset licenses](../ASSET_LICENSES.md) and
[third-party notices](../THIRD_PARTY_NOTICES.md).
