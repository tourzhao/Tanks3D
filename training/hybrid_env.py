"""Train neural proposals with the same fixed action rules used at inference.

PPO must retain its proposed action and that proposal's log probability in the
rollout buffer. The executed action is a transition of this wrapped environment,
not a new label for the policy. No teacher action is relabeled as neural output.
"""

import gymnasium as gym

from training.hybrid import HybridPolicy


class _Proposal:
    def __init__(self):
        self.action = 0

    def predict(self, observation, deterministic=True):
        return self.action, None


class HybridTrainingEnv(gym.Wrapper):
    """Optional action transform; observations, rewards and game rules stay intact.

    Training still computes a neural proposal on every step, including takeover.
    The runtime policy skips that work during takeover. Its route/hysteresis
    memory remains private to the rules; no new actor observation is introduced.
    """

    def __init__(self, env, mode="defense"):
        super().__init__(env)
        self.proposal = _Proposal()
        self.controller = HybridPolicy(self.proposal, mode)
        self._observation = None

    def reset(self, **kwargs):
        observation, info = self.env.reset(**kwargs)
        self.controller.reset()
        self._observation = {key: value.copy() for key, value in observation.items()}
        return observation, info

    def step(self, action):
        if self._observation is None:
            raise RuntimeError("Call reset before stepping a finished episode")
        # Validate before arbitration: takeover must not hide invalid proposals
        # or advance controller state for an action the native adapter rejects.
        if not self.action_space.contains(action):
            raise ValueError("Expected one discrete action in [0,9]")
        self.proposal.action = int(action)
        before_fire = self.controller.counts["fire_suppressions"]
        before_entries = self.controller.counts["defense_entries"]
        executed = self.controller.predict(self._observation)
        observation, reward, terminated, truncated, info = self.env.step(executed)
        self._observation = (
            None
            if terminated or truncated
            else {key: value.copy() for key, value in observation.items()}
        )
        info = {
            **info,
            "hybrid_action": {
                "proposed": int(action),
                "executed": executed,
                "defender": self.controller.defending,
                "fire_suppressed": self.controller.counts["fire_suppressions"] > before_fire,
                "defense_entry": self.controller.counts["defense_entries"] > before_entries,
            },
        }
        if terminated or truncated:
            stats = self.controller.statistics()
            # These count arbitration branches, not actual training forwards.
            stats["neural_branch_decisions"] = stats.pop("neural_calls")
            stats["defender_decisions"] = stats.pop("defender_calls")
            info["hybrid_training"] = stats
        return observation, reward, terminated, truncated, info

    def close(self):
        self._observation = None
        self.env.close()
