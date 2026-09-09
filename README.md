# Tanks 3D

Arcade tank combat on destructible 3D battlefields for macOS. Defend your
headquarters, upgrade through three national tank lines, and play solo or
local co-op across the 35 original Battle City battlefield layouts.

## Current main preview

**These screenshots show the current `main` source build.** The new tank and
building models, restored Battle City maps, adjustable camera and updated controller input are newer than
the downloadable Alpha 4 release. [Build the current version](#build-and-play-current-main)
to play with the visuals shown here.

![Current main: two-player battlefield with rebuilt tanks, buildings and headquarters](build/release-evidence/main-showcase-20260908/gameplay-coop.png)

*Local two-player play on original stage 1, with the camera rotated left 45° and set to
45° elevation. Captured directly from the game.*

## Highlights

- Chunky arcade machinery with continuous tracks, layered armor, distinctive
  turrets and painted materials, inspired by the military adventure art of
  1990s arcade games.
- 12 player tanks across United States, Soviet and German light-to-super-heavy
  lines, plus four enemy roles.
- Destructible buildings with tiled roofs, shop awnings and exposed ruins;
  three national field headquarters, forest cover and steel fortifications.
- All 35 original Battle City maps in their original order, with full-map
  radar, nine pickups, upgrades, HP, streaks and shell cancellation.
- Adjustable camera rotation (-45° to +45°) and elevation (40° to 70°) in
  5-degree steps. Local co-op shares a camera that follows both players.

![Current main: detailed T28/T95 and IS-2 tanks in the two-player battle report](build/release-evidence/main-showcase-20260908/battle-report.png)

*Battle-report preview using the game's built-in showcase. Tank geometry is
original procedural work; see the [art direction](docs/ART_DIRECTION.md).*

[View a current single-player screenshot](build/release-evidence/main-showcase-20260908/gameplay-solo.png)
· [Screenshot sources and capture commands](build/release-evidence/main-showcase-20260908/README.md)

## Build and play current main

On macOS, with Homebrew and the Xcode Command Line Tools installed:

```sh
git clone https://github.com/tourzhao/Tanks3D.git
cd Tanks3D
brew install raylib
make run-app
```

The project uses C++17 and raylib 6.0. `make run-app` builds and opens
`build/Tanks3D.app`; `make run` starts the executable in the terminal. See the
[development guide](docs/DEVELOPMENT.md) for prerequisites, tests and project layout.

## Download Alpha 4

**[Download Tanks 3D Alpha 4 for Apple Silicon](https://github.com/tourzhao/Tanks3D/releases/download/v0.1.0-alpha.4/Tanks3D-0.1.0-alpha.4-macos-arm64-macos26.0.zip)**

This published build predates the current visual and camera upgrades. For its
original screenshots and features, see the [Alpha 4 release notes](docs/releases/v0.1.0-alpha.4.md).

Requires **macOS 26.0 or later**. Extract the ZIP, right-click `Tanks3D.app`,
and choose **Open**. If macOS blocks it, use
**System Settings > Privacy & Security > Open Anyway**.

Alpha 4 is an early pre-release and is not Apple notarized.

## Controls in current main

- **P1:** Arrow keys; `Space`/Right Option/Right Control fires.
- **P2:** `WASD`; `F`/Left Option/Left Control fires.
- **Controller:** In battle, the left stick follows the visible map lanes at
  the selected camera angle. The D-pad and keyboard retain one button per
  world-cardinal lane; menu input is unrotated. `B`/`Y`/`R`/`ZR` fires;
  `+` starts or pauses; `-` returns. ABXY never exits the game.
- `Enter` pauses · `Esc` returns to setup · `R` restarts.
- **Advanced Settings:** `View Horizontal` and `View Elevation` adjust the
  camera. Default: 0° horizontal, 50° elevation.

## More

[Report a bug](https://github.com/tourzhao/Tanks3D/issues) ·
[Development guide](docs/DEVELOPMENT.md) ·
[Art direction](docs/ART_DIRECTION.md) ·
[Map sources and verification](docs/BATTLE_CITY_MAPS.md) ·
[License](LICENSE) · [Third-party notices](THIRD_PARTY_NOTICES.md)

Independent, non-commercial fan project. Not affiliated with or endorsed by
any game publisher, vehicle manufacturer, government, or other rights holder.
