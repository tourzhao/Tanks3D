"""Optional training-only reward terms over the unchanged native game."""

import math

import gymnasium as gym


class DeathPenalty(gym.Wrapper):
    """Add a cost per actual player destruction; never reward merely staying alive.

    Native rewards and outcome information remain available for auditing. This
    deliberately changes the learning objective, not the game's scoring or rules.
    It is not potential-based shaping or an optimal-policy-preserving transform.
    """

    def __init__(self, env, extra_penalty):
        super().__init__(env)
        if not math.isfinite(extra_penalty) or extra_penalty < 0:
            raise ValueError("Extra death penalty must be finite and nonnegative")
        self.extra_penalty = float(extra_penalty)
        self._deaths = None
        self.totals = {}

    def reset(self, **kwargs):
        observation, info = self.env.reset(**kwargs)
        self._deaths = info["deaths"]
        self.totals = dict.fromkeys(("native", "extra_death", "shaped"), 0.0)
        return observation, info

    def step(self, action):
        if self._deaths is None:
            raise RuntimeError("Call reset before stepping a finished episode")
        observation, reward, terminated, truncated, info = self.env.step(action)
        deaths = info["deaths"] - self._deaths
        if not math.isfinite(deaths) or deaths < 0 or not float(deaths).is_integer():
            raise RuntimeError("Native cumulative death count must not decrease")
        self._deaths = info["deaths"]
        extra = -self.extra_penalty * deaths
        terms = {"native": float(reward), "extra_death": extra, "shaped": reward + extra}
        for key, value in terms.items():
            self.totals[key] += value
        info = {**info, "reward_terms": terms}
        if terminated or truncated:
            info["episode_reward_terms"] = self.totals.copy()
            self._deaths = None
        return observation, terms["shaped"], terminated, truncated, info

    def close(self):
        self._deaths = None
        self.env.close()
