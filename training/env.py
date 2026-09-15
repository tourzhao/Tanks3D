"""Gymnasium adapter over the unchanged, headless C++ game simulation."""

from __future__ import annotations

import ctypes as ct
from pathlib import Path

import gymnasium as gym
import numpy as np

ROOT = Path(__file__).resolve().parents[1]
CHANNELS, MAP_SIZE, STATE_SIZE = 14, 26, 256
OBS_SIZE = CHANNELS * MAP_SIZE * MAP_SIZE + STATE_SIZE
INFO_NAMES = (
    "reward",
    "terminated",
    "truncated",
    "ticks",
    "kills",
    "deaths",
    "base_alive",
    "won",
    "shots",
    "own_base_hits",
    "score",
    "stage",
    "seconds",
    "pickups",
    "reserved0",
    "reserved1",
)


def load_native(path=None):
    path = Path(path or ROOT / "build/ai/libtanks3d_training.dylib").resolve()
    if not path.is_file():
        raise FileNotFoundError(f"Build the training bridge first: make ai-native ({path})")
    lib = ct.CDLL(str(path))
    floats = np.ctypeslib.ndpointer(dtype=np.float32, ndim=1, flags="C_CONTIGUOUS")
    doubles = np.ctypeslib.ndpointer(dtype=np.float64, ndim=1, flags="C_CONTIGUOUS")
    lib.t3ai_error.restype = ct.c_char_p
    lib.t3ai_create.argtypes, lib.t3ai_create.restype = [ct.c_char_p], ct.c_void_p
    lib.t3ai_destroy.argtypes, lib.t3ai_destroy.restype = [ct.c_void_p], None
    lib.t3ai_reset.argtypes = [
        ct.c_void_p,
        ct.c_uint32,
        ct.c_int,
        ct.c_int,
        ct.c_int,
        floats,
        doubles,
    ]
    lib.t3ai_step.argtypes = [ct.c_void_p, ct.c_int, floats, doubles]
    lib.t3ai_digest.argtypes, lib.t3ai_digest.restype = [ct.c_void_p], ct.c_uint64
    lib.t3ai_render.argtypes = [
        ct.c_void_p,
        np.ctypeslib.ndpointer(dtype=np.uint8, ndim=3, flags="C_CONTIGUOUS"),
        ct.c_int,
        ct.c_int,
        ct.c_int,
    ]
    lib.t3ai_close_viewer.argtypes, lib.t3ai_close_viewer.restype = [], None
    if lib.t3ai_version() != 1 or lib.t3ai_observation_size() != OBS_SIZE:
        raise RuntimeError("Training ABI does not match this Python adapter")
    return lib


class TanksEnv(gym.Env):
    metadata = {"render_modes": ["rgb_array", "human"], "render_fps": 20}

    def __init__(
        self,
        stages=(1,),
        lives=3,
        max_seconds=120,
        render_mode=None,
        native_path=None,
        seed_limit=None,
    ):
        super().__init__()
        if render_mode not in (None, "rgb_array", "human"):
            raise ValueError("Unknown render mode")
        self.stages = tuple(stages)
        if not self.stages or any(not 1 <= s <= 35 for s in self.stages):
            raise ValueError("Stages must be in [1,35]")
        if not 1 <= lives <= 99 or not 0.05 <= max_seconds <= 3600:
            raise ValueError("Invalid episode limits")
        self.lives, self.max_ticks = lives, int(max_seconds * 60)
        self.seed_limit = 2**32 if seed_limit is None else seed_limit
        if not isinstance(self.seed_limit, int) or not 1 <= self.seed_limit <= 2**32:
            raise ValueError("Seed limit must be in [1,2**32]")
        self.render_mode = render_mode
        self.lib = load_native(native_path)
        self.handle = self.lib.t3ai_create(str(ROOT / "resources").encode())
        if not self.handle:
            raise RuntimeError(self.lib.t3ai_error().decode())
        self._obs = np.empty(OBS_SIZE, dtype=np.float32)
        self._info = np.empty(16, dtype=np.float64)
        self.action_space = gym.spaces.Discrete(10)
        self.observation_space = gym.spaces.Dict(
            {
                "map": gym.spaces.Box(
                    0, 1, shape=(CHANNELS, MAP_SIZE, MAP_SIZE), dtype=np.float32
                ),
                "state": gym.spaces.Box(-16, 16, shape=(STATE_SIZE,), dtype=np.float32),
            }
        )
        self.episode_seed = None
        self._ended = True
        self._rendered = False

    def _check(self, result):
        if result != 0:
            raise RuntimeError(self.lib.t3ai_error().decode())

    def _observation(self):
        # Copies are required: a subsequent native step must not overwrite a
        # rollout already retained by the learner.
        return {
            "map": self._obs[:-STATE_SIZE].reshape(CHANNELS, MAP_SIZE, MAP_SIZE).copy(),
            "state": self._obs[-STATE_SIZE:].copy(),
        }

    def _information(self):
        return {
            **{name: float(value) for name, value in zip(INFO_NAMES[:14], self._info[:14])},
            "episode_seed": self.episode_seed,
        }

    def reset(self, *, seed=None, options=None):
        super().reset(seed=seed)
        if not self.handle:
            raise RuntimeError("Environment is closed")
        options = options or {}
        self.episode_seed = int(
            seed if seed is not None else self.np_random.integers(0, self.seed_limit)
        )
        if not 0 <= self.episode_seed < 2**32:
            raise ValueError("Seed must fit uint32")
        stage = int(options.get("stage", self.np_random.choice(self.stages)))
        self._check(
            self.lib.t3ai_reset(
                self.handle,
                self.episode_seed,
                stage,
                self.lives,
                self.max_ticks,
                self._obs,
                self._info,
            )
        )
        self._ended = False
        return self._observation(), self._information()

    def step(self, action):
        if self._ended:
            raise RuntimeError("Call reset before stepping a finished episode")
        if not self.action_space.contains(action):
            raise ValueError("Expected one discrete action in [0,9]")
        self._check(self.lib.t3ai_step(self.handle, int(action), self._obs, self._info))
        terminated, truncated = bool(self._info[1]), bool(self._info[2])
        self._ended = terminated or truncated
        return (
            self._observation(),
            float(self._info[0]),
            terminated,
            truncated,
            self._information(),
        )

    def digest(self):
        if not self.handle or self.episode_seed is None:
            raise RuntimeError("No active episode")
        return f"{self.lib.t3ai_digest(self.handle):016x}"

    def render(self):
        if self.render_mode is None:
            return None
        frame = np.empty((540, 960, 4), dtype=np.uint8)
        self._check(
            self.lib.t3ai_render(
                self.handle, frame, 960, 540, int(self.render_mode == "rgb_array")
            )
        )
        self._rendered = True
        return frame[:, :, :3].copy()

    def close(self):
        if getattr(self, "_rendered", False):
            self.lib.t3ai_close_viewer()
            self._rendered = False
        if getattr(self, "handle", None):
            self.lib.t3ai_destroy(self.handle)
            self.handle = None
        self._ended = True
