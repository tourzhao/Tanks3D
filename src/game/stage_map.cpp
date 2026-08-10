#include "game/stage_map.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace tanks3d::game
{
using core::distanceSquared;
using core::lengthSquared;
using core::normalizedNation;

namespace
{
float projectionRadiusOnAxis(const GovernmentWallSegment &segment, XZ axis)
{
    return segment.halfLength * std::fabs(segment.along.x * axis.x +
                                          segment.along.z * axis.z) +
           segment.halfThickness * std::fabs(segment.outward.x * axis.x +
                                              segment.outward.z * axis.z);
}

bool governmentWallContainsPoint(const GovernmentWallSegment &segment,
                                 XZ position)
{
    const XZ delta = position - segment.center;
    const float along = delta.x * segment.along.x +
                        delta.z * segment.along.z;
    const float across = delta.x * segment.outward.x +
                         delta.z * segment.outward.z;
    return std::fabs(along) < segment.halfLength &&
           std::fabs(across) < segment.halfThickness;
}
} // namespace

bool ShellImpactDetails::destroyedBrick() const
{
    for (int index = 0; index < brickCount; ++index)
        if (bricks[static_cast<std::size_t>(index)].beforeMask != 0U &&
            bricks[static_cast<std::size_t>(index)].afterMask == 0U)
            return true;
    return false;
}

bool ShellImpactDetails::destroyedGovernmentWall() const
{
    return governmentWallIndex >= 0 && governmentWallHealthBefore > 0 &&
           governmentWallHealthAfter == 0;
}

bool bonusOverlapsGovernmentBase(XZ position)
{
    return position.x + 1.0f > kGovernmentBaseCenter.x - 1.0f &&
           position.x - 1.0f < kGovernmentBaseCenter.x + 1.0f &&
           position.z + 1.0f > kGovernmentBaseCenter.z - 1.0f &&
           position.z - 1.0f < kGovernmentBaseCenter.z + 1.0f;
}

XZ governmentPentagonCorner(int requestedIndex)
{
    constexpr float kPi = 3.14159265358979323846f;
    const int index = (requestedIndex % kGovernmentWallCount +
                       kGovernmentWallCount) % kGovernmentWallCount;
    const float angle = kGovernmentPentagonYaw +
                        static_cast<float>(index) * 2.0f * kPi /
                            static_cast<float>(kGovernmentWallCount);
    return {kGovernmentBaseCenter.x +
                std::sin(angle) * kGovernmentWallRadius,
            kGovernmentBaseCenter.z +
                std::cos(angle) * kGovernmentWallRadius};
}

GovernmentWallSegment governmentWallSegment(int index)
{
    GovernmentWallSegment segment;
    segment.start = governmentPentagonCorner(index);
    segment.end = governmentPentagonCorner(index + 1);
    const XZ edge = segment.end - segment.start;
    segment.length = std::sqrt(lengthSquared(edge));
    segment.halfLength = segment.length * 0.5f + kGovernmentWallEndOverlap;
    segment.along = edge * (1.0f / std::max(0.0001f, segment.length));
    segment.center = (segment.start + segment.end) * 0.5f;
    const XZ radial = segment.center - kGovernmentBaseCenter;
    const float radialLength = std::sqrt(lengthSquared(radial));
    segment.outward = radial * (1.0f / std::max(0.0001f, radialLength));
    segment.yaw = std::atan2(edge.z, edge.x);
    return segment;
}

bool governmentWallOverlapsShell(const GovernmentWallSegment &segment,
                                 XZ shellCenter)
{
    const XZ delta = shellCenter - segment.center;
    const std::array<XZ, 4> axes{{
        segment.along, segment.outward, {1.0f, 0.0f}, {0.0f, 1.0f}}};
    for (XZ axis : axes)
    {
        const float centerDistance = std::fabs(delta.x * axis.x +
                                               delta.z * axis.z);
        const float wallRadius = projectionRadiusOnAxis(segment, axis);
        const float shellRadius = kShellHalfSize *
                                  (std::fabs(axis.x) + std::fabs(axis.z));
        if (centerDistance >= wallRadius + shellRadius - 0.0001f)
            return false;
    }
    return true;
}

bool governmentWallOverlapsAabb(const GovernmentWallSegment &segment,
                                XZ position, float halfExtent)
{
    const XZ delta = position - segment.center;
    const std::array<XZ, 4> axes{{
        segment.along, segment.outward, {1.0f, 0.0f}, {0.0f, 1.0f}}};
    for (XZ axis : axes)
    {
        const float centerDistance = std::fabs(delta.x * axis.x +
                                               delta.z * axis.z);
        const float wallRadius = projectionRadiusOnAxis(segment, axis);
        const float tankRadius = halfExtent *
                                 (std::fabs(axis.x) + std::fabs(axis.z));
        if (centerDistance >= wallRadius + tankRadius - 0.0001f)
            return false;
    }
    return true;
}

bool aabbOverlapsRectangle(XZ center, float halfExtent,
                           float minimumX, float minimumZ,
                           float maximumX, float maximumZ)
{
    return center.x - halfExtent < maximumX &&
           center.x + halfExtent > minimumX &&
           center.z - halfExtent < maximumZ &&
           center.z + halfExtent > minimumZ;
}

int normalizedStage(int stage)
{
    const int remainder = stage % kStageCount;
    return remainder > 0 ? remainder : remainder + kStageCount;
}

void StageMap::setGovernmentNation(Nation nation)
{
    governmentNation_ = normalizedNation(nation);
}

Nation StageMap::governmentNation() const
{
    return governmentNation_;
}

GovernmentBaseTheme StageMap::governmentBaseTheme() const
{
    return governmentBaseThemeForNation(governmentNation_);
}

bool StageMap::load(const std::filesystem::path &, int requestedStage,
                    std::string &error)
{
    stage_ = normalizedStage(requestedStage);
    tiles_ = StageGenerator::generate(stage_);
    resetTerrainDamageState();
    prepareGovernmentBase();
    if (!generatedStageIsPlayable())
    {
        error = "Generated stage " + std::to_string(stage_) +
                " failed its spawn or route validation";
        return false;
    }
    error.clear();
    return true;
}

int StageMap::stage() const
{
    return stage_;
}

char StageMap::tile(int row, int column) const
{
    if (row < 0 || row >= kMapSize || column < 0 || column >= kMapSize)
        return '@';
    return tiles_[row][column];
}

unsigned char StageMap::brickMask(int row, int column) const
{
    if (row < 0 || row >= kMapSize || column < 0 || column >= kMapSize)
        return 0U;
    return brickMask_[row][column];
}

unsigned char StageMap::brickHitCount(int row, int column) const
{
    if (row < 0 || row >= kMapSize || column < 0 || column >= kMapSize)
        return 0U;
    return brickHitCount_[row][column];
}

CardinalDirection StageMap::brickFirstDirection(int row, int column) const
{
    if (row < 0 || row >= kMapSize || column < 0 || column >= kMapSize)
        return CardinalDirection::None;
    return static_cast<CardinalDirection>(
        brickFirstDirection_[row][column]);
}

bool StageMap::wallOccupies(XZ position) const
{
    for (int index = 0; index < kGovernmentWallCount; ++index)
        if (governmentWallHealth_[static_cast<std::size_t>(index)] > 0 &&
            governmentWallContainsPoint(governmentWallSegment(index),
                                         position))
            return true;
    const int column = static_cast<int>(std::floor(position.x));
    const int row = static_cast<int>(std::floor(position.z));
    if (row < 0 || row >= kMapSize || column < 0 || column >= kMapSize)
        return true;
    const char value = tiles_[row][column];
    if (value == '@')
        return true;
    if (value != '#')
        return false;
    const int quadrantX = position.x - column >= 0.5f ? 1 : 0;
    const int quadrantZ = position.z - row >= 0.5f ? 1 : 0;
    const unsigned char bit = static_cast<unsigned char>(
        1U << (quadrantX + quadrantZ * 2));
    return (brickMask_[row][column] & bit) != 0U;
}

bool StageMap::solidSeparatesShells(XZ firstPosition,
                                    XZ secondPosition) const
{
    // At the instant two half-tile shell AABBs overlap, their centers may
    // still lie on opposite sides of a thin angled Pentagon wall. Sample the
    // short center-to-center segment instead of checking only its midpoint.
    const XZ separation = secondPosition - firstPosition;
    const float distance = std::sqrt(lengthSquared(separation));
    const int steps = std::max(
        1, static_cast<int>(std::ceil(distance / 0.04f)));
    for (int step = 0; step <= steps; ++step)
    {
        const float amount = static_cast<float>(step) /
                             static_cast<float>(steps);
        const XZ sample = firstPosition + separation * amount;
        if (wallOccupies(sample) || isInsideBase(sample))
            return true;
    }
    return false;
}

void StageMap::prepareShowcaseArena()
{
    for (int row = 0; row < kMapSize; ++row)
    {
        for (int column = 0; column < kMapSize; ++column)
        {
            tiles_[row][column] = '.';
            brickMask_[row][column] = 0U;
            brickHitCount_[row][column] = 0U;
            brickFirstDirection_[row][column] =
                static_cast<unsigned char>(CardinalDirection::None);
        }
    }
    prepareGovernmentBase();
}

void StageMap::prepareGovernmentBase()
{
    // The expanded Pentagon owns this footprint directly. Clear the old
    // classic brick/steel ring so collision comes from the same five angled
    // wall segments that are rendered on screen.
    for (int row = 21; row <= 25; ++row)
    {
        for (int column = 10; column <= 15; ++column)
        {
            tiles_[row][column] = '.';
            brickMask_[row][column] = 0U;
            brickHitCount_[row][column] = 0U;
            brickFirstDirection_[row][column] =
                static_cast<unsigned char>(CardinalDirection::None);
        }
    }
    governmentSteelTimer_ = 0.0f;
    repairGovernmentWalls();
}

void StageMap::repairGovernmentWalls()
{
    governmentWallHealth_.fill(kGovernmentWallMaximumHealth);
}

void StageMap::activateGovernmentSteel()
{
    repairGovernmentWalls();
    governmentSteelTimer_ = kGovernmentSteelDuration;
}

void StageMap::updateGovernmentProtection(float dt)
{
    governmentSteelTimer_ = std::max(
        0.0f, governmentSteelTimer_ - std::max(0.0f, dt));
}

bool StageMap::governmentWallsSteel() const
{
    return governmentSteelTimer_ > 0.0f;
}

float StageMap::governmentSteelTimeRemaining() const
{
    return governmentSteelTimer_;
}

bool StageMap::governmentSteelVisible() const
{
    if (!governmentWallsSteel())
        return false;
    if (governmentSteelTimer_ > kGovernmentSteelWarningDuration)
        return true;
    return static_cast<int>(governmentSteelTimer_ /
                            kGovernmentSteelFlashPeriod) % 2 == 0;
}

int StageMap::governmentWallHealth(int index) const
{
    if (index < 0 || index >= kGovernmentWallCount)
        return 0;
    return governmentWallHealth_[static_cast<std::size_t>(index)];
}

bool StageMap::isInsideBase(XZ position) const
{
    return distanceSquared(position, kGovernmentBaseCenter) <
           kGovernmentCoreRadius * kGovernmentCoreRadius;
}

bool StageMap::shellHitsGovernmentCore(XZ shellCenter) const
{
    const float closestX = std::clamp(
        kGovernmentBaseCenter.x, shellCenter.x - kShellHalfSize,
        shellCenter.x + kShellHalfSize);
    const float closestZ = std::clamp(
        kGovernmentBaseCenter.z, shellCenter.z - kShellHalfSize,
        shellCenter.z + kShellHalfSize);
    const float dx = kGovernmentBaseCenter.x - closestX;
    const float dz = kGovernmentBaseCenter.z - closestZ;
    return dx * dx + dz * dz <
           kGovernmentCoreRadius * kGovernmentCoreRadius;
}

bool StageMap::collidesWithTank(XZ position, float halfExtent,
                                bool allowWater) const
{
    if (position.x - halfExtent < 0.0f ||
        position.z - halfExtent < 0.0f ||
        position.x + halfExtent > static_cast<float>(kMapSize) ||
        position.z + halfExtent > static_cast<float>(kMapSize))
        return true;

    for (int index = 0; index < kGovernmentWallCount; ++index)
        if (governmentWallHealth_[static_cast<std::size_t>(index)] > 0 &&
            governmentWallOverlapsAabb(governmentWallSegment(index), position,
                                       halfExtent))
            return true;
    const float closestCoreX = std::clamp(
        kGovernmentBaseCenter.x, position.x - halfExtent,
        position.x + halfExtent);
    const float closestCoreZ = std::clamp(
        kGovernmentBaseCenter.z, position.z - halfExtent,
        position.z + halfExtent);
    const float coreDx = kGovernmentBaseCenter.x - closestCoreX;
    const float coreDz = kGovernmentBaseCenter.z - closestCoreZ;
    if (coreDx * coreDx + coreDz * coreDz <
        kGovernmentCoreRadius * kGovernmentCoreRadius)
        return true;

    const int left = std::max(
        0, static_cast<int>(std::floor(position.x - halfExtent)));
    const int right = std::min(
        kMapSize - 1,
        static_cast<int>(std::floor(position.x + halfExtent)));
    const int top = std::max(
        0, static_cast<int>(std::floor(position.z - halfExtent)));
    const int bottom = std::min(
        kMapSize - 1,
        static_cast<int>(std::floor(position.z + halfExtent)));

    for (int row = top; row <= bottom; ++row)
    {
        for (int column = left; column <= right; ++column)
        {
            const char value = tiles_[row][column];
            if (value == '#')
            {
                const unsigned char mask = brickMask_[row][column];
                for (int quadrant = 0; quadrant < 4; ++quadrant)
                {
                    if ((mask & (1U << quadrant)) == 0U)
                        continue;
                    const float minimumX =
                        column + ((quadrant & 1) != 0 ? 0.5f : 0.0f);
                    const float minimumZ =
                        row + ((quadrant & 2) != 0 ? 0.5f : 0.0f);
                    if (aabbOverlapsRectangle(
                            position, halfExtent, minimumX, minimumZ,
                            minimumX + 0.5f, minimumZ + 0.5f))
                        return true;
                }
                continue;
            }
            if (value != '@' && (value != '~' || allowWater))
                continue;
            if (aabbOverlapsRectangle(
                    position, halfExtent, static_cast<float>(column),
                    static_cast<float>(row),
                    static_cast<float>(column + 1),
                    static_cast<float>(row + 1)))
                return true;
        }
    }
    return false;
}

bool StageMap::hasTankRoute(XZ start, XZ goal) const
{
    const int startColumn = static_cast<int>(std::lround(start.x));
    const int startRow = static_cast<int>(std::lround(start.z));
    const int goalColumn = static_cast<int>(std::lround(goal.x));
    const int goalRow = static_cast<int>(std::lround(goal.z));
    const auto insideNavigationGrid = [](int row, int column) {
        return row >= 1 && row < kMapSize &&
               column >= 1 && column < kMapSize;
    };
    if (!insideNavigationGrid(startRow, startColumn) ||
        !insideNavigationGrid(goalRow, goalColumn) ||
        collidesWithTank(start, kTankRadius) ||
        collidesWithTank(goal, kTankRadius))
        return false;

    std::array<std::array<bool, kMapSize>, kMapSize> visited{};
    std::vector<std::array<int, 2>> frontier;
    frontier.push_back({startRow, startColumn});
    visited[static_cast<std::size_t>(startRow)]
           [static_cast<std::size_t>(startColumn)] = true;
    std::size_t cursor = 0;
    static constexpr std::array<std::array<int, 2>, 4> steps{{
        {{-1, 0}}, {{1, 0}}, {{0, -1}}, {{0, 1}}}};
    while (cursor < frontier.size())
    {
        const int row = frontier[cursor][0];
        const int column = frontier[cursor][1];
        ++cursor;
        if (row == goalRow && column == goalColumn)
            return true;
        for (const auto &step : steps)
        {
            const int nextRow = row + step[0];
            const int nextColumn = column + step[1];
            if (!insideNavigationGrid(nextRow, nextColumn) ||
                visited[static_cast<std::size_t>(nextRow)]
                       [static_cast<std::size_t>(nextColumn)] ||
                collidesWithTank({static_cast<float>(nextColumn),
                                  static_cast<float>(nextRow)},
                                 kTankRadius))
                continue;
            visited[static_cast<std::size_t>(nextRow)]
                   [static_cast<std::size_t>(nextColumn)] = true;
            frontier.push_back({nextRow, nextColumn});
        }
    }
    return false;
}

bool StageMap::isIce(XZ position) const
{
    return tile(static_cast<int>(position.z),
                static_cast<int>(position.x)) == '-';
}

ImpactKind StageMap::impactShell(XZ position, bool powerShell,
                                 CardinalDirection direction,
                                 ShellImpactDetails *details)
{
    if (details != nullptr)
        *details = {};
    constexpr float epsilon = 0.0001f;
    const float minimumX = position.x - kShellHalfSize;
    const float maximumX = position.x + kShellHalfSize;
    const float minimumZ = position.z - kShellHalfSize;
    const float maximumZ = position.z + kShellHalfSize;

    int rowStart = 0;
    int rowEnd = 0;
    int columnStart = 0;
    int columnEnd = 0;
    const bool outside = minimumX < 0.0f || minimumZ < 0.0f ||
                         maximumX > static_cast<float>(kMapSize) ||
                         maximumZ > static_cast<float>(kMapSize);
    switch (direction)
    {
    case CardinalDirection::North:
        rowStart = rowEnd =
            static_cast<int>(std::floor(minimumZ + epsilon));
        columnStart = static_cast<int>(std::floor(minimumX + epsilon));
        columnEnd = static_cast<int>(std::floor(maximumX - epsilon));
        break;
    case CardinalDirection::East:
        columnStart = columnEnd =
            static_cast<int>(std::floor(maximumX - epsilon));
        rowStart = static_cast<int>(std::floor(minimumZ + epsilon));
        rowEnd = static_cast<int>(std::floor(maximumZ - epsilon));
        break;
    case CardinalDirection::South:
        rowStart = rowEnd =
            static_cast<int>(std::floor(maximumZ - epsilon));
        columnStart = static_cast<int>(std::floor(minimumX + epsilon));
        columnEnd = static_cast<int>(std::floor(maximumX - epsilon));
        break;
    case CardinalDirection::West:
        columnStart = columnEnd =
            static_cast<int>(std::floor(minimumX + epsilon));
        rowStart = static_cast<int>(std::floor(minimumZ + epsilon));
        rowEnd = static_cast<int>(std::floor(maximumZ - epsilon));
        break;
    case CardinalDirection::None:
        return ImpactKind::Boundary;
    }

    rowStart = std::max(rowStart, 0);
    rowEnd = std::min(rowEnd, kMapSize - 1);
    columnStart = std::max(columnStart, 0);
    columnEnd = std::min(columnEnd, kMapSize - 1);

    int governmentImpact = -1;
    float governmentImpactDistance = std::numeric_limits<float>::max();
    for (int index = 0; index < kGovernmentWallCount; ++index)
    {
        if (governmentWallHealth_[static_cast<std::size_t>(index)] <= 0)
            continue;
        const GovernmentWallSegment segment = governmentWallSegment(index);
        if (!governmentWallOverlapsShell(segment, position))
            continue;
        const float candidateDistance = distanceSquared(position,
                                                         segment.center);
        if (candidateDistance < governmentImpactDistance)
        {
            governmentImpact = index;
            governmentImpactDistance = candidateDistance;
        }
    }
    if (governmentImpact >= 0)
    {
        int &health = governmentWallHealth_[
            static_cast<std::size_t>(governmentImpact)];
        const int before = health;
        if (governmentWallsSteel())
        {
            if (details != nullptr)
            {
                details->governmentWallIndex = governmentImpact;
                details->governmentWallHealthBefore = before;
                details->governmentWallHealthAfter = before;
            }
            return ImpactKind::Steel;
        }
        health = std::max(
            0, health - (powerShell ? kGovernmentPowerShellDamage : 1));
        if (details != nullptr)
        {
            details->governmentWallIndex = governmentImpact;
            details->governmentWallHealthBefore = before;
            details->governmentWallHealthAfter = health;
        }
        return ImpactKind::GovernmentWall;
    }

    const auto remainingHalf = [](CardinalDirection hitDirection) {
        switch (hitDirection)
        {
        case CardinalDirection::North:
            return static_cast<unsigned char>(0x03U);
        case CardinalDirection::East:
            return static_cast<unsigned char>(0x0aU);
        case CardinalDirection::South:
            return static_cast<unsigned char>(0x0cU);
        case CardinalDirection::West:
            return static_cast<unsigned char>(0x05U);
        case CardinalDirection::None:
            return static_cast<unsigned char>(0x00U);
        }
        return static_cast<unsigned char>(0x00U);
    };
    const auto vertical = [](CardinalDirection hitDirection) {
        return hitDirection == CardinalDirection::North ||
               hitDirection == CardinalDirection::South;
    };
    const auto overlapsBrick = [&](int row, int column) {
        const unsigned char mask = brickMask_[row][column];
        for (int quadrant = 0; quadrant < 4; ++quadrant)
        {
            if ((mask & (1U << quadrant)) == 0U)
                continue;
            const float brickMinimumX =
                column + ((quadrant & 1) != 0 ? 0.5f : 0.0f);
            const float brickMinimumZ =
                row + ((quadrant & 2) != 0 ? 0.5f : 0.0f);
            const bool overlapX = std::max(minimumX, brickMinimumX) <
                                  std::min(maximumX,
                                           brickMinimumX + 0.5f);
            const bool overlapZ = std::max(minimumZ, brickMinimumZ) <
                                  std::min(maximumZ,
                                           brickMinimumZ + 0.5f);
            if (overlapX && overlapZ)
                return true;
        }
        return false;
    };

    ImpactKind result = ImpactKind::None;
    for (int row = rowStart; row <= rowEnd; ++row)
    {
        for (int column = columnStart; column <= columnEnd; ++column)
        {
            char &value = tiles_[row][column];
            if (value == '%' && powerShell)
            {
                value = '.';
                continue;
            }
            if (value == '#' && overlapsBrick(row, column))
            {
                unsigned char &mask = brickMask_[row][column];
                const unsigned char beforeMask = mask;
                if (powerShell)
                {
                    value = '.';
                    mask = 0U;
                    brickHitCount_[row][column] = 0U;
                    brickFirstDirection_[row][column] =
                        static_cast<unsigned char>(CardinalDirection::None);
                }
                else
                {
                    unsigned char &hitCount = brickHitCount_[row][column];
                    ++hitCount;
                    if (hitCount == 1U)
                    {
                        brickFirstDirection_[row][column] =
                            static_cast<unsigned char>(direction);
                        mask = remainingHalf(direction);
                    }
                    else if (hitCount == 2U)
                    {
                        const CardinalDirection firstDirection =
                            static_cast<CardinalDirection>(
                                brickFirstDirection_[row][column]);
                        if (vertical(firstDirection) != vertical(direction))
                            mask = static_cast<unsigned char>(
                                mask & remainingHalf(direction));
                        else
                            mask = 0U;
                    }
                    else
                    {
                        mask = 0U;
                    }
                    if (mask == 0U)
                    {
                        value = '.';
                        brickHitCount_[row][column] = 0U;
                        brickFirstDirection_[row][column] =
                            static_cast<unsigned char>(
                                CardinalDirection::None);
                    }
                }
                if (details != nullptr &&
                    details->brickCount <
                        static_cast<int>(details->bricks.size()))
                {
                    BrickDamage &damage = details->bricks[
                        static_cast<std::size_t>(details->brickCount++)];
                    damage = {row, column, beforeMask, mask};
                }
                if (result == ImpactKind::None)
                    result = ImpactKind::Brick;
            }
            else if (value == '@')
            {
                if (powerShell)
                    value = '.';
                result = ImpactKind::Steel;
            }
        }
    }
    return result != ImpactKind::None
               ? result
               : outside ? ImpactKind::Boundary : ImpactKind::None;
}

void StageMap::resetTerrainDamageState()
{
    for (int row = 0; row < kMapSize; ++row)
    {
        for (int column = 0; column < kMapSize; ++column)
        {
            brickMask_[row][column] =
                tiles_[row][column] == '#' ? 0x0fU : 0U;
            brickHitCount_[row][column] = 0U;
            brickFirstDirection_[row][column] =
                static_cast<unsigned char>(CardinalDirection::None);
        }
    }
}

bool StageMap::generatedStageIsPlayable() const
{
    // The fixed Battle City stage deliberately shields the base approach with
    // destructible brickwork.  Keep its spawn/player routes validated, but do
    // not require that protected approach to be open before combat begins.
    if (stage_ == 1)
    {
        for (XZ spawn : kEnemySpawnPoints)
            if (collidesWithTank(spawn, kTankRadius) ||
                !hasTankRoute(spawn, kPlayerSpawnPoints[0]) ||
                !hasTankRoute(spawn, kPlayerSpawnPoints[1]))
                return false;
        for (XZ spawn : kPlayerSpawnPoints)
            if (collidesWithTank(spawn, kTankRadius))
                return false;
        return hasTankRoute(kPlayerSpawnPoints[0], kPlayerSpawnPoints[1]) &&
               !collidesWithTank({13.0f, 20.0f}, kTankRadius);
    }

    for (XZ spawn : kEnemySpawnPoints)
        if (collidesWithTank(spawn, kTankRadius) ||
            !hasTankRoute(spawn, kPlayerSpawnPoints[0]) ||
            !hasTankRoute(spawn, kPlayerSpawnPoints[1]))
            return false;
    if (!hasTankRoute(kPlayerSpawnPoints[0], kPlayerSpawnPoints[1]))
        return false;

    const XZ baseApproach{13.0f, 20.0f};
    return !collidesWithTank(baseApproach, kTankRadius) &&
           hasTankRoute(kEnemySpawnPoints[1], baseApproach);
}
} // namespace tanks3d::game
