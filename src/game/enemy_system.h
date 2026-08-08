#ifndef TANKS3D_GAME_ENEMY_SYSTEM_H
#define TANKS3D_GAME_ENEMY_SYSTEM_H

#include "game/entities.h"

#include <array>
#include <cstddef>
#include <functional>
#include <optional>
#include <vector>

namespace tanks3d::game
{
inline constexpr int kBasicEnemyType = 0;
inline constexpr int kFastEnemyType = 1;
inline constexpr int kPowerEnemyType = 2;
inline constexpr int kArmorEnemyType = 3;
inline constexpr float kBonusCarrierChance = 0.12f;
inline constexpr float kEnemySpawnInterval = 0.5f;
inline constexpr float kEnemySpawnRetryInterval = 0.15f;
inline constexpr float kEnemyInitialFireDelay = 0.1f;
inline constexpr float kEnemyCreationDuration = 1.0f;
inline constexpr float kEnemyPursuitPrimaryProbability = 0.7f;
inline constexpr float kArmorEnemyFiringLaneHalfWidth = 2.0f;
inline constexpr std::size_t kEnemySpawnSlotCount = 3U;
inline constexpr std::size_t kMaximumActiveEnemies = 4U;
inline constexpr float kEnemyBlockedEscapeDelay = 0.30f;
inline constexpr float kEnemyEscapeCommitTime = 0.55f;
inline constexpr float kEnemyEscapeProbeStep = 0.40f;
inline constexpr float kEnemyEscapeDecisionIntervalRange = 0.18f;
inline constexpr float kEnemyDirectionDecisionMinimumInterval = 0.1f;
inline constexpr float kEnemyDirectionDecisionIntervalRange = 0.8f;
inline constexpr float kBasicEnemyPursuitProbability = 0.8f;
inline constexpr float kOtherEnemyPursuitProbability = 0.5f;
inline constexpr float kEnemyCollisionProbePadding = 1.0f / 16.0f;
inline constexpr float kEnemyTrackDustCooldown = 0.19f;

enum class EnemyFramePhase : unsigned char
{
    Destroyed,
    Creating,
    Frozen,
    Active
};

enum class EnemySteeringOutcome : unsigned char
{
    Invalid,
    NoDecision,
    OrdinaryDecision,
    EscapeCommitted,
    EscapeUnavailable
};

enum class EnemyFireOutcome : unsigned char
{
    Invalid,
    CoolingDown,
    AimRejected,
    OwnedShellPresent,
    Fired
};

enum class EnemySpawnOutcome : unsigned char
{
    Invalid,
    QueueExhausted,
    IdentifierExhausted,
    CapacityFull,
    CoolingDown,
    SpawnBlocked,
    Spawned
};

struct EnemyFrameStart
{
    // Invalid elapsed time returns valid == false and leaves Enemy untouched.
    bool valid = false;
    EnemyFramePhase phase = EnemyFramePhase::Active;
    bool hadMomentum = false;
    CardinalDirection previousTravel = CardinalDirection::None;
};

struct EnemyEscapeChoice
{
    CardinalDirection direction = CardinalDirection::None;
    XZ alignedPosition{};
    int clearProbeCount = 0;

    bool available() const
    {
        return direction != CardinalDirection::None;
    }
};

struct EnemyFirePlan
{
    bool shouldFire = true;
    float baseReloadInterval = 0.0f;
};

struct EnemyFireConfiguration
{
    int ratePercent = 0;
    // CombatSystem owns the canonical geometry constant. Passing it explicitly
    // keeps EnemySystem independent while still constructing the complete
    // detached Shell value before the orchestration callback.
    float shellSpawnDistance = -1.0f;
};

struct EnemyShellLaunchIntent
{
    Shell shell{};
    XZ tankPosition{};
    CardinalDirection direction = CardinalDirection::None;
    int enemyType = kBasicEnemyType;
};

struct EnemyTrackDustIntent
{
    XZ position{};
    XZ velocity{};
};

struct EnemyMovementUpdate
{
    bool valid = false;
    bool blocked = false;
    bool movementAccepted = false;
    std::optional<EnemyTrackDustIntent> dust{};
};

struct EnemyArmorThresholds
{
    double oneHit = 0.0;
    double twoHits = 0.0;
    double threeHits = 0.0;
};

// The caller owns random-number generation so the Armor branch can omit the
// regular-type integer draw. makeSpawnedEnemy ignores regularType when
// typeRoll selects Armor and otherwise preserves the supplied regular type.
struct EnemySpawnParameters
{
    int id = -1;
    int stage = 1;
    XZ position{};
    XZ target{};
    float typeRoll = 0.0f;
    int regularType = kBasicEnemyType;
    float carrierRoll = 0.0f;
    float armorRoll = 0.0f;
    float initialFireCooldown = kEnemyInitialFireDelay;
};

using EnemyPositionAvailable = std::function<bool(XZ)>;
using EnemyUnitRoll = std::function<float()>;
using EnemyDirectionRoll = std::function<int()>;
using EnemyRegularTypeRoll = std::function<int()>;
using EnemyOwnedShellQuery = std::function<bool(int)>;
using EnemyShellLaunch =
    std::function<void(const EnemyShellLaunchIntent &)>;
using EnemySpawnCommit = std::function<void(const Enemy &)>;

struct EnemyFireRandom
{
    // Pulled exactly once for a due attempt, before any owned-shell query.
    // The callback must be non-throwing and return a value in [0, 1).
    EnemyUnitRoll reloadRoll;
};

struct EnemySpawnState
{
    int remaining = 0;
    int nextSpawnIndex = 0;
    int nextEnemyId = 0;
    float timer = 0.0f;
};

struct EnemySpawnConfiguration
{
    int stage = 1;
    std::array<XZ, kEnemySpawnSlotCount> spawnPoints{};
    XZ target{};
    float initialFireCooldown = kEnemyInitialFireDelay;
    float normalInterval = kEnemySpawnInterval;
    float retryInterval = kEnemySpawnRetryInterval;
};

struct EnemySpawnRandom
{
    // Callbacks are pulled only after a spawn point is accepted. They must be
    // non-throwing and return values in [0, 1) and [0, 2], respectively.
    EnemyUnitRoll unitRoll;
    EnemyRegularTypeRoll regularTypeRoll;
};

struct EnemySteeringRandom
{
    // Callbacks are pulled only by the selected branch. They must be
    // non-throwing and return values in [0, 1) and [0, 3], respectively.
    EnemyUnitRoll unitRoll;
    EnemyDirectionRoll directionRoll;
};

// Advances only the clocks and moving-state gate at the beginning of one enemy
// update. The caller must stop when the returned phase is not Active. Holding
// creation freezes only the creation warning; a Clock bonus still expires.
EnemyFrameStart beginEnemyFrame(Enemy &enemy, float dt,
                                bool holdCreationTimer);

// Commits movement for an enemy that has passed beginEnemyFrame's Active gate
// and whose steering decision is already complete. Availability queries retain
// the historical lane-snap-then-probe order. Dust is returned as a detached
// intent so Game3D can preserve FX-before-fire ordering and then commit the
// cooldown. Invalid inputs leave Enemy untouched and issue no queries.
EnemyMovementUpdate advanceActiveEnemyMovement(
    Enemy &enemy, const EnemyFrameStart &frame, float dt,
    float movementSpeed, bool onIce,
    const EnemyPositionAvailable &positionAvailable);

// Selects a collision-free local route after an enemy repeatedly fails to
// advance. Candidate directions keep the historical tie-breaker order, require
// an available snapped position plus at least one clear forward probe, prefer
// progress and longer clear paths, and mildly penalize reversing. An empty
// availability query fails closed.
EnemyEscapeChoice chooseEnemyEscape(
    XZ position, CardinalDirection blockedDirection, XZ target,
    int tieBreaker, const EnemyPositionAvailable &positionAvailable);

// Advances the active steering timer and commits at most one direction
// decision. Random callbacks are invoked synchronously at the historical draw
// points: escape success uses R, pursuit uses R-R-R, and wandering uses R-R-I.
// Failed escape and no-decision paths consume no randomness. Invalid static
// inputs leave Enemy untouched and issue no callbacks.
EnemySteeringOutcome advanceActiveEnemySteering(
    Enemy &enemy, const EnemyFrameStart &frame, float dt,
    const EnemyPositionAvailable &positionAvailable,
    const EnemySteeringRandom &random);

// Basic and Armor enemies may replace the base fallback with a strictly closer
// active player. Equal-distance candidates preserve the earlier choice, making
// the base and then player vector order deterministic. Other or invalid types
// retain the base target.
XZ chooseEnemyTarget(int enemyType, XZ enemyPosition, XZ baseTarget,
                     const std::vector<Player> &players);

// Selects the dominant pursuit axis for rolls strictly below 0.7 and the other
// axis otherwise. Equal axis distances make the vertical direction dominant.
// The roll domain is [0, 1); invalid coordinates or rolls fail closed with
// None.
CardinalDirection chooseEnemyPursuitDirection(XZ enemyPosition, XZ target,
                                              float primaryDirectionRoll);

// A blocked Armor enemy fires regardless of its legal facing. Otherwise it
// fires only toward a target in front and strictly within the two-tile lateral
// lane. None preserves only the blocked behavior; invalid enum values fail
// closed.
bool armorEnemyShouldFire(CardinalDirection movementDirection,
                          XZ enemyPosition, XZ target, bool blocked);

// Projects the post-movement enemy state and an already-consumed [0, 1) reload
// roll into the historical type-specific firing decision. It does not draw
// randomness, inspect owned shells, fire, or commit the cooldown.
EnemyFirePlan planEnemyFire(const Enemy &enemy, bool blocked,
                            float reloadRoll);

// Owns one complete post-movement fire attempt. A positive cooldown invokes no
// callback. A due attempt pulls R, applies the type/Armor policy, conditionally
// queries Q for an owned shell, and synchronously launches C only when the slot
// is open. The exact transcripts are R, R-Q, and R-Q-C for aim rejection,
// occupied, and fired outcomes. C receives a complete detached Shell whose
// launch direction comes from driveDirection; Armor policy deliberately uses
// movementDirection. Every valid due outcome commits its tuned cooldown only
// after Q/C return. Static invalid input is atomic and invokes no callback; an
// invalid R leaves Enemy unchanged after consuming that one draw. Callbacks
// must be non-throwing, must not mutate or invalidate the source Enemy or its
// owning container, and must not re-enter this transaction. The launch callback
// also must not retain references to the intent.
EnemyFireOutcome advanceEnemyFireTransaction(
    Enemy &enemy, bool blocked,
    const EnemyFireConfiguration &configuration,
    const EnemyFireRandom &random,
    const EnemyOwnedShellQuery &ownedShellPresent,
    const EnemyShellLaunch &launch);

// These probability rules intentionally use strict boundaries to match the
// original game. Callers supply rolls in [0, 1); no randomness is consumed.
float enemyArmorTankChanceForStage(int stage);
bool enemyRollCreatesArmorTank(int stage, float roll);
bool enemyRollCreatesBonusCarrier(float roll);
EnemyArmorThresholds enemyArmorThresholdsForStage(int stage);
int enemyArmorForRoll(int stage, float roll);

// Builds the complete initial gameplay state for a newly warned enemy. It is
// pure: the caller remains responsible for IDs, spawn-slot rotation, cadence
// timers, container insertion, and the conditional random draw transcript.
Enemy makeSpawnedEnemy(const EnemySpawnParameters &parameters);

// Scans exactly three availability slots from nextSpawnIndex with wraparound.
// Invalid starting indices or a fully blocked set return -1.
int chooseEnemySpawnIndex(
    const std::array<bool, kEnemySpawnSlotCount> &spawnAvailable,
    int nextSpawnIndex);

// Owns one complete spawn attempt. Queue and identifier exhaustion precede the
// timer debit; capacity and strict-positive cooldown gates follow it.
// Availability stops at the first open slot.
// Successful Armor and regular transcripts are R-R-R and R-I-R-R. The ID is
// committed before those pulls; commit is then called synchronously before the
// remaining count, slot rotation, and normal timer are committed. Availability,
// random, and commit callbacks must be non-throwing. Invalid state or
// configuration is atomic and invokes no callback.
EnemySpawnOutcome advanceEnemySpawnTransaction(
    EnemySpawnState &state, float dt, std::size_t activeEnemyCount,
    const EnemySpawnConfiguration &configuration,
    const EnemyPositionAvailable &positionAvailable,
    const EnemySpawnRandom &random,
    const EnemySpawnCommit &commit);
} // namespace tanks3d::game

#endif
