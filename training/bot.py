"""A fixed, observation-only defender used as the evaluation baseline."""

from __future__ import annotations
import heapq
import numpy as np

DIRECTIONS = ((0, 0), (0, -1), (0, 1), (-1, 0), (1, 0))


class Defender:
    def __init__(self):
        self.goal = None
        self.calls = 0
        self.route = []

    def reset(self):
        self.goal, self.calls, self.route = None, 0, []

    def predict(self, obs):
        s, terrain = obs["state"], obs["map"]
        x, z = s[:2] * 26
        self.calls += 1
        enemies = s[16:64].reshape(4, 12)
        enemies = enemies[enemies[:, 0] > 0.5]
        # An aligned tank can be engaged immediately. Keep firing through
        # destructible cover, but never aim a shot into the headquarters.
        facing = 1
        target = None
        if len(enemies):
            target = max(enemies, key=lambda e: e[2] * 26 - 0.12 * abs(e[1] * 26 - x))
            ex, ez = target[1:3] * 26
            if abs(ex - x) < 0.70:
                facing = 1 if ez < z else 2
                if self.safe_fire(x, z, facing):
                    return facing * 2 + 1
            if abs(ez - z) < 0.70 and ez < 22:
                facing = 3 if ex < x else 4
                if self.safe_fire(x, z, facing):
                    return facing * 2 + 1
        # Intercept the enemy nearest the base; absent a threat, patrol its
        # approach. Search the original 2x2 movement lanes, paying for bricks.
        target_x = target[1] * 26 if target is not None else 13
        target_z = min(21, max(5, target[2] * 26 + 3)) if target is not None else 19
        goal = (
            int(np.clip(round((target_x - 1) / 2), 0, 12)),
            int(np.clip(round((target_z - 1) / 2), 0, 12)),
        )
        start = (
            int(np.clip(round((x - 1) / 2), 0, 12)),
            int(np.clip(round((z - 1) / 2), 0, 12)),
        )
        if self.calls % 8 == 1 or self.goal != goal or not self.route:
            self.route = self.path(terrain, start, goal, s[9] > 0.5)
            self.goal = goal
        while self.route:
            nx, nz = self.route[0][0] * 2 + 1, self.route[0][1] * 2 + 1
            if abs(nx - x) < 0.15 and abs(nz - z) < 0.15:
                self.route.pop(0)
                continue
            # Center on the cross lane before committing a perpendicular turn.
            if abs(nx - x) >= abs(nz - z):
                facing = 4 if nx > x else 3
            else:
                facing = 2 if nz > z else 1
            break
        return facing * 2 + int(self.safe_fire(x, z, facing))

    @staticmethod
    def safe_fire(x, z, direction):
        if direction == 2 and 10.5 < x < 15.5:
            return False
        if direction == 3 and z > 22.5 and x > 11:
            return False
        if direction == 4 and z > 22.5 and x < 15:
            return False
        return True

    @staticmethod
    def path(terrain, start, goal, boat):
        blocked = np.maximum(terrain[4], terrain[9])
        blocked = np.maximum(blocked, terrain[8] > 0)
        if not boat:
            blocked = np.maximum(blocked, terrain[5])
        brick = terrain[:4].max(axis=0)
        queue, costs, previous = [(0.0, start)], {start: 0.0}, {}
        best = start
        while queue:
            cost, node = heapq.heappop(queue)
            if cost != costs[node]:
                continue
            if abs(node[0] - goal[0]) + abs(node[1] - goal[1]) < abs(best[0] - goal[0]) + abs(
                best[1] - goal[1]
            ):
                best = node
            if node == goal:
                best = node
                break
            for dx, dz in DIRECTIONS[1:]:
                nxt = node[0] + dx, node[1] + dz
                col, row = nxt[0] * 2, nxt[1] * 2
                if not (0 <= nxt[0] < 13 and 0 <= nxt[1] < 13):
                    continue
                if blocked[row : row + 2, col : col + 2].max() > 0:
                    continue
                value = cost + 1 + 5 * brick[row : row + 2, col : col + 2].mean()
                if value < costs.get(nxt, float("inf")):
                    costs[nxt], previous[nxt] = value, node
                    heapq.heappush(queue, (value, nxt))
        route = []
        while best != start:
            route.append(best)
            best = previous[best]
        return route[::-1]
