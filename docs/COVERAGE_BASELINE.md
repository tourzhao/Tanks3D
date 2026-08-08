# Coverage Baseline

This report records the pre-modularization self-test baseline begun on
2026-08-04 and refreshed on 2026-08-08 after the forty-fourth release-screenshot
increment.
It is a measurement, not a whole-project merge threshold. The larger self-test
is still textually included from `tests/self_tests.inl` in `src/main.cpp`; the
new core/game rule suites and app-layer suites are independent executables that
do not link raylib. The Makefile merges all sixteen profiles and explicitly passes
only production source/header paths to `llvm-cov`, excluding `tests/` and
third-party headers from the denominator. Compiled-module tests reuse canonical instrumented
objects; the CombatSystem test driver is instrumented for inline `GameEvent`
value semantics. The merge completes without duplicate-map warnings. Renderer
and embedded visual code lower the whole-source number.

## Instrumented Result

Apple clang 21 compiled the game, ten pure-rule executables, and five raylib-free
app-layer executables with
`-O0 -g -fprofile-instr-generate -fcoverage-mapping`. The game ran
`--self-test`, the pure-rule executables ran their table suites, and
`llvm-profdata`/`llvm-cov`
reported:

| Scope | Region | Function | Line | Branch |
| --- | ---: | ---: | ---: | ---: |
| All forty production source/header files | 49.70% | 63.30% | 45.24% | 52.27% |
| `src/main.cpp` | 23.42% | 59.14% | 29.42% | 20.83% |
| `src/app/command_side_effect_dispatch.cpp` | 100.00% | 100.00% | 99.55% | 100.00% |
| `src/app/shell_cancellation_presentation.cpp` | 100.00% | 100.00% | 100.00% | 100.00% |
| `src/app/shell_map_core_presentation.cpp` | 93.36% | 100.00% | 99.37% | 94.63% |
| `src/app/shell_tank_presentation.cpp` | 75.71% | 100.00% | 98.29% | 86.05% |
| `src/app/release_screenshot_options.h` | 100.00% | 100.00% | 100.00% | 95.45% |
| `src/core/coordinates.h` | 98.73% | 100.00% | 99.09% | 97.50% |
| `src/core/gameplay_rules.h` | 84.62% | 100.00% | 100.00% | 100.00% |
| `src/core/nation.h` | 100.00% | 100.00% | 100.00% | 100.00% |
| `src/audio/audio_output.h` | 100.00% | 100.00% | 100.00% | N/A |
| `src/game/bonus_rules.h` | 100.00% | 100.00% | 100.00% | 100.00% |
| `src/game/bonus_system.cpp` | 100.00% | 100.00% | 100.00% | 100.00% |
| `src/game/entities.h` | 100.00% | 100.00% | 100.00% | 100.00% |
| `src/game/combat_system.cpp` | 100.00% | 100.00% | 100.00% | 97.52% |
| `src/game/enemy_system.cpp` | 100.00% | 100.00% | 100.00% | 100.00% |
| `src/game/game_event.h` | 100.00% | 100.00% | 100.00% | 100.00% |
| `src/game/player_system.cpp` | 100.00% | 100.00% | 100.00% | 100.00% |
| `src/game/settlement_system.cpp` | 100.00% | 100.00% | 100.00% | 100.00% |
| `src/game/stage_generator.cpp` | 100.00% | 100.00% | 100.00% | 100.00% |
| `src/game/stage_map.h` | 100.00% | 100.00% | 100.00% | 100.00% |
| `src/game/stage_map.cpp` | 97.97% | 100.00% | 98.18% | 93.40% |
| `StageMap::impactShell` | 100.00% | 100.00% | 100.00% | 97.37% |

The overall percentages now include all forty production files and 19,401
production lines: `src/main.cpp` is 7,273 lines and the textually included
`tests/self_tests.inl` is 10,826 lines. Thirteen pure headers and seven pure
implementation sources are compiled independently. `player_system.h` is 277
lines, `player_system.cpp` is 289 lines, and its 1,793-line direct test remains
outside the production denominator. Moving tile generation from `StageMap`
into `StageGenerator` changes both files'
instrumentation denominators. All 35 direct generator goldens, the mutable-map
bridge and routes, dumped signatures, gameplay expectations, and the versioned
same-build deterministic replay gate pass.

The first PR 3.3 increment mechanically moved shell spawn, impact state,
cancellation eligibility/overlap, swept cancellation, and the `StageMap`
wall/core separation wrapper into `CombatSystem`. The second adds the pure-data
`CombatOutcome` and moves `StageMap::impactShell` query/mutation into
`resolveShellMapImpact()`. `Game3D` retains the live-shell guard, consumes
brick/base events, effects, and audio, and calls `beginShellImpact()` at the
original point. The third adds `CombatTarget`, explicit core-health fields, and
`resolveShellGovernmentCoreImpact()` for the binary core transition. It consumes
the preceding map result and refuses a core hit after any stopping map impact.
The fourth adds `evaluatePlayerShellEnemyTankImpact()`, which preserves terminal
map/core results and otherwise selects the first live, fully-created enemy under
a player shell with the existing strict AABB. It snapshots the enemy identity,
position, carrier flag, and one predicted armor reduction without mutating the
enemy or shell. `Game3D` still releases carrier bonuses before committing armor,
and at the end of that increment still owned enemy death, score/tally/streak,
events, presentation, and `updateShells()` ordering.

The fifth adds `commitPlayerShellEnemyTankImpact()`. It validates target index,
identity, type, carrier flag, eligibility, and the one-layer armor transition
before any side effect. For a valid carrier it invokes a caller-owned callback
before armor mutation, then commits armor, direct score/tally/streak, and fatal
enemy state. A repeated or stale outcome is rejected before the callback, so it
cannot duplicate a pickup or score. `Game3D` supplies the existing bonus RNG and
continues to own events, audio, effects, camera shake, and shell-impact timing.

The sixth adds `evaluateEnemyShellPlayerTankImpact()` and
`commitEnemyShellPlayerTankImpact()`. Selection remains read-only, enemy-owned,
strict-AABB, vector-first, and skips inactive, creating, or zero-HP players. The
outcome snapshots target identity, position, HP, Shield, and Boat state; commit
revalidates identity, eligibility, HP, Shield, and Boat before applying
Shield-to-Boat-to-HP priority and the fatal player transition. `Game3D` supplies
a narrow pre-commit callback for the
original generic impact effect, then retains event, explosion, camera, audio,
and final shell-impact order.

The seventh adds pure shell frame lifecycle helpers. They age every shell once,
derive a common step schedule from only live flying shells, snapshot all start
positions before each micro-step, move eligible shells, and stably remove
expired entries at frame end. Focused boundary cases preserve the exact 0.12
step, `life == dt`, impacting-shell occupancy, full 0.200-second same-frame
impact reset, and survivor order. Physical hit and cancellation presentation
remain in `Game3D`.

The eighth adds a presentation-free cancellation batch for one micro-step. It
keeps the historical nested-vector greedy order, commits each successful pair
immediately, lets rejected low-index candidates fall through, and snapshots
source attribution plus both contact positions. `Game3D` invokes it only after
all physical hits for that micro-step and consumes its ordered results before
advancing again, preserving terrain/core/tank priority and event/effect/audio
order.

The ninth adds `ShellPhysicalImpactResult` and a single-shell ordered resolver.
It rejects inert shells, composes map, core, enemy, and player resolution,
exposes the validated tank commit, and leaves the shell unchanged. `Game3D`
still consumes each result immediately, preserves the asymmetric branch-specific
event/effect/audio order, and calls `beginShellImpact()` last. Focused tests also
lock power-shell forest mutation followed by a tank hit, dead-core absorption,
carrier callback timing, and Shield/Boat/HP transitions.

The tenth moves the data-only event enums, payload, equality, and shell-source
helper into `game/game_event.h`. `eventsForPhysicalShellImpact()` projects only
the detached typed-result snapshot: brick records retain scan order, wall/core events
require real health loss, committed enemy/player damage uses snapshot IDs and
values, and unresolved, steel, boundary, dead-core, Shield, or Boat outcomes
emit nothing. `Game3D` consumes the returned values at the prior branch-specific
points, preserving the fatal-enemy audio-before-event asymmetry and all
effect/camera/final-shell timing. The CombatSystem coverage driver is
instrumented so every `GameEvent` field's equal and unequal paths contribute to
the production-header report without introducing a second test executable.

The eleventh adds `eventForShellCancellation()`. The total projector uses only
the detached cancellation snapshot, emits exactly one `ShellCancelled` event,
keeps the midpoint and both source domains, and leaves cause, impact kind,
power, and unrelated fields at their defaults. `Game3D` consumes each result
through a narrow typed helper in event, impact FX, then audio order, preserving
the historical per-pair interleaving without moving presentation into the pure
module. A production-path case advances two approaching rounds through a
nonzero frame and verifies the projected midpoint, IDs, sentinel cause, and
committed impact states.

The twelfth adds a default-off, app-local observer to the narrow cancellation
consumer. It records only after the actual event append, impact spawn, or audio
request and retains the exact event, contact point, effect parameters, and cue.
One production update locks the three-step payload; a two-pair update locks
`E-FX-A / E-FX-A`, so batching events ahead of presentation is now observable.
No trace is retained in normal play, and the headless contract is explicitly an
audio request rather than proof of hardware playback.

The thirteenth adds a separate default-off observer limited to StageMap and
GovernmentCore presentation. It receives the typed physical result and shell
only after real event-batch, semantic FX, camera-pass, audio-request, or final
impact actions. Eight production checks cover player brick, silent enemy wall,
enemy and player protected-wall behavior, player boundary, final wall breach,
same-frame live/dead core, and enemy core. Active-effect growth and breach shake
prove the observed FX/camera actions occurred without fixing particle counts.

The fourteenth adds paired default-off observers around tank presentation. The
post-result hook records only after actual event, armor FX, explosion, camera,
audio-request, or final-impact actions; the player-only hook records generic
armor FX at its pre-commit point. Six production cases lock enemy nonfatal and
fatal order plus Shield, Boat, player damage, and player destruction. A carrier
case preserves the already-appended bonus event, all pre-final snapshots keep
the shell flying at its contact with exact incoming velocity, and a seventh
check proves a later shell against the destroyed target produces no tank trace,
FX, or camera change.

The fifteenth replaces the six inline post-result tank branches with one owned,
fixed-capacity app-layer command and synchronous consumer. Variant payloads
carry events, exact FX values, radial/target camera semantics, cues, and final
contact without references or a retained queue. The generic player armor action
is typed and still consumed in the pre-commit callback. Existing production
cases now inspect every command payload, prove a 0.60 camera value is directly
replaced by Boat's 0.16, route a two-player hit to camera index 1, and preserve
a larger existing second-player value under enemy radial shake.

The sixteenth moves the command values, step mapping, pre-commit armor
projection, and six-branch factory into the raylib-free
`app/shell_tank_presentation` module. The unchanged `AudioCue` ordinal contract
now lives in `audio/audio_cue.h`; `Game3D` retains only explicit POD-to-raylib
conversion and concrete synchronous side effects. Three standalone suites add
18 checks for every action order and payload, non-tank/malformed silence, all
seven step alternatives, and the pre-commit calculation. The app implementation
reaches 98.29% line and 86.05% branch coverage through its canonical instrumented
production object.

The seventeenth moves the cancellation presentation mapping into
`app/shell_cancellation_presentation`. Two standalone suites/checks prove that
a complete projected cancellation event becomes the owned event, exact
non-heavy upward impact at elevation 0.67, and `BulletHit` request, while all
nine other event types are rejected. The canonical implementation object
reaches 100% in all four metrics. Production snapshots still prove actual
post-action event/effect transitions, whole-batch shell commitment, no-repeat
behavior, and per-pair `E-FX-A / E-FX-A` interleaving.

The eighteenth moves map/core presentation mapping into
`app/shell_map_core_presentation` and shares its renderer-independent payloads
through `app/presentation_values.h`. The owned fixed-capacity command carries
event batches, exact brick/surface/breach/explosion FX values, radial camera
shake, audio requests, and final contact. Its optional factory rejects invalid
owners and coordinates, contradictory tank commits, mismatched events,
non-deleting brick masks, partial power-shell brick damage, and impossible
government-wall HP transitions. Three standalone suites add 22 checks across
all eight action kinds, eight canonical branches, and grouped malformed-input
boundaries. Production snapshots observe only owned actions after real side
effects and retain the established live/dead-core same-frame ordering. The
implementation reaches 99.37% line and 94.63% branch coverage.

The nineteenth closes the two highest-priority orchestration gaps before an
ownership move. Production-path settlement tests now cover natural classified
counting, the five-second hold, stage wrap, survivor life gain, exhausted-player
recovery, the 99-life cap, record and non-record game over, both menu paths, and
the 5.2-second high-score display. `finishSettlement()` and
`updateSettlement()` now reach 100% line coverage; `finishSettlement()` also
reaches 100% branch coverage. Pickup tests exercise every valid effect,
including zero-target Grenade, Helmet extension, Clock max semantics, Tank cap,
Boat, injured Bandage, and maximum-level Star. They also exposed and fixed an
invalid-state bug: the `BonusType::Count` sentinel could previously be collected
as `UNKNOWN` for 300 points. Invalid pickup values are now rejected by the
raylib-free rule and silently discarded by orchestration. `applyBonus()` reaches
96.94% line and 94.74% branch coverage; its only unexecuted switch arm is the
now-unreachable sentinel.

The twentieth extracts the characterized report state into the raylib-free
`game/settlement_system`. `SettlementState` snapshots up to two participants and
owns score/K.O. cadence, confirmation, the five-second Idle timeout, and the
`AdvanceStage`/`GameOver` completion value. `SettlementUpdate` returns an exact
counted-step total; `Game3D` maps it to `ScoreCounted` and continues to own
`StageEnded`, audio, high-score/menu policy, stage loading, and player mutation.
Focused cases cover empty/score-only/tally-only/two-player reports, category
order, fractional and catch-up time, both completion kinds, snapshot retention,
invalid player counts, non-finite/non-positive time, and saturating `INT_MAX`
score arithmetic. Eight standalone suites add 55 checks and the implementation
reaches 100% in all four metrics.

The same increment makes automatic next-stage loading transactional. A candidate
`StageMap` is committed only after validation; failure restores the prior stage
and full player vector, preserves the error, produces one menu request, and
keeps the consumed-request session frozen through `awaitingMenu`. The production
integration case verifies rollback, no repeated `StageEnded`, one-shot
notification, and persistent freeze. These changes establish only the pure
settlement state boundary; PR 3.5 still retains its external consumers.

The twenty-first extracts `game/bonus_system` and moves `Pickup` from a raylib
`Vector3` to `core::XZ`. The compiled reducer validates weighting/pixel draws,
advances lifetime and strict-AABB collection in the established order, owns
saturating score/tally plus all player/enemy bonus mutations, and returns
ordered detached commands for Grenade/Shovel consumers. Twenty standalone
suites add 142 checks for every valid type, spawn and collection boundaries,
malformed time/state/index/type inputs, stacked eligibility semantics, exact
Grenade command order, corrupt armor filtering, and integer caps. The
implementation reaches 100% in all four metrics. Renderer bridges prove the
game-owned XZ state, while production regressions retain random draw order,
event order, map/FX/audio behavior, and the v1 digest field layout.

The twenty-second adds the raylib-free `CommandSideEffectSink` and compiled
dispatcher shared by cancellation, map/core, tank, and Bonus command leaves.
`Game3D` remains the synchronous concrete implementation; live shell impact and
Shovel steel are returned as domain commits and execute at the original point.
Fourteen RecordingSink suites add 73 checks for all action alternatives, event
batches, exact FX/camera/audio payloads and order, sink-free domain commits,
invalid audio, rejected tank cameras, and silently ignored Bonus cameras. The
canonical dispatcher object reaches 100% region/function/branch and 99.55% line
coverage. Existing production observers and session replay retain event/FX/
camera/audio/final-impact order, including player armor FX before commit.

The twenty-third through twenty-fifth increments move Enemy target/pursuit,
Armor-fire, spawn-slot selection, frame lifecycle, local escape, and fire
planning into the compiled `EnemySystem`. The twenty-sixth adds strict spawn
type/carrier/armor probability rules and full detached initialization while
leaving random draws and transaction commits in `Game3D`. The twenty-seventh
adds active movement-delay/ice resolution, conditional lane snapping, padded
collision probing, movement/blocked commits, and detached dust intent. It leaves
the on-ice classification, live occupancy implementation, concrete FX/cooldown,
and fire draw in `Game3D`. The twenty-eighth adds the active steering timer and
decision gates, conditional pursuit/wander random-callback pulls, local escape
selection, and atomic escape commits. Thirty-two direct suites add 105 checks;
spawn transactions preserve conditional RNG and all committed fields, movement
checks preserve callback order, ice carry, blocked state, and FX-before-reload
state, and steering checks preserve invalid-input atomicity, strict thresholds,
zero-draw paths, callback/state order, and `R-R-R`/`R-R-I` transcripts. The
implementation's 220 regions, 18 functions, 361 lines, and 184 branches all
remain at 100% coverage.

The twenty-ninth moves the complete spawn-attempt state machine into
`EnemySystem`. `advanceEnemySpawnTransaction()` owns atomic validation,
queue and identifier exhaustion before debit, capacity and cooldown gates after
debit, rotated live queries that stop at the first open slot, blocked retry, ID
reservation, conditional regular `R-I-R-R` and Armor `R-R-R` callback pulls,
detached construction, and synchronous insertion before final
count/rotation/timer commits. `Game3D` supplies tuned configuration, live
availability and seeded-random callbacks, and the concrete enemy-container
insertion callback. Five direct suites add seven checks, bringing the module to
37 suites and 112 checks. Its expanded 283 regions, 23 functions, 441 lines, and
238 branches all remain at 100% coverage; the production path retains exact
ordering and reaches 16 suites and 7,127 checks.
A fixed digest prefix also locks serialization of remaining count, rotation
index, next ID, and timer in that order.

The thirtieth completes PR 3.4 by moving the full post-movement firing
transaction into `EnemySystem`. It owns the due gate, strict fire-roll
validation, conditional `R`, `R-Q`, and `R-Q-C` callback transcripts,
type/Armor reloads with normalized advanced setting, owned-shell suppression,
complete detached `Shell` construction, synchronous launch callback, and the
cooldown commit afterward. `Game3D` supplies live occupancy and seeded-random
adapters and retains the concrete shell insertion, event append, and muzzle
effect. Six direct suites add eight checks, bringing the module to 43 suites and
120 checks; its 314 regions, 24 functions, 490 lines, and 262 branches all
remain at 100% coverage. Production cases lock +30% fire tuning, Armor
movement-direction aim with an ice-carried drive-direction launch, impacting
zero-life owned-shell occupancy, unrelated distractors, exact
shell-event-muzzle order, and enemy-fire audio silence. The integrated run
reaches 16 suites and 7,129 checks.

The thirty-first moves the complete carrier-release transaction into
`BonusSystem`. It snapshots Bandage eligibility once; owns the exact
`T-X-Z-Q-C` or retrying `T-X-Z-Q-(X-Z-Q)*-C` callback transcript; validates a
complete type/position attempt before any base query; keeps the type across
rejected positions; and synchronously commits a detached intent. `Game3D`
retains the seeded-random/STL adapter, authoritative government-base predicate,
and concrete pickup/event/audio consumer. Its default-off observer proves
`pickup-event-audio` order while armor and score still hold their pre-hit values.
A malformed draw produces no release side effect but still lets the validated
carrier hit commit. Five direct suites add eleven checks, bringing BonusSystem
to 25 suites and 153 checks. Its 139 regions, 10 functions, 220 lines, and 108
branches all reach 100% coverage; the integrated run reaches 16 suites and
7,130 checks.

The thirty-second moves the per-pickup `U-B-A-F` transaction into
`BonusSystem`: update clocks/eligibility, begin with a detached intent, apply
rule state, and synchronously finish. Missing callbacks are atomic;
retain/discard invokes neither callback; defensive rejection returns a distinct
outcome so the concrete consumer can remove its staged event and retain the
pickup. `Game3D` preserves `E0-A-C-M-O-Ep-D` and erases only after return.
Four direct suites add five checks, bringing BonusSystem to 29 suites and 158
checks. Its 154 regions, 11 functions, 250 lines, and 118 branches all remain at
100% coverage. Integrated Grenade phase, Tank audio, stacked two-player
Bandage, and same-frame double-Grenade cases bring the game run to 16 suites and
7,134 checks.

The thirty-third moves pure settlement completion policy into
`SettlementSystem`. `planSettlementTransition()` owns no-op/invalid handling,
strict record selection, stage advance/wrap, survivor life gain/cap, and
eliminated-player recovery on a detached player snapshot. Three direct suites
add ten checks, bringing the module to eleven suites and 65 checks; its 143
regions, 20 functions, 225 lines, and 104 branches remain at 100% coverage.
`Game3D` prepares and validates a candidate stage before committing live stage,
player progression, or world reset. Six integrated checks compare complete
player state, snapshot the exact `StageStart` request-time state, reject a loader
that reports success with the wrong stage, and cover different-configuration
failed start, atomic restart, plus manual stage-change load failures. `start()`,
`loadStage()`, `prepareStageMap()`,
`commitPreparedStage()`, and `restart()` now have 100% line coverage;
`finishSettlement()` reaches 98.53% line and 78.95% branch coverage. The
integrated run reaches 16 suites and 7,144 checks.

The thirty-fourth moves stable outer pickup-vector advancement into
`BonusSystem`. `advanceBonusPickups()` preserves the per-element `U-B-A-F`
transaction, current-index erasure, shifted-successor processing, silent discard,
and rejected-pickup retention. Missing begin/finish callbacks reject the whole
batch before mutation, while an optional collected callback observes detached
values only after erasure. `Game3D` retains its concrete
`E0-A-C-M-O-Ep-D` callbacks. A nonzero-time production hit locks carrier
release followed by same-frame aging and collection. Four direct suites add five
checks, bringing BonusSystem to 33 suites and 163 checks; its 171 regions, 12
functions, 279 lines, and 134 branches remain at 100% coverage. The integrated
run reaches 16 suites and 7,145 checks.

The thirty-fifth moves pure report-entry planning into `SettlementSystem`.
`planSettlementBegin()` validates stage bounds, exact one- or two-player scope,
vector size, and cleared/base consistency before it returns a detached
score/tally snapshot classified as `Cleared`, `BaseDestroyed`, or
`PlayersDefeated`. `Game3D` retains event and audio work and consumes the plan in
`PlanReady` -> `ReportCommitted` -> optional `StageEndEventAppended` ->
`AudioStopBoundaryPassed` order. Production paths cover the event-free showcase,
all three reasons, append-at-end behavior with an existing event, post-commit
count-audio state, and silent confirmation. Three direct suites add 24 checks,
bringing SettlementSystem to 14 suites and 89 checks; its expanded 170 regions,
21 functions, 251 lines, and 126 branches all reach 100% coverage. Four
integrated checks bring the game run to 16 suites and 7,149 checks.

The thirty-sixth adds the raylib-free `audio/audio_output.h` contract with only
`play`, `updateEngine`, and `stopAll`. `Game3D` now stores a nullable,
non-owning interface pointer; normal `main` still stack-owns the concrete
`AudioBank`, controls device and load/unload lifetime, and retains its priority,
overlap, and voice policy. A recording implementation verifies post-commit
`StageStart`, creating-player mute and ready moving-engine state, pause
`stopAll` before `Pause` with silent resume, and settlement stop only after the
`StageEnded` append. Three new boundary checks create a seventeenth integrated
suite; a strengthened existing settlement sentinel locks the last of those
orders, bringing the game run to 7,152 checks. They do not open an audio device,
verify hardware playback, or exercise the concrete bank's internal voice
arbitration. The interface contributes one covered region, function, and line;
it has no applicable branch.

The thirty-seventh adds `game/player_system.h/.cpp`, moves the three input-frame
types out of `main.cpp`, and limits `planPlayerControl()` to final drive,
propulsion, and held-fire intent. Three direct suites contain 13 checks; two
independent literal tables exhaust five valid current directions x 16 held
masks x 16 pressed masks x two fire states, or 2,560 combinations (not checks).
Two additional checks preserve legacy invalid `5`/`255` behavior rather than
imposing fail-closed validation. Three integrated checks cover the
pressed-only consumer and moved-position fire fixture/order. `Game3D` retains
the death/creation gates, cooldown debit, unconditional drive commit,
propelling-only yaw, ice/map/snap/collision/dust sequence, and held/due/cap
fire last. At that increment,
the implementation's 35 regions, three functions, 42 lines, and 34 branches
all reached 100%.

The thirty-eighth adds the scalar `advancePlayerFireTransaction()` and leaves
the header independent of entities, CombatSystem, StageMap, audio, and
presentation. Five new direct suites bring PlayerSystem to eight suites and 22
checks. They cover atomic invalid configuration, idle/cooling gates including
non-finite cooldown parity, reset-before-cap behavior at and above every level's
limit, six requested-level boundaries, `None`/`5`/`255` zero-vector launches,
owning-index payloads, and explicit reload/spawn scalars. The implementation
expands to 60 regions, four functions, 74 lines, and 54 branches, all at 100%.
The 32-check scripted-player-input suite exercises the real consumer: successful
P2 ownership, cooldown visibility inside the callback, the five-step
shell/event/muzzle/camera/audio adapter, full-slot presentation silence with an
impacting shell, post-movement spawn position, and drive-direction fire during
ice-carried travel.
At that increment the gate had 17 integrated suites and 7,161 checks, 131
core/game suites and 1,071 checks, and 22 app suites and 115 checks: 170 suites
and 8,347 checks across fifteen profiles. The 39 production files contained
18,716 lines; `main.cpp` was 7,078 lines, `self_tests.inl` was 10,237 lines,
`player_system.h/.cpp` were 101/105 lines, and the direct test was 493 lines.
Coverage was 48.95%/62.61%/44.30%/51.52% overall and
23.12%/58.59%/29.10%/20.72% for `main.cpp`.

The thirty-ninth adds `advanceActivePlayerMovement()` without adding an entity,
map, audio, or presentation dependency. At that increment, six new direct
suites brought PlayerSystem to 14 suites and 40 checks. The scalar transaction
rejects invalid elapsed time, speed, a non-finite movement product, and an empty
callback before
state mutation or availability queries. Direct transcripts preserve cardinal
and unknown-direction zero-vector parity, exact optional lane-snap then forward
query order, rejected and retained snaps, ice continuation/expiry, blocked
momentum reset, zero-distance behavior, and detached dust position/velocity.
The 38-check scripted-player-input suite covered the live `Game3D` adapter:
state writeback then dust/clock then fire, blocked movement with same-frame fire,
Boat-dependent water traversal, and P2 observing P1's same-frame committed
position. At that increment, totals were 17 integrated suites and 7,167 checks,
137 core/game suites and 1,089 checks, and 22 app suites and 115 checks: 176
suites and 8,371 checks across fifteen profiles. The 39 production files
contained 18,865 lines; `main.cpp` was 7,086 lines, `self_tests.inl` was 10,424
lines, `player_system.h/.cpp` were 166/181 lines, and the direct test was 1,046
lines. The pure boundary remained 13 headers plus seven implementations. The
implementation had 96 regions, five functions, 137 lines, and 86 branches, all
at 100%. Coverage was 49.19%/62.78%/44.63%/51.80% overall and
23.00%/58.91%/29.28%/20.43% for `main.cpp`; `core/coordinates.h` reached
98.73%/100%/99.09%/97.50%.

The fortieth adds entity-free `beginPlayerFrame()` ownership of the creation,
fire, dust, shield, and streak clock policy. Invalid elapsed time is atomic;
valid phases are chosen from the entry snapshot and preserve shared-clock,
creation-only, ready-only, creation-cross-zero, and ordered malformed-`std::max`
behavior. `Game3D` commits all five returned clocks before death/respawn, clears
moving in Creating, and reads input only in Ready. Active-shell counting, launch
payloads, and respawn cleanup were hardened to slot-index ownership. Five new
direct suites brought PlayerSystem to 19 suites and 48 checks; the production
script had 41 checks and covered the concrete frame orchestration.
Direct, production, and sanitizer gates passed. At that increment, totals were
17 integrated suites
and 7,170 checks, 142 core/game suites and 1,097 checks, and 22 app suites and
115 checks: 181 suites and 8,382 checks across fifteen profiles. The 39
production files contained 18,959 lines; `main.cpp` was 7,107 lines,
`self_tests.inl` was 10,523 lines, `player_system.h/.cpp` were 203/217 lines, and
the direct test was 1,220 lines. The pure boundary remained 13 headers plus seven
implementations. The implementation had 108 regions, six functions, 166 lines,
and 94 branches, all at 100%. Coverage was
49.32%/62.78%/44.78%/51.89% overall and
23.38%/58.75%/29.40%/20.73% for `main.cpp`; `core/coordinates.h` reached
98.73%/100%/99.09%/97.50%.

The forty-first adds `advanceInactivePlayerDeath()` with only scalar
`deathTimer`/lives state. Invalid elapsed time is atomic; entry, exact and
cross-zero, malformed timer, and non-positive timer behavior remain explicit.
Life debit saturates at zero without `INT_MIN` overflow. `Game3D` commits the
returned timer/lives before slot-owned shell cleanup, spawn reset, event append,
and audio. Final-life shells remain live and delay Game Over. Synthetic
`id != slot` cases preserve entity identity in the event while proving shell
cleanup and defeat checks use the vector slot. Six new direct suites bring
PlayerSystem to 25 suites and 58 checks; scripted-player-input has 43 checks and
observable-event has 75. At that increment, totals were 17 integrated suites and
7,173 checks, 148 core/game suites and 1,101 checks, and 22 app suites and 115
checks: 187 suites and 8,389 checks across fifteen profiles. The 39 production
files contained 18,997 lines; `main.cpp` was 7,118 lines, `self_tests.inl` was
10,595 lines, `player_system.h/.cpp` were 228/239 lines, and the direct test was
1,410 lines. The pure boundary remained 13 headers plus seven implementations.
The implementation had 126 regions, seven functions, 183 lines, and 106 branches,
all at 100%.
Coverage at that increment was 49.49%/62.92%/44.87%/52.13% overall and
23.77%/59.14%/29.58%/21.32% for `main.cpp`; `core/coordinates.h` remained
98.73%/100%/99.09%/97.50%.

The forty-second adds `preparePlayerSpawnState()` as an entity-free transaction
over nineteen spawn/reset fields. It validates progression, detached coordinates,
and creation/fire/shield durations before an atomic commit. Complete independent
write masks cover both modes: `Preserve` retains progression and positive HP,
while `Reset` clears progression and copies maximum HP. Maximum HP remains
unvalidated to preserve zero, negative, extreme, and positive-over-maximum legacy
behavior. Signed-zero and maximum-finite inputs are explicit boundaries. The
`Game3D` adapter retains ID-to-spawn mapping, entity snapshot and writeback,
slot-shell cleanup, events, and audio. Six new direct suites bring
PlayerSystem to 31 suites and 64 checks. Current totals are 17 integrated suites
and 7,181 checks, 154 core/game suites and 1,107 checks, and 22 app suites and 115
checks: 193 suites and 8,403 checks across fifteen profiles. The 39 production
files contain 19,136 lines; `main.cpp` is 7,158 lines, `self_tests.inl` is 10,826
lines, `player_system.h/.cpp` are 277/289 lines, and the direct test is 1,793
lines. The pure boundary remains 13 headers plus seven implementations. The
implementation has 154 regions, eight functions, 229 lines, and 134 branches,
all at 100%. Current coverage is 49.70%/62.98%/45.17%/52.40% overall and
23.85%/59.14%/29.94%/21.34% for `main.cpp`; `core/coordinates.h` remains
98.73%/100%/99.09%/97.50%. The observable-event suite reaches 76 checks and locks
post-commit respawn cue ordering. ASan/UBSan passes all 17 integrated suites and
7,181 checks.

The outcome retains the target, full `ShellImpactDetails`, source position,
incoming velocity, owner, owner index, power flag, explicit core-health
transition, plus independent enemy-target and player-target snapshots.
`mapStopsShell()` and `stopsShell()` are distinct from mutation:
`ImpactKind::None` can mean either a power shell cleared forest and continues
flying or a core hit stopped the shell.

Every self-test `Game3D` uses an explicit seed. Two consecutive reports produce
identical totals; normal gameplay still seeds once from `std::random_device`.
The seeded transcript, typed random source, scripted input, and 480-frame replay
drive real session paths. The pre-PR3.3 function snapshot recorded
`Game3D::spawnEnemyIfNeeded` at 92.45% line and 91.67% branch coverage and
`Game3D::updateEnemies` at 91.21% and 63.00%. `Game3D::releaseBonus`,
`firePlayerShell`, and `fireEnemyShell` were at 100% for both line and branch
coverage; `updatePlayers` was at 93.18% line and 87.50% branch coverage. The
injected key-state mapping covered every binding; only the five-line raylib
query wrapper remained outside the headless path.

The then-73-check observable-event suite exercises combat and lifecycle results
through production methods and public updates. Immediately before the first
combat-helper extraction, its function snapshot was:

| Function | Line | Branch |
| --- | ---: | ---: |
| `Game3D::sessionDigest` | 95.00% | 96.15% |
| `Game3D::beginSettlement` | 97.62% | 86.36% |
| `Game3D::updateShells` | 100.00% | 88.89% |
| `Game3D::resolveFlyingShellImpact` | 92.96% | 80.56% |
| `Game3D::updateBonuses` | 100.00% | 81.25% |
| `Game3D::damagePlayer` | 88.89% | 66.67% |

That table is a historical pre-extraction snapshot. `Game3D::damagePlayer` was
removed by the sixth increment; its state transition now lives behind the
validated `CombatSystem` player commit.

The integrated game run executes seventeen suites and 7,181 checks. The ten
raylib-free core/game executables add 154 suites and 1,107 checks; the five
raylib-free app-layer executables add twenty-seven suites and 136 checks, for
198 suites and 8,424 checks across the aggregate gate. The release-screenshot
parser contributes five suites and 21 checks. The standalone
`StageGenerator` executable contributes two suites and 43 checks; the
`StageMap` executable retains four suites and 136 checks. The new
`CombatSystem` executable contributes twenty suites and 132 checks, while the
`SettlementSystem` executable contributes fourteen suites and 89 checks. The
`BonusSystem` executable contributes thirty-three suites and 163 checks. The
`EnemySystem` executable contributes forty-three suites and 120 checks. Test
source is not in the denominator. The `PlayerSystem` executable contributes
thirty-one suites and 64 checks while exercising 2,560 control combinations
through two literal tables plus the frame-entry, inactive-death, scalar
movement, firing, and spawn/reset transaction matrices.

The nineteenth pre-extraction characterization snapshot reported:

| Function | Line | Branch |
| --- | ---: | ---: |
| `Game3D::finishSettlement` | 100.00% | 100.00% |
| `Game3D::applyBonus` | 96.94% | 94.74% |

Input, randomness, event observation, deterministic digest seams, all Phase 2
pure-rule extractions, and the PR 3.1/3.2 `StageMap`/`StageGenerator` split are
complete. PR 3.3 is in progress: its shell-helper/cancellation increment is
complete, and its map-impact and government-core outcome increments are now
complete. Its read-only player-shell/enemy-target evaluator, validated enemy
armor/death/score commit, and enemy-shell/player selection plus Shield/Boat/HP
commit, frame lifetime/micro-step helpers, greedy opposing-shell cancellation,
ordered single-shell physical resolution, the data-only event contract, and
physical-result event projection are complete as well. The tank, map/core, and
cancellation paths now use owned app-layer commands and synchronous consumers.
Settlement entry, completion, and every valid pickup effect are now
characterized. Pure report-entry validation/reason planning, the settlement
snapshot/count/confirm/timeout state, and completion values now live in `game/`.
Pickup XZ state, the carrier-release transaction,
lifetime/collection, score/tally, player/enemy effects, and ordered commands now
live in `BonusSystem`; concrete map/audio/FX/camera consumers plus seeded-random
and authoritative base-query adapters remain in `Game3D`. The four proven
command families now cross one synchronous app-layer side-effect sink, while
live shell/Shovel commits and the concrete raylib/audio implementation remain in
`Game3D`. The formerly uncovered EnemySystem production paths now lock inactive
and tied target selection, horizontal steering and strict RNG thresholds, all
Armor firing directions, owned-shell suppression, and fully blocked spawn
retry. The compiled module owns those decisions with 100% coverage, together
with the Destroyed/Creating/Frozen/Active frame-entry clock gate, local escape
selection, post-movement type-specific fire planning, spawn probability/armor
rules, detached initialization, active movement, the active steering/escape
transaction, the complete spawn-attempt transaction, and the complete firing
transaction. Integrated production
checks retain the original clocks, exact per-enemy RNG transcript, both escape
outcomes, complete spawn commits, movement/dust/reload order, firing outputs,
blocked retries, nonzero fire tuning, aim/launch direction separation, and
occupied/distractor shell behavior. Tuned configuration, live occupancy and
seeded-random callbacks, synchronous container insertion, and concrete
event/muzzle/audio-observation adapters remain in `Game3D`. PR 3.4 is complete;
PR 3.5 is partial. Stable bonus-vector advancement now lives in `BonusSystem`,
pure settlement-entry planning plus count-audio ordering are characterized, and
`Game3D` delegates its runtime audio calls through the injected raylib-free
output boundary. Player input values, the five-clock
`Inactive`/`Creating`/`Ready` frame transaction, inactive scalar death
transaction, control planning, active scalar cardinal/ice movement with detached
dust, the scalar player-fire transaction, and the 19-field spawn/reset
transaction now live in `PlayerSystem`. `Game3D` retains entity snapshot/commit
and phase routing, ID-to-position mapping, slot-owned shell cleanup, respawn
event/audio, map ice and live occupancy/Boat
adapters, scalar movement writeback, concrete dust presentation and cooldown,
Shell construction/insertion, and the fire presentation adapter. Owned-shell
cleanup, spawn-point adaptation, events, audio, entities, map, rendering,
cameras, and concrete movement/fire orchestration and presentation adapters
remain in `Game3D`.

## Reproduction Outline

```sh
make coverage
```

The target builds the instrumented game, pure-rule executables, and app-layer
executables, reuses the canonical compiled-module objects, and instruments the
CombatSystem driver for inline event value branches. It runs all sixteen profiles
and reports only the
production source/header list. Raw and merged profiles remain under
`build/coverage/` for local inspection.
