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
horizontal and 50° elevation. The minimum orthographic view spans 18.5 world
units vertically, about 19% wider than the previous 15.5-unit view at the same
angle. Tank movement remains in four map directions; the gamepad left stick
follows the selected view. The camera follows the solo tank or the co-op
midpoint even at map edges. Co-op expands beyond the minimum to keep both
players visible, including uncapped expansion in narrow portrait windows.
At large spans the camera retreats along the same viewing axis so its near
plane cannot cut through foreground terrain; the normal 18.5-unit view keeps
its original camera position.

**Pixel Style** in Advanced Settings switches the gameplay scene between the
full-detail view (**OFF**, the default) and a crisp pixel grid (**ON**). Use left,
right or the confirm button to toggle it. Reset restores OFF. The choice stays
active while returning to setup, restarting or advancing stages in the current
app session. HUD text and menus remain sharp in either mode.

For a quick two-player stage 10 preview after building:

```sh
./build/Tanks3D --stage=10 --camera-yaw=45 --camera-elevation=70 --quick-start-2p
```

## LAN development

The main menu's **LOCAL NETWORK** entry starts IPv4 two-computer co-op.
`--lan-host[=port]` and `--lan-join=IPv4[:port]` expose the same connection flow
from the command line. Both peers must use the same executable build.
See [LAN play and verification](LAN_PLAY.md) for controls and limitations.

The default suite includes socket-free LAN protocol/session checks. Run
`make test-lan-sockets` for two real processes over loopback TCP and
`make test-lan-sockets-sanitize` for its fully instrumented version. These
focused targets require local socket permission but no graphics context.
`NSLocalNetworkUsageDescription` is checked in the normal app-bundle gate.

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

`make test-tank-drawing` checks legacy and explicit-national drawing calls by
recording their complete geometry commands without opening a GPU window.
It runs with `make test` and coverage; `make test-sanitize` also runs it and
the standalone nation-selection boundary cases under ASan/UBSan. Coverage keeps
the drawing test's profile separate and prints a second report for the two
rendering headers, including legacy overloads unused by the main executable.
These supplementary counts are not merged into the main production report.

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

## Local AI teammate

`make run-app` opens setup. Cycle **PLAYERS** to **AI AS P2** and press Enter.
P1 keeps the normal keyboard/controller controls, and P2 uses the native port
of the selected `TacticalDefender` v1 rules with the headquarters firing guard.
P2's nation, starting stage, lives and advanced rules use the existing menu.
To skip setup for a local smoke test:

```sh
make all
./build/Tanks3D --quick-start-ai
```

The controller lives in `src/app/ai_player.{h,cpp}`. It reads const game state,
plans at 20 Hz and supplies only P2 command input to the existing elapsed-time
simulation. Intro, pause, respawn and report screens suspend decisions; stage
changes and restarts reset its history. LAN sessions remain human/human.

`make test` includes native menu/controller and production-world regressions.
`make test-ai` also checks observation/action parity against the Python rule
reference on all 35 maps and complete seeded episodes. `make test-sanitize`
includes the native AI controller. Physical controller and human cooperation
acceptance still require interactive play; AI/AI clears are not that acceptance.

## Training an AI player

[AI_TRAINING.md](AI_TRAINING.md) documents the optional C++/Gymnasium adapter,
scripted baseline, imitation initialization, PPO training, evaluation and native
3D recordings. Use `make ai-setup`, `make ai-native`, then `make test-ai`.
The app does not depend on Python or ship any ML library. Generated checkpoints,
training datasets, traces and recordings stay under `build/`.
