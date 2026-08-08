#include "game/enemy_system.h"

#include "test_support.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <string>
#include <vector>

namespace
{
using tanks3d::core::CardinalDirection;
using tanks3d::core::XZ;
using tanks3d::core::cardinalYaw;
using tanks3d::game::Enemy;
using tanks3d::game::EnemyArmorThresholds;
using tanks3d::game::EnemyEscapeChoice;
using tanks3d::game::EnemyFireConfiguration;
using tanks3d::game::EnemyFireOutcome;
using tanks3d::game::EnemyFirePlan;
using tanks3d::game::EnemyFireRandom;
using tanks3d::game::EnemyFramePhase;
using tanks3d::game::EnemyFrameStart;
using tanks3d::game::EnemyMovementUpdate;
using tanks3d::game::EnemyOwnedShellQuery;
using tanks3d::game::EnemyPositionAvailable;
using tanks3d::game::EnemyShellLaunch;
using tanks3d::game::EnemySpawnCommit;
using tanks3d::game::EnemySpawnConfiguration;
using tanks3d::game::EnemySpawnOutcome;
using tanks3d::game::EnemySpawnParameters;
using tanks3d::game::EnemySpawnRandom;
using tanks3d::game::EnemySpawnState;
using tanks3d::game::EnemySteeringOutcome;
using tanks3d::game::EnemySteeringRandom;
using tanks3d::game::EnemyShellLaunchIntent;
using tanks3d::game::Player;
using tanks3d::game::ShellOwner;
using tanks3d::game::advanceActiveEnemyMovement;
using tanks3d::game::advanceActiveEnemySteering;
using tanks3d::game::advanceEnemyFireTransaction;
using tanks3d::game::advanceEnemySpawnTransaction;
using tanks3d::game::armorEnemyShouldFire;
using tanks3d::game::beginEnemyFrame;
using tanks3d::game::chooseEnemyEscape;
using tanks3d::game::chooseEnemyPursuitDirection;
using tanks3d::game::chooseEnemySpawnIndex;
using tanks3d::game::chooseEnemyTarget;
using tanks3d::game::enemyArmorForRoll;
using tanks3d::game::enemyArmorTankChanceForStage;
using tanks3d::game::enemyArmorThresholdsForStage;
using tanks3d::game::enemyRollCreatesArmorTank;
using tanks3d::game::enemyRollCreatesBonusCarrier;
using tanks3d::game::kArmorEnemyFiringLaneHalfWidth;
using tanks3d::game::kArmorEnemyType;
using tanks3d::game::kBasicEnemyType;
using tanks3d::game::kBonusCarrierChance;
using tanks3d::game::kEnemyCreationDuration;
using tanks3d::game::kEnemyCollisionProbePadding;
using tanks3d::game::kEnemyBlockedEscapeDelay;
using tanks3d::game::kEnemyDirectionDecisionIntervalRange;
using tanks3d::game::kEnemyDirectionDecisionMinimumInterval;
using tanks3d::game::kEnemyEscapeCommitTime;
using tanks3d::game::kEnemyEscapeDecisionIntervalRange;
using tanks3d::game::kEnemyInitialFireDelay;
using tanks3d::game::kEnemyPursuitPrimaryProbability;
using tanks3d::game::kEnemyEscapeProbeStep;
using tanks3d::game::kEnemyTrackDustCooldown;
using tanks3d::game::kFastEnemyType;
using tanks3d::game::kMaximumActiveEnemies;
using tanks3d::game::kPowerEnemyType;
using tanks3d::game::makeSpawnedEnemy;
using tanks3d::game::planEnemyFire;

bool samePosition(XZ first, XZ second)
{
    return first.x == second.x && first.z == second.z;
}

Player playerAt(XZ position, bool active = true)
{
    Player player;
    player.position = position;
    player.active = active;
    return player;
}

bool nearlyEqual(float first, float second)
{
    return std::fabs(first - second) < 0.00001f;
}

bool nearlySamePosition(XZ first, XZ second)
{
    return nearlyEqual(first.x, second.x) && nearlyEqual(first.z, second.z);
}

EnemyFrameStart activeMovementFrame(
    bool hadMomentum, CardinalDirection previousTravel)
{
    EnemyFrameStart frame;
    frame.valid = true;
    frame.phase = EnemyFramePhase::Active;
    frame.hadMomentum = hadMomentum;
    frame.previousTravel = previousTravel;
    return frame;
}

bool rejectedMovementUpdate(const EnemyMovementUpdate &update)
{
    return !update.valid && !update.blocked && !update.movementAccepted &&
           !update.dust.has_value();
}

float largestFloatBelow(double threshold)
{
    float atOrAbove = static_cast<float>(threshold);
    while (static_cast<double>(atOrAbove) < threshold)
    {
        atOrAbove = std::nextafter(
            atOrAbove, std::numeric_limits<float>::infinity());
    }
    float below = std::nextafter(
        atOrAbove, -std::numeric_limits<float>::infinity());
    while (static_cast<double>(below) >= threshold)
    {
        below = std::nextafter(
            below, -std::numeric_limits<float>::infinity());
    }
    return below;
}

float smallestFloatAtOrAbove(double threshold)
{
    float candidate = static_cast<float>(threshold);
    while (static_cast<double>(candidate) < threshold)
    {
        candidate = std::nextafter(
            candidate, std::numeric_limits<float>::infinity());
    }
    return candidate;
}

Enemy frameFixture()
{
    Enemy enemy;
    enemy.id = 73;
    enemy.position = {4.0f, 5.0f};
    enemy.target = {13.0f, 23.6f};
    enemy.yaw = 1.25f;
    enemy.driveDirection = CardinalDirection::East;
    enemy.movementDirection = CardinalDirection::West;
    enemy.type = 3;
    enemy.armor = 4;
    enemy.carriesBonus = true;
    enemy.moving = true;
    enemy.frozenTimer = 0.40f;
    enemy.fireCooldown = 0.60f;
    enemy.creationTimer = 0.30f;
    enemy.deathTimer = 0.50f;
    enemy.blockedTimer = 0.20f;
    enemy.dustCooldown = 0.45f;
    enemy.directionTimer = 0.35f;
    enemy.directionDecisionInterval = 0.70f;
    enemy.movementDelay = 0.15f;
    enemy.iceSlipTimer = 0.25f;
    enemy.onIce = true;
    return enemy;
}

bool sameEnemyState(const Enemy &first, const Enemy &second)
{
    return first.id == second.id &&
           samePosition(first.position, second.position) &&
           samePosition(first.target, second.target) &&
           first.yaw == second.yaw &&
           first.driveDirection == second.driveDirection &&
           first.movementDirection == second.movementDirection &&
           first.type == second.type && first.armor == second.armor &&
           first.carriesBonus == second.carriesBonus &&
           first.destroyed == second.destroyed &&
           first.moving == second.moving &&
           first.frozenTimer == second.frozenTimer &&
           first.fireCooldown == second.fireCooldown &&
           first.creationTimer == second.creationTimer &&
           first.deathTimer == second.deathTimer &&
           first.blockedTimer == second.blockedTimer &&
           first.dustCooldown == second.dustCooldown &&
           first.directionTimer == second.directionTimer &&
           first.directionDecisionInterval ==
               second.directionDecisionInterval &&
           first.movementDelay == second.movementDelay &&
           first.iceSlipTimer == second.iceSlipTimer &&
           first.onIce == second.onIce;
}

bool sameEnemySpawnState(const EnemySpawnState &first,
                         const EnemySpawnState &second)
{
    return first.remaining == second.remaining &&
           first.nextSpawnIndex == second.nextSpawnIndex &&
           first.nextEnemyId == second.nextEnemyId &&
           (first.timer == second.timer ||
            (std::isnan(first.timer) && std::isnan(second.timer)));
}

EnemySpawnConfiguration spawnConfigurationFixture()
{
    EnemySpawnConfiguration configuration;
    configuration.stage = 17;
    configuration.spawnPoints = {{{1.0f, 1.0f},
                                  {13.0f, 1.0f},
                                  {25.0f, 1.0f}}};
    configuration.target = {13.0f, 23.6f};
    configuration.initialFireCooldown = 0.125f;
    configuration.normalInterval = 0.70f;
    configuration.retryInterval = 0.20f;
    return configuration;
}

EnemyFireConfiguration fireConfigurationFixture(int ratePercent = 0)
{
    EnemyFireConfiguration configuration;
    configuration.ratePercent = ratePercent;
    configuration.shellSpawnDistance = 0.625f;
    return configuration;
}
} // namespace

int main()
{
    tanks3d_test::Reporter reporter;
    reporter.reset();
    bool passed = true;
    const auto expect = [&](bool condition, const std::string &message) {
        if (!reporter.check(condition, message))
            passed = false;
    };

    reporter.beginSuite("enemy-frame-invalid-elapsed-time-is-atomic");
    Enemy invalidEnemy = frameFixture();
    const Enemy invalidBefore = invalidEnemy;
    const auto negativeFrame = beginEnemyFrame(invalidEnemy, -0.01f, false);
    expect(!negativeFrame.valid &&
               negativeFrame.phase == EnemyFramePhase::Active &&
               !negativeFrame.hadMomentum &&
               negativeFrame.previousTravel == CardinalDirection::None &&
               sameEnemyState(invalidEnemy, invalidBefore),
           "negative elapsed time partially changed enemy state");
    const auto nanFrame = beginEnemyFrame(
        invalidEnemy, std::numeric_limits<float>::quiet_NaN(), true);
    expect(!nanFrame.valid && sameEnemyState(invalidEnemy, invalidBefore),
           "NaN elapsed time partially changed enemy state");
    const auto infiniteFrame = beginEnemyFrame(
        invalidEnemy, std::numeric_limits<float>::infinity(), false);
    expect(!infiniteFrame.valid && sameEnemyState(invalidEnemy, invalidBefore),
           "infinite elapsed time partially changed enemy state");

    reporter.beginSuite("enemy-frame-destroyed-clock-only");
    Enemy destroyedEnemy = frameFixture();
    destroyedEnemy.destroyed = true;
    destroyedEnemy.deathTimer = 0.20f;
    const float destroyedFrozenBefore = destroyedEnemy.frozenTimer;
    const float destroyedCreationBefore = destroyedEnemy.creationTimer;
    const float destroyedDustBefore = destroyedEnemy.dustCooldown;
    const float destroyedFireBefore = destroyedEnemy.fireCooldown;
    const float destroyedIceBefore = destroyedEnemy.iceSlipTimer;
    const auto destroyedFrame = beginEnemyFrame(destroyedEnemy, 0.30f, true);
    expect(destroyedFrame.valid &&
               destroyedFrame.phase == EnemyFramePhase::Destroyed &&
               !destroyedFrame.hadMomentum &&
               destroyedFrame.previousTravel == CardinalDirection::None,
           "destroyed frame returned the wrong gate or synthetic snapshots");
    expect(nearlyEqual(destroyedEnemy.deathTimer, 0.0f) &&
               destroyedEnemy.moving &&
               destroyedEnemy.frozenTimer == destroyedFrozenBefore &&
               destroyedEnemy.creationTimer == destroyedCreationBefore &&
               destroyedEnemy.dustCooldown == destroyedDustBefore &&
               destroyedEnemy.fireCooldown == destroyedFireBefore &&
               destroyedEnemy.iceSlipTimer == destroyedIceBefore,
           "destroyed frame changed clocks other than clamped death time");
    destroyedEnemy.deathTimer = 0.50f;
    const auto liveDeathCountdown = beginEnemyFrame(
        destroyedEnemy, 0.125f, false);
    expect(liveDeathCountdown.valid &&
               liveDeathCountdown.phase == EnemyFramePhase::Destroyed &&
               nearlyEqual(destroyedEnemy.deathTimer, 0.375f),
           "destroyed death clock did not preserve a positive remainder");

    reporter.beginSuite("enemy-frame-creating-clock-and-hold-policy");
    Enemy creatingEnemy = frameFixture();
    creatingEnemy.creationTimer = 0.15f;
    creatingEnemy.frozenTimer = 0.40f;
    const float creatingDustBefore = creatingEnemy.dustCooldown;
    const float creatingFireBefore = creatingEnemy.fireCooldown;
    const float creatingIceBefore = creatingEnemy.iceSlipTimer;
    const auto creatingFrame = beginEnemyFrame(creatingEnemy, 0.20f, false);
    expect(creatingFrame.valid &&
               creatingFrame.phase == EnemyFramePhase::Creating &&
               creatingFrame.hadMomentum &&
               creatingFrame.previousTravel == CardinalDirection::West &&
               !creatingEnemy.moving,
           "creating frame lost movement snapshots or failed to stop motion");
    expect(nearlyEqual(creatingEnemy.creationTimer, 0.0f) &&
               nearlyEqual(creatingEnemy.frozenTimer, 0.20f) &&
               creatingEnemy.dustCooldown == creatingDustBefore &&
               creatingEnemy.fireCooldown == creatingFireBefore &&
               creatingEnemy.iceSlipTimer == creatingIceBefore,
           "creating frame advanced the wrong clocks or missed clamping");

    Enemy heldCreationEnemy = frameFixture();
    heldCreationEnemy.creationTimer = 0.25f;
    heldCreationEnemy.frozenTimer = 0.10f;
    heldCreationEnemy.moving = false;
    heldCreationEnemy.movementDirection = CardinalDirection::North;
    const auto heldCreatingFrame = beginEnemyFrame(
        heldCreationEnemy, 0.20f, true);
    expect(heldCreatingFrame.valid &&
               heldCreatingFrame.phase == EnemyFramePhase::Creating &&
               !heldCreatingFrame.hadMomentum &&
               heldCreatingFrame.previousTravel == CardinalDirection::North &&
               nearlyEqual(heldCreationEnemy.creationTimer, 0.25f) &&
               nearlyEqual(heldCreationEnemy.frozenTimer, 0.0f),
           "held creation changed its warning timer or froze Clock duration");

    reporter.beginSuite("enemy-frame-frozen-clock-policy");
    Enemy frozenEnemy = frameFixture();
    frozenEnemy.creationTimer = 0.0f;
    frozenEnemy.frozenTimer = 0.15f;
    frozenEnemy.dustCooldown = 0.10f;
    frozenEnemy.iceSlipTimer = 0.05f;
    frozenEnemy.fireCooldown = 0.30f;
    const auto frozenFrame = beginEnemyFrame(frozenEnemy, 0.20f, true);
    expect(frozenFrame.valid &&
               frozenFrame.phase == EnemyFramePhase::Frozen &&
               frozenFrame.hadMomentum &&
               frozenFrame.previousTravel == CardinalDirection::West &&
               !frozenEnemy.moving,
           "frozen frame lost movement snapshots or returned the wrong gate");
    expect(nearlyEqual(frozenEnemy.dustCooldown, 0.0f) &&
               nearlyEqual(frozenEnemy.frozenTimer, 0.0f) &&
               nearlyEqual(frozenEnemy.iceSlipTimer, 0.0f) &&
               nearlyEqual(frozenEnemy.fireCooldown, 0.30f),
           "frozen frame missed clamping or advanced its fire cooldown");

    reporter.beginSuite("enemy-frame-active-clock-and-snapshots");
    Enemy activeEnemy = frameFixture();
    activeEnemy.creationTimer = 0.0f;
    activeEnemy.frozenTimer = 0.0f;
    activeEnemy.dustCooldown = 0.10f;
    activeEnemy.fireCooldown = 0.05f;
    activeEnemy.iceSlipTimer = 0.25f;
    const auto activeFrame = beginEnemyFrame(activeEnemy, 0.20f, false);
    expect(activeFrame.valid && activeFrame.phase == EnemyFramePhase::Active &&
               activeFrame.hadMomentum &&
               activeFrame.previousTravel == CardinalDirection::West &&
               !activeEnemy.moving,
           "active frame lost the pre-reset movement snapshot");
    expect(nearlyEqual(activeEnemy.dustCooldown, 0.0f) &&
               nearlyEqual(activeEnemy.fireCooldown, -0.15f) &&
               nearlyEqual(activeEnemy.iceSlipTimer, 0.25f),
           "active frame clamped reload or changed the ice timer");

    Enemy zeroElapsedEnemy = frameFixture();
    zeroElapsedEnemy.creationTimer = 0.0f;
    zeroElapsedEnemy.frozenTimer = 0.0f;
    const float zeroDustBefore = zeroElapsedEnemy.dustCooldown;
    const float zeroFireBefore = zeroElapsedEnemy.fireCooldown;
    const auto zeroElapsedFrame = beginEnemyFrame(
        zeroElapsedEnemy, 0.0f, true);
    expect(zeroElapsedFrame.valid &&
               zeroElapsedFrame.phase == EnemyFramePhase::Active &&
               zeroElapsedFrame.hadMomentum &&
               zeroElapsedFrame.previousTravel == CardinalDirection::West &&
               !zeroElapsedEnemy.moving &&
               zeroElapsedEnemy.dustCooldown == zeroDustBefore &&
               zeroElapsedEnemy.fireCooldown == zeroFireBefore,
           "zero elapsed time was rejected or changed active clocks");

    reporter.beginSuite("enemy-movement-invalid-input-is-atomic");
    int invalidMovementQueries = 0;
    const EnemyPositionAvailable countingAvailability = [&](XZ) {
        ++invalidMovementQueries;
        return true;
    };

    Enemy invalidFrameEnemy = frameFixture();
    const Enemy invalidFrameBefore = invalidFrameEnemy;
    const EnemyMovementUpdate invalidFrameMovement =
        advanceActiveEnemyMovement(
            invalidFrameEnemy, EnemyFrameStart{}, 0.1f, 2.0f, false,
            countingAvailability);
    expect(rejectedMovementUpdate(invalidFrameMovement) &&
               sameEnemyState(invalidFrameEnemy, invalidFrameBefore) &&
               invalidMovementQueries == 0,
           "invalid frame changed movement state or queried availability");

    bool inactiveFramesAreAtomic = true;
    for (EnemyFramePhase phase : {EnemyFramePhase::Destroyed,
                                  EnemyFramePhase::Creating,
                                  EnemyFramePhase::Frozen})
    {
        Enemy inactiveEnemy = frameFixture();
        const Enemy inactiveBefore = inactiveEnemy;
        EnemyFrameStart inactiveFrame = activeMovementFrame(
            true, inactiveEnemy.movementDirection);
        inactiveFrame.phase = phase;
        const EnemyMovementUpdate inactiveMovement =
            advanceActiveEnemyMovement(
                inactiveEnemy, inactiveFrame, 0.1f, 2.0f, false,
                countingAvailability);
        inactiveFramesAreAtomic =
            inactiveFramesAreAtomic &&
            rejectedMovementUpdate(inactiveMovement) &&
            sameEnemyState(inactiveEnemy, inactiveBefore);
    }
    expect(inactiveFramesAreAtomic && invalidMovementQueries == 0,
           "non-active frame changed movement state or queried availability");

    const float movementQuietNaN =
        std::numeric_limits<float>::quiet_NaN();
    const float movementInfinity =
        std::numeric_limits<float>::infinity();
    bool invalidElapsedTimesAreAtomic = true;
    for (float invalidDt : {-0.01f, movementQuietNaN, movementInfinity})
    {
        Enemy invalidDtEnemy = frameFixture();
        const Enemy invalidDtBefore = invalidDtEnemy;
        const EnemyMovementUpdate invalidDtMovement =
            advanceActiveEnemyMovement(
                invalidDtEnemy,
                activeMovementFrame(true,
                                    invalidDtEnemy.movementDirection),
                invalidDt, 2.0f, false, countingAvailability);
        invalidElapsedTimesAreAtomic =
            invalidElapsedTimesAreAtomic &&
            rejectedMovementUpdate(invalidDtMovement) &&
            sameEnemyState(invalidDtEnemy, invalidDtBefore);
    }
    expect(invalidElapsedTimesAreAtomic && invalidMovementQueries == 0,
           "invalid movement dt changed state or queried availability");

    bool invalidSpeedsAreAtomic = true;
    for (float invalidSpeed : {-0.01f, movementQuietNaN,
                               movementInfinity})
    {
        Enemy invalidSpeedEnemy = frameFixture();
        const Enemy invalidSpeedBefore = invalidSpeedEnemy;
        const EnemyMovementUpdate invalidSpeedMovement =
            advanceActiveEnemyMovement(
                invalidSpeedEnemy,
                activeMovementFrame(true,
                                    invalidSpeedEnemy.movementDirection),
                0.1f, invalidSpeed, false, countingAvailability);
        invalidSpeedsAreAtomic =
            invalidSpeedsAreAtomic &&
            rejectedMovementUpdate(invalidSpeedMovement) &&
            sameEnemyState(invalidSpeedEnemy, invalidSpeedBefore);
    }
    expect(invalidSpeedsAreAtomic && invalidMovementQueries == 0,
           "invalid movement speed changed state or queried availability");

    Enemy emptyQueryEnemy = frameFixture();
    const Enemy emptyQueryBefore = emptyQueryEnemy;
    const EnemyMovementUpdate emptyQueryMovement =
        advanceActiveEnemyMovement(
            emptyQueryEnemy,
            activeMovementFrame(true, emptyQueryEnemy.movementDirection),
            0.1f, 2.0f, false, EnemyPositionAvailable{});
    expect(rejectedMovementUpdate(emptyQueryMovement) &&
               sameEnemyState(emptyQueryEnemy, emptyQueryBefore),
           "empty movement availability query did not fail atomically");

    reporter.beginSuite("enemy-movement-stationary-transition");
    Enemy stationaryEnemy = frameFixture();
    stationaryEnemy.position = {2.25f, 6.75f};
    stationaryEnemy.driveDirection = CardinalDirection::East;
    stationaryEnemy.movementDirection = CardinalDirection::North;
    stationaryEnemy.moving = false;
    stationaryEnemy.movementDelay = 0.25f;
    stationaryEnemy.iceSlipTimer = 0.30f;
    stationaryEnemy.onIce = true;
    stationaryEnemy.blockedTimer = 0.42f;
    stationaryEnemy.dustCooldown = 0.0f;
    int stationaryQueries = 0;
    const EnemyMovementUpdate stationaryMovement =
        advanceActiveEnemyMovement(
            stationaryEnemy,
            activeMovementFrame(false, CardinalDirection::North),
            0.10f, 2.0f, false,
            [&](XZ) {
                ++stationaryQueries;
                return false;
            });
    expect(stationaryMovement.valid && !stationaryMovement.blocked &&
               !stationaryMovement.movementAccepted &&
               !stationaryMovement.dust.has_value() &&
               stationaryQueries == 0,
           "stationary transition queried collision or reported movement");
    expect(nearlySamePosition(stationaryEnemy.position, {2.25f, 6.75f}) &&
               nearlyEqual(stationaryEnemy.movementDelay, 0.15f) &&
               !stationaryEnemy.moving &&
               stationaryEnemy.movementDirection == CardinalDirection::East &&
               nearlyEqual(stationaryEnemy.iceSlipTimer, 0.0f) &&
               !stationaryEnemy.onIce &&
               nearlyEqual(stationaryEnemy.blockedTimer, 0.42f) &&
               nearlyEqual(stationaryEnemy.dustCooldown, 0.0f),
           "stationary transition changed position or non-movement state");

    reporter.beginSuite("enemy-movement-forward-probe-and-commit");
    Enemy forwardEnemy = frameFixture();
    forwardEnemy.position = {1.5f, -2.0f};
    forwardEnemy.driveDirection = CardinalDirection::South;
    forwardEnemy.movementDirection = CardinalDirection::South;
    forwardEnemy.moving = false;
    forwardEnemy.movementDelay = 0.05f;
    forwardEnemy.blockedTimer = 0.40f;
    forwardEnemy.dustCooldown = 0.30f;
    std::vector<XZ> forwardQueries;
    const float forwardDt = 0.10f;
    const float forwardSpeed = 2.0f;
    const EnemyMovementUpdate forwardMovement =
        advanceActiveEnemyMovement(
            forwardEnemy,
            activeMovementFrame(false, CardinalDirection::South),
            forwardDt, forwardSpeed, false,
            [&](XZ candidate) {
                forwardQueries.push_back(candidate);
                return true;
            });
    const XZ expectedForwardProbe{
        1.5f, -2.0f + forwardSpeed * forwardDt +
                  kEnemyCollisionProbePadding};
    expect(forwardMovement.valid && !forwardMovement.blocked &&
               forwardMovement.movementAccepted &&
               !forwardMovement.dust.has_value() &&
               forwardQueries.size() == 1U &&
               nearlySamePosition(forwardQueries[0], expectedForwardProbe),
           "forward movement changed its single collision probe transcript");
    expect(nearlySamePosition(forwardEnemy.position, {1.5f, -1.8f}) &&
               nearlyEqual(forwardEnemy.movementDelay, 0.0f) &&
               forwardEnemy.moving &&
               nearlyEqual(forwardEnemy.blockedTimer, 0.0f),
           "available forward movement did not commit or clear blocking");

    Enemy zeroDistanceEnemy = frameFixture();
    zeroDistanceEnemy.position = {3.0f, 4.0f};
    zeroDistanceEnemy.driveDirection = CardinalDirection::East;
    zeroDistanceEnemy.movementDirection = CardinalDirection::East;
    zeroDistanceEnemy.movementDelay = 0.0f;
    zeroDistanceEnemy.dustCooldown = 0.30f;
    int zeroDistanceQueries = 0;
    const EnemyMovementUpdate zeroDistanceMovement =
        advanceActiveEnemyMovement(
            zeroDistanceEnemy,
            activeMovementFrame(false, CardinalDirection::East),
            0.0f, 0.0f, false,
            [&](XZ candidate) {
                ++zeroDistanceQueries;
                return nearlySamePosition(
                    candidate,
                    {3.0f + kEnemyCollisionProbePadding, 4.0f});
            });
    expect(zeroDistanceMovement.valid &&
               zeroDistanceMovement.movementAccepted &&
               !zeroDistanceMovement.blocked && zeroDistanceQueries == 1 &&
               nearlySamePosition(zeroDistanceEnemy.position, {3.0f, 4.0f}),
           "zero dt or speed stopped being a valid accepted movement");

    reporter.beginSuite("enemy-movement-snap-query-order");
    Enemy acceptedSnapEnemy = frameFixture();
    acceptedSnapEnemy.position = {4.18f, 7.73f};
    acceptedSnapEnemy.driveDirection = CardinalDirection::East;
    acceptedSnapEnemy.movementDirection = CardinalDirection::North;
    acceptedSnapEnemy.movementDelay = 0.0f;
    acceptedSnapEnemy.dustCooldown = 0.30f;
    std::vector<XZ> acceptedSnapQueries;
    const float snapDt = 0.25f;
    const float snapSpeed = 2.0f;
    const EnemyMovementUpdate acceptedSnapMovement =
        advanceActiveEnemyMovement(
            acceptedSnapEnemy,
            activeMovementFrame(false, CardinalDirection::North),
            snapDt, snapSpeed, false,
            [&](XZ candidate) {
                acceptedSnapQueries.push_back(candidate);
                return true;
            });
    const XZ expectedSnappedPosition{4.18f, 8.0f};
    const std::vector<XZ> expectedAcceptedSnapQueries{
        expectedSnappedPosition,
        {expectedSnappedPosition.x + snapSpeed * snapDt +
             kEnemyCollisionProbePadding,
         expectedSnappedPosition.z}};
    expect(acceptedSnapMovement.valid &&
               acceptedSnapMovement.movementAccepted &&
               !acceptedSnapMovement.blocked &&
               acceptedSnapQueries.size() ==
                   expectedAcceptedSnapQueries.size() &&
               std::equal(acceptedSnapQueries.begin(),
                          acceptedSnapQueries.end(),
                          expectedAcceptedSnapQueries.begin(),
                          nearlySamePosition) &&
               nearlySamePosition(
                   acceptedSnapEnemy.position,
                   {expectedSnappedPosition.x + snapSpeed * snapDt,
                    expectedSnappedPosition.z}),
           "accepted lane snap did not precede and feed collision probing");

    Enemy rejectedSnapEnemy = frameFixture();
    rejectedSnapEnemy.position = {4.18f, 7.73f};
    rejectedSnapEnemy.driveDirection = CardinalDirection::East;
    rejectedSnapEnemy.movementDirection = CardinalDirection::North;
    rejectedSnapEnemy.movementDelay = 0.0f;
    rejectedSnapEnemy.dustCooldown = 0.30f;
    std::vector<XZ> rejectedSnapQueries;
    const EnemyMovementUpdate rejectedSnapMovement =
        advanceActiveEnemyMovement(
            rejectedSnapEnemy,
            activeMovementFrame(false, CardinalDirection::North),
            snapDt, snapSpeed, false,
            [&](XZ candidate) {
                rejectedSnapQueries.push_back(candidate);
                return rejectedSnapQueries.size() != 1U;
            });
    const std::vector<XZ> expectedRejectedSnapQueries{
        expectedSnappedPosition,
        {4.18f + snapSpeed * snapDt + kEnemyCollisionProbePadding,
         7.73f}};
    expect(rejectedSnapMovement.valid &&
               rejectedSnapMovement.movementAccepted &&
               !rejectedSnapMovement.blocked &&
               rejectedSnapQueries.size() ==
                   expectedRejectedSnapQueries.size() &&
               std::equal(rejectedSnapQueries.begin(),
                          rejectedSnapQueries.end(),
                          expectedRejectedSnapQueries.begin(),
                          nearlySamePosition) &&
               nearlySamePosition(rejectedSnapEnemy.position,
                                  {4.68f, 7.73f}),
           "rejected lane snap did not probe and move from original position");

    reporter.beginSuite("enemy-movement-ice-continuation-and-expiry");
    Enemy continuingIceEnemy = frameFixture();
    continuingIceEnemy.position = {4.0f, 5.0f};
    continuingIceEnemy.driveDirection = CardinalDirection::East;
    continuingIceEnemy.movementDirection = CardinalDirection::North;
    continuingIceEnemy.movementDelay = 0.40f;
    continuingIceEnemy.iceSlipTimer = 0.25f;
    continuingIceEnemy.onIce = true;
    continuingIceEnemy.blockedTimer = 0.30f;
    continuingIceEnemy.dustCooldown = 0.30f;
    std::vector<XZ> continuingIceQueries;
    const EnemyMovementUpdate continuingIceMovement =
        advanceActiveEnemyMovement(
            continuingIceEnemy,
            activeMovementFrame(true, CardinalDirection::North),
            0.10f, 1.5f, true,
            [&](XZ candidate) {
                continuingIceQueries.push_back(candidate);
                return true;
            });
    expect(continuingIceMovement.valid &&
               continuingIceMovement.movementAccepted &&
               !continuingIceMovement.blocked &&
               continuingIceQueries.size() == 1U &&
               nearlySamePosition(continuingIceQueries[0],
                                  {4.0f, 5.0f - 0.15f -
                                             kEnemyCollisionProbePadding}) &&
               nearlySamePosition(continuingIceEnemy.position,
                                  {4.0f, 4.85f}) &&
               continuingIceEnemy.moving &&
               continuingIceEnemy.movementDirection ==
                   CardinalDirection::North &&
               nearlyEqual(continuingIceEnemy.iceSlipTimer, 0.15f) &&
               continuingIceEnemy.onIce &&
               nearlyEqual(continuingIceEnemy.blockedTimer, 0.0f),
           "ice momentum did not continue along prior travel direction");

    Enemy expiringIceEnemy = frameFixture();
    expiringIceEnemy.position = {4.0f, 5.0f};
    expiringIceEnemy.driveDirection = CardinalDirection::East;
    expiringIceEnemy.movementDirection = CardinalDirection::North;
    expiringIceEnemy.movementDelay = 0.40f;
    expiringIceEnemy.iceSlipTimer = 0.05f;
    expiringIceEnemy.onIce = true;
    expiringIceEnemy.blockedTimer = 0.30f;
    expiringIceEnemy.dustCooldown = 0.30f;
    int expiringIceQueries = 0;
    const EnemyMovementUpdate expiringIceMovement =
        advanceActiveEnemyMovement(
            expiringIceEnemy,
            activeMovementFrame(true, CardinalDirection::North),
            0.10f, 1.5f, true,
            [&](XZ) {
                ++expiringIceQueries;
                return true;
            });
    expect(expiringIceMovement.valid && !expiringIceMovement.blocked &&
               !expiringIceMovement.movementAccepted &&
               !expiringIceMovement.dust.has_value() &&
               expiringIceQueries == 0 &&
               nearlySamePosition(expiringIceEnemy.position, {4.0f, 5.0f}) &&
               !expiringIceEnemy.moving &&
               expiringIceEnemy.movementDirection ==
                   CardinalDirection::East &&
               nearlyEqual(expiringIceEnemy.iceSlipTimer, 0.0f) &&
               expiringIceEnemy.onIce &&
               nearlyEqual(expiringIceEnemy.blockedTimer, 0.30f),
           "expired ice momentum moved, probed, or changed blocking state");

    reporter.beginSuite("enemy-movement-blocked-transition-and-cap");
    Enemy blockedIceEnemy = frameFixture();
    blockedIceEnemy.position = {4.0f, 5.0f};
    blockedIceEnemy.driveDirection = CardinalDirection::East;
    blockedIceEnemy.movementDirection = CardinalDirection::North;
    blockedIceEnemy.movementDelay = 0.0f;
    blockedIceEnemy.iceSlipTimer = 0.30f;
    blockedIceEnemy.onIce = true;
    blockedIceEnemy.blockedTimer = 0.20f;
    blockedIceEnemy.dustCooldown = 0.0f;
    int blockedIceQueries = 0;
    const EnemyMovementUpdate blockedIceMovement =
        advanceActiveEnemyMovement(
            blockedIceEnemy,
            activeMovementFrame(true, CardinalDirection::North),
            0.10f, 1.0f, true,
            [&](XZ candidate) {
                ++blockedIceQueries;
                return !nearlySamePosition(
                    candidate,
                    {4.0f, 5.0f - 0.10f -
                               kEnemyCollisionProbePadding});
            });
    expect(blockedIceMovement.valid && blockedIceMovement.blocked &&
               !blockedIceMovement.movementAccepted &&
               !blockedIceMovement.dust.has_value() &&
               blockedIceQueries == 1 &&
               nearlySamePosition(blockedIceEnemy.position, {4.0f, 5.0f}) &&
               !blockedIceEnemy.moving &&
               blockedIceEnemy.movementDirection == CardinalDirection::East &&
               nearlyEqual(blockedIceEnemy.iceSlipTimer, 0.0f) &&
               nearlyEqual(blockedIceEnemy.blockedTimer, 0.30f),
           "blocked movement did not stop ice drift and accumulate timer");

    Enemy cappedBlockedEnemy = frameFixture();
    cappedBlockedEnemy.position = {1.0f, 2.0f};
    cappedBlockedEnemy.driveDirection = CardinalDirection::South;
    cappedBlockedEnemy.movementDirection = CardinalDirection::South;
    cappedBlockedEnemy.movementDelay = 0.0f;
    cappedBlockedEnemy.blockedTimer = 0.79f;
    cappedBlockedEnemy.dustCooldown = 0.0f;
    int cappedBlockedQueries = 0;
    const EnemyMovementUpdate cappedBlockedMovement =
        advanceActiveEnemyMovement(
            cappedBlockedEnemy,
            activeMovementFrame(false, CardinalDirection::South),
            0.25f, 2.0f, false,
            [&](XZ) {
                ++cappedBlockedQueries;
                return false;
            });
    expect(cappedBlockedMovement.valid && cappedBlockedMovement.blocked &&
               !cappedBlockedMovement.movementAccepted &&
               !cappedBlockedMovement.dust.has_value() &&
               cappedBlockedQueries == 1 &&
               nearlyEqual(cappedBlockedEnemy.blockedTimer, 0.80f),
           "blocked timer exceeded or missed its 0.8-second cap");

    reporter.beginSuite("enemy-movement-detached-dust-boundary");
    Enemy dustEnemy = frameFixture();
    dustEnemy.position = {3.0f, 4.0f};
    dustEnemy.driveDirection = CardinalDirection::West;
    dustEnemy.movementDirection = CardinalDirection::West;
    dustEnemy.movementDelay = 0.0f;
    dustEnemy.dustCooldown = 0.0f;
    const EnemyMovementUpdate dustMovement = advanceActiveEnemyMovement(
        dustEnemy,
        activeMovementFrame(false, CardinalDirection::West),
        0.25f, 2.0f, false, [](XZ) { return true; });
    expect(dustMovement.valid && dustMovement.movementAccepted &&
               !dustMovement.blocked && dustMovement.dust.has_value() &&
               nearlySamePosition(dustEnemy.position, {2.5f, 4.0f}) &&
               nearlySamePosition(dustMovement.dust->position,
                                  {3.08f, 4.0f}) &&
               nearlySamePosition(dustMovement.dust->velocity,
                                  {-2.0f, 0.0f}),
           "dust intent did not use post-move position, offset, or velocity");
    expect(nearlyEqual(dustEnemy.dustCooldown, 0.0f) &&
               nearlyEqual(kEnemyTrackDustCooldown, 0.19f),
           "movement helper committed the detached dust cooldown");

    Enemy positiveDustEnemy = frameFixture();
    positiveDustEnemy.position = {3.0f, 4.0f};
    positiveDustEnemy.driveDirection = CardinalDirection::West;
    positiveDustEnemy.movementDirection = CardinalDirection::West;
    positiveDustEnemy.movementDelay = 0.0f;
    positiveDustEnemy.dustCooldown = std::nextafter(
        0.0f, std::numeric_limits<float>::infinity());
    const float positiveDustCooldown = positiveDustEnemy.dustCooldown;
    const EnemyMovementUpdate positiveDustMovement =
        advanceActiveEnemyMovement(
            positiveDustEnemy,
            activeMovementFrame(false, CardinalDirection::West),
            0.25f, 2.0f, false, [](XZ) { return true; });
    expect(positiveDustMovement.valid &&
               positiveDustMovement.movementAccepted &&
               !positiveDustMovement.dust.has_value() &&
               positiveDustEnemy.dustCooldown == positiveDustCooldown,
           "strictly positive dust cooldown emitted or was mutated");

    reporter.beginSuite("enemy-escape-closed-and-short-paths");
    const EnemyEscapeChoice emptyQueryChoice = chooseEnemyEscape(
        {0.0f, 0.0f}, CardinalDirection::South, {0.0f, -10.0f}, 0, {});
    expect(!emptyQueryChoice.available() &&
               emptyQueryChoice.direction == CardinalDirection::None &&
               emptyQueryChoice.clearProbeCount == 0,
           "empty escape availability query did not fail closed");

    int sealedQueries = 0;
    const EnemyEscapeChoice sealedChoice = chooseEnemyEscape(
        {0.0f, 0.0f}, CardinalDirection::South, {0.0f, -10.0f}, 0,
        [&](XZ) {
            ++sealedQueries;
            return false;
        });
    expect(!sealedChoice.available() && sealedQueries == 3,
           "sealed escape did not skip the blocked direction or queried probes");

    int noProbeQueries = 0;
    const EnemyEscapeChoice noProbeChoice = chooseEnemyEscape(
        {0.0f, 0.0f}, CardinalDirection::South, {0.0f, -10.0f}, 0,
        [&](XZ candidate) {
            ++noProbeQueries;
            return nearlyEqual(candidate.x, 0.0f) &&
                   nearlyEqual(candidate.z, 0.0f);
        });
    expect(!noProbeChoice.available() && noProbeQueries == 6,
           "escape accepted a snapped lane without a clear forward probe");

    reporter.beginSuite("enemy-escape-dead-end-and-probe-count");
    const EnemyEscapeChoice deadEndChoice = chooseEnemyEscape(
        {0.0f, 0.0f}, CardinalDirection::South, {0.0f, -10.0f}, 0,
        [](XZ candidate) {
            return nearlyEqual(candidate.x, 0.0f) &&
                   candidate.z <= 0.0f && candidate.z >= -1.2001f;
        });
    expect(deadEndChoice.available() &&
               deadEndChoice.direction == CardinalDirection::North &&
               deadEndChoice.clearProbeCount == 3 &&
               samePosition(deadEndChoice.alignedPosition, {0.0f, 0.0f}),
           "dead-end escape did not retain the legal reverse route");

    const EnemyEscapeChoice oneProbeChoice = chooseEnemyEscape(
        {0.0f, 0.0f}, CardinalDirection::South, {0.0f, -10.0f}, 0,
        [](XZ candidate) {
            return nearlyEqual(candidate.x, 0.0f) &&
                   candidate.z <= 0.0f &&
                   candidate.z >= -kEnemyEscapeProbeStep - 0.00001f;
        });
    expect(oneProbeChoice.direction == CardinalDirection::North &&
               oneProbeChoice.clearProbeCount == 1,
           "escape probe scan did not stop at the first blocked sample");

    reporter.beginSuite("enemy-escape-scoring-ties-and-snapping");
    const auto openTerrain = [](XZ) { return true; };
    const EnemyEscapeChoice progressChoice = chooseEnemyEscape(
        {0.0f, 0.0f}, CardinalDirection::South, {10.0f, 0.0f}, 0,
        openTerrain);
    expect(progressChoice.direction == CardinalDirection::East &&
               progressChoice.clearProbeCount == 3,
           "escape scoring did not prefer the path toward the target");

    const EnemyEscapeChoice firstTie = chooseEnemyEscape(
        {0.0f, 0.0f}, CardinalDirection::None, {0.0f, 0.0f}, 0,
        openTerrain);
    const EnemyEscapeChoice rotatedTie = chooseEnemyEscape(
        {0.0f, 0.0f}, CardinalDirection::None, {0.0f, 0.0f}, 1,
        openTerrain);
    const EnemyEscapeChoice negativeTie = chooseEnemyEscape(
        {0.0f, 0.0f}, CardinalDirection::None, {0.0f, 0.0f}, -1,
        openTerrain);
    expect(firstTie.direction == CardinalDirection::North &&
               rotatedTie.direction == CardinalDirection::East &&
               negativeTie.direction == CardinalDirection::West,
           "escape tie breaker did not preserve rotated or negative ordering");

    const EnemyEscapeChoice snappedChoice = chooseEnemyEscape(
        {4.18f, 7.73f}, CardinalDirection::South, {4.0f, 0.0f}, 0,
        openTerrain);
    expect(snappedChoice.direction == CardinalDirection::North &&
               nearlyEqual(snappedChoice.alignedPosition.x, 4.0f) &&
               nearlyEqual(snappedChoice.alignedPosition.z, 7.73f),
           "escape route did not expose its snapped cardinal-lane position");

    reporter.beginSuite("enemy-escape-query-transcript");
    std::vector<XZ> escapeQueries;
    const EnemyEscapeChoice transcriptChoice = chooseEnemyEscape(
        {0.0f, 0.0f}, CardinalDirection::South, {0.0f, 0.0f}, 2,
        [&](XZ candidate) {
            escapeQueries.push_back(candidate);
            return true;
        });
    std::vector<XZ> expectedEscapeQueries;
    for (CardinalDirection direction : {CardinalDirection::West,
                                        CardinalDirection::North,
                                        CardinalDirection::East})
    {
        expectedEscapeQueries.push_back({0.0f, 0.0f});
        const XZ axis = tanks3d::core::cardinalVector(direction);
        for (int probe = 1; probe <= 3; ++probe)
        {
            expectedEscapeQueries.push_back(
                axis * (kEnemyEscapeProbeStep * probe));
        }
    }
    const bool queryTranscriptMatches =
        escapeQueries.size() == expectedEscapeQueries.size() &&
        std::equal(escapeQueries.begin(), escapeQueries.end(),
                   expectedEscapeQueries.begin(), samePosition);
    expect(transcriptChoice.direction == CardinalDirection::West &&
               transcriptChoice.clearProbeCount == 3 &&
               queryTranscriptMatches,
           "escape query order, coordinates, or tieBreaker=2 result changed");

    reporter.beginSuite("enemy-steering-invalid-static-input-is-atomic");
    int invalidQueries = 0;
    int invalidUnitRolls = 0;
    int invalidDirectionRolls = 0;
    const EnemyPositionAvailable validSteeringAvailability =
        [&](XZ) {
            ++invalidQueries;
            return true;
        };
    const EnemySteeringRandom validSteeringRandom{
        [&]() {
            ++invalidUnitRolls;
            return 0.25f;
        },
        [&]() {
            ++invalidDirectionRolls;
            return 0;
        }};
    const EnemyFrameStart validSteeringFrame = activeMovementFrame(
        false, CardinalDirection::None);
    const auto rejectsSteering = [&](const EnemyFrameStart &frame, float dt,
                                     const EnemyPositionAvailable &available,
                                     const EnemySteeringRandom &random) {
        Enemy enemy = frameFixture();
        const Enemy before = enemy;
        return advanceActiveEnemySteering(
                   enemy, frame, dt, available, random) ==
                   EnemySteeringOutcome::Invalid &&
               sameEnemyState(enemy, before);
    };
    EnemyFrameStart invalidSteeringFrame = validSteeringFrame;
    invalidSteeringFrame.valid = false;
    EnemyFrameStart frozenSteeringFrame = validSteeringFrame;
    frozenSteeringFrame.phase = EnemyFramePhase::Frozen;
    EnemySteeringRandom missingUnitRoll = validSteeringRandom;
    missingUnitRoll.unitRoll = {};
    EnemySteeringRandom missingDirectionRoll = validSteeringRandom;
    missingDirectionRoll.directionRoll = {};
    expect(rejectsSteering(invalidSteeringFrame, 0.0f,
                           validSteeringAvailability,
                           validSteeringRandom) &&
               rejectsSteering(frozenSteeringFrame, 0.0f,
                               validSteeringAvailability,
                               validSteeringRandom) &&
               rejectsSteering(
                   validSteeringFrame,
                   std::numeric_limits<float>::quiet_NaN(),
                   validSteeringAvailability, validSteeringRandom) &&
               rejectsSteering(
                   validSteeringFrame,
                   std::numeric_limits<float>::infinity(),
                   validSteeringAvailability, validSteeringRandom) &&
               rejectsSteering(validSteeringFrame, -0.01f,
                               validSteeringAvailability,
                               validSteeringRandom) &&
               rejectsSteering(validSteeringFrame, 0.0f,
                               EnemyPositionAvailable{},
                               validSteeringRandom) &&
               rejectsSteering(validSteeringFrame, 0.0f,
                               validSteeringAvailability,
                               missingUnitRoll) &&
               rejectsSteering(validSteeringFrame, 0.0f,
                               validSteeringAvailability,
                               missingDirectionRoll) &&
               invalidQueries == 0 && invalidUnitRolls == 0 &&
               invalidDirectionRolls == 0,
           "invalid steering input mutated state or issued a query/draw");

    reporter.beginSuite("enemy-steering-strict-timer-and-blocked-gates");
    int gateQueries = 0;
    int gateUnitRolls = 0;
    int gateDirectionRolls = 0;
    const EnemyPositionAvailable closedSteeringAvailability =
        [&](XZ) {
            ++gateQueries;
            return false;
        };
    const EnemySteeringRandom unexpectedGateRandom{
        [&]() {
            ++gateUnitRolls;
            return 0.25f;
        },
        [&]() {
            ++gateDirectionRolls;
            return 0;
        }};

    Enemy equalTimerEnemy = frameFixture();
    equalTimerEnemy.blockedTimer = 0.0f;
    equalTimerEnemy.directionTimer = 0.05f;
    equalTimerEnemy.directionDecisionInterval = 0.10f;
    equalTimerEnemy.driveDirection = CardinalDirection::West;
    equalTimerEnemy.yaw = 1.25f;
    Enemy equalTimerExpected = equalTimerEnemy;
    equalTimerExpected.directionTimer = 0.10f;
    equalTimerExpected.yaw = cardinalYaw(CardinalDirection::West);
    const EnemySteeringOutcome equalTimerOutcome =
        advanceActiveEnemySteering(
            equalTimerEnemy, validSteeringFrame, 0.05f,
            closedSteeringAvailability, unexpectedGateRandom);

    Enemy belowBlockedEnemy = frameFixture();
    belowBlockedEnemy.id = 0;
    belowBlockedEnemy.blockedTimer = std::nextafter(
        kEnemyBlockedEscapeDelay, 0.0f);
    belowBlockedEnemy.directionTimer = 0.0f;
    belowBlockedEnemy.directionDecisionInterval = 1.0f;
    belowBlockedEnemy.driveDirection = CardinalDirection::East;
    belowBlockedEnemy.movementDirection = CardinalDirection::South;
    belowBlockedEnemy.yaw = -2.0f;
    Enemy belowBlockedExpected = belowBlockedEnemy;
    belowBlockedExpected.yaw = cardinalYaw(CardinalDirection::East);
    const EnemySteeringOutcome belowBlockedOutcome =
        advanceActiveEnemySteering(
            belowBlockedEnemy, validSteeringFrame, 0.0f,
            closedSteeringAvailability, unexpectedGateRandom);

    Enemy exactBlockedEnemy = belowBlockedEnemy;
    exactBlockedEnemy.blockedTimer = kEnemyBlockedEscapeDelay;
    exactBlockedEnemy.yaw = -2.0f;
    Enemy exactBlockedExpected = exactBlockedEnemy;
    exactBlockedExpected.directionTimer = 0.0f;
    exactBlockedExpected.directionDecisionInterval =
        kEnemyDirectionDecisionMinimumInterval;
    exactBlockedExpected.yaw = cardinalYaw(CardinalDirection::East);
    const EnemySteeringOutcome exactBlockedOutcome =
        advanceActiveEnemySteering(
            exactBlockedEnemy, validSteeringFrame, 0.0f,
            closedSteeringAvailability, unexpectedGateRandom);

    Enemy tinyBlockedEnemy = belowBlockedEnemy;
    tinyBlockedEnemy.blockedTimer = std::nextafter(0.0f, 1.0f);
    tinyBlockedEnemy.directionTimer = std::nextafter(
        tinyBlockedEnemy.directionDecisionInterval,
        std::numeric_limits<float>::infinity());
    tinyBlockedEnemy.yaw = -2.0f;
    Enemy tinyBlockedExpected = tinyBlockedEnemy;
    tinyBlockedExpected.directionTimer = 0.0f;
    tinyBlockedExpected.directionDecisionInterval =
        kEnemyDirectionDecisionMinimumInterval;
    tinyBlockedExpected.yaw = cardinalYaw(CardinalDirection::East);
    const EnemySteeringOutcome tinyBlockedOutcome =
        advanceActiveEnemySteering(
            tinyBlockedEnemy, validSteeringFrame, 0.0f,
            closedSteeringAvailability, unexpectedGateRandom);

    expect(equalTimerOutcome == EnemySteeringOutcome::NoDecision &&
               sameEnemyState(equalTimerEnemy, equalTimerExpected) &&
               belowBlockedOutcome == EnemySteeringOutcome::NoDecision &&
               sameEnemyState(belowBlockedEnemy, belowBlockedExpected) &&
               exactBlockedOutcome ==
                   EnemySteeringOutcome::EscapeUnavailable &&
               sameEnemyState(exactBlockedEnemy, exactBlockedExpected) &&
               tinyBlockedOutcome ==
                   EnemySteeringOutcome::EscapeUnavailable &&
               sameEnemyState(tinyBlockedEnemy, tinyBlockedExpected) &&
               gateQueries == 6 && gateUnitRolls == 0 &&
               gateDirectionRolls == 0,
           "steering changed strict >/>= gates or a blocked retry draw");

    reporter.beginSuite("enemy-steering-escape-commit-and-draw-order");
    Enemy escapingEnemy = frameFixture();
    escapingEnemy.id = 0;
    escapingEnemy.position = {4.18f, 7.73f};
    escapingEnemy.target = {4.0f, 0.0f};
    escapingEnemy.driveDirection = CardinalDirection::South;
    escapingEnemy.movementDirection = CardinalDirection::South;
    escapingEnemy.yaw = cardinalYaw(CardinalDirection::South);
    escapingEnemy.blockedTimer = kEnemyBlockedEscapeDelay;
    escapingEnemy.directionTimer = 0.02f;
    escapingEnemy.directionDecisionInterval = 0.73f;
    escapingEnemy.iceSlipTimer = 0.37f;
    const Enemy escapingBefore = escapingEnemy;
    Enemy escapingAtQuery = escapingBefore;
    escapingAtQuery.directionTimer = 0.0f;
    Enemy escapingAtDraw = escapingAtQuery;
    escapingAtDraw.position = {4.0f, 7.73f};
    escapingAtDraw.driveDirection = CardinalDirection::North;
    escapingAtDraw.movementDirection = CardinalDirection::North;
    escapingAtDraw.yaw = cardinalYaw(CardinalDirection::North);
    escapingAtDraw.iceSlipTimer = 0.0f;
    int escapeSteeringQueries = 0;
    int escapeUnitRolls = 0;
    int escapeDirectionRolls = 0;
    bool escapeQueryStateMatched = true;
    bool escapeDrawStateMatched = true;
    std::string escapeTranscript;
    const EnemyPositionAvailable northOnlyEscape = [&](XZ) {
        escapeTranscript.push_back('Q');
        escapeQueryStateMatched = escapeQueryStateMatched &&
                                  sameEnemyState(escapingEnemy,
                                                 escapingAtQuery);
        ++escapeSteeringQueries;
        return escapeSteeringQueries <= 4;
    };
    const EnemySteeringRandom escapeRandom{
        [&]() {
            escapeTranscript.push_back('R');
            ++escapeUnitRolls;
            escapeDrawStateMatched = escapeDrawStateMatched &&
                                     sameEnemyState(escapingEnemy,
                                                    escapingAtDraw);
            return 0.5f;
        },
        [&]() {
            escapeTranscript.push_back('I');
            ++escapeDirectionRolls;
            return 0;
        }};
    const EnemySteeringOutcome escapeOutcome =
        advanceActiveEnemySteering(
            escapingEnemy, validSteeringFrame, 0.0f,
            northOnlyEscape, escapeRandom);
    Enemy escapingExpected = escapingAtDraw;
    escapingExpected.directionDecisionInterval = kEnemyEscapeCommitTime +
        0.5f * kEnemyEscapeDecisionIntervalRange;
    escapingExpected.blockedTimer = 0.0f;

    Enemy sealedEnemy = escapingBefore;
    sealedEnemy.driveDirection = CardinalDirection::East;
    sealedEnemy.yaw = -2.0f;
    const Enemy sealedBefore = sealedEnemy;
    Enemy sealedAtQuery = sealedBefore;
    sealedAtQuery.directionTimer = 0.0f;
    int sealedSteeringQueries = 0;
    int sealedRolls = 0;
    bool sealedQueryStateMatched = true;
    std::string sealedTranscript;
    const EnemySteeringOutcome sealedOutcome =
        advanceActiveEnemySteering(
            sealedEnemy, validSteeringFrame, 0.0f,
            [&](XZ) {
                sealedTranscript.push_back('Q');
                ++sealedSteeringQueries;
                sealedQueryStateMatched = sealedQueryStateMatched &&
                                          sameEnemyState(sealedEnemy,
                                                         sealedAtQuery);
                return false;
            },
            EnemySteeringRandom{
                [&]() {
                    ++sealedRolls;
                    sealedTranscript.push_back('R');
                    return 0.0f;
                },
                [&]() {
                    ++sealedRolls;
                    sealedTranscript.push_back('I');
                    return 0;
                }});
    Enemy sealedExpected = sealedAtQuery;
    sealedExpected.directionDecisionInterval =
        kEnemyDirectionDecisionMinimumInterval;
    sealedExpected.yaw = cardinalYaw(CardinalDirection::East);

    expect(escapeOutcome == EnemySteeringOutcome::EscapeCommitted &&
               escapeSteeringQueries == 6 && escapeUnitRolls == 1 &&
               escapeDirectionRolls == 0 &&
               escapeTranscript == "QQQQQQR" &&
               escapeQueryStateMatched && escapeDrawStateMatched &&
               sameEnemyState(escapingEnemy, escapingExpected) &&
               sealedOutcome == EnemySteeringOutcome::EscapeUnavailable &&
               sealedSteeringQueries == 3 && sealedRolls == 0 &&
               sealedTranscript == "QQQ" && sealedQueryStateMatched &&
               sameEnemyState(sealedEnemy, sealedExpected),
           "escape steering lost atomic route commit or callback/draw order");

    reporter.beginSuite("enemy-steering-thresholds-transcripts-and-mapping");
    struct OrdinarySteeringCase
    {
        int type;
        float branchRoll;
        bool pursuit;
        int directionIndex;
        CardinalDirection expectedDirection;
    };
    const float belowBasicPursuit = std::nextafter(
        0.8f, -std::numeric_limits<float>::infinity());
    const float belowOtherPursuit = std::nextafter(
        0.5f, -std::numeric_limits<float>::infinity());
    const std::array<OrdinarySteeringCase, 6> ordinaryCases{{
        {kBasicEnemyType, belowBasicPursuit, true, 0,
         CardinalDirection::East},
        {kBasicEnemyType, 0.8f, false, 0, CardinalDirection::North},
        {kPowerEnemyType, belowOtherPursuit, true, 0,
         CardinalDirection::East},
        {kPowerEnemyType, 0.5f, false, 1, CardinalDirection::East},
        {kBasicEnemyType, 0.8f, false, 2, CardinalDirection::South},
        {kBasicEnemyType, 0.8f, false, 3, CardinalDirection::West}}};
    bool ordinaryCasesPreserved = true;
    for (const OrdinarySteeringCase &steeringCase : ordinaryCases)
    {
        Enemy ordinaryEnemy = frameFixture();
        ordinaryEnemy.position = {0.0f, 0.0f};
        ordinaryEnemy.target = {4.0f, -1.0f};
        ordinaryEnemy.type = steeringCase.type;
        ordinaryEnemy.blockedTimer = 0.0f;
        ordinaryEnemy.directionTimer = std::nextafter(
            0.1f, std::numeric_limits<float>::infinity());
        ordinaryEnemy.directionDecisionInterval = 0.1f;
        ordinaryEnemy.driveDirection = CardinalDirection::South;
        ordinaryEnemy.movementDirection = CardinalDirection::West;
        ordinaryEnemy.yaw = 1.25f;
        const Enemy ordinaryBefore = ordinaryEnemy;
        Enemy ordinaryAtFirstDraw = ordinaryBefore;
        ordinaryAtFirstDraw.directionTimer = 0.0f;
        Enemy ordinaryAtLaterDraw = ordinaryAtFirstDraw;
        ordinaryAtLaterDraw.directionDecisionInterval =
            kEnemyDirectionDecisionMinimumInterval +
            0.25f * kEnemyDirectionDecisionIntervalRange;
        int availabilityQueries = 0;
        int unitRolls = 0;
        int directionRolls = 0;
        bool ordinaryDrawStateMatched = true;
        std::string drawTranscript;
        const std::array<float, 3> unitValues{{
            0.25f, steeringCase.branchRoll, 0.25f}};
        const EnemySteeringOutcome ordinaryOutcome =
            advanceActiveEnemySteering(
                ordinaryEnemy, validSteeringFrame, 0.0f,
                [&](XZ) {
                    ++availabilityQueries;
                    return true;
                },
                EnemySteeringRandom{
                    [&]() {
                        drawTranscript.push_back('R');
                        ordinaryDrawStateMatched =
                            ordinaryDrawStateMatched &&
                            sameEnemyState(
                                ordinaryEnemy,
                                unitRolls == 0 ? ordinaryAtFirstDraw
                                               : ordinaryAtLaterDraw);
                        return unitValues[static_cast<std::size_t>(
                            unitRolls++)];
                    },
                    [&]() {
                        drawTranscript.push_back('I');
                        ordinaryDrawStateMatched =
                            ordinaryDrawStateMatched &&
                            sameEnemyState(ordinaryEnemy,
                                           ordinaryAtLaterDraw);
                        ++directionRolls;
                        return steeringCase.directionIndex;
                    }});
        Enemy ordinaryExpected = ordinaryBefore;
        ordinaryExpected.directionTimer = 0.0f;
        ordinaryExpected.directionDecisionInterval =
            kEnemyDirectionDecisionMinimumInterval +
            0.25f * kEnemyDirectionDecisionIntervalRange;
        ordinaryExpected.driveDirection = steeringCase.expectedDirection;
        ordinaryExpected.yaw = cardinalYaw(
            steeringCase.expectedDirection);
        ordinaryCasesPreserved = ordinaryCasesPreserved &&
            ordinaryOutcome == EnemySteeringOutcome::OrdinaryDecision &&
            availabilityQueries == 0 &&
            unitRolls == (steeringCase.pursuit ? 3 : 2) &&
            directionRolls == (steeringCase.pursuit ? 0 : 1) &&
            ordinaryDrawStateMatched &&
            drawTranscript == (steeringCase.pursuit ? "RRR" : "RRI") &&
            sameEnemyState(ordinaryEnemy, ordinaryExpected);
    }
    expect(ordinaryCasesPreserved,
           "steering thresholds, conditional transcript, mapping, or fields "
           "changed");

    reporter.beginSuite("enemy-target-type-and-active-player-policy");
    const XZ enemyPosition{10.0f, 10.0f};
    const XZ baseTarget{20.0f, 10.0f};
    std::vector<Player> typePlayers{
        playerAt({10.5f, 10.0f}, false),
        playerAt({13.0f, 10.0f}),
        playerAt({10.0f, 14.0f})};
    expect(samePosition(chooseEnemyTarget(kBasicEnemyType, enemyPosition,
                                          baseTarget, typePlayers),
                        typePlayers[1].position),
           "Basic enemy ignored the first closer active player");
    expect(samePosition(chooseEnemyTarget(kArmorEnemyType, enemyPosition,
                                          baseTarget, typePlayers),
                        typePlayers[1].position),
           "Armor enemy ignored the first closer active player");
    expect(samePosition(chooseEnemyTarget(kFastEnemyType, enemyPosition,
                                          baseTarget, typePlayers),
                        baseTarget),
           "Fast enemy stopped focusing on the base");
    expect(samePosition(chooseEnemyTarget(kPowerEnemyType, enemyPosition,
                                          baseTarget, typePlayers),
                        baseTarget),
           "Power enemy stopped focusing on the base");
    expect(samePosition(chooseEnemyTarget(-1, enemyPosition, baseTarget,
                                          typePlayers),
                        baseTarget) &&
               samePosition(chooseEnemyTarget(4, enemyPosition, baseTarget,
                                               typePlayers),
                            baseTarget),
           "invalid enemy type did not fail safely to the base");
    expect(samePosition(chooseEnemyTarget(kBasicEnemyType, enemyPosition,
                                          baseTarget, {}),
                        baseTarget),
           "empty player list did not preserve the base fallback");

    reporter.beginSuite("enemy-target-strict-distance-and-vector-order");
    std::vector<Player> equalPlayers{
        playerAt({7.0f, 10.0f}), playerAt({13.0f, 10.0f})};
    expect(samePosition(chooseEnemyTarget(kBasicEnemyType, enemyPosition,
                                          baseTarget, equalPlayers),
                        equalPlayers[0].position),
           "equal closer players did not preserve vector-first choice");
    std::reverse(equalPlayers.begin(), equalPlayers.end());
    expect(samePosition(chooseEnemyTarget(kBasicEnemyType, enemyPosition,
                                          baseTarget, equalPlayers),
                        equalPlayers[0].position),
           "reordered equal players did not preserve new vector-first choice");
    const XZ equalToBase{10.0f, 20.0f};
    expect(samePosition(chooseEnemyTarget(kBasicEnemyType, enemyPosition,
                                          baseTarget,
                                          {playerAt(equalToBase)}),
                        baseTarget),
           "player tied with base replaced the strict fallback target");
    const XZ fartherThanBase{10.0f, 20.0001f};
    expect(samePosition(chooseEnemyTarget(kArmorEnemyType, enemyPosition,
                                          baseTarget,
                                          {playerAt(fartherThanBase)}),
                        baseTarget),
           "farther player replaced the base target");
    const XZ euclideanCloserButManhattanFarther{16.0f, 16.0f};
    expect(samePosition(
               chooseEnemyTarget(
                   kBasicEnemyType, enemyPosition, baseTarget,
                   {playerAt(euclideanCloserButManhattanFarther)}),
               baseTarget),
           "target selection stopped using Manhattan distance");
    Player creatingZeroHitPointPlayer = playerAt({11.0f, 10.0f});
    creatingZeroHitPointPlayer.creationTimer = 1.0f;
    creatingZeroHitPointPlayer.hitPoints = 0;
    expect(samePosition(
               chooseEnemyTarget(kBasicEnemyType, enemyPosition, baseTarget,
                                 {creatingZeroHitPointPlayer}),
               creatingZeroHitPointPlayer.position),
           "target policy added creation or HP filtering beyond active state");

    reporter.beginSuite("enemy-pursuit-axis-and-roll-boundaries");
    const float belowPrimary = std::nextafter(
        kEnemyPursuitPrimaryProbability, 0.0f);
    expect(chooseEnemyPursuitDirection({0.0f, 0.0f}, {4.0f, -1.0f},
                                       belowPrimary) ==
               CardinalDirection::East,
           "horizontal-dominant target did not choose east below 0.7");
    expect(chooseEnemyPursuitDirection({0.0f, 0.0f}, {4.0f, -1.0f},
                                       kEnemyPursuitPrimaryProbability) ==
               CardinalDirection::North,
           "roll equal to 0.7 did not choose the secondary vertical axis");
    expect(chooseEnemyPursuitDirection({0.0f, 0.0f}, {-4.0f, 1.0f},
                                       belowPrimary) ==
               CardinalDirection::West,
           "negative horizontal delta did not choose west");
    expect(chooseEnemyPursuitDirection({0.0f, 0.0f}, {1.0f, -4.0f},
                                       belowPrimary) ==
               CardinalDirection::North,
           "vertical-dominant target did not choose north below 0.7");
    expect(chooseEnemyPursuitDirection({0.0f, 0.0f}, {-1.0f, 4.0f},
                                       belowPrimary) ==
               CardinalDirection::South,
           "positive vertical delta did not choose south");
    expect(chooseEnemyPursuitDirection({0.0f, 0.0f}, {-1.0f, 4.0f},
                                       kEnemyPursuitPrimaryProbability) ==
               CardinalDirection::West,
           "vertical-dominant secondary choice did not use horizontal sign");
    expect(chooseEnemyPursuitDirection({0.0f, 0.0f}, {3.0f, -3.0f},
                                       belowPrimary) ==
               CardinalDirection::North &&
               chooseEnemyPursuitDirection(
                   {0.0f, 0.0f}, {3.0f, -3.0f},
                   kEnemyPursuitPrimaryProbability) == CardinalDirection::East,
           "equal axis magnitudes did not keep vertical-first tie behavior");
    expect(chooseEnemyPursuitDirection({2.0f, 2.0f}, {2.0f, 2.0f},
                                       belowPrimary) ==
                   CardinalDirection::South &&
               chooseEnemyPursuitDirection(
                   {2.0f, 2.0f}, {2.0f, 2.0f},
                   kEnemyPursuitPrimaryProbability) == CardinalDirection::East,
           "zero-delta pursuit changed its south/east fallback directions");

    reporter.beginSuite("enemy-pursuit-invalid-input-fails-closed");
    const float quietNaN = std::numeric_limits<float>::quiet_NaN();
    const float infinity = std::numeric_limits<float>::infinity();
    const float belowOne = std::nextafter(1.0f, 0.0f);
    expect(chooseEnemyPursuitDirection({0.0f, 0.0f}, {1.0f, 0.0f},
                                       belowOne) == CardinalDirection::South,
           "largest valid pursuit roll did not choose the secondary axis");
    expect(chooseEnemyPursuitDirection({0.0f, 0.0f}, {1.0f, 0.0f},
                                       -0.0001f) == CardinalDirection::None &&
               chooseEnemyPursuitDirection({0.0f, 0.0f}, {1.0f, 0.0f},
                                            1.0f) ==
                   CardinalDirection::None &&
               chooseEnemyPursuitDirection({0.0f, 0.0f}, {1.0f, 0.0f},
                                            1.0001f) ==
                   CardinalDirection::None,
           "out-of-range pursuit roll did not fail closed");
    expect(chooseEnemyPursuitDirection({0.0f, 0.0f}, {1.0f, 0.0f},
                                       quietNaN) == CardinalDirection::None,
           "NaN pursuit roll did not fail closed");
    expect(chooseEnemyPursuitDirection({infinity, 0.0f}, {1.0f, 0.0f},
                                       0.0f) == CardinalDirection::None &&
               chooseEnemyPursuitDirection({0.0f, 0.0f}, {1.0f, quietNaN},
                                            0.0f) == CardinalDirection::None,
           "non-finite pursuit coordinate did not fail closed");

    reporter.beginSuite("armor-fire-cardinal-and-blocked-policy");
    const XZ origin{0.0f, 0.0f};
    expect(armorEnemyShouldFire(CardinalDirection::North, origin,
                                {1.0f, -3.0f}, false),
           "North-facing Armor enemy rejected target in its forward lane");
    expect(armorEnemyShouldFire(CardinalDirection::East, origin,
                                {3.0f, -1.0f}, false),
           "East-facing Armor enemy rejected target in its forward lane");
    expect(armorEnemyShouldFire(CardinalDirection::South, origin,
                                {-1.0f, 3.0f}, false),
           "South-facing Armor enemy rejected target in its forward lane");
    expect(armorEnemyShouldFire(CardinalDirection::West, origin,
                                {-3.0f, 1.0f}, false),
           "West-facing Armor enemy rejected target in its forward lane");
    expect(!armorEnemyShouldFire(CardinalDirection::North, origin,
                                 {0.0f, 3.0f}, false) &&
               !armorEnemyShouldFire(CardinalDirection::East, origin,
                                     {-3.0f, 0.0f}, false) &&
               !armorEnemyShouldFire(CardinalDirection::South, origin,
                                     {0.0f, -3.0f}, false) &&
               !armorEnemyShouldFire(CardinalDirection::West, origin,
                                     {3.0f, 0.0f}, false),
           "Armor enemy fired behind its movement direction");
    expect(!armorEnemyShouldFire(CardinalDirection::North, origin,
                                 {0.0f, 0.0f}, false) &&
               !armorEnemyShouldFire(CardinalDirection::East, origin,
                                     {0.0f, 0.0f}, false),
           "Armor enemy fired without a strictly forward displacement");
    for (CardinalDirection direction : {
             CardinalDirection::None, CardinalDirection::North,
             CardinalDirection::South, CardinalDirection::West,
             CardinalDirection::East})
    {
        expect(armorEnemyShouldFire(direction, origin, {100.0f, 100.0f},
                                    true),
               "blocked Armor enemy did not fire for a legal direction");
    }
    expect(!armorEnemyShouldFire(CardinalDirection::None, origin,
                                 {0.0f, -3.0f}, false),
           "unblocked None direction fired");

    reporter.beginSuite("armor-fire-strict-lane-and-invalid-input");
    const float insideLane = std::nextafter(
        kArmorEnemyFiringLaneHalfWidth, 0.0f);
    expect(armorEnemyShouldFire(CardinalDirection::North, origin,
                                {insideLane, -3.0f}, false) &&
               armorEnemyShouldFire(CardinalDirection::East, origin,
                                    {3.0f, -insideLane}, false) &&
               armorEnemyShouldFire(CardinalDirection::South, origin,
                                    {-insideLane, 3.0f}, false) &&
               armorEnemyShouldFire(CardinalDirection::West, origin,
                                    {-3.0f, -insideLane}, false),
           "Armor enemy rejected a target just inside the firing lane");
    expect(!armorEnemyShouldFire(CardinalDirection::North, origin,
                                 {kArmorEnemyFiringLaneHalfWidth, -3.0f},
                                 false) &&
               !armorEnemyShouldFire(CardinalDirection::East, origin,
                                     {3.0f,
                                      kArmorEnemyFiringLaneHalfWidth},
                                     false) &&
               !armorEnemyShouldFire(CardinalDirection::South, origin,
                                     {-kArmorEnemyFiringLaneHalfWidth, 3.0f},
                                     false) &&
               !armorEnemyShouldFire(CardinalDirection::West, origin,
                                     {-3.0f,
                                      -kArmorEnemyFiringLaneHalfWidth},
                                     false),
           "Armor enemy accepted the strict two-tile lane boundary");
    const CardinalDirection invalidDirection =
        static_cast<CardinalDirection>(255);
    expect(!armorEnemyShouldFire(invalidDirection, origin, {0.0f, -3.0f},
                                 false) &&
               !armorEnemyShouldFire(invalidDirection, origin,
                                     {0.0f, -3.0f}, true),
           "invalid direction did not fail closed even while blocked");
    expect(!armorEnemyShouldFire(CardinalDirection::North, origin,
                                 {quietNaN, -3.0f}, false),
           "non-finite Armor target fired while unblocked");

    reporter.beginSuite("enemy-fire-plan-type-scaling-and-atomicity");
    Enemy fireEnemy = frameFixture();
    fireEnemy.position = origin;
    fireEnemy.target = {0.0f, -3.0f};
    fireEnemy.movementDirection = CardinalDirection::North;
    const Enemy fireEnemyBefore = fireEnemy;

    fireEnemy.type = kBasicEnemyType;
    const EnemyFirePlan basicPlan = planEnemyFire(fireEnemy, false, 0.75f);
    fireEnemy.type = kFastEnemyType;
    const EnemyFirePlan fastPlan = planEnemyFire(fireEnemy, false, 0.75f);
    fireEnemy.type = kPowerEnemyType;
    const EnemyFirePlan powerPlan = planEnemyFire(fireEnemy, false, 0.75f);
    fireEnemy.type = 99;
    const EnemyFirePlan invalidPlan = planEnemyFire(fireEnemy, false, 0.75f);
    float expectedPowerReload = 0.75f;
    expectedPowerReload *= 0.8f;
    expect(basicPlan.shouldFire && fastPlan.shouldFire &&
               powerPlan.shouldFire && invalidPlan.shouldFire &&
               basicPlan.baseReloadInterval == 0.75f &&
               fastPlan.baseReloadInterval == 0.75f &&
               powerPlan.baseReloadInterval == expectedPowerReload &&
               invalidPlan.baseReloadInterval == 0.75f,
           "enemy fire plan changed the historical type reload scaling");

    fireEnemy = fireEnemyBefore;
    fireEnemy.type = kArmorEnemyType;
    const Enemy armorBefore = fireEnemy;
    const EnemyFirePlan armorPlan = planEnemyFire(fireEnemy, false, 0.75f);
    float expectedArmorReload = 0.75f;
    expectedArmorReload *= 0.4f;
    expect(armorPlan.shouldFire &&
               armorPlan.baseReloadInterval == expectedArmorReload &&
               sameEnemyState(fireEnemy, armorBefore),
           "Armor fire plan changed its source enemy or reload scaling");

    reporter.beginSuite("enemy-fire-plan-armor-gating-and-roll-boundaries");
    fireEnemy.target = {3.0f, 0.0f};
    const EnemyFirePlan rejectedArmor = planEnemyFire(fireEnemy, false, 0.0f);
    const EnemyFirePlan blockedArmor = planEnemyFire(fireEnemy, true, 0.0f);
    const float largestRoll = std::nextafter(1.0f, 0.0f);
    const EnemyFirePlan largestArmor = planEnemyFire(
        fireEnemy, true, largestRoll);
    float expectedLargestArmorReload = largestRoll;
    expectedLargestArmorReload *= 0.4f;
    expect(!rejectedArmor.shouldFire && blockedArmor.shouldFire &&
               rejectedArmor.baseReloadInterval == 0.0f &&
               blockedArmor.baseReloadInterval == 0.0f &&
               largestArmor.baseReloadInterval == expectedLargestArmorReload,
           "Armor fire plan lost blocked override or roll boundaries");

    reporter.beginSuite("enemy-fire-transaction-invalid-static-input-is-atomic");
    Enemy dueFireEnemy = frameFixture();
    dueFireEnemy.fireCooldown = 0.0f;
    const Enemy dueFireBefore = dueFireEnemy;
    int invalidFireCallbackCount = 0;
    const EnemyFireRandom validFireRandom{
        [&]() {
            ++invalidFireCallbackCount;
            return 0.5f;
        }};
    const EnemyOwnedShellQuery validOwnedShellQuery =
        [&](int) {
            ++invalidFireCallbackCount;
            return false;
        };
    const EnemyShellLaunch validShellLaunch =
        [&](const EnemyShellLaunchIntent &) {
            ++invalidFireCallbackCount;
        };
    const auto rejectsFireAtomically = [=](
        const EnemyFireConfiguration &configuration,
        const EnemyFireRandom &random,
        const EnemyOwnedShellQuery &query,
        const EnemyShellLaunch &launch) {
        Enemy candidate = dueFireBefore;
        return advanceEnemyFireTransaction(candidate, false, configuration,
                                           random, query, launch) ==
                   EnemyFireOutcome::Invalid &&
               sameEnemyState(candidate, dueFireBefore);
    };
    EnemyFireConfiguration nonfiniteFireConfiguration =
        fireConfigurationFixture();
    nonfiniteFireConfiguration.shellSpawnDistance =
        std::numeric_limits<float>::quiet_NaN();
    EnemyFireConfiguration negativeFireConfiguration =
        fireConfigurationFixture();
    negativeFireConfiguration.shellSpawnDistance = -0.001f;
    expect(rejectsFireAtomically(nonfiniteFireConfiguration,
                                 validFireRandom, validOwnedShellQuery,
                                 validShellLaunch) &&
               rejectsFireAtomically(negativeFireConfiguration,
                                      validFireRandom,
                                      validOwnedShellQuery,
                                      validShellLaunch) &&
               rejectsFireAtomically(fireConfigurationFixture(), {},
                                      validOwnedShellQuery,
                                      validShellLaunch) &&
               rejectsFireAtomically(fireConfigurationFixture(),
                                      validFireRandom, {},
                                      validShellLaunch) &&
               rejectsFireAtomically(fireConfigurationFixture(),
                                      validFireRandom,
                                      validOwnedShellQuery, {}) &&
               invalidFireCallbackCount == 0,
           "invalid fire configuration or callback was not atomic");

    reporter.beginSuite("enemy-fire-transaction-cooling-and-roll-validation");
    int coolingFireCallbackCount = 0;
    Enemy coolingFireEnemy = dueFireBefore;
    coolingFireEnemy.fireCooldown = 0.125f;
    const Enemy coolingFireBefore = coolingFireEnemy;
    const EnemyFireOutcome coolingFireOutcome = advanceEnemyFireTransaction(
        coolingFireEnemy, false, fireConfigurationFixture(),
        EnemyFireRandom{[&]() {
            ++coolingFireCallbackCount;
            return 0.5f;
        }},
        [&](int) {
            ++coolingFireCallbackCount;
            return false;
        },
        [&](const EnemyShellLaunchIntent &) {
            ++coolingFireCallbackCount;
        });
    Enemy nanCooldownEnemy = dueFireBefore;
    nanCooldownEnemy.fireCooldown =
        std::numeric_limits<float>::quiet_NaN();
    const Enemy nanCooldownBefore = nanCooldownEnemy;
    const EnemyFireOutcome nanCooldownOutcome =
        advanceEnemyFireTransaction(
            nanCooldownEnemy, false, fireConfigurationFixture(),
            EnemyFireRandom{[&]() {
                ++coolingFireCallbackCount;
                return 0.5f;
            }},
            [&](int) {
                ++coolingFireCallbackCount;
                return false;
            },
            [&](const EnemyShellLaunchIntent &) {
                ++coolingFireCallbackCount;
            });
    Enemy nanCooldownComparable = nanCooldownEnemy;
    Enemy nanCooldownBeforeComparable = nanCooldownBefore;
    const bool nanCooldownPreserved =
        std::isnan(nanCooldownEnemy.fireCooldown) &&
        std::isnan(nanCooldownBefore.fireCooldown);
    nanCooldownComparable.fireCooldown = 0.0f;
    nanCooldownBeforeComparable.fireCooldown = 0.0f;
    expect(coolingFireOutcome == EnemyFireOutcome::CoolingDown &&
               nanCooldownOutcome == EnemyFireOutcome::CoolingDown &&
               coolingFireCallbackCount == 0 &&
               sameEnemyState(coolingFireEnemy, coolingFireBefore) &&
               nanCooldownPreserved &&
               sameEnemyState(nanCooldownComparable,
                              nanCooldownBeforeComparable),
           "positive or NaN cooldown invoked a fire callback or changed state");

    int invalidRollCount = 0;
    int invalidRollQueryCount = 0;
    int invalidRollLaunchCount = 0;
    const auto rejectsInvalidFireRoll = [&](float roll) {
        Enemy candidate = dueFireBefore;
        const Enemy candidateBefore = candidate;
        const EnemyFireOutcome outcome = advanceEnemyFireTransaction(
            candidate, false, fireConfigurationFixture(),
            EnemyFireRandom{[&]() {
                ++invalidRollCount;
                return roll;
            }},
            [&](int) {
                ++invalidRollQueryCount;
                return false;
            },
            [&](const EnemyShellLaunchIntent &) {
                ++invalidRollLaunchCount;
            });
        return outcome == EnemyFireOutcome::Invalid &&
               sameEnemyState(candidate, candidateBefore);
    };
    expect(rejectsInvalidFireRoll(
               std::numeric_limits<float>::quiet_NaN()) &&
               rejectsInvalidFireRoll(-0.001f) &&
               rejectsInvalidFireRoll(1.0f) && invalidRollCount == 3 &&
               invalidRollQueryCount == 0 && invalidRollLaunchCount == 0,
           "invalid fire roll did not stop after exactly one draw");

    reporter.beginSuite("enemy-fire-transaction-aim-rejection-order-and-reload");
    Enemy rejectedFireEnemy = dueFireBefore;
    rejectedFireEnemy.type = kArmorEnemyType;
    rejectedFireEnemy.position = {0.0f, 0.0f};
    rejectedFireEnemy.target = {3.0f, 0.0f};
    rejectedFireEnemy.movementDirection = CardinalDirection::North;
    rejectedFireEnemy.driveDirection = CardinalDirection::East;
    std::vector<char> rejectedFireTranscript;
    const EnemyFireOutcome rejectedFireOutcome =
        advanceEnemyFireTransaction(
            rejectedFireEnemy, false, fireConfigurationFixture(-30),
            EnemyFireRandom{[&]() {
                rejectedFireTranscript.push_back('R');
                return 0.75f;
            }},
            [&](int) {
                rejectedFireTranscript.push_back('Q');
                return false;
            },
            [&](const EnemyShellLaunchIntent &) {
                rejectedFireTranscript.push_back('C');
            });
    const float expectedRejectedReload = (0.75f * 0.4f) / 0.70f;
    expect(rejectedFireOutcome == EnemyFireOutcome::AimRejected &&
               rejectedFireTranscript == std::vector<char>{'R'} &&
               nearlyEqual(rejectedFireEnemy.fireCooldown,
                           expectedRejectedReload),
           "Armor aim rejection queried a shell, launched, or lost tuning");

    reporter.beginSuite("enemy-fire-transaction-owned-shell-order-and-reload");
    Enemy occupiedFireEnemy = dueFireBefore;
    occupiedFireEnemy.type = kPowerEnemyType;
    occupiedFireEnemy.fireCooldown = -0.25f;
    const float occupiedCooldownBefore = occupiedFireEnemy.fireCooldown;
    std::vector<char> occupiedFireTranscript;
    bool occupiedQuerySawDueCooldown = false;
    bool occupiedLaunchCalled = false;
    const EnemyFireOutcome occupiedFireOutcome =
        advanceEnemyFireTransaction(
            occupiedFireEnemy, false, fireConfigurationFixture(30),
            EnemyFireRandom{[&]() {
                occupiedFireTranscript.push_back('R');
                return 0.625f;
            }},
            [&](int enemyId) {
                occupiedFireTranscript.push_back('Q');
                occupiedQuerySawDueCooldown =
                    enemyId == occupiedFireEnemy.id &&
                    occupiedFireEnemy.fireCooldown == occupiedCooldownBefore;
                return true;
            },
            [&](const EnemyShellLaunchIntent &) {
                occupiedFireTranscript.push_back('C');
                occupiedLaunchCalled = true;
            });
    const float expectedOccupiedReload = (0.625f * 0.8f) / 1.30f;
    expect(occupiedFireOutcome == EnemyFireOutcome::OwnedShellPresent &&
               occupiedFireTranscript == std::vector<char>({'R', 'Q'}) &&
               occupiedQuerySawDueCooldown && !occupiedLaunchCalled &&
               nearlyEqual(occupiedFireEnemy.fireCooldown,
                           expectedOccupiedReload),
           "owned shell did not suppress C after R-Q or changed reload order");

    reporter.beginSuite("enemy-fire-transaction-power-launch-order-and-payload");
    Enemy powerFireEnemy = dueFireBefore;
    powerFireEnemy.id = 91;
    powerFireEnemy.type = kPowerEnemyType;
    powerFireEnemy.position = {4.0f, 5.0f};
    powerFireEnemy.driveDirection = CardinalDirection::West;
    powerFireEnemy.movementDirection = CardinalDirection::South;
    powerFireEnemy.fireCooldown = 0.0f;
    std::vector<char> powerFireTranscript;
    bool powerQuerySawDueCooldown = false;
    bool powerLaunchSawDueCooldown = false;
    EnemyShellLaunchIntent powerIntent;
    const EnemyFireOutcome powerFireOutcome = advanceEnemyFireTransaction(
        powerFireEnemy, false, fireConfigurationFixture(),
        EnemyFireRandom{[&]() {
            powerFireTranscript.push_back('R');
            return 0.5f;
        }},
        [&](int enemyId) {
            powerFireTranscript.push_back('Q');
            powerQuerySawDueCooldown = enemyId == powerFireEnemy.id &&
                                        powerFireEnemy.fireCooldown == 0.0f;
            return false;
        },
        [&](const EnemyShellLaunchIntent &intent) {
            powerFireTranscript.push_back('C');
            powerLaunchSawDueCooldown =
                powerFireEnemy.fireCooldown == 0.0f;
            powerIntent = intent;
        });
    expect(powerFireOutcome == EnemyFireOutcome::Fired &&
               powerFireTranscript ==
                   std::vector<char>({'R', 'Q', 'C'}) &&
               powerQuerySawDueCooldown && powerLaunchSawDueCooldown &&
               nearlyEqual(powerFireEnemy.fireCooldown, 0.4f),
           "power fire transaction changed R-Q-C or committed reload early");
    expect(powerIntent.enemyType == kPowerEnemyType &&
               samePosition(powerIntent.tankPosition,
                            powerFireEnemy.position) &&
               powerIntent.direction == CardinalDirection::West &&
               samePosition(powerIntent.shell.position, {3.375f, 5.0f}) &&
               samePosition(powerIntent.shell.velocity,
                            {-tanks3d::core::kFastShellSpeed, 0.0f}) &&
               powerIntent.shell.owner == ShellOwner::Enemy &&
               powerIntent.shell.ownerIndex == powerFireEnemy.id &&
               !powerIntent.shell.power && !powerIntent.shell.impacting &&
               nearlyEqual(powerIntent.shell.life, 4.0f),
           "power launch intent lost shell geometry, speed, owner, or defaults");

    reporter.beginSuite("enemy-fire-transaction-armor-dual-direction-payload");
    Enemy dualDirectionEnemy = dueFireBefore;
    dualDirectionEnemy.id = 92;
    dualDirectionEnemy.type = kArmorEnemyType;
    dualDirectionEnemy.position = {4.0f, 5.0f};
    dualDirectionEnemy.target = {4.0f, 2.0f};
    dualDirectionEnemy.movementDirection = CardinalDirection::North;
    dualDirectionEnemy.driveDirection = CardinalDirection::East;
    dualDirectionEnemy.fireCooldown = 0.0f;
    std::vector<char> dualDirectionTranscript;
    EnemyShellLaunchIntent dualDirectionIntent;
    const EnemyFireOutcome dualDirectionOutcome =
        advanceEnemyFireTransaction(
            dualDirectionEnemy, false, fireConfigurationFixture(),
            EnemyFireRandom{[&]() {
                dualDirectionTranscript.push_back('R');
                return 0.75f;
            }},
            [&](int) {
                dualDirectionTranscript.push_back('Q');
                return false;
            },
            [&](const EnemyShellLaunchIntent &intent) {
                dualDirectionTranscript.push_back('C');
                dualDirectionIntent = intent;
            });
    expect(dualDirectionOutcome == EnemyFireOutcome::Fired &&
               dualDirectionTranscript ==
                   std::vector<char>({'R', 'Q', 'C'}) &&
               nearlyEqual(dualDirectionEnemy.fireCooldown, 0.30f) &&
               dualDirectionIntent.direction == CardinalDirection::East &&
               samePosition(dualDirectionIntent.shell.position,
                            {4.625f, 5.0f}) &&
               samePosition(dualDirectionIntent.shell.velocity,
                            {tanks3d::core::kBaseShellSpeed, 0.0f}),
           "Armor policy stopped using movement direction or launch stopped "
           "using drive direction");

    reporter.beginSuite("enemy-spawn-probability-strict-boundaries");
    const float stage17ArmorChance = enemyArmorTankChanceForStage(17);
    const float belowStage17ArmorChance = std::nextafter(
        stage17ArmorChance, -std::numeric_limits<float>::infinity());
    expect(nearlyEqual(enemyArmorTankChanceForStage(1), 0.10f) &&
               nearlyEqual(stage17ArmorChance, 0.2176f) &&
               nearlyEqual(enemyArmorTankChanceForStage(35), 0.3499f),
           "stage-based Armor probability formula changed");
    expect(enemyRollCreatesArmorTank(17, belowStage17ArmorChance) &&
               !enemyRollCreatesArmorTank(17, stage17ArmorChance),
           "Armor selection stopped using a strict probability boundary");
    const float belowCarrierChance = std::nextafter(
        kBonusCarrierChance, -std::numeric_limits<float>::infinity());
    expect(enemyRollCreatesBonusCarrier(belowCarrierChance) &&
               !enemyRollCreatesBonusCarrier(kBonusCarrierChance),
           "bonus-carrier selection stopped using a strict boundary");

    reporter.beginSuite("enemy-spawn-armor-threshold-branches");
    const EnemyArmorThresholds stage17Thresholds =
        enemyArmorThresholdsForStage(17);
    expect(std::fabs(stage17Thresholds.oneHit - 0.25) < 0.0000001 &&
               std::fabs(stage17Thresholds.twoHits - 0.50) < 0.0000001 &&
               std::fabs(stage17Thresholds.threeHits - 0.75) < 0.0000001,
           "stage-17 armor thresholds changed");
    expect(enemyArmorForRoll(
               17, largestFloatBelow(stage17Thresholds.oneHit)) == 1 &&
               enemyArmorForRoll(
                   17, smallestFloatAtOrAbove(stage17Thresholds.oneHit)) == 2 &&
               enemyArmorForRoll(
                   17, largestFloatBelow(stage17Thresholds.twoHits)) == 2 &&
               enemyArmorForRoll(
                   17, smallestFloatAtOrAbove(stage17Thresholds.twoHits)) == 3 &&
               enemyArmorForRoll(
                   17, largestFloatBelow(stage17Thresholds.threeHits)) == 3 &&
               enemyArmorForRoll(
                   17, smallestFloatAtOrAbove(stage17Thresholds.threeHits)) == 4,
           "stage-17 strict armor boundaries changed");
    const EnemyArmorThresholds stage18Thresholds =
        enemyArmorThresholdsForStage(18);
    expect(stage18Thresholds.oneHit > 0.0 &&
               stage18Thresholds.oneHit < stage18Thresholds.twoHits &&
               stage18Thresholds.twoHits < stage18Thresholds.threeHits &&
               stage18Thresholds.threeHits < 1.0,
           "post-stage-17 armor-threshold branch is not ordered");
    expect(enemyArmorForRoll(
               18, largestFloatBelow(stage18Thresholds.oneHit)) == 1 &&
               enemyArmorForRoll(
                   18, smallestFloatAtOrAbove(stage18Thresholds.oneHit)) == 2 &&
               enemyArmorForRoll(
                   18, largestFloatBelow(stage18Thresholds.twoHits)) == 2 &&
               enemyArmorForRoll(
                   18, smallestFloatAtOrAbove(stage18Thresholds.twoHits)) == 3 &&
               enemyArmorForRoll(
                   18, largestFloatBelow(stage18Thresholds.threeHits)) == 3 &&
               enemyArmorForRoll(
                   18, smallestFloatAtOrAbove(stage18Thresholds.threeHits)) == 4,
           "post-stage-17 strict armor boundaries changed");

    reporter.beginSuite("enemy-spawn-complete-initial-state");
    EnemySpawnParameters regularSpawn;
    regularSpawn.id = 41;
    regularSpawn.stage = 17;
    regularSpawn.position = {3.0f, 1.0f};
    regularSpawn.target = {13.0f, 23.6f};
    regularSpawn.typeRoll = stage17ArmorChance;
    regularSpawn.regularType = kPowerEnemyType;
    regularSpawn.carrierRoll = kBonusCarrierChance;
    regularSpawn.armorRoll = smallestFloatAtOrAbove(
        stage17Thresholds.threeHits);
    regularSpawn.initialFireCooldown = 0.125f;
    const Enemy regularSpawned = makeSpawnedEnemy(regularSpawn);
    expect(regularSpawned.id == regularSpawn.id &&
               samePosition(regularSpawned.position, regularSpawn.position) &&
               samePosition(regularSpawned.target, regularSpawn.target) &&
               regularSpawned.type == kPowerEnemyType &&
               regularSpawned.armor == 4 && !regularSpawned.carriesBonus,
           "regular enemy spawn did not preserve supplied identity or rolls");
    expect(nearlyEqual(regularSpawned.yaw,
                       cardinalYaw(CardinalDirection::South)) &&
               regularSpawned.driveDirection == CardinalDirection::South &&
               regularSpawned.movementDirection == CardinalDirection::South &&
               !regularSpawned.destroyed && !regularSpawned.moving &&
               nearlyEqual(regularSpawned.frozenTimer, 0.0f) &&
               nearlyEqual(regularSpawned.fireCooldown, 0.125f) &&
               nearlyEqual(regularSpawned.creationTimer,
                           kEnemyCreationDuration) &&
               nearlyEqual(regularSpawned.deathTimer, 0.0f) &&
               nearlyEqual(regularSpawned.blockedTimer, 0.0f) &&
               nearlyEqual(regularSpawned.dustCooldown, 0.0f) &&
               nearlyEqual(regularSpawned.directionTimer, 0.0f) &&
               nearlyEqual(regularSpawned.directionDecisionInterval, 0.1f) &&
               nearlyEqual(regularSpawned.movementDelay, 0.1f) &&
               nearlyEqual(regularSpawned.iceSlipTimer, 0.0f) &&
               !regularSpawned.onIce,
           "regular enemy spawn left lifecycle fields outside their defaults");

    EnemySpawnParameters armorSpawn = regularSpawn;
    armorSpawn.id = 42;
    armorSpawn.typeRoll = belowStage17ArmorChance;
    armorSpawn.regularType = 99;
    armorSpawn.carrierRoll = belowCarrierChance;
    armorSpawn.armorRoll = largestFloatBelow(stage17Thresholds.oneHit);
    armorSpawn.initialFireCooldown = kEnemyInitialFireDelay;
    const Enemy armorSpawned = makeSpawnedEnemy(armorSpawn);
    expect(armorSpawned.id == 42 && armorSpawned.type == kArmorEnemyType &&
               armorSpawned.armor == 1 && armorSpawned.carriesBonus &&
               nearlyEqual(armorSpawned.fireCooldown,
                           kEnemyInitialFireDelay),
           "Armor spawn used the ignored regular type or changed initial state");

    reporter.beginSuite("enemy-spawn-rotation-and-fail-safe");
    expect(chooseEnemySpawnIndex({{true, true, true}}, 0) == 0 &&
               chooseEnemySpawnIndex({{true, true, true}}, 1) == 1 &&
               chooseEnemySpawnIndex({{true, true, true}}, 2) == 2,
           "available rotation origin was not selected first");
    expect(chooseEnemySpawnIndex({{false, true, true}}, 0) == 1,
           "spawn scan from zero did not select slot one");
    expect(chooseEnemySpawnIndex({{true, false, true}}, 1) == 2,
           "spawn scan from one did not select slot two");
    expect(chooseEnemySpawnIndex({{true, true, false}}, 2) == 0,
           "spawn scan from two did not wrap to slot zero");
    expect(chooseEnemySpawnIndex({{false, false, false}}, 0) == -1 &&
               chooseEnemySpawnIndex({{false, false, false}}, 1) == -1 &&
               chooseEnemySpawnIndex({{false, false, false}}, 2) == -1,
           "fully blocked spawn set did not return -1");
    expect(chooseEnemySpawnIndex({{true, true, true}}, -1) == -1 &&
               chooseEnemySpawnIndex({{true, true, true}}, 3) == -1 &&
               chooseEnemySpawnIndex({{true, true, true}},
                                     std::numeric_limits<int>::max()) == -1,
           "invalid spawn rotation index did not fail closed");

    reporter.beginSuite("enemy-spawn-transaction-invalid-input-is-atomic");
    const EnemySpawnState validSpawnState{3, 1, 40, 0.25f};
    const EnemySpawnConfiguration validSpawnConfiguration =
        spawnConfigurationFixture();
    const auto rejectsSpawnAtomically = [=](
                                              EnemySpawnState state, float dt,
                                              EnemySpawnConfiguration config,
                                              bool hasAvailability,
                                              bool hasUnitRoll,
                                              bool hasRegularTypeRoll,
                                              bool hasCommit) {
        const EnemySpawnState before = state;
        int callbackCount = 0;
        EnemyPositionAvailable availability;
        if (hasAvailability)
        {
            availability = [&](XZ) {
                ++callbackCount;
                return true;
            };
        }
        EnemySpawnRandom random;
        if (hasUnitRoll)
        {
            random.unitRoll = [&]() {
                ++callbackCount;
                return 0.5f;
            };
        }
        if (hasRegularTypeRoll)
        {
            random.regularTypeRoll = [&]() {
                ++callbackCount;
                return kBasicEnemyType;
            };
        }
        EnemySpawnCommit commit;
        if (hasCommit)
        {
            commit = [&](const Enemy &) { ++callbackCount; };
        }
        return advanceEnemySpawnTransaction(
                   state, dt, 0U, config, availability, random, commit) ==
                   EnemySpawnOutcome::Invalid &&
               sameEnemySpawnState(state, before) && callbackCount == 0;
    };

    EnemySpawnState lowSlot = validSpawnState;
    lowSlot.nextSpawnIndex = -1;
    EnemySpawnState highSlot = validSpawnState;
    highSlot.nextSpawnIndex = 3;
    EnemySpawnState negativeId = validSpawnState;
    negativeId.nextEnemyId = -1;
    EnemySpawnState nanTimer = validSpawnState;
    nanTimer.timer = quietNaN;
    EnemySpawnConfiguration invalidStage = validSpawnConfiguration;
    invalidStage.stage = 0;
    EnemySpawnConfiguration invalidTarget = validSpawnConfiguration;
    invalidTarget.target.x = quietNaN;
    EnemySpawnConfiguration invalidPoint = validSpawnConfiguration;
    invalidPoint.spawnPoints[1].z = quietNaN;
    EnemySpawnConfiguration invalidFireDelay = validSpawnConfiguration;
    invalidFireDelay.initialFireCooldown = -0.01f;
    EnemySpawnConfiguration invalidNormalInterval = validSpawnConfiguration;
    invalidNormalInterval.normalInterval = quietNaN;
    EnemySpawnConfiguration invalidRetryInterval = validSpawnConfiguration;
    invalidRetryInterval.retryInterval = -0.01f;
    expect(
        rejectsSpawnAtomically(validSpawnState, -0.01f,
                               validSpawnConfiguration, true, true, true,
                               true) &&
            rejectsSpawnAtomically(validSpawnState, quietNaN,
                                   validSpawnConfiguration, true, true, true,
                                   true) &&
            rejectsSpawnAtomically(lowSlot, 0.0f, validSpawnConfiguration,
                                   true, true, true, true) &&
            rejectsSpawnAtomically(highSlot, 0.0f, validSpawnConfiguration,
                                   true, true, true, true) &&
            rejectsSpawnAtomically(negativeId, 0.0f, validSpawnConfiguration,
                                   true, true, true, true) &&
            rejectsSpawnAtomically(nanTimer, 0.0f, validSpawnConfiguration,
                                   true, true, true, true) &&
            rejectsSpawnAtomically(validSpawnState, 0.0f, invalidStage, true,
                                   true, true, true) &&
            rejectsSpawnAtomically(validSpawnState, 0.0f, invalidTarget,
                                   true, true, true, true) &&
            rejectsSpawnAtomically(validSpawnState, 0.0f, invalidPoint, true,
                                   true, true, true) &&
            rejectsSpawnAtomically(validSpawnState, 0.0f, invalidFireDelay,
                                   true, true, true, true) &&
            rejectsSpawnAtomically(validSpawnState, 0.0f,
                                   invalidNormalInterval, true, true, true,
                                   true) &&
            rejectsSpawnAtomically(validSpawnState, 0.0f,
                                   invalidRetryInterval, true, true, true,
                                   true) &&
            rejectsSpawnAtomically(validSpawnState, 0.0f,
                                   validSpawnConfiguration, false, true, true,
                                   true) &&
            rejectsSpawnAtomically(validSpawnState, 0.0f,
                                   validSpawnConfiguration, true, false, true,
                                   true) &&
            rejectsSpawnAtomically(validSpawnState, 0.0f,
                                   validSpawnConfiguration, true, true, false,
                                   true) &&
            rejectsSpawnAtomically(validSpawnState, 0.0f,
                                   validSpawnConfiguration, true, true, true,
                                   false),
        "invalid spawn transaction input mutated state or invoked a callback");

    reporter.beginSuite("enemy-spawn-transaction-guards-and-timer-boundaries");
    std::vector<char> guardedTranscript;
    const EnemyPositionAvailable guardedAvailability = [&](XZ) {
        guardedTranscript.push_back('Q');
        return true;
    };
    const EnemySpawnRandom guardedRandom{
        [&]() {
            guardedTranscript.push_back('R');
            return 0.5f;
        },
        [&]() {
            guardedTranscript.push_back('I');
            return kBasicEnemyType;
        }};
    const EnemySpawnCommit guardedCommit = [&](const Enemy &) {
        guardedTranscript.push_back('C');
    };
    EnemySpawnState exhaustedQueue{0, 2, 9, 0.40f};
    const EnemySpawnState exhaustedQueueBefore = exhaustedQueue;
    const EnemySpawnOutcome exhaustedOutcome = advanceEnemySpawnTransaction(
        exhaustedQueue, 0.15f, 0U, validSpawnConfiguration,
        guardedAvailability, guardedRandom, guardedCommit);
    EnemySpawnState capacityFull{2, 2, 9, 0.40f};
    const EnemySpawnOutcome capacityOutcome = advanceEnemySpawnTransaction(
        capacityFull, 0.15f, kMaximumActiveEnemies,
        validSpawnConfiguration, guardedAvailability, guardedRandom,
        guardedCommit);
    EnemySpawnState coolingDown{2, 2, 9, 0.20f};
    const EnemySpawnOutcome coolingOutcome = advanceEnemySpawnTransaction(
        coolingDown, 0.05f, 0U, validSpawnConfiguration,
        guardedAvailability, guardedRandom, guardedCommit);
    EnemySpawnState finalIdentifier{
        2, 0, std::numeric_limits<int>::max() - 1, 0.0f};
    std::vector<char> finalIdentifierTranscript;
    int committedFinalIdentifier = -1;
    const EnemySpawnOutcome finalIdentifierOutcome =
        advanceEnemySpawnTransaction(
            finalIdentifier, 0.0f, 0U, validSpawnConfiguration,
            [&](XZ) {
                finalIdentifierTranscript.push_back('Q');
                return true;
            },
            EnemySpawnRandom{
                [&]() {
                    finalIdentifierTranscript.push_back('R');
                    return 0.0f;
                },
                [&]() {
                    finalIdentifierTranscript.push_back('I');
                    return kBasicEnemyType;
                }},
            [&](const Enemy &enemy) {
                finalIdentifierTranscript.push_back('C');
                committedFinalIdentifier = enemy.id;
            });
    const EnemySpawnState exhaustedIdentifier = finalIdentifier;
    const EnemySpawnOutcome identifierExhaustedOutcome =
        advanceEnemySpawnTransaction(
            finalIdentifier, 0.1f, 0U, validSpawnConfiguration,
            [&](XZ) {
                finalIdentifierTranscript.push_back('Q');
                return true;
            },
            EnemySpawnRandom{
                [&]() {
                    finalIdentifierTranscript.push_back('R');
                    return 0.0f;
                },
                [&]() {
                    finalIdentifierTranscript.push_back('I');
                    return kBasicEnemyType;
                }},
            [&](const Enemy &) {
                finalIdentifierTranscript.push_back('C');
            });
    expect(kMaximumActiveEnemies == 4U &&
               exhaustedOutcome == EnemySpawnOutcome::QueueExhausted &&
               sameEnemySpawnState(exhaustedQueue, exhaustedQueueBefore) &&
               capacityOutcome == EnemySpawnOutcome::CapacityFull &&
               capacityFull.remaining == 2 &&
               capacityFull.nextSpawnIndex == 2 &&
               capacityFull.nextEnemyId == 9 &&
               nearlyEqual(capacityFull.timer, 0.25f) &&
               coolingOutcome == EnemySpawnOutcome::CoolingDown &&
               coolingDown.remaining == 2 &&
               coolingDown.nextSpawnIndex == 2 &&
               coolingDown.nextEnemyId == 9 &&
               nearlyEqual(coolingDown.timer, 0.15f) &&
               guardedTranscript.empty() &&
               finalIdentifierOutcome == EnemySpawnOutcome::Spawned &&
               committedFinalIdentifier ==
                   std::numeric_limits<int>::max() - 1 &&
               finalIdentifierTranscript ==
                   std::vector<char>{'Q', 'R', 'R', 'R', 'C'} &&
               identifierExhaustedOutcome ==
                   EnemySpawnOutcome::IdentifierExhausted &&
               sameEnemySpawnState(finalIdentifier, exhaustedIdentifier),
           "spawn guards changed timer debit, capacity, identifier exhaustion, "
           "or zero-callback rules");

    reporter.beginSuite("enemy-spawn-transaction-query-order-and-blocked-retry");
    EnemySpawnState blockedState{2, 2, 9, 0.25f};
    std::vector<XZ> blockedQueries;
    int blockedRandomCalls = 0;
    int blockedCommitCalls = 0;
    const EnemySpawnOutcome blockedOutcome = advanceEnemySpawnTransaction(
        blockedState, 0.25f, 0U, validSpawnConfiguration,
        [&](XZ point) {
            blockedQueries.push_back(point);
            return false;
        },
        EnemySpawnRandom{
            [&]() {
                ++blockedRandomCalls;
                return 0.5f;
            },
            [&]() {
                ++blockedRandomCalls;
                return kBasicEnemyType;
            }},
        [&](const Enemy &) { ++blockedCommitCalls; });
    expect(blockedOutcome == EnemySpawnOutcome::SpawnBlocked &&
               blockedQueries.size() == 3U &&
               samePosition(blockedQueries[0],
                            validSpawnConfiguration.spawnPoints[2]) &&
               samePosition(blockedQueries[1],
                            validSpawnConfiguration.spawnPoints[0]) &&
               samePosition(blockedQueries[2],
                            validSpawnConfiguration.spawnPoints[1]) &&
               blockedRandomCalls == 0 && blockedCommitCalls == 0 &&
               blockedState.remaining == 2 &&
               blockedState.nextSpawnIndex == 2 &&
               blockedState.nextEnemyId == 9 &&
               nearlyEqual(blockedState.timer,
                           validSpawnConfiguration.retryInterval),
           "blocked spawn changed Q-Q-Q order, consumed RNG, or missed retry");

    reporter.beginSuite("enemy-spawn-transaction-regular-order-and-commit");
    EnemySpawnConfiguration regularConfiguration = validSpawnConfiguration;
    regularConfiguration.normalInterval = 0.73f;
    EnemySpawnState regularState{3, 1, 40, 0.25f};
    std::vector<XZ> regularQueries;
    std::vector<char> regularTranscript;
    const std::array<float, 3> regularUnitRolls{{0.50f, 0.50f, 0.80f}};
    std::size_t regularUnitIndex = 0U;
    int regularTypeCalls = 0;
    int regularCommitCalls = 0;
    bool regularQueryTiming = true;
    bool regularDrawTiming = true;
    bool regularCommitTiming = false;
    Enemy committedRegular;
    const EnemySpawnOutcome regularOutcome = advanceEnemySpawnTransaction(
        regularState, 0.25f, 1U, regularConfiguration,
        [&](XZ point) {
            regularTranscript.push_back('Q');
            regularQueries.push_back(point);
            regularQueryTiming = regularQueryTiming &&
                regularState.remaining == 3 &&
                regularState.nextSpawnIndex == 1 &&
                regularState.nextEnemyId == 40 &&
                regularState.timer == 0.0f;
            return samePosition(point, regularConfiguration.spawnPoints[2]);
        },
        EnemySpawnRandom{
            [&]() {
                regularTranscript.push_back('R');
                regularDrawTiming = regularDrawTiming &&
                    regularState.remaining == 3 &&
                    regularState.nextSpawnIndex == 1 &&
                    regularState.nextEnemyId == 41 &&
                    regularState.timer == 0.0f &&
                    regularCommitCalls == 0;
                if (regularUnitIndex >= regularUnitRolls.size())
                    return 0.0f;
                return regularUnitRolls[regularUnitIndex++];
            },
            [&]() {
                regularTranscript.push_back('I');
                ++regularTypeCalls;
                regularDrawTiming = regularDrawTiming &&
                    regularState.nextEnemyId == 41 &&
                    regularCommitCalls == 0;
                return kPowerEnemyType;
            }},
        [&](const Enemy &enemy) {
            regularTranscript.push_back('C');
            ++regularCommitCalls;
            regularCommitTiming =
                regularState.remaining == 3 &&
                regularState.nextSpawnIndex == 1 &&
                regularState.nextEnemyId == 41 &&
                regularState.timer == 0.0f &&
                regularUnitIndex == regularUnitRolls.size() &&
                regularTypeCalls == 1;
            committedRegular = enemy;
        });
    expect(regularOutcome == EnemySpawnOutcome::Spawned &&
               regularTranscript ==
                   std::vector<char>{'Q', 'Q', 'R', 'I', 'R', 'R', 'C'} &&
               regularQueries.size() == 2U &&
               samePosition(regularQueries[0],
                            regularConfiguration.spawnPoints[1]) &&
               samePosition(regularQueries[1],
                            regularConfiguration.spawnPoints[2]) &&
               regularQueryTiming && regularDrawTiming &&
               regularCommitTiming && regularCommitCalls == 1 &&
               regularUnitIndex == regularUnitRolls.size() &&
               regularTypeCalls == 1 && regularState.remaining == 2 &&
               regularState.nextSpawnIndex == 0 &&
               regularState.nextEnemyId == 41 &&
               nearlyEqual(regularState.timer,
                           regularConfiguration.normalInterval),
           "regular spawn changed Q-Q-R-I-R-R-C or commit timing");
    EnemySpawnParameters expectedRegularParameters;
    expectedRegularParameters.id = 40;
    expectedRegularParameters.stage = regularConfiguration.stage;
    expectedRegularParameters.position = regularConfiguration.spawnPoints[2];
    expectedRegularParameters.target = regularConfiguration.target;
    expectedRegularParameters.typeRoll = regularUnitRolls[0];
    expectedRegularParameters.regularType = kPowerEnemyType;
    expectedRegularParameters.carrierRoll = regularUnitRolls[1];
    expectedRegularParameters.armorRoll = regularUnitRolls[2];
    expectedRegularParameters.initialFireCooldown =
        regularConfiguration.initialFireCooldown;
    expect(sameEnemyState(
               committedRegular,
               makeSpawnedEnemy(expectedRegularParameters)),
           "regular transaction committed incomplete spawned-enemy state");

    reporter.beginSuite("enemy-spawn-transaction-armor-order-and-commit");
    EnemySpawnConfiguration armorConfiguration = validSpawnConfiguration;
    armorConfiguration.normalInterval = 0.51f;
    armorConfiguration.initialFireCooldown = 0.08f;
    EnemySpawnState armorState{1, 0, 52, 0.0f};
    const std::array<float, 3> armorUnitRolls{{
        belowStage17ArmorChance, belowCarrierChance,
        largestFloatBelow(stage17Thresholds.oneHit)}};
    std::size_t armorUnitIndex = 0U;
    int armorRegularTypeCalls = 0;
    int armorCommitCalls = 0;
    bool armorQueryTiming = false;
    bool armorDrawTiming = true;
    bool armorCommitTiming = false;
    std::vector<char> armorTranscript;
    Enemy committedArmor;
    const EnemySpawnOutcome armorOutcome = advanceEnemySpawnTransaction(
        armorState, 0.0f, 0U, armorConfiguration,
        [&](XZ point) {
            armorTranscript.push_back('Q');
            armorQueryTiming =
                samePosition(point, armorConfiguration.spawnPoints[0]) &&
                armorState.remaining == 1 &&
                armorState.nextSpawnIndex == 0 &&
                armorState.nextEnemyId == 52 && armorState.timer == 0.0f;
            return true;
        },
        EnemySpawnRandom{
            [&]() {
                armorTranscript.push_back('R');
                armorDrawTiming = armorDrawTiming &&
                    armorState.remaining == 1 &&
                    armorState.nextSpawnIndex == 0 &&
                    armorState.nextEnemyId == 53 &&
                    armorState.timer == 0.0f && armorCommitCalls == 0;
                if (armorUnitIndex >= armorUnitRolls.size())
                    return 0.0f;
                return armorUnitRolls[armorUnitIndex++];
            },
            [&]() {
                armorTranscript.push_back('I');
                ++armorRegularTypeCalls;
                return kFastEnemyType;
            }},
        [&](const Enemy &enemy) {
            armorTranscript.push_back('C');
            ++armorCommitCalls;
            armorCommitTiming =
                armorState.remaining == 1 &&
                armorState.nextSpawnIndex == 0 &&
                armorState.nextEnemyId == 53 && armorState.timer == 0.0f &&
                armorUnitIndex == armorUnitRolls.size() &&
                armorRegularTypeCalls == 0;
            committedArmor = enemy;
        });
    expect(armorOutcome == EnemySpawnOutcome::Spawned &&
               armorTranscript ==
                   std::vector<char>{'Q', 'R', 'R', 'R', 'C'} &&
               armorQueryTiming && armorDrawTiming && armorCommitTiming &&
               armorRegularTypeCalls == 0 && armorCommitCalls == 1 &&
               armorUnitIndex == armorUnitRolls.size() &&
               armorState.remaining == 0 && armorState.nextSpawnIndex == 1 &&
               armorState.nextEnemyId == 53 &&
               nearlyEqual(armorState.timer,
                           armorConfiguration.normalInterval),
           "Armor spawn changed Q-R-R-R-C or drew a regular type");
    expect(committedArmor.id == 52 &&
               samePosition(committedArmor.position,
                            armorConfiguration.spawnPoints[0]) &&
               samePosition(committedArmor.target, armorConfiguration.target) &&
               committedArmor.type == kArmorEnemyType &&
               committedArmor.carriesBonus && committedArmor.armor == 1 &&
               nearlyEqual(committedArmor.fireCooldown,
                           armorConfiguration.initialFireCooldown) &&
               nearlyEqual(committedArmor.creationTimer,
                           kEnemyCreationDuration),
           "Armor transaction committed incomplete spawned-enemy state");

    reporter.finish();
    return passed ? 0 : 1;
}
