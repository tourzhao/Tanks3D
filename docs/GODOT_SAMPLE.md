# Godot / Metal frontend

Godot is the sole active development frontend, as confirmed on September 27,
2026. This macOS arm64 version runs the engine-independent C++ game through
Godot's native Metal renderer. It includes deployment menus, local and AI co-op,
LAN controls, radar, battle reports and an authored arcade art roster. Raylib is
retained as a frozen historical reference, without routine feature/art updates.
The unqualified Makefile targets still select raylib; use the explicit Godot
commands below until those entry points are migrated. This development decision
does not constitute a published replacement release.

## Build and play

Use Apple Silicon, Xcode Command Line Tools and Python 3. Godot-only builds
need no Homebrew raylib. The extension and godot-cpp target macOS 13.0, matching
[Godot's Metal requirement](https://docs.godotengine.org/en/stable/about/system_requirements.html).
Runtime QA includes development M2 and M5 Pro machines, not macOS 13 or every
supported Mac.
From the repository root:

```sh
make godot-setup
make run-godot
```

Setup downloads pinned, free tools into `build/godot-tools/`, verifies SHA256
hashes, and builds godot-cpp. No paid service, global Python installation or
project upload is involved. The first build takes longer than cached builds.
See [toolchain details](../godot/TOOLCHAIN.md) for versions, caches, macOS
application-support access and retained third-party notices.

`godot-sample` and the play targets stage changed resources and run the editor
import only when the engine, staged content or generated import products have
changed. Unchanged staged files keep their timestamps. These preparation steps
do not run the regression suite or create a validation result. Run
`make test-godot-import` for the full script, scene, art, camera, audio and
native/UI checks; it always performs a fresh import and executes every check.
The script's `--prepare-only` selects preparation, while the existing
`--import-only` still runs all presentation contracts and omits only the final
native/UI gameplay smoke checks.

The default is the setup menu, using Mobile rendering through native Metal.
Normal clear rendering fits the window's actual pixels at 16:9, up to
1920×1080; Pixel Style keeps a 1280×720 ceiling. The minimum internal size is
640×360. Resizing or changing Pixel Style updates the viewport and redraws a
frozen menu background once. Diagnostic captures, benchmarks and fixed-frame
runs retain their original 1280×720 default; `--render-size=WIDTHxHEIGHT`
explicitly fixes a diagnostic size (16:9, up to 3840×2160).

A new interactive profile starts with **1 PLAYER**, matching the raylib setup;
existing saved crew preferences are retained. **2 PLAYERS** and **AI AS P2**
remain available. Setup offers nations, stages 1–35, and 1–99 lives per player.
The separate Advanced page exposes HP 1–6, enemy speed/fire/spawn adjustments
from −30% to +30% in 5% steps, camera yaw −45° to +45°, elevation 40°–70° in
5° steps, Pixel Style, and the Godot-only volume setting. Preferences are saved to
`user://deployment.cfg`, under the platform application-support directory for
`Tanks3D-Godot`. High scores currently persist within a running native session,
not across application launches.

To compare Forward+ with screen-space ambient occlusion:

```sh
make run-godot GODOT_RENDERER=forward_plus
```

Other entry points:

```sh
make run-godot GODOT_ARGS="--solo"
make run-godot GODOT_ARGS="--human-2p --stage=10 --pixel"
make run-godot GODOT_RENDERER=forward_plus GODOT_ARGS="--quick-start --stage=10"
```

`--solo` and `--human-2p` start immediately; `--quick-start` starts the AI crew
without opening the menu. Normal play uses the deployment menu and its saved
preferences. The existing raylib game still starts with `make run-app`.

For an independently runnable local app:

```sh
make godot-app
make run-godot-app
make test-godot-bundle
```

`build/Tanks3D-Godot.app` contains an arm64 slice of the cached official Godot
editor executable, a project PCK and the engine-independent native extension.
It contains no raylib library and runs without Homebrew or the staged project.
Package size and actual Mach-O minimum OS requirements are recorded in each
build receipt. This is an ad-hoc signed local development app, not a notarized
release. No export-template download is needed. The default `Tanks3D.app` is
separate and unchanged by these targets.

Development staging keeps every managed source and diagnostic. The production
PCK omits the unused `urban_masonry.png`, `art_review.gd` and the 11 standalone
check scripts, together with their sidecars and unused import products. It
retains `report_preview.gd` and the UI smoke scripts loaded by `main.gd`, as well
as the grass, font and audio imports. Audio and grass resolve through their
validated `.import` remaps; duplicate raw OGG/grass PNG payloads stay in development
staging rather than the PCK. Both original bitmap font files remain packaged.
All original notices remain in
`Contents/Resources/licenses/`; their duplicate PCK copies are omitted. Source
hash checks and candidate notice verification still use the full staging map.

For a clean tagged source snapshot, `make godot-candidate` now builds an
immutable Alpha candidate and records its regression gates and package hashes.
See [Godot release acceptance](GODOT_RELEASE.md) for candidate verification and
the separate, candidate-specific human/hardware QA required before publication.

| Control | Action |
| --- | --- |
| Arrow keys / Space, right Ctrl or right Alt | P1 movement / fire |
| WASD / F, left Ctrl or left Alt | P2 movement / fire in local two-human mode |
| Enter | Pause / resume during battle; count or continue the battle report |
| Escape | Return to setup; leave an active LAN room |
| Tab | Pixel Style on / off; HUD stays clear |
| R | Restart the current stage through the native rules, preserving the session RNG stream |
| F11 | Toggle fullscreen |

During battle, Godot supplies controller samples to the existing native pad
mapper, preserving its dead zone, dominant-axis hysteresis, D-pad priority,
camera-relative steering and button edges. Left stick or D-pad moves; A, X, right shoulder or right trigger
fires; Start pauses; bottom/right face confirms reports; Back returns to deployment.
Godot's face-button names describe positions: its A is **B on a Switch Pro**.
Both bottom and right face buttons now confirm menus, pause and reports, so
**Switch A or B** works; neither face button cancels or quits. Resume activates
on press, without waiting for release. Fire bindings remain bottom/left face,
right shoulder and right trigger (Switch B/Y/R/ZR).

On macOS, the packaged app and `make run-godot` prefer Apple's GameController
backend for the original Switch Pro (`057e:2009`). They set
[`SDL_HIDAPI_IGNORE_DEVICES`](https://wiki.libsdl.org/SDL3/SDL_HINT_HIDAPI_IGNORE_DEVICES)
to `0x057e/0x2009`, excluding just this device from SDL's raw HID backend;
Godot's built-in MFi backend still receives its input. Other controller models
keep their usual backend selection. The `.app` carries this process-local
default in `Info.plist`'s `LSEnvironment`; no system Bluetooth setting changes.
Connection notifications refresh the menu's gamepad count and discard stale
per-device stick state, including discovery that completes after menu setup.

For driver comparison, `SDL_HIDAPI_IGNORE_DEVICES= make run-godot` restores
SDL's normal device selection for that launch. Direct execution of the binary
inside the bundle bypasses `LSEnvironment`: supply the same environment variable
to reproduce a normal Finder launch. Native fullscreen (F11) also makes the
game eligible for [macOS Game Mode](https://support.apple.com/en-us/105118) on
supported Macs. Human feedback on an M2 with Switch Pro over Bluetooth found
the system backend more responsive; it is not a measured button-to-display
latency guarantee. USB, reconnects and other controllers still need hardware
acceptance for each release candidate.

The frontend disables input accumulation and flushes pending events before its
frame's menu guard and native input sample. Buffered move/turn/fire taps are
checked against the very next native update, without extra simulation steps or
changes to movement speed, dead zone, fire cooldown, camera or rendering queues.
This checks software dispatch, not physical button-to-display latency: Bluetooth
and USB latency still require hardware comparison. Background GUI/gamepad input
remains blocked; controls restore when focus returns. Native session/network
ticking is unchanged by the focus guard.
Player turning preserves small movements instead of rounding every turn back
onto a grid lane. If a wall corner blocks forward movement, nearby lane
alignment is attempted only when both its origin and destination are clear;
held directions retry at narrow entries. The 5/16-tile assistance limit,
collision footprint, movement speeds, and 0.380-second ice momentum remain.
Keyboard, D-pad, stick, and both local player slots share this native rule.
There is no F1 crew shortcut; choose the crew in setup.

The setup, Advanced and Local Network pages follow the raylib row-selection
workflow. Up/down selects a row; left/right changes its value. Shift or a
shoulder button changes stage/lives by ten. Keys **1** and **2** select solo or
two human players. Enter, Space or either bottom/right face button starts the
game from an ordinary setup row, or opens the selected submenu. **Start / +**
deploys directly from setup or Advanced, regardless of the focused row. In the
Local Network page Start still activates its selected row; it cannot launch an
offline battle over a pending connection.
Escape/Back returns from a submenu; at the main setup it quits. Advanced uses
**R** or the top face button to reset gameplay/view/Pixel settings; it keeps
volume unchanged. The selected row has gold chevrons, and setup shows the four
vehicle tiers for each selected nation. Stage selection wraps between 35 and 1;
lives and Advanced numeric settings stop at their limits.

Battle reports have a separate screen with the existing player models in a
presentation-only preview, a per-player TYPE/K.O./POINTS table, total kills,
combat points, bonus points, stage points, score and lives. Kill counts and the
score counter follow the native settlement sequence; the per-type points are
the native final tally, as in raylib. A new record has its own HISCORE screen.
These screens do not alter the battle camera or native state.

For LAN, open **LOCAL NETWORK**. The host chooses the stage/rules and creates a
room; the guest enters the host's displayed IPv4 address and matching port
(default 41987), chooses its own nation using **P1 NATION** in setup, then
joins as network P2. This matches raylib's “YOUR NATION” workflow. The host
uses the same setup nation for network P1. Both machines need the same
built Godot app. LAN uses two human players and the existing native lockstep
session; mixed raylib/Godot builds are not a supported pairing. Each peer uses
the P1 keyboard/controller bindings for its local tank. The Host IP row also
accepts `IPv4:port`; Enter while editing joins, and Escape cancels editing.
Escape cancels a pending connection or leaves an active room; Enter pauses or
resumes the battle. Only the host can restart the stage. Real
two-Mac play and local-network permission prompts still need manual validation.

## Implementation boundary

- `src/app/game_session.h` owns the shared command-driven simulation,
  seeded randomness, session high score, settlement and engine-neutral camera
  values. Raylib's `Game3D` derives from it and supplies effects and rendering.
  The shared implementation has no raylib include or link dependency.
- `src/godot/sample_core.{h,cpp}` exposes a C ABI for configuration, session
  starts/restarts, commands, stepping, native pad mapping, LAN and snapshots.
  It uses `GameSession`, `AiPlayerController` and `LanSession` directly, without
  including `main.cpp`. Dedicated Godot objects link independently of raylib.
  Its ordered `AudioOutput` queue is drained separately from deterministic
  snapshots, events and RNG state.
- `godot/native/extension.cpp` wraps that ABI as a GDExtension `RefCounted`
  class.
- `godot/sample/main.gd` presents snapshots, routes input and game events,
  follows the native camera, and handles pause/report/restart/network flows.
  Terrain processing is skipped when map rows, damage masks, base wall health
  and visible steel state are unchanged. Each presented battle snapshot requests
  one 3D frame; the static deployment menu reuses the last texture. Paused battles
  still render each frame.
- `godot/sample/audio_bank.gd` consumes native play/stop/engine requests for all
  22 existing recordings. Recordings are preloaded; players are created on first
  use and retained for reuse. It preserves cue gains, 12-voice limits for overlapping
  effects, overlap attenuation, stage/record/respawn jingle priority, exclusive
  idle/moving engines and pause-entry silence. Resuming does not replay Pause;
  report counting, boundary hits and new records use their native cues. Volume
  zero truly mutes. Menu selection is owned by the frontend. Asset licenses and
  recording bytes are unchanged.
- `godot/sample/frontend.gd` owns the separate setup/Advanced/LAN pages,
  combat HUD, pause and settlement controls. `arcade_menu_row.gd` draws the
  selectable rows; `arcade_backdrop.gd` draws the dark-blue gradient and diagonal
  lines. Both reuse the exported raylib default bitmap font in
  `resources/fonts/arcade.fnt` and `arcade.png`, with nearest texture filtering.
  `scripts/export_arcade_font.cpp` reproduces the atlas and metrics from raylib
  6.0; the glyphs retain their zlib license, recorded in
  [ASSET_LICENSES.md](../ASSET_LICENSES.md#shared-arcade-bitmap-font).
- Both combat HUDs use three paired rows: player/nation and lives, vehicle and
  HP, then tier and score. Plain translucent black rectangles occupy
  the upper corners; HP text uses the original green/amber/red/gray severity
  colors, and P1 gold/P2 green identify the players. At 1280×720, each panel is
  320×80 pixels, or 320×102 with an optional streak/player-state row.
  Text fitting retains the full score and model name. The upper center remains
  clear. Stage, enemy total and base-steel status appear once above the shared
  lower-right radar, whose 130×130 map uses integer five-pixel cells.
  The transparent `hud_root` stays 74 pixels tall: taller corner panels do not
  enlarge the camera safety band or change native/presentation camera settings.
  `arcade_plate.gd` continues to draw mission and report plates.
  `radar.gd` reads native brick masks, headquarters walls, pickups, spawning
  enemies and bonus carriers for the minimap; it does not infer gameplay rules.
- `report_preview.gd` displays the existing tank roster in a separate World3D
  and SubViewport. It caches the player nation/id/level selection and requests
  a frame when the lineup or layout changes. It does not move gameplay actors,
  change their transforms or replace the normal gameplay camera.
- `godot/sample/art.gd` provides newly authored profiles for all national player
  tiers and enemy roles, including the German wheeled scout. It also builds
  connected buildings, damaged cells, national headquarters, forest, water,
  nine distinct pickups, flotation equipment and combat effects. Armor, rubber,
  exposed metal and player identity markings remain separate. See the exact
  roster and geometry mapping in [Godot art](GODOT_ART.md). Running gear shares
  prebuilt wheel/tread phases driven by actual displacement and turns, while
  preserving the existing suspension. Broader shoes and fewer, wider wheel
  spokes reduce repeated pattern frequency without slowing the wheel rotation.
  Restart and stage boundaries explicitly
  reset its presentation history, including when the same model node survives.
- `godot/sample/pixel.gdshader` applies a small scene pixel grid and color
  quantization after 3D rendering. Contrasting source texels preserve narrow
  cannon and track edges; quiet areas keep the two-pixel blocks. Pixel OFF
  retains its full-detail path, and text is drawn separately afterward.
- Firing uses a brief directional muzzle jet and a bright-nosed shell with a
  short rear tail. The jet copies the animated gun's orientation at the event,
  then stays at that firing position. Small impacts reuse the same bounded pool
  and restore their original mesh, orientation and upward drift. Native shell
  spawn, event order, ballistics, damage and effect lifetimes are unchanged.
- `godot/sample/player_visibility.gd` provides a restrained gold/cyan hint on
  building-obscured player fragments. Its depth shader uses a half-cell brick
  mask and actual building height bounds; it does not fade buildings or reveal
  enemies. Forest cover or a foreground canopy overlap suppresses the whole
  hint immediately, as do hidden/creating players. Copies share the existing
  rigid meshes, follow their current poses and never change camera bounds,
  shadows, source materials or native state. A cool muted interior and lighter,
  narrower team-colored rim improve contrast against warm roofs without adding
  another rendering pass.
- `scripts/prepare_godot_sample.py` stages source, the extension, existing
  sounds/textures and licenses into `build/godot/project/`. Import caches stay
  there. `scripts/package_godot_app.py` creates the local standalone app and
  verifies its resources, signatures and native library paths. The original
  raylib `.app` resource manifest is unchanged.

`make test-godot-audio-mixer` optionally checks all 22 recordings through the
real Metal runtime and audio mixer, writing
`build/godot/audio-validation/result.json`. It does not measure physical speaker
output or playback latency.
`make test-godot-audio-mixer-bundle` runs the same decoded-sample checks against
the standalone app PCK from `/private/tmp`, without a source-project path. Its
external test script also checks that the raw OGG sources are absent; the signed
app and test-script hashes must remain unchanged. Its receipt is
`build/godot/audio-package-validation/result.json`.

The default camera uses the original 0° yaw, 50° elevation and 18.5-unit
minimum vertical span. In co-op, `coop_camera.gd` constrains the native shared
view to keep both animated tank silhouettes and three world units of ground
ahead/behind them below the fixed top HUD safety band. It applies only the necessary translation
and zoom-out, preserving yaw/elevation. `battlefield_camera.gd` then pans solo
and co-op views inward at map edges, reducing unused outside ground without
changing that span or direction. Both tank bounds and three world units of
nearby road take precedence over showing more map. Interior views that already
use the available battlefield stay unchanged. These adjustments are
presentation-only and do not mutate the native camera, input mapping or
deterministic state. The viewport keeps a 16:9 aspect
ratio; a resized window letterboxes the scene, and only the HUD's overlap with
that scene is reserved. A diagnostic
`--close-up` flag explicitly changes the camera and is not a normal-game
comparison. No speed, collision, shell spawn, upgrade, map or random-sequence
changes are introduced in the production game.

## Verification and reproducible captures

```sh
make clean
make -j4 test test-ai test-godot-core
make test-game-session-adapter
make -j4 test-sanitize test-ai-sanitize test-godot-core-sanitize
make test-game-session-adapter-sanitize
make test-godot-import
make test-godot-lan-sockets
make test-godot-lan
make test-godot-bundle
python3 -B tests/test_godot_benchmark.py
```

The native bridge checks all 35 stages against the shared session,
including state/RNG/events and native AI parity, and exercises custom settings,
session continuation, restarts, pad mapping, ordered audio and LAN synchronization.
A separate adapter test compares the original raylib integration with the shared
session, including state/RNG/events and concrete effect hooks. The socket
target runs an actual localhost TCP pair. `test-godot-lan` additionally starts
two independent Godot processes and verifies negotiated settings, both players'
movement/fire, at least 360 matching state/RNG observations beyond a minimum
480-tick run, and explicit disconnect handling. Slow peers may consume several
ticks per poll, so the test continues until both observation logs have enough
common ticks, within its existing 30-second deadline. A second run deliberately
polls every 33 ms; it must satisfy the same digest and coverage checks.
Neither test replaces physical two-machine QA.
Import validation checks staged bytes, all GDScripts/scenes and actual native
loading, then runs mesh contracts, a deterministic game tape and UI
signal/configuration checks. The UI gate also checks HUD-safe co-op
framing, edge-view scale and building/forest visibility against real native
input tapes and wheel/tread lifecycle across native restart/deployment, earns an actual high score,
checks record confirmation and timeout, returns from a non-record defeat, and
verifies record-preserving deployment/restart and held-confirm safety. High
scores remain session-only, as in the original app. Native firing checks cover
animated muzzle attachment, pause and pooled impact reuse. Interface checks
cover the unobstructed upper center, player/status information and layout at
three canvas sizes. The aligned menus additionally need direct keyboard/pad
row-navigation checks, separate-page return/cancel behavior, the full visible
report fields and native minimap markers. Fresh receipts for this revision
belong under `build/release-evidence/ui-alignment-20260918/`; earlier interface
receipts do not establish that the new alignment has passed its final gates.
Keyboard checks include side-specific Ctrl/Alt,
held keys, quick taps, overlapping modifiers and focus loss. Renderer fixtures
verify terrain damage invalidation, shared mesh restoration and unchanged native
state. They are isolated presentation fixtures, not gameplay screenshots.
The running-gear contract additionally checks all 24 configurations and 768
gear frames for distance/turn direction, blocked/sideways movement, pause/freeze,
respawn and catch-up, immutable shared banks, ground/model bounds, budgets,
current-frame shadows/occlusion meshes and unchanged gameplay RNG.
The gate rejects engine errors even when Godot returns zero. The bundle gate
also launches the packaged native and UI checks from `/private/tmp` without a
source-project path. Headless rendering is not visual or audible QA.

For the actual audio mixer check, after import:

```sh
build/godot-tools/Godot.app/Contents/MacOS/Godot \
  --path build/godot/project --rendering-method mobile --rendering-driver metal \
  --script res://audio_checks.gd -- --capture-audio
```

It verifies nonzero decoded samples for all 22 cues, priority/engine behavior,
overlap limits, volume, stop and disabled output through `AudioEffectCapture`.
A muted downstream bus prevents test sounds reaching speakers. This proves
mixing and routing, not audible listening or hardware latency. Without
`--capture-audio`, the script checks resources, lazy allocation, reuse, voice
bounds, gain and priority policy with a simulated playback lifecycle. It does
not queue decoded streams into the headless audio driver.

After `make godot-sample`, capture the actual Metal renderer with a fixed
seed and input tape:

```sh
build/godot-tools/Godot.app/Contents/MacOS/Godot \
  --path build/godot/project --rendering-method forward_plus \
  --rendering-driver metal --resolution 1280x720 \
  -- --demo --stage=1 --seed=20260916 --frames=360 --capture-at=300 \
  --capture="$PWD/build/godot/capture"
```

This saves full clean/pixel images, native-size tank crops, camera/state
metadata and timing observations. Simulation freezes while the paired images
are captured. `--demo` uses fixed 1/60 simulation steps and disables audio;
it does not measure live input latency. Use a different output directory when
preserving another comparison.

For a fixed movement direction without firing, add `--demo-command=BITS`.
This diagnostic replaces the normal six-segment input tape: up=1, down=2,
left=4, right=8, fire=16; values can be ORed together. For example,
`--demo --demo-command=1 --frames=360 --capture-at=300` captures the tank after
moving north. Keep the same command, seed, camera and capture frame across
comparisons. The flag affects diagnostic input only, not normal controls.

For the original renderer under identical game state and camera:

```sh
make build/tests/godot_reference_render
build/tests/godot_reference_render \
  --output=build/godot/reference --stage=1 --seed=20260916 \
  --frames=360 --capture-at=300
```

The original crop uses a conservative projected envelope; the Godot crop uses
the actual mesh bounds. Their crop dimensions are not directly comparable as
occupied tank pixel areas. Compare the full equal-size frames first. Lighting,
materials and surrounding art differ between the two implementations.

For a sequential Mobile / Forward+ benchmark without capture overhead:

```sh
python3 -B scripts/benchmark_godot_sample.py \
  --log-dir build/godot/benchmark-new
```

It uses a 1280×720 window, 1920×1080 internal rendering, stage 10 and a shared
3,600-frame tape. Both processes must complete real Metal rendering with the
same final simulation digest. Terminal states restart in benchmark mode, and
timing excludes introductions and the first 120 active samples. New receipts
use monotonic callback-to-callback wall times as the primary measurement and
retain Godot's smoothed/paced process delta separately. Neither is a GPU timer
or a display-present timestamp. See the
[benchmark contract](../godot/TOOLCHAIN.md#standalone-renderer-benchmark).

For a 20-minute Mobile stress workload with thermal-pressure observations:

```sh
python3 -B scripts/benchmark_godot_sample.py --renderer mobile \
  --seconds 1200 --stress --thermal --log-dir build/godot/soak-new
```

Stress uses legal +30% enemy tuning, six HP, 99 lives, P1 held fire and AI P2;
normal settings are unchanged. Duration tests require one renderer. All valid
active samples after warmup contribute to bounded 0.01 ms histograms. Percentiles
below 500 ms round upward by at most one bin; an overflowing percentile reports
the actual maximum. Exact per-minute distributions, counts, maxima and slow-frame
counts are retained. Thermal classes are macOS pressure observations, not
measured temperatures or power. The command does not imply a performance pass.

`--pixel` enables Pixel Style for the measured workload. Both ON and the default
OFF are checked against the renderer's reported mode. `--wide-coop` is a separate
two-human diagnostic: native input moves P1 north while P2 fires in place, with
six HP and 99 lives. It uses the ordinary co-op camera and never injects player
positions, changes maps or overrides the camera. Use stage 1 for the reference
route, and do not combine it with the AI-P2 `--stress` workload:

```sh
python3 -B scripts/benchmark_godot_sample.py --renderer mobile --stage 1 \
  --seconds 180 --wide-coop --pixel --thermal --trace-slow-frames \
  --log-dir build/godot/wide-pixel-new
```

A wide run requires both living, fully spawned players at least 10 units apart
and the ordinary camera span above 18.51. These observations must cover more
than 25% of active frames and at least 25% of elapsed time. The same minimum
elapsed coverage must include focused, drawable intervals with an advancing
engine draw counter. `wide_draw_timing` reports that subset separately, excluding
its first 120 samples; it is still a callback clock, not display-present timing.
This prevents a brief camera expansion or an entirely background run from being
reported as sustained co-op coverage. Native progress and benchmark restarts
remain recorded; the `--stage` argument names the starting stage.

For a separate diagnostic run, append `--trace-slow-frames` and use a new log
directory. This records every process interval, including intro, restart and
terminal transitions, with no warmup exclusion. Existing combat percentiles
keep their original definition. The trace keeps the first 16 and worst 64
intervals above 33.333 ms, the full slow count and the overall maximum context;
the two retained lists may overlap. Initialization time is recorded separately.

Each interval is associated with the **previous** callback's entry/exit phase,
native ticks, restart count, measured body and segment times. Context includes
terrain scans, primary mesh/material cache entries, nodes and observed pipeline
counters. `cached_meshes` excludes the separate running-gear phase bank; it is
not a total unique-mesh or memory measurement. These are counts, not allocation
totals or cache-miss rates. Counter
changes may lag the actual work and show correlation, not its duration or cause.
Time outside the measured callback includes other nodes, rendering, deferred
work, pacing and diagnostics; it is neither GPU time nor evidence of an OS stall.
Trace collection adds overhead, so keep its results distinct from runs without
the flag. No screenshots or per-frame file writes are introduced by tracing.

Trace v2 also records window focus, drawability and the engine's cumulative draw
count. It partitions callback intervals into stable focused/drawable intervals
with a draw-count advance, other draw-advancing intervals, no draw advance, and
unavailable counters. Up to 32 observed window-state transitions are retained.
An occluded macOS window can continue updating without drawing; those callbacks
must not be reported as rendered FPS. These are CPU-side observations, not GPU
completion or display-present timestamps. Historical trace-v1 receipts remain
readable, while v2 rendered benchmarks require at least one qualified interval.

The trace distinguishes `stage_transition` from combat. The existing active
timing filter still includes the five-second stage-clear transition, during
which the native game continues to update players, shells and pickups; it
excludes intro, pause, game-over and settlement. This preserves earlier timing
semantics. Use the diagnostic phase labels when separating those intervals.

## Engine-independent session and terminal/audio checks, 2026-09-17

The historical evidence paths shown as text below refer to **historical local
evidence, not included in this checkout**. Their original paths and reported
results are preserved for provenance; they do not establish current release
acceptance.

The shared session builds independently of raylib; both frontends reuse its
rules. Packaging prohibits a raylib runtime. Actual headless and Mobile/Metal
UI runs passed all 20 checks: stage-clear reached tick 4,511; a solo defeat ended
at tick 1,387 with zero points; a normal seeded co-op run cleared stage 20 and
lost stage 21 at tick 9,454 with a 3,150-point record. That run was repeated for
confirmation and native timed return. No scores, terminal snapshots or gameplay
state were injected. Captured panels were inspected at 1280×720: text and buttons
fit completely.

All 22 recordings produced actual decoded samples in the separate muted
capture-bus check, including priority, engine, overlap, true-zero volume and stop
behavior. Native UI play also observed boundary, score-count and record cues,
and exactly one Pause request on entry. Receipts and screenshots were indexed in
`build/release-evidence/godot-completion-20260917/ui/README.md`
and `build/release-evidence/godot-completion-20260917/audio/mixer.json`.
The historical measurements in the following sections belong to earlier
revisions. The latest M2 matrix is recorded in
[the September 19 arcade pass](#arcade-pass-m2-measurement-2026-09-19).

## Historical visual polish results, 2026-09-17

Six medium tank tiers now have distinct hull/turret profiles. Buildings use
four roof families with deterministic facade/eave variants. Two actual-render
reviews led to flatter, overlapping tree crowns, reducing each tree from 280
to 156 triangles with the same materials. The new `roster-game` sheet compares
all 12 player tiers at the normal game's world-units-per-pixel scale.

Stage 1, stage 10, diagnostic closeup and both camera boundaries match every
native snapshot field and camera parameter against the retained Godot baseline.
Full 1280×720 clean/pixel images, original-size crops, enlarged and game-size
rosters, and both art iterations were indexed in
`build/release-evidence/godot-polish-20260917/README.md`.

After `make clean`, the full test, AI and 35-stage bridge suites passed, followed
by ASan/UBSan, real localhost TCP, strict import and local app packaging checks.
Import validates seven scripts and 307 unique meshes, covering 24 vehicle
configurations, 512 brick cases, nine pickups, ten wall states and three bases.
Python fixtures passed: 13 import, 17 package and nine benchmark tests. The
expanded 14-part UI suite covers side-key fire, focus clearing, modal confirmation
and terrain cache invalidation/restoration without changing native state.
The final standalone app also passed that suite through actual Mobile/Metal
from `/private/tmp`, capturing deployment, pause and the real stage-clear report
at native tick 4,511. The renderer's actual viewport state confirms idle drawing
stops; a separate pixel probe verifies textures change only when requested.

One sequential Mobile/Metal run on the development M2 used the same stage 10,
3,600-frame tape, seed 20260916, 1280×720 window and 1920×1080 internal rendering.
It collected 3,093 monotonic wall-frame samples after exclusions, with one
restart and final digest `84b29737dbdd152a`. P50/P95/P99 callback intervals were
8.387/11.448/14.443 ms; presentation/simulation update P95 was 3.289 ms and
sampled peak RSS was 302.3 MiB. Terrain was processed 201 times over 3,600
frames. The static deployment scene also stops submitting 3D frames after its
requested update; this does not change battle frame frequency.

This is a short-run observation, not a sustained performance guarantee or an
isolated measurement of the cache change. Compared with the earlier run below,
frame pacing differs and sampled RSS is higher; the difference cannot all be
attributed to this patch. Forward+ was not rebenchmarked in this pass. Exact
commands, source hashes and measurements were recorded in
`build/release-evidence/godot-polish-20260917/benchmark-final/summary.json`.

## Historical migration baseline results, 2026-09-17

The clean full test, AI and native bridge suites passed, followed by ASan/UBSan.
Strict validation parsed all six then-current GDScripts, loaded declared scenes and
completed the original 180-frame native smoke with digest `f6ce119e378ac1f9`.
The expanded UI test exercised real controls/native settings, quick pause,
Pixel Style and return to deployment, then reached an actual stage-clear report
after 4,511 native ticks, checked press/hold/release behavior and advanced the
native stage. Real localhost TCP host/join/disconnect passed with matching state
over 360 authoritative ticks. Python fixtures passed: 10 import, 17 package and
9 benchmark checks. Authored geometry/state checks cover all 24 player/enemy
configurations, 180 damaged-brick/lot combinations, bases, pickups and effects.

Actual Mobile / Metal captures at native frame 300 match every pre-existing
snapshot field and camera parameter against the retained prototype for stages
1 and 10. Their digests are `afba842a6ef93d11` and `4791a8591a26e36b`. Full
1280×720 images, native-size crops, Pixel Style pairs, all four headings,
representative camera limits and real deployment/pause/report captures were indexed in
`build/release-evidence/godot-migration-20260917/README.md`.
The normal camera's scale and framing were not enlarged for the comparison.

Two sequential standalone runs used native Metal, stage 10, seed 20260916,
1920×1080 internal rendering, a 1280×720 window and the same 3,600-frame tape.
Each collected 3,093 monotonic wall-frame samples and 3,095 engine-delta samples
after exclusions, with one benchmark restart. Both ended at native digest
`84b29737dbdd152a`.

| Historical short benchmark | Mobile | Forward+ |
| --- | ---: | ---: |
| Monotonic frame interval P50 / P95 / P99 | 16.665 / 17.454 / 20.458 ms | 21.259 / 81.432 / 106.146 ms |
| Engine delta P50 / P95 / P99 | 16.667 / 16.667 / 16.667 ms | 23.810 / 78.302 / 105.310 ms |
| Presentation + simulation update P95 | 3.195 ms | 3.810 ms |
| Peak sampled process RSS | 261.5 MiB | 373.7 MiB |

Mobile remains the default. These measurements do not establish stable 60 FPS
or sustained thermal performance. Forward+ has substantial frame-time spikes;
their cause has not been isolated. The difference between engine delta and the
monotonic clock is why current comparisons use actual callback intervals. RSS
is sampled once per second and can miss brief peaks; neither frame clock is
GPU-only timing. Exact commands, source hashes and receipts were recorded in
`build/release-evidence/godot-migration-20260917/benchmark-final/summary.json`.

One UI-only change after this benchmark clears stale deployment connection text
when returning to the menu; it does not execute during the measured combat.
Final import and standalone package checks cover that change. The local app
is separately tested from `/private/tmp`, without a source-project path, using
the native smoke and UI integration checks; its signed-file/resource receipts
were recorded under `build/release-evidence/godot-migration-20260917/package/`.

## Historical sample results, 2026-09-16

These results describe the earlier small sample, before the full roster,
terrain, UI and LAN migration. They are retained as historical evidence, not
performance or visual acceptance of the current frontend. In particular, the
table measures engine delta, not the monotonic frame intervals now required by
the benchmark runner.

On the development Apple M2, both native Metal renderer runs completed with
valid measurements. This is not a performance pass: the final Forward+ run
did not sustain a 60 FPS frame budget. The original
and Godot frame-300 capture snapshots matched in every field, with digest
`afba842a6ef93d11`, including the camera. The two benchmark simulations also
ended with the same digest.

| Final 1080p test after clean rebuild/import | Mobile | Forward+ |
| --- | ---: | ---: |
| Elapsed wall time | 68.03 s | 125.03 s |
| Frame delta P50 / P95 / P99 | 16.67 / 18.52 / 26.54 ms | 29.14 / 69.81 / 100.96 ms |
| Presentation + simulation update P95 | 2.189 ms | 2.534 ms |
| Peak sampled process RSS | 241.64 MiB | 280.22 MiB |

An earlier run of the same rendered scene measured P50/P95 of 8.33/9.09 ms
for both renderers (Mobile 32.48 s and 328.47 MiB; Forward+ 31.19 s and
299.03 MiB). Both runs are retained in `benchmarks/` and `benchmarks-final/`
under the evidence directory. Only input/restart guards changed between the
capture/first-run version and the final version, with no rendering changes.
The substantial variation is unresolved; rebuild timing, thermal state, frame
pacing and shader caching have not been isolated as causes.

Each collected 3,095 timing samples from active gameplay, with one restart.
Frame delta includes engine pacing/vsync and is not GPU-only time. RSS was
sampled once per second and can miss peaks. These runs establish that this
small scene works through Metal, but do not establish stable 60 FPS, sustained
thermal performance, loaded late-game performance or that Forward+ is cheaper
in general. Mobile remains the conservative starting configuration while the
variation is investigated.

The clean full test run, AI tests, bridge parity, ASan/UBSan, strict import and
benchmark fixtures passed. Logs, full screenshots, crops and receipts were indexed in
`build/release-evidence/godot-sample-20260916/README.md`.
These are local experimental evidence, not a release candidate attestation.

Historical window checks confirmed R restart, the then-present F1 AI shortcut, Tab Pixel on/off and
Escape pause. A quick-tap polling miss was corrected by queuing pause/confirm
key events; strict import and the native smoke run passed again afterward.
This input-only fix followed the saved visual/performance captures and did not
change their art, camera or demo commands. Full control feel remains unverified.

The recorded video excerpt at `build/release-evidence/godot-sample-20260916/video/tanks-godot-demo.mp4`
is 7.37 seconds, 1280×720 and silent. A longer PNG recording reached its time
limit; the 442 complete consecutive frames were encoded and fully decoded to
verify their timestamps. Many pictures are static, so 60 fps playback does
not establish 60 fps live performance. Raw PNGs were removed after validation
to recover disk space; source hashes and four decoded review frames remain.

## Arcade-pass M2 measurement, 2026-09-19

The final environment, vehicle and HUD revision completed four sequential
Mobile/Metal measurements on the development Apple M2. All four runs have the
same 122 source/native-input hashes and 74 managed staged-resource hashes.
The requested window is 1280×720, with 1920×1080 internal rendering. The local
app was rebuilt and its exact package verified against the same managed inputs.

| Workload | Duration | Active wall p95 / p99 | Active maximum | Active intervals > 33.333 ms |
| --- | ---: | ---: | ---: | ---: |
| Stage 10 stress, Pixel off | 180 s | 17.09 / 17.49 ms | 25.875 ms | 0 |
| Separated co-op, Pixel off | 180 s | 16.90 / 17.33 ms | 20.053 ms | 0 |
| Separated co-op, Pixel on | 180 s | 16.91 / 17.46 ms | 22.301 ms | 0 |
| Stage 10 stress, Pixel on | 1,200 s | 17.44 / 17.77 ms | 33.102 ms | 0 |

The stress workload uses legal +30% enemy tuning, P1 firing and native AI P2;
the observed maxima remain the classic four live enemies, eight shells and
eight simultaneous presentation effects. The separated co-op runs use native
movement and the normal camera. They actually draw an expanded view for 140.70
and 140.68 seconds (about 78% of each run), rather than counting two-player
configuration alone as wide-camera coverage.

The 20-minute run includes 71,998 focused, drawable intervals with advancing
engine draw counts across all phases, excluding five other-window intervals.
None of those foreground intervals exceeds 1000/30 ms; this is separate from
the active-combat table, which excludes its first 120 valid timing samples.
Later minute-level p95 values rise modestly to 17.46–17.67 ms. Thermal pressure
has 212 nominal and 28 fair observations, with no serious/critical samples;
fair begins around 1,062 seconds. Peak sampled RSS is 341.45 MiB, and the final
minute averages 173.54 MiB. The later RSS decrease has no established cause and
is not evidence of an optimization or a proof that no leak exists.

Each run retains one 38–45 ms startup interval outside the focused draw category.
Most of those intervals lie outside the measured callback; some also coincide
with pipeline-counter changes. These observations do not isolate a GPU or OS
cause. The earlier intermittent large stalls did not recur in this matrix,
which does not establish their permanent resolution.

Wall intervals include pacing, scheduling and rendering, not GPU completion
or display-present timestamps. Measurements validate these local workloads,
not a release frame-budget guarantee or other Macs. Full per-minute data,
slow-frame contexts, thermal observations, source bindings and reproduction
commands were recorded in
`build/release-evidence/arcade-upgrade-20260919/FINAL_REVIEW.md`
and its linked performance summary. The earlier short runs and deliberately
interrupted soak before the LAN pause-guide correction are retained separately
and are not the final matrix.

## First-water transition preparation, 2026-09-30

The frontend now registers one hidden water instance in its normal world during
scene setup. The material alone did not prepare the instance's surface pipeline.
All water cells share that instance's mesh/material; lot coordinates and brick
damage do not change water geometry. The hidden node is outside the tile map,
never draws, and survives restarts without adding more warmup instances.
This follows Godot's [pipeline instancing guidance](https://docs.godotengine.org/en/stable/tutorials/performance/pipeline_compilations.html#pipeline-precompilation-instancing).

On the M2, two stage 3-to-4 diagnostic pairs used the same 1280×720 window,
default camera, seed, native input and Mobile/Metal renderer. Godot's shader
disk cache was disabled only in isolated test projects; the Metal driver cache
was not cleared. The second pair reversed execution order. Maximum callback
intervals around the transition were 84.34 / 81.72 ms before and 33.01 / 20.28 ms
after. Surface pipeline counts increased by one at the old transition and zero
after the change. These are process intervals, not display-present measurements
or a frame-budget guarantee. Preparation moves work to startup; it does not
eliminate the compilation cost.

Normal and Pixel Style stage-4 captures are byte-identical before/after, with
identical native snapshots and camera metadata. The existing 1.42-second
long-session outlier did not recur in the cached baseline, so its full cause
is still unassigned. This scoped improvement does not attest a new candidate or
transfer the old candidate's 30-minute QA to modified source. Raw diagnostics,
source bindings and captures are under
`build/release-evidence/water-stall-20260930/`.

The rebuilt development app also completed a separate 240-second normal
stage-progression run with four clears, 100% observed window focus and nominal
thermal pressure. Actual Metal display intervals after the declared 30-second
startup exclusion averaged 59.94 FPS, with a 54.39 FPS 1% low; the maximum across
the entire run was 50.00 ms. Peak sampled RSS was 335.5 MiB, with 8.9 MiB growth
using the recorded median-window method. This short, Pixel-off workload is
regression evidence, not the candidate-specific extended-session gate.

## Earlier diagnostics and remaining acceptance work

The final local completion run and actual captures were indexed in
`build/release-evidence/godot-finish-20260917/README.md`.
On the development M2, a 20-minute Mobile/Metal run at 1920×1080 internal render
resolution completed 134,490 active wall-frame measurements: p95 12.58 ms,
p99 14.74 ms, maximum 43.269 ms, with 240 nominal thermal-pressure samples and
no progressive per-minute slowdown. A separate one-minute calibration contained
a 1061 ms outlier that did not recur; its cause is unassigned. Demo audio and
intro/reset phases are excluded, so this is not combined hardware-input/audio
latency or a display-present 60 FPS release attestation.

Those measurements predate the subsequent forest/smoke refinement. Its actual
captures and checks were indexed in
`build/release-evidence/godot-refinement-20260917/README.md`.
The later three-minute stage 10 stress run with diagnostics enabled measured
wall p95 16.90 ms, p99 18.12 ms and a 328.622 ms maximum. The corresponding
callback took 8.679 ms; the remaining time is unassigned engine/render/pacing
work, not measured GPU time. A separate 3,600-frame ordinary run without tracing
measured p95 18.26 ms, p99 31.79 ms and maximum 43.587 ms. These different
workloads do not isolate trace overhead or an art-related performance change;
neither included thermal observations. The optional trace improves evidence,
but does not resolve the intermittent stall.

The next forest/effect continuation, recorded in
`build/release-evidence/godot-continuation-20260918/README.md`, reduces foliage
from 208 to 156 triangles per cell and reuses transient effects.
Its separate 18,000-frame pooling comparison kept the same art, native library,
input and final digest. It created 11 instances and reused them 3,393 times;
the final observed surface/specialization pipeline counters were 18/11, versus
baseline late lower bounds of 914/823. Identical normal/Pixel screenshots verify
the pool alone preserved the sampled rendered image. A 1018.386 ms interval
still occurred while the window was unfocused, with only 1.739 ms inside the
measured callback. Focus and thermal conditions were not matched, and the
remaining delay is unassigned. This establishes resource reuse, not a resolved
long-frame problem or a stable 60 FPS claim. The complete timing distributions
and limitations were recorded with that comparison.

The later battlefield-readability trace separates foreground/drawable frames
from background callbacks. Its largest observed interval was 83.344 ms, of
which only 1.485 ms was inside the measured update (native 0.017 ms,
snapshot 0.615 ms, presentation 0.750 ms). No terrain, mesh-cache, effect-pool
allocation or pipeline-counter increase accompanied that interval. This narrows
the investigation to work outside the callback; it does not identify GPU time
or distinguish engine pacing from OS scheduling. The follow-up
`build/release-evidence/readability-polish-20260918/visibility-profile/`
isolates the unchanged visibility module at 0.145 ms mean / 0.228 ms P95 over
the 3,600-frame native tape. A camera-projection cache experiment was rejected
because it changed forest/near-camera boundary classifications. These diagnostic
results do not establish a fix for the intermittent long-frame issue.

The normal-size art still has family resemblances between some tank tiers;
Pixel Style reduces fine profile differences. Cell-based forest placement and
roof material blocks remain visibly procedural. See [Godot art](GODOT_ART.md)
for the latest smoke/forest revisions and their actual-render checks.

Sustained frame pacing and dense-combat results must be reviewed separately from
a completed workload. Physical Bluetooth controls, audible listening, two-Mac
LAN, a full human 35-stage run and a clean second-Mac launch remain unverified.
The fixed 16:9 scene is a presentation constraint. Game-over, earned-record,
record timeout and session continuation now have actual native/UI checks.
Local packaging and the immutable Godot candidate workflow are available.
Bundles use ad-hoc signing; Developer ID signing/notarization and publication
acceptance have not been completed for this app.

## Optional silent demonstration video

Godot's movie writer can record actual rendered frames with the same demo
commands. This is offline recording, not a performance benchmark:

```sh
mkdir -p build/godot/movie-frames
build/godot-tools/Godot.app/Contents/MacOS/Godot \
  --path build/godot/project --rendering-method forward_plus \
  --rendering-driver metal --resolution 1280x720 \
  --write-movie "$PWD/build/godot/movie-frames/frame.png" --fixed-fps 60 \
  -- --demo --stage=10 --frames=1200
xcrun swiftc -O -module-cache-path build/godot/swift-module-cache \
  scripts/encode_godot_video.swift -o build/godot/encode_godot_video
build/godot/encode_godot_video build/godot/movie-frames build/godot/demo.mp4 60
```

The encoder uses macOS AVFoundation/ImageIO, requires no extra package and
refuses to overwrite an existing movie. Use a fresh PNG directory for each
recording. The MP4 has no audio track.
