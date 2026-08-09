# Incremental Refactoring Plan

## Purpose and Guardrails

This plan turns the current working game into independently testable modules
without changing established gameplay. Refactoring and behavior changes must be
separate pull requests. A move-only pull request may relocate or rename code but
must not rebalance timing, collision extents, probabilities, HP, scoring, camera
composition, models, shaders, audio selection, or effects.

The pre-refactoring audit baseline was 15,508 source lines. `src/main.cpp`
contained 9,786 lines, including `StageMap`, `Game3D`, rendering, menus,
self-tests, and process startup. Its approximately 1,730-line `runSelfTests()`
contained 111 check sites. The project builds cleanly with
`-Wall -Wextra -Wpedantic`; the headless test covers 35 deterministic stages,
and ASan/UBSan currently pass. Preserve that baseline after every step.

## Target Dependency Direction

```text
app/main + app/menu (wiring)
                 |
platform/raylib_input --commands--> game/GameSession <-- tests/scripted_input
                                         | emits          | exposes
                                         v                v
                                  game/GameEvent    game/GameSnapshot
                                         |                |
                                  audio + effects    render/Renderer

core/  <--- game/  <--- app, platform, audio, render
```

`core/` and `game/` must not include raylib or call `IsKey*`, `GetTime`, `Draw*`,
audio APIs, or particle APIs. `GameSession` receives commands and elapsed
simulation time. It emits events and exposes a read-only snapshot. Audio and
effects consume events; rendering consumes the snapshot.

## Pull Request Sequence

Each item is intended to be independently reviewable and revertible. Except for
mechanical file moves, keep a pull request to one responsibility and roughly 300
changed production lines or fewer.

### Phase 0: Freeze and Measure the Behavior

**PR 0.1 — Gameplay invariant ledger**

- Record exact constants and outcomes for movement, reloads, projectile speed,
  wall damage, spawn warning, HP, bonuses, upgrades, settlement, and pause.
- Split the existing checks, initially in the same translation unit, into named
  stage, base/brick, player, shell, session-timing, asset, and vehicle suites so
  a failure identifies a subsystem. Preserve assertion order and return codes.
- Save expected hashes for all 35 generated tile layouts. Updating a hash must
  require a documented intentional rule or layout change.
- Record a baseline coverage report; do not introduce a whole-project coverage
  threshold yet.

Status: implemented. The current ledger is
[`GAMEPLAY_INVARIANTS.md`](GAMEPLAY_INVARIANTS.md); seventeen named suites execute
7,181 runtime checks, including 35 versioned stage-layout hashes, seeded and
scripted randomness, production-path player input, and observable event
scenarios. Ten independent core/game executables add 154 suites and
1,107 focused checks; five raylib-free app-layer executables add thirty
suites and 154 checks, for 201 suites and 8,442 checks across the aggregate
gate. The current
instrumented result is recorded in
[`COVERAGE_BASELINE.md`](COVERAGE_BASELINE.md).

Gate: `make clean`, then `make test`, ASan/UBSan, and the current manual smoke
checklist.

**PR 0.2 — Separate test build plumbing**

- Extend the Makefile from one translation unit to object lists while retaining
  the existing executable and `.app` outputs.
- Add `make test-unit`, `make test-session`, `make test-assets`,
  `make test-sanitize`, `make coverage`, and an aggregate `make test` target.
- Move the minimal `checkTest` facility to `tests/test_support.h`; do not adopt a
  network-fetched test dependency during the refactor.
- Decouple pure rule tests from the GLB and OGG presence checks; only
  `test-assets` should require runtime resources.

Status: implemented. The Makefile now builds dependency-tracked objects and
provides full, filtered, sanitizer, and production-only coverage targets.
`tests/self_tests.inl` remains textually included by `src/main.cpp` so existing
anonymous-namespace helpers keep internal linkage; moving tests to a separate
binary waits for stable rule-module interfaces. Here, `unit` means the
resource-free rule group and may include mixed rule/progression checks.

Gate: the app bundle resource manifest and self-test output remain unchanged.

### Phase 1: Create Deterministic Test Seams Before Moving Logic

**PR 1.1 — Seeded simulation randomness**

- Let tests construct `Game3D` with an explicit `std::uint32_t` seed and print
  that seed whenever a randomized scenario fails.
- Keep the default constructor behavior for normal play.
- Add repeated-run tests for enemy types, armor, bonus carriers, bonus type, and
  bonus position. Equal seeds must produce equal outcomes; different seeds need
  not produce a prescribed sequence.
- Follow with a small `RandomSource` seam for explicit probability-boundary
  tests; do not use flaky Monte Carlo assertions.

Status: implemented. Normal play seeds a private `Mt19937RandomSource` once from
`std::random_device`; tests provide an explicit seed and compare enemy type,
armor, carrier state, bonus type, and bonus position without golden
STL-specific sequences. Restart and stage changes deliberately continue the
current stream. A typed scripted `RandomSource` now verifies exact distribution
ranges, draw order, strict probability boundaries, Bandage weighting, and base
position retries without Monte Carlo assertions.

**PR 1.2 — Input snapshots**

- Introduce `PlayerInputFrame` with held directions, pressed directions, and
  fire state for both players.
- Add an update path that accepts the snapshot. Keep raylib key polling in a
  thin compatibility adapter used by the application.
- Script player movement, cardinal turning, ice travel, firing, cooldown, and
  two-player independent control in headless tests.
- Characterize the current input priority exactly: a newly pressed direction
  wins, the last checked direction wins if several are pressed in one frame,
  releasing it falls back to a still-held direction, and held fire repeats when
  the cooldown permits.

Status: implemented. `PlayerInputFrame` carries held/pressed cardinal controls
and held fire for both players. `Game3D::update` now requires an explicit frame,
while one application adapter retains the Arrow/WASD and three-key fire
bindings. Twenty-three checks cover all physical bindings, pressed and held
priority, facing retention, same-frame turn/fire, cooldown and live/impacting
shell caps, independent two-player control, ice travel, and input suppression
during stage intro, creation, and death.

**PR 1.3 — Observable game events**

- Define data-only events such as `ShellFired`, `ShellCancelled`, `BrickHit`,
  `TankDamaged`, `TankDestroyed`, `PlayerRespawned`, `BonusSpawned`,
  `BonusCollected`, `BaseDamaged`, and `StageEnded`.
- Initially record events alongside existing audio/effect calls. This makes rule
  outcomes testable without changing presentation.
- Assert event order where order is a rule, especially terrain/tank impact before
  shell cancellation and direct-fire scoring before destruction cleanup.

Status: implemented as a transitional dual-write observation seam. Ten data-only
event types record accepted firing, opposing-shell cancellation, brick and base
damage, tank damage/destruction, respawn, pickup spawn/collection, and final stage
outcomes without replacing existing audio or effects. Causes distinguish direct
player fire, enemy fire, and Grenade destruction. Forty-two production-path
checks pair each event with authoritative state, enforce physical-impact and
pickup causality, reject events for steel or an ineligible Bandage, and verify
that terminal events are not repeated on later updates.

Gate for Phase 1: a fixed seed plus scripted input must run the same 1/60-second
scenario twice and produce identical `SessionDigest` values and event sequences.
The digest excludes camera, particles, audio playback state, and wall-clock
animation.

Status: passed. Two 480-frame replays with the same explicit seed and scripted
input produce equal per-frame event counts, equal non-empty ordered event streams,
and equal versioned `SessionDigest` values. The digest serializes deterministic
session, map, entity, settlement, and random-stream state field by field rather
than hashing object memory. This is a same-build determinism gate, not a
cross-version digest compatibility guarantee.

### Phase 2: Extract Pure, Stable Building Blocks

Perform each move without cleanup; make style or API cleanup a following pull
request.

**PR 2.1 — Core coordinates and directions**

- Move `XZ`, vector operations, `CardinalDirection`, lane snapping, cardinal
  targeting, overlap helpers, and ice travel into `core/`.
- Add boundary, edge-only contact, and all-direction table tests.

Status: implemented. `src/core/coordinates.h` now owns the raylib-free `XZ`,
cardinal, center-overlap, lane-snap, targeting, and ice-travel primitives. The
main translation unit uses compatibility imports, so callers and gameplay
expectations did not change. A standalone 93-check executable covers arithmetic,
all five direction values, targeting ties, the strict 5/16 snap boundary,
positive/negative AABB contact, and 0.380-second ice transitions. The header
reaches 98.18% line and 96.25% branch coverage.

**PR 2.2 — Settings, nations, and progression**

- Move advanced-setting normalization, nation selection, player-level stats,
  and upgrade rules to `core/` or `game/rules/`.
- Make the rendering model consume the shared nation type instead of defining
  the gameplay nation type.
- Test HP 1–6, tuning -30% to +30% in 5% steps, all four levels, and all three
  nation progressions.

Status: implemented. `core/gameplay_rules.h` owns advanced-setting
normalization, HP/tuning limits, player movement/projectile constants, four
level stat rows, and Star saturation. `core/nation.h` is the only `Nation`
definition and owns selection order, normalization, cycling, and labels.
Renderer headers consume that type through a compile-time bridge while retaining
their `Vehicle`, model, muzzle, and draw responsibilities. Two standalone suites
add 132 checks for HP 1–6, all 13 tuning steps plus rounding boundaries, every
nation cycle, level boundaries, and upgrade saturation. Integrated tests directly
assert the 12 USA/USSR/Germany vehicle IDs and clamped endpoints. The new headers
both reach 100% line and branch coverage.

**PR 2.3 — Entities and bonus rules**

- Move `Player`, `Enemy`, `Shell`, `StageTally`, hit/death transitions, bonus
  type, lifetime, eligibility, and weighting to `game/`.
- Leave icon generation and 3D pickup drawing in `render/`.
- Test shield, boat, 1–6 HP, fatal hit, respawn, bandage eligibility/double
  weight, star/gun upgrades, grenade scoring, and direct-kill tally exclusion.

Status: implemented. `game/entities.h` owns entity data, stage tallies, streak
state, hit priority, death/life transitions, and enemy cleanup after owned
shells finish. `game/bonus_rules.h` is the sole owner of `BonusType`, the
12.5-second lifetime, blink threshold, classic slots, and Bandage double
weight. Since the twenty-first increment, `bonus_assets.h` re-exports the
game-owned `Pickup` with `core::XZ` and retains only 3D/icon drawing. Four
standalone entity suites add 141 checks and give both
game headers 100% region/function/line/branch coverage. Existing production
scenarios now cover every valid pickup effect and cap/max branch. Sentinel and
out-of-range bonus types are rejected by the pure rule and discarded without a
reward by orchestration.

Gate for pure modules: every `core/` and `game/` header compiles when included
by itself, every pure implementation source compiles without raylib include
paths, and a CI dependency check rejects raylib calls or includes in those
directories. `make test-architecture` enforces these requirements and the
core-to-game dependency direction; `make test-rules` runs all forty-one
standalone suites.

### Phase 3: Extract Stateful Rule Modules

**PR 3.1 — `StageMap`**

- Move generation, route validation, collision, brick masks, government walls,
  shovel steel, water, forest, and ice queries to `game/stage_map.*`.
- Remove the unused resource-path argument only in a follow-up cleanup PR.
- Preserve all 35 layout signatures and route checks.

Status: implemented as the first stateful ownership boundary.
`game/stage_map.h` and `game/stage_map.cpp` own stage wrapping, spawn-route
validation, terrain/tank/shell collision, brick masks and hit metadata,
national-base geometry and wall HP, and Shovel steel timing without raylib.
Generation moved through this boundary first and is separated in PR 3.2 below.
Compatibility imports keep callers unchanged, and the unused resource-path
argument is intentionally retained. Four standalone suites add 136 checks
covering all 35 integrated layout signatures, validated routes, strict
collision boundaries, brick damage, wall damage, and steel behavior. After the
generator split,
`stage_map.cpp` reaches 97.97% region, 100% function, 98.18% line, and 93.40%
branch coverage; `StageMap::impactShell` reaches 100% region, function, and line
coverage with 97.37% branch coverage.

**PR 3.2 — `StageGenerator`**

- Separate deterministic tile generation from mutable map state. The generator
  returns a tile grid; `StageMap` retains brick masks, base-wall HP, protection,
  collision queries, and destruction.
- Compare generator output against the 35 recorded hashes.

Status: implemented. `game/stage_generator.h` and
`game/stage_generator.cpp` now own deterministic tile generation, while
`StageMap` retains stage normalization, mutable masks, routes, collision,
protection, and destruction. Two standalone suites add 43 checks covering all
35 direct generator goldens and deterministic repeats. The existing map bridge
preserves the same layout signatures and validated routes, and
`--dump-stage-signatures` is unchanged. `stage_generator.cpp` has 100%
region/function/line/branch coverage. Coverage tests reuse canonical
instrumented implementation objects; the then-six profiles merged without
warnings.
ASan/UBSan and the versioned same-build deterministic replay gate pass.

**PR 3.3 — `CombatSystem`**

- Move shell creation, swept movement, wall/core/tank hits, shell cancellation,
  damage, death, score attribution, and destruction timers.
- Return state changes and `GameEvent` values; do not call audio or effects.
- Retain the exact collision order: terrain/base walls, government core, tanks,
  then opposing shells.

Status: in progress. The first increment added `game/combat_system.h` and
`game/combat_system.cpp` and mechanically extracted shell spawn, impact-state
transition, cancellation eligibility/overlap, continuous swept cancellation,
and the `StageMap` wall/core separation wrapper. The second introduces the
pure-data `CombatOutcome`, and `resolveShellMapImpact()` now owns the
`StageMap::impactShell` query/mutation. The third adds `CombatTarget`, explicit
core-health fields, and `resolveShellGovernmentCoreImpact()`, which owns the
core's existing binary `1 -> 0` transition. It consumes the prior map outcome
and refuses core damage after a stopping wall/terrain hit. The fourth adds
`evaluatePlayerShellEnemyTankImpact()`: a read-only evaluator that consumes the
ordered environment outcome, skips destroyed/creating enemies, applies strict
AABB selection in vector order, ignores empty armor, and snapshots exactly one
armor reduction. The fifth adds `commitPlayerShellEnemyTankImpact()`. It rejects
malformed, stale, or repeated snapshots before any side effect; invokes an
optional carrier callback before armor mutation; then owns armor, direct score,
classified tally, streak, and fatal enemy state.
The sixth adds `evaluateEnemyShellPlayerTankImpact()` and
`commitEnemyShellPlayerTankImpact()`. The evaluator skips inactive, creating,
or zero-HP players, keeps strict-AABB/vector-first selection, and snapshots the
target's ID, position, HP, Shield, and Boat state without mutation. The commit
revalidates identity, eligibility, HP, Shield, and Boat, then owns
Shield-to-Boat-to-HP priority and the fatal player transition. A narrow
pre-commit callback lets `Game3D` preserve its
generic impact effect before state mutation.
The seventh increment adds `ShellFrameSchedule`, `prepareShellFrame()`,
`advanceShellMicrostep()`, and `removeExpiredShells()`. `CombatSystem` now owns
single-frame lifetime aging, fastest-live-shell step selection, start-position
snapshots, movement eligibility, and stable expiry cleanup.
The eighth adds `ShellCancellationOutcome` and
`resolveShellCancellations()`. Within each micro-step it greedily commits the
first successful opposing pair in vector order, skips invalid or already
committed candidates, preserves wall separation, and snapshots indices,
attribution, midpoint, and both contact positions. `Game3D` retains physical
impact resolution and consumes the returned cancellation results before the
next micro-step.
The ninth adds `ShellPhysicalImpactResult` and
`resolveShellPhysicalImpact()`. The resolver owns the live-shell guard and
composes map, core, enemy evaluation/commit, and player evaluation/commit in the
established order for exactly one shell. Map/core hits are authoritative; tank
hits set `resolved` only after validated commits. Carrier release and generic
player-impact callbacks keep their pre-mutation timing, and the input shell
remains unchanged for immediate presentation.
The tenth moves the event enums and detached value payload into
`game/game_event.h`, then adds `eventsForPhysicalShellImpact()`. The projector
uses only the typed result snapshot and emits ordered brick, wall/core, and
committed tank events. It emits nothing for unresolved, steel, boundary,
dead-core, Shield, or Boat outcomes. `Game3D` appends each returned batch at the
same historical branch point. Fatal-enemy events remain after explosion,
camera shake, and audio; nonfatal-enemy events remain before presentation;
player damage events follow generic pre-commit FX but precede hit-specific
camera/audio presentation.
The eleventh adds `eventForShellCancellation()`. It maps one detached resolver
snapshot to one `ShellCancelled` event, preserves midpoint and both source-ID
domains, and leaves non-cancellation fields at their defaults. A narrow
`Game3D::consumeShellCancellation()` keeps the established per-pair event,
midpoint impact FX, then `BulletHit` audio sequence; no batch is emitted ahead
of presentation. A nonzero-time production update locks the swept crossing,
midpoint event payload, source IDs, and committed shell states end to end.
The twelfth adds a cancellation-only presentation record and default-off
observer in the app layer. `consumeShellCancellation()` records after the real
event append, impact spawn, and `BulletHit` request, using the same local values
passed to those actions. Production-path tests lock the full single-pair payload
and exact `E-FX-A / E-FX-A` two-pair interleaving; normal play retains no trace.
The thirteenth adds a separate default-off StageMap/GovernmentCore presentation
observer in the app layer. It reports only after real event-batch, semantic FX,
camera, audio-request, or final shell-impact actions. Eight production-path
checks lock player brick, enemy wall, protected-wall owner differences, player
boundary, final wall breach, same-frame live/dead core, and enemy-core
consumption without asserting visual particle counts.
The fourteenth adds paired, default-off tank observers. The result observer
reports only after real event-batch, armor FX, explosion, camera, audio-request,
or final-impact actions; a player-only hook records the generic armor FX at its
historical pre-commit point. Six production-path checks lock enemy nonfatal and
fatal asymmetry plus Shield, Boat, player damage, and player destruction. A
seventh proves that a later shell against an already-destroyed enemy stays
flying and emits no tank trace. Every other pre-final snapshot also retains the
uncommitted shell contact and exact incoming velocity.
The fifteenth introduces an owned app-layer `ShellTankPresentationCommand` and
one synchronous consumer for all six post-result tank paths. Its fixed-capacity
variant actions carry event values, armor/explosion parameters, radial or target
camera shake, audio requests, and the final contact without retaining entity or
shell references. The player generic armor action remains synchronously
consumed in the validated pre-commit callback. Existing production checks now
also lock exact payloads, direct camera assignment from a larger prior value,
two-player target index 1, and radial `max` behavior for the other player;
normal play retains no command queue.
The sixteenth extracts that value contract into
`app/shell_tank_presentation.{h,cpp}` and moves the unchanged cue enum into
`audio/audio_cue.h`. App-owned `Float3` and `Rgba8` payloads keep the command,
step mapping, six-branch factory, and player pre-commit armor projection
raylib-free. `Game3D` retains only POD conversion and the concrete synchronous
side effects. Three independent suites add 18 order, payload, and
malformed-input checks while the existing production-path observers continue to
verify real effects, camera, audio requests, and final shell timing.
The seventeenth extracts cancellation presentation into
`app/shell_cancellation_presentation.{h,cpp}`. Its fixed command owns the
already-projected event, midpoint impact parameters, and `BulletHit` request;
the optional factory rejects every non-cancellation event. `Game3D` consumes
event, FX, then audio synchronously for each outcome and does not recommit the
two shells already committed by the resolver. Two independent suites/checks
lock the valid payload and all nine invalid event types, the implementation has
100% region/function/line/branch coverage, and post-action snapshots preserve
both whole-batch impact state and `E-FX-A / E-FX-A` interleaving.
The eighteenth extracts map/core presentation into
`app/shell_map_core_presentation.{h,cpp}` and moves the shared renderer-neutral
`Float3`/`Rgba8` values into `app/presentation_values.h`. A fixed-capacity owned
variant carries map/base events, exact FX values, radial camera shake, requested
cues, and final contact. The optional factory rejects mismatched events,
invalid source/coordinate data, contradictory tank commits, impossible brick
mask changes, and government-wall HP transitions that violate normal/power
damage. `Game3D` consumes and observes each action synchronously after the real
side effect; a failed canonical projection commits the shell fail-closed. Three
independent suites add 22 branch, payload, step, and malformed-boundary checks;
the implementation reaches 99.37% line and 94.63% branch coverage.
The nineteenth deliberately characterizes the next stateful boundaries before
moving ownership. Natural settlement counting and all completion outcomes now
run through public updates or confirmations, including the 5.0-second report
hold, stage 35 wrap, exhausted/surviving/capped lives, record selection across
two players, menu requests, and the 5.2-second record screen. Every valid bonus
effect now runs through production collection, including max-duration/cap
branches and injured Bandage. A malformed `BonusType::Count` pickup formerly
awarded 300 points as `UNKNOWN`; the shared rule now rejects sentinel and
out-of-range types and orchestration drops them without an event or score.
`finishSettlement()` reaches 100% line/branch coverage and `applyBonus()` reaches
96.94% line and 94.74% branch coverage. No system was extracted in this
increment: settlement still owns stage loading/menu/audio transitions, while
bonuses still own raylib position, map, FX, camera, and audio side effects.
The twentieth extracts the already-characterized report state into
`game/settlement_system.{h,cpp}`. `SettlementState` owns its fixed participant
snapshot, score/K.O. cadence, confirmation, five-second Idle timeout, and the
presentation-free `SettlementCompletion`; `SettlementUpdate` reports how many
count steps occurred so `Game3D` can request the unchanged cue. `Game3D` retains
`StageEnded`, audio, high-score/menu policy, stage loading, and all live-player
mutation. Invalid participant counts fail inactive, invalid elapsed time is
ignored, and the score helper saturates rather than overflowing at `INT_MAX`.
Eight standalone suites add 55 checks and cover the module at 100% in all four
metrics. Automatic next-stage loading also became transactional: a candidate map
is committed only after validation; failure restores stage and players, preserves
the error, issues one menu notification, and keeps the session frozen through
`awaitingMenu` after notification consumption. This is a narrow partial PR 3.5
step, not completion of `BonusSystem` or the coupled settlement consumers.
The twenty-first implements the next PR 3.5 boundary. `Pickup` moves from
`bonus_assets.h` to `game/bonus_system.h` and replaces raylib `Vector3` with
`core::XZ`; the renderer now aliases that game-owned value and supplies only
local display heights. The compiled reducer validates weighted spawn draws,
advances lifetime before collection, chooses the first eligible strict-AABB
collector, saturates common/Grenade score and bonus tally, and owns every
player/enemy bonus mutation. Ordered detached commands preserve Shovel map
activation and Grenade armor cues, destruction audio, explosion, event, and
camera assignment while concrete consumers remain in `Game3D`. The production
path retains type-first RNG and complete x/z rejection pairs, collection-event
precedence, stacked-pickup behavior, and the v1 digest's historical zero-height
field. Twenty standalone suites add 142 checks and cover the implementation at
100% in all four metrics.
The twenty-second completes the planned narrow side-effect seam without moving
domain state or changing command order. A raylib-free `CommandSideEffectSink`
and compiled dispatcher now receive event, impact/brick/explosion FX,
radial/assigned camera, and audio leaves from cancellation, map/core, tank, and
Bonus commands. `Game3D` implements the sink synchronously; final shell impact
and Shovel steel remain immediate domain commits, and player armor FX stays
before its combat commit. Audio sentinels fail closed while tank-camera rejection
and Bonus-camera ignore retain their distinct behavior. Fourteen standalone
suites add 73 checks for all alternatives, exact payload/order, rejection, and
sink-free domain commits. The dispatcher reaches 100% region/function/branch
and 99.55% line coverage.
Map, core, and tank evaluators leave `Shell` unchanged; the cancellation batch
commits both successful shells to their impacting state. The callbacks let `Game3D`
retain bonus RNG, pickup creation, non-physical events, audio, effects, and
camera behavior without importing those dependencies into the projector.
`Shell` and its owner enum remain in `entities.h`.

`Game3D` consumes one typed physical result immediately. On an enemy hit it
supplies the validated carrier callback, then builds and synchronously consumes
the typed tank command. On a player hit it consumes the typed generic armor
action before commit, then builds the Shield/Boat/HP-specific command. The sole
tank consumer owns the projected event, FX, camera, audio-request, and final
`beginShellImpact()` operations in their established order. Cancellation
outcomes similarly build and synchronously consume the fixed cancellation
command. Map/core outcomes build and synchronously consume their owned command
with final impact last. The compatibility orchestrator now consumes the pure
frame schedule, movement/cleanup, single-shell physical transaction,
cancellation-batch helpers, and all three command-backed physical presentation
paths. Their common command leaves now cross one synchronous app-layer sink;
PR 3.3 remains open for extracting that concrete sink implementation and final
shell impact ownership from `Game3D`.
`ImpactKind::None` is intentionally ambiguous:
a power shell can clear forest and continue, while a government-core hit stops.
`CombatTarget`, `mapStopsShell()`, and `stopsShell()` carry those decisions
explicitly.

The 211-line header and 616-line implementation are covered by the 2,510-line
`tests/combat_system_tests.cpp`: twenty standalone suites and 132 checks.
`combat_system.cpp` reaches 100% region/function/line coverage and 97.52%
branch coverage. At that increment, the 126-line `game_event.h` reached 100% in
all four metrics; the CombatSystem coverage driver was instrumented for its
inline value branches, and all fourteen profiles merged without warnings.
Resolving and consuming one shell at a time preserves terrain/base, core, tank,
then opposing-shell priority. The then-6,931-line `main.cpp` and 8,482-line
transitional self-test included default-off cancellation, map/core, and tank
observers, with two
cancellation-order checks, eight map/core-order checks, six tank-order checks,
and one rejected-target silence check. Tank checks additionally inspect every
command payload and two-player camera routing. All three proven physical
presentation command boundaries now live behind `app/` implementation seams;
audio, effects, camera behavior, and the final impact side effect remain outside
`game/`.

**PR 3.4 — `EnemySystem`**

- Move target choice, local escape, cardinal movement, frozen state, firing,
  spawn selection, warning time, armor, and carrier generation.
- Consume the injected random source and settings explicitly.

**PR 3.5 — `BonusSystem` and `SettlementSystem`**

- Move random pickup creation, collection, expiry, effect application, stage
  tally animation state, stage transition, and high-score transition.
- Preserve the rule that grenade-cleared enemies do not appear in classified
  K.O. rows.
- Treat `BonusType::Count` and out-of-range values as invalid input, never as a
  collectible; preserve the characterized settlement and valid-pickup
  completion matrices while moving ownership.

Status: PR 3.4 is complete and PR 3.5 is partially implemented through the
forty-first increment. `planSettlementBegin()` owns pure report-entry
validation, reason classification, and a detached score/tally snapshot. The pure
`SettlementState` owns snapshot/count/confirm/timeout state and completion, while
`planSettlementTransition()` owns strict high-score selection, stage wrap, and a
detached lives/level progression snapshot. The
compiled `BonusSystem` owns XZ pickup state, spawn-draw validation, the complete
carrier-release transaction, lifetime/collector decisions, the per-pickup
`U-B-A-F` collection/application transaction, stable vector advancement,
scoring, player/enemy effects, and ordered side-effect commands. All detached command
leaves share one tested synchronous app-layer sink. Concrete settlement events,
audio stops and count cues, display, menu navigation, candidate-map loading, and
live-player commits remain in `Game3D`; the bonus seeded-random adapter,
authoritative base predicate,
concrete pickup/event/audio phases, map mutation, messages, FX, and camera remain
at its orchestration edge. The compiled
`EnemySystem` slices now own target,
pursuit-axis, local escape selection, type-specific fire planning, Armor-fire,
three-slot spawn-selection policy, spawn probability/armor rules, detached
spawn-state construction, and the four-phase frame-entry lifecycle/timer gate.
It also owns the active cardinal/ice movement transaction, detached dust intent,
and active steering transaction, including conditional pursuit/wander callback
pulls and atomic escape commits. The complete spawn-attempt transaction now owns
queue/identifier exhaustion, guard/timer order, rotated availability queries,
blocked retry, ID reservation, conditional random pulls, detached construction,
synchronous insertion, and the remaining/rotation/timer commits. `EnemySystem`
also owns the complete post-movement firing transaction: the due gate,
conditional `R`/`R-Q`/`R-Q-C` transcript, type/Armor reload, setting
normalization, owned-shell suppression, detached shell intent, synchronous
launch callback, and cooldown commit after launch. `Game3D` retains the
map-derived on-ice fact, tuned configuration, live occupancy and seeded-random
callbacks, synchronous enemy/shell container insertion, and concrete FX, event,
audio-observation, and renderer-facing adapters.

Gate for Phase 3: unit tests cover each module directly, and an integration test
runs a complete deterministic stage lifecycle without opening a window.

The event and scenario seams now cover every valid bonus plus settlement entry
and completion branch; the pure modules have direct standalone coverage.
Simulation pickups now use `XZ`, render heights exist only in the renderer, and
effect decisions are separated from the shared map/audio/FX consumer boundary.
EnemySystem target/fire/spawn-retry and lifecycle branches are now characterized
through the production path, and its policy, lifecycle, escape, fire-plan,
movement, steering, spawn, and firing transactions have direct 100% coverage
across 43 suites and 120 checks. BonusSystem also retains direct 100% coverage
across 33 suites and 163 checks; production cases lock stable vector erasure,
its exact `E0-A-C-M-O-Ep-D` order, and sequential stacked-Bandage behavior. Enemy tests
lock +30% fire tuning, the
ice-carried aim/launch direction distinction, occupied/distractor shell cases,
concrete shell-event-muzzle order, and enemy-fire audio silence. Settlement entry
and completion now have detached plans and characterized default-off concrete
consumer seams. SettlementSystem has 14 suites and 89 checks at 100% coverage;
entry cases lock all reasons, atomic invalid input, event append order, and
post-commit `ScoreCounted` requests without synthetic confirm cues. A separate
raylib-free `AudioOutput` interface now lets `Game3D` delegate its three runtime
audio operations through a nullable, non-owning pointer while `main` retains
stack ownership, load/unload, device lifetime, and `AudioBank` policy. Headless
recording tests lock post-commit stage start, creating/ready engine state,
pause stop/cue/resume silence, and settlement stop ordering; they do not claim
hardware playback or concrete voice-priority coverage. The compiled
`PlayerSystem` now owns the input frame values, the five-clock
`Inactive`/`Creating`/`Ready` frame-entry transaction, the inactive scalar
death-timer/lives transaction, final
drive/propulsion/fire-held planning, the active scalar cardinal/ice movement
transaction and detached dust intent, and the post-movement scalar firing
transaction. Frame entry rejects invalid elapsed time atomically, selects phase
from its entry snapshot, and retains ordered `std::max` behavior for malformed
clocks. Inactive death rejects invalid elapsed time atomically, preserves entry,
cross-zero, and malformed-timer behavior, and saturates non-positive life counts
without `INT_MIN` overflow. Movement rejects invalid or overflowing input
atomically, performs an optional exact lane-snap query before its forward query,
preserves ice and blocking behavior, and returns presentation-free dust data.
`Game3D` commits the returned clocks before phase-specific work, commits death
timer/lives before respawn side effects, clears moving in Creating, and reads
input only in Ready. The entity-free spawn/reset transaction owns nineteen
scalar/value fields, validates mode, position, and durations before an atomic
commit, and preserves the legacy `Preserve`/`Reset` progression and malformed-HP
rules. `Game3D` retains ID-to-position mapping, entity snapshot/writeback,
slot-owned shell cleanup, respawn events/audio, map-derived ice and live
occupancy/Boat adapters, scalar movement writeback, dust presentation, and the
concrete fire adapter. Owned-shell cleanup, spawn-point adaptation, events,
audio, entities, map, rendering, cameras, and concrete movement/fire
orchestration and presentation adapters remain in `Game3D`.

Keep shared national-base
collision and visible geometry together until tests prove that each rendered
wall segment matches its physical segment.

### Phase 4: Establish the Session Boundary

**PR 4.1 — `GameSession` compatibility facade**

- Compose `StageMap`, combat, enemy, bonus, and settlement systems behind the
  current `Game3D` public query API.
- Keep `Game3D` as a temporary alias or facade so rendering changes are not mixed
  with simulation changes.

**PR 4.2 — Snapshot and event consumers**

- Introduce `GameSnapshot` containing only renderable, read-only state.
- Move `BattleFx` and camera shake to an effects consumer. Move audio cue mapping
  to an audio consumer.
- Delete the temporary dual-write event path after event and presentation tests
  prove parity.

Gate: the core session links and runs with no raylib library. The normal app still
produces all 22 expected runtime cues through event mapping.

### Phase 5: Mechanical Presentation and Application Split

**PR 5.1 — Renderer and HUD**

- Move world rendering, national bases, minimap, HUD, settlement, and high-score
  rendering to `render/` implementation files.
- Move code without redesigning geometry or output. Preserve QA showcase modes.

**PR 5.2 — Resource owners**

- Move `AudioBank`, `SceneLighting`, `ViewTargets`, `EnvironmentAssets`, and
  `TankAssets` to implementation files.
- Upgrade non-copyable owners to move-only RAII and make repeated load/unload
  safe. Test empty, failed, repeated, and moved ownership where possible.

**PR 5.3 — Menus and process entry**

- Move setup and advanced menus to `app/menu.*`, raylib keyboard mapping to
  `platform/raylib_input.*`, and retain only lifecycle wiring in `app/main.cpp`.
- Update Makefile source lists and application-bundle packaging.

Gate: `app/main.cpp` performs wiring rather than gameplay, and incremental builds
compile only changed modules.

## Test Matrix and Required Gates

| Layer | Automated checks | Required behavior |
| --- | --- | --- |
| Pure rules | Table-driven unit tests | settings, directions, HP, upgrades, scoring, probabilities |
| Map | Generator goldens and mutable-map checks | 35 direct and bridged layouts, open spawns/routes, exact brick/steel/base damage |
| Combat | Fixed-step scenarios | muzzle/wall edge cases, physical-hit priority, opposing-shell cancellation |
| Session | Seeded scripted integration | spawn cadence, AI escape, pickups, death/respawn, event order, digest, settlement |
| Resources | Manifest and ownership tests | 22 cues, two textures, one GLB, no duplicate unload |
| Presentation | Manual QA/showcases | one/two player, pause/Esc, camera, forest cover, all bases, effects, report |

Coverage policy:

- Phase 0 reports coverage without blocking.
- Every newly extracted pure rule module must reach at least 85% line coverage
  and 75% branch coverage.
- Newly added core rule code should reach 90% line coverage.
- Every bug fix must first add a failing regression test against the production
  path.
- Whole-core coverage is ratcheted upward; a pull request may not reduce it by
  more than one percentage point without an explicit review note.
- Raise whole-repository coverage toward 70% over the migration, excluding test
  code and embedded shader strings; do not block Phase 0 on that final target.
- GPU screenshot comparison remains advisory because driver output varies. Use
  stable QA scenes and human review for visual parity.

CI runs warning-clean optimized and warning-as-error debug builds, the core/game
header/source/dependency-direction gate, the full and filtered self-tests,
ASan/UBSan, production coverage reporting, and the exact app-bundle resource
manifest check. It also builds the self-contained macOS Alpha archive and
revalidates its checksum, metadata, static dependency boundary, ad-hoc signature,
license manifest, and full self-test after extraction. Developer ID signing and
notarization are deliberately outside this certificate-free CI gate.
Run the manual smoke checklist before merging any presentation change.

The minimum scripted integration scenarios are:

- firing while the muzzle touches a wall and enforcing active-shell limits;
- terrain or tank impact taking priority over shell cancellation;
- three hits, streak reset, 490 ms destruction, and respawn;
- Star upgrade, one-HP Bandage disable, and grenade exclusion from classified
  K.O. counts;
- a bonus carrier releasing a pickup on every valid hit;
- the five-second stage-end window still allowing player or base destruction;
- the last life remaining present until its in-flight shell finishes;
- enemy corner escape and all three +/-30% advanced settings;
- two-player collision and friendly shells passing through each other.

For visual regression, retain the existing tank, bonus, forest, base-damage,
base-steel, enemy-creation, settlement, and advanced-settings showcase modes.
The default-off release-capture path now supplies a fixed gameplay seed and
normalized 1280x720 export. Scene animation still uses the live rendering clock,
so GPU screenshots remain human-review artifacts with a tolerant perceptual
comparison, not an exact-pixel merge gate.
During the settlement showcase, also verify by ear that the active engine loop
stops at report entry, each count cue aligns with a visible step, and confirming
the report does not replay skipped count cues.

## Rollback and Review Rules

- Tag or branch the known-good baseline before Phase 1.
- Never combine a file move, behavior change, and visual redesign in one pull
  request.
- For mechanical moves, compare with whitespace ignored and require no changed
  test expectations.
- Keep compatibility facades until all callers and tests have migrated.
- If a step changes deterministic state or event output unexpectedly, revert the
  step rather than updating golden expectations without a documented rule change.
- Merge one subsystem extraction at a time; do not keep long-lived parallel
  rewrites of `Game3D`.

## Completion Criteria

The refactor is complete when gameplay tests run without raylib, input and RNG
are deterministic in tests, core rules emit events rather than presentation side
effects, tests no longer live in `main.cpp`, and application startup only wires
menus, session, renderer, audio, and resources. The procedural vehicle and base
files may remain large visual modules; file size alone is not a reason to rewrite
stable rendering code.

The implementation batch through PR 3.2 is complete. The Phase 1
deterministic-session gates, Phase 2 pure core/game boundaries, and isolated
`StageGenerator`/`StageMap` modules are green. PR 3.3 is in progress: its pure
`CombatSystem` extraction, physical and cancellation event projection, and the
cancellation, map/core, and enemy/player tank presentation seams are green. The
tank, map/core, and cancellation paths now use owned commands and pure mappers
in `app/`, with synchronous concrete consumers in `Game3D`. The nineteenth
increment characterizes `finishSettlement()` and every valid `applyBonus()`
variant; the twentieth moves the pure settlement snapshot/count/confirm/timeout
machine and completion value into `game/` with direct 100% coverage. The
twenty-first moves XZ pickup state, spawn decisions, collection/lifetime,
scoring, and all player/enemy bonus effects into the compiled `BonusSystem`,
also with direct 100% coverage. The twenty-second unifies the four command
families behind the synchronous app-layer side-effect sink and directly tests
every leaf/rejection/domain-commit result. The twenty-third adds the first pure
PR 3.4 EnemySystem policy slice plus direct and production-path coverage for the
formerly uncovered target, pursuit, Armor-fire, and blocked-spawn branches.
The twenty-fourth adds the frame lifecycle/timer gate and production checks for
Destroyed, Creating, Frozen, Active, and showcase-hold behavior.
The twenty-fifth moves local escape selection and post-movement fire planning
into `EnemySystem`. Direct tests lock closed paths, probe counts, exact query
transcripts, progress/tie scoring, snapping, type reload multipliers, Armor
gating, roll boundaries, and input atomicity. Production tests lock successful
and failed escape commits plus per-enemy `R-R-R-R` then `R-R-I-R` conditional
draw ordering while retaining the existing effect order.
The twenty-sixth moves strict type/carrier/armor probabilities and full detached
enemy initialization into `EnemySystem` without moving random draws or the
spawn transaction. Twenty-one direct suites now contribute 82 checks at 100%
module coverage. Production checks lock exhausted, cooling-down, four-slot-full,
blocked-rotation, exact-zero, tuned-success, and fully blocked paths, including
complete field/ID/count/rotation/timer commits and zero RNG on every rejected
path. The release candidate evidence schema also advances to v2: its builder
self-test is now an eighth attested gate and runs independently in macOS CI.
The twenty-seventh moves the active cardinal/ice movement transaction into
`EnemySystem` without moving steering, map ice classification, concrete FX, or
the fire draw. Seven new direct suites lock invalid-input atomicity, exact
snap/probe query order, forward and zero-distance commits, ice continuation and
expiry, blocked state/cap, and detached dust payloads. The module now has 28
suites and 101 checks at 100% coverage. Four production checks retain movement
then dust then reload behavior and the blocked Armor same-frame fire override.
The twenty-eighth moves the active steering transaction and escape commit into
`EnemySystem`. `advanceActiveEnemySteering()` owns the strict timer/blocked
gates, conditional `R-R-R` pursuit and `R-R-I` wander callback pulls, ordinary
direction and interval updates, local escape selection, and the atomic route,
blocked, yaw, and ice-state commit. Four new direct suites lock invalid-input
atomicity, exact boundary behavior, zero-draw paths, callback/state ordering,
all four random-direction mappings, and the complete escape success/failure
transaction. The module now has 32 suites and 105 checks at 100% coverage.
Production checks exercise both escape outcomes through `updateEnemies()` and
retain the 16-suite/7,125-check integrated result and per-enemy draw transcript.
The twenty-ninth moves the complete spawn attempt into `EnemySystem`.
`advanceEnemySpawnTransaction()` validates inputs atomically, preserves
queue and identifier exhaustion before debit, then capacity and strict-positive
cooldown gates after debit. It queries rotated slots only until the first open
point and commits the blocked retry with no RNG, insertion, ID, count, or
rotation change. An accepted slot
reserves its ID before the conditional `R-I-R-R` regular or `R-R-R` Armor draws,
constructs the complete detached enemy, invokes the synchronous insertion
callback, and only then commits remaining count, rotation, and the normal timer.
Five new direct suites add seven checks, bringing the module to 37 suites and
112 checks. Its 283 regions, 23 functions, 441 lines, and 238 branches all reach
100% coverage. Production checks retain the tuned, exhausted, identifier-limit,
capacity, blocked, regular, and Armor paths and bring the integrated result to
16 suites and 7,127 checks. A fixed session-digest prefix locks the serialized
spawn-state order as remaining count, rotation index, next ID, then timer.
The thirtieth completes PR 3.4 by moving the post-movement fire transaction into
`EnemySystem`. The module now owns its exact due/roll/query gates, conditional
`R`, `R-Q`, and `R-Q-C` callback transcripts, Armor/type reload calculation,
normalized advanced setting, owned-shell slot rule, complete detached `Shell`
intent, synchronous launch callback, and cooldown commit afterward. `Game3D`
retains live occupancy and seeded-random adapters plus the concrete
shell-event-muzzle consumer. Default-off production observers prove that order
and enemy-fire audio silence; real update cases cover +30% fire tuning,
movement-direction Armor aim while ice carries a different drive-direction
launch, impacting zero-life owned-shell suppression, and unrelated distractors.
Six direct suites add eight checks, bringing `EnemySystem` to 43 suites and 120
checks; its 314 regions, 24 functions, 490 lines, and 262 branches all remain at
100% coverage. The integrated result is 16 suites and 7,129 checks.
The thirty-first starts the carrier-release slice of PR 3.5.
`advanceBonusReleaseTransaction()` snapshots healing eligibility, owns the
single type draw plus complete X/Z/rejection retry loop, validates malformed
draws after each complete attempt, and synchronously commits a detached
`BonusReleaseIntent`. `Game3D` retains the seeded-random/STL adapter and
authoritative government-base query, then preserves pickup insertion,
`BonusSpawned`, and `BonusAppeared` before the enemy armor/score/death commit.
Five direct suites add eleven checks, bringing `BonusSystem` to 25 suites and
153 checks; its 139 regions, 10 functions, 220 lines, and 108 branches all reach
100% coverage. The integrated result is 16 suites and 7,130 checks, and the
aggregate gate is 147 suites and 8,250 checks. A malformed release remains
side-effect-free without aborting the valid carrier hit.
The thirty-second extracts the per-pickup `U-B-A-F` transaction into
`BonusSystem`: update clock/eligibility, begin with a detached intent, apply
rule state, and finish synchronously. Missing callbacks are atomic;
retain/discard invokes no callback; defensive application rejection rolls back
the staged event and retains the pickup. `Game3D` preserves concrete
`E0-A-C-M-O-Ep-D` ordering and performs erasure only after the transaction, so
stacked pickups remain sequential. Four direct suites add five checks, bringing
`BonusSystem` to 29 suites and 158 checks; its 154 regions, 11 functions, 250
lines, and 118 branches remain fully covered. Production Grenade, Tank, and
two-player stacked-Bandage and same-frame double-Grenade cases bring the
integrated result to 16 suites and 7,134 checks and the aggregate gate to 151
suites and 8,259 checks.
The thirty-third moves settlement completion decisions into the raylib-free
`SettlementSystem`. `SettlementTransitionPlan` owns strict high-score comparison,
stage advance/wrap, survivor life gain/cap, and eliminated-player recovery on a
detached snapshot. `Game3D` first prepares and validates a candidate map, then
commits planned stage/player progression and the prepared world. Loader failure
or a candidate that reports the wrong stage commits neither; failed `start()` or
`restart()` also restores the prior configuration, nations, stage, players, and
events. A default-off synchronous observer locks the exact record, non-record,
successful-stage, and rejected-stage presentation orders without retaining a
runtime trace. Concrete StageMap, audio, display, and menu work remains in
`Game3D`, and dependency checks keep SettlementSystem independent of StageMap.
Three direct planner suites add ten checks, bringing SettlementSystem to eleven
suites and 65 checks with 100% region/function/line/branch coverage. Six
integrated checks bring the game result to 16 suites and 7,144 checks and the
aggregate gate to 154 suites and 8,279 checks. They compare complete player
state, capture the exact `StageStart` request-time state, and exercise rejected,
wrong-stage, different-configuration start, restart, and manual-stage loader
paths.
The thirty-fourth moves the remaining stable pickup-vector loop into
`BonusSystem`. `advanceBonusPickups()` preserves one `U-B-A-F` transaction per
element, erases collected and discarded pickups at the current index, processes
shifted successors without skipping, keeps rejected pickups, and leaves discard
silent. Missing begin/finish callbacks reject the whole batch atomically; the
optional collected callback runs after erasure with detached values. `Game3D`
retains the concrete `E0-A-C-M-O-Ep-D` callback consumers. A real
nonzero-delta carrier hit proves that the newly released pickup is aged and
collected in that same frame. Four direct suites add five checks, bringing
BonusSystem to 33 suites and 163 checks; its 171 regions, 12 functions, 279
lines, and 134 branches remain at 100% coverage. The integrated result reaches
16 suites and 7,145 checks and the aggregate gate reaches 158 suites and 8,285
checks.
The thirty-fifth moves settlement-entry decisions into `SettlementSystem`.
`SettlementBeginPlan` validates stage/player scope and contradictory clear state,
classifies clear, base loss, or player defeat, and owns an exact detached
score/tally snapshot plus the event-emission intent. `Game3D` consumes it in
`PlanReady` -> `ReportCommitted` -> optional `StageEndEventAppended` ->
`AudioStopBoundaryPassed` order. The event-free showcase and all three end
reasons are covered, existing update events retain their order, count-audio
requests observe committed report state, and confirmation remains silent. Three
direct suites add 24 checks, bringing SettlementSystem to 14 suites and 89
checks; its 170 regions, 21 functions, 251 lines, and 126 branches remain at
100% coverage. Four integrated checks bring the game result to 16 suites and
7,149 checks and the aggregate gate to 161 suites and 8,313 checks.
The thirty-sixth adds the raylib-free `audio/audio_output.h` seam with exactly
`play`, `updateEngine`, and `stopAll`. `Game3D` changes only from a concrete bank
pointer to a nullable, non-owning `AudioOutput *`; normal `main` continues to
stack-own `AudioBank`, bracket its load/unload with the device lifetime, and own
its cue-priority and voice policy. A recording implementation verifies one
post-commit `StageStart`, creating-player mute then ready moving-engine state,
pause `stopAll` before `Pause` plus silent resume, and settlement stop only after
the `StageEnded` event is appended. It deliberately does not claim hardware
playback or exercise concrete bank voice arbitration. Three new integrated
checks form one named suite, while a strengthened existing settlement sentinel
locks the final stop boundary. The game result reaches 17 suites and 7,152
checks and the aggregate gate reaches 162 suites and 8,316 checks. Coverage
reaches 48.60% regions,
62.28% functions, 43.98% lines, and 51.15% branches overall; `main.cpp` reaches
23.60%, 58.27%, 29.11%, and 21.69%, while the one-region/one-function/one-line
`audio_output.h` contract is fully covered with no applicable branches.
The thirty-seventh adds `game/player_system.h/.cpp` and moves
`DirectionButtonFrame`, `PlayerControlFrame`, and `PlayerInputFrame` out of
`main.cpp`. `planPlayerControl()` plans only final drive, propulsion, and
held-fire intent. The consumer retains the existing death/creation/cooldown
gates, unconditional drive commit, propelling-only yaw, ice/map/snap/collision/
dust order, and fire last. Three direct suites contain 13 checks; independent
literal tables exhaust five valid current directions x 16 held masks x 16
pressed masks x two fire states, or 2,560 combinations (not checks). Two
additional checks lock legacy invalid `5`/`255` parity rather than fail-closed
behavior. Three integrated checks cover the pressed-only consumer and the
moved-position fire fixture/order. At that increment there were 17 integrated
suites and 7,155 checks, 126 core/game suites and 1,062 checks, 22 app suites
and 115 checks, or 165 suites and 8,332 checks overall. Fifteen profiles merged.
The 39 production files contained 18,545 lines; `main.cpp` was 7,007 lines,
`self_tests.inl` was 9,918 lines, and the pure boundary was 13 headers plus seven
implementations. Coverage was 48.74%/62.39%/44.00%/51.32% overall and
23.05%/58.10%/28.61%/20.78% for `main.cpp`; all 35 regions, three functions, 42
lines, and 34 branches in `player_system.cpp` were covered at that increment.
The thirty-eighth adds `advancePlayerFireTransaction()` without widening the
module's dependencies. A scalar parameter snapshot carries request state,
active-shell count, owning player index, raw level, post-movement tank position,
drive direction, and explicit reload/spawn distances. Static invalid input is
atomic; idle and cooling attempts are inert; a valid due attempt resets the
cooldown before rejecting a full slot or synchronously publishing a detached
launch intent. The direct matrix preserves clamped level statistics and
`None`/`5`/`255` zero-vector parity. `Game3D` constructs and inserts the concrete
Shell, then appends the event, spawns muzzle FX, commits camera shake, and
requests audio in the original order. Full-slot rejection remains silent while
impacting shells occupy capacity. The moved-position and ice-carried cases lock
post-movement position and drive-direction firing. At that increment,
PlayerSystem had eight direct suites and 22 checks; the scripted-player-input
integrated suite had 32
checks. At that increment, totals were 17 integrated suites and 7,161 checks, 131
core/game suites and 1,071 checks, 22 app suites and 115 checks, or 170 suites
and 8,347 checks overall across fifteen profiles. The 39 production files
contained 18,716 lines; `main.cpp` was 7,078 lines, `self_tests.inl` was 10,237
lines, `player_system.h/.cpp` were 101/105 lines, and the 493-line direct test was
excluded from the production denominator. The pure boundary remained 13 headers
plus seven implementations.
Coverage was 48.95%/62.61%/44.30%/51.52% overall and
23.12%/58.59%/29.10%/20.72% for `main.cpp`; all 60 regions, four functions, 74
lines, and 54 branches in `player_system.cpp` were covered.
The thirty-ninth adds `advanceActivePlayerMovement()` as an entity-free scalar
transaction.
Finite nonnegative elapsed time and speed are multiplied and checked before any
state or availability query; a missing callback and overflow reject atomically.
For valid attempts it commits drive/yaw and the existing ice transition, then
issues the optional exact lane-snap query before the forward-candidate query. A
blocked candidate retains an accepted snap, stops movement, clears ice slip,
and restores travel to drive direction. Accepted movement can return a detached
dust position/velocity intent without owning its cooldown. `Game3D` supplies
`StageMap` ice and live occupancy with Boat permission, writes scalar state,
presents dust and commits the 0.120-second dust clock, then fires. The 38-check
production suite locks that adapter, blocked same-frame fire, water with and
without Boat, and ordered P1-then-P2 occupancy. At that increment, PlayerSystem
had 14 direct suites and 40 checks. Totals were 17 integrated suites and 7,167
checks, 137 core/game suites and 1,089 checks, and 22 app suites and 115 checks:
176 suites and 8,371 checks across fifteen profiles. The 39 production files
contained 18,865 lines; `main.cpp` was 7,086 lines, `self_tests.inl` was 10,424
lines, `player_system.h/.cpp` were 166/181 lines, and the 1,046-line direct test
was excluded from the production denominator. The pure boundary remained 13
headers plus seven implementations. Coverage was
49.19%/62.78%/44.63%/51.80% overall and
23.00%/58.91%/29.28%/20.43% for `main.cpp`; all 96 regions, five functions, 137
lines, and 86 branches in `player_system.cpp` were covered.
The fortieth adds the entity-free `beginPlayerFrame()` transaction for the five
creation, fire, dust, shield, and streak clocks. Invalid elapsed time is atomic.
For valid input, phase comes from the entry snapshot: all phases debit the three
shared clocks, Inactive preserves creation/fire, Creating debits creation only,
and Ready debits fire only. Crossing creation through zero stays Creating, and
malformed clocks retain the legacy ordered-`std::max` results. `Game3D` commits
all returned clocks before death/respawn, clears moving in Creating, and reads
input only in Ready. Active-shell counting, launch ownership, and respawn cleanup
were hardened to the player slot index. Five new direct suites brought
PlayerSystem to 19 suites and 48 checks; the 41-check production suite locked
phase routing, cross-zero behavior, and concrete frame orchestration. Direct,
production, and sanitizer regressions passed. At that increment, the gate had
17 integrated suites and
7,170 checks, 142 core/game suites and 1,097 checks, and 22 app suites and 115
checks: 181 suites and 8,382 checks across fifteen profiles. The 39 production
files contained 18,959 lines; `main.cpp` was 7,107 lines, `self_tests.inl` was
10,523 lines, `player_system.h/.cpp` were 203/217 lines, and the 1,220-line direct
test was excluded from the production denominator. The pure boundary remained
13 headers plus seven implementations. Coverage was
49.32%/62.78%/44.78%/51.89% overall and 23.38%/58.75%/29.40%/20.73% for
`main.cpp`; all 108 regions, six functions, 166 lines, and 94 branches in
`player_system.cpp` were covered, while `core/coordinates.h` reached
98.73%/100%/99.09%/97.50%.
The forty-first adds `advanceInactivePlayerDeath()` as an entity-free scalar
transaction. Invalid elapsed time is atomic; completed entry timers are
idempotent, positive timers debit through the exact/cross-zero boundary, and
malformed timers preserve legacy ordered-`std::max` behavior. Life debit
saturates non-positive counts at zero without `INT_MIN` overflow. `Game3D`
commits returned timer/lives before slot-owned shell cleanup, spawn reset,
respawn event append, and audio. Final-life shells remain live and delay Game
Over. Synthetic `id != slot` regressions distinguish event identity from shell
ownership in both respawn and final-life paths. Six new direct suites bring
PlayerSystem to 25 suites and 58 checks; the scripted-player-input suite has 43
checks and the observable-event suite has 75. At that increment, the gate had 17
integrated suites and 7,173 checks, 148 core/game suites and 1,101 checks, and 22
app suites and 115 checks: 187 suites and 8,389 checks across fifteen profiles.
The 39 production files contained 18,997 lines; `main.cpp` was 7,118 lines,
`self_tests.inl` was 10,595 lines, `player_system.h/.cpp` were 228/239 lines, and
the 1,410-line direct test was excluded from the production denominator. The pure
boundary remained 13 headers plus seven implementations. Coverage was
49.49%/62.92%/44.87%/52.13% overall and 23.77%/59.14%/29.58%/21.32% for
`main.cpp`; all 126 regions, seven functions, 183 lines, and 106 branches in
`player_system.cpp` were covered, while `core/coordinates.h` remained
98.73%/100%/99.09%/97.50%.
The forty-second adds `preparePlayerSpawnState()` as a detached nineteen-field
spawn/reset transaction. It writes position, orientation and directions,
activity/movement/Boat/ice flags, six lifecycle and motion clocks, shield, HP,
level, direct-kill streak, and popup time. Unknown progression, non-finite
position, or invalid creation/fire/shield duration rejects atomically.
`Preserve` retains level/streak/popup and positive HP but restores non-positive
HP from maximum HP; `Reset` clears progression and copies maximum HP. Maximum HP
remains deliberately unvalidated, preserving exact zero, negative, extreme, and
positive-over-maximum behavior. Maximum-finite and signed-zero inputs are direct
boundary cases. `Game3D` still resolves `id == 0` to the first
spawn and every other ID to the second, snapshots and writes back the entity,
and owns slot-shell cleanup plus event/audio side effects. Six new direct suites
bring PlayerSystem to 31 suites and 64 checks. The current gate has 17 integrated
suites and 7,181 checks, 154 core/game suites and 1,107 checks, and 22 app suites
and 115 checks: 193 suites and 8,403 checks across fifteen profiles. The 39
production files contain 19,136 lines; `main.cpp` is 7,158 lines,
`self_tests.inl` is 10,826 lines, `player_system.h/.cpp` are 277/289 lines, and
the 1,793-line direct test is excluded from the production denominator. The pure
boundary remains 13 headers plus seven implementations. Coverage is
49.70%/62.98%/45.17%/52.40% overall and 23.85%/59.14%/29.94%/21.34% for
`main.cpp`; all 154 regions, eight functions, 229 lines, and 134 branches in
`player_system.cpp` are covered, while `core/coordinates.h` remains
98.73%/100%/99.09%/97.50%. The observable-event suite reaches 76 checks and locks
the post-commit respawn cue order. ASan/UBSan passes all 17 integrated suites and
7,181 checks.
The forty-third increment hardens the real macOS distribution verifier without
changing production gameplay code or the runtime-suite ledger. Checksum and
code-sign failures now emit stable project-level diagnostics under a fixed C
locale. Canonical paths and explicit file-symlink rejection close lexical and
symlink path escapes. Inputs are copied without following final-component
symlinks into a private snapshot before validation, so every later check and the
self-test consume the same fixed bytes; newline-bearing paths are rejected
before command-substitution normalization, and physically resolved paths are
read directly from shell `PWD` and checked again. Input parents are re-resolved
after a no-follow recursive copy to detect concurrent directory replacement.
The negative gate grows from three to eleven isolated cases:
checksum-name/content mismatches, a checksum symlink, input and resolved
newline-bearing paths, both path-escape forms, an archive symlink, an unexpected
top-level payload, an archived symbolic link, and a runtime OGG mutation whose
ZIP checksum was recomputed. The latter reaches `codesign` with an unchanged
archive path manifest, proving detection by the existing ad-hoc resource seal;
it is an integrity check, not publisher authentication.
The forty-fourth increment adds a default-off release-screenshot contract for
exact-candidate evidence. A raylib-free parser validates the PNG path and
1-3,600 rendered-frame gate; five suites and 21 checks cover defaults, ordering,
boundaries, malformed values, and duplicates. The application fixes the capture
seed, locks the logical canvas, ignores borderless switching, normalizes Retina
framebuffers to 1280x720, refuses overwrite or a missing parent, saves after a
complete game frame, and exits through normal resource cleanup. The explicit
local GPU smoke remains outside headless CI and was visually checked for a real,
unobscured frame. That gate had 17 integrated suites and 7,181
checks, 154 core/game suites and 1,107 checks, and 27 app-layer suites and 136
checks: 198 suites and 8,424 checks across sixteen profiles. The 40 production
files contain 19,401 lines; `main.cpp` is 7,273 lines and the new parser header
is 150 lines. Coverage is 49.70%/63.30%/45.24%/52.27% overall and
23.42%/59.14%/29.42%/20.83% for `main.cpp`; the parser reaches
100%/100%/100%/95.45%. ASan/UBSan and the distribution verifier pass.
The forty-fifth increment closes the screenshot output race found during final
review. PNG encoding now occurs in memory; a raylib-free writer uses a private
same-directory file, flushes and closes it, then publishes with an atomic hard
link that cannot replace an existing regular file or symlink. A deterministic
pre-publish hook creates the formerly racy target inside the unit test and proves
its bytes survive; regular-file, symlink, missing-parent, invalid-input, and
temporary-cleanup paths are covered as well. The screenshot executable now has
eight suites and 39 checks, bringing the current aggregate to 201 suites and
8,442 checks across sixteen profiles. The 42 production files contain 19,662
lines; `main.cpp` is 7,302 lines. Coverage is
49.77%/63.83%/45.38%/52.16% overall and
23.32%/59.14%/29.29%/20.72% for `main.cpp`; the file writer reaches
59.60%/100%/68.70%/51.79% and the option parser remains
100%/100%/100%/95.45%. ASan/UBSan and the Retina GPU smoke pass.
The forty-sixth increment hardens the post-tag release process without changing
gameplay, maps, assets, or the runtime-suite ledger. A private-clone verifier
rechecks an immutable attested tag after documentation commits advance `HEAD`;
its contract covers four tagged-candidate rejection classes and two success
paths in addition to the existing eleven build and seven strict-verifier
rejections. A standard-library Python gate binds the candidate, release page,
QA report, fixed-size publication captures, manual matrices, published controls,
interactive evidence, clean-Mac download/quarantine facts, numeric 30-minute
criteria, known issues, the decision-specific inherited-audio review, chronology,
and both approvals. Twenty-three focused tests include a synthetic complete
release plus false-PASS attempts. The real Alpha 3 record remains explicitly
blocked until its external human evidence and signatures exist.
The final adversarial pass replaces free-form PASS support with strict,
candidate-bound interactive-session, gameplay-event, command, and raw
performance JSON records. Alpha performance limits are compiled into the
requirements profile, both release documents carry one canonical gate table,
and review/approval timestamps follow the completed tests. The v1 profile also
refuses a self-authored `CONFIRM` claim; that path needs an externally trusted
cryptographic rights-holder signature. This keeps the gate useful as an audit
boundary rather than a checklist that can approve self-asserted strings.
PR 3.5 remains partial because concrete bonus presentation, messages and map
commits, plus the concrete settlement event,
audio/display, navigation, map-load, and player commits still live in `Game3D`.
Keep remaining non-command producers in place until a narrow consumer boundary
is proven. Owned-shell cleanup, spawn-point mapping and entity adaptation,
events, audio, entities, map, rendering, cameras, and concrete movement/fire
orchestration and presentation adapters remain in `Game3D`.

The forty-seventh increment closes the last automatable long-session evidence
gap without changing gameplay, maps, or assets. A default-off recorder samples
real completed render frames in one-second monotonic windows and reads current
macOS physical footprint rather than the process high-water mark. Raw integer
timing, frame, RSS, stage, player, state, per-window gameplay/focus duration,
and candidate-event cleared-stage count are bound to the candidate SHA,
embedded source commit/tag, and a fresh nonce; output uses the same atomic
no-replace writer as release screenshots. A separate runner re-verifies the
tagged candidate, copies it through no-follow descriptors to a private snapshot,
rehashes that snapshot, extracts only that immutable input, hashes its exact
executable, captures stdout/stderr, and publishes a receipt only after an
ordered START/COMPLETE pair and clean exit. The v2 release verifier recomputes
weighted average FPS, one-percent-low FPS, MiB values, gameplay/focus coverage,
player mode, and cleared-stage count; it checks continuous 0.75–1.25 second
windows, caps physical-footprint growth, and rejects a forged receipt,
executable, identity, nonce, argv, marker, raw sample, static-screen session, or
ZIP replacement race. Legacy v1 reports remain valid only while blocked. The
C++ ledger is now 221 suites and 8,580 checks; 35 release-verifier tests and 12
runner tests cover the Python gate. ASan/UBSan, the 18-profile coverage merge,
static distribution verifier, and a real GPU telemetry smoke pass. Overall
production coverage is 51.52%/65.34%/46.70%/53.32%; the recorder is
88.33%/95.45%/81.60%/73.50%, and its option parser is
100%/100%/100%/96.72%.

The forty-eighth increment corrects the v2 clean-Mac acquisition contract
before any v2 candidate exists. Safari now performs the real HTTPS download so
the ZIP receives genuine quarantine metadata; a signed, candidate-bound browser
record captures its identity and interval before the five canonical checksum,
quarantine, signature, and assessment commands. The verifier binds the ZIP's
Safari agent while allowing Finder/Archive Utility to become the app's agent,
and rejects reserved, numeric, malformed, Unicode, single-label, or nonstandard-
port hosts. Legacy v1 keeps its six-command contract only for blocked historical
records. Forty-one release-verifier, fifteen v2-initializer, and fifteen
performance-runner tests now cover the Python release gate; the 13/9/5 candidate
rejection suites and both candidate success paths remain green.

The forty-ninth increment closes the automatable live-gameplay evidence gap
without changing gameplay, maps, or assets. An all-`NOT_RUN` v2 plan expands the
canonical profile into 67 ordered gameplay, base, pickup, and settlement
observations; compilation requires explicit PASS results, exact checks, bounded
UTC times, non-blocking notes, five distinct media groups, and independent
tester/reviewer identities. Valid PNGs and >=64 KiB recordings are bound to the
candidate through one canonical manifest, five event logs, and five interactive
sessions. Large recordings are streamed rather than retained in memory, inputs
are rechecked for TOCTOU changes, and dirfd/inode-bound no-replace output safely
rolls back partial writes. The verifier rejects undersized recordings, duplicate
publication pixels, contradictory PASS/audio prose, producer or manifest drift,
and observation/status/event disagreement; eight advanced-settings checks are
now explicit in the v2 published-control contract. A real compiler-to-verifier
test prevents schema drift. Forty-nine release-verifier, fifteen v2-initializer,
eighteen evidence-compiler, and fifteen performance-runner tests pass; the
13/9/5 candidate rejection suites and both success paths remain green. Separate
published-control and source-free Safari collectors, plus human QA and approval,
remain release work.

The fiftieth increment binds the published controls to the same candidate-bound
interactive evidence pipeline without changing gameplay, maps, or assets. The
v2 plan now contains 70 ordered observations across six evidence categories,
adding unique main-menu, one-player, and two-player control tokens. Those three
contexts require distinct >=64 KiB recordings, exact context-specific checks,
one shared manifest identity, and matching event/session references; the
compiler aggregates them into the 21-check `published_controls` result. The
verifier requires dynamic status tokens, manifest tokens, event tokens, and
coverage references to agree exactly, while legacy v1 remains isolated and
superseded. Clean-Mac v2 now uses only its Gatekeeper evidence identity so it
cannot conflict with the compiled main-menu session. Fifty-three
release-verifier, fifteen v2-initializer, twenty-two evidence-compiler, and
fifteen performance-runner tests pass; the 13/9/5 candidate rejection suites
and both success paths remain green. Human one-/two-player, audio, 30-minute,
and clean-Mac execution, plus approval and publication, remain release work.
