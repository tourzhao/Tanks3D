# Gameplay Invariants

These are characterization contracts for the current game. Incremental
refactoring must preserve them. An intentional gameplay change must update this
document and its production-path regression tests in the same pull request; a
mechanical file move must not update either.

## World and Movement

- The map is 26x26 tiles with 35 stages. Stage numbers wrap (`36 -> 1` and
  `0 -> 35`). Every stage contains 20 enemies, with at most four active enemy
  slots.
- Enemy spawns are `(1,1)`, `(13,1)`, and `(25,1)`; player spawns are `(9,25)`
  and `(17,25)`. Generated layouts must match their recorded hashes and keep all
  validated spawn/player/base routes open.
- Simulation `dt` is clamped to 0.05 seconds. Tanks move only north, south, west,
  or east. Ice momentum lasts 0.380 seconds; lane snapping is allowed within
  5/16 tile of the cardinal lane.
- A tank has a 0.875-tile half-extent. Tank centers overlap only when both axis
  separations are strictly below 1.75; exact edge contact is not collision.

Player progression is fixed as follows:

| Level | Move speed | Shell speed | Active shells | Power shell |
| --- | ---: | ---: | ---: | --- |
| 0 | 5.0 | 9.775 | 2 | No |
| 1 | 6.5 | 12.7075 | 2 | No |
| 2 | 6.5 | 12.7075 | 3 | No |
| 3 | 6.5 | 12.7075 | 4 | Yes |

Reload time is 0.120 seconds. An attempted shot resets this cooldown even when
the active-shell cap rejects it. Simulation shells spawn 0.625 tile in front of
the tank center; visual muzzle length must not change gameplay spawn position.

## Player Input

- The application samples one data-only `game/player_system.h`
  `PlayerInputFrame` per game update. `Game3D` does not query raylib keys. P1
  uses Arrow keys and Right Alt, Right Control, or Space to fire; P2 uses WASD
  and Left Alt, Left Control, or F.
- A newly pressed direction overrides the previously held direction. When
  several directions are newly pressed in one frame, the fixed write order is
  North, South, West, East, so East has final priority.
- While the selected direction remains held it is sticky. Once released, held
  fallback priority is North, South, West, East. Releasing every direction stops
  propulsion but preserves facing; existing ice momentum may continue.
- `planPlayerControl()` returns only final drive, propulsion, and held-fire
  intent. Legacy malformed input is intentional parity, not fail-closed
  validation: a pressed-but-not-held direction may change facing without
  propulsion; invalid directions `5` and `255` remain when no direction is held
  or pressed, and recover through the normal held fallback when one exists.
- `beginPlayerFrame()` owns the five creation, fire, dust, shield, and streak
  clocks without entities. A negative or non-finite elapsed time returns
  `Invalid` and leaves every clock unchanged.
- Every valid phase debits dust, shield, and streak. `Inactive` leaves creation
  and fire unchanged; `Creating` debits only creation; `Ready` debits only fire.
  Phase is chosen from the entry snapshot, so a creation clock that crosses zero
  still yields `Creating` for that update.
- Clock fields themselves are not validated. Their entry comparison and
  `std::max(0.0f, clock - elapsed)` argument order are parity contracts: NaN and
  negative infinity selected for debit clamp to zero, while positive infinity
  remains positive infinity.
- `advanceInactivePlayerDeath()` owns only scalar `deathTimer` and lives. A
  negative or non-finite elapsed time returns `Invalid` atomically. A
  non-positive entry timer returns `None` unchanged; a positive timer that
  reaches or crosses zero debits one life in that same update.
- Death-timer fields retain legacy comparisons and ordered
  `std::max(0.0f, deathTimer - elapsed)` behavior: NaN completes, positive
  infinity remains waiting for finite elapsed, and negative infinity returns
  `None` unchanged. Non-positive life counts saturate at zero without evaluating
  `INT_MIN - 1`; positive counts decrement once.
- `preparePlayerSpawnState()` owns exactly nineteen spawn fields: position, yaw,
  drive and travel directions, active/moving/Boat flags, creation/respawn/death/
  fire/dust/ice-slip clocks, ice state, shield, HP, level, direct-kill streak,
  and popup time. It does not own ID, nation, lives, maximum HP, score, or stage
  tally.
- Every valid plan writes the requested position, zero yaw, North drive/travel,
  `active = true`, and `moving = hasBoat = onIce = false`. Creation, fire, and
  shield receive the requested durations; respawn, death, dust, and ice-slip are
  zeroed.
- Unknown progression, non-finite coordinates, or negative/non-finite creation,
  fire, or shield durations returns `Invalid` without changing any field. Signed
  zero is valid. `Preserve` keeps level/streak/popup and any positive HP; only
  non-positive HP is replaced by maximum HP. `Reset` sets level/streak/popup to
  zero and copies maximum HP. Maximum HP is intentionally not validated or
  clamped, preserving zero, negative, extreme, and positive-over-maximum legacy
  states.
- The concrete adapter retains entity policy: ID `0` maps to `(9,25)` and every
  other signed ID maps to `(17,25)`. Initial start, restart, and respawn use
  `Reset`; stage entry uses `Preserve`, after which the stage caller separately
  clears popup time and resets the stage tally. `Game3D` owns the entity
  snapshot/writeback, slot-shell cleanup, event/audio ordering, and all other
  side effects.
- `advanceActivePlayerMovement()` receives only scalar position, cardinal, yaw,
  ice, time, speed, and dust-clock state plus one synchronous availability
  query. Negative/non-finite elapsed time or speed, a non-finite product, and an
  empty query reject atomically without a callback or state change. Zero and
  negative-zero distance retain the legacy query, accepted-move, and dust gates.
- Valid movement commits drive direction unconditionally and yaw only while
  propelling, then applies the existing ice transition. `None` and invalid
  directions `5`/`255` retain zero-vector movement parity. When lane correction
  is eligible, the exact snapped position is queried before the forward
  candidate. A rejected snap advances from the original position; an accepted
  snap remains committed even if the following forward candidate is blocked.
- Blocking stops movement, clears `iceSlipTimer`, restores movement direction
  to drive direction, emits no dust, and does not prevent same-frame firing.
  Otherwise ice carries the prior travel direction until expiry. Dust crosses
  the boundary only as detached position/velocity data; the caller presents it
  before setting the 0.120-second dust cooldown.
- Fire uses held state. Movement and turning run before firing, so a same-frame
  turn/fire uses the new direction and the position reached in that frame.
- `advancePlayerFireTransaction()` receives only scalar state. Static invalid
  configuration is atomic; a released trigger and a positive, NaN, or positive
  infinite cooldown are inert. A negative infinite cooldown remains due. Every
  valid due attempt resets the 0.120-second clock before the active-shell cap is
  tested, so a rejected full-slot shot still consumes the reload interval.
- Shell ownership is the player's vector slot/index, not a mutable entity ID.
  Active-shell counting, launch payloads, and respawn cleanup all use that slot.
  Level statistics clamp to levels 0–3. `None` and invalid directions `5`/`255`
  preserve legacy zero-vector launches rather than failing closed. Impacting
  shells and shells that will expire later in the same update still occupy a
  firing slot.
- An accepted launch crosses one synchronous detached intent. `Game3D` then
  inserts the Shell, appends `ShellFired`, spawns muzzle FX, commits camera
  shake, and requests `PlayerFired` audio in that order. Full-slot rejection is
  silent after the cooldown reset.
- The consumer snapshots and commits all five clocks before phase-specific work.
  `Inactive` snapshots scalar death state and commits its returned timer/lives
  before slot-owned shell cleanup, spawn reset, `PlayerRespawned`, and
  `PlayerRespawn` audio. Final-life shells are retained and delay Game Over;
  synthetic `id != slot` sentinels preserve entity identity in events while
  cleanup and defeat checks use the vector slot. `Creating` clears `moving`, and
  only `Ready` reads input. A ready frame derives ice from the current map cell,
  calls the scalar movement transaction with a live occupancy query and Boat
  permission, writes back movement, presents optional dust and commits its
  cooldown, and then calls the scalar fire transaction. Players are processed in
  vector order, so P2 collision sees P1's same-frame committed position; water
  blocks a player without Boat and permits the same move with it.
- Pause, stage intro, game over, settlement, high-score display, death, and the
  one-second player creation state ignore input; pressed edges are not buffered.

## Audio Output

- `audio/audio_output.h` is a raylib-free synchronous boundary containing only
  `play(AudioCue)`, `updateEngine(active, moving)`, and `stopAll()`. `Game3D`
  stores a nullable, non-owning `AudioOutput *`; normal process startup
  stack-owns the concrete `AudioBank`, sets up the device, and invokes resource
  load/unload. The bank retains cue priority, overlap, and engine-voice policy.
- Successful stage entry requests exactly one `StageStart` only after the stage,
  map, intro, players, and their creation state have committed. Engine output is
  inactive while either player is creating; once both are ready it reflects
  whether a ready player is moving.
- Entering pause calls `stopAll()` before requesting `Pause`. A paused update
  keeps the engine inactive. Resuming requests no cue; the next live update
  restores the appropriate engine state.
- Settlement begins by committing its report and optional `StageEnded` event
  before calling `stopAll()`. The event-free showcase retains the same final
  stop boundary without synthesizing a stage-end event.
- Recording-output tests characterize these method requests and their committed
  state without opening an audio device. They do not prove audible hardware
  playback or exercise the concrete `AudioBank`'s internal voice-priority
  arbitration.

## Projectile and Destruction Order

Each projectile micro-step is at most 0.12 tile and resolves in this order:

1. Move live shells.
2. Resolve government walls, terrain, steel, and map boundaries.
3. Resolve the vulnerable government core.
4. Resolve player-shell/enemy-tank or enemy-shell/player-tank hits.
5. Cancel surviving opposing shells.

Physical hits therefore take priority over shell cancellation. Only player and
enemy shells cancel one another; friendly player shells and two enemy shells do
not. A solid wall between two shells prevents cancellation.

The first PR 3.3 increment moves shell spawn, impact-state, overlap, and swept
cancellation helpers into the raylib-free `CombatSystem`. The second moves the
`StageMap::impactShell` query/mutation behind a pure-data `CombatOutcome`.
The third moves government-core detection and its binary state transition
behind the same target-tagged outcome boundary. The fourth evaluates
player-shell/enemy-tank overlap without mutation: it skips destroyed or
still-creating or empty-armor enemies, selects the first strict-AABB match in
vector order, and predicts one armor loss regardless of shell power. The fifth
validates that snapshot before any side effect, invokes the carrier-bonus
callback before armor mutation, then commits armor, 50-point attribution,
classified tally, streak, and fatal enemy state. Reusing a stale snapshot is a
no-op and cannot release another bonus. The sixth evaluates only enemy-owned
shells against players, skips inactive, creating, or zero-HP targets, and keeps
strict-AABB/vector-first selection. Its outcome snapshots player ID, HP, Shield,
and Boat state; commit revalidates them before applying Shield, then Boat, then
one HP of damage and the fatal transition. An unknown enemy owner ID still
causes physical damage because attribution does not determine collision.
Repeated/stale Boat, HP, and fatal commits are rejected. A Shield result changes
no player state, so production exactly-once behavior comes from moving the shell
to its impacting state. `BonusSystem` owns the carrier-release transaction;
`Game3D` supplies seeded-random/base-query adapters and concrete presentation.
For a player hit it preserves generic impact FX before commit, then emits the applicable
event/explosion, camera shake, and audio before the final shell-impact transition.
The seventh moves the once-per-frame lifetime decrement, fastest-shell
micro-step schedule, per-step start snapshots/movement, and stable expired-shell
cleanup into `CombatSystem`; physical-hit priority and event/presentation
consumption remain unchanged in `Game3D`.
The eighth moves surviving opposing-shell enumeration and cancellation commits
into `CombatSystem`. Each first shell takes its first successful later candidate
in vector order; rejected candidates do not block later ones, and an already
committed shell cannot cancel again. The ordered value results snapshot both
owners, IDs, midpoint, and distinct contact positions; `Game3D` consumes them in
the same micro-step after every physical hit and before the next movement step.
A start-snapshot count that does not match the shell vector is a safe no-op.
The ninth composes the map, core, enemy, and player operations into one typed
transaction per live shell. A map or core target is resolved immediately; a
tank target is resolved only when its validated commit succeeds. Carrier release
still runs before enemy armor mutation, and the generic player impact effect
still runs before Shield/Boat/HP mutation. The resolver never changes the shell:
`Game3D` consumes the result immediately and calls `beginShellImpact()` last
before processing the next shell. This preserves map/core/tank priority, RNG and
event ordering, and the later opposing-shell cancellation phase.
The tenth moves `GameEvent` into the pure game layer and projects physical
results without reading live entities. Brick events retain recorded scan order
even when steel is the final stopping kind; a damaged government wall follows
those brick records. Core and tank events require an actual health/armor/HP
transition and a validated commit. Unresolved results, steel, boundaries, an
already-destroyed core, Shield, and Boat produce no physical-damage event.
`Game3D` appends each projected batch at its former branch location: map/core
and nonfatal-enemy events precede their presentation; player damage events
follow generic pre-commit FX but precede hit-specific camera/audio; fatal enemy
audio still precedes `TankDestroyed`. Shell impact still begins last.
The eleventh projects every detached cancellation outcome to exactly one
`ShellCancelled` event. The event position remains the contact midpoint, player
and enemy IDs retain their separate owner domains regardless of pair order, and
cause, impact kind, power, and unrelated payload fields remain at their sentinel
defaults. `Game3D` consumes one outcome at a time as event, midpoint impact FX,
then `BulletHit` audio, so multiple pairs cannot reorder presentation. A
nonzero-time production scenario also locks the swept-crossing midpoint and
owner snapshot rather than relying only on initially overlapping shells.
The twelfth observes that cancellation-only presentation path after each real
action. One pair must append its full projected event, spawn a non-heavy impact
at `{x, 0.67, z}` with an upward normal, then request `BulletHit`. Two pairs must
remain `E-FX-A / E-FX-A`; events may not be batched ahead of their effects and
audio requests. Headless tests verify the request, not hardware playback, and
normal sessions retain no presentation trace.
The thirteenth observes only StageMap/GovernmentCore presentation. Player brick,
steel, boundary, and lethal-wall paths retain their event/semantic-FX/camera/
audio-request/final-impact order. Enemy wall and protected-wall hits remain
silent; a protected wall also emits no damage event. A live core appends its
event, spawns the explosion, requests `EagleDestroyed`, and commits; a second
same-frame dead-core hit commits alone. Tests require each semantic FX to add
real active effects and the breach camera pass to raise nearby shake, without
fixing particle counts.
The fourteenth observes tank-hit presentation through paired default-off hooks.
Enemy nonfatal order is event batch, armor FX, camera pass, `EnemyHit` request,
then shell commit; fatal order is explosion, camera pass, `EnemyDestroyed`
request, event batch, then commit. Player Shield order is generic pre-commit
armor FX then commit; Boat adds its 0.16 camera assignment before commit; damage
adds event, 0.22 camera, and `PlayerHit` before commit; destruction adds event,
explosion, 0.42 camera, and `PlayerDestroyed` before commit. Carrier release may
already have appended `BonusSpawned` before the enemy branch. Every pre-final
snapshot retains a flying shell at the same contact and exact incoming
velocity, the final snapshot alone starts the 0.200-second impact state, and a
later hit test against an already-destroyed enemy emits no tank presentation,
FX, or camera change.
The fifteenth expresses those same post-result sequences as an owned,
fixed-capacity app-layer command consumed immediately for one shell. Each typed
action carries its event batch, FX position/normal/heavy flag or explosion
color, radial origin/maximum/falloff or target player/value, cue, or final
contact. The player armor action remains inside the pre-commit callback. Camera
target actions are direct assignments: Boat must replace 0.60 with 0.16, and a
hit on eligible player 2 writes index 1 without changing player 1. Enemy radial
shake still uses `max`, so a larger value on the second camera is preserved.
Commands hold no entity or shell references and never persist across a shell or
frame.
The sixteenth moves that exact value contract into the raylib-free
`app/shell_tank_presentation` module and shares the unchanged cue ordinals from
`audio/audio_cue.h`. The pure factory still produces the same six branch
sequences; `Game3D` converts `Float3`/`Rgba8` payloads only at the concrete FX
edge and consumes each action immediately. Three independent suites add 18
checks for all payloads, steps, and malformed-input rejection while the
production-path observers continue to prove pre-commit timing and final shell
state.
The seventeenth gives cancellation presentation its own raylib-free fixed
command in `app/shell_cancellation_presentation`. It owns the complete projected
event, a non-heavy midpoint impact at `{x, 0.67, z}` with upward normal, and a
`BulletHit` request; every non-`ShellCancelled` event is rejected. `Game3D`
consumes those actions immediately as `E-FX-A` for each outcome. The optional
observer runs only after each real side effect and retains no normal-play queue
or trace. Every selected pair is already impacting before the first command is
consumed, so presentation must never call `beginShellImpact()` again.
The eighteenth gives StageMap/GovernmentCore presentation the same owned,
raylib-free command boundary in `app/shell_map_core_presentation`. Brick and
ordinary wall hits append any physical events before brick FX; player shells
then request the matching cue before final impact, while enemy map hits stay
silent. A lethal government wall additionally emits breach FX and a radial
camera pass before audio. Steel and boundary use upward surface FX; Steel may
retain a preceding one- or two-brick event batch. A live core remains
event-explosion-`EagleDestroyed`-impact, while a dead core produces only impact.
All actions own their payload and are observed only after the real side effect.
The factory rejects mismatched event batches, invalid attribution/coordinates,
non-deleting or partial-power brick changes, impossible 1/2-damage wall HP
transitions, and contradictory tank commits. A failed canonical projection
still commits the shell impact in release builds so physical damage cannot
repeat on the next micro-step.
The twenty-second routes all cancellation, map/core, tank, and Bonus command
leaves through one synchronous raylib-free `CommandSideEffectSink`. It must not
queue work: events, impact/brick/explosion FX, radial/assigned camera shake, and
audio complete before the command-specific observer runs. Final shell impact
and Shovel steel remain immediate domain commits in `Game3D`, and generic player
armor FX stays before HP/Boat/lifecycle mutation. Invalid audio is rejected with
no sink call; invalid tank-camera assignment is rejected for that action while
later command actions continue, whereas invalid Bonus-camera assignment remains
silently ignored.

Shells use a strict 0.5x0.5-tile AABB for cancellation. Edge-only contact is not
a hit. A shell hits a tank when both center-axis separations are strictly below
1.125 tiles. Impacting shells remain for 0.200 seconds and continue occupying the
owner's active-shell slot.

Brick tiles contain four quadrants. A first normal hit removes the impacted half;
a same-axis second hit clears the tile, while a perpendicular second hit may
leave one quarter. A third hit clears it. A shell crossing a cell seam may damage
two brick tiles. Power shells clear every intersecting brick, ordinary steel,
and foliage; foliage does not stop the power shell.

Accordingly, `CombatOutcome::stopsShell()` is authoritative. A
`CombatTarget::None` outcome can still accompany a map mutation when a power
shell clears forest and continues flying; `CombatTarget::GovernmentCore` stops
the shell even though its map-specific `ImpactKind` remains `None`.

## Player HP, Death, and Upgrades

- Maximum HP defaults to 3 and is configurable from 1 through 6. An effective
  enemy hit always removes one HP, independent of tank level.
- Hit priority is Helmet shield, then Boat consumption, then HP.
- At zero HP, the tank stops, the direct-kill streak resets, and the 0.490-second
  death animation begins. A life is deducted only when that animation ends.
- A respawn with lives remaining resets the vehicle to level 0, restores full HP,
  removes Boat and old-life shells, runs a 1.0-second creation animation, and
  grants 10 seconds of protection.
- Final-life shells remain until their own lifecycle ends. They may still score
  and enter classified K.O. totals, but cannot start a new-life streak.
- Direct-fire streaks have no timeout and increment only on direct enemy
  destruction. Their popup lasts 0.90 seconds. Grenade kills never increment the
  streak.
- Star advances one level up to 3; Gun sets level 3. A surviving player preserves
  level and HP across stages and gains one life, capped at 99. An eliminated
  player returns with two lives at level 0.

## Bonuses

- Each newly spawned enemy independently has a 12% carrier chance. Every valid
  player-shell hit on a carrier releases another pickup, including non-fatal
  hits.
- Carrier release snapshots Bandage eligibility once, draws the type once, then
  consumes complete x/z pixel pairs until the government-base predicate accepts
  one. Its exact callback transcript is `T-X-Z-Q-C` or
  `T-X-Z-Q-(X-Z-Q)*-C`; rejection never redraws the type. An invalid type or
  coordinate fails closed after its complete draw attempt, without a position
  query or commit. The concrete commit order is pickup insertion,
  `BonusSpawned`, then `BonusAppeared`, all before armor/score/death mutation.
  Release failure never cancels the otherwise valid carrier shell hit.
- With no eligible injured player, the eight classic types each have weight 1
  and Bandage has weight 0. When any active player with maximum HP above 1 is
  injured, Bandage has weight 2 while every classic type stays at weight 1.
- Bandage never appears in a 1-HP game, cannot be collected at full HP, and heals
  exactly one HP. Every collected pickup awards a base 300 points.
- Grenade adds 200 points per enemy not already being destroyed. Those points are
  bonus points and do not enter the four classified K.O. categories or streak.
- Helmet grants at least 10 seconds of protection. Clock freezes live enemies for
  at least 8 seconds. Tank adds one life up to 99. Boat enables water travel and
  absorbs the first otherwise effective hit.
- Shovel repairs all five government wall segments and grants 20 seconds of
  invulnerable steel. Re-collection resets the duration; the last 3 seconds flash
  on a 0.18-second period.
- Pickups last 12.5 seconds and blink twice as fast during the final 25%. Pickup
  overlap is strict below 1.875 tiles on both axes. Spawn centers use 1/16-tile
  precision from 1.0 through 24.9375 and exclude only the government core area.
- The raylib-free `BonusSystem` owns `Pickup{BonusType, XZ, age, life}`. Each
  carrier release owns its eligibility snapshot, draw/retry policy, validation,
  and detached synchronous commit intent. Each active update advances age/life
  before expiry and collection, then selects the first eligible player in vector
  order. It owns common/Grenade score and tally credit plus every player/enemy
  state mutation, returning ordered detached commands for Shovel and Grenade
  map/event/audio/FX/camera consumers.
- Each pickup executes one synchronous `U-B-A-F` transaction: update, begin with
  a post-update detached intent, apply rule state, then finish concrete
  consumption. `Game3D` preserves `E0-A-C-M-O-Ep-D`: append `BonusCollected`
  with zero points, commit application state, consume commands, set the message,
  request audio, patch the actual score delta, then erase the pickup. Pickups are
  processed in vector order; a prior pickup's HP/enemy mutations immediately
  affect the next pickup's eligibility or result.
- The thirty-fourth increment makes that outer vector order a `BonusSystem`
  contract. `advanceBonusPickups()` erases collected and discarded pickups at
  their current index and processes the shifted successor immediately. Discard
  is silent, application rejection retains the pickup and continues, and the
  optional collection-removal callback runs only after erasure with detached
  values. Missing begin/finish callbacks reject the whole batch before clocks,
  containers, players, or enemies change. A carrier pickup released during
  shell resolution is therefore aged and may be collected later in the same
  nonzero-time frame.
- Invalid or negative elapsed time is a no-op. Non-finite pickup state is
  discarded on the next valid update. Bonus score/tally, Grenade target totals,
  and Tank lives saturate instead of overflowing; malformed live enemies with
  non-positive armor are not Grenade targets.
- `BonusType::Count` is an internal sentinel, not a pickup. Sentinel or
  out-of-range values are never collectible and are discarded without an event,
  score, tally, message, or effect.
- Pause, stage intro, settlement, and the game-over hold freeze pickup and Shovel
  clocks. The five-second stage-clear buffer does not.

## Government Base and Enemy Creation

- P1's nation selects the shared national base. The three visual themes use the
  same physical five-segment structure.
- Each wall segment starts with 4 HP. Normal shells deal 1; level-3 power shells
  deal 2. A corner hit damages only the nearest segment. Steel protection blocks
  both shell types.
- The core has a 0.92-tile radius and binary 1 HP, but is vulnerable only
  through a real wall breach. Player and enemy shells, ordinary or power,
  destroy it in one hit. The destroyed core continues absorbing shells without
  repeating damage events. Destroying it loses the game.
- Stage intro lasts 3.2 seconds and freezes players, enemies, projectiles,
  pickups, protection, and spawn clocks.
- After intro, the default first spawn cooldown is 0.5 seconds. Enemy creation is
  a fixed 1.0-second, ten-frame warning with no physical tank, movement, firing,
  or damage.
- Enemy frame entry is phase ordered. Destroyed enemies advance only the death
  timer. Creating enemies stop moving and advance creation plus Clock/frozen
  time; the developer showcase may hold only creation time. Frozen enemies stop
  moving, advance dust/frozen/ice timers, and pause fire and steering. Only an
  Active result continues through target, steering, movement, firing, and
  presentation.
- Spawn attempts rotate through the three points. An exhausted queue or exhausted
  nonnegative `int` ID space returns before timer debit and without callbacks;
  `INT_MAX - 1` is the last safely reservable ID, and `nextEnemyId == INT_MAX`
  terminates. Otherwise the timer is debited before the four-enemy capacity and
  strict-positive cooldown gates. Capacity-full and cooling-down attempts perform
  no availability, RNG, or insertion callbacks, and an exact zero timer proceeds.
  A due attempt queries from the current rotation only until the first open point.
  If all three are blocked, the default retry is 0.15 seconds after the `Q-Q-Q`
  transcript, with no RNG or insertion and no change to the next spawn, enemy ID,
  or remaining count.
- An accepted spawn reserves its enemy ID before random callbacks. A regular
  enemy completes `Q...-R-I-R-R-C`; Armor omits the integer draw and completes
  `Q...-R-R-R-C`. The synchronous insertion callback observes the reserved ID
  but the old remaining count, rotation, and debited timer; those three fields
  commit afterward, with the timer set to the normal interval.
- Basic (type 0) and Armor (type 3) enemies may replace the base with the first
  strictly closer active player by Manhattan distance; equal candidates keep the
  earlier base/player target. Fast and Power enemies remain base-focused.
- Active steering makes an ordinary decision only when `directionTimer` is
  strictly greater than its interval; `blockedTimer >= 0.30` forces a decision.
  A due decision with any positive blocked time first attempts the ordered local
  escape search. Success atomically commits its aligned position, drive/travel
  direction, yaw, and zero ice-slip state, clears the blocked timer, and chooses
  the next interval as `0.55 + R * 0.18`. Failure retries at `0.1` seconds.
  No-decision and failed-escape paths consume no randomness.
- An ordinary steering decision chooses its next interval as `0.1 + R * 0.8`.
  Basic enemies pursue for a branch roll strictly below `0.8`; every other type
  uses a strict `0.5` threshold. Pursuit then uses an `R-R-R` transcript and
  chooses the larger absolute target axis (vertical wins a tie) for an axis roll
  strictly below `0.7`; wandering uses `R-R-I`, with integer values 0–3 mapping
  to North, East, South, and West.
- Armor enemies fire when blocked or when their movement direction points
  forward within a strict two-tile lateral lane. Every due attempt consumes its
  reload draw (`R`); an accepted aim then queries the owned-shell slot (`Q`), and
  an open slot invokes the launch callback (`C`). Thus rejected, occupied, and
  fired paths are exactly `R`, `R-Q`, and `R-Q-C`. An enemy-owned shell with the
  same source ID occupies the slot even while impacting with zero life;
  player-owned or other-enemy shells do not. Launch uses the drive direction,
  while Armor aim continues to use the post-movement direction. The complete
  detached shell reaches the synchronous callback before the reload cooldown is
  committed; concrete shell insertion, event append, and muzzle effect remain
  in `Game3D` and request no enemy-fire audio.
- Basic enemy speed is 4.0 and Fast speed is 5.2. Speed, fire rate, and spawn rate
  settings range from -30% through +30% in 5% steps. Movement multiplies by
  `1 + p/100`; fire/spawn intervals divide by that value. The one-second warning
  never changes.
- Active movement resolves ice carry before collision. A changed travel
  direction first attempts a collision-safe lane snap; the collision probe is
  `speed * dt + 1/16` tile while the committed displacement remains
  `speed * dt`. A successful move clears the blocked timer. A blocked move
  cancels motion and ice slip, restores travel to the drive direction, and caps
  the blocked timer at 0.8 seconds. Track dust is requested only after an
  accepted move when its cooldown is due; `Game3D` consumes that detached
  request and commits the 0.19-second cooldown before any due fire draw.
- Destroyed enemies keep their slot through the 0.490-second animation and until
  every shell owned by that enemy has ended.

## Stage and Settlement Timing

Normal battle update order is:

```text
players -> enemies -> shells -> pickups -> enemy spawn
        -> game-over/stage-clear detection
```

- The stage clears only after the final enemy animation and its shells finish.
- The following 5.0-second buffer still updates players, shells, pickups, and
  base protection. Player or base destruction during it converts the result to
  game over.
- Game over freezes combat for 3.1 seconds before settlement.
- Settlement advances every 0.1 seconds. Score grows in
  `+1/+10/+100/+1000/+10000/+100000` bands; each player's displayed K.O. count
  advances by one in enemy-category order.
- `planSettlementBegin()` validates the current stage and exact one- or two-player
  scope, rejects a cleared report when the base is already destroyed, and
  classifies `Cleared`, `BaseDestroyed`, or `PlayersDefeated`. Its score/tally
  snapshot is detached from live players. `Game3D` consumes it as `PlanReady` ->
  `ReportCommitted` -> optional `StageEndEventAppended` ->
  `AudioStopBoundaryPassed`; the event-free showcase omits only the event step.
- The raylib-free `SettlementState` owns the participant/tally snapshot,
  displayed score and K.O. progress, confirmation, and the 5.0-second Idle
  timeout. Its update returns the number of count steps plus a
  `SettlementCompletion`. The same raylib-free module turns a non-`None`
  completion into a detached `SettlementTransitionPlan`; neither path emits
  events, requests audio, loads a stage, navigates, or mutates live players.
  `Game3D` requests `ScoreCounted` once per returned count step after the report
  state commit and performs the concrete completion consumers. Confirmation
  reveals skipped totals without synthesizing count cues.
- Invalid settlement player counts fail closed to an inactive reset. Non-finite
  or non-positive elapsed time does not change report state, and score counting
  saturates at `INT_MAX` instead of overflowing. Completion clears the active
  phase and timers but retains the immutable report snapshot until the next
  successful stage reset.
- Direct shell hits award 50 points, including the destroying hit. Only direct
  destruction appears in classified K.O. rows. Grenade clears do not.
- Completed settlement automatically continues after a 5.0-second Idle hold
  unless confirmed earlier. Confirmation while Counting reveals every total;
  confirmation while Idle continues. Initial high score is 2000. A strictly
  higher game-over score displays the high-score view for 5.2 seconds.
- Successful completion advances and wraps the stage, gives each survivor one
  life up to 99, and restores an eliminated player to two lives at level 0. A
  non-record defeat requests the menu immediately after settlement. A record is
  the maximum participating-player score and requests the menu only after
  confirmation or the 5.2-second record-screen timeout.
- `SettlementTransitionPlan` performs the strict record comparison and changes
  only lives/level in its copied player vector. A default-off synchronous
  observer locks the concrete order. New records use `PlanReady` →
  `HighScoreCommitted` → `HighScoreAudioRequested` →
  `HighScoreDisplayCommitted`; non-record defeats use `PlanReady` →
  `HighScoreCommitted` → `MenuRequested`; successful stages use `PlanReady` →
  `StageCandidateRequested` → `StageCandidatePrepared` →
  `PlayerProgressionCommitted` → `StageLoadCommitted`; rejected candidates use
  `PlanReady` → `StageCandidateRequested` → `StageCandidateRejected` →
  `MenuRequested`. `None` produces no plan observation.
- Automatic progression validates a candidate `StageMap` before committing it.
  The candidate must report the exact requested stage. A failed or mismatched
  next-stage load preserves the prior stage and complete player state, preserves
  the load error, and produces one return-to-menu notification.
  Consuming that notification does not resume simulation: `awaitingMenu` remains
  a persistent frozen terminal state until a later successful stage load resets
  it.
- `start()` and therefore `restart()` are atomic across stage-load failure: the
  prior events, player count, starting lives, advanced settings, nations, stage,
  and full player vector are restored rather than mixing an old world with new
  session configuration.

## Observable Rule Events

- `eventsThisUpdate()` is an ordered, read-only observation of the most recent
  simulation update. It is cleared before every update, including paused and
  early-return states, by successful start/restart, and by every manual stage
  change attempt. A rejected start/restart restores the prior event snapshot.
  Consumers must copy events they need before the next update.
- The ten event types are `ShellFired`, `ShellCancelled`, `BrickHit`,
  `TankDamaged`, `TankDestroyed`, `PlayerRespawned`, `BonusSpawned`,
  `BonusCollected`, `BaseDamaged`, and `StageEnded`. Payloads contain values and
  IDs only—never entity pointers, renderer objects, audio handles, or effects.
  Sentinel fields are ignored; `direction` currently applies only to
  `ShellFired`. QA showcase setup does not emit simulation-update events.
- `ShellFired` requires an accepted projectile; a shell-cap rejection emits
  nothing. One `ShellCancelled` represents one surviving player/enemy shell pair;
  its attribution comes from the cancellation result snapshot, not a later read
  of mutable shell storage.
  Physical brick, base, or tank impacts are recorded before cancellation is
  considered. Each changed brick cell produces its own row-major `BrickHit`.
- `TankDamaged` represents a nonfatal armor/HP reduction; the fatal reduction is
  represented by `TankDestroyed`. Direct hits have `PlayerShell` cause and 50
  points after score/tally state is committed. Grenade destruction has a distinct
  cause, awards 200 points per target, and never enters classified K.O. or streak
  state. Shield, Boat, full-HP Bandage, and steel-blocked wall hits emit no damage
  or collection event.
- Carrier hits emit `BonusSpawned` before their tank result. Collection emits
  `BonusCollected` before any Grenade destruction events, with the final score
  delta in its payload. `PlayerRespawned` is recorded only after life, level, HP,
  position, shield, and creation state have been initialized.
- `BaseDamaged` distinguishes a wall segment from the core and records before/
  after health. `StageEnded` is emitted once when the final settlement begins—not
  when the five-second clear buffer starts—with `Cleared`, `BaseDestroyed`, or
  `PlayersDefeated` reason. If base and final-player loss coincide, base loss has
  reason priority.

## Refactoring Verification

### Random-stream lifetime

- Normal play seeds `Game3D` exactly once from `std::random_device`; tests may
  construct it with an explicit `std::uint32_t` seed.
- `start()`, restart, manual stage changes, and automatic stage progression do
  not reseed. Identical seeds are reproducible only when the same calls, frame
  steps, and inputs consume the shared gameplay stream in the same order.
  The stored seed identifies the stream at `Game3D` construction, not the
  already-advanced state after a restart.
- Enemy AI, enemy metadata, bonus-carrier rolls, and pickup type/position share
  that stream. Stage layout hashes, procedural environment variation, particle
  randomness, camera time, and audio are deliberately outside it.
- After an open point is accepted and its ID is reserved, enemy creation consumes
  a `[0,1)` type roll, consumes a `[0,2]` integer only for a regular tank, then
  consumes carrier and armor `[0,1)` rolls. Armor-tank chance is
  `0.00735 * stage + 0.09265`; carrier chance is 0.12. Both comparisons are
  strict `<`, as are the three stage-dependent armor thresholds.
- Pickup creation consumes its type first (`[0,7]`, or `[0,9]` when Bandage is
  eligible), then x/z pixel draws in complete pairs. Slots 8 and 9 are Bandage.
  A base-overlapping pair is discarded before the next pair is consumed.
- `SessionDigest` schema `Tanks3D-session-v1` serializes gameplay settings,
  modes/timers (including `awaitingMenu`), settlement state, the complete mutable
  map, entities, pickups, and mt19937 state field by field. The XZ migration keeps
  the v1 pickup-height field as the historical `0.0` value. A fixed prefix locks
  spawn state as remaining count, rotation index, next ID, then timer. The digest
  excludes the event buffer, camera, particle and `BattleFx` state, audio
  playback, and wall-clock animation. It is a replay comparison aid within the
  same build, not a raw-memory hash, cross-version compatibility guarantee, or
  cross-schema golden file.
- The deterministic gate compares both each frame's event count and the complete
  ordered event sequence across two equal-seed, 480-frame runs at 1/60 second,
  so moving an otherwise identical event to another update fails the replay.

Run the full current gate with:

```sh
make clean
make debug
make test-architecture
make test
make test-unit test-session test-assets
make test-sanitize
make coverage
./build/Tanks3D --dump-stage-signatures
```

The integrated self-test reports seventeen named suites and 7,181 runtime checks.
Ten independent, raylib-free core/game executables add 154 suites and
1,107 checks; seven app-layer executables add fifty suites and 292 checks, so
the aggregate gate runs 221 suites and 8,580 checks across eighteen profiles.
They lock vector arithmetic, all cardinal values, targeting ties, strict 5/16 lane
snapping, edge-only AABB contact, 0.380-second ice transitions, HP 1–6, every
tuning step, nation cycling, four player levels, bonus enum/weighting,
Shield/Boat priority, classified tally exclusion, death transitions,
enemy-shell cleanup, all 35 direct generator goldens and their map bridge,
stage wrapping, open spawn routes, strict map/tank/shell collision, brick
quadrants, five base-wall segments, Shovel steel timing, exact shell spawn and
impact state, frame aging/scheduling/movement/stable cleanup, cancellation
eligibility, swept crossings, wall-separated shells,
greedy multi-pair ordering, invalid-candidate continuation, attribution/contact
snapshots, and mismatched-start safety, plus complete map/core outcome snapshots,
stopping decisions, inert-shell guards, ordered map/core/tank transactions,
deferred shell impact, the
non-stopping power-shell forest mutation, binary core damage, dead-core
absorption, strict core edges, wall-before-core priority, player-only enemy
targeting, creation/death filtering, vector-first target selection, deferred
armor commit, carrier callback order, stale/repeated commit rejection, invalid
and posthumous owner attribution, classified direct-hit state, and
map/core-before-tank priority. Enemy-shell/player checks additionally lock owner
filtering, player eligibility, strict/vector-first targeting, Shield/Boat/HP
snapshot validation, unknown-source damage, fatal lifecycle, repeated commit
rejection, wall-before-player priority, production event/camera/shell order, and
power-forest pass-through into a same-position enemy hit. Physical-event
projection checks lock every payload field, double-brick order, wall/core
damage, committed enemy/player damage and destruction, shell attribution,
bounded malformed snapshots, and all no-event outcomes. Cancellation-event
projection checks lock midpoint, both owner orderings, both source-ID domains,
and every unrelated sentinel field. Standalone cancellation-command checks lock
the owned event, exact FX/cue constants, and all invalid-event rejection.
Standalone map/core-command checks lock all eight branch sequences, eight action
kinds, exact FX/camera/audio/contact payloads, and grouped malformed snapshots.
Production presentation checks additionally lock cancellation payload,
requested cue, no-repeat behavior, whole-batch impact state, and per-pair
interleaving, plus map/core event-batch, exact semantic-FX, camera-pass,
audio-request, and final-shell-impact ordering for brick, wall, protected
steel, boundary, breached-wall, live-core, and dead-core outcomes. Paired tank
presentation checks lock the six exact enemy/player branch orders, generic
player FX before commit, every typed command payload, real FX growth, requested
cues, direct camera assignments and two-player routing, deferred shell
finalization, and silence for an already-destroyed target.
Fourteen standalone SettlementSystem suites add 89 checks for entry-plan reason
classification, exact detached snapshots, event intent, stage/player/vector
validation, atomic contradiction rejection, score bands, classified order,
catch-up cue counts, confirmation, timeout/completion, invalid player/time
inputs, `INT_MAX` saturation, snapshot retention, no-op/invalid transition input,
strict high-score selection, stage wrap, eliminated-player recovery, and the
99-life cap. The implementation reaches 100% coverage in all four metrics.
Integrated settlement checks additionally lock both completion controls,
detached entry and transition decisions, the event-free showcase, all three
stage-end reasons, begin order and append-at-end behavior, all four completion
presentation orders, post-commit `ScoreCounted` request state, confirm silence,
complete stage-entry player state, `StageStart` request-time state, strict record
selection, rejected and wrong-stage candidates, atomic restart failure, manual
stage-load rollback, different-configuration failed-start rollback, one-shot
menu notification with persistent freeze, and the 5.0/5.2-second boundaries.
Pickup checks run every valid effect through collection,
including cap/max behavior and invalid-sentinel removal without reward. Thirty-three
standalone BonusSystem suites add 163 checks for XZ/default state, weighted and
pixel spawn decisions, carrier-release draw/retry/failure transcripts,
per-pickup and stable-vector callback/outcome ordering,
lifetime/strict-AABB/eligibility
boundaries, invalid input, all nine effects,
ordered Grenade commands, corrupt armor filtering, integer saturation, atomic
missing-callback rejection, adjacent erasure, rejected retention, and
post-erasure detached callbacks; the implementation reaches 100% coverage in
all four metrics.
Forty-three standalone EnemySystem suites add 120 checks for lifecycle clocks,
targeting, pursuit, escape, firing, spawn construction and transactions, active
movement, and active steering. Spawn checks lock invalid-input atomicity,
queue/identifier/timer/capacity boundaries, exact availability/random/commit
transcripts, the omitted regular-type draw for Armor, and complete state/Enemy
commits.
Steering checks lock strict timer and blocked thresholds, zero-draw paths,
callback/state ordering, conditional `R-R-R`/`R-R-I` transcripts, all four
direction mappings, and complete escape success/failure commits. Movement checks
retain zero-query stationary behavior, snap-before-probe ordering, ice carry,
blocked-state commits, and detached dust payloads. The implementation's 283
regions, 23 functions, 441 lines, and 238 branches reached 100% coverage after
the spawn increment. The firing increment expands that to 314 regions, 24
functions, 490 lines, and 262 branches, still all at 100%. Its direct tests lock
invalid/cooling/aim-rejected/occupied/fired paths, exact callback transcripts,
both shell speeds, Armor aim/launch direction separation, detached payloads, and
callback-before-cooldown timing. Real `Game3D` cases additionally lock +30% fire
tuning, ice carry, impacting zero-life occupancy, distractor shells,
shell-event-muzzle order, and no enemy-fire audio request. `Game3D` supplies live
occupancy and seeded-random adapters and retains the synchronous concrete
shell/event/muzzle presentation. PR 3.4 is complete; PR 3.5 bonus release,
per-pickup and vector collection/application, and pure settlement entry and
transition planning are now transactional or detached. PR 3.5 remains partial.
The headless audio-output checks additionally lock stage-entry, engine, and
pause/resume calls at the injected raylib-free boundary; the settlement suite
locks its final stop request there as well. Neither claims hardware or concrete
voice-policy coverage. Thirty-one standalone PlayerSystem suites contain 64 checks;
independent literal tables exercise 2,560 input combinations (not 2,560 checks)
plus atomic invalid frame/death elapsed, signed-zero and entry-snapshot phase
policy, death entry/cross-zero/malformed timer behavior, `INT_MIN` life
saturation, ordered malformed-clock clamps, invalid `5`/`255`
control/movement/launch parity,
atomic invalid and overflowing movement, exact snap/forward query transcripts,
ice/block states, detached dust, request/cooldown gates, reset-before-cap
behavior, explicit reload/spawn scalars, slot-owned payloads, level-derived
speed, cap, and power, plus complete Preserve/Reset spawn write masks,
invalid-input atomicity, signed-zero and maximum-finite acceptance, and
malformed-HP parity. The
scripted-player-input suite locks clock, death, and spawn commits, creation
cross-zero without input fallthrough, ready-only input,
post-movement spawn position, ice travel versus launch direction, full-slot
silence, `id != slot` respawn cleanup and final-shell delay, ordered two-player
movement, Boat/water collision, blocked movement with same-frame fire, and
concrete presentation. The observable-event suite has 76 checks and locks the
post-commit respawn cue order. Direct and
production regressions plus ASan/UBSan pass all 17 integrated suites and 7,181
checks. Owned-shell cleanup, spawn-point mapping and entity adaptation, events,
audio, entities, map, rendering, cameras, and concrete movement/fire
orchestration and presentation adapters remain in `Game3D`.
Integrated metadata checks directly cover all 12 national vehicle IDs.
Stage signatures use a versioned, row-major FNV-1a byte stream over each tile
and initial brick mask;
they do not use `std::hash` or raw object memory. Do not replace expected hashes
merely to make a failing refactor pass. First explain and review the intended
layout change.

Visual proportions, shader colors, and particle counts are outside this gameplay
ledger. Preserve them with the showcase and manual visual QA process described in
[`REFACTORING_PLAN.md`](REFACTORING_PLAN.md).
