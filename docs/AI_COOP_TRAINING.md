# Training a cooperative Player 2

Status: the versioned native adapter, shared-policy PPO, mixed-partner training,
partner evaluation, optional tactical rules and native two-AI viewer are implemented.
Experiments are
recorded in `build/release-evidence/ai-coop-20260914-round1/` and
`build/release-evidence/ai-coop-20260914-round2/`; rule development is recorded in
`build/release-evidence/rules-20260914-round1/`. Existing single-player evidence
remains unchanged. The ordinary app now offers **PLAYERS → AI AS P2** using
the selected tactical rules in native C++; P1 keeps normal human controls.
See [local teammate development](DEVELOPMENT.md#local-ai-teammate). Neural
policy integration and acceptance with actual human partners remain pending.

The product target is an optional AI teammate for a human controlling P1.
Training two AI tanks with shared policy parameters is the first stage. The
acceptance target is cooperation with unfamiliar partners, including humans.

The first three-seed pilot completed 786,432 world decisions / 759 team episodes.
The validation-frozen seed-14051 endpoint had mixed reserved Stage 1 results and
regressed on the exploratory 35-map probe (43/70 clears versus 55/70 for the
converted initial actor; base survival 51/70 versus 64/70). Keep these new weights
experimental and retain the initial actor. See [the experiment results](AI_TRAINING.md)
and the local round-one report for partner-specific outcomes and limitations.

Round two completed 1,572,864 world decisions and 1,074 team episodes using all
35 maps and mixed partners. All three new endpoints failed the shared-policy
all-map base-survival gate, so the retained initial actor remains selected.
The curriculum is implemented, but it has not established improved cooperation.
See the round-two section in [experiment results](AI_TRAINING.md) and its local
report for the 3,720 actual evaluation episodes and exact regression replay.

## Implemented interfaces

`src/training/coop.inl` adds the independent `t3coop_*` ABI, version 1, by
composing the existing native environment. `training/coop_env.py` exposes one
`MultiDiscrete([10, 10])` joint action. Its observation is a dictionary with
`map: float32[2,15,26,26]` and `state: float32[2,384]`. The team critic reads
the canonical slot-zero view from this dictionary; there is no separate,
duplicated team-state return value. Each actor receives 10,524 floats.

The original fourteen map channels retain their self-relative meanings;
channel 14 is ally occupancy. State offsets are half-open ranges:

| State range | Contents |
| --- | --- |
| 0:256 | Existing single-player layout, viewed from this actor's slot |
| 256:272 | Ally's sixteen original self fields |
| 272:274 | Relative ally x/z position divided by 26 |
| 274:276 | Native slot identifier (0 or 1), two-player flag |
| 276:300 | Ownership of the same 24 sorted projectile slots: self +1, ally -1, enemy/padding 0 |
| 300:332 | Four newest-first decision intervals; self then ally, each with dx/2, dz/2, executed direction/4, fire bit |
| 332:336 | Self ready, ally ready, self eliminated, ally eliminated |
| 336:340 | Self/ally death timers, then self/ally scores divided by 10,000 |
| 340:384 | Reserved zero |

Enemy/padding projectile slots remain distinguishable through the original
projectile fields. History never includes a current, unexecuted proposal;
displacement across death/spawn transitions is zeroed. Availability means an
active tank with positive HP and no creation delay, not simply remaining lives.
The information dictionary retains the original fourteen team-level metrics and
adds kills, deaths, shots, pickups, own-base hits, score, lives, active, ready and
eliminated counters for each player.

`training/coop_policy.py` copies the retained actor's compatible weights and
adds a zero-initialized cooperative feature residual. The resulting actor has
227,282 parameters (909,128 FP32 bytes). A separate freshly initialized team
critic and two optimizers are used only in `training/coop_train.py`. Loading
the old SB3 checkpoint resets its global sampling seed; initialization explicitly
restores the requested new training seed afterward, with a regression check.
The actor-only `.pt` checkpoint uses format `tanks3d-coop-v1` and is distinct
from the old single-player PPO `.zip` format.

`training/coop_control.py` batches the current actor when both slots share it,
or evaluates only P2 alongside a fixed P1. Evaluation gives each slot its own
seeded sampling generator, so changing P1 does not consume P2's random stream.
`training/coop_evaluate.py` supports shared, frozen-initial, fixed-defender and
tactical-rule partners. `training/coop_play.py` records/replays the real native
renderer and checks that rendering never mutates the gameplay digest.

### Optional observation-only tactical rules

`training/rule_policy.py` adds `TacticalDefender` version 1. It uses the existing
self-relative map/state observation, including ready ally position, without a
native-world handle, future enemy decisions or game RNG. No network is evaluated
for a rule-controlled slot. Each tank owns its route and recent movement state.
The historical `Defender`, `Navigator` and HQ fire guard remain unchanged.

The new policy reuses Navigator's one-tile navigation and conservative firing
geometry, selects reachable firing positions, stops to fire in aligned lanes,
prioritizes enemies approaching home, and assigns the nearer tank to home coverage
with a consistent slot tie break. Paths avoid the ready ally's footprint and
impassable terrain; brick cover can be cleared. Periodic, role-change, respawn and
stalled-movement replanning update the route. These are approximate decision rules,
not a copy of native physics. Active projectile evasion was tested as version 2
and rejected by worse development results; it is not in the selected policy.

Both evaluation and the viewer accept `--rule-p2 tactical` instead of `--model`.
`--partner self` uses separate instances of the same rules for both players;
`--partner defender` keeps the historical P1, and `--partner initial --initial FILE`
uses a frozen neural P1. `--partner tactical` also allows neural P2 evaluation with
the new rule P1. Historical defaults, `--defender-p2`, training partner mixes and
trained weights retain their existing behavior; the new rules are opt-in.

```sh
make ai-setup ai-native
caffeinate -dimsu env PYTHONDONTWRITEBYTECODE=1 build/ai-venv/bin/python -m training.coop_play \
  --rule-p2 tactical --partner self --seed 3000004 --stage 5 --seconds 120 \
  --trace build/ai-runs/tactical-watch.json
```

Use a fresh trace path on repeat. Add `--hidden --record build/ai-runs/tactical.mp4`
to record the actual native renderer. The generic controller/viewer imports Torch
for optional neural partners, although pure rule decisions do not run a network;
the standalone rule benchmark loads neither Torch nor a model. See the rule section
in [experiment results](AI_TRAINING.md) for frozen development selection, reserved
partner comparisons, costs and failures. Human controls and ordinary-app teammate
selection remain separate, unimplemented work.

### Actor transfer and training

The CLI can start a fresh transferred or scratch run. `--parent` imports the
old single-player PPO archive; `--coop-parent` loads compatible actor-only `.pt`
weights without clearing already learned ally features. Both initialize a fresh
team critic and optimizers. It saves actor weights, critic, optimizers and RNG
state; it does not yet expose optimizer resume, and live native worlds are not
serialized. Do not describe restarting from a saved actor as exact continuation
of an unfinished rollout.

```sh
make ai-setup ai-native
caffeinate -dimsu env PYTHONDONTWRITEBYTECODE=1 build/ai-venv/bin/python -m training.coop_train \
  --parent build/release-evidence/ai-20260913-round8/ai-player.zip \
  --seed 14001 --world-steps 262144 --stages 1 \
  --output build/ai-runs/coop-new-seed14001
```

Use a fresh output directory. Omit both parent options for a scratch initialization;
the first experiment does not include a budget-matched scratch comparison.
The remaining sections describe the implemented training contract and the
remaining human curriculum, not evidence that all acceptance phases have passed.

### Mixed partners and balanced map coverage

`training/coop_partners.py` supplies episode-fixed partners and a shuffled stage
deck. Enable `--balanced-stages --stages 1 2 ... 35` with all desired stage
numbers written explicitly (the ellipsis is explanatory, not a CLI argument).
Every stage is assigned once before that deck repeats; longer games still
occupy more world decisions. Stage/partner selection uses local generators
separate from native gameplay and current-actor sampling.

`--partner-mix .5 .3 .2` selects shared-current, frozen-historical and fixed
Defender partners with those probabilities per new episode. Supply historical
actor files with `--historical FILE1.pt FILE2.pt ...`; these are sampled
uniformly within the historical branch. A non-shared episode randomly assigns
the learning actor to P1 or P2 without swapping spawns or native update order.
Partner weights stay frozen, their action RNG does not consume the current
actor's stream, and no partner identity is added to either actor observation.
The current actor is evaluated only for its learning slots. Historical and
scripted actions are excluded from PPO/entropy loss and keep separate counters.

`--critic-warmup-steps N` first collects genuine native rollouts while updating
only the team critic. Actor weights and the actor optimizer remain untouched
through the warmup boundary. `N` must be a multiple of the world count, and it
is included in the overall world-decision budget. The team reward and final
state bootstrap convention are unchanged. Default flags retain the original
self-play behavior; an actual before/after CLI replay verified identical actor,
critic, optimizer and RNG state when the new options are disabled.

Each run records started episodes (map, native seed, partner checkpoint hash and
learning slots), completed team outcomes, world decisions per partner kind,
historical/scripted actions and policy-update counts. `actor_proposals` counts
current-actor samples, including unavailable slots and warmup, but excludes
frozen actors. It equals world decisions plus shared-current world decisions;
it is no longer necessarily twice the number of world transitions.
`ready_learner_samples` includes warmup, while `valid_actor_samples` counts ready
learning transitions collected after warmup for actor updates. It does not count
repeated PPO epochs or how many minibatches execute before a KL stop. Realized
episode and time proportions can differ from the requested probabilities.

Round two starts from the retained converted initial actor, uses three seeds
with 524,288 world decisions each (32,768 critic-only warmup), covers all 35
maps, and lowers the actor learning rate to 1e-5. The historical pool includes
the initial actor and round-one seeds 14001/14101; seed 14051 is reserved as an
unseen partner for final evaluation. This combines curriculum and stabilization
changes, so it is not an ablation establishing which component caused a gain.
Rewards, runtime actor, native physics and HQ fire guard remain unchanged.

## Preserved game constraints

- The existing `t3ai_*` path still starts `Game3D` with one player. Its
  action handling and damage, destruction, pickup and own-HQ accounting retain
  their original behavior. Observation extraction accepts an explicit player
  slot for the new adapter while defaulting to slot zero for the original ABI.
- `training/env.py` still exposes one `Discrete(10)` action and a v1 observation:
  14 map channels plus 256 state floats. The policy and fixed defender contain
  explicit dimensions/offsets. Changing only the player count is insufficient.
- Production already accepts both slots in `PlayerInputFrame`. Players collide;
  P2 sees P1's committed movement within the same native update. Preserve this
  ordering. Player shells do not damage teammate tanks or cancel friendly shells,
  although player shells can damage headquarters and its enclosure.
- Maps still have 20 enemies and at most four active enemy slots. Each player
  has their own lives, progression, spawn state and projectile ownership.
  One player's death or elimination does not by itself end the team episode.
  The native game also waits for final-life projectiles before declaring defeat.
- Preserve the real five-second clear buffer and all death/respawn/settlement
  behavior. See [gameplay invariants](GAMEPLAY_INVARIANTS.md), the
  [architecture](ARCHITECTURE.md), and [existing training](AI_TRAINING.md).

## Environment and observation contract

Use the explicitly versioned cooperative adapter alongside the existing v1 path.
Do not reinterpret an old checkpoint's input shape or silently change its ABI.
One environment handle owns exactly one two-player `Game3D` instance.

```text
reset(seed, options={"stage": stage}) -> observations, info
step([action_p1, action_p2]) -> observations,
                              team_reward, terminated, truncated, info
```

Both actions come from the same pre-step snapshot. Build both input slots, then
advance the world once for three 1/60-second ticks. Each slot needs its own
previous-direction state. Validate the complete action pair before advancing
either player. Retain 20 Hz decisions and the existing ten actions per tank.

Each actor receives the same schema, with its controlled tank designated
`self` and the other tank designated `ally`. Keep world-cardinal coordinates;
do not mirror the maps or reverse native player iteration order.

| Input | Purpose |
| --- | --- |
| Existing terrain, enemies, bonuses and headquarters | Preserve learned navigation/combat context |
| Self position, heading, movement, HP, lives, level, cooldown and availability | Choose a feasible action for this tank |
| Ally position, relative position, heading, HP, lives, level, cooldown, shield/Boat and availability | Yield, cover approaches, and adapt when the partner is weak or absent |
| Self/ally/enemy projectile ownership and motion | Represent both firing capacities and incoming threats accurately |
| Recent displacement and executed actions for both players | Distinguish movement attempts from progress and observe partner behavior |
| Slot identifier and masks | Resolve ties and distinguish spawning, inactive and eliminated tanks |

Append new features with a documented layout; add an ally occupancy channel.
Start with four recent decision intervals of small numeric features, rather
than a recurrent network or stacks of full maps. History must be computed the
same way in training and human play, using past executed commands only.
Neither current unexecuted teammate commands, future actions, RNG state, enemy
targets nor partner checkpoint identity belong in the actor observation.
The critic uses a canonical team snapshot, not two needlessly duplicated maps.

## Shared policy and PPO learning

Use parameter-shared PPO with a team value function: a small MAPPO-style
implementation. The two actors share one set of weights and receive different
self/ally observations. This does not require them to take identical actions.

```python
# Both calls use theta_old during rollout collection.
actions, log_probs = actor(observations_for_both_players)
next_state, reward, done = world.step(actions)
# A separate value network estimates the team's future return during training.
```

Training keeps rollouts indexed by `[time, world, player]`, plus one team
transition and reward per world step. Compute team GAE from the team value
function. Optimize per-player clipped PPO objectives, averaged over valid
learning-player samples, with one shared actor optimizer. Train the critic once
per team transition. Do not treat two coupled players as independently resetting
Gym environments, multiply policy ratios into an unnecessary 100-action
controller, or count one enemy kill twice in team statistics.

Keep the rollout actor and any frozen partner fixed during collection. Store
each learning actor's actual sampled proposal and its old log probability.
Historical or scripted partners contribute environment transitions, not
on-policy actor-loss samples. If a fixed rule transforms a proposal, retain
both proposed and executed actions; never relabel a rule action as neural output.

Spawning, dead and eliminated players receive no-op commands while unavailable;
mask their actor and entropy losses. Keep team value targets and future credit
through respawn. Only the team's native terminal condition ends the episode.
Keep rollout boundaries and artificial time limits distinct from native defeat;
bootstrap truncations from the final pre-reset state under the declared
time-limit convention. Never bootstrap from a reset observation.

Transfer the retained single-player actor's compatible weights as an
initialization. Zero-initialize added input connections and verify old logits
when ally/history inputs are absent. Initialize and fit the team critic anew;
use a fresh optimizer for this changed task, rather than blindly resuming the
old single-player value function and optimizer state. Preserve the old model
as a baseline. Measure whether transfer helps with a small scratch baseline.

Keep inference in the current approximately 200,000-parameter size class.
With two AI tanks, batch the two actor evaluations. With human P1, run only the
P2 actor: no P1 model, critic or training framework is inherently required by
the decision algorithm. Export/app integration is a separate implementation
step; the current game still does not ship an ML runtime. Remeasure the complete
P2 observation/rule/actor path; the old 0.45 ms measurement is not a cooperative
performance result.

The algorithm choice follows the parameter sharing and training-only value
function approach in [the MAPPO paper](https://arxiv.org/abs/2103.01955).
Its benchmark results do not establish performance in Tanks3D.

## Reward: score the team's outcome

The adapter implements a symmetric version of the existing native event reward.
Both cooperative rounds keep this baseline unchanged before testing additional
shaping.

| Event, counted once for the team | Initial unscaled reward |
| --- | ---: |
| Enemy destroyed by either player | +3 |
| Nonfatal enemy damage | +0.25 |
| Nonfatal damage to either player | -1 |
| Either player destroyed | -3 |
| Bonus collected by either player | +1 |
| Player-caused headquarters/enclosure damage event, either slot | -2 |
| Actual completed stage clear | +50 |
| Base-loss terminal | -50 |
| All-players-defeated terminal with base alive | -30 |

Both learners receive the same team reward; PPO loss reduction must not double
its scale. Use the existing 0.1 reward scale as the initial comparison setting.
Count damage/destruction and terminal events according to the real event
contract, including grenade effects, attribution and base-loss priority.
Record per-player deaths, pickups and contributions separately from reward.

Keep the experimental additional death cost at zero initially: rounds nine and
ten did not establish a reliable benefit. Do not reward personal last hits,
proximity to P1, merely staying alive or arbitrary movement. Those incentives
can encourage competition, crowding, inactivity or aimless motion.

Initially measure blocked movement and ally obstruction without penalizing them.
Useful stationary firing and destruction of a brick passage must not be confused
with useless blockage. If logs later justify shaping, introduce one bounded
term at a time with matched-seed controls and the original outcome metrics.

## Partner curriculum

1. **Shared-policy self-play:** both tanks use the current actor; pool their
   learning samples. Establish the two-player baseline and preserve checkpoints
   of different strengths from multiple independent training seeds.
2. **Mixed partners:** retain self-play episodes and add episodes with a frozen
   historical or scripted partner. Round two uses 50% current/current,
   30% historical partners, and 20% scripted Defender episodes. These are
   experiment settings, not established optimal ratios. Train only the current learner
   in frozen-partner episodes and vary its assigned slot without changing native
   update order or spawns.
3. **Human-P1 acceptance:** evaluate P2 with unfamiliar partners and actual human
   play. Include partners that attack, defend, hesitate, hold a direction longer,
   collect bonuses, or become eliminated. Artificial delays apply to the partner's
   commands, not native physics. Such proxies do not replace human evaluation.

All trainable actors still use the same policy architecture and shared current
weights. Historical partners are training fixtures; only one final actor is
needed for deployment. Partner weights do not update within a rollout; a new
fixed partner can be sampled when an episode resets. Do not train forever only
against the current policy's identical copy: it can learn conventions a human does not follow.

This is inspired by [Fictitious Co-Play](https://arxiv.org/abs/2110.08176), which
trained against varied self-play agents and historical checkpoints and tested
human cooperation in Overcooked. Transfer of that benefit to Tanks3D remains
an experiment, not a guarantee.

Do not hard-code P1 as attacker and P2 as permanent headquarters guard. Let
observed partner behavior and threats drive the division of work. Retain the
small headquarters fire guard as a controlled training/runtime option. The
existing full defense takeover should be a separately evaluated baseline:
duplicating it for both slots can send both tanks toward the same threatened
area without considering their partner. Do not assume this is coordination.

## Evaluation and implementation sequence

Evaluate every strategy in the same two-player game, with the same partner,
maps, seeds, lives and time-limit convention. Comparing AI+AI with the old solo
agent confounds cooperation with an extra tank, gun and life pool.

Primary comparisons are human/held-out P1 + candidate P2 against the same P1
partner + fixed-rule P2 and + a compatible previous-actor baseline. Current
policy + itself is a useful additional test, not the primary selection target.
Keep held-out partners, episode seeds and final outcomes separate from training
and selection. Report partner-specific and worst-partner results, not only a
pooled average. Keep the existing 350 all-map and 100 extra Stage 1 episode
cohorts for full assessments, and report multiple independent training seeds.

Measure team clears, headquarters survival, both players' deaths and elimination,
P1 survival, timeouts, player-caused HQ damage, long blocked runs, diagnosed ally
obstruction and unnecessary camera expansion caused by tank separation. Do not
force equal kills or pickups: useful cooperation can be asymmetric. Test P1
elimination, P2 respawn, narrow passages, contested pickups and continuing alone.
Inspect native recordings and actual human-P1 play before claiming a good teammate.

Implement in these bounded increments:

1. Versioned two-player native adapter and observation/event contract, with
   one-player compatibility and exact production two-player replay tests.
2. Shared-actor/team-critic rollout and PPO training, including action masks,
   shared rewards, proposal log probabilities and correct resets/bootstrap.
3. Fixed-partner pool, matched evaluation and native two-AI recordings; then
   human-P1/AI-P2 playback for acceptance and later app integration.

An initial learning pilot can use four worlds and 262,144 world decisions per
training seed, with at least three seeds when comparing methods. In shared
self-play this yields up to twice as many actor proposals, not twice as many
independent world transitions. Report world ticks, joint decisions, valid actor
samples and completed team episodes separately. Establish Stage 1 operation,
then use the original 35-map curriculum without changing terrain or physics.
Freeze training budgets, reward settings, partners and selection rules before
evaluating each comparison; do not inherit the old late-stage tiny learning rate
without a short stability check for the new critic and observation inputs.

Native/build changes require `make clean`, `make test`,
`make test-ai test-ai-native`, and appropriate sanitizer checks for refactors.
Add two-player state/RNG/event parity, separate direction-edge handling,
teammate observations, reward symmetry and once-only accounting, inactive-player
masks, terminal/truncation behavior and actual shared-weight updates to the
relevant tests. Keep golden layouts intact. Put run artifacts and recordings
under `build/`; ordinary app controls and gameplay remain production-owned.
