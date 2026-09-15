# Training a player

The optional `training/` tools train players on the actual C++ game. The original
single-USA-player path and the new cooperative two-player path have separate
versioned interfaces and checkpoint formats. They do not change gameplay or the
normal app, and no ML dependency is bundled in `Tanks3D.app`. These are
internal-state agents, not models that recognize screenshots.

## Rule-policy round one: stronger observation-only cooperation

The opt-in `TacticalDefender` version 1 improves the scripted cooperative player
without training a network. It reuses the existing Navigator geometry for one-tile
paths and firing lanes, avoids the ready ally's footprint, chooses reachable firing
positions, stops for aligned shots, and assigns home coverage from both tanks'
positions. It reads the existing self-relative map/state only. Native gameplay,
maps, rewards, actor weights, historical Defender and HQ fire guard are unchanged.

Two rule candidates and the old control completed **630 development episodes** on
all 35 maps. Version 1 passed the frozen safety/cost gates and achieved a weighted
clear score of .940 versus .754 for the old rules. Version 2 added short-horizon
projectile evasion but scored .914, so it was rejected. Candidate sources and the
selection were frozen before the independent evaluation below; final outcomes did
not change the selected policy.

**2,160 reserved evaluation episodes actually ran**, 1,080 per policy, using matched
seeds, three lives per tank, a 120-second limit and the original stage-clear buffer:

| P1 partner / maps | Episodes per policy | Old clears | New clears | Old bases alive | New bases alive |
| --- | ---: | ---: | ---: | ---: | ---: |
| Same rules / all 35 | 350 | 247 (70.6%) | 326 (93.1%) | 321 | 343 |
| Frozen retained actor / all 35 | 350 | 288 (82.3%) | 326 (93.1%) | 328 | 344 |
| Historical Defender / all 35 | 140 | 105 (75.0%) | 124 (88.6%) | 127 | 136 |
| Held-out historical actor / all 35 | 140 | 117 (83.6%) | 127 (90.7%) | 136 | 136 |
| Same rules / Stage 1 | 100 | 78 (78.0%) | 99 (99.0%) | 99 | 99 |

The shared-rule comparison changes both tanks; fixed-partner comparisons change
only P2. Shared-rule clear improvement was +22.6 percentage points, with a paired
95% bootstrap interval of +17.7 to +27.4 points. The frozen-neural-partner gain was
+10.9 points, interval +6.9 to +14.9. These intervals describe evaluation-seed
uncertainty on these fixed maps/AI partners, not human acceptance or unseen maps.
The held-out partner was round-one seed 14051, excluded from rule selection.
No self-inflicted HQ hits occurred in any development or reserved arm.

P1 safety is mixed: mean deaths changed from 1.289 to 1.374 with shared rules and
from 1.236 to 1.336 with historical Defender P1; with frozen neural P1 they changed
from 1.489 to 1.417. Their paired intervals include zero. Better team clearing does
not establish safer human cooperation. Shared-rule timeouts fell from 66/350 to
9/350, but stopped fire/yielding is excluded from the moving-command stall metric;
zero measured movement stalls is not proof of no deadlock.

Quiet single-P2 rule prediction including its observation view and unchanged HQ
guard averaged **0.149 ms**, p95 **1.647 ms**, versus old **0.268 / 2.086 ms**.
Fixed old P1 + P2 rules + native observations/world updates averaged **0.462 ms**
versus **0.474 ms** over 4,096 measured decisions after 256 warmup decisions per
policy. These are continuous own-policy trajectories, not identical-input
microbenchmarks, and exclude rendering/reset/initialization. The standalone
benchmark loaded no Torch/model/critic and peaked at 61.1 MiB for the entire Python
process. The generic cooperative CLI still imports Torch for optional neural P1.

Matched Stage 5 recordings, seed 3000004, show the old pair timing out with 18 kills
and P2 blocked near P1, while the new pair clears at 54.75 seconds (with two P1 deaths
versus zero in the old episode). A new-rule Stage 21 loss, seed 3000020, remains a
counterexample: P2 was on the western side near z=16.75 while an enemy was already
on the headquarters' z=25 row; the base fell at 36.033 seconds. Home assignment
still needs a better emergency return/interception rule. This is not a per-rule
causal ablation. Three native 960x540/20 fps recordings were decoded in full;
all 4,216 recorded actions, native digests and outcomes reproduced exactly.

`make clean`, `make -j4 test ai-native`, `make test-ai`, and
`make -j4 test-sanitize test-ai-sanitize` passed. The Python suite now has 57 checks;
native parity still covers 18,204 cooperative ticks and 3,620 original adapter
ticks. An additional 14,000-decision comparison across 35 maps matched all old
controller actions/counters against the archived source. The native library is
byte-identical; prior uncommitted work and the previous evidence remain preserved.

See `build/release-evidence/rules-20260914-round1/REVIEW.md` for complete results,
paired intervals, recordings, source hashes and exact commands. `--rule-p2 tactical`
is opt-in; existing training partners/defaults retain historical Defender behavior.
These are independent stage episodes, not a continuous campaign. Human-P1 controls
and ordinary-app AI-P2 integration remain unimplemented.

```sh
make ai-native
caffeinate -dimsu env PYTHONDONTWRITEBYTECODE=1 build/ai-venv/bin/python -m training.coop_play \
  --rule-p2 tactical --partner self --seed 3000004 --stage 5 --seconds 120 \
  --trace build/ai-runs/tactical-watch.json
```

Both tanks use separate instances of the new rules. Use a fresh trace path on
repeat. `make run-app` does not enable experimental AI controllers.

## Cooperative round two: all maps and mixed partners

Three independent runs completed **1,572,864 world decisions**, **2,348,129
current-actor proposals** and **1,074 complete team episodes**. Each run used 524,288
decisions, including 32,768 critic-only warmup, on a shuffled deck of all original 35
maps. Episode partners were 50% current/current, 30% frozen history and 20% scripted
Defender, with a randomized learning slot for fixed partners. Actor LR was 1e-5.
Rewards, native rules, observation layout, runtime actor/controller and the HQ fire
guard were unchanged.

**All three new endpoints failed the all-map shared-policy base-survival gate. The
retained initial actor remains selected.** This round does not establish a better
policy.

| Validation endpoint | Shared P1 clears / 140 | Shared P1 bases / 140 | Initial P1 clears / 140 | Defender P1 clears / 140 | Stage 1 clears / 100 |
| --- | ---: | ---: | ---: | ---: | ---: |
| initial | 115 | 128 | 105 | 107 | 89 |
| seed-16001 | 92 | 119 | 98 | 105 | 91 |
| seed-16051 | 102 | 114 | 107 | 106 | 97 |
| seed-16101 | 96 | 115 | 111 | 103 | 94 |

Seed 16101 also failed the scripted-partner P1-death gate. Stage 1 gains do not offset
the shared-policy all-map regression. Frozen and scripted partners were held fixed
across paired policies; the shared-policy comparison changes both slots. No checkpoint
screening or final-result reselection occurred. The old actor is eligible as a fallback.

Additional reserved outcomes below characterize that retained actor. Initial and
selected have identical weights, so their final files explicitly alias one execution.
These are not gains from the new training:

| Reserved P1 partner | Episodes | Clears | Base alive |
| --- | ---: | ---: | ---: |
| Same retained actor / all 35 maps | 350 | 260 | 318 |
| Frozen initial / all 35 maps | 350 | 267 | 317 |
| Scripted Defender / all 35 maps | 350 | 291 | 338 |
| Same retained actor / Stage 1 | 100 | 94 | 95 |
| Held-out historical / all 35 maps | 140 | 111 | 130 |

The Defender + Defender control cleared 257/350 with 320 bases alive. In total **3,720
evaluation episodes actually ran**: 2,080 validation and 1,640 reserved. The held-out P1
was round-one seed 14051, excluded from this round training and selection; it is one
unfamiliar AI, not a human. Historical training partners were the retained initial actor
and round-one seeds 14001/14101.

Exact replay found a concrete regression on Stage 4, seed 2100003: the reference cleared
at 40.083 s. Seed 16051 killed 19 enemies without losing a player but lost the base at
53.017 s. Both tanks stayed far north while the remaining enemy operated near the
southern headquarters. This supports investigating missed home coverage; it does not
prove a new reward term would help. Additional death shaping remains off. A recorded
baseline loss also shows the retained actor is not yet a dependable teammate.

The actor stays at **227,282 parameters / 909,128 FP32 bytes**, receiving 10,524
internal-state floats per player. Quiet single-thread prediction, tensor conversion,
sampling and HQ guard averaged **0.207 ms** on Apple M2. Scripted P1 + neural P2 plus
both native observations and world ticks averaged **0.541 ms**, p95 **2.256 ms**, over
4,096 decisions. This excludes rendering, reset and initialization; no critic or P1
neural model was instantiated. Whole Python/PyTorch peak RSS was 236.5 MiB.

`make test-ai test-ai-native` passed 51 Python checks and the existing native parity
checks; `make test-ai-sanitize` passed. A real 8,192-decision mixed smoke passed. An
archived-before/new default-self-play CLI comparison matched actor, critic, both
optimizers and RNG states bitwise after 1,024 world decisions. The native library is
byte-identical. Changes are confined to the training CLI, new partner/deck module,
related tests and AI docs; prior uncommitted work and evidence were preserved.

See `build/release-evidence/ai-coop-20260914-round2/REVIEW.md` for the frozen protocol,
full gates, actual recordings, counterexample replay, model hashes and exact
training/viewer commands. Training saves critic/optimizers/RNG, but loading
`--coop-parent` is actor transfer with a fresh critic and optimizers, not exact resume.
Human-P1 controls, ordinary-app AI-P2 integration and human acceptance remain
unimplemented. No release or push was performed.

```sh
make ai-native
caffeinate -dimsu env PYTHONDONTWRITEBYTECODE=1 build/ai-venv/bin/python -m training.coop_play \
  --model build/release-evidence/ai-coop-20260914-round2/ai-player2.pt \
  --partner self --seed 2300000 --stage 1 --seconds 120 \
  --trace build/ai-runs/coop-round2-watch.json
```

The viewer shows two AI tanks sharing the retained policy. Choose a fresh trace path on
repeat; `make run-app` does not load experimental training weights.

## Cooperative round one: shared actor in a real two-player world

Three independent Stage 1 runs completed **786,432 world decisions**,
**1,572,864 actor proposals** and **759 complete team episodes**. Both tanks use
one current actor with their own self/ally observations; a separate team critic
is used only in training. The new adapter preserves the production two-player
update order, collisions, lives/respawns, projectile rules and five-second clear
buffer. A single player's death does not end the team episode.

The first experiment transfers compatible weights from the retained round-eight
actor, with a zero-initialized cooperative residual and a fresh team critic and
optimizers. Team rewards count native events once, symmetrically for both slots;
the extra death cost remains zero. All learners and baselines use the same small
HQ fire guard, without the full single-player defense takeover. Training uses
shared self-play; frozen-old-policy and fixed-defender partners are evaluation
conditions in this round, not yet a mixed-partner training curriculum.

Each of three fixed-budget endpoints and the converted initial actor received
100 matched validation episodes per partner. The validation-frozen selection is
**seed 14051**; the other two trained endpoints failed the shared-partner base
survival gate. This initial two-player baseline already clears 95–96% of Stage 1
validation episodes. Old solo scores cannot establish a cooperative improvement.

Reserved Stage 1 outcomes, with 100 episodes per partner and policy:

| P1 partner | Initial actor clears | Selected actor clears | Initial / selected base alive |
| --- | ---: | ---: | ---: |
| Same policy as P2 | 92 | 95 | 98 / 96 |
| Frozen initial actor | 97 | 95 | 97 / 95 |
| Fixed defender | 95 | 97 | 97 / 100 |

The exploratory all-35-map probe is a regression: **43/70** clears for the
Stage 1 continuation versus **55/70** for the initial shared actor, with base
survival falling from **64/70 to 51/70**. There are only two episodes per map,
so this is not a full all-map qualification, but the loss is sufficient reason
to keep this selection experimental. The selected artifact remains frozen;
the previous actor is preserved. This round does not establish a new best policy.
The next experiment should restore the original map curriculum and introduce
varied partners, with all-map retention checks, before adding more reward terms.

The actor has **227,282 parameters / 909,128 FP32 bytes**, with 10,524
internal-state floats per controlled player. Quiet single-thread actor prediction,
tensor conversion, sampling and HQ guard averaged **0.246 ms** on Apple M2.
The separate real-world measurement, including a scripted P1, neural P2, both
observations and three native ticks, averaged **0.606 ms** (p95 2.474 ms), excluding
rendering and episode resets. No critic or P1 neural model was instantiated in
that live measurement. Whole Python/PyTorch peak RSS was 237.2 MiB, which is
separate from actor tensor size.

See [the cooperative interface and curriculum](AI_COOP_TRAINING.md) and the local
experiment at `build/release-evidence/ai-coop-20260914-round1/REVIEW.md` for
reserved outcomes, gates, exact commands, recordings and limitations. The normal
app does not yet expose an AI-P2 toggle or human-P1 acceptance mode. This round
does not establish a dependable human teammate.

```sh
make ai-native
caffeinate -dimsu env PYTHONDONTWRITEBYTECODE=1 build/ai-venv/bin/python -m training.coop_play \
  --model build/release-evidence/ai-coop-20260914-round1/ai-player2.pt \
  --partner self --seed 1900000 --stage 1 --seconds 120 \
  --trace build/ai-runs/coop-round1-watch.json
```

Choose a fresh trace path when repeating the command. The cooperative actor is
an actor-only `.pt`; it is not interchangeable with the old single-player PPO
`.zip`. Both original single-player tools and evidence remain available below.

## Round ten: fixed-budget reward replication

**524,288 new PPO proposals** and **341 complete training episodes**
were executed in four runs. Two new training seeds (12401 and 12451) each
compared the original reward with the existing extra destruction cost of five.
Every run resumed the same round-nine common pilot and optimizer, added exactly
131,072 proposals, and had one fixed endpoint. No intermediate checkpoint
screening, new reward term, network change or runtime rule change was introduced.

The reference remains `build/release-evidence/ai-20260913-round8/ai-player.zip`
(counter 1,873,924), rather than the round-nine experimental continuation.
Restarting each arm from the common pilot isolates the replicated reward
comparison from any previously selected reward-arm updates.

Every endpoint and the previous reference received 350 all-map and 100 Stage 1
validation episodes. Paired reward differences below are extra cost minus
original reward; each row uses matching training and evaluation seeds.

| Training seed / validation | Clear difference (pp) | Base survival difference (pp) | Mean death difference |
| --- | ---: | ---: | ---: |
| 12401 / all35 | -0.57 | +0.29 | +0.017 |
| 12401 / stage1 | +1.00 | +3.00 | +0.060 |
| 12451 / all35 | -0.86 | +0.00 | +0.029 |
| 12451 / stage1 | +14.00 | +9.00 | -0.020 |

The validation-frozen candidate is **survival-12451**. The original regression
and efficiency gates were retained. The highest validation-score endpoint from
each reward arm was also frozen for descriptive final comparison, even if it
failed a gate. Both arm finalists happened to use training seed 12451.

After freezing, **1,800 actually executed reserved episodes** compared the
candidate, reference, arm finalists and fixed defender. Identical model/rule
combinations were executed once; aliases: `{'survival': 'selected'}`.

| Reserved test | Previous reference | Frozen candidate | Control finalist | Extra-cost finalist | Fixed defender |
| --- | ---: | ---: | ---: | ---: | ---: |
| All-map clears / 350 | 110 | 112 | 105 | 112 | 103 |
| All-map base alive / 350 | 270 | 264 | 272 | 264 | 260 |
| Additional Stage 1 clears / 100 | 65 | 68 | 73 | 68 | 46 |
| Additional Stage 1 base alive / 100 | 84 | 88 | 85 | 88 | 88 |

This is not an established new best player. The candidate gained two all-map
clears over the previous reference (+0.57 pp; paired stage-stratified 95%
bootstrap interval -4.57 to +5.71), while base survival fell by six episodes.
Stage 1 gained three clears (+3 pp; interval -10 to +16), but timeouts rose
from 1/100 to 9/100. Keep the validation-frozen weights as an experiment and
retain the previous reference; no weights were reselected on these results.

The added death cost did not replicate an all-map benefit: the two matched
validation pairs lost 2/350 and 3/350 clears, with slightly more mean deaths
in both. Stage 1 validation gains of +1 and +14 episodes did not establish
general improvement: the seed-12451 finalists reversed to 68/100 versus 73/100
on the reserved Stage 1 tests. The same finalists gained 7/350 all-map clears
(+2 pp; interval -3.14 to +7.14), with eight fewer surviving bases. Two training
seeds are a small exploratory sample. Keep the existing extra death cost
optional and default-off; neither a reliable benefit nor harm is established.

Long stationary moving-command runs averaged 19.36 seconds per all-map episode
for the candidate versus 15.99 for the previous reference; mean per-episode
maximum runs were 8.67 versus 7.05 seconds. The candidate also averaged 2.28
kills during these runs, and episode lengths differ. These totals alone do not
justify a blanket stationary penalty. Exact replay of the previously observed
round-nine 11.2-second run attributed its 224 decisions to the neural branch,
without establishing a causal explanation. Investigate blocked movement and
timely HQ defense before adding more reward terms.

Read-only diagnostics count consecutive moving commands with under .01 tile
displacement per three-tick decision for at least one second, excluding dead
or spawning players and intentional stop commands. Useful firing can occur
during these runs; the metric is descriptive and did not alter rewards or
selection gates. Ten matched episodes reproduced every original native field,
reward, final digest and hybrid counter with the diagnostic evaluator enabled.

The frozen candidate still has **201,426 actor parameters / 805,704 FP32 bytes**
and 9,720 internal-state floats. Quiet single-thread full hybrid prediction
averaged **0.450 ms**; decision plus three native ticks averaged
0.450 ms, excluding rendering. Whole Python/PyTorch/SB3 peak RSS
was 342.4 MiB, including the critic. The checked inference path
did not load the reward-wrapper module or execute the critic. Thirty-five
Python checks and 35-map native
observation / 3,620-tick production-state and RNG parity checks passed.

Full protocol, all weights, two-seed validation comparison, reserved outcomes,
movement diagnostics, figures, timing and native win/loss recordings are in
`build/release-evidence/ai-20260913-round10/REVIEW.md`. Production training,
runtime and game sources were unchanged; no ordinary-app integration or release
was performed. All previous evidence and weights remain available.

```sh
make ai-native
caffeinate -dimsu env PYTHONDONTWRITEBYTECODE=1 build/ai-venv/bin/python -m training.play \
  --model build/release-evidence/ai-20260913-round10/ai-player.zip \
  --hybrid-mode defense --stochastic --stage 1 --seed 1110077 --seconds 120 \
  --trace build/ai-runs/round10-watch.json
```

## Round nine: continuation and a death-penalty comparison

**458,752 new PPO proposals** and **298 completed training episodes** were
executed: a 65,536-proposal common pilot, then two matched 196,608-proposal
continuations. Another 6,144 proposals checked the actual CLI reward pipeline
and are excluded from candidate training. The validation-selected candidate is the original-
reward continuation, `control-196608`, with counter **2,136,068**. The actor
and runtime defense rules are unchanged; reward shaping remains default-off.

The original aggregate pilot trigger for reward modification was **not met**.
A subsequent 35-episode event replay motivated a separately documented
exploratory study: add five to the cost of each actual player destruction
(event contribution -3 to -8), keeping all other rewards and gameplay.
Forty-seven of 69 deaths occurred in neural-controlled steps, but the neural
branch also controlled 73.5% of decisions. Neither this count nor the
exposure-normalized rates establish causation; the branches face different
danger and previous actions can cause later events. The original negative
trigger and adaptive study addendum are both preserved in evidence.

Both branches used the same pilot weights/optimizer, seed, curriculum, PPO
configuration and training budget, with three checkpoints screened per arm.
Their best checkpoints were validated on 350 all-map and 100 Stage 1 episodes.

| Validation | Previous hybrid | Original reward | Extra death cost |
| --- | ---: | ---: | ---: |
| All-map clears / 350 | 96 | 100 | 99 |
| All-map base alive / 350 | 248 | 258 | 250 |
| Additional Stage 1 clears / 100 | 61 | 68 | 74 |
| Additional Stage 1 base alive / 100 | 78 | 78 | 89 |
| Additional Stage 1 timeouts / 100 | 0 | 2 | 4 |

The extra-death finalist failed the predeclared Stage 1 timeout gate
(maximum reference +3 percentage points). The common pilot also failed that
gate. The original-reward continuation passed every gate and ranked above
the reference. The gates were not relaxed for the reward experiment.

After freezing that selection, **1,800 actual reserved episodes** compared
the selected continuation, previous hybrid, reward finalist and fixed
defender. Selected and control are one explicitly aliased execution.

| Reserved test | Previous hybrid | Continued original reward | Extra death cost | Fixed defender |
| --- | ---: | ---: | ---: | ---: |
| All-map clears / 350 | 104 | 97 | 108 | 97 |
| All-map base alive / 350 | 282 | 271 | 273 | 269 |
| Additional Stage 1 clears / 100 | 73 | 66 | 78 | 48 |
| Additional Stage 1 base alive / 100 | 84 | 87 | 90 | 90 |

Neither continuation has established a stable overall improvement. The reward
finalist exceeded the matched control by +3.14 all-map clear percentage points
(paired 95% interval -2.29 to +8.86) and +12 Stage 1 points (-1 to +25).
Both intervals include zero and only one training seed was run per arm.
Mean all-map deaths were identical at 1.974; Stage 1 deaths were 1.71 with
the extra penalty versus 1.59 in the control. The original-reward continuation
also cleared fewer final episodes than the previous model. The validation-frozen
candidate remains unchanged as an experimental artifact, and previous weights
remain available; this is not a claim of a new best player or a deployment.

The recorded base-loss example had zero player deaths and a continuous 11.2 s
of movement commands producing under .01 tile displacement per decision.
This is descriptive evidence of poor progress, not a diagnosis of the exact
obstacle or controller cause, but the extra death cost cannot directly signal
that failure. Keep shaping optional and default-off while investigating blocked
movement and timely HQ protection; no passive-survival or blanket stationary
penalty was added. Final tests were not used to reselect weights or relax gates.

The selected full warm hybrid decision averaged **0.391 ms** on one CPU thread,
after training/validation workers exited, using the same 188 real observations
and 3 x 4,096-call protocol. The live decision plus three native ticks averaged
0.368 ms; rendering is excluded. The actor remains 201,426 parameters,
805,704 FP32 bytes and 9,720 internal-state floats. Whole Python/PyTorch/SB3
process peak RSS was 341.0 MiB, including the critic. Forward hooks on the
selected model confirmed no critic calls and no reward module at inference.

Thirty-five Python checks and original-map/native gameplay/RNG parity passed.
Three real CLI smoke continuations additionally verified bitwise-identical
weights and optimizer state between the archived original entrypoint and the
new zero-penalty entrypoint, plus correct nonzero death penalties and actual
finite updates when enabled. The original-reward control had already loaded
the archived entrypoint before the new wrapper was implemented.

Protocol, adaptive addendum, all checkpoints, raw independent results, timing,
source snapshots and native win/loss recordings are in
`build/release-evidence/ai-20260913-round9/REVIEW.md`.
The frozen experimental candidate can be watched with the native renderer:

```sh
make ai-native
caffeinate -dimsu env PYTHONDONTWRITEBYTECODE=1 build/ai-venv/bin/python -m training.play \
  --model build/release-evidence/ai-20260913-round9/ai-player.zip \
  --hybrid-mode defense --stochastic --stage 1 --seed 920000 --seconds 120 \
  --trace build/ai-runs/round9-watch.json
```

These are local development weights, not ordinary-app integration or a release.

## Round eight: train with the runtime defense rules

Two continuations executed **524,288 new PPO proposals** and **349 complete
training episodes** with the existing `defense` rules participating in training.
The actor still has **201,426 parameters** and the same internal-state input.
The original neural proposal/log probability stays in the PPO buffer; the
executed action is transformed by the fixed rules. A separate 2,048-proposal CLI
smoke test was not part of candidate training or selection.

Neither trained finalist displaced the previous hybrid on predeclared validation:

| Validation | Previous hybrid | Arm A, 262,144 steps | Arm B, 131,072 steps |
| --- | ---: | ---: | ---: |
| All-map clears / 350 | **117** | 108 | 99 |
| All-map base alive / 350 | 276 | **289** | 271 |
| Additional Stage 1 clears / 100 | **76** | **76** | 70 |
| Additional Stage 1 base alive / 100 | 88 | 88 | 88 |

Arm A passed regression gates but ranked below the reference. Its fewer base
losses came with more game overs while the base survived (148 versus 126),
reducing clears. Arm B failed the Stage 1 clear gate. The selected model therefore
**retains the round-seven hybrid's weights and rule configuration**; this round
does not demonstrate a gain from newly trained weights.

After that freeze, **900 actually executed reserved episodes** compared the
retained hybrid with the fixed defender. Identical selected/reference models
were explicitly aliased, not counted as separate executions:

| Reserved test | Retained hybrid | Fixed defender |
| --- | ---: | ---: |
| All-map clears / 350 | 97 (27.7%) | 95 (27.1%) |
| All-map base alive / 350 | 272 | 254 |
| Additional Stage 1 clears / 100 | 64 | 59 |
| Additional Stage 1 base alive / 100 | 87 | 96 |

Both strategies had zero observed own-HQ/wall damage events. The hybrid's
all-map clear difference is +0.6 percentage points, with paired, stage-stratified
95% bootstrap interval **−4.9 to +6.0 points**; a reliable clear-rate advantage
over fixed rules has not been established. Stage 1 base survival is worse than
the fixed defender in this cohort (−9 points, interval −17 to −2).

The complete warm hybrid decision averaged **0.342 ms** on one CPU thread on the
same 188 saved real observations. A separate live sample averaged 0.313 ms for
a decision plus three native simulation ticks. Rendering is excluded; the
Python/PyTorch/SB3 process peaked at 343 MiB, including the critic. These are
local development measurements, not native-app deployment results.

The new optional training wrapper passed 32 Python checks, native terrain and
gameplay/RNG parity, and actual continuation/optimizer checks. Original native
gameplay, runtime rules and previous evidence were preserved. The protocol,
all checkpoints, independent results, timing, source snapshots and native win/loss
recordings are in `build/release-evidence/ai-20260913-round8/REVIEW.md`.

## Round seven: optional rule + neural decisions

Two further 524,288-decision PPO continuations completed 705 training episodes.
All four expanded neural candidates failed the original regression gates, so
the selected pure neural player retains the round-six weights. The optional
hybrid experiment uses those same weights with a separate runtime rule layer;
its gains must not be attributed to newly trained weights.

After independent validation and model/rule freezes, 1,350 actually executed
reserved episodes compared the retained actor, the fixed defender and the
selected `defense` hybrid. Identical previous/selected neural weights were
evaluated once and explicitly aliased in the raw pure-model comparison.

| Reserved test | Retained neural | Fixed defender | Hybrid |
| --- | ---: | ---: | ---: |
| All 35 stages, clears / 350 | 89 (25.4%) | 98 (28.0%) | **106 (30.3%)** |
| All 35 stages, base alive / 350 | 260 | 270 | **276** |
| Additional Stage 1, clears / 100 | 54 | 59 | **69** |
| Additional Stage 1, base alive / 100 | 72 | **93** | 91 |

The hybrid's all-map clear gain over the same neural weights is 4.9 percentage
points, with a paired, stage-stratified 95% bootstrap interval of **−0.3 to
+10.0 points**. This does not establish a reliable overall advantage over the
neural or fixed-rule baseline. The additional Stage 1 clear gain is 15 points
(interval +3 to +27), and base survival improves by 19 points (+10 to +29).
All 35 maps were trained; these are independent stages with three initial
lives, normal extra lives, a 120-second cap and the live five-second clear grace,
not a continuous campaign or unseen-map evaluation.

The hybrid still uses the **201,426-parameter actor** and the same **9,720
internal-state floats**. Quiet single-thread CPU measurements averaged
**0.343 ms** for the complete hybrid decision versus **0.308 ms** for ordinary
neural prediction on the same saved observations. These timings include rule
arbitration and defender navigation when selected; rendering is excluded.
Teacher takeover accounted for 29.4% of all-map final decisions, with no neural
call during takeover. The union of takeover and fire restriction was 29.6%.
The Python/PyTorch/SB3 process peaked at about 340 MiB; this is not a native-game
deployment or a model-only memory figure.

Observed player-caused headquarters/wall damage events fell from 204 to zero
in the all-map cohort. This does not imply the base always survives: the hybrid
still lost the base in 74/350 episodes, timed out in 35, and reached game over
with the base alive in 135. The guard ignores intervening cover and can
block useful shots. No hybrid-aware PPO training or normal-app AI integration
was performed.

The full protocol, original and revised source snapshots, all results, uncertainty,
timings and native recordings are in
`build/release-evidence/ai-20260913-round7/REVIEW.md`. Weights and recordings are
local development artifacts under `build/`, not distributed app assets.

### Historical round-six comparison

The sixth local round trained on **all 35 original maps**, compared wider
networks, and distilled a wider teacher back into the original small actor.
After validation-only selection and a recorded model freeze, reserved tests
compared that model, the fifth-round weights, and the unchanged scripted defender:

| Reserved test | Previous neural model | Selected neural model | Fixed defender |
| --- | ---: | ---: | ---: |
| All 35 stages, clears / 350 | 49 (14.0%) | **96 (27.4%)** | 105 (30.0%) |
| Additional Stage 1, clears / 100 | 46 | **54** | 52 |
| Additional Stage 1, base alive | 86% | **75%** | 92% |

The all-map clear-rate gain over the previous weights is 13.4 percentage
points, with a paired, stage-stratified 95% bootstrap interval of 9.1–17.7
points. It has not demonstrated beating the fixed defender. Stage 1 clear-rate
uncertainty includes no improvement, and its observed base survival regressed.
These are independent single-stage episodes with three **initial** lives,
normal extra-life awards and a 120-second cap, not a continuous campaign.
All 35 maps participated in training; there is no unseen-map claim.

The selected actor still has **201,426 parameters**, takes **9,720 internal-state
floats**, and averaged **0.295 ms** per sampled CPU prediction on this Mac.
There is no teacher, search or firing guard at inference. It won at least one
of ten reserved episodes on 25/35 maps; Stages 7, 12, 18, 21, 22, 26, 27, 28,
32 and 33 had no wins. This is broader competence, not reliable completion.

Weights, all candidate results, source snapshots, a Stage 34 winning video,
a Stage 1 base-loss video, uncertainty and reproduction commands are in
`build/release-evidence/ai-20260913-round6/REVIEW.md`. One training branch hit
an existing native enemy-fire assertion and resumed from a saved checkpoint.
The suspected floating-point RNG endpoint is documented in that folder's
`NATIVE_ASSERTION.md`; the historical crash was not fully reproduced in that
round. No native gameplay, normal-app integration or distributed assets changed
during round six. Earlier evidence and weights remain available.

The subsequent continuation corrects the demonstrated floating-point upper
endpoint in the production random adapter, without resampling. The old adapter
fails the new regression; the corrected adapter passes a clean full test build,
ASan/UBSan and native parity checks. This fixes the demonstrated boundary, while
the exact historical crash trajectory remains unavailable. Completed training and fresh
comparisons are tracked under `build/release-evidence/ai-20260913-round7/`;
see `NATIVE_FIX.md` there for the intentional correction and actual checks.

## Setup and first run

From the repository root, with Python 3.14 and the normal game dependencies:

```sh
make ai-setup
make ai-native
make test-ai test-ai-native
PYTHONDONTWRITEBYTECODE=1 build/ai-venv/bin/python -m training.train \
  --output build/ai-runs/ppo-first --steps 262144
```

The pinned requirements were verified on macOS arm64. The native adapter is
currently macOS-specific, like the game build. Python environments stay under
`build/ai-venv`; compiler output is in `build/ai`. `make clean` removes the
native library, so rebuild it before the next training run. Run outputs must
be under `build/` and new output names are required to preserve prior evidence.

A model can learn from a scripted defender before reinforcement learning:

```sh
PYTHONDONTWRITEBYTECODE=1 build/ai-venv/bin/python -m training.imitate \
  --local --output build/ai-runs/imitation --samples 60000 --epochs 24
PYTHONDONTWRITEBYTECODE=1 build/ai-venv/bin/python -m training.train \
  --resume build/ai-runs/imitation/model.zip \
  --output build/ai-runs/ppo-after-imitation --steps 524288 \
  --lr .00003 --entropy .003 --gamma .999
```

`imitate` uses supervised teacher labels, not reinforcement learning. Its
optional `--dagger --model PATH` collects teacher corrections in states visited
mostly by the learner. Report these stages separately. The `defender` policy
is a hand-written baseline. Ordinary `--policy ppo` evaluation and playback
without `--hybrid-mode` execute the learned policy without that defender.

`--local` adds a player-centered 5×5 terrain crop, relative enemy/shell
coordinates and lane offsets, all derived from the same observation. The
global-only encoder remains loadable for earlier checkpoints. `--data PATH`
reuses a demonstration archive and records its hash. Checkpoint continuation
preserves the saved encoder. Learning rate, entropy and discount can be
overridden with `train --lr`, `--entropy` and `--gamma`.
`latest.zip` is refreshed during training; completed optimizer updates are
also retained at rollout boundaries after each 65,536 decisions as `step-N.zip`. Use validation episodes
to choose among these checkpoints, leaving final test seeds untouched.

### Optional rule + neural inference

`training.hybrid.HybridPolicy` wraps the existing trained actor without changing
its parameters, observation or ten-action contract. `base-guard` strips only
the firing bit for the fixed defender's conservative headquarters corridors,
including a stopped shot in the current heading. It ignores intervening cover;
it is not an exact projectile forecast or a guarantee that the base survives.

`defense` additionally hands control to the unchanged scripted Defender when
an observed active enemy comes within 10 world units of headquarters (13,25).
The takeover persists until all eligible enemies are more than 12 units away.
Spawning/frozen enemies do not trigger takeover or serve as its target, and
teacher routes reset on entry, exit and episode reset. The model is skipped
while rules control the tank. Both paths pass through the same firing guard.
Counters record neural calls, defender calls, suppressed shots and the fraction
of decisions controlled or changed by rules, counting each decision once.
The existing `own_base_hits` metric includes player-caused damage events to
the headquarters and its protective walls; it is not a count of destroyed bases.

This path is explicitly **hybrid**, unlike `--policy ppo`, which still runs
only the neural actor. Round seven trained without executing the hybrid;
changing actions at inference can therefore expose weaknesses in those weights.
The optional training wrapper described below uses the same runtime rules.
All decisions use the existing internal-state observation; there is no image
recognition, enemy hidden-target access or lookahead into game RNG.

### Training with the fixed hybrid rules

`training.train --hybrid-mode defense` wraps the real environment with
`HybridTrainingEnv`. PPO proposes an action; the unchanged `HybridPolicy`
arbitrates and applies its firing guard before the native simulation advances.
The underlying observations, reward, termination and ten-action contract stay
the same. Omitting this option preserves ordinary neural-only training.

The PPO buffer retains the **original neural proposal and its log probability**.
The executed rule action is an environment action transform, not a supervised
label or a neural action with a mismatched likelihood. Tests check the actual
SB3 rollout buffer, execute an optimizer update, and replay wrapped actions
against the standalone hybrid on seeded native games, including resets.

Unlike runtime inference, PPO still computes a proposal on every training step,
even when the rules take over. Rule-only steps have no proposal-dependent action
effect, so their policy gradient adds sampling noise rather than teacher
imitation. The rule controller's route/hysteresis memory is not an added network
input; the feed-forward actor and critic do not observe all controller history.
This is a small compatible continuation experiment, not an optimality guarantee.

Per-step `hybrid_action` records separate proposed/executed actions, takeover and
fire restriction. Terminal `hybrid_training` counters use
`neural_branch_decisions` and `defender_decisions`, avoiding a claim that training
skips network forwards. Training `result.json` totals include incomplete final
episodes. Saved model ZIPs still contain ordinary PPO weights; playback requires
the matching `--hybrid-mode defense` to reproduce the trained action environment.

```sh
PYTHONDONTWRITEBYTECODE=1 build/ai-venv/bin/python -m training.train \
  --resume build/release-evidence/ai-20260913-round7/ai-player.zip \
  --output build/ai-runs/hybrid-continuation-example --steps 262144 \
  --hybrid-mode defense --seed 10401 --threads 1 \
  --lr .000003 --entropy .002 --gamma .9995 --gae .98 --reward-scale .1 \
  --stages $(seq 1 35)
```

This example samples maps uniformly. The round-eight experiment's predeclared
weighted curriculum, exact commands and independent evaluation seeds are saved
in `build/release-evidence/ai-20260913-round8/protocol.json` and its run configs.

Evaluate the retained round-eight hybrid with the mode recorded
in `build/release-evidence/ai-20260913-round8/selected-hybrid.json`. For example:

```sh
make ai-native
PYTHONDONTWRITEBYTECODE=1 build/ai-venv/bin/python -m training.evaluate \
  --policy hybrid --hybrid-mode defense \
  --model build/release-evidence/ai-20260913-round8/ai-player.zip \
  --stochastic --episodes 35 --stages $(seq 1 35) --seed-start 820000 \
  --output build/ai-runs/hybrid-new-evaluation.json
PYTHONDONTWRITEBYTECODE=1 build/ai-venv/bin/python -m training.play \
  --model build/release-evidence/ai-20260913-round8/ai-player.zip \
  --hybrid-mode defense --stage 1 --seed 710001 --stochastic --seconds 120 \
  --trace build/ai-runs/round8-hybrid-play.json
```

`--hybrid-mode` requires `--policy hybrid` in evaluation and `--model` in
playback. Omitting it in playback retains ordinary neural playback. Recorded
hybrid traces include the configuration, counters, model and rule-source hashes;
`play --replay PATH --trace NEW_PATH` replays their recorded actions without
needing to run either the model or the rules. New output paths are required.
The optional tools are local development code, not bundled in the normal app.

### Continued training with defensive demonstrations

The second experiment continues existing policy weights, with an optional
separate value encoder and supervised rehearsal between PPO rollouts:

```sh
PYTHONDONTWRITEBYTECODE=1 build/ai-venv/bin/python -m training.train \
  --resume build/release-evidence/ai-20260912/ai-player.zip \
  --output build/ai-runs/defensive-continuation --steps 524288 \
  --seed 271 --lr .00003 --entropy .003 --gamma .999 --gae .98 \
  --separate-value --reward-scale .1 \
  --guidance-data build/release-evidence/ai-20260912/imitation-v1/demonstrations.npz \
  --guidance-batches 8 --guidance-lr .0001
```

`--separate-value` copies the existing weights into independent actor and value
encoders, preserving action probabilities at migration. The optimizer is reset
when migrating the encoder; later ordinary checkpoint continuation preserves
the PPO optimizer. `--reward-scale` only rescales rewards received by training;
native gameplay, scoring, event metrics and evaluation rewards are unchanged.

`--guidance-data` enables training-only rehearsal. Before each new PPO rollout,
the policy takes supervised gradient steps using half original demonstrations
and half corrections collected in recent learner-visited states. A fixed
observation-only defender supplies these labels. They are attached to the state
that produced the action, including at episode boundaries. The bounded replay
retains 16 rollouts; the initial `defense-v1` experiment used 32. This addresses
the distribution shift studied in [DAgger](https://proceedings.mlr.press/v15/ross11a.html)
using bounded replay, without assuming the full algorithm's theoretical guarantees.
The update runs before collection, consistent with the installed SB3 callback
lifecycle and its [callback documentation](https://stable-baselines3.readthedocs.io/en/master/guide/callbacks.html).

The rehearsal optimizer and recent-label buffer start fresh on each continuation;
the checkpoint contains the neural policy and PPO optimizer, with no teacher at
inference. Configurations record source model/data hashes and hyperparameters.
Supervised loss is recorded separately in `rehearsal.json`; it is not a win rate.

`--tactical` adds tile-scale alignment and headquarters-relative cues to the
existing local encoder. The additional residual starts at zero when migrating
existing weights, so initial action probabilities are unchanged. The cues do
not select an enemy, choose an action or suppress a shot. The original 35 maps
share the headquarters position used by these cues. `--guidance-nonfire-weight 4`
emphasizes rare non-firing teacher labels during training; it adds no runtime
firing restriction. `--stages 1 1 1 2 3 4 5` samples the first stage more often
while also training on Stages 2–5. No maps or gameplay rules are simplified.

Collect demonstrations from multiple original maps without fitting a new model:

```sh
PYTHONDONTWRITEBYTECODE=1 build/ai-venv/bin/python -m training.imitate \
  --collect-only --output build/ai-runs/multimap-demos --samples 120000 \
  --seed 3981 --stages 1 2 3 4 5 6 7 8 9 10
```

The collector cycles the requested stages at episode resets, using fresh
training seeds. Stage/seed pairs are recorded in its configuration; it saves
`demonstrations.npz` without writing a model checkpoint. All demonstration actions
come from the same fixed scripted defender, whose own limitations still apply.

`train --guidance-safety-weight 2` optionally adds supervised negative firing
labels during rehearsal. It penalizes neural probability assigned to firing
down the defender's conservative headquarters corridors, including firing
while stopped in the current heading. The labels ignore intervening cover;
they are not an exact shell collision predictor. This is a training loss,
separate from native rewards, with no firing restriction at inference. Its
weight defaults to zero and is recorded alongside imitation loss in the run.

Second-round evidence is under `build/release-evidence/ai-20260912-round2/`.
Its validation uses 72000+, final testing uses 120000+, and untrained Stages
6–10 use 130000+. Compare old and new policies on the same new test seeds.

The third round ran two 524,288-decision continuations from the second-round
selected weights: the unchanged Stage 1–5 configuration, and a Stage 1–10
curriculum with the new demonstration corpus and safety loss. The selected
run used `--stages 1 1 1 1 2 3 4 5 6 7 8 9 10`, `--lr .00002`,
`--entropy .003`, `--gamma .999`, `--gae .98`, `--reward-scale .1`,
`--guidance-batches 16`, `--guidance-lr .0001`,
`--guidance-nonfire-weight 4`, and `--guidance-safety-weight 2`, with seed 935.
Its existing tactical encoder and separate value encoder were preserved.
These changes were tested together, so the experiment does not isolate the
causal contribution of each change.

Third-round selection used 50 Stage 1 validation episodes (74000+) and 90
Stage 2–10 episodes (75000+), maximizing the two clear fractions' equal-weight
mean subject to a Stage 1 regression limit. The final multi-map checkpoint
was frozen before all final tests. Fresh test cohorts start at 140000,
150000 and 160000. Stages 6–10 now participate in training/model selection;
only Stages 11–15 in this test are untrained maps. All candidates and the
selection protocol remain in the third-round evidence folder.

The fourth round added 262,144 decisions with the previous configuration
(seed 1421), plus a separate 524,288-decision navigation-tutor experiment
(seed 1776). Both resumed the third-round selected checkpoint, preserving its
201,426-parameter actor and PPO optimizer. The navigation experiment performed
worse and was not selected. The selected control checkpoint has a cumulative
counter of 1,349,636 and used the third-round multimap demonstrations, stages,
learning rate and rehearsal settings listed above.

Fourth-round validation used Stage 1 seeds 78000–78049 and Stage 2–10 seeds
79000–79089. Two control and four navigation checkpoints were expanded after
screening, with the previous model retained as a fallback. Selection maximized
the equal-weight mean of the two clear fractions, subject to the same Stage 1
regression limit. The model was frozen before 720 final episodes comparing
new, previous and fixed-script policies. Final cohorts start at 180000,
190000 and 200000; only Stages 1–10 were used for training. The full protocol,
candidate results, source snapshots and hashes remain in the fourth-round
evidence folder. Its Stage 7 video is a curated validation win, not an estimate
of that stage's success rate.

The fifth round resumed the fourth-round weights in three independent
262,144-decision runs: unchanged defender guidance (seed 2394), moving-navigator
guidance (2865), and terrain guidance (3271). This totals 786,432 new decisions
and 505 completed training episodes. Each candidate used 40,000 new teacher
labels mixed with 40,000 sampled third-round defender labels. Candidate curricula
weighted Stages 7–9 twice, while rehearsal used eight batches, learning rate
.00005 and safety weight 3. The PPO learning rate remained .00002; the actor,
separate value encoder and saved PPO optimizer were preserved. Teacher/data,
curriculum and rehearsal strength changed together, not in a single-factor test.

Fifth-round validation used Stage 1 seeds 82000–82049 and Stage 2–10 seeds
83000–83089. After initial screening, two control and four experimental
checkpoints received the full validation cohorts. Selection maximized the
equal-weight mean of the two clear fractions. The planned gate required each
cohort's base survival to stay within ten percentage points of the previous
model. A post-run audit found the freeze script used Stage 1 clear fraction
instead of Stage 1 base survival for that gate. Recalculation from saved
validation data with the planned gate selects the same terrain final checkpoint;
the script and discrepancy are preserved in the evidence. Its cumulative counter is
1,611,780. It was frozen before 720 final episodes across new, previous and
fixed-script policies: Stage 1 seeds 220000–220099, Stage 2–10 seeds
230000–230089, and untrained Stage 21–25 seeds 240000–240049. Final tests were
not used to reselect weights. Its Stage 9 video is a curated validation win;
the preserved fifth-round report describes that run's broader limitations.

### Wider policies and broader curricula

`train --wide` optionally expands a tactical 128-feature/64-head policy to
256 features and 128-unit actor/value heads. The terrain convolution channels
increase from 8/16 to 16/32. `TankWideFeatures` retains the same structured
observation, local crop and tactical cues; its actor has 622,234 parameters
(2,488,936 bytes of FP32 weights). Existing units and connections are copied
into the larger tensors, and new connections into old units start at zero.
Migration preserves the initial action distribution and value estimate; the
optimizer starts fresh because its tensor shapes change. Ordinary continuation
of an already widened checkpoint preserves that checkpoint's PPO optimizer.
The original encoders and old checkpoint behavior remain available.

`train --context` adds two residual spatial blocks to an already widened
checkpoint, giving the terrain encoder four additional 3×3 convolutions before
flattening its 7×7 grid. Its actor has 659,226 parameters. The blocks' output
projections start at zero during migration, preserving the current policy;
their internal convolutions retain trainable initialization. Migration resets
the optimizer and is verified separately from subsequent learning performance.
The blocks are neural layers, with no path search or action rules at inference.

`train --projectiles` is a separate widened-policy experiment with 646,810 actor
parameters. It derives four relative-flight cues for each observed enemy shell:
along-flight distance, signed lateral offset, alignment and time to the player's
current along-flight position. These cues do not predict wall collisions or
future movement. A zero-initialized linear residual preserves actor/value outputs
when migrating from a widened checkpoint; migration resets the optimizer.
Observations and the action space stay unchanged, and no evasive rule is added.
This encoder cannot be combined with `--context`.

`--threads` controls PyTorch CPU threads during PPO training (default 2), and
`--checkpoint-steps` controls completed-update checkpoint spacing (default
65,536 decisions). These settings are recorded alongside encoder and source
hashes. Measure inference cost separately on a quiet CPU before choosing a
wider model for deployment.

`python -m training.distill --teacher PATH --student PATH --data PATH --output
build/ai-distilled` compresses a neural policy into an existing smaller policy.
The input archive uses the same `map`/`state` schema as imitation training, but
its scripted action labels are ignored. The frozen neural teacher supplies soft
action probabilities, and training minimizes KL divergence to the student's
distribution. This preserves stochastic behavior better than simply copying
argmax labels. The student must have a separate value encoder; only actor
parameters are updated. Teacher probabilities are cached once under the output
directory, and the teacher is absent at inference. Distillation counts label
presentations, not new PPO decisions. Saved policies retain the student's PPO
counter, with a fresh PPO optimizer for a later reinforcement-learning resume.
As with imitation accuracy, low training KL is not evidence of game-playing
quality: evaluate the saved model on the same native episodes before selecting it.

For broad training, `imitate --teacher-config PATH` and
`train --guidance-teacher-config PATH` optionally select a teacher once per
episode from a manifest. Its `schema` is 1 and its `maps` object maps an
initial-terrain SHA-256 fingerprint to an object containing `teacher` (one of
the existing teacher names). The first ten observation channels, rounded to
uint8 values, define the fingerprint. Unknown terrain uses the explicit
`--teacher` / `--guidance-teacher` fallback. Changes to terrain during an
episode do not switch teachers; resets discard the selected teacher and its
cache. The manifest and its hash are training artifacts, never policy inputs
or inference dependencies. Select its entries using training diagnostics,
without using final neural-policy test seeds.

`imitate --lr`, `--nonfire-weight` and `--safety-weight` control supervised
adaptation using the same conservative training-only headquarters firing loss
as rehearsal. Defaults preserve the original learning rate .0003, uniform
label weights and zero safety-loss weight. Cross-entropy, safety loss and label
accuracy are logged separately. `--checkpoint-epochs 4` retains a checkpoint
every four supervised epochs; its default 0 saves only the final checkpoint.
Supervised epochs and PPO decision steps are different training work and must
be reported separately.

`imitate --dagger --dagger-stochastic --model PATH` collects corrections while
sampling the learner's actions. As with ordinary DAgger collection, 75% of
executed actions come from the learner and 25% from the teacher; every saved
label comes from the teacher for the exact pre-action state. This option makes
collection match a sampled deployment policy, while the default DAgger mode
keeps deterministic learner actions. The collection seed, mode and source
model/teacher hashes are recorded with the new archive.

The completed sixth-round experiments covered all 35 original maps. Seventeen
PPO processes recorded 8,416 completed episodes and 12,582,912 new decisions
represented by completed or recovery checkpoints, after subtracting inherited
parent counters. Sixteen processes completed; one aborted and was recovered
in a separate process. Another 21,920 reported post-checkpoint decisions were
discarded at recovery. Supervised work was separate: 1.8 million hard-label
presentations and 2.4 million soft-label presentations across two distillations.

Twenty-seven candidates each received 350 all-map plus 100 Stage 1 validation
episodes and quiet CPU timing. The predeclared gates limited actor size,
latency, base-survival regression, Stage 1 clear regression and own-base damage.
Eligible candidates were ranked by 90% all-map clear fraction plus 10%
supplemental Stage 1 clear fraction. Higher-scoring candidates that failed
base-survival gates were retained as evidence and excluded, without rounding
or relaxing those gates. The small `distilled-hard-repair/step-1873924.zip`
checkpoint was frozen before all 1,350 reserved final episodes. Final results
did not change the selection. Exact plans, seeds, source/data/model hashes
and failures are under `build/release-evidence/ai-20260913-round6/`.

The original stochastic DAgger collection had an audited seed discrepancy:
loading a checkpoint restored PyTorch seed 5127 after the requested seed 9627.
The saved archive and original source snapshot are preserved with the actual
stream documented. The collector now seeds after loading, with a test of
repeated action trajectories; existing data was not silently relabeled.

### Optional navigation tutor

`imitate --teacher navigator` and `train --guidance-teacher navigator` select
an alternative training-only teacher. The default remains `defender`, and the
fixed `training.bot.Defender` evaluation benchmark is unchanged. The navigation
tutor searches every integer movement lane, including even coordinates allowed
by the production lane snap, and avoids shooting at enemies behind steel. When
already facing an enemy along a clear firing lane, it can stop and fire instead
of driving into the enemy. Its route costs account for destructible bricks and
the current boat ability. It uses the same observation as the learner.

This is a heuristic teacher, not an exact simulator or optimal policy. Its
cover checks conservatively treat steel as permanent even at higher weapon
levels, and it does not plan incoming-shell evasions. Teaching these decisions
does not guarantee the neural model will reproduce its navigation. The teacher
and data source hashes are recorded with each run. Rehearsal resets a separate
teacher at each environment's episode boundary; neither this path search nor
its firing rules execute when a trained checkpoint plays the game.

```sh
PYTHONDONTWRITEBYTECODE=1 build/ai-venv/bin/python -m training.imitate \
  --collect-only --teacher navigator --samples 120000 --seed 4832 \
  --stages 1 1 2 3 4 5 6 7 8 9 10 \
  --output build/ai-runs/navigator-demos
PYTHONDONTWRITEBYTECODE=1 build/ai-venv/bin/python -m training.train \
  --resume build/release-evidence/ai-20260912-round3/ai-player.zip \
  --output build/ai-runs/navigator-continuation --steps 524288 --seed 1776 \
  --stages 1 1 1 1 2 3 4 5 6 6 7 7 8 8 9 9 10 \
  --lr .00002 --entropy .003 --gamma .999 --gae .98 --reward-scale .1 \
  --separate-value --guidance-teacher navigator \
  --guidance-data build/ai-runs/navigator-demos/demonstrations.npz \
  --guidance-batches 16 --guidance-lr .0001 \
  --guidance-nonfire-weight 4 --guidance-safety-weight 2
```

Two additional training-only teacher choices provide more targeted guidance:
`moving-navigator` uses the navigator's route planning but converts a stopped
shot into a move-and-fire action in the same valid heading. `terrain` retains
the fixed defender's label unless its next move meets steel, water without a
boat, headquarters cover or the map boundary and the moving navigator offers
an unblocked alternative. Both component teachers observe every state, and
their route caches reset together at episode boundaries. The terrain tutor
keeps the original label on ice or during ice momentum.

The terrain check is an approximate teaching heuristic, not a replacement for
production collision detection. It checks the 0.875-tile tank footprint across
one decision's movement, allows destructible brick cover, and ignores other
tanks. Neither teacher runs during neural inference. The fifth-round candidate
experiments mix 40,000 new labels with 40,000 sampled old defender labels and
use fewer, smaller rehearsal updates to limit forgetting. Exact sample indices,
shuffle order, source hashes and training configurations are saved in the run
evidence; the fixed defender benchmark and neural architecture are unchanged.

## Observation and actions

The C++ adapter exports a versioned C ABI, loaded with Python `ctypes`.
`TanksEnv` implements the Gymnasium reset/step API; observations are owned copies
and remain valid after the next step. Each action advances **three separate
1/60-second production updates**, with the same direction-edge semantics as
player input. Rendering and wall-clock waiting are absent during training.
The real 3.2-second opening timer is advanced with neutral input during reset.

The 14×26×26 spatial channels are: four brick quadrants, steel, water, forest,
ice, headquarters wall health, headquarters core, player position, enemy armor,
enemy shells and pickups. The 256 scalar values include exact player state,
up to four active enemies, the 24 nearest flying shells, up to nine pickups,
and current public episode progress. Shell overflow is counted; the enemy-shell
map still marks all flying enemy rounds. No future spawn choices, enemy private
targets or RNG state are policy inputs. Full-map entity positions include
otherwise obscured/offscreen enemies; fair human comparison would need a
separate observation restriction.

Action `2 * direction + fire` has ten possibilities. Direction is
0=stop, 1=north, 2=south, 3=west, 4=east; fire is 0 or 1. A stop keeps the actual
tank's facing and ice behavior. Steering, collision, firing cooldown, shells,
upgrades and the original 35 maps remain production rules.

## Episode and reward contract

An episode covers one original stage. A win is recognized only when the actual
successful battle report starts, **after the live five-second stage-clear
period**, when lingering shells can still destroy the base. Game over is a
terminal loss. A time limit is `truncated`, not a win or a loss. Respawning is
part of the same episode. Reports distinguish a surviving base at timeout from
successfully clearing the stage.

The initial event rewards are +3 per destroyed enemy, +0.25 per nonfatal enemy
hit, +1 per collected pickup, -1 per player damage, -3 per player destruction,
and -2 per player-caused headquarters damage event. Clearing the stage gives
+50; losing the base gives -50, and losing all players with the base intact
-30. There is no brick-destruction or passive-survival reward. Raw kills,
deaths, own-base hits and outcome are retained separately from learning reward.

`training.train --extra-death-penalty 5` optionally adds a training-only cost
of five per actual player destruction, changing that event's contribution
from -3 to -8 **before** `--reward-scale`. Damage and terminal events remain
additive, so a complete step can include other rewards too. The default is
zero, which leaves the original wrapper path and rewards unchanged. This is
an intentional change to the learning objective, not a potential-based
transformation that preserves the optimal policy.

`training/rewards.py` applies the cost from the native cumulative destruction
count. It does not change observations, gameplay, random state, episode
boundaries, inference or other reward terms. There is no new passive-survival
bonus or timeout penalty. Training logs retain unscaled native, extra-death
and shaped totals separately, including episode totals; Monitor returns still
reflect the selected global reward scale. Evaluate all reward variants with
the original `training.evaluate` and compare clears, base survival, deaths and
timeouts. Shaped training return cannot be compared directly with native
return as a measure of playing strength.

PPO uses a small two-convolution terrain encoder, the scalar state, and separate
64-unit actor/value heads. The initial CPU configuration uses four environments,
512 decisions per rollout/environment, batches of 256, four optimizer epochs,
and a 0.995 discount per decision. Measure throughput before scaling compute.
There is no guarantee that the first checkpoint can clear a stage.

### First local experiment, 2026-09-12

The selected player-centered checkpoint was initialized from 60,000 scripted
decisions (24 supervised epochs), then updated with PPO. It was selected using
validation seeds 70000–70009 and sampled actions, before running the final test.
It contains 104,448 decisions of completed PPO optimizer updates; its snapshot
counter is 104,452 because collection of the next rollout had begun.

| Policy | Stage clears / 100 | Mean kills | Base alive at episode end |
| --- | ---: | ---: | ---: |
| Random actions | 0 | 2.01 | 1% |
| Learned BC + PPO | 0 | 9.75 | 18% |
| Scripted defender | 46 | 16.36 | 81% |

All three use seeds 100000–100099, Stage 1, three lives and a 120-second limit.
The learned policy is a working training prototype, with weak base defense;
it has not achieved reliable stage completion. On 20 separate episodes in
untrained Stages 2–5 it cleared zero stages and averaged 1.30 kills. The later
524,288-decision checkpoint performed worse on validation; more steps alone
did not produce a better policy in this run. Local weights, native video,
per-episode results and source/native hashes are in
`build/release-evidence/ai-20260912/REVIEW.md`.

## Evaluation and watching

Watch the sixth-round candidate in this workspace (a curated Stage 34 win):

```sh
make ai-native
PYTHONDONTWRITEBYTECODE=1 build/ai-venv/bin/python -m training.play \
  --model build/release-evidence/ai-20260913-round6/ai-player.zip \
  --stochastic --stage 34 --seed 270068 --seconds 120 \
  --trace build/ai-watch-round6.json
```

That checkpoint is a local development artifact, not a file distributed in the
repository or ordinary app bundle. Use a new trace filename for each run.
The previous model remains at
`build/release-evidence/ai-20260913-round5/ai-player.zip` for comparison.
For the recorded defense failure, use `--stage 1 --seed 410063` and a different
trace path. Both recordings were replayed independently and matched the
headless episode's native gameplay digest. The successful example is selected
from validation wins and does not represent the model's average success rate.

Use fixed, disjoint seed cohorts declared before each experiment. Current
training code samples episode seeds below 60000; demonstration seeds are in
[2000,60000). Sixth-round expanded validation used 270000–270349 and
280000–280099, while final tests used 400000–400349 and 410000–410099.
These final cohorts are now read; future selection must reserve new test seeds.
Earlier rounds used other recorded ranges above 70000. Keep unseen stages
separate from new seeds on trained maps. The initial PPO-only pilot predates
the explicit training seed range; its configuration is recorded in local evidence.

```sh
PYTHONDONTWRITEBYTECODE=1 build/ai-venv/bin/python -m training.evaluate \
  --policy ppo --model build/ai-runs/ppo-first/model.zip \
  --episodes 100 --seed-start 100000 --output build/ai-runs/ppo-test.json
PYTHONDONTWRITEBYTECODE=1 build/ai-venv/bin/python -m training.evaluate \
  --policy defender --episodes 100 --output build/ai-runs/defender-test.json
PYTHONDONTWRITEBYTECODE=1 build/ai-venv/bin/python -m training.evaluate \
  --policy random --episodes 100 --output build/ai-runs/random-test.json
PYTHONDONTWRITEBYTECODE=1 build/ai-venv/bin/python -m training.play \
  --model build/ai-runs/ppo-first/model.zip --seconds 30 \
  --trace build/ai-runs/play.json
```

`play` opens a real 960×540 game renderer using the normal camera, lighting,
models, effects and HUD. Add `--record build/ai-runs/play.mp4 --hidden` for
native-frame recording at 20 fps. The recorded action trace includes the seed,
model/native hashes and final gameplay digest. Use `--replay TRACE.json` in
place of `--model` to replay those actions and require the same final digest.
Only one viewer per Python process is supported; close it before opening another.

Both evaluation and playback default to the largest-probability action. Add
`--stochastic` to sample the policy using a fixed per-episode PyTorch seed.
These are different policies in practice: compare them on validation seeds,
choose one before final testing, and record the mode with every result. The
viewer has no audio. On macOS with a sleeping display, prefix the playback
command with `caffeinate -dimsu env` to keep a graphics surface available.

`evaluate --stages 2 3 4 5 --episodes 20` gives a separate unseen-stage report.
The random baseline holds actions for half a second. All policies use the same lives, stage, seed and
time limits. Evaluate success, deaths, base losses, friendly-base damage and
timeouts together, not just mean training reward.

## Computational efficiency

Computational efficiency is a requirement alongside playing strength. Keep
structured internal-state observations as the default input and record CPU
latency, actor size, whole-process memory and win rates together when changing
the policy. Offline training cost and in-game inference cost are separate.

The sixth-round selected model was benchmarked after all training and
validation jobs stopped: one CPU thread, 188 fixed real states, 256 warmups
and three repetitions of 4,096 sampled predictions. Mean prediction was
**0.295 ms**, with repeat p95 values **0.308–0.312 ms**. Including three native
simulation ticks and observation construction averaged **0.321 ms**, excluding
rendering. Prediction CPU time at 20 decisions/second corresponds to about
**0.59% of one core** on this machine, not total game CPU usage.

The 201,426-parameter actor occupies 805,704 FP32 parameter bytes. The full
checkpoint is 5,214,144 bytes and contains training state; measured whole-Python-
process peak RSS was 356,728,832 bytes (about 357 MB). The two memory figures
are different. Forward hooks confirmed that prediction calls only the actor
encoder, with no value encoder/head. All 27 candidate timing records and the
reproducible `benchmark.py` are in the sixth-round evidence folder. No native
lightweight inference runtime or normal-app AI integration was implemented.

The third-round selected model was measured locally on this arm64 Mac with one
CPU thread, warm inference, and the existing Python/SB3 sampling path. Across
three repetitions of 4,096 calls over 188 real gameplay observations, mean
inference time was 0.306–0.309 ms and p95 was 0.345–0.353 ms. This includes
observation conversion and action sampling, and excludes rendering. At 20
decisions per simulated second, measured process CPU time corresponds to about
6.1 ms per second, roughly 0.6% of one CPU core for one agent's decision work.
This estimate is specific to this machine and the measured warm path.

The actor has 201,426 parameters (805,704 bytes of FP32 weights). The full
training checkpoint is 5,215,862 bytes; it contains more than the actor.
Forward hooks confirmed that `predict()` does not execute the value encoder
or value head. The teacher and training losses do not run during inference.
Whole-process peak RSS during the benchmark was about 359 MB, including the
Python/PyTorch framework, full checkpoint and benchmark data; this is separate
from actor weight size. Native lightweight deployment is not yet implemented.
When integrating AI into the ordinary C++ game, retain the actor and required
observation processing, with training dependencies kept outside the app.

Raw results and a reproducible benchmark are under
`build/release-evidence/ai-efficiency-20260913/`. No weights or gameplay changed
during this measurement, and no pixel-input policy was benchmarked.

The fourth-round weights were measured again after all training, evaluation
and recording completed. Old and new weights used the same 188-observation
corpus, one CPU thread, 256 warmup calls and three 4,096-call repetitions.
New mean latency was 0.296–0.297 ms, p95 0.309–0.315 ms; the actor still has
201,426 parameters and uses about 0.6% of one CPU core at 20 decisions/second.
Whole-process peak RSS was about 355 MB for both models. Prediction plus three
production updates and observation extraction averaged 0.324 ms for the new
model. Raw results are in the fourth-round evidence folder. An initial corpus
loader accidentally retained repeated decompressions of the entire NPZ batch;
those inflated memory readings are explicitly superseded and preserved in
`benchmark-corpus-v1/`. The corrected benchmark loads each field only once.

The fifth-round candidate was measured after training, testing and recording,
using that same corrected benchmark and observation corpus. Mean inference
was **0.296–0.297 ms**, p95 **0.311–0.319 ms**; the previous weights measured
0.294–0.300 ms in the same session. This is comparable inference cost, with
the same 201,426 actor parameters and 9,720 input floats. The new prediction
plus three production updates and observation extraction averaged **0.325 ms**.
Whole-process peak RSS was about **356 MB** for both models. At 20 decisions
per second, the measured prediction CPU time remains about **0.6% of one core**
for one agent, excluding rendering. Forward hooks again confirmed no value
encoder/head calls during prediction. Raw results and `benchmark.py` are in
the fifth-round evidence folder; native app deployment remains unimplemented.

## Verification and provenance

```sh
make clean
make -j4 test
make ai-native test-ai
make test-sanitize test-ai-sanitize
```

The normal test target includes the C++ adapter's original-map and per-tick
production/RNG parity checks without needing Python. Python tests additionally
run Gymnasium's environment checker, deterministic trajectories, observation
ownership, close/reset guards, training-seed separation, terminal/truncation
handling, the live stage-clear grace period and checkpoint/action reproducibility.
Additional tests check actor/value gradient isolation, zero-residual migration,
and the alignment of correction labels across vectorized episode resets.

`src/training/native.cpp` temporarily compiles the existing main translation
unit under a renamed entry point, the same integration seam as the LAN game
test. It adds no method or state to Game3D and does not create a duplicate
simulator. Replace this seam when GameSession becomes a separately linkable
module; training alone does not require that wider refactor now.

Code and generated training data/checkpoints are project-owned under the root
license. No external game dataset or pretrained model is used. Training-only
libraries retain their upstream licenses; see `THIRD_PARTY_NOTICES.md`.
Local evidence is under `build/release-evidence/ai-20260912/` and is not an app
resource or release candidate.
