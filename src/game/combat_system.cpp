#include "game/combat_system.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace tanks3d::game
{
namespace
{
CombatOutcome snapshotShell(const Shell &shell)
{
    CombatOutcome outcome;
    outcome.position = shell.position;
    outcome.incomingVelocity = shell.velocity;
    outcome.owner = shell.owner;
    outcome.ownerIndex = shell.ownerIndex;
    outcome.power = shell.power;
    return outcome;
}

CombatOutcome continueShellImpact(const CombatOutcome &previousOutcome)
{
    CombatOutcome outcome = previousOutcome;
    outcome.target = CombatTarget::None;
    outcome.impactKind = ImpactKind::None;
    outcome.impactDetails = {};
    outcome.governmentCoreHealthBefore = 0;
    outcome.governmentCoreHealthAfter = 0;
    outcome.targetEnemyIndex = -1;
    outcome.targetEnemyId = -1;
    outcome.targetEnemyType = -1;
    outcome.targetEnemyPosition = {};
    outcome.targetEnemyCarriedBonus = false;
    outcome.targetEnemyArmorBefore = 0;
    outcome.targetEnemyArmorAfter = 0;
    outcome.targetPlayerIndex = -1;
    outcome.targetPlayerId = -1;
    outcome.targetPlayerPosition = {};
    outcome.targetPlayerHitPointsBefore = 0;
    outcome.targetPlayerShielded = false;
    outcome.targetPlayerHadBoat = false;
    return outcome;
}
} // namespace

bool CombatOutcome::mapStopsShell() const
{
    return target == CombatTarget::StageMap;
}

bool CombatOutcome::stopsShell() const
{
    return target != CombatTarget::None;
}

bool CombatOutcome::damagedGovernmentCore() const
{
    return target == CombatTarget::GovernmentCore &&
           governmentCoreHealthAfter < governmentCoreHealthBefore;
}

bool CombatOutcome::destroyedGovernmentCore() const
{
    return target == CombatTarget::GovernmentCore &&
           governmentCoreHealthBefore > 0 &&
           governmentCoreHealthAfter <= 0;
}

bool CombatOutcome::damagedEnemyTank() const
{
    return target == CombatTarget::EnemyTank &&
           targetEnemyArmorAfter < targetEnemyArmorBefore;
}

bool CombatOutcome::destroyedEnemyTank() const
{
    return target == CombatTarget::EnemyTank &&
           targetEnemyArmorBefore > 0 && targetEnemyArmorAfter <= 0;
}

XZ shellSpawnPosition(XZ tankPosition, CardinalDirection direction)
{
    return tankPosition + cardinalVector(direction) * kShellSpawnDistance;
}

CombatOutcome resolveShellMapImpact(StageMap &map, const Shell &shell)
{
    CombatOutcome outcome = snapshotShell(shell);
    outcome.impactKind = map.impactShell(
        shell.position, shell.power,
        cardinalToward({0.0f, 0.0f}, shell.velocity),
        &outcome.impactDetails);
    if (outcome.impactKind != ImpactKind::None)
        outcome.target = CombatTarget::StageMap;
    return outcome;
}

CombatOutcome resolveShellGovernmentCoreImpact(const StageMap &map,
                                               bool &baseAlive,
                                               const CombatOutcome &mapOutcome)
{
    if (mapOutcome.target != CombatTarget::None)
        return mapOutcome;

    CombatOutcome outcome = continueShellImpact(mapOutcome);
    if (!map.shellHitsGovernmentCore(outcome.position))
        return outcome;

    outcome.target = CombatTarget::GovernmentCore;
    outcome.governmentCoreHealthBefore = baseAlive ? 1 : 0;
    baseAlive = false;
    outcome.governmentCoreHealthAfter = 0;
    return outcome;
}

CombatOutcome evaluatePlayerShellEnemyTankImpact(
    const std::vector<Enemy> &enemies,
    const CombatOutcome &environmentOutcome)
{
    if (environmentOutcome.target != CombatTarget::None)
        return environmentOutcome;

    CombatOutcome outcome = continueShellImpact(environmentOutcome);
    if (outcome.owner != ShellOwner::Player)
        return outcome;

    for (std::size_t index = 0; index < enemies.size(); ++index)
    {
        const Enemy &enemy = enemies[index];
        if (enemy.destroyed || enemy.creationTimer > 0.0f ||
            enemy.armor <= 0 ||
            !axisAlignedCentersOverlap(outcome.position, enemy.position,
                                       kShellTankHitExtent))
            continue;

        outcome.target = CombatTarget::EnemyTank;
        outcome.targetEnemyIndex = static_cast<int>(index);
        outcome.targetEnemyId = enemy.id;
        outcome.targetEnemyType = enemy.type;
        outcome.targetEnemyPosition = enemy.position;
        outcome.targetEnemyCarriedBonus = enemy.carriesBonus;
        outcome.targetEnemyArmorBefore = enemy.armor;
        outcome.targetEnemyArmorAfter = enemy.armor - 1;
        return outcome;
    }
    return outcome;
}

EnemyTankImpactCommit commitPlayerShellEnemyTankImpact(
    std::vector<Enemy> &enemies, std::vector<Player> &players,
    const CombatOutcome &outcome,
    const CarrierBonusRelease &releaseCarrierBonus)
{
    EnemyTankImpactCommit commit;
    if (outcome.target != CombatTarget::EnemyTank ||
        outcome.owner != ShellOwner::Player ||
        outcome.targetEnemyIndex < 0 ||
        outcome.targetEnemyIndex >= static_cast<int>(enemies.size()) ||
        outcome.targetEnemyArmorBefore <= 0 ||
        outcome.targetEnemyArmorAfter != outcome.targetEnemyArmorBefore - 1)
        return commit;

    Enemy &enemy = enemies[static_cast<std::size_t>(
        outcome.targetEnemyIndex)];
    if (enemy.id != outcome.targetEnemyId ||
        enemy.type != outcome.targetEnemyType ||
        enemy.armor != outcome.targetEnemyArmorBefore ||
        enemy.carriesBonus != outcome.targetEnemyCarriedBonus ||
        enemy.destroyed || enemy.creationTimer > 0.0f)
        return commit;

    if (outcome.targetEnemyCarriedBonus && releaseCarrierBonus)
        releaseCarrierBonus(enemy);

    enemy.armor = outcome.targetEnemyArmorAfter;
    commit.destroyedNow = outcome.destroyedEnemyTank();
    commit.eventPoints = kDirectEnemyHitPoints;
    if (outcome.ownerIndex >= 0 &&
        outcome.ownerIndex < static_cast<int>(players.size()))
    {
        players[static_cast<std::size_t>(outcome.ownerIndex)]
            .creditDirectEnemyHit(outcome.targetEnemyType,
                                  kDirectEnemyHitPoints,
                                  commit.destroyedNow);
        commit.creditedPlayerIndex = outcome.ownerIndex;
    }
    if (commit.destroyedNow)
    {
        enemy.destroyed = true;
        enemy.moving = false;
        enemy.deathTimer = kTankDeathDuration;
    }
    commit.applied = true;
    return commit;
}

CombatOutcome evaluateEnemyShellPlayerTankImpact(
    const std::vector<Player> &players,
    const CombatOutcome &previousOutcome)
{
    if (previousOutcome.target != CombatTarget::None)
        return previousOutcome;

    CombatOutcome outcome = continueShellImpact(previousOutcome);
    if (outcome.owner != ShellOwner::Enemy)
        return outcome;

    for (std::size_t index = 0; index < players.size(); ++index)
    {
        const Player &player = players[index];
        if (!player.active || player.creationTimer > 0.0f ||
            player.hitPoints <= 0 ||
            !axisAlignedCentersOverlap(outcome.position, player.position,
                                       kShellTankHitExtent))
            continue;

        outcome.target = CombatTarget::PlayerTank;
        outcome.targetPlayerIndex = static_cast<int>(index);
        outcome.targetPlayerId = player.id;
        outcome.targetPlayerPosition = player.position;
        outcome.targetPlayerHitPointsBefore = player.hitPoints;
        outcome.targetPlayerShielded = player.shieldTimer > 0.0f;
        outcome.targetPlayerHadBoat = player.hasBoat;
        return outcome;
    }
    return outcome;
}

PlayerTankImpactCommit commitEnemyShellPlayerTankImpact(
    std::vector<Player> &players, const CombatOutcome &outcome,
    const PlayerTankPreCommit &beforeCommit)
{
    PlayerTankImpactCommit commit;
    if (outcome.target != CombatTarget::PlayerTank ||
        outcome.owner != ShellOwner::Enemy ||
        outcome.targetPlayerIndex < 0 ||
        outcome.targetPlayerIndex >= static_cast<int>(players.size()) ||
        outcome.targetPlayerHitPointsBefore <= 0)
        return commit;

    Player &player = players[static_cast<std::size_t>(
        outcome.targetPlayerIndex)];
    if (player.id != outcome.targetPlayerId || !player.active ||
        player.creationTimer > 0.0f ||
        player.hitPoints != outcome.targetPlayerHitPointsBefore ||
        (player.shieldTimer > 0.0f) != outcome.targetPlayerShielded ||
        player.hasBoat != outcome.targetPlayerHadBoat)
        return commit;

    if (beforeCommit)
        beforeCommit(player);

    commit.hitPointsBefore = player.hitPoints;
    commit.hitResult = resolvePlayerHit(player);
    commit.hitPointsAfter = player.hitPoints;
    if (commit.hitResult == PlayerHitResult::Destroyed)
    {
        player.resetDirectKillStreak();
        player.active = false;
        player.moving = false;
        player.deathTimer = kTankDeathDuration;
        player.respawnTimer = 0.0f;
        player.shieldTimer = 0.0f;
    }
    commit.applied = true;
    return commit;
}

ShellPhysicalImpactResult resolveShellPhysicalImpact(
    StageMap &map, bool &baseAlive, std::vector<Enemy> &enemies,
    std::vector<Player> &players, const Shell &shell,
    const CarrierBonusRelease &releaseCarrierBonus,
    const PlayerTankPreCommit &beforePlayerCommit)
{
    ShellPhysicalImpactResult result;
    if (shell.impacting || shell.life <= 0.0f)
        return result;

    result.outcome = resolveShellMapImpact(map, shell);
    if (result.outcome.mapStopsShell())
    {
        result.resolved = true;
        return result;
    }

    result.outcome = resolveShellGovernmentCoreImpact(
        map, baseAlive, result.outcome);
    if (result.outcome.target == CombatTarget::GovernmentCore)
    {
        result.resolved = true;
        return result;
    }

    result.outcome = evaluatePlayerShellEnemyTankImpact(
        enemies, result.outcome);
    if (result.outcome.target == CombatTarget::EnemyTank)
    {
        result.enemyCommit = commitPlayerShellEnemyTankImpact(
            enemies, players, result.outcome, releaseCarrierBonus);
        result.resolved = result.enemyCommit.applied;
        return result;
    }

    result.outcome = evaluateEnemyShellPlayerTankImpact(
        players, result.outcome);
    if (result.outcome.target == CombatTarget::PlayerTank)
    {
        result.playerCommit = commitEnemyShellPlayerTankImpact(
            players, result.outcome, beforePlayerCommit);
        result.resolved = result.playerCommit.applied;
    }
    return result;
}

std::vector<GameEvent> eventsForPhysicalShellImpact(
    const ShellPhysicalImpactResult &result)
{
    std::vector<GameEvent> events;
    if (!result.resolved)
        return events;

    events.reserve(3);
    const CombatOutcome &outcome = result.outcome;
    const ShellImpactDetails &details = outcome.impactDetails;
    const int brickCount = std::clamp(
        details.brickCount, 0, static_cast<int>(details.bricks.size()));
    for (int index = 0; index < brickCount; ++index)
    {
        const BrickDamage &damage =
            details.bricks[static_cast<std::size_t>(index)];
        GameEvent event = shellEvent(
            GameEventType::BrickHit, outcome.owner, outcome.ownerIndex,
            outcome.position, outcome.power);
        event.impactKind = ImpactKind::Brick;
        event.row = damage.row;
        event.column = damage.column;
        event.valueBefore = damage.beforeMask;
        event.valueAfter = damage.afterMask;
        events.push_back(event);
    }
    if (details.governmentWallIndex >= 0 &&
        details.governmentWallHealthAfter <
            details.governmentWallHealthBefore)
    {
        GameEvent event = shellEvent(
            GameEventType::BaseDamaged, outcome.owner, outcome.ownerIndex,
            outcome.position, outcome.power);
        event.impactKind = ImpactKind::GovernmentWall;
        event.basePart = GovernmentBasePart::Wall;
        event.baseSegmentIndex = details.governmentWallIndex;
        event.valueBefore = details.governmentWallHealthBefore;
        event.valueAfter = details.governmentWallHealthAfter;
        events.push_back(event);
    }

    if (outcome.target == CombatTarget::GovernmentCore &&
        outcome.damagedGovernmentCore())
    {
        GameEvent event = shellEvent(
            GameEventType::BaseDamaged, outcome.owner, outcome.ownerIndex,
            outcome.position, outcome.power);
        event.basePart = GovernmentBasePart::Core;
        event.valueBefore = outcome.governmentCoreHealthBefore;
        event.valueAfter = outcome.governmentCoreHealthAfter;
        events.push_back(event);
    }
    else if (outcome.target == CombatTarget::EnemyTank &&
             result.enemyCommit.applied)
    {
        GameEvent event = shellEvent(
            result.enemyCommit.destroyedNow
                ? GameEventType::TankDestroyed
                : GameEventType::TankDamaged,
            outcome.owner, outcome.ownerIndex, outcome.targetEnemyPosition,
            outcome.power);
        event.targetEnemyId = outcome.targetEnemyId;
        event.enemyType = outcome.targetEnemyType;
        event.valueBefore = outcome.targetEnemyArmorBefore;
        event.valueAfter = outcome.targetEnemyArmorAfter;
        event.points = result.enemyCommit.eventPoints;
        events.push_back(event);
    }
    else if (outcome.target == CombatTarget::PlayerTank &&
             result.playerCommit.applied &&
             (result.playerCommit.hitResult == PlayerHitResult::Damaged ||
              result.playerCommit.hitResult == PlayerHitResult::Destroyed))
    {
        GameEvent event = shellEvent(
            result.playerCommit.hitResult == PlayerHitResult::Destroyed
                ? GameEventType::TankDestroyed
                : GameEventType::TankDamaged,
            outcome.owner, outcome.ownerIndex, outcome.targetPlayerPosition,
            outcome.power);
        event.targetPlayerId = outcome.targetPlayerId;
        event.valueBefore = result.playerCommit.hitPointsBefore;
        event.valueAfter = result.playerCommit.hitPointsAfter;
        events.push_back(event);
    }
    return events;
}

void beginShellImpact(Shell &shell, XZ impactPosition)
{
    if (shell.impacting)
        return;
    shell.position = impactPosition;
    shell.velocity = {};
    shell.impacting = true;
    shell.life = kShellImpactDuration;
}

ShellFrameSchedule prepareShellFrame(std::vector<Shell> &shells, float dt)
{
    for (Shell &shell : shells)
        shell.life -= dt;

    float maximumTravel = 0.0f;
    for (const Shell &shell : shells)
    {
        if (shell.impacting || shell.life <= 0.0f)
            continue;
        maximumTravel = std::max(
            maximumTravel,
            std::sqrt(lengthSquared(shell.velocity)) * dt);
    }

    ShellFrameSchedule schedule;
    schedule.stepCount = std::max(
        1, static_cast<int>(std::ceil(maximumTravel / kShellSweepStep)));
    schedule.stepTime = dt / static_cast<float>(schedule.stepCount);
    return schedule;
}

void advanceShellMicrostep(std::vector<Shell> &shells, float stepTime,
                           std::vector<XZ> &previousPositions)
{
    previousPositions.resize(shells.size());
    for (std::size_t index = 0; index < shells.size(); ++index)
    {
        Shell &shell = shells[index];
        previousPositions[index] = shell.position;
        if (!shell.impacting && shell.life > 0.0f)
            shell.position = shell.position + shell.velocity * stepTime;
    }
}

void removeExpiredShells(std::vector<Shell> &shells)
{
    shells.erase(
        std::remove_if(shells.begin(), shells.end(),
                       [](const Shell &shell) {
                           return shell.life <= 0.0f;
                       }),
        shells.end());
}

bool shellsCanCancel(const Shell &first, const Shell &second)
{
    // The original game only checks player fire against enemy fire. P1/P2
    // rounds pass one another, as do rounds fired by two enemies.
    return !first.impacting && !second.impacting &&
           first.owner != second.owner;
}

bool shellsOverlapForCancellation(const Shell &first, const Shell &second)
{
    if (!shellsCanCancel(first, second))
        return false;
    const XZ separation = first.position - second.position;
    return std::fabs(separation.x) < kShellCancellationExtent &&
           std::fabs(separation.z) < kShellCancellationExtent;
}

bool shellCancellationPoint(const Shell &first, const Shell &second,
                            float dt, XZ &collisionPoint,
                            XZ *firstAtCollision,
                            XZ *secondAtCollision)
{
    if (!shellsCanCancel(first, second))
        return false;

    if (shellsOverlapForCancellation(first, second))
    {
        if (firstAtCollision != nullptr)
            *firstAtCollision = first.position;
        if (secondAtCollision != nullptr)
            *secondAtCollision = second.position;
        collisionPoint = {(first.position.x + second.position.x) * 0.5f,
                          (first.position.z + second.position.z) * 0.5f};
        return true;
    }
    if (dt <= 0.0f)
        return false;

    const XZ relativePosition = first.position - second.position;
    const XZ relativeVelocity = first.velocity - second.velocity;
    float enterTime = 0.0f;
    float exitTime = dt;
    const auto sweepAxis = [&](float position, float velocity) {
        if (std::fabs(velocity) <= 0.000001f)
            return std::fabs(position) < kShellCancellationExtent;

        float firstTime = (-kShellCancellationExtent - position) / velocity;
        float secondTime = (kShellCancellationExtent - position) / velocity;
        if (firstTime > secondTime)
            std::swap(firstTime, secondTime);
        enterTime = std::max(enterTime, firstTime);
        exitTime = std::min(exitTime, secondTime);
        return enterTime < exitTime && exitTime >= 0.0f && enterTime <= dt;
    };
    if (!sweepAxis(relativePosition.x, relativeVelocity.x) ||
        !sweepAxis(relativePosition.z, relativeVelocity.z))
        return false;

    // Step a tiny amount inside the open AABB interval: edge-only contact has
    // zero area and is not a collision in the SDL Rect implementation.
    const float collisionTime = std::clamp(
        enterTime + std::min(0.00001f, (exitTime - enterTime) * 0.25f), 0.0f,
        dt);
    const XZ firstPosition = first.position + first.velocity * collisionTime;
    const XZ secondPosition = second.position + second.velocity * collisionTime;
    if (firstAtCollision != nullptr)
        *firstAtCollision = firstPosition;
    if (secondAtCollision != nullptr)
        *secondAtCollision = secondPosition;
    collisionPoint = {
        (firstPosition.x + secondPosition.x) * 0.5f,
        (firstPosition.z + secondPosition.z) * 0.5f};
    return true;
}

bool resolveSweptShellCancellation(Shell &first, Shell &second,
                                   XZ firstStart, XZ secondStart, float dt,
                                   const StageMap &map, XZ &collisionPoint)
{
    if (first.impacting || second.impacting ||
        first.life <= 0.0f || second.life <= 0.0f)
        return false;

    Shell firstSweep = first;
    Shell secondSweep = second;
    firstSweep.position = firstStart;
    secondSweep.position = secondStart;
    XZ firstAtCollision{};
    XZ secondAtCollision{};
    if (!shellCancellationPoint(firstSweep, secondSweep, dt,
                                collisionPoint, &firstAtCollision,
                                &secondAtCollision) ||
        map.solidSeparatesShells(firstAtCollision, secondAtCollision))
        return false;

    beginShellImpact(first, firstAtCollision);
    beginShellImpact(second, secondAtCollision);
    return true;
}

std::vector<ShellCancellationOutcome> resolveShellCancellations(
    std::vector<Shell> &shells,
    const std::vector<XZ> &previousPositions, float stepTime,
    const StageMap &map)
{
    std::vector<ShellCancellationOutcome> outcomes;
    if (previousPositions.size() != shells.size())
        return outcomes;

    for (std::size_t first = 0; first < shells.size(); ++first)
    {
        if (shells[first].impacting || shells[first].life <= 0.0f)
            continue;
        for (std::size_t second = first + 1; second < shells.size(); ++second)
        {
            if (shells[second].impacting || shells[second].life <= 0.0f)
                continue;

            XZ collisionPoint{};
            if (!resolveSweptShellCancellation(
                    shells[first], shells[second], previousPositions[first],
                    previousPositions[second], stepTime, map,
                    collisionPoint))
                continue;

            ShellCancellationOutcome outcome;
            outcome.firstShellIndex = static_cast<int>(first);
            outcome.secondShellIndex = static_cast<int>(second);
            outcome.position = collisionPoint;
            outcome.firstImpactPosition = shells[first].position;
            outcome.secondImpactPosition = shells[second].position;
            outcome.firstOwner = shells[first].owner;
            outcome.firstOwnerIndex = shells[first].ownerIndex;
            outcome.secondOwner = shells[second].owner;
            outcome.secondOwnerIndex = shells[second].ownerIndex;
            outcomes.push_back(outcome);
            break;
        }
    }
    return outcomes;
}

GameEvent eventForShellCancellation(
    const ShellCancellationOutcome &outcome)
{
    GameEvent event;
    event.type = GameEventType::ShellCancelled;
    event.position = outcome.position;
    const auto attributeSource = [&](ShellOwner owner, int ownerIndex) {
        if (owner == ShellOwner::Player)
            event.sourcePlayerId = ownerIndex;
        else
            event.sourceEnemyId = ownerIndex;
    };
    attributeSource(outcome.firstOwner, outcome.firstOwnerIndex);
    attributeSource(outcome.secondOwner, outcome.secondOwnerIndex);
    return event;
}
} // namespace tanks3d::game
