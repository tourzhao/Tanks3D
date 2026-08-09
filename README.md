# Tanks 3D

Tanks 3D is a non-commercial, isometric arcade tank game for macOS. It uses a
fixed 45-degree camera, four-direction movement, destructible environments,
local co-op, national vehicle progressions, configurable difficulty, pickups,
and an end-of-stage battle report. Models, level layouts, pickup art, effects,
and most environment geometry are authored procedurally in C++.

This is an independent fan-made project. It is not affiliated with or endorsed
by any game publisher, vehicle manufacturer, government, or other rights
holder. Fictionalized WWII-era themes and historical political figures appear
for period context; their inclusion is not an endorsement.

## Requirements and build

The source build requires macOS, a C++17 compiler,
[Homebrew](https://brew.sh/), and raylib. Release-status tooling additionally
uses Python 3 and only its standard library. The current development build is
tested with Apple Clang and raylib 6.0:

```sh
brew install raylib
make clean
make debug
make test-architecture
make test
make test-sanitize
make coverage
make run
```

`make all` creates both `build/Tanks3D` and `build/Tanks3D.app`. This local
development bundle links the active Homebrew raylib; use `make run-app` to open
it. If raylib is installed outside Homebrew's active prefix, pass it explicitly:

```sh
make RAYLIB_PREFIX=/path/to/raylib clean all
```

`make clean` removes disposable compiler, test, coverage, and distribution
outputs. It deliberately preserves immutable candidate files under
`build/release/` and candidate-bound QA evidence under
`build/release-evidence/`; remove those records only as an explicit manual
operation after verifying that they are archived elsewhere.

## macOS Alpha package

`make dist` builds and verifies a self-contained ZIP for the current Mac
architecture. It requires and statically links raylib 6.0, copies the
version-locked raylib and embedded-dependency notices into the app, applies an
ad-hoc integrity signature, generates a SHA-256 file, extracts the ZIP into a
temporary directory, and reruns the complete self-test from that extracted app.
Outputs are written under `build/dist/`.

The archive name records the architecture and the real deployment target, for
example `Tanks3D-0.1.0-alpha.3-macos-arm64-macos26.0.zip`. That target is read
from the installed raylib and must match the game build; supplying a raylib
built for an older macOS target produces an appropriately named package. The
ad-hoc signature proves bundle integrity but is not Developer ID signing or
Apple notarization, so this remains an Alpha package. See
`docs/RELEASE_CHECKLIST.md` before publishing it.

The source repository preserves the blocked Alpha 3 release page at
`docs/releases/v0.1.0-alpha.3.md` and the earlier Alpha 1 page at
`docs/releases/v0.1.0-alpha.1.md`. Alpha 3 is a frozen legacy-v1 record, not a
publishable candidate for the current source.

To audit that frozen record, run:

```sh
make test-release-status
make verify-tagged-alpha-candidate DIST_CHANNEL=alpha.3
make check-alpha-release-evidence DIST_CHANNEL=alpha.3
make verify-alpha-release-ready DIST_CHANNEL=alpha.3
```

The evidence check validates an honestly incomplete report and prints every
blocker; it is not release approval. The final command omits that exception and
must pass before public upload. Both candidate checks require the five local,
ignored files under `build/release/v0.1.0-alpha.3/` and a clean worktree. See
the [Alpha 3 status](docs/releases/v0.1.0-alpha.3-status.json) and
[release checklist](docs/RELEASE_CHECKLIST.md). Manual PASS records use
candidate-bound structured session, command, event, and performance logs; the
fixed 30-minute Alpha budget requires >=50 average FPS, >=30 1% low FPS, and
<=256 MiB memory growth. Raw telemetry must also prove at least one cleared
stage, >=80% active-gameplay time, and >=95% focused-window time. Any new
candidate must use `macos-alpha-v2`. After capturing the seven required PNGs
under `build/release-evidence/<tag>/screenshots/`, a one-shot helper can create
the versioned release page, QA report, status JSON, and copied images as an
honest BLOCKED baseline:

```sh
make init-alpha-v2-status DIST_CHANNEL=alpha.N
```

The helper refuses existing outputs and never records manual PASS results. It
rolls back ordinary failures on a best-effort basis; after a forced process
termination, inspect and remove any partial versioned outputs manually before
retrying. Commit and review its output before running the evidence checker.
The v2 interactive compiler then turns 70 explicit observations into six
candidate-bound evidence categories: main menu/advanced settings, one-player,
two-player, national bases, pickups, and settlement. The first three contexts
must each have their own recording of at least 64 KiB; a shared clip or a static
menu image is not sufficient. Keep every recording at or below 95 MB so the
evidence commit remains compatible with ordinary GitHub Git storage. Clean-Mac
acquisition remains a separate Gatekeeper session so control testing never has
to share its session identity.

Clean-Mac evidence uses a two-machine workflow. On the release workstation,
stage the exact candidate at a recordable HTTPS URL, then create a new transfer
kit:

```sh
make prepare-alpha-v2-clean-mac-qa-kit DIST_CHANNEL=alpha.N \
  CLEAN_MAC_DOWNLOAD_URL='<recordable HTTPS URL ending in the exact filename>'
```

The kit contains only `START_HERE.command`, `clean-mac-plan.plist`,
`README.txt`, and `kit-manifest.json`; it contains neither the game repository
nor the candidate. Transfer it to a fresh Apple Silicon Mac, start the required
screen recording or external-camera capture, and run its documented
`/bin/zsh -f START_HERE.command <new-output-dir>` command. That Mac needs no
checkout, `make`, Python, Homebrew, or raylib. A static PNG cannot replace the
continuous launch-path recording. Use a 64 KiB–95 MB MOV, MP4, or M4V
recording with a structurally valid ISO-BMFF video container. Follow the prompts
to perform the real Safari download, Finder/Archive Utility extraction, five
diagnostic commands, and first Finder launch. Do not automate **Open Anyway**
or add/remove quarantine attributes.

Return the completed raw intake and capture to the release workstation. A
different human reviews them and compiles a new persistent evidence pack:

```sh
make compile-alpha-v2-clean-mac-evidence DIST_CHANNEL=alpha.N \
  CLEAN_MAC_INTAKE_DIR=/absolute/path/to/raw-intake \
  CLEAN_MAC_MEDIA=/absolute/path/to/gatekeeper-capture.mov \
  CLEAN_MAC_REVIEWER='Reviewer Name' \
  CLEAN_MAC_REVIEWER_SIGNATURE='Reviewer Name' \
  CLEAN_MAC_REVIEWED_AT_UTC="$(date -u '+%Y-%m-%dT%H:%M:%SZ')" \
  CLEAN_MAC_REVIEW_NOTES='Safari, Finder, dialog, and main menu reviewed.' \
  CLEAN_MAC_RELEASE_NOTE_WORDING_VERIFIED=yes
```

The compiler refuses replacement and writes only a candidate-bound evidence
pack (including canonical acquisition, command-log, session and receipt JSON,
the reviewed media, and `status.next.json`); it never overwrites the canonical
release status. Use the release-note verification flag only after the observed
launch path matches that wording. Run the command immediately after completing
the review so its generated UTC value follows the recorded session. Review and
commit the pack before running the clean-worktree, allow-blocked evidence check
described in the release checklist.

Collect candidate-generated performance telemetry and its launch receipt with:

```sh
make run-alpha-performance-qa DIST_CHANNEL=alpha.N
```

The runner re-verifies the tagged candidate, copies the verified ZIP to a
private hash-checked snapshot, extracts only that snapshot, and first executes
its no-window, identity-bound performance capability self-check. Legacy or
drifted candidates fail before evidence files or the long session are started.
New `macos-alpha-v2` work requires the v3 candidate attestation, proving that
the tagged strict verifier enforced this capability contract.
Compatible candidates record real one-second frame/RSS windows for 1,801
seconds; the runner terminates a hung process after the requested duration plus
a two-minute grace period and refuses to replace an existing evidence
directory. Before signing the receipt, it applies the same shared raw-v2
contract as the final verifier: exact keys and identity, nested UTC times,
continuous 0.75–1.25 second samples, valid state/focus/stage relationships, and
complete duration coverage. Telemetry over 32 MiB or bytes changed between
validation and receipt hashing are rejected. The receipt proves evidence
integrity; the final verifier still decides whether the run meets the fixed
30-minute, FPS, memory, gameplay, focus, and cleared-stage thresholds. For the
inherited 22-sound set, the profile
accepts only the release owner's explicit `ACCEPT` risk decision; `CONFIRM`
needs a future profile with an externally trusted cryptographic rights-holder
signature.

## Controls

- Menus: arrow keys or `WASD` change a setting; `Enter` or `Space` confirms.
  `Esc` returns from Advanced Settings; `Esc` or `Q` exits from the main setup
  screen.
- Player 1: arrow keys move; Right Option, Right Control, or `Space` fires.
- Player 2: `WASD` moves; Left Option, Left Control, or `F` fires.
- During play: `Enter` pauses, `Esc` returns to setup, and `R` restarts.
  `F8` changes rendering quality, `F11` toggles borderless mode, and `N`/`B`
  changes stage.

For release QA, “or” lists alternatives available to the player, not permission
to sample only one: exercise every listed navigation, confirmation, firing,
stage, and exit binding. Verify `F8` with the accompanying `high-quality`
(2048x2048) / `balanced` (1024x1024) shadow-map log as well as the rendered
scene; capture that line in the context recording and observation notes because
the compiler does not accept a separate log attachment.

Movement remains aligned to the map axes. Players select one or two tanks,
national vehicle trees, stage, lives, and advanced enemy speed, firing, spawn,
and HP settings. Stars upgrade the current vehicle. Players have configurable
HP; healing pickups appear only when useful. Shells can cancel opposing shells,
and direct-fire streaks reset when the player tank is destroyed.

## Repository layout

- `src/core/` contains raylib-free coordinates/directions, advanced-setting
  normalization, player progression, and the single shared `Nation` type.
- `src/game/` contains raylib-free entity and bonus rules, a deterministic
  `StageGenerator`, the mutable `StageMap` route/collision/destruction rules,
  and a compiled `BonusSystem`. Pickup state now uses `core::XZ`; the system
  owns spawn-draw validation, the complete carrier-release transaction,
  lifetime/collector decisions, the per-pickup collection/application
  transaction, stable vector traversal/current-index erasure, safe score/tally
  credit, and all player/enemy bonus mutations.
  Carrier release snapshots Bandage eligibility once, preserves
  type/position retry draw order, rejects only the government-base area, and
  synchronously commits a detached pickup intent. The system also returns
  ordered value-only commands for Shovel, Grenade audio/FX/events, and camera feedback. The module
  also contains incremental `CombatSystem` ownership of shell spawn/impact state, swept
  opposing-shell cancellation, pure map/core-impact outcomes, and read-only
  tank targeting in both directions. Validated commits own enemy armor, death,
  score, tally, and streak changes plus player Shield/Boat/HP and fatal-state
  transitions; the data-only `GameEvent` contract and physical-result event
  projection also live here. Opposing-shell cancellation outcomes likewise
  project to detached events in this module. The raylib-free settlement module
  validates and classifies report entry through a detached
  `SettlementBeginPlan`, then owns battle-report snapshots, classified counting,
  confirmation, and the Idle timeout through `SettlementState`. It also builds a
  detached
  `SettlementTransitionPlan` that performs the strict high-score comparison and
  plans stage wrap plus lives/level progression without calling audio, loading a
  map, navigating, or mutating live players. The incremental compiled
  `EnemySystem` owns active-player target selection, pursuit-axis choice,
  collision-safe local escape selection, type-specific fire planning, Armor
  firing lanes, rotating spawn-slot policy, spawn probability/armor rules,
  detached enemy construction, and the Destroyed/Creating/Frozen/Active
  frame-entry timer gate. It also owns the active cardinal/ice movement and
  steering transactions, including the strict decision gates, conditional
  pursuit/wander random-callback pulls, and atomic escape route commits, while
  returning detached track-dust data. Its complete spawn-attempt transaction
  owns queue/identifier exhaustion, guard and timer ordering, rotating
  availability queries, conditional random pulls, ID reservation, detached
  construction, and ordered state commits. Its post-movement firing transaction
  owns the due gate, conditional `R`/`R-Q`/`R-Q-C` callback transcript,
  type/Armor reloads, normalized fire-rate setting, owned-shell suppression,
  detached `Shell` launch intent, and cooldown commit after the launch callback.
  The compiled `PlayerSystem` owns the data-only two-player input types, the
  entity-free five-clock `Inactive`/`Creating`/`Ready` frame-entry transaction,
  the inactive `deathTimer`/lives transaction,
  the entity-free 19-field spawn/reset transaction,
  the pure control plan, the active scalar movement transaction, and the
  post-movement scalar fire transaction. Frame entry rejects invalid elapsed
  time atomically, chooses its phase from the entry snapshot, and preserves the
  legacy ordered-`std::max` behavior of malformed clocks. Inactive death also
  rejects invalid elapsed time atomically, preserves entry and cross-zero timer
  behavior, retains malformed-timer parity, and saturates non-positive lives
  without `INT_MIN` overflow. Movement validates elapsed time and speed before
  mutation, preserves ice carry and blocking semantics, performs an optional
  lane-snap query before the forward-candidate query, and returns a detached
  track-dust intent. A blocked candidate keeps an accepted snap but cancels
  momentum; invalid, overflowing, or callback-free attempts are atomic.
  Firing preserves the held/due gates, reload-before-cap reset, owning player
  index, clamped level statistics, and legacy `None`/`5`/`255` behavior.
  Spawn preparation validates its detached position, durations, and progression
  mode before an atomic commit. `Preserve` keeps progression and positive HP;
  `Reset` clears progression and restores the configured maximum HP. For parity,
  malformed maximum-HP values remain unvalidated and are copied exactly whenever
  legacy reset/recovery policy selects them.
  `Game3D` first commits all five returned clocks, then routes `Inactive` through
  the scalar death transaction, commits its returned timer/lives before
  slot-owned shell cleanup, spawn reset, event append, and audio, clears
  movement for `Creating`, and reads input only for `Ready`. It supplies map ice
  and live occupancy/water queries, commits the scalar movement result, presents
  dust and resets its 0.120-second clock, then invokes firing and consumes
  shell/event/muzzle/camera/audio presentation.
  Active-shell counting, launch ownership, and respawn cleanup use the player
  slot index even when an entity ID differs. Final-life shells remain until
  expiry and continue delaying Game Over. `Game3D` resolves entity ID to the
  legacy spawn point, snapshots and writes back the entity fields, and retains
  owned-shell cleanup, spawn events/audio, entities, map, rendering, cameras,
  and concrete movement/fire orchestration and presentation adapters.
  `Game3D` retains the map-derived ice fact, tuned configuration, live occupancy
  and seeded-random adapters, synchronous enemy/shell container insertion, and
  the concrete shell-to-event-to-muzzle presentation sequence.
- `src/app/` owns shared raylib-free presentation values, three pure command
  mappers, and the common synchronous side-effect sink/dispatcher used by tank
  hits, map/base-core impacts, opposing-shell cancellation, and bonus commands.
  Detached actions own events, FX/camera parameters, requested cues, and final
  contact where applicable; domain commits such as Shovel steel and shell impact
  remain explicit at the orchestration edge. The same layer owns the pure,
  tested parser and atomic no-replace file writer for default-off release
  screenshots; framebuffer capture and PNG encoding remain at the `main.cpp`
  application boundary. `src/audio/` owns the
  shared 22-value `AudioCue` contract and the raylib-free `AudioOutput` runtime
  boundary; neither module owns devices or renderer resources. `Game3D` keeps
  only a nullable, non-owning `AudioOutput *`. The concrete `AudioBank` remains
  stack-owned by `main`, which controls device setup plus resource load/unload,
  while the bank retains cue priority, overlap, and engine-voice policy.
- The remaining top-level `src/` files contain `Game3D` orchestration and
  concrete projected-event consumers. Tank hits build and immediately consume
  an owned command plan for event, FX, camera, audio, and final-impact actions;
  map/core hits and shell cancellations likewise build and immediately consume
  owned commands through the shared synchronous sink. Default-off observers
  retain exact post-side-effect ordering and payload tests without storing
  traces during play. Carrier-bonus `RandomSource`/STL draw adapters, the live
  base-position query, concrete pickup/event/audio commits, bonus messages,
  and the exact collection presentation callbacks remain here. `BonusSystem`
  processes pickups sequentially, so one pickup's state changes affect the
  next, then invokes the deletion observer only after erasing a collected
  pickup.
  Non-physical event producers and the concrete settlement event/audio/display,
  menu, map-load, and player commits also remain here. Settlement entry uses a
  default-off observer to lock `PlanReady` -> `ReportCommitted` -> optional
  `StageEndEventAppended` -> `AudioStopBoundaryPassed`; showcase reports omit the
  event step. `ScoreCounted` is requested only after report state advances, and
  confirmation does not synthesize skipped count cues. Settlement completion is
  observed by a default-off synchronous seam in exact post-side-effect order:
  plan, high-score/audio/display or menu, and candidate request/preparation,
  progression, then stage commit. Candidate maps must report the requested
  stage. Failed automatic loads preserve the old stage and players and enter a
  frozen one-shot return-to-menu state; failed `start()`/`restart()` calls also
  restore the complete prior session configuration and player/event snapshots.
  Rendering, effects, audio devices, and procedural models remain here while the
  split continues.
- `tests/` contains independent core/game/bonus/generator/map/combat/enemy/player/settlement
  and app-command/side-effect-dispatch test executables, renderer bridges and
  dependency-boundary probes,
  and the larger transitional self-test still compiled into the game translation
  unit.
- `docs/ARCHITECTURE.md` records module boundaries and current risks;
  `docs/REFACTORING_PLAN.md` defines the incremental PR sequence and test gates.
- `docs/GAMEPLAY_INVARIANTS.md` freezes the rule and timing contracts that
  refactoring must preserve.
- `docs/COVERAGE_BASELINE.md` records the pre-modularization coverage baseline
  and the production paths that need deterministic tests first.
- `resources/sounds/` contains the 22 runtime OGG cues.
- `resources/textures/` contains two generated environment albedo maps.
- `resources/models/tank_basic.glb` is an optional importer QA probe; normal
  gameplay uses code-generated vehicles.
- `macos/` contains application-bundle metadata.
- `scripts/verify_macos_dist.sh` validates the extracted Alpha archive rather
  than trusting the staging directory.
- `tests/test_macos_dist_verifier.sh` exercises sixteen isolated rejection paths:
  checksum-name/content mismatches, a checksum symlink, input and resolved
  newline-bearing paths, lexical and symlink-parent path escapes, an archive
  symlink, an extra top-level payload, an archived symbolic link, and a
  checksum-updated signed-resource mutation, plus re-signed mutations of the
  raylib dependency notice, official raylib license, compiled performance
  capability schema, a bounded capability-probe timeout, and a bounded
  integrated-self-test timeout.

Run the importer check with:

```sh
build/Tanks3D --quick-start --gltf-tank-qa
```

For narrower checks, use `make test-core`, `make test-game`, `make test-rules`,
`make test-app`, `make test-unit`, `make test-session`, or `make test-assets`.
`make test-release-performance-capabilities` runs the exact no-resource,
no-window performance handshake and compares its JSON byte-for-byte.
Use `make test-release-screenshot` for the explicit local 1280x720 GPU smoke;
it opens a window and remains outside CI and headless release gates.
Use `make test-dist` to rebuild and validate only the self-contained archive.
`make test-architecture` compiles every `core/`, `game/`, `app/`, and `audio/`
value boundary without raylib include paths, with warnings as errors. It rejects
reverse dependencies plus raylib, renderer, device-audio, and particle calls in
those modules. Only the asset category requires the runtime OGG, PNG, and GLB
files. `make test` also compares the `.app`
resource bundle against
`tests/expected_bundle_resources.txt`. Session tests use explicit gameplay
seeds plus a typed scripted source for exact probability boundaries and draw
order. They also drive data-only two-player input frames through the production
update path, verify the ordered per-update `GameEvent` stream, and compare a
versioned session digest plus full event sequence across two fixed-seed,
480-frame same-build replays at 1/60 second, including per-frame event
boundaries. The digest is a replay aid, not a cross-version compatibility
guarantee. Normal play continues to choose a fresh seed at process startup.

The complete automated gate currently runs 18 integrated suites with 7,191
checks, 154 core/game suites with 1,107 checks, and 50 app-layer suites with
292 checks: 222 suites and 8,590 checks across nineteen profiles. The exclusive
release-performance capability handshake supplies the second instrumented game
profile and is byte-compared with its golden JSON contract. The direct
PlayerSystem executable contributes 31 suites and 64 checks. Its direct and
production-path coverage includes spawn-position adapter mapping, both
progression modes, complete 19-field write masks, invalid-input atomicity,
signed-zero and maximum-finite inputs, legacy malformed-HP behavior, five-clock
frame entry, movement/fire ordering, inactive death/respawn, post-commit respawn
cue ordering, mismatched entity ID versus slot ownership, and final-shell Game
Over delay. Targeted production regressions and the
AddressSanitizer/UndefinedBehaviorSanitizer run pass all 18 integrated suites and
7,191 checks. The integrated `view-target-allocation` suite locks the raylib 6.0
depth-metadata contract, HDR-first/RGBA8 fallback, cache reuse, invalid and
partial cleanup, transactional replacement, unique ownership, retry, and
invalid-count rejection. A window/context failure exits before the application
initializes audio or loads its GPU assets. A gameplay-target failure returns
ordinary play to setup, while release screenshot and performance modes exit
nonzero. A recording `AudioOutput` verifies post-commit `StageStart`,
creating-versus-ready engine state, pause `stopAll` then `Pause` ordering with
silent resume, and settlement
stop requests only after `StageEnded` is appended. These headless checks verify
requests at the output boundary; they do not verify hardware playback or the
concrete `AudioBank`'s internal voice-priority behavior.

## Licensing

Project-owned source and assets are provided under the
[PolyForm Noncommercial License 1.0.0](LICENSE). Commercial use is not granted.
This is source-available software and is not advertised as OSI open source.
The recorded author identity is **tourzhao**; the required notice line is
`Required Notice: Copyright (c) 2026 tourzhao.` and is preserved in
[NOTICE](NOTICE).

Third-party portions keep their own licenses, which take precedence over the
project license: upstream MIT material, statically linked raylib 6.0 and its
embedded dependencies, and the CC0 QA model are documented in
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) and `LICENSES/`. Those terms
may grant rights independently of PolyForm. Asset provenance and the
conservative audio caveat are in [ASSET_LICENSES.md](ASSET_LICENSES.md). No
license here grants trademark, character, publicity, or other third-party IP
rights; this is an independent, unaffiliated fan project.
