"""One real two-player world, joint actions, symmetric team reward (coop ABI v1)."""

import ctypes as ct

import gymnasium as gym
import numpy as np

from training.env import ROOT, INFO_NAMES, load_native

CHANNELS, STATE_SIZE = 15, 384
OBS_SIZE, INFO_SIZE = CHANNELS * 676 + STATE_SIZE, 40
PLAYER_INFO = ("kills", "deaths", "shots", "pickups", "own_base_hits", "score",
               "lives", "active", "ready", "eliminated")


def player_view(observation, slot, legacy=False):
    return {"map": observation["map"][slot, :14 if legacy else CHANNELS],
            "state": observation["state"][slot, :256 if legacy else STATE_SIZE]}


class CoopEnv(gym.Env):
    metadata = {"render_modes": ["rgb_array", "human"], "render_fps": 20}

    def __init__(self, stages=(1,), lives=3, max_seconds=120, render_mode=None,
                 native_path=None, seed_limit=2**32):
        super().__init__()
        self.stages = tuple(stages)
        if not self.stages or any(not 1 <= s <= 35 for s in self.stages):
            raise ValueError("Stages must be in [1,35]")
        if not 1 <= lives <= 99 or not .05 <= max_seconds <= 3600:
            raise ValueError("Invalid episode limits")
        if render_mode not in (None, "rgb_array", "human"):
            raise ValueError("Unknown render mode")
        if not isinstance(seed_limit, int) or not 1 <= seed_limit <= 2**32:
            raise ValueError("Invalid seed limit")
        self.lives, self.max_ticks = lives, int(max_seconds * 60)
        self.seed_limit, self.render_mode = seed_limit, render_mode
        self.lib = load_native(native_path)
        floats = np.ctypeslib.ndpointer(dtype=np.float32, ndim=1, flags="C_CONTIGUOUS")
        doubles = np.ctypeslib.ndpointer(dtype=np.float64, ndim=1, flags="C_CONTIGUOUS")
        self.lib.t3coop_create.argtypes, self.lib.t3coop_create.restype = [ct.c_char_p], ct.c_void_p
        self.lib.t3coop_destroy.argtypes, self.lib.t3coop_destroy.restype = [ct.c_void_p], None
        self.lib.t3coop_reset.argtypes = [ct.c_void_p, ct.c_uint32, ct.c_int, ct.c_int,
                                        ct.c_int, floats, doubles]
        self.lib.t3coop_step.argtypes = [ct.c_void_p, ct.c_int, ct.c_int, floats, doubles]
        self.lib.t3coop_digest.argtypes, self.lib.t3coop_digest.restype = [ct.c_void_p], ct.c_uint64
        self.lib.t3coop_render.argtypes = [ct.c_void_p,
            np.ctypeslib.ndpointer(dtype=np.uint8, ndim=3, flags="C_CONTIGUOUS"),
            ct.c_int, ct.c_int, ct.c_int]
        if (self.lib.t3coop_version() != 1 or self.lib.t3coop_observation_size() != OBS_SIZE
                or self.lib.t3coop_info_size() != INFO_SIZE):
            raise RuntimeError("Cooperative ABI mismatch")
        self._obs = np.empty(2 * OBS_SIZE, np.float32)
        self._info = np.empty(INFO_SIZE, np.float64)
        self.action_space = gym.spaces.MultiDiscrete([10, 10])
        self.observation_space = gym.spaces.Dict({
            "map": gym.spaces.Box(0, 1, (2, CHANNELS, 26, 26), np.float32),
            "state": gym.spaces.Box(-16, 16, (2, STATE_SIZE), np.float32)})
        self.episode_seed = None
        self._ended, self._rendered = True, False
        self.handle = self.lib.t3coop_create(str(ROOT / "resources").encode())
        if not self.handle:
            raise RuntimeError(self.lib.t3ai_error().decode())

    def _check(self, result):
        if result != 0:
            raise RuntimeError(self.lib.t3ai_error().decode())

    def _observation(self):
        values = self._obs.reshape(2, OBS_SIZE)
        return {"map": values[:, :CHANNELS * 676].reshape(2, CHANNELS, 26, 26).copy(),
                "state": values[:, CHANNELS * 676:].copy()}

    def _information(self):
        names = [*INFO_NAMES[:14], *(f"p{slot + 1}_{key}" for slot in range(2) for key in PLAYER_INFO)]
        return {**{k: float(v) for k, v in zip(names, self._info)}, "episode_seed": self.episode_seed}

    def reset(self, *, seed=None, options=None):
        super().reset(seed=seed)
        if not self.handle:
            raise RuntimeError("Environment is closed")
        options = options or {}
        self.episode_seed = int(seed if seed is not None else self.np_random.integers(self.seed_limit))
        if not 0 <= self.episode_seed < 2**32:
            raise ValueError("Seed must fit uint32")
        stage = int(options["stage"] if "stage" in options else self.np_random.choice(self.stages))
        self._check(self.lib.t3coop_reset(self.handle, self.episode_seed, stage, self.lives,
                                        self.max_ticks, self._obs, self._info))
        self._ended = False
        return self._observation(), self._information()

    def step(self, actions):
        if self._ended:
            raise RuntimeError("Reset before stepping")
        actions = np.asarray(actions)
        if (actions.shape != (2,) or not np.issubdtype(actions.dtype, np.integer)
                or not self.action_space.contains(actions)):
            raise ValueError("Expected two integer actions in [0,9]")
        self._check(self.lib.t3coop_step(self.handle, int(actions[0]), int(actions[1]), self._obs, self._info))
        terminated, truncated = bool(self._info[1]), bool(self._info[2])
        self._ended = terminated or truncated
        return self._observation(), float(self._info[0]), terminated, truncated, self._information()

    def digest(self):
        if not self.handle or self.episode_seed is None:
            raise RuntimeError("No active episode")
        return f"{self.lib.t3coop_digest(self.handle):016x}"

    def render(self):
        if self.render_mode is None:
            return None
        frame = np.empty((540, 960, 4), np.uint8)
        self._check(self.lib.t3coop_render(self.handle, frame, 960, 540,
                                         int(self.render_mode == "rgb_array")))
        self._rendered = True
        return frame[:, :, :3].copy()

    def close(self):
        if getattr(self, "_rendered", False):
            self.lib.t3ai_close_viewer()
            self._rendered = False
        if getattr(self, "handle", None):
            self.lib.t3coop_destroy(self.handle)
            self.handle = None
