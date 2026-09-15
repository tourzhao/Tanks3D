"""Observation-only tactical rules; the historical Defender stays unchanged.

Reuse the project's integer-lane and firing-ray geometry from Navigator. Each
instance owns only its route/history; no native handle, seed or future state.
"""

import numpy as np

from training.bot import DIRECTIONS, Defender
from training.tutor import Navigator


class TacticalDefender:
    version = 1

    def __init__(self):
        self.reset()

    def reset(self):
        self.calls = 0
        self.route = []
        self.previous_position = None
        self.previous_action = 0
        self.stalled = 0
        self.next_plan = 0
        self.guardian = None
        self.last_reason = "reset"
        self.counts = dict.fromkeys(("plans", "aimed_fire", "guarding", "yielding", "recovery"), 0)

    @staticmethod
    def ally(state):
        if len(state) >= 340 and state[333] > .5:
            return state[256:272]
        return None

    @classmethod
    def home_role(cls, state):
        ally = cls.ally(state)
        if ally is None:
            return True
        x, z = state[:2] * 26
        ax, az = ally[:2] * 26
        own = 25 - z + abs(x - 13) * .15
        other = 25 - az + abs(ax - 13) * .15
        # Both self-relative observations make the same assignment. Slot is
        # only a tie breaker; whichever tank comes home can take over coverage.
        return bool(state[274] == 1) if abs(own - other) < 2 else own < other

    @staticmethod
    def predicted_position(state, direction):
        x, z = map(float, state[:2] * 26)
        dx, dz = DIRECTIONS[direction]
        if (dx, dz) != tuple(state[2:4]):
            if not dx and abs(x - round(x)) <= 5 / 16:
                x = float(round(x))
            elif not dz and abs(z - round(z)) <= 5 / 16:
                z = float(round(z))
        distance = .325 if state[6] > 0 else .25
        return x + dx * distance, z + dz * distance

    @classmethod
    def passable(cls, obs, direction, bricks=False):
        state, terrain = obs["state"], obs["map"]
        x, z = cls.predicted_position(state, direction)
        if min(x, z) < .875 or max(x, z) > 25.125:
            return False
        left, right = int(np.floor(x - .875)), int(np.ceil(x + .875))
        top, bottom = int(np.floor(z - .875)), int(np.ceil(z + .875))
        area = terrain[:, top:bottom, left:right]
        if area[4].any() or area[8:10].any() or (state[9] < .5 and area[5].any()):
            return False
        if bricks and area[:4].any():
            return False
        ally = cls.ally(state)
        if ally is not None and abs(x - ally[0] * 26) < 1.75 and abs(z - ally[1] * 26) < 1.75:
            return False
        return True

    def plan(self, obs, enemies, guardian):
        state, terrain = obs["state"], obs["map"]
        x, z = map(float, state[:2] * 26)
        start = int(np.clip(round(x), 1, 25)), int(np.clip(round(z), 1, 25))
        navigation = terrain.copy()
        ally = self.ally(state)
        if ally is not None:
            ax, az = ally[:2] * 26
            left, right = max(0, int(np.floor(ax - .875))), min(26, int(np.ceil(ax + .875)))
            top, bottom = max(0, int(np.floor(az - .875))), min(26, int(np.ceil(az + .875)))
            navigation[9, top:bottom, left:right] = 1
        distance, previous = Navigator.navigation(navigation, start, state[9] > .5)
        best, best_score = start, float("inf")
        for enemy in enemies:
            ex, ez = map(float, enemy[1:3] * 26)
            columns = [i for i in range(1, 26) if abs(i - ex) < .70]
            rows = [i for i in range(1, 26) if abs(i - ez) < .70]
            candidates = {(cx, rz) for cx in columns for rz in range(1, 26)}
            candidates.update((cx, rz) for rz in rows for cx in range(1, 26))
            for node in sorted(candidates):
                if node not in distance:
                    continue
                nx, nz = node
                gap = abs(nx - ex) + abs(nz - ez)
                direction = Navigator.firing_direction(nx, nz, ex, ez)
                if gap < 2.75 or not Navigator.clear_shot(terrain, nx, nz, ex, ez, direction):
                    continue
                score = distance[node] * .8 + gap * .12 - ez * .6
                if guardian:
                    score += max(0, 17 - nz) * 2 + max(0, ez - nz) * .3
                elif ally is not None and ez < 19:
                    ax, az = ally[:2] * 26
                    if abs(ax - ex) + abs(az - ez) + 3 < abs(x - ex) + abs(z - ez):
                        score += 3
                if enemy[10] > .5:
                    score += 3  # Frozen tanks can be shot, but do not advance.
                if score < best_score:
                    best, best_score = node, score
        if best_score == float("inf"):
            target = max(enemies, key=lambda e: e[2]) if len(enemies) else None
            gx = float(target[1] * 26) if target is not None else (11 if x < 13 else 15)
            gz = min(21, max(17 if guardian else 7, float(target[2] * 26) + 3)) if target is not None else 19
            best = min(distance, key=lambda n: abs(n[0] - gx) + abs(n[1] - gz) + .2 * distance[n])
        route = []
        while best != start:
            route.append(best)
            best = previous[best]
        self.route = route[::-1]
        self.counts["plans"] += 1
        self.next_plan = self.calls + 10

    def predict(self, obs):
        state, terrain = obs["state"], obs["map"]
        if state[8] < .5 or state[4] <= 0 or state[12] > .5 or state[13] > .5:
            self.last_reason = "unavailable"
            return 0
        self.calls += 1
        x, z = map(float, state[:2] * 26)
        if self.previous_position is not None:
            moved = abs(x - self.previous_position[0]) + abs(z - self.previous_position[1])
            self.stalled = self.stalled + 1 if self.previous_action // 2 and moved < .015 else 0
            if moved > 3:
                self.route, self.next_plan = [], 0
        self.previous_position = x, z
        guardian = self.home_role(state)
        self.counts["guarding"] += int(guardian)
        enemies = state[16:64].reshape(4, 12)
        enemies = enemies[(enemies[:, 0] > .5) & (enemies[:, 9] < .5)]
        urgent = max((float(e[2] * 26) for e in enemies if e[10] < .5), default=0)
        for enemy in sorted(enemies, key=lambda e: -e[2]):
            ex, ez = map(float, enemy[1:3] * 26)
            if guardian and urgent >= 18 and ez < urgent - 3:
                continue
            direction = Navigator.firing_direction(x, z, ex, ez)
            if not Navigator.clear_shot(terrain, x, z, ex, ez, direction):
                continue
            gap = abs(ex - x) + abs(ez - z)
            if gap >= 2.75 and tuple(state[2:4]) == DIRECTIONS[direction]:
                self.previous_action, self.last_reason = 1, "aimed_fire"
                self.counts["aimed_fire"] += 1
                return 1
            if gap >= 2.25:
                self.previous_action, self.last_reason = 2 * direction + 1, "turn_fire"
                return self.previous_action
        if guardian != self.guardian or self.calls >= self.next_plan or self.stalled >= 50:
            self.plan(obs, enemies, guardian)
            self.guardian = guardian
            if self.stalled >= 50:
                self.counts["recovery"] += 1
                self.stalled = 0
        while self.route:
            nx, nz = self.route[0]
            if abs(nx - x) < .17 and abs(nz - z) < .17:
                self.route.pop(0)
                continue
            if abs(nx - x) >= abs(nz - z):
                direction = 4 if nx > x else 3
            else:
                direction = 2 if nz > z else 1
            if not self.passable(obs, direction):
                self.next_plan = 0
                self.counts["yielding"] += 1
                self.previous_action, self.last_reason = int(Defender.safe_fire(x, z, self.heading(state))), "yield"
            else:
                self.previous_action = direction * 2 + int(Defender.safe_fire(x, z, direction))
                self.last_reason = "route"
            return self.previous_action
        self.previous_action = 1 if tuple(state[2:4]) == DIRECTIONS[1] else 3
        self.last_reason = "cover_lane"
        return self.previous_action

    @staticmethod
    def heading(state):
        return DIRECTIONS.index(tuple(state[2:4]))
