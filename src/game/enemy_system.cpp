#include "game/enemy_system.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace tanks3d::game
{
namespace
{
constexpr float kEnemyTrackDustTrailOffset = 0.58f;
constexpr std::array<CardinalDirection, 4> kRandomEnemyDirections{{
    CardinalDirection::North, CardinalDirection::East,
    CardinalDirection::South, CardinalDirection::West}};

bool finitePosition(XZ position)
{
    return std::isfinite(position.x) && std::isfinite(position.z);
}

bool finiteNonnegative(float value)
{
    return std::isfinite(value) && value >= 0.0f;
}

bool validEnemySpawnState(const EnemySpawnState &state)
{
    return state.nextSpawnIndex >= 0 &&
           state.nextSpawnIndex < static_cast<int>(kEnemySpawnSlotCount) &&
           state.nextEnemyId >= 0 &&
           std::isfinite(state.timer);
}

bool validEnemySpawnConfiguration(
    const EnemySpawnConfiguration &configuration)
{
    if (configuration.stage < 1 || !finitePosition(configuration.target) ||
        !finiteNonnegative(configuration.initialFireCooldown) ||
        !finiteNonnegative(configuration.normalInterval) ||
        !finiteNonnegative(configuration.retryInterval))
    {
        return false;
    }
    return std::all_of(
        configuration.spawnPoints.begin(), configuration.spawnPoints.end(),
        [](XZ point) { return finitePosition(point); });
}

float manhattanDistance(XZ first, XZ second)
{
    return std::fabs(first.x - second.x) +
           std::fabs(first.z - second.z);
}

bool commitEnemyEscape(Enemy &enemy, const EnemyEscapeChoice &choice)
{
    if (!choice.available())
        return false;

    enemy.position = choice.alignedPosition;
    enemy.driveDirection = choice.direction;
    enemy.movementDirection = choice.direction;
    enemy.yaw = core::cardinalYaw(choice.direction);
    enemy.iceSlipTimer = 0.0f;
    return true;
}
} // namespace

EnemyFrameStart beginEnemyFrame(Enemy &enemy, float dt,
                                bool holdCreationTimer)
{
    EnemyFrameStart start;
    if (!std::isfinite(dt) || dt < 0.0f)
        return start;

    start.valid = true;
    if (enemy.destroyed)
    {
        start.phase = EnemyFramePhase::Destroyed;
        enemy.deathTimer = std::max(0.0f, enemy.deathTimer - dt);
        return start;
    }

    start.hadMomentum = enemy.moving;
    start.previousTravel = enemy.movementDirection;
    enemy.moving = false;
    if (enemy.creationTimer > 0.0f)
    {
        start.phase = EnemyFramePhase::Creating;
        if (!holdCreationTimer)
        {
            enemy.creationTimer = std::max(
                0.0f, enemy.creationTimer - dt);
        }
        enemy.frozenTimer = std::max(0.0f, enemy.frozenTimer - dt);
        return start;
    }

    enemy.dustCooldown = std::max(0.0f, enemy.dustCooldown - dt);
    if (enemy.frozenTimer > 0.0f)
    {
        start.phase = EnemyFramePhase::Frozen;
        enemy.frozenTimer = std::max(0.0f, enemy.frozenTimer - dt);
        enemy.iceSlipTimer = std::max(0.0f, enemy.iceSlipTimer - dt);
        return start;
    }

    start.phase = EnemyFramePhase::Active;
    enemy.fireCooldown -= dt;
    return start;
}

EnemyMovementUpdate advanceActiveEnemyMovement(
    Enemy &enemy, const EnemyFrameStart &frame, float dt,
    float movementSpeed, bool onIce,
    const EnemyPositionAvailable &positionAvailable)
{
    EnemyMovementUpdate update;
    if (!frame.valid || frame.phase != EnemyFramePhase::Active ||
        !std::isfinite(dt) || dt < 0.0f ||
        !std::isfinite(movementSpeed) || movementSpeed < 0.0f ||
        !positionAvailable)
    {
        return update;
    }

    update.valid = true;
    enemy.movementDelay = std::max(0.0f, enemy.movementDelay - dt);
    const bool propelling = enemy.movementDelay <= 0.0f;
    enemy.moving = core::resolveIceTravel(
        onIce, propelling, false, frame.hadMomentum,
        enemy.driveDirection, dt, enemy.movementDirection,
        enemy.iceSlipTimer, enemy.onIce);

    if (enemy.moving &&
        enemy.movementDirection != frame.previousTravel)
    {
        const XZ snapped = core::snappedToCardinalLane(
            enemy.position, enemy.movementDirection);
        if (positionAvailable(snapped))
            enemy.position = snapped;
    }

    const XZ direction = core::cardinalVector(enemy.movementDirection);
    const float movementDistance = movementSpeed * dt;
    const XZ collisionProbe = enemy.position + direction *
        (movementDistance + kEnemyCollisionProbePadding);
    update.blocked = enemy.moving && !positionAvailable(collisionProbe);
    if (enemy.moving && !update.blocked)
    {
        enemy.position = enemy.position + direction * movementDistance;
        enemy.blockedTimer = 0.0f;
        update.movementAccepted = true;
        if (enemy.dustCooldown <= 0.0f)
        {
            update.dust = EnemyTrackDustIntent{
                enemy.position - direction * kEnemyTrackDustTrailOffset,
                direction * movementSpeed};
        }
    }
    else if (update.blocked)
    {
        // Collision cancels the remaining ice drift. Steering retains its
        // separate historical decision clock in the caller.
        enemy.moving = false;
        enemy.iceSlipTimer = 0.0f;
        enemy.movementDirection = enemy.driveDirection;
        enemy.blockedTimer = std::min(
            kEnemyBlockedEscapeDelay + 0.5f,
            enemy.blockedTimer + dt);
    }
    return update;
}

EnemyEscapeChoice chooseEnemyEscape(
    XZ position, CardinalDirection blockedDirection, XZ target,
    int tieBreaker, const EnemyPositionAvailable &positionAvailable)
{
    static constexpr std::array<CardinalDirection, 4> directions{{
        CardinalDirection::North, CardinalDirection::East,
        CardinalDirection::South, CardinalDirection::West}};
    EnemyEscapeChoice best;
    if (!positionAvailable)
        return best;

    float bestScore = -std::numeric_limits<float>::infinity();
    // Keep the legacy subtraction order before fabs for bit-for-bit behavior
    // under every supported floating-point environment.
    const float currentDistance = manhattanDistance(target, position);
    const int first = (tieBreaker % 4 + 4) % 4;

    for (int offset = 0; offset < 4; ++offset)
    {
        const CardinalDirection direction = directions[
            static_cast<std::size_t>((first + offset) % 4)];
        if (direction == blockedDirection)
            continue;

        const XZ aligned = core::snappedToCardinalLane(position, direction);
        if (!positionAvailable(aligned))
            continue;

        const XZ axis = core::cardinalVector(direction);
        int clearProbeCount = 0;
        XZ furthest = aligned;
        for (int probe = 1; probe <= 3; ++probe)
        {
            const XZ candidate = aligned + axis *
                                 (kEnemyEscapeProbeStep * probe);
            if (!positionAvailable(candidate))
                break;
            furthest = candidate;
            ++clearProbeCount;
        }
        if (clearProbeCount == 0)
            continue;

        const float candidateDistance = manhattanDistance(target, furthest);
        float score = currentDistance - candidateDistance +
                      clearProbeCount * 0.30f;
        // Prefer a side lane around an obstacle, but retain reverse as the
        // correct escape from a genuine dead end.
        if (direction == core::oppositeCardinalDirection(blockedDirection))
            score -= 0.35f;
        if (!best.available() || score > bestScore + 0.0001f)
        {
            best.direction = direction;
            best.alignedPosition = aligned;
            best.clearProbeCount = clearProbeCount;
            bestScore = score;
        }
    }
    return best;
}

EnemySteeringOutcome advanceActiveEnemySteering(
    Enemy &enemy, const EnemyFrameStart &frame, float dt,
    const EnemyPositionAvailable &positionAvailable,
    const EnemySteeringRandom &random)
{
    if (!frame.valid || frame.phase != EnemyFramePhase::Active ||
        !std::isfinite(dt) || dt < 0.0f || !positionAvailable ||
        !random.unitRoll || !random.directionRoll)
    {
        return EnemySteeringOutcome::Invalid;
    }

    enemy.directionTimer += dt;
    const bool ordinaryDirectionDecision =
        enemy.directionTimer > enemy.directionDecisionInterval;
    const bool forcedEscape =
        enemy.blockedTimer >= kEnemyBlockedEscapeDelay;
    if (!ordinaryDirectionDecision && !forcedEscape)
    {
        enemy.yaw = core::cardinalYaw(enemy.driveDirection);
        return EnemySteeringOutcome::NoDecision;
    }

    enemy.directionTimer = 0.0f;
    if (enemy.blockedTimer > 0.0f)
    {
        const EnemyEscapeChoice choice = chooseEnemyEscape(
            enemy.position, enemy.movementDirection, enemy.target,
            enemy.id, positionAvailable);
        if (commitEnemyEscape(enemy, choice))
        {
            enemy.directionDecisionInterval = kEnemyEscapeCommitTime +
                random.unitRoll() * kEnemyEscapeDecisionIntervalRange;
            enemy.blockedTimer = 0.0f;
            enemy.yaw = core::cardinalYaw(enemy.driveDirection);
            return EnemySteeringOutcome::EscapeCommitted;
        }

        enemy.directionDecisionInterval =
            kEnemyDirectionDecisionMinimumInterval;
        enemy.yaw = core::cardinalYaw(enemy.driveDirection);
        return EnemySteeringOutcome::EscapeUnavailable;
    }

    enemy.directionDecisionInterval =
        kEnemyDirectionDecisionMinimumInterval +
        random.unitRoll() * kEnemyDirectionDecisionIntervalRange;
    const float pursuitProbability = enemy.type == kBasicEnemyType
                                         ? kBasicEnemyPursuitProbability
                                         : kOtherEnemyPursuitProbability;
    if (random.unitRoll() < pursuitProbability)
    {
        enemy.driveDirection = chooseEnemyPursuitDirection(
            enemy.position, enemy.target, random.unitRoll());
    }
    else
    {
        enemy.driveDirection = kRandomEnemyDirections[
            static_cast<std::size_t>(random.directionRoll())];
    }
    enemy.yaw = core::cardinalYaw(enemy.driveDirection);
    return EnemySteeringOutcome::OrdinaryDecision;
}

XZ chooseEnemyTarget(int enemyType, XZ enemyPosition, XZ baseTarget,
                     const std::vector<Player> &players)
{
    if (enemyType != kBasicEnemyType && enemyType != kArmorEnemyType)
        return baseTarget;

    XZ target = baseTarget;
    float bestDistance = manhattanDistance(enemyPosition, target);
    for (const Player &player : players)
    {
        if (!player.active)
            continue;
        const float candidateDistance =
            manhattanDistance(enemyPosition, player.position);
        if (candidateDistance < bestDistance)
        {
            bestDistance = candidateDistance;
            target = player.position;
        }
    }
    return target;
}

CardinalDirection chooseEnemyPursuitDirection(XZ enemyPosition, XZ target,
                                              float primaryDirectionRoll)
{
    if (!finitePosition(enemyPosition) || !finitePosition(target) ||
        !std::isfinite(primaryDirectionRoll) || primaryDirectionRoll < 0.0f ||
        primaryDirectionRoll >= 1.0f)
    {
        return CardinalDirection::None;
    }

    const float dx = target.x - enemyPosition.x;
    const float dz = target.z - enemyPosition.z;
    const CardinalDirection horizontal =
        dx < 0.0f ? CardinalDirection::West : CardinalDirection::East;
    const CardinalDirection vertical =
        dz < 0.0f ? CardinalDirection::North : CardinalDirection::South;
    const bool horizontalDominant = std::fabs(dx) > std::fabs(dz);
    const CardinalDirection primary =
        horizontalDominant ? horizontal : vertical;
    const CardinalDirection secondary =
        horizontalDominant ? vertical : horizontal;
    return primaryDirectionRoll < kEnemyPursuitPrimaryProbability
               ? primary
               : secondary;
}

bool armorEnemyShouldFire(CardinalDirection movementDirection,
                          XZ enemyPosition, XZ target, bool blocked)
{
    const float dx = target.x - enemyPosition.x;
    const float dz = target.z - enemyPosition.z;
    switch (movementDirection)
    {
    case CardinalDirection::North:
        return blocked || (dz < 0.0f &&
                           std::fabs(dx) <
                               kArmorEnemyFiringLaneHalfWidth);
    case CardinalDirection::East:
        return blocked || (dx > 0.0f &&
                           std::fabs(dz) <
                               kArmorEnemyFiringLaneHalfWidth);
    case CardinalDirection::South:
        return blocked || (dz > 0.0f &&
                           std::fabs(dx) <
                               kArmorEnemyFiringLaneHalfWidth);
    case CardinalDirection::West:
        return blocked || (dx < 0.0f &&
                           std::fabs(dz) <
                               kArmorEnemyFiringLaneHalfWidth);
    case CardinalDirection::None:
        return blocked;
    }
    return false;
}

EnemyFirePlan planEnemyFire(const Enemy &enemy, bool blocked,
                            float reloadRoll)
{
    EnemyFirePlan plan;
    plan.baseReloadInterval = reloadRoll;
    if (enemy.type == kArmorEnemyType)
    {
        plan.baseReloadInterval *= 0.4f;
        plan.shouldFire = armorEnemyShouldFire(
            enemy.movementDirection, enemy.position, enemy.target, blocked);
    }
    else if (enemy.type == kPowerEnemyType)
    {
        plan.baseReloadInterval *= 0.8f;
    }
    return plan;
}

EnemyFireOutcome advanceEnemyFireTransaction(
    Enemy &enemy, bool blocked,
    const EnemyFireConfiguration &configuration,
    const EnemyFireRandom &random,
    const EnemyOwnedShellQuery &ownedShellPresent,
    const EnemyShellLaunch &launch)
{
    if (!std::isfinite(configuration.shellSpawnDistance) ||
        configuration.shellSpawnDistance < 0.0f || !random.reloadRoll ||
        !ownedShellPresent || !launch)
    {
        return EnemyFireOutcome::Invalid;
    }
    // Preserve the legacy `<= 0` gate exactly. NaN and positive infinity are
    // therefore inert rather than being reclassified as a due attempt.
    if (!(enemy.fireCooldown <= 0.0f))
        return EnemyFireOutcome::CoolingDown;

    const float reloadRoll = random.reloadRoll();
    if (!std::isfinite(reloadRoll) || reloadRoll < 0.0f ||
        reloadRoll >= 1.0f)
    {
        return EnemyFireOutcome::Invalid;
    }

    const EnemyFirePlan plan = planEnemyFire(enemy, blocked, reloadRoll);
    const float rateScale =
        1.0f + static_cast<float>(core::normalizedEnemyTuningPercent(
                   configuration.ratePercent)) /
                   100.0f;
    const float reloadInterval = plan.baseReloadInterval / rateScale;

    if (!plan.shouldFire)
    {
        enemy.fireCooldown = reloadInterval;
        return EnemyFireOutcome::AimRejected;
    }
    if (ownedShellPresent(enemy.id))
    {
        enemy.fireCooldown = reloadInterval;
        return EnemyFireOutcome::OwnedShellPresent;
    }

    EnemyShellLaunchIntent intent;
    intent.tankPosition = enemy.position;
    intent.direction = enemy.driveDirection;
    intent.enemyType = enemy.type;
    const XZ direction = core::cardinalVector(intent.direction);
    intent.shell.position =
        intent.tankPosition + direction * configuration.shellSpawnDistance;
    intent.shell.velocity =
        direction * (enemy.type == kPowerEnemyType
                         ? core::kFastShellSpeed
                         : core::kBaseShellSpeed);
    intent.shell.owner = ShellOwner::Enemy;
    intent.shell.ownerIndex = enemy.id;
    intent.shell.power = false;

    launch(intent);
    enemy.fireCooldown = reloadInterval;
    return EnemyFireOutcome::Fired;
}

float enemyArmorTankChanceForStage(int stage)
{
    return 0.00735f * static_cast<float>(stage) + 0.09265f;
}

bool enemyRollCreatesArmorTank(int stage, float roll)
{
    return roll < enemyArmorTankChanceForStage(stage);
}

bool enemyRollCreatesBonusCarrier(float roll)
{
    return roll < kBonusCarrierChance;
}

EnemyArmorThresholds enemyArmorThresholdsForStage(int stage)
{
    return stage <= 17
               ? EnemyArmorThresholds{
                     -0.040625 * stage + 0.940625,
                     -0.028125 * stage + 0.978125,
                     -0.014375 * stage + 0.994375}
               : EnemyArmorThresholds{
                     -0.012778 * stage + 0.467222,
                     -0.025000 * stage + 0.925000,
                     -0.036111 * stage + 1.363889};
}

int enemyArmorForRoll(int stage, float roll)
{
    const EnemyArmorThresholds thresholds =
        enemyArmorThresholdsForStage(stage);
    return roll < thresholds.oneHit       ? 1
           : roll < thresholds.twoHits   ? 2
           : roll < thresholds.threeHits ? 3
                                         : 4;
}

Enemy makeSpawnedEnemy(const EnemySpawnParameters &parameters)
{
    Enemy enemy;
    enemy.id = parameters.id;
    enemy.position = parameters.position;
    enemy.target = parameters.target;
    enemy.yaw = core::cardinalYaw(CardinalDirection::South);
    enemy.driveDirection = CardinalDirection::South;
    enemy.movementDirection = CardinalDirection::South;
    enemy.type = enemyRollCreatesArmorTank(parameters.stage,
                                            parameters.typeRoll)
                     ? kArmorEnemyType
                     : parameters.regularType;
    enemy.armor = enemyArmorForRoll(parameters.stage, parameters.armorRoll);
    enemy.carriesBonus =
        enemyRollCreatesBonusCarrier(parameters.carrierRoll);
    enemy.destroyed = false;
    enemy.moving = false;
    enemy.frozenTimer = 0.0f;
    enemy.fireCooldown = parameters.initialFireCooldown;
    enemy.creationTimer = kEnemyCreationDuration;
    enemy.deathTimer = 0.0f;
    enemy.blockedTimer = 0.0f;
    enemy.dustCooldown = 0.0f;
    enemy.directionTimer = 0.0f;
    enemy.directionDecisionInterval = 0.1f;
    enemy.movementDelay = 0.1f;
    enemy.iceSlipTimer = 0.0f;
    enemy.onIce = false;
    return enemy;
}

int chooseEnemySpawnIndex(
    const std::array<bool, kEnemySpawnSlotCount> &spawnAvailable,
    int nextSpawnIndex)
{
    const int slotCount = static_cast<int>(kEnemySpawnSlotCount);
    if (nextSpawnIndex < 0 || nextSpawnIndex >= slotCount)
        return -1;
    for (int attempt = 0; attempt < slotCount; ++attempt)
    {
        const int spawnIndex = (nextSpawnIndex + attempt) % slotCount;
        if (spawnAvailable[static_cast<std::size_t>(spawnIndex)])
            return spawnIndex;
    }
    return -1;
}

EnemySpawnOutcome advanceEnemySpawnTransaction(
    EnemySpawnState &state, float dt, std::size_t activeEnemyCount,
    const EnemySpawnConfiguration &configuration,
    const EnemyPositionAvailable &positionAvailable,
    const EnemySpawnRandom &random,
    const EnemySpawnCommit &commit)
{
    if (!std::isfinite(dt) || dt < 0.0f ||
        !validEnemySpawnState(state) ||
        !validEnemySpawnConfiguration(configuration) ||
        !positionAvailable || !random.unitRoll || !random.regularTypeRoll ||
        !commit)
    {
        return EnemySpawnOutcome::Invalid;
    }
    if (state.remaining <= 0)
        return EnemySpawnOutcome::QueueExhausted;
    if (state.nextEnemyId == std::numeric_limits<int>::max())
        return EnemySpawnOutcome::IdentifierExhausted;

    state.timer -= dt;
    if (activeEnemyCount >= kMaximumActiveEnemies)
        return EnemySpawnOutcome::CapacityFull;
    if (state.timer > 0.0f)
        return EnemySpawnOutcome::CoolingDown;

    std::array<bool, kEnemySpawnSlotCount> spawnAvailable{};
    for (int attempt = 0;
         attempt < static_cast<int>(kEnemySpawnSlotCount); ++attempt)
    {
        const int spawnIndex =
            (state.nextSpawnIndex + attempt) %
            static_cast<int>(kEnemySpawnSlotCount);
        spawnAvailable[static_cast<std::size_t>(spawnIndex)] =
            positionAvailable(configuration.spawnPoints[
                static_cast<std::size_t>(spawnIndex)]);
        if (spawnAvailable[static_cast<std::size_t>(spawnIndex)])
            break;
    }

    const int spawnIndex = chooseEnemySpawnIndex(
        spawnAvailable, state.nextSpawnIndex);
    if (spawnIndex < 0)
    {
        state.timer = configuration.retryInterval;
        return EnemySpawnOutcome::SpawnBlocked;
    }

    EnemySpawnParameters parameters;
    parameters.id = state.nextEnemyId++;
    parameters.stage = configuration.stage;
    parameters.position = configuration.spawnPoints[
        static_cast<std::size_t>(spawnIndex)];
    parameters.target = configuration.target;
    parameters.typeRoll = random.unitRoll();
    if (!enemyRollCreatesArmorTank(parameters.stage, parameters.typeRoll))
        parameters.regularType = random.regularTypeRoll();
    parameters.carrierRoll = random.unitRoll();
    parameters.armorRoll = random.unitRoll();
    parameters.initialFireCooldown = configuration.initialFireCooldown;

    const Enemy enemy = makeSpawnedEnemy(parameters);
    commit(enemy);
    --state.remaining;
    state.nextSpawnIndex =
        (spawnIndex + 1) % static_cast<int>(kEnemySpawnSlotCount);
    state.timer = configuration.normalInterval;
    return EnemySpawnOutcome::Spawned;
}
} // namespace tanks3d::game
