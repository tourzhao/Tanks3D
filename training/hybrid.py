"""Optional observation-only arbitration between a learned actor and fixed rules."""

from training.bot import Defender

HYBRID_MODES = ("base-guard", "defense")
COUNTERS = (
    "decisions",
    "neural_calls",
    "defender_calls",
    "fire_suppressions",
    "rule_decisions",
    "defense_entries",
)


def guard_fire(obs, action):
    """Strip firing into conservative HQ corridors, preserving movement.

    This uses the fixed defender's cover-independent heuristic, not a shell
    collision forecast. A stopped shot retains the observed cardinal heading.
    """
    if not action % 2:
        return action
    state = obs["state"]
    direction = action // 2
    if direction == 0:
        dx, dz = state[2:4]
        direction = 4 if dx > 0 else 3 if dx < 0 else 2 if dz > 0 else 1
    x, z = float(state[0]) * 26, float(state[1]) * 26
    return action if Defender.safe_fire(x, z, direction) else action - 1


class HybridPolicy:
    """Use the existing actor normally; optionally hand urgent defense to rules.

    The existing 20 Hz action contract and model weights remain unchanged.
    Reset this stateful wrapper on every episode, like the fixed Defender.
    No network call is made during a takeover. Counters distinguish choosing a
    rule action from suppressing its fire bit, without double-counting a step.
    """

    def __init__(self, model, mode="defense"):
        if mode not in HYBRID_MODES:
            raise ValueError(f"Unknown hybrid mode: {mode}")
        self.model = model
        self.mode = mode
        self.defender = Defender()
        self.reset()

    def reset(self):
        self.defending = False
        self.defender.reset()
        self.counts = dict.fromkeys(COUNTERS, 0)

    def configuration(self):
        return {
            "version": 1,
            "mode": self.mode,
            "headquarters_center": [13, 25],
            "enter_radius": 10,
            "leave_radius": 12,
            "fire_guard": "fixed-defender-corridors-including-stopped-heading",
            "ignores_cover": True,
        }

    def statistics(self):
        denominator = max(1, self.counts["decisions"])
        return {
            **self.counts,
            "rule_fraction": self.counts["rule_decisions"] / denominator,
            "defender_fraction": self.counts["defender_calls"] / denominator,
            "fire_suppression_fraction": self.counts["fire_suppressions"] / denominator,
        }

    def predict(self, obs, deterministic=True):
        state = obs["state"]
        enemies = state[16:64].reshape(4, 12)
        eligible = None
        defending = False
        if self.mode == "defense" and state[8] > 0.5 and not (state[12] or state[13]):
            eligible = (enemies[:, 0] > 0.5) & (enemies[:, 9] < 0.5) & (enemies[:, 10] < 0.5)
            radius = 12 if self.defending else 10
            distance_sq = (enemies[:, 1] * 26 - 13) ** 2 + (enemies[:, 2] * 26 - 25) ** 2
            defending = bool((eligible & (distance_sq <= radius**2)).any())
        if defending != self.defending:
            self.defender.reset()
            self.counts["defense_entries"] += int(defending)
        self.defending = defending
        self.counts["decisions"] += 1
        if defending:
            # Do not mutate the learner's observation or treat spawning/frozen
            # enemies as the emergency target. Map and live eligible slots stay
            # identical; the baseline Defender itself is deliberately unchanged.
            teacher_state = state.copy()
            teacher_state[16:64].reshape(4, 12)[~eligible, 0] = 0
            action = self.defender.predict({"map": obs["map"], "state": teacher_state})
            self.counts["defender_calls"] += 1
        else:
            action = int(self.model.predict(obs, deterministic=deterministic)[0])
            self.counts["neural_calls"] += 1
        guarded = guard_fire(obs, action)
        suppressed = guarded != action
        self.counts["fire_suppressions"] += int(suppressed)
        self.counts["rule_decisions"] += int(defending or suppressed)
        return guarded
