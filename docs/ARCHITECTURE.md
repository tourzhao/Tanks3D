# Architecture and Refactoring Plan

This document summarizes the intended architecture. The reviewable pull-request
sequence, test gates, and rollback rules are in
[`REFACTORING_PLAN.md`](REFACTORING_PLAN.md).

## Decision

Refactor this project incrementally. The current build is warning-clean, passes
the headless rule suite, and passes AddressSanitizer and UndefinedBehaviorSanitizer.
A large rewrite before release would put established collision, progression, and
rendering behavior at unnecessary risk. New features should, however, stop
expanding the existing `Game3D` class.

## Current Shape and Risks

The forty-nine production source/header files contain 21,236 lines.
`src/main.cpp` remains one 7,679-line production translation unit; 11,042 lines
of transitional self-tests still live in `tests/self_tests.inl` and are
textually compiled into that unit. Thirteen pure headers and seven pure
implementation sources under
`src/core/` and `src/game/` are independently compiled and tested without
raylib. `player_system.h/.cpp` are 277/289 lines; their 1,793-line direct test is
outside the production denominator. The eight `src/app/` implementation modules,
their shared POD values and release-screenshot option parser,
synchronous side-effect boundary, and the `src/audio/` cue/output headers are
also compiled without raylib and guarded against reverse dependencies.
`Game3D` still combines inactive player respawn side effects plus phase,
movement, and firing adapters, outer
simulation sequencing, live enemy
fire/spawn configuration, random and occupancy adapters, projectile
presentation, carrier-pickup random/base-query adapters and concrete consumers,
direct-fire scoring, settlement side effects, audio delegation, concrete
effect/camera sinks, camera state, and rendering.
Renderer-local `ViewTargets` remains in `main.cpp`, but its injected operations
now isolate allocation policy from ownership. It validates the raylib 6.0
depth-metadata contract, attempts a complete RGBA16F target before RGBA8, and
commits a resize only after every replacement is valid and configured. Invalid
or partial candidates are released through their FBO ownership roots. Window
creation and render-target failure both fail closed before an invalid target can
reach a draw call.
`StageGenerator` now produces deterministic tile grids independently of the
mutable `StageMap`, which retains route validation, collision, brick masks,
base-wall state, and destruction. The first `CombatSystem` increment owns shell
spawn geometry, impact-state transitions, cancellation eligibility/overlap,
swept cancellation, and its `StageMap`-backed wall/core separation wrapper. The
second adds the pure-data `CombatOutcome` and mechanically moves the
`StageMap::impactShell` query/mutation into `resolveShellMapImpact()`.
The third adds `CombatTarget` and
`resolveShellGovernmentCoreImpact()`, which owns the binary core transition
while leaving the shell untouched and refuses core damage after a stopping map
outcome. The fourth adds a read-only player-shell/enemy-tank evaluator. It
selects the first eligible enemy in vector order, snapshots one armor reduction,
and preserves a prior stopping map/core result without mutating the enemy or
shell. The fifth adds a validated commit boundary: it rejects stale/repeated
snapshots, invokes a narrow carrier callback before armor mutation, then owns
direct-hit armor, score, classified tally, streak, and fatal enemy state.
The sixth adds read-only enemy-shell/player-tank selection and a validated
commit. It skips inactive, creating, or zero-HP players, preserves strict-AABB
and vector-first selection, snapshots HP/Shield/Boat state, then commits the
existing Shield-to-Boat-to-HP priority and fatal player transition. A narrow
pre-commit callback retains the original generic impact effect before state
mutation.
The seventh moves per-frame shell aging, fastest-shell fixed-step scheduling,
micro-step position snapshots/movement, and stable expired-shell cleanup into
`CombatSystem` while leaving physical impacts and presentation ordering in
`Game3D`.
The eighth moves surviving opposing-shell enumeration and greedy cancellation
commits into `CombatSystem`. Each micro-step still resolves physical impacts
first; cancellation results snapshot source IDs and both contact positions in
vector order before `Game3D` emits events, effects, and audio.
The ninth adds `ShellPhysicalImpactResult` and composes the existing map, core,
enemy, and player operations into one ordered transaction per live shell. It
commits rule state while leaving the shell untouched; `Game3D` immediately
consumes the typed result and only then begins the impact animation.
The tenth moves the four event enums and detached `GameEvent` value payload into
`game/game_event.h`. `eventsForPhysicalShellImpact()` now projects the typed
transaction into ordered brick, base, and tank events without presentation or
live-state reads. `Game3D` appends those values at each branch's historical
point, including the deliberately later fatal-enemy event.
The eleventh adds `eventForShellCancellation()`, a one-outcome/one-event pure
projection that snapshots the midpoint and both owner IDs without inventing a
single-shell cause. A narrow `Game3D` consumer keeps each pair's historical
event, midpoint impact effect, then `BulletHit` audio sequence; it does not
batch events ahead of presentation. A nonzero-time production update now locks
the swept crossing, midpoint payload, and both owner domains end to end.
The twelfth adds an app-local typed record and default-off observer around that
narrow consumer. Records are emitted only after the real event append, impact
spawn, and `BulletHit` request. Headless production updates now lock the exact
single-pair payload and `E-FX-A / E-FX-A` multi-pair interleaving without
retaining a trace during normal play or moving presentation into `game/`.
The thirteenth adds a separate StageMap/GovernmentCore-only step observer. It
reuses the typed physical result and live shell instead of introducing a second
sentinel-heavy record, and reports only after each real operation. Eight
production cases now lock player brick, silent enemy wall, silent protected
wall, player steel and boundary cues, final wall breach/camera handling,
same-frame live/dead core, and enemy core. `A` denotes an audio request and `I`
the committed shell impact state; tests require real semantic FX growth without
fixing particle counts. Normal play retains no trace and tank-hit timing stays
separate.
The fourteenth adds paired, default-off tank observers. The post-result hook
records enemy/player event, armor FX, explosion, camera, audio-request, and
final-impact steps; the player-only hook records the generic armor FX at its
original pre-commit point. Six branch-order checks preserve enemy nonfatal and
fatal asymmetry plus Shield, Boat, player damage, and player destruction. A
seventh check proves a later shell against an already-destroyed enemy emits no
tank presentation. Every pre-final snapshot retains a flying shell, while only
the last commits the 0.200-second impact state. No normal-play trace is kept.
The fifteenth replaces all six post-result tank branches with one app-local,
owned `ShellTankPresentationCommand`. A fixed-capacity sequence of typed variant
actions carries event batches, exact FX parameters, radial or target camera
shake, requested cues, and final contact by value. `Game3D` builds and consumes
each plan synchronously for one shell; it retains no queue. The generic player
armor action is also typed but remains immediately consumed inside the
validated pre-commit callback. Tests now inspect payloads, prove target-camera
shake is assignment rather than `max`, lock two-player target index 1, and prove
radial shake preserves a larger existing value for the other player.
The sixteenth moves that proven value contract into
`app/shell_tank_presentation.{h,cpp}`. Renderer-independent `Float3` and `Rgba8`
payloads keep the owned command, pre-commit armor projection, step mapping, and
six-branch factory independently compilable and testable without raylib.
`audio/audio_cue.h` now owns the unchanged 22-value cue ordinal contract.
`Game3D` injects the fatal-player color, converts POD values at the BattleFx
edge, and retains the synchronous concrete consumer. Three standalone suites
lock all six command sequences and payloads, all seven step alternatives,
non-tank and malformed-result silence, and the pre-commit armor projection.
The seventeenth replaces the cancellation-only sentinel record with
`app/shell_cancellation_presentation.{h,cpp}`. Its fixed owned command carries
the already-projected `ShellCancelled` event, midpoint impact parameters, and
`BulletHit` request without raylib or live shell references; all other event
types are rejected. `Game3D` consumes the three fields synchronously as event,
FX, then audio for each pair. It deliberately performs no shell commit because
the cancellation resolver has already committed both shells. Two independent
checks lock the valid mapping and all nine rejected event types, while
post-action production snapshots prove `E-FX-A / E-FX-A` interleaving and real
event/effect-count transitions.
The eighteenth replaces the StageMap/GovernmentCore observer's live physical
result and shell references with an owned, fixed-capacity
`ShellMapCorePresentationCommand`. Its variant actions carry event batches,
brick/surface/breach FX values, radial camera shake, requested audio, and final
contact by value. The optional factory rejects mismatched events, invalid
source/coordinate data, impossible brick-mask or government-wall HP changes,
and contradictory tank commits. `Game3D` consumes each action synchronously,
observes it only after the real side effect, and fails closed to an impacting
shell if a canonical command cannot be built. Three standalone suites add 22
checks across every branch, action alternative, and malformed boundary; the
existing production cases now also lock exact payloads and live/dead-core
same-frame shell state. `Float3` and `Rgba8` moved mechanically into the shared
raylib-free `app/presentation_values.h` header.
The nineteenth holds ownership steady and closes the next characterization
gaps. Settlement now reaches its real completion through natural counting,
confirmation, and timeout paths for successful stages, non-record defeats, and
two-player record defeats. Tests lock life recovery/caps, stage wrap, menu
request consumption, and record-screen timing. Every valid pickup effect now
runs through production collection. The exercise exposed a latent validation
bug where the `BonusType::Count` sentinel behaved as an `UNKNOWN` 300-point
pickup; the shared rule rejects sentinel/out-of-range values and orchestration
drops them without an event or reward. This is intentionally a hardening and
characterization increment, not a premature `BonusSystem`/`SettlementSystem`
move: both still cross map, stage-load, menu, FX, camera, and audio boundaries.
The twentieth moves only the proven battle-report state machine into
`game/settlement_system.{h,cpp}`. A fixed `SettlementStart` snapshot feeds the
raylib-free `SettlementState`, which owns classified K.O. and score counting,
confirmation, the five-second Idle timeout, and the `AdvanceStage`/`GameOver`
completion decision. Each update returns a counted-step total rather than
calling audio. `Game3D` maps that total to `ScoreCounted` requests and retains
`StageEnded`, high-score/menu decisions, stage loading, and every live-player
mutation. Invalid player counts fail to an inactive reset, non-finite or
non-positive elapsed time is ignored, and score-band arithmetic saturates at
`INT_MAX`. Eight standalone suites add 55 checks and give the new implementation
100% region/function/line/branch coverage. This is one partial PR 3.5 boundary,
not completion of the coupled `BonusSystem` or outer settlement transitions.
Automatic progression now loads into a candidate `StageMap` and commits it only
after validation. Failure restores the prior stage and complete player vector,
preserves the error for the menu, emits one return request, and leaves
`awaitingMenu` frozen even after that notification is consumed; only a successful
stage load starts another session state.
The twenty-first moves simulation pickup ownership and bonus rule mutations into
`game/bonus_system.{h,cpp}`. `Pickup` now stores the shared raylib-free `XZ`
instead of renderer-owned `Vector3`; the v1 replay digest deliberately retains
its historical zero-height field. The compiled system validates raw spawn draws,
advances lifetime before collection, preserves strict 1.875-tile AABB and
vector-first player selection, owns common and Grenade score/tally credit, and
commits all Helmet/Clock/Tank/Star/Gun/Boat/Bandage and enemy-clear state.
Grenade and Shovel return ordered, detached commands for the existing map,
event, FX, camera, and audio consumers. This retains type-before-position RNG,
complete x/z retry pairs, `BonusCollected` before Grenade destruction events,
and per-enemy armor-cue/explosion/event order. Invalid time/state, player index,
bonus values, non-positive enemy armor, and score/life overflow fail closed or
saturate. Twenty standalone suites add 142 checks, and the implementation has
100% region/function/line/branch coverage. Random-source ownership and concrete
presentation/map consumers deliberately remain in `Game3D`.
The twenty-second introduces a single raylib-free
`app/command_side_effect_sink.h` boundary and a compiled dispatcher for all
detached cancellation, map/core, tank, and bonus command leaves. `Game3D`
implements the sink synchronously, so event, impact/brick/explosion FX, radial
and assigned camera shake, and audio requests share one tested interface while
retaining their command-specific order. Live `Shell` impact and Shovel steel are
reported as domain commits and still execute immediately in their original
consumers; player armor FX still runs before the combat commit. Invalid audio
sentinels now fail closed, tank camera rejection and silent bonus-camera ignore
remain distinct, and no command queue is introduced. Fourteen standalone suites
add 73 checks across every leaf, payload, order, rejection, and domain-commit
alternative; the dispatcher reaches 100% region/function/branch and 99.55% line
coverage.
The twenty-third begins PR 3.4 with a compiled, raylib-free
`game/enemy_system.{h,cpp}` policy boundary. Production targeting now delegates
active-player filtering and strict Manhattan/vector-order selection; pursuit
delegates its dominant-axis and strict 0.7 roll rule; Armor firing delegates all
four directions, `None`, blocked firing, and the strict two-tile lane; spawn
selection delegates its three-slot rotation. The orchestration retains the
existing conditional RNG draws, mutable enemy loop, collision queries, shell and
event creation, FX, timers, and container cleanup. Seven standalone suites add
46 checks and give this policy implementation 100% region/function/line/branch
coverage. A 20-check production-path suite locks inactive/tied/closer targets,
real-versus-integer draw order, every Armor direction, owned-shell suppression,
and fully blocked retry without advancing IDs, rotation, or RNG.
The twenty-fourth moves the enemy frame-entry lifecycle gate into that same
module. `beginEnemyFrame()` owns the exact Destroyed, Creating, Frozen, and
Active timer order, pre-reset movement snapshots, showcase creation hold, and
atomic rejection of negative/non-finite elapsed time. `Game3D` now continues
only for an Active result; its map, ice, escape, RNG, movement, firing, FX, and
cleanup orchestration are unchanged. At that increment, the direct EnemySystem
executable had 12 suites and 60 checks at 100% coverage. Seven production-path checks lock all
four phases plus showcase hold, including frozen dust/ice progression, paused
fire/steering, and active ice momentum without consuming RNG.
The twenty-fifth moves the already-characterized local escape scorer and the
post-movement fire planner into `EnemySystem`. Escape selection retains snapped
lane queries, three ordered probes, ID-rotated ties, progress scoring, and the
reverse penalty; `Game3D` still performs collision queries and commits the
selected position/direction. Fire planning consumes an already-drawn float and
preserves Basic/Fast, Power `*= 0.8f`, and Armor `*= 0.4f` behavior without
drawing RNG, inspecting shells, or producing effects. `Game3D` retains the due
gate, exact draw point, owned-shell suppression, firing side effects, and final
rate-adjusted cooldown commit. The direct executable now has 18 suites and 72
checks, and `enemy_system.cpp` remains at 100% in all four coverage metrics.
Production checks also lock the complete per-enemy `R-R-R-R` then `R-R-I-R`
draw transcript across two enemies and atomic escape-wrapper commits.
The twenty-sixth moves enemy type/carrier/armor probability rules and complete
detached spawn-state construction into `EnemySystem`. `Game3D` still consumes
the random draws conditionally—Armor `R-R-R`, regular `R-I-R-R`—and owns spawn
availability, ID allocation, container/count/rotation commits, and tuned timer
updates. Direct tests lock strict probability and HP boundaries at the stage-17
formula split plus every initial field. Production tests add exhausted, waiting,
four-slot-full, blocked-then-available, exact-zero, tuned-success, and fully
blocked transactions with zero-RNG assertions where required. The direct
executable now has 21 suites and 82 checks; `enemy_system.cpp` remains at 100%
in all four coverage metrics.
The twenty-seventh moves the already-characterized active movement transaction
into `EnemySystem`. After lifecycle and steering, `advanceActiveEnemyMovement()`
owns movement-delay advancement, cardinal/ice travel resolution from a
caller-supplied on-ice fact, conditional lane snapping, the padded occupancy
probe, position/blocked commits, and detached track-dust intent. `Game3D`
supplies the live occupancy callback, consumes dust FX, commits its 0.19-second
cooldown, and only then reaches the existing fire draw. Direct tests lock the
snap-then-probe callback transcript, ice continuation/expiry, the 1/16-tile
probe margin, the 0.8-second blocked cap, zero-distance behavior, and atomic
rejection of invalid frame/time/speed/query inputs. The executable now has 28
suites and 101 checks; `enemy_system.cpp` retains 100% coverage in all four
metrics. Four production checks preserve open movement/dust/reload ordering and
the blocked Armor same-frame fire override.
The twenty-eighth moves the active steering transaction into `EnemySystem`.
`advanceActiveEnemySteering()` owns the strict ordinary-decision and
forced-escape gates, conditional `R-R-R` pursuit and `R-R-I` wander callback
pulls, ordinary direction/interval commits, collision-safe escape selection,
and the atomic escape route, blocked, yaw, and ice-state commit. `Game3D`
supplies synchronous `RandomSource`-backed real/integer callbacks and the live
occupancy query, then continues through movement, dust, and firing in the same
order. Four new standalone suites bring the direct executable to 32 suites and
105 checks while `enemy_system.cpp` remains at 100% in all four coverage
metrics. Production-path checks preserve successful and failed escape behavior,
the complete per-enemy draw transcript, and the 16-suite/7,125-check integrated
self-test total.
The twenty-ninth moves the complete spawn-attempt transaction into
`EnemySystem`. `advanceEnemySpawnTransaction()` owns atomic input validation;
queue and identifier exhaustion before timer debit; capacity and cooldown gates
after debit; rotated availability queries that stop at the first open slot;
blocked retry state; ID reservation before conditional regular `R-I-R-R` or
Armor `R-R-R` draws; detached enemy construction; and synchronous insertion
before remaining, rotation, and normal-timer commits. `Game3D` supplies tuned
configuration, live availability and seeded-random callbacks, and the concrete
`enemies_.push_back` adapter. Five new standalone suites add seven checks, so the
direct executable now has 37 suites and 112 checks. The expanded implementation
has 283 regions, 23 functions, 441 lines, and 238 branches, all at 100% coverage;
production-path checks retain the exact transaction order and the
16-suite/7,127-check integrated total. A fixed digest prefix also locks remaining
count, rotation index, next ID, and timer serialization in that order.
The thirtieth completes PR 3.4 by moving the post-movement firing transaction
into `EnemySystem`. `advanceEnemyFireTransaction()` owns the due gate, strict
fire-roll validation, conditional `R`, `R-Q`, and `R-Q-C` callback transcripts,
type/Armor reloads, normalized fire-rate setting, owned-shell suppression,
complete detached `Shell` construction, synchronous launch callback, and the
cooldown commit after that callback. `Game3D` supplies the live owned-shell
query and seeded-random callback, then synchronously inserts the shell, appends
its event, and creates its muzzle effect. A default-off observer locks that
`shell-event-muzzle` order and the absence of an enemy-fire audio request.
Six new direct suites add eight checks, bringing the executable to 43 suites
and 120 checks; the expanded 314 regions, 24 functions, 490 lines, and 262
branches remain at 100% coverage. Production cases also lock +30% fire tuning,
an ice-carried movement direction distinct from launch direction, an impacting
zero-life owned shell occupying the slot, and unrelated shell distractors. The
integrated total is 16 suites and 7,129 checks.
The thirty-first starts the next PR 3.5 slice by moving the complete
carrier-bonus release transaction into `BonusSystem`.
`advanceBonusReleaseTransaction()` snapshots Bandage eligibility once, owns the
exact `T-X-Z-Q-C` or `T-X-Z-Q-(X-Z-Q)*-C` transcript, validates each complete
draw attempt, keeps the type across rejected positions, and synchronously
passes a detached pickup intent to its commit callback. `Game3D` supplies the
seeded `RandomSource`/STL adapters and authoritative government-base predicate,
then preserves concrete `pickup-event-audio` presentation before the enemy
armor/score/death commit. Five direct suites add eleven checks, bringing the
standalone BonusSystem executable to 25 suites and 153 checks; its 139 regions,
10 functions, 220 lines, and 108 branches all reach 100% coverage. A malformed
dynamic draw emits no pickup/presentation but does not cancel the already-valid
carrier shell hit. The integrated total is 16 suites and 7,130 checks.
The thirty-second extracts the per-pickup collection/application transaction.
`advanceBonusPickupTransaction()` owns clock/eligibility update, a detached
collection intent, rule-state application, and the synchronous finish callback
(`U-B-A-F`). Missing callbacks fail atomically; retained and discarded pickups
invoke neither callback; a defensive rejected application rolls back the staged
consumer event and retains the pickup. `Game3D` keeps concrete presentation and
deletion in the proven `E0-A-C-M-O-Ep-D` order: append a zero-point event,
commit application state, consume commands, set the message, request audio,
patch actual points, then erase. Each pickup remains a separate transaction, so
stacked Bandages re-evaluate eligibility after the previous pickup. Four direct
suites add five checks, bringing BonusSystem to 29 suites and 158 checks; its
154 regions, 11 functions, 250 lines, and 118 branches remain at 100% coverage.
Integrated production cases bring the total to 16 suites and 7,134 checks.
The thirty-third extracts the remaining pure settlement-completion policy.
`planSettlementTransition()` returns a detached `SettlementTransitionPlan`:
`None` is a no-op, `GameOver` uses the strict best-player-score comparison to
choose high-score display or menu return, and `AdvanceStage` validates the stage,
wraps after the last map, caps surviving lives at 99, and restores an eliminated
player to two lives at level zero. Only the copied lives/level fields change.
`Game3D` retains every concrete consumer. Its default-off synchronous observer
locks four paths: `PlanReady` → `HighScoreCommitted` →
`HighScoreAudioRequested` → `HighScoreDisplayCommitted`; `PlanReady` →
`HighScoreCommitted` → `MenuRequested`; `PlanReady` →
`StageCandidateRequested` → `StageCandidatePrepared` →
`PlayerProgressionCommitted` → `StageLoadCommitted`; and `PlanReady` →
`StageCandidateRequested` → `StageCandidateRejected` → `MenuRequested`.
The observer must not throw, re-enter or mutate `Game3D`, or retain the plan
reference.
The candidate is prepared before any live progression and must report the exact
requested stage. A rejected loader or stage mismatch preserves the old world and
requests the menu with its error; the remaining prepared-stage commit has no
recoverable failure. Failed `start()` and `restart()` attempts likewise restore
the prior settings, nations, stage, players, and event snapshot. StageMap, audio,
display, and navigation remain outside `SettlementSystem`.
Three direct planner suites add ten checks, bringing SettlementSystem to eleven
suites and 65 checks; its 143 regions, 20 functions, 225 lines, and 104 branches
remain fully covered. Six integrated checks bring the game run to 16 suites and
7,144 checks and lock full-player commits, `StageStart` request-time state,
wrong-stage rejection, different-configuration start rollback, and
restart/manual-stage load failure.
The thirty-fourth moves stable outer pickup-vector advancement into
`BonusSystem`. `advanceBonusPickups()` executes one synchronous `U-B-A-F`
transaction at a time, erases collected or discarded pickups at their current
index, and immediately processes the shifted successor. Discard remains silent;
an application-rejected pickup remains in place without blocking later pickups;
and an optional collected callback receives detached values only after erasure.
Missing begin/finish callbacks reject the entire batch before any pickup,
player, or enemy mutation. `Game3D` retains the concrete
`E0-A-C-M-O-Ep-D` callbacks and presentation consumers. A nonzero-time
production case proves that a carrier pickup released during shell resolution is
aged and collected by the same frame's vector transaction. Four direct suites
add five checks, bringing BonusSystem to 33 suites and 163 checks; its 171
regions, 12 functions, 279 lines, and 134 branches remain fully covered. The
integrated run reaches 16 suites and 7,145 checks.
The thirty-fifth extracts pure settlement-entry planning.
`planSettlementBegin()` validates stage and participant scope, rejects a cleared
report with a destroyed base, classifies `Cleared`, `BaseDestroyed`, or
`PlayersDefeated`, and copies only configured scores and tallies into a detached
`SettlementBeginPlan`. `Game3D` consumes that value synchronously in
`PlanReady` -> `ReportCommitted` -> optional `StageEndEventAppended` ->
`AudioStopBoundaryPassed` order. Production checks cover the event-free showcase
and all three stage-end reasons, preserve earlier update events, and prove that
`ScoreCounted` requests observe already-committed count state while confirmation
does not synthesize skipped cues. Three direct suites add 24 checks, bringing
SettlementSystem to 14 suites and 89 checks; its 170 regions, 21 functions, 251
lines, and 126 branches are fully covered. Four integrated checks bring the game
run to 16 suites and 7,149 checks and the aggregate gate to 161 suites and 8,313
checks.
The thirty-sixth introduces the narrow, raylib-free
`audio/audio_output.h` runtime boundary. It exposes only synchronous
`play(AudioCue)`, `updateEngine(active, moving)`, and `stopAll()` operations.
`Game3D` now retains a nullable, non-owning `AudioOutput *`; normal startup still
stack-owns one concrete `AudioBank`, loads and unloads it around the audio-device
lifetime, and leaves file mapping, cue priority, overlap, and concrete voice
policy inside that bank. A recording output verifies post-commit `StageStart`,
creating-player engine mute followed by ready moving-engine state, pause
`stopAll` then `Pause` with silent resume, and settlement `stopAll` only after
the `StageEnded` append. These are headless boundary-call checks, not evidence of
hardware playback or of `AudioBank`'s internal voice-priority behavior. The
integrated run reaches 17 suites and 7,152 checks and the aggregate gate reaches
162 suites and 8,316 checks.
The thirty-seventh adds the compiled, raylib-free
`game/player_system.{h,cpp}` boundary and moves `DirectionButtonFrame`,
`PlayerControlFrame`, and `PlayerInputFrame` out of `main.cpp`.
`planPlayerControl()` returns only final drive, propulsion, and held-fire
intent. It preserves pressed priority and held fallback in
North/South/West/East order, including legacy malformed pressed-but-not-held
behavior and invalid directions `5`/`255`; this is parity, not fail-closed
validation. After the existing death/creation gates, `Game3D` debits the fire
cooldown, commits drive unconditionally, changes yaw only while propelling,
then retains ice, map, snap, collision, dust, and final held/due/cap fire
sequencing. Three direct
suites contain 13 checks; independent literal tables exercise five valid
current directions x 16 held masks x 16 pressed masks x two fire states, or
2,560 input combinations (not 2,560 checks). Two additional checks preserve
invalid directions `5` and `255`. Three
integrated scripted checks lock the pressed-only consumer and moved-position
fire fixture/order. At that point the game run was 17 suites and 7,155 checks;
core/game was 126 suites and 1,062 checks, app remained 22 suites and 115 checks,
and the aggregate was 165 suites and 8,332 checks. At that increment, all 35
regions, three functions, 42 lines, and 34 branches in `player_system.cpp` were
covered.
The thirty-eighth extends `PlayerSystem` with a post-movement scalar firing
transaction. It accepts only the request, already-debited cooldown, active-shell
count, owning player index, level, tank position, drive direction, and explicit
reload/spawn scalars. Valid due attempts reset cooldown before the shell-cap
decision; accepted attempts synchronously emit a detached scalar launch intent.
The transaction preserves level clamping, `None`/`5`/`255` zero-vector launches,
and the rule that impacting shells still occupy a slot. `Game3D` retains shell
construction and the five-step shell insertion, event, muzzle FX, camera-shake,
and audio adapter. At that increment, direct PlayerSystem coverage grew to
eight suites and 22 checks. The 32-check scripted-player-input suite
additionally locks full-slot silence, post-movement launch position, ice-carried
movement versus drive direction, player-index ownership, and concrete presentation order. At that
increment, totals were 17 integrated suites and 7,161 checks, 131 core/game
suites and 1,071 checks, and 22 app suites and 115 checks, or 170 suites and 8,347 checks across
all fifteen profiles. All 60 regions, four functions, 74 lines, and 54 branches
in `player_system.cpp` were covered.
The thirty-ninth adds `advanceActivePlayerMovement()` as an entity-free scalar
transaction. It rejects a missing availability query, invalid elapsed time or
speed, and overflowing distance without mutation or queries. Valid attempts
commit drive/yaw plus the existing ice state machine, issue the optional exact
lane-snap query before the forward-candidate query, and preserve an accepted
snap when the candidate is blocked. Blocking stops movement, clears the slip
timer, and returns travel to the drive direction. Track dust is returned as a
detached position/velocity intent; `Game3D` writes back state, presents dust and
then commits its 0.120-second clock before firing. The live adapter still
derives ice from `StageMap`, passes Boat permission into the occupancy query,
and processes players in vector order. Production regressions prove P2 observes
P1's same-frame committed position, Boat-only water traversal, and blocked
movement followed by same-frame fire. At that increment, PlayerSystem had 14
direct suites and 40 checks, while the scripted-player-input suite had 38
checks. At that increment, totals were 17 integrated suites and 7,167 checks,
137 core/game suites and 1,089 checks, and 22 app suites and 115 checks: 176
suites and 8,371 checks across all fifteen profiles. The 39 production files
contained 18,865 lines; `main.cpp` was 7,086 lines, `self_tests.inl` was 10,424
lines, `player_system.h/.cpp` were 166/181 lines, and the direct test was 1,046
lines. The pure boundary remained 13 headers plus seven implementations. All 96
regions, five functions, 137 lines, and 86 branches in `player_system.cpp` were
covered.
Coverage was 49.19% regions, 62.78% functions, 44.63% lines, and 51.80% branches.
`main.cpp` reached 23.00%/58.91%/29.28%/20.43%, while
`core/coordinates.h` reached 98.73%/100%/99.09%/97.50%.
The fortieth adds `beginPlayerFrame()` as an entity-free five-clock transaction.
It rejects non-finite or negative elapsed time atomically; otherwise dust,
shield, and streak clocks advance in every valid phase, creation advances only
for an active player that entered Creating, and fire advances only in Ready. Phase is
selected from the entry snapshot, so crossing creation through zero still
returns Creating. Malformed clock fields preserve the legacy entry comparison
and ordered `std::max(0.0f, clock - elapsed)` results. `Game3D` commits all
returned clocks before death/respawn handling, clears moving in Creating, and
reads input only in Ready. Respawn cleanup, active-shell counting, and launch
payloads now consistently use the player slot index rather than a mutable entity
ID. Direct frame-boundary and production-path regressions passed together with the
sanitizer gate. At that increment, PlayerSystem had 19 direct suites and
48 checks; the scripted-player-input suite had 41 checks. Totals were 17
integrated suites and 7,170 checks, 142 core/game suites and 1,097 checks, and 22
app suites and 115 checks: 181 suites and 8,382 checks across fifteen profiles.
The 39 production files contained 18,959 lines; `main.cpp` was 7,107 lines,
`self_tests.inl` was 10,523 lines, `player_system.h/.cpp` were 203/217 lines, and
the direct test was 1,220 lines. The pure boundary remained 13 headers plus seven
implementations. All 108 regions, six functions, 166 lines, and 94 branches in
`player_system.cpp` were covered. Overall coverage was
49.32%/62.78%/44.78%/51.89%; `main.cpp` reached
23.38%/58.75%/29.40%/20.73%, while `core/coordinates.h` reached
98.73%/100%/99.09%/97.50%.
The forty-first adds `advanceInactivePlayerDeath()` as an entity-free
`deathTimer`/lives transaction. Invalid elapsed time is atomic. A non-positive
entry timer returns `None` unchanged; a positive timer that reaches or crosses
zero debits a life in the same frame. NaN and positive/negative infinity retain
the legacy entry comparison and ordered-`std::max` outcomes, while non-positive
life counts saturate at zero without `INT_MIN` overflow. `Game3D` commits the
returned timer and lives before slot-owned shell cleanup, spawn reset,
`PlayerRespawned`, and `PlayerRespawn` audio. Final-life shells are not cleaned
up and continue delaying Game Over. Synthetic `id != slot` regressions preserve
entity identity in the event while proving cleanup and defeat checks use the
vector slot. PlayerSystem then had 25 direct suites and 58 checks; the
scripted-player-input suite has 43 checks and the observable-event suite has 75.
At that increment, totals were 17 integrated suites and 7,173 checks, 148
core/game suites and 1,101 checks, and 22 app suites and 115 checks: 187 suites
and 8,389 checks across fifteen profiles. The 39 production files contained
18,997 lines; `main.cpp` was 7,118 lines, `self_tests.inl` was 10,595 lines,
`player_system.h/.cpp` were 228/239 lines, and the direct test was 1,410 lines.
The pure boundary remained 13 headers plus seven implementations. All 126
regions, seven functions, 183 lines, and 106 branches in `player_system.cpp`
were covered. Overall coverage was 49.49%/62.92%/44.87%/52.13%; `main.cpp`
reached 23.77%/59.14%/29.58%/21.32%, while `core/coordinates.h` remained
98.73%/100%/99.09%/97.50%.
The forty-second adds `preparePlayerSpawnState()` as an entity-free spawn/reset
transaction over nineteen owned fields: position; yaw and drive/travel
directions; active, moving, Boat, and ice flags; six lifecycle/motion clocks;
shield; HP; level; direct-kill streak; and streak-popup time. It validates the
progression enum, finite spawn coordinates, and non-negative finite creation,
fire, and shield durations before one atomic commit. `Preserve` retains level,
streak, popup time, and positive HP, restoring non-positive HP from maximum HP;
`Reset` clears progression and copies maximum HP. Maximum HP deliberately remains
unvalidated: reset/recovery copies even zero, negative, or extreme legacy values,
and Preserve does not clamp positive HP. `Game3D` retains the `id == 0` versus
other-ID spawn-point mapping, entity snapshot/writeback, slot-owned shell cleanup,
events, and audio. PlayerSystem then had 31 direct suites and 64 checks. At that
increment, totals were 17 integrated suites and 7,181 checks, 154 core/game
suites and 1,107 checks, and 22 app suites and 115 checks: 193 suites and 8,403
checks across fifteen profiles. The 39 production files contained 19,136 lines;
`main.cpp` was 7,158 lines, `self_tests.inl` was 10,826 lines, and
`player_system.h/.cpp` were 277/289 lines; the direct test was 1,793 lines. The
pure boundary remained 13 headers plus seven implementations. All 154 regions,
eight functions, 229 lines, and 134 branches in `player_system.cpp` were
covered. Overall coverage was
49.70%/62.98%/45.17%/52.40%; `main.cpp` reached
23.85%/59.14%/29.94%/21.34%, while `core/coordinates.h` remained
98.73%/100%/99.09%/97.50%. The observable-event suite reached 76 checks,
including post-commit respawn cue ordering. ASan/UBSan passed all 17 integrated
suites and 7,181 checks.
The forty-fourth release-screenshot increment added a header-only app parser plus
a `main.cpp` framebuffer adapter without changing gameplay or maps. Five new
suites and 21 checks brought the aggregate to 198 suites and 8,424 checks
across sixteen profiles. The 40 production files contained 19,401 lines;
`main.cpp` was 7,273 lines. Overall coverage was
49.70%/63.30%/45.24%/52.27%, `main.cpp` reaches
23.42%/59.14%/29.42%/20.83%, and the parser reached
100%/100%/100%/95.45%. ASan/UBSan, the distribution verifier, and the explicit
1280x720 Retina GPU smoke passed.
The forty-fifth increment adds the raylib-free atomic file writer. PNG bytes are
flushed through a private same-directory file, then published with a hard link
that fails if the destination appeared after startup. The eight screenshot
suites and 39 checks cover this deterministic race plus regular files,
symlinks, invalid inputs, missing parents, and temporary cleanup. The current
aggregate is 201 suites and 8,442 checks across sixteen profiles. The 42
production files contain 19,662 lines; `main.cpp` is 7,302 lines. Overall
coverage is 49.77%/63.83%/45.38%/52.16%; `main.cpp` reaches
23.32%/59.14%/29.29%/20.72%, and the writer reaches
59.60%/100%/68.70%/51.79%. ASan/UBSan and the explicit Retina GPU smoke pass.
`Shell` and `ShellOwner` remain entity types. `Game3D` retains the live-shell
loop, supplies the carrier-release callback through `CombatSystem`, consumes
projected events beside audio and effects, and consumes each command's final
impact at the original point. It also retains explosion, camera, audio, and
shell-impact side effects; concrete presentation remains in `src/main.cpp`.
`ImpactKind::None` does not by itself identify a pass-through: a power shell may
clear forest and continue, while a core hit uses `ImpactKind::None` and stops.
Consumers use `target`, `mapStopsShell()`, and `stopsShell()` instead.

The map, wall destruction, player input, movement, firing, damage, pickups, and
stage outcomes now have direct production-path characterization tests. Raylib
player-key polling is isolated in an application adapter, and `Game3D` records a
data-only ordered event stream. Detached command leaves cross one tested
side-effect boundary, while `Game3D` reaches its main-owned concrete audio bank
through the separate raylib-free `AudioOutput` boundary. The concrete raylib
effects implementation and several non-command producers remain inside
`Game3D`; extracting those services is still an ownership barrier. Gameplay
randomness is deterministic and scriptable in tests but remains owned by
`Game3D`.

## Target Boundaries

```text
app/        lifecycle/setup target; owned command values/mappers and sync sink
core/       coordinates, directions, settings, shared value types
game/       entities, BonusSystem, StageGenerator, StageMap, CombatSystem,
            SettlementSystem, incremental EnemySystem, GameSession
platform/   raylib input adapter
audio/      shared cue values and runtime output interface; mapping target
render/     Renderer, HUD, lighting, models, GPU resources
tests/      focused rule and session tests
```

The current input adapter produces commands for `Game3D`, which now records
`GameEvent` values such as `ShellFired`, `TankDamaged`, and `BonusCollected`.
The target `GameSession` keeps that contract while rendering reads a const
snapshot and audio/effects consume events. Code eventually moved into `core/`
and `game/` must not call raylib. `tanks3d::core` now owns coordinates,
directions, strict center overlap, lane snapping, ice travel, advanced-setting
normalization, player-level statistics, and the sole `Nation` definition.
`tanks3d::game` owns `Player`, `Enemy`, `Shell`, `GameEvent`, classified stage
tallies, hit/death transitions, bonus type/timing/weighting, raylib-free pickup
state, collector decisions, and bonus state commits. `StageGenerator` owns
deterministic tile generation; `StageMap` owns stage normalization, spawn-route
validation, terrain collision, brick masks, national-base wall state, and
Shovel steel protection. `CombatSystem` owns the mechanically extracted shell
helpers, frame scheduling/movement/cleanup, swept and greedy batch
cancellation, the map-impact mutation/snapshot boundary,
government-core hit/state transition, read-only tank targeting in both
directions, the validated enemy/player commits, the ordered single-shell
physical-impact transaction, its pure event projection, and cancellation-event
projection described above. It
does not own carrier RNG, presentation, final shell-impact animation, or outer
micro-step ordering.
`planSettlementBegin()` owns pure report-entry validation, reason classification,
and the detached score/tally snapshot. `SettlementState` owns the immutable
report snapshot, displayed-count progress, confirmation, Idle timing, and a
presentation-free completion value.
`planSettlementTransition()` owns the pure strict high-score, stage-wrap, and
lives/level decisions in a detached player snapshot. These pure paths do not
emit `GameEvent` values, request cues, load a stage, navigate, or mutate live
players; `Game3D` performs those concrete commits.
Compatibility imports keep renderer callers unchanged, and checked bridges prove
that render code consumes the shared nation, bonus type, and game-owned `Pickup`
with `core::XZ`; only the renderer creates local `Vector3` heights.

## Migration Order

1. **Characterize behavior.** Input, random, ordered-event, and deterministic
   digest seams are complete. Add focused cases only when an extraction exposes
   an uncovered branch or a bug needs a production-path regression.
2. **Extract stable primitives.** Coordinates, directions, settings, nation
   selection, player progression, entity data, bonus probability rules, and the
   `StageGenerator`/`StageMap` split are complete. Combat extraction is in
   progress. Both tank-target outcomes and commit boundaries, shell frame
   scheduling/lifetime, surviving opposing-shell cancellation, the typed
   single-shell physical-impact transaction, and physical-result event
   projection are complete. Cancellation-result projection and its narrow typed
   `Game3D` consumer are also complete. Cancellation, map/core, and all
   tank-hit event/FX/audio/final-impact paths now have separate default-off
   recording seams. All three paths' owned commands and pure
   mappers, plus the cue value and output interface, now live behind raylib-free
   `app/` and `audio/` seams. All four command families now dispatch their
   event/FX/camera/audio
   leaves through one synchronous raylib-free sink; `Game3D` still implements
   that concrete sink and owns live-shell/Shovel domain commits.
   Pure settlement-entry planning, counting/confirmation/timeout state, and the
   detached high-score/stage/player-progression plan are complete; concrete
   settlement events, cues, display, navigation, map loads, and live-player
   commits remain in `Game3D` behind characterized synchronous seams.
   Pickup `XZ`
   storage, carrier-release transaction, lifecycle/collection decisions,
   per-pickup `U-B-A-F` transaction, stable pickup-vector advancement, scoring,
   player/enemy effects, and ordered side-effect commands now live in
   `BonusSystem`; the seeded-random adapter,
   authoritative base query, map mutation, event/message/audio phases, and
   concrete presentation consumption remain at the orchestration edge. Enemy
   target, pursuit, local escape selection, fire
   planning, Armor-fire, and spawn-rotation policies plus the four-phase frame
   lifecycle gate, spawn probabilities, armor selection, detached enemy
   construction, active cardinal/ice movement, the active steering/escape
   transaction, the complete spawn-attempt transaction, and the post-movement
   firing transaction now live in `EnemySystem`, backed by both direct and
   production-path characterization. Player input values, the five-clock
   `Inactive`/`Creating`/`Ready` frame-entry transaction, the inactive scalar
   death-timer/lives transaction, final drive/propulsion/fire-held planning, the
   active scalar cardinal/ice movement transaction, detached dust intent,
   post-movement scalar firing transaction, and 19-field spawn/reset transaction
   now live in `PlayerSystem`. `Game3D` retains entity snapshot/commit and phase
   routing, ID-to-position mapping, slot-owned shell cleanup, respawn event/audio
   consumption, the map-derived ice fact, live
   occupancy/Boat query, scalar movement writeback and dust-presentation
   adapter, shell construction, and concrete fire presentation.
   PR 3.4 is complete; PR 3.5 remains partial. Owned-shell cleanup, spawn-point
   mapping and entity adaptation, events, audio, entities, map, rendering,
   cameras, and concrete movement/fire orchestration and presentation adapters
   remain in `Game3D`.
3. **Migrate event consumers.** Replace the temporary event plus presentation
   dual-write only after audio/effect mappings have dedicated tests. Keep the
   existing renderer consuming a compatibility snapshot during this phase.
4. **Split platform services.** Move `AudioBank`, raylib input, render targets,
   lighting, and resource owners into implementation files with move-only RAII.
5. **Migrate visual modules unchanged.** Move procedural tanks, national bases,
   shaders, and effects only after screenshot and gameplay checks exist. Redesign
   them in separate changes, never during an ownership extraction.

## Completed Hardening

- `Game3D` accepts an explicit test seed while normal play retains one-time
  `random_device` seeding. All self-test sessions are deterministic, and a
  transcript compares real enemy-generation and pickup-release paths.
- A typed scripted `RandomSource` exercises the same production spawn and
  pickup paths while checking distribution ranges, lazy draw consumption,
  strict probability boundaries, Bandage weighting, and base-position retries.
- `Game3D` consumes data-only two-player input frames. A thin raylib adapter
  preserves the existing bindings, while headless production-path tests lock
  direction priority, ice travel, held fire, shell caps, and player isolation.
- `Game3D` records ten data-only rule event types for each update. A 69-check
  suite verifies event causes, ordering, authoritative state, and non-repetition
  across combat, pickups, player lifecycle, base damage, and settlement.
  Default-off cancellation, map/core, and paired tank observers additionally
  verify real event/FX/camera/audio-request/final-impact order while remaining
  empty in normal play.
- A versioned `SessionDigest` explicitly serializes deterministic map, entity,
  mode, settlement, and mt19937 state. Two fixed-seed 480-frame runs at 1/60
  second produce identical per-frame event counts, non-empty event sequences,
  and digests within the same build; this is not a cross-version compatibility
  guarantee.
- Runtime shell cancellation and its regression test now use the same swept
  collision helper in `CombatSystem`, preventing fast opposing rounds from
  crossing between samples.
- GPU and audio resource-owning classes are non-copyable, preventing accidental
  duplicate unloads.
- The first core module has a 93-check table-driven suite covering all five
  direction values, strict 5/16 lane and AABB contact boundaries, and ice-state
  transitions. It reaches 98.18% line and 96.25% branch coverage.
- A second raylib-free executable adds 132 checks for HP 1–6, every 5% tuning
  step and rounding boundary, nation normalization/cycling, and level/Star
  progression. `gameplay_rules.h` has 100% line/branch coverage and `nation.h`
  has 100% across regions, functions, lines, and branches.
- `Nation` is defined only in `core/nation.h`. A standalone renderer bridge
  compiles `wwii_tank_model.h` and `tank_assets.h` and verifies they consume the
  exact shared type; integrated tests directly check all 12 nation/tier vehicle
  IDs and their clamped endpoints.
- `game/entities.h` and `game/bonus_rules.h` move entity defaults, scoring,
  streak, hit/death transitions, and bonus timing/weighting out of `main.cpp`.
  Four standalone suites add 141 checks across HP 1–6, Shield/Boat priority,
  Bandage eligibility, direct/grenade tally separation, death/respawn, and
  owned-shell cleanup. Both headers reach 100% region/function/line/branch
  coverage, and a renderer bridge verifies the shared bonus type.
- `game/stage_map.h` and `game/stage_map.cpp` move route validation, terrain and
  national-base collision, brick damage, wall health, and steel timing out of
  `main.cpp`. Four standalone suites add 136 checks, preserve all 35 integrated
  layout signatures and validated routes, and cover `stage_map.cpp` at 97.97%
  region, 100% function, 98.18% line, and 93.40% branch; the header is fully
  covered.
- `game/stage_generator.h` and `game/stage_generator.cpp` separate deterministic
  tile generation from mutable map state. Two standalone suites add 43 checks
  over all 35 direct generator goldens and deterministic repeats; the
  implementation has 100% region/function/line/branch coverage, while the map
  bridge retains the same signatures and routes.
- `game/combat_system.h` and `game/combat_system.cpp` mechanically extract shell
  spawn, impact state, cancellation eligibility/overlap, swept cancellation,
  `StageMap` wall/core separation, map-impact query/mutation, and the binary
  government-core transition without changing collision order. It also
  evaluates the first eligible enemy hit and predicted armor result without
  mutating enemy or shell state, then validates and commits armor, direct score,
  classified tally, streak, and fatal enemy state. A narrow callback preserves
  carrier-bonus release before armor mutation without importing RNG or
  presentation. It likewise evaluates enemy-shell/player-tank hits without
  mutation and validates Shield/Boat/HP plus fatal-player state before commit;
  a read-only pre-commit callback preserves the original generic impact effect.
  Per-frame shell aging, shared micro-step scheduling, position snapshots,
  movement, stable expiry cleanup, and greedy surviving-pair cancellation now
  use the same pure module. Cancellation results snapshot vector indices,
  attribution, midpoint, and both impact positions without retaining references.
  `ShellPhysicalImpactResult` now composes guard, map, core, enemy, and player
  resolution in the established order for one shell at a time. It exposes both
  validated tank commits and a `resolved` flag while deliberately deferring the
  shell impact animation until after presentation. `game/game_event.h` owns the
  value-only event contract, and `eventsForPhysicalShellImpact()` maps recorded
  brick damage, wall/core damage, and committed tank outcomes without reading
  live containers. Shield, Boat, steel, boundary, dead-core, and unresolved
  results remain event-free. `eventForShellCancellation()` maps each detached
  cancellation result to exactly one source-attributed event, and a narrow
  cancellation app mapper builds an owned fixed command, and its `Game3D`
  consumer preserves per-pair event/effect/audio interleaving. An optional
  observer snapshots that command and real event/effect counts after each
  corresponding action for tests only.
  The map/core and tank app mappers emit owned variant actions to synchronous
  `Game3D` consumers. Their leaves, cancellation fields, player pre-commit FX,
  and Bonus commands now share the compiled `CommandSideEffectSink` dispatcher.
  Live shell impact and Shovel steel remain explicit domain commits. Action-only
  observers snapshot map/core payloads after
  each real side effect; paired tank observers preserve generic player armor FX
  before commit while recording each consumed command payload. Neither path
  imports raylib into `game/`.
  `CombatOutcome` snapshots source and target state;
  `mapStopsShell()` and `stopsShell()` separate map presentation from physical
  stopping. Twenty standalone suites add 132 checks; `combat_system.cpp`
  reaches 100% region/function/line coverage and 97.52% branch coverage, while
  `game_event.h` reaches 100% in all four metrics. The four raylib-free app
  executables add twenty-two suites and 115 checks; the shared dispatcher adds
  fourteen suites and 73 checks and reaches 100% region/function/branch plus
  99.55% line coverage. The map/core implementation
  reaches 99.37% line and 94.63% branch coverage, the tank implementation
  reaches 98.29% and 86.05%, and cancellation reaches 100% in all four metrics.
  A fifth raylib-free app executable covers the default-off screenshot options
  and atomic no-replace writer with eight suites and 39 checks; `main.cpp`
  retains framebuffer capture, PNG encoding, and renderer-resource lifetime.
  PR 3.3 remains in progress because the concrete sink implementation and final
  shell-impact side effect still belong to `Game3D`.
- `game/bonus_system.h` and `game/bonus_system.cpp` own `Pickup` XZ state,
  spawn-draw validation, the carrier-release transaction, lifetime/collection
  selection, the per-pickup `U-B-A-F` transaction, stable vector advancement,
  saturating bonus credit,
  player/enemy effects, and ordered detached
  world/presentation commands. Standalone suites cover every valid bonus,
  malformed input, strict collision edge, weighting/coordinate boundaries,
  release transcripts, vector erasure/retention order, Grenade order, and integer
  caps. The implementation has 33 suites and 163 checks and reaches 100% in all
  four metrics. `Game3D`
  retains the seeded-random adapter,
  authoritative base query, concrete container/event/audio/FX/camera consumers,
  and message text.
- `game/settlement_system.h` and `game/settlement_system.cpp` own pure report-entry
  validation/reason planning, the report snapshot, score/K.O. cadence,
  confirmation, Idle timeout, counted-step output, completion kind, and detached
  high-score/stage/player-progression plan. The
  original report-state suites cover catch-up updates, exact timeout boundaries,
  snapshot retention, invalid player counts/time, and `INT_MAX` saturation;
  focused plan cases cover no-op/invalid input, strict high-score selection,
  stage wrap, eliminated-player recovery, and the 99-life cap. Three entry-plan
  suites cover exact snapshots, detachment, all reasons, event intent, stage and
  participant bounds, vector-size mismatches, and atomic contradiction rejection.
  The module has 14 suites and 89 checks and reaches 100% in all four metrics.
  `Game3D` retains events, audio, display, menu, candidate-map validation/commit,
  and live-player
  mutation. Dependency checks keep SettlementSystem raylib- and StageMap-free.
- `game/enemy_system.h` and `game/enemy_system.cpp` own target choice, pursuit
  axis, local escape selection, type-specific fire planning, Armor firing
  eligibility, spawn-slot rotation, spawn probability/armor rules, detached
  enemy construction, the four-phase frame-entry lifecycle/timer gate, the
  active movement transaction with detached dust intent, and the active
  steering transaction with conditional random-callback pulls and atomic escape
  commits. It also owns the complete spawn-attempt transaction: ordered queue and
  identifier exhaustion, timer debit and gates, rotating live queries, blocked
  retry, conditional random pulls, ID reservation, construction, synchronous
  insertion, and final state commits. The module also owns the complete
  post-movement fire transaction: due/roll/query gates, normalized type-specific
  reload, detached shell intent, synchronous launch, and final cooldown commit.
  Forty-three standalone suites add 120 checks and reach 100% in all coverage
  metrics. Integrated production checks
  keep lifecycle clocks, steering thresholds and exact conditional draw order,
  escape success/failure,
  movement/dust sequencing, complete spawn commits, firing outputs, and blocked
  retry state observable. `Game3D` retains map-derived ice, tuned configuration,
  live occupancy and seeded-random callbacks, synchronous enemy/shell container
  insertion, and concrete event, muzzle, audio-observation, and renderer-facing
  presentation.
- `game/player_system.h` and `game/player_system.cpp` own the data-only input
  frames, five-clock frame-entry phase transaction, inactive scalar death
  transaction, pure final
  drive/propulsion/held-fire plan, active scalar movement transaction, detached
  dust intent, scalar player-fire transaction, and entity-free 19-field
  spawn/reset transaction. Thirty-one direct suites contain 64 checks and
  independently exercise 2,560 control combinations plus
  frame/death invalid elapsed atomicity, entry and cross-zero timer behavior,
  malformed-clock `std::max` parity, `INT_MIN` life saturation,
  invalid/overflow movement rejection, exact snap then forward queries,
  cardinal/ice/blocking behavior, dust detachment, legacy invalid directions,
  cooldown, cap, slot ownership, level, launch parity, complete Preserve/Reset
  write masks, invalid spawn atomicity, signed-zero and maximum-finite inputs,
  and malformed-HP parity. The production suite additionally locks
  clock/death/spawn commit and phase routing, creation cross-zero behavior,
  `id != slot` respawn cleanup and final-shell delay, ordered two-player
  movement, Boat/water traversal, blocked-fire sequencing, and concrete
  presentation. The observable-event suite has 76 checks, including post-commit
  respawn cue ordering. Targeted regressions and ASan/UBSan pass all 18
  integrated suites and 7,191 checks.
- `audio/audio_output.h` owns the three-operation, raylib-free runtime output
  interface. `Game3D` stores only a nullable, non-owning pointer; `main` retains
  stack ownership of `AudioBank` and its device/resource lifecycle. A recording
  implementation covers stage-entry, engine-state, pause/resume, and settlement
  ordering without opening an audio device. This proves synchronous delegation,
  not audible hardware output or the concrete bank's priority/voice internals.
- The Makefile emits header dependency files. macOS CI performs optimized and
  warning-as-error debug builds, compiles pure core/game plus app/audio value
  boundaries without raylib include paths, scans dependency direction, and runs
  the full headless gates. Instrumented compiled-module
  tests reuse canonical production coverage objects; the CombatSystem test
  driver is also instrumented for inline event value semantics. Ten pure-rule
  and seven app-layer executables plus the integrated and exclusive-capability
  runs of the instrumented game now contribute to nineteen profiles, which
  merge without duplicate-map warnings. The capability profile reaches 86.36%
  line coverage in its implementation while exercising the production parser,
  recorder, JSON serializer, and pre-resource CLI branch.
- The macOS Alpha distribution path requires and statically links raylib 6.0,
  matches its real deployment target, carries the exact raylib license and the
  version-locked embedded-dependency notices, applies an ad-hoc integrity
  signature, and verifies the ZIP checksum, manifest, system-only dependencies,
  signature, and full self-test after a clean extraction. Developer ID signing
  and notarization remain release work.
- Post-tag verification clones the attested source commit privately and runs
  that commit's strict candidate verifier, so later documentation cannot move
  or silently reinterpret the release tag. A separate machine-readable status
  gate binds manual evidence and approvals to that candidate. Interactive,
  command and gameplay-event records use strict JSON formats. The v2
  interactive evidence path starts from an all-`NOT_RUN` observation plan;
  its standard-library compiler accepts only 70 explicitly passed rows across
  main-menu/control and five live-play categories, with exact checks,
  non-blocking notes, bounded timestamps, valid captures, and
  independent reviewer identity. Recordings are hashed as streams, and input
  identity is rechecked before a dirfd/inode-bound no-replace publication. It
  writes a new status snapshot plus six session/event pairs without modifying
  the source status or overwriting evidence. V2 event logs identify the QA
  compiler—not the game binary—as their producer and bind the canonical
  observation-manifest hash. Main-menu, one-player, and two-player evidence use
  distinct >=64 KiB recordings. The menu context owns both navigation/confirm/
  exit alternatives plus ranges, defaults/reset, and `Esc` preservation;
  one-player owns P1/shared hotkeys plus runtime tuning/Bandage; two-player owns
  P1/P2/shared hotkeys plus the same runtime checks. An `or` check requires every
  published alternative. F11 is borderless, while F8 is corroborated by its
  2048x2048 high-quality / 1024x1024 balanced shadow-map line captured in the
  context recording and observation notes, not a separate log artifact.
  Published-control PASS also requires the eight fixed advanced-settings checks.
  The v2
  clean-Mac contract uses a real Safari HTTPS acquisition so quarantine is
  observed rather than synthesized, binds that acquisition's identity and
  interval in a structured record, then records the five post-download
  checksum/quarantine/signature commands. Clean-Mac binds only the
  `gatekeeper_launch` session; `main_menu_reached` remains a Gatekeeper detail,
  not a reason to reuse the controls session. Its two-machine tooling keeps the
  trust boundary explicit: the release workstation prepares a minimal,
  candidate-bound kit containing no game checkout or candidate; a `/bin/zsh -f`
  collector on the fresh Mac writes a new permission-restricted raw intake;
  and an independent reviewer compiles that intake and its continuous recording
  (not a static PNG) back in the repository. The collector opens the planned URL
  in Safari but never drives the download, Finder extraction, Gatekeeper dialogs,
  or **Open Anyway**; it never writes or removes quarantine metadata and
  preserves five stdout/stderr pairs (ten raw command streams). The
  compiler rejects test-mode, incomplete, replaced, or candidate-mismatched
  intake and publishes a no-overwrite evidence pack with `status.next.json`;
  it never mutates canonical release state. A shared no-follow ISO-BMFF parser
  rejects padded file-header stand-ins and requires positive duration, a video
  track, samples, and media payload. The final verifier independently parses
  the raw plan, intake, Safari-origin plist, command streams, and recording;
  structural checks support, but do not replace, the independent human review
  of what the continuous capture shows. The v2 performance path embeds
  source identity in the distribution binary and exposes a no-window,
  no-resource capability JSON that runs the production parser and recorder
  self-check. The distribution verifier checks that exact packaged-Mach-O
  handshake, while the performance runner repeats it on its private ZIP
  snapshot before the long session and bounds that session with a
  duration-plus-grace watchdog. Candidate attestation v3 proves that the
  attested tag's strict verifier enforced the current capability contract; all
  `macos-alpha-v2` records require v3 even while blocked. Compatible candidates
  then record genuine frame/RSS windows plus active-gameplay, focused-window,
  and cleared-stage evidence, publish with atomic no-replace output, and bind
  them to the tagged ZIP through a launch receipt. The runner hashes a no-follow
  private ZIP snapshot and extracts only that immutable input. A shared,
  I/O-free contract module validates exact raw-v2 keys, identity, timestamps,
  sample continuity, and state semantics in both the runner and final verifier;
  the runner also binds its receipt hash to the bytes it validated. Fixed Alpha
  performance thresholds, status cross-checks, and ordered human approvals stay
  in the final verifier and cannot be redefined after a run. Legacy v1 records
  remain readable only while blocked; `--allow-blocked` never grants
  publication approval.

Every migration change must keep `make clean`, `make debug`,
`make test-architecture`, and `make test` green. Visual
changes also require a manual one-player and two-player smoke test, including
pause, pickups, base destruction, stage completion, and the battle report.
