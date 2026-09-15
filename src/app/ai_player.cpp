#include "app/ai_player.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <queue>

namespace tanks3d::app
{
namespace
{
constexpr int kSize = 26;
constexpr std::array<std::pair<int, int>, 5> kDirections{{
    {0, 0}, {0, -1}, {0, 1}, {-1, 0}, {1, 0}}};

float coordinate(float value) { return value * 26.0f; }

int nearestEven(double value)
{
    const int lower = static_cast<int>(std::floor(value));
    const double fraction = value - lower;
    return lower + (fraction > .5 || (fraction == .5 && lower % 2 != 0));
}

bool ready(const game::Player &player)
{
    return player.active && player.hitPoints > 0 && player.creationTimer <= 0;
}

void playerState(const game::Player &player, float *state)
{
    const auto facing = core::cardinalVector(player.driveDirection);
    const float values[]{player.position.x / 26, player.position.z / 26,
        facing.x, facing.z, player.hitPoints / 6.0f, player.lives / 10.0f,
        player.level / 3.0f, player.fireCooldown / .12f,
        player.active ? 1.0f : 0.0f, player.hasBoat ? 1.0f : 0.0f,
        std::min(1.0f, player.shieldTimer / 10), player.iceSlipTimer / .38f,
        player.creationTimer > 0 ? 1.0f : 0.0f,
        player.respawnTimer > 0 ? 1.0f : 0.0f,
        player.onIce ? 1.0f : 0.0f, player.moving ? 1.0f : 0.0f};
    std::copy(std::begin(values), std::end(values), state);
}

int heading(const AiObservation &observation)
{
    for (int direction = 1; direction <= 4; ++direction)
        if (observation.state[2] == kDirections[direction].first &&
            observation.state[3] == kDirections[direction].second)
            return direction;
    return 1;
}

bool homeRole(const AiObservation &observation)
{
    const auto &s = observation.state;
    if (s[333] <= .5f)
        return true;
    const float own = 25.0f - coordinate(s[1]) +
                      std::abs(coordinate(s[0]) - 13.0f) * .15f;
    const float other = 25.0f - coordinate(s[257]) +
                        std::abs(coordinate(s[256]) - 13.0f) * .15f;
    return std::abs(own - other) < 2.0f ? s[274] == 1.0f : own < other;
}

int firingDirection(double x, double z, double ex, double ez)
{
    if (std::abs(ex - x) < .70)
        return ez < z ? 1 : 2;
    if (std::abs(ez - z) < .70)
        return ex < x ? 3 : 4;
    return 0;
}

bool clearShot(const AiObservation &observation, double x, double z,
               double ex, double ez, int direction)
{
    if (!direction || !TacticalAi::safeFire(x, z, direction))
        return false;
    const auto [dx, dz] = kDirections[direction];
    const double mx = x + dx * .625, mz = z + dz * .625;
    const double left = dx == 0 ? x - .25 : std::min(mx, ex);
    const double right = dx == 0 ? x + .25 : std::max(mx, ex);
    const double top = dx == 0 ? std::min(mz, ez) : z - .25;
    const double bottom = dx == 0 ? std::max(mz, ez) : z + .25;
    for (int row = std::max(0, static_cast<int>(std::floor(top)));
         row < std::min(26, static_cast<int>(std::floor(bottom)) + 1); ++row)
        for (int col = std::max(0, static_cast<int>(std::floor(left)));
             col < std::min(26, static_cast<int>(std::floor(right)) + 1); ++col)
            if (observation.cell(4, row, col) || observation.cell(8, row, col) ||
                observation.cell(9, row, col))
                return false;
    return true;
}

bool passable(const AiObservation &observation, int direction)
{
    const auto &s = observation.state;
    double x = coordinate(s[0]), z = coordinate(s[1]);
    const auto [dx, dz] = kDirections[direction];
    if (direction != heading(observation))
    {
        if (!dx && std::abs(x - nearestEven(x)) <= 5.0 / 16)
            x = nearestEven(x);
        else if (!dz && std::abs(z - nearestEven(z)) <= 5.0 / 16)
            z = nearestEven(z);
    }
    const double distance = s[6] > 0 ? .325 : .25;
    x += dx * distance;
    z += dz * distance;
    if (std::min(x, z) < .875 || std::max(x, z) > 25.125)
        return false;
    for (int row = static_cast<int>(std::floor(z - .875));
         row < static_cast<int>(std::ceil(z + .875)); ++row)
        for (int col = static_cast<int>(std::floor(x - .875));
             col < static_cast<int>(std::ceil(x + .875)); ++col)
            if (observation.cell(4, row, col) || observation.cell(8, row, col) ||
                observation.cell(9, row, col) ||
                (s[9] < .5f && observation.cell(5, row, col)))
                return false;
    // NumPy's scalar promotion keeps these ally-distance comparisons float32.
    if (s[333] > .5f &&
        std::abs(static_cast<float>(x) - coordinate(s[256])) < 1.75f &&
        std::abs(static_cast<float>(z) - coordinate(s[257])) < 1.75f)
        return false;
    return true;
}
} // namespace

AiObservation observeAiPlayer(const game::StageMap &map,
                              const std::vector<game::Player> &players,
                              const std::vector<game::Enemy> &enemies, int slot)
{
    AiObservation observation;
    if (slot < 0 || slot >= static_cast<int>(players.size()) || slot > 1)
        return observation;
    for (int row = 0; row < kSize; ++row)
        for (int col = 0; col < kSize; ++col)
        {
            const auto cell = [&](int channel, float value)
            { observation.terrain[channel * AiObservation::kCells + row * kSize + col] = value; };
            const auto mask = map.brickMask(row, col);
            for (int bit = 0; bit < 4; ++bit)
                cell(bit, (mask & (1 << bit)) ? 1.0f : 0.0f);
            const char tile = map.tile(row, col);
            cell(4, tile == '@');
            cell(5, tile == '~');
            cell(6, tile == '%');
            cell(7, tile == '-');
            const int wall = game::governmentWallIndexForCell(row, col);
            if (wall >= 0)
            {
                cell(8, map.governmentWallHealth(wall) / 4.0f);
                if (map.governmentWallsSteel() && map.governmentWallHealth(wall) > 0)
                    cell(4, 1);
            }
            cell(9, row >= 24 && col >= 12 && col <= 13);
        }
    auto &state = observation.state;
    playerState(players[slot], state.data());
    state[274] = static_cast<float>(slot);
    state[275] = players.size() == 2 ? 1.0f : 0.0f;
    state[332] = ready(players[slot]) ? 1.0f : 0.0f;
    const int ally = 1 - slot;
    if (ally < static_cast<int>(players.size()))
    {
        playerState(players[ally], state.data() + 256);
        state[333] = ready(players[ally]) ? 1.0f : 0.0f;
    }
    int index = 0;
    for (const auto &enemy : enemies)
    {
        if (enemy.destroyed || index >= 4)
            continue;
        const auto facing = core::cardinalVector(enemy.driveDirection);
        const float values[]{1, enemy.position.x / 26, enemy.position.z / 26,
            facing.x, facing.z, enemy.armor / 4.0f, enemy.type / 3.0f,
            enemy.moving ? 1.0f : 0.0f, enemy.carriesBonus ? 1.0f : 0.0f,
            enemy.creationTimer > 0 ? 1.0f : 0.0f,
            enemy.frozenTimer > 0 ? 1.0f : 0.0f, enemy.onIce ? 1.0f : 0.0f};
        std::copy(std::begin(values), std::end(values), state.data() + 16 + index++ * 12);
    }
    return observation;
}

void TacticalAi::reset()
{
    *this = TacticalAi{};
}

bool TacticalAi::safeFire(double x, double z, int direction)
{
    return !(direction == 2 && x > 10.5 && x < 15.5) &&
           !(direction == 3 && z > 22.5 && x > 11) &&
           !(direction == 4 && z > 22.5 && x < 15);
}

void TacticalAi::plan(const AiObservation &observation,
                      const std::vector<int> &enemies, bool guardian)
{
    const auto &s = observation.state;
    const double x = coordinate(s[0]), z = coordinate(s[1]);
    const Node start{std::clamp(nearestEven(x), 1, 25),
                     std::clamp(nearestEven(z), 1, 25)};
    std::array<bool, AiObservation::kCells> solid{}, blocked{};
    std::array<double, AiObservation::kCells> distance;
    std::array<float, AiObservation::kCells> bricks{}, cost{};
    std::array<int, AiObservation::kCells> previous{};
    distance.fill(std::numeric_limits<double>::infinity());
    for (int row = 0; row < kSize; ++row)
        for (int col = 0; col < kSize; ++col)
        {
            const int cell = row * kSize + col;
            solid[cell] = observation.cell(4, row, col) > 0 ||
                          observation.cell(8, row, col) > 0 ||
                          observation.cell(9, row, col) > 0 ||
                          (s[9] <= .5f && observation.cell(5, row, col) > 0);
            for (int bit = 0; bit < 4; ++bit)
                bricks[cell] += observation.cell(bit, row, col) * .25f;
        }
    if (s[333] > .5f)
    {
        const float ax = coordinate(s[256]), az = coordinate(s[257]);
        for (int row = std::max(0, static_cast<int>(std::floor(az - .875f)));
             row < std::min(26, static_cast<int>(std::ceil(az + .875f))); ++row)
            for (int col = std::max(0, static_cast<int>(std::floor(ax - .875f)));
                 col < std::min(26, static_cast<int>(std::ceil(ax + .875f))); ++col)
                solid[row * kSize + col] = true;
    }
    // Node IDs preserve Python's (x,z) tuple ordering. Discovery order is kept
    // separately for the insertion-ordered fallback minimum.
    for (int nx = 1; nx <= 25; ++nx)
        for (int nz = 1; nz <= 25; ++nz)
        {
            const int cell = (nz - 1) * kSize + nx - 1, node = nx * kSize + nz;
            blocked[node] = solid[cell] || solid[cell + 26] || solid[cell + 1] || solid[cell + 27];
            cost[node] = 1.0f + 3.0f * (bricks[cell] + bricks[cell + 26] +
                                        bricks[cell + 1] + bricks[cell + 27]);
        }
    using Entry = std::pair<double, int>;
    std::priority_queue<Entry, std::vector<Entry>, std::greater<Entry>> queue;
    const int startId = start.first * kSize + start.second;
    distance[startId] = 0;
    std::vector<int> discovered{startId};
    queue.push({0, startId});
    while (!queue.empty())
    {
        const auto [value, node] = queue.top();
        queue.pop();
        if (value != distance[node])
            continue;
        for (int direction = 1; direction <= 4; ++direction)
        {
            const auto [dx, dz] = kDirections[direction];
            const int nx = node / kSize + dx, nz = node % kSize + dz;
            if (nx < 1 || nx > 25 || nz < 1 || nz > 25)
                continue;
            const int next = nx * kSize + nz;
            if (blocked[next])
                continue;
            const double candidate = value + cost[next];
            if (candidate < distance[next])
            {
                if (!std::isfinite(distance[next]))
                    discovered.push_back(next);
                distance[next] = candidate;
                previous[next] = node;
                queue.push({candidate, next});
            }
        }
    }
    int best = startId;
    double bestScore = std::numeric_limits<double>::infinity();
    for (int enemy : enemies)
    {
        const double ex = coordinate(s[enemy + 1]), ez = coordinate(s[enemy + 2]);
        for (int nx = 1; nx <= 25; ++nx)
            for (int nz = 1; nz <= 25; ++nz)
            {
                if (std::abs(nx - ex) >= .70 && std::abs(nz - ez) >= .70)
                    continue;
                const int node = nx * kSize + nz;
                const double gap = std::abs(nx - ex) + std::abs(nz - ez);
                if (!std::isfinite(distance[node]) || gap < 2.75 ||
                    !clearShot(observation, nx, nz, ex, ez, firingDirection(nx, nz, ex, ez)))
                    continue;
                double score = distance[node] * .8 + gap * .12 - ez * .6;
                if (guardian)
                    score += std::max(0, 17 - nz) * 2 + std::max(0.0, ez - nz) * .3;
                else if (s[333] > .5f && ez < 19)
                {
                    const float ax = coordinate(s[256]), az = coordinate(s[257]);
                    if (std::abs(ax - static_cast<float>(ex)) +
                        std::abs(az - static_cast<float>(ez)) + 3.0f <
                        static_cast<float>(std::abs(x - ex) + std::abs(z - ez)))
                        score += 3;
                }
                if (s[enemy + 10] > .5f)
                    score += 3;
                if (score < bestScore)
                {
                    best = node;
                    bestScore = score;
                }
            }
    }
    if (!std::isfinite(bestScore))
    {
        int target = -1;
        for (int enemy : enemies)
            if (target < 0 || s[enemy + 2] > s[target + 2])
                target = enemy;
        const double gx = target >= 0 ? coordinate(s[target + 1]) : (x < 13 ? 11 : 15);
        const double gz = target >= 0
            ? std::min(21.0, std::max(guardian ? 17.0 : 7.0, coordinate(s[target + 2]) + 3.0)) : 19.0;
        for (int node : discovered)
        {
            const double score = std::abs(node / kSize - gx) +
                                 std::abs(node % kSize - gz) + .2 * distance[node];
            if (score < bestScore)
            {
                best = node;
                bestScore = score;
            }
        }
    }
    route_.clear();
    while (best != startId)
    {
        route_.push_back({best / kSize, best % kSize});
        best = previous[best];
    }
    std::reverse(route_.begin(), route_.end());
    nextPlan_ = calls_ + 10;
}

int TacticalAi::chooseAction(const AiObservation &observation)
{
    const auto &s = observation.state;
    if (s[8] < .5f || s[4] <= 0 || s[12] > .5f || s[13] > .5f)
        return 0;
    ++calls_;
    const double x = coordinate(s[0]), z = coordinate(s[1]);
    if (hasPreviousPosition_)
    {
        const double moved = std::abs(x - previousPosition_.first) +
                             std::abs(z - previousPosition_.second);
        stalled_ = previousAction_ / 2 && moved < .015 ? stalled_ + 1 : 0;
        if (moved > 3)
        {
            route_.clear();
            nextPlan_ = 0;
        }
    }
    hasPreviousPosition_ = true;
    previousPosition_ = {x, z};
    const bool guardian = homeRole(observation);
    std::vector<int> enemies;
    double urgent = 0;
    for (int enemy = 16; enemy < 64; enemy += 12)
        if (s[enemy] > .5f && s[enemy + 9] < .5f)
        {
            enemies.push_back(enemy);
            if (s[enemy + 10] < .5f)
                urgent = std::max(urgent, static_cast<double>(coordinate(s[enemy + 2])));
        }
    auto sorted = enemies;
    std::stable_sort(sorted.begin(), sorted.end(), [&](int a, int b)
        { return s[a + 2] > s[b + 2]; });
    for (int enemy : sorted)
    {
        const double ex = coordinate(s[enemy + 1]), ez = coordinate(s[enemy + 2]);
        if (guardian && urgent >= 18 && ez < urgent - 3)
            continue;
        const int direction = firingDirection(x, z, ex, ez);
        if (!clearShot(observation, x, z, ex, ez, direction))
            continue;
        const double gap = std::abs(ex - x) + std::abs(ez - z);
        if (gap >= 2.75 && heading(observation) == direction)
            return previousAction_ = 1;
        if (gap >= 2.25)
            return previousAction_ = 2 * direction + 1;
    }
    if (guardian_ != static_cast<int>(guardian) || calls_ >= nextPlan_ || stalled_ >= 50)
    {
        plan(observation, enemies, guardian);
        guardian_ = guardian;
        if (stalled_ >= 50)
            stalled_ = 0;
    }
    while (!route_.empty())
    {
        const auto [nx, nz] = route_.front();
        if (std::abs(nx - x) < .17 && std::abs(nz - z) < .17)
        {
            route_.erase(route_.begin());
            continue;
        }
        const int direction = std::abs(nx - x) >= std::abs(nz - z)
            ? (nx > x ? 4 : 3) : (nz > z ? 2 : 1);
        if (!passable(observation, direction))
        {
            nextPlan_ = 0;
            return previousAction_ = safeFire(x, z, heading(observation)) ? 1 : 0;
        }
        return previousAction_ = 2 * direction + static_cast<int>(safeFire(x, z, direction));
    }
    return previousAction_ = heading(observation) == 1 ? 1 : 3;
}

int TacticalAi::predict(const AiObservation &observation)
{
    const int action = chooseAction(observation);
    const int direction = action / 2 ? action / 2 : heading(observation);
    // guard_fire converts each normalized scalar to double before multiplying.
    const double x = static_cast<double>(observation.state[0]) * 26;
    const double z = static_cast<double>(observation.state[1]) * 26;
    return action % 2 && !safeFire(x, z, direction) ? action - 1 : action;
}

game::PlayerControlFrame aiActionInput(int action, int previousDirection)
{
    game::PlayerControlFrame input;
    if (action < 0 || action > 9)
        return input;
    const int direction = action / 2;
    game::DirectionButtonFrame *buttons[]{nullptr, &input.north, &input.south,
                                         &input.west, &input.east};
    if (direction)
        *buttons[direction] = {true, direction != previousDirection};
    input.fireHeld = action % 2 != 0;
    return input;
}

void AiPlayerController::reset()
{
    *this = AiPlayerController{};
}

void AiPlayerController::update(float elapsed, bool battleRunning, int stage,
                               const game::StageMap &map,
                               const std::vector<game::Player> &players,
                               const std::vector<game::Enemy> &enemies,
                               game::PlayerInputFrame &input)
{
    input.players[1] = {};
    if (stage_ != stage)
    {
        reset();
        stage_ = stage;
    }
    if (!battleRunning || players.size() != 2 || !ready(players[1]) ||
        !std::isfinite(elapsed) || elapsed < 0)
    {
        action_ = previousDirection_ = 0;
        untilDecision_ = 0;
        return;
    }
    if (untilDecision_ <= 1e-6)
    {
        action_ = policy_.predict(observeAiPlayer(map, players, enemies, 1));
        untilDecision_ += .05;
        ++decisions_;
    }
    input.players[1] = aiActionInput(action_, previousDirection_);
    previousDirection_ = action_ / 2;
    untilDecision_ -= std::clamp(static_cast<double>(elapsed), 0.0, .05);
}
} // namespace tanks3d::app
