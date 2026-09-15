"""Optional observation-only navigation tutor, used for training labels only.

The historical Defender remains the fixed benchmark. This tutor searches all
integer movement lanes and refuses firing lines blocked by permanent cover.
It never receives native handles, random state or future simulation results.
"""

import hashlib
import heapq
import json
from pathlib import Path

import numpy as np

from training.bot import DIRECTIONS, Defender

TEACHER_NAMES = ("defender", "navigator", "moving-navigator", "terrain")


class Navigator:
    def __init__(self):
        self.reset()

    def reset(self):
        self.calls = 0
        self.route = []

    @staticmethod
    def firing_direction(x, z, ex, ez):
        if abs(ex - x) < 0.70:
            return 1 if ez < z else 2
        if abs(ez - z) < 0.70:
            return 3 if ex < x else 4
        return 0

    @staticmethod
    def clear_shot(terrain, x, z, ex, ez, direction):
        """Conservative shell-width ray; bricks can be cleared, steel cannot."""
        if not direction or not Defender.safe_fire(x, z, direction):
            return False
        dx, dz = DIRECTIONS[direction]
        mx, mz = x + dx * 0.625, z + dz * 0.625
        # Align the ray with the actual gun, rather than the enemy center.
        tx, tz = (x, ez) if dx == 0 else (ex, z)
        left, right = sorted((mx, tx))
        top, bottom = sorted((mz, tz))
        if dx == 0:
            left, right = x - 0.25, x + 0.25
        else:
            top, bottom = z - 0.25, z + 0.25
        col0, col1 = max(0, int(np.floor(left))), min(26, int(np.floor(right)) + 1)
        row0, row1 = max(0, int(np.floor(top))), min(26, int(np.floor(bottom)) + 1)
        return not (
            terrain[4, row0:row1, col0:col1].any() or terrain[8:10, row0:row1, col0:col1].any()
        )

    @staticmethod
    def navigation(terrain, start, boat):
        blocked = (terrain[4] > 0) | (terrain[8] > 0) | (terrain[9] > 0)
        if not boat:
            blocked |= terrain[5] > 0
        # A tank centered at integer (x,z) covers the adjacent 2x2 tiles.
        # Both odd and even centers are legal in the production lane snap.
        blocked = blocked[:-1, :-1] | blocked[1:, :-1] | blocked[:-1, 1:] | blocked[1:, 1:]
        bricks = terrain[:4].mean(axis=0)
        cost = 1 + 3 * (bricks[:-1, :-1] + bricks[1:, :-1] + bricks[:-1, 1:] + bricks[1:, 1:])
        distance, previous = {start: 0.0}, {}
        queue = [(0.0, start)]
        while queue:
            value, node = heapq.heappop(queue)
            if value != distance[node]:
                continue
            for dx, dz in DIRECTIONS[1:]:
                nx, nz = node[0] + dx, node[1] + dz
                if not (1 <= nx <= 25 and 1 <= nz <= 25) or blocked[nz - 1, nx - 1]:
                    continue
                candidate = value + float(cost[nz - 1, nx - 1])
                nxt = nx, nz
                if candidate < distance.get(nxt, float("inf")):
                    distance[nxt], previous[nxt] = candidate, node
                    heapq.heappush(queue, (candidate, nxt))
        return distance, previous

    def plan(self, terrain, x, z, enemies, boat):
        start = int(np.clip(round(x), 1, 25)), int(np.clip(round(z), 1, 25))
        distance, previous = self.navigation(terrain, start, boat)
        best, best_score = start, float("inf")
        for enemy in enemies:
            ex, ez = enemy[1:3] * 26
            # At most 50 aligned firing positions per enemy, not every
            # reachable cell. Prefer short routes against threats near HQ.
            columns = [i for i in range(1, 26) if abs(i - ex) < 0.70]
            rows = [i for i in range(1, 26) if abs(i - ez) < 0.70]
            candidates = {(cx, rz) for cx in columns for rz in range(1, 26)}
            candidates.update((cx, rz) for rz in rows for cx in range(1, 26))
            for node in sorted(candidates):
                if node not in distance:
                    continue
                nx, nz = node
                gap = abs(nx - ex) + abs(nz - ez)
                if gap < 2.5:
                    continue
                facing = self.firing_direction(nx, nz, ex, ez)
                if not self.clear_shot(terrain, nx, nz, ex, ez, facing):
                    continue
                score = distance[node] * 0.65 + gap * 0.12 - ez * 0.55
                if score < best_score:
                    best, best_score = node, score
        if best_score == float("inf"):
            target = max(enemies, key=lambda e: e[2]) if len(enemies) else None
            gx = float(target[1] * 26) if target is not None else 13.0
            gz = min(21.0, float(target[2] * 26) + 3) if target is not None else 19.0
            best = min(
                distance,
                key=lambda n: abs(n[0] - gx) + abs(n[1] - gz) + distance[n] * 0.1,
            )
        route = []
        while best != start:
            route.append(best)
            best = previous[best]
        return route[::-1]

    def predict(self, obs):
        state, terrain = obs["state"], obs["map"]
        x, z = state[:2] * 26
        enemies = state[16:64].reshape(4, 12)
        enemies = enemies[enemies[:, 0] > 0.5]
        self.calls += 1
        for enemy in sorted(enemies, key=lambda e: -e[2]):
            ex, ez = enemy[1:3] * 26
            facing = self.firing_direction(x, z, ex, ez)
            if self.clear_shot(terrain, x, z, ex, ez, facing):
                gap = abs(ex - x) + abs(ez - z)
                # A held heading can fire without driving into the target.
                # Ice still follows the actual game's momentum rules.
                if gap >= 2.5 and tuple(state[2:4]) == DIRECTIONS[facing]:
                    return 1
                return 2 * facing + 1
        if self.calls % 10 == 1 or not self.route:
            self.route = self.plan(terrain, x, z, enemies, state[9] > 0.5)
        while self.route:
            nx, nz = self.route[0]
            if abs(nx - x) < 0.15 and abs(nz - z) < 0.15:
                self.route.pop(0)
                continue
            if abs(nx - x) >= abs(nz - z):
                facing = 4 if nx > x else 3
            else:
                facing = 2 if nz > z else 1
            return 2 * facing + int(Defender.safe_fire(x, z, facing))
        return 3


class MovingNavigator(Navigator):
    """Ablation of the navigator's stopped-fire labels; not an inference helper."""

    def predict(self, obs):
        action = super().predict(obs)
        if action == 1:
            heading = tuple(obs["state"][2:4])
            return 2 * DIRECTIONS.index(heading) + 1
        return action


class TerrainTutor:
    """Retain Defender labels except for an immediately blocked terrain move.

    Both component teachers observe every state, keeping their route caches
    consistent. Collision checks are only an approximate training heuristic:
    they ignore brick cover, other tanks and movement during ice momentum.
    """

    def __init__(self):
        self.defender = Defender()
        self.navigator = MovingNavigator()

    def reset(self):
        self.defender.reset()
        self.navigator.reset()

    @staticmethod
    def blocked_ahead(obs, direction):
        if not direction:
            return False
        state, terrain = obs["state"], obs["map"]
        x, z = map(float, state[:2] * 26)
        dx, dz = DIRECTIONS[direction]
        if (dx, dz) != tuple(state[2:4]):
            # Match the integer-lane convention when changing direction.
            if dx == 0 and abs(x - round(x)) < 5 / 16:
                x = float(round(x))
            elif dz == 0 and abs(z - round(z)) < 5 / 16:
                z = float(round(z))
        distance = 0.325 if state[6] > 0 else 0.25
        x, z = x + dx * distance, z + dz * distance
        left, right, top, bottom = x - 0.875, x + 0.875, z - 0.875, z + 0.875
        if left < 0 or top < 0 or right > 26 or bottom > 26:
            return True
        # At exact contact the next tile does not overlap the strict AABB.
        c0, c1 = int(np.floor(left)), int(np.ceil(right))
        r0, r1 = int(np.floor(top)), int(np.ceil(bottom))
        return bool(
            terrain[4, r0:r1, c0:c1].any()
            or terrain[8:10, r0:r1, c0:c1].any()
            or (state[9] < 0.5 and terrain[5, r0:r1, c0:c1].any())
        )

    def predict(self, obs):
        original = self.defender.predict(obs)
        alternate = self.navigator.predict(obs)
        state = obs["state"]
        if state[14] > 0.5 or state[11] > 0:
            return original
        if self.blocked_ahead(obs, original // 2) and not self.blocked_ahead(
            obs, alternate // 2
        ):
            return alternate
        return original


def terrain_fingerprint(observation):
    """Identify the initial terrain for optional offline curriculum routing."""
    terrain = np.rint(observation["map"][:10] * 255).astype(np.uint8)
    return hashlib.sha256(terrain.tobytes()).hexdigest()


class CurriculumTutor:
    """Choose one teacher per episode from an explicit training-only manifest.

    Routing uses only the first observation's terrain. The manifest can be
    derived from separate teacher diagnostics; it is never loaded by a policy.
    Unknown terrain uses the caller's fixed fallback teacher.
    """

    def __init__(self, fallback, manifest):
        config = json.loads(Path(manifest).read_text())
        if (
            not isinstance(config, dict)
            or config.get("schema") != 1
            or not isinstance(config.get("maps"), dict)
        ):
            raise ValueError("Invalid teacher curriculum manifest")
        self.mapping = {}
        for fingerprint, entry in config["maps"].items():
            if (
                len(fingerprint) != 64
                or any(character not in "0123456789abcdef" for character in fingerprint)
                or not isinstance(entry, dict)
                or entry.get("teacher") not in TEACHER_NAMES
            ):
                raise ValueError("Invalid teacher curriculum mapping")
            self.mapping[fingerprint] = entry["teacher"]
        if fallback not in TEACHER_NAMES:
            raise ValueError("Invalid fallback teacher")
        self.fallback = fallback
        self.reset()

    def reset(self):
        self.teacher = None
        self.selected_name = None

    def predict(self, observation):
        if self.teacher is None:
            self.selected_name = self.mapping.get(
                terrain_fingerprint(observation), self.fallback
            )
            self.teacher = make_teacher(self.selected_name)
        return self.teacher.predict(observation)


def make_teacher(name, manifest=None):
    if manifest is not None:
        return CurriculumTutor(name, manifest)
    if name == "defender":
        return Defender()
    if name == "navigator":
        return Navigator()
    if name == "moving-navigator":
        return MovingNavigator()
    if name == "terrain":
        return TerrainTutor()
    raise ValueError(f"Unknown training teacher: {name}")
