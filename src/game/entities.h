#ifndef TANKS3D_GAME_ENTITIES_H
#define TANKS3D_GAME_ENTITIES_H

#include "core/coordinates.h"
#include "core/gameplay_rules.h"
#include "core/nation.h"
#include "game/bonus_rules.h"

#include <algorithm>
#include <array>
#include <numeric>
#include <vector>

namespace tanks3d::game
{
using core::CardinalDirection;
using core::Nation;
using core::XZ;

inline constexpr int kEnemyTypeCount = 4;
inline constexpr float kStreakPopupDuration = 0.90f;
// ST_DESTROY_TANK has seven 70 ms frames in the classic sprite sheet.
inline constexpr float kTankDeathDuration = 0.490f;

struct StageTally
{
    std::array<int, kEnemyTypeCount> destroyed{};
    std::array<int, kEnemyTypeCount> enemyPoints{};
    int bonusPoints = 0;
    int scoreAtStageStart = 0;

    void reset(int currentScore)
    {
        destroyed.fill(0);
        enemyPoints.fill(0);
        bonusPoints = 0;
        scoreAtStageStart = currentScore;
    }

    void creditEnemy(int type, int points, bool destroyedNow)
    {
        if (type < 0 || type >= kEnemyTypeCount)
            return;
        enemyPoints[static_cast<std::size_t>(type)] += std::max(0, points);
        if (destroyedNow)
            ++destroyed[static_cast<std::size_t>(type)];
    }

    void creditBonus(int points)
    {
        bonusPoints += std::max(0, points);
    }

    int totalDestroyed() const
    {
        return std::accumulate(destroyed.begin(), destroyed.end(), 0);
    }

    int totalEnemyPoints() const
    {
        return std::accumulate(enemyPoints.begin(), enemyPoints.end(), 0);
    }

    int stagePoints() const
    {
        return totalEnemyPoints() + bonusPoints;
    }
};

struct Player
{
    int id = 0;
    Nation nation = Nation::UnitedStates;
    XZ position{};
    float yaw = 0.0f;
    CardinalDirection driveDirection = CardinalDirection::North;
    CardinalDirection movementDirection = CardinalDirection::North;
    int lives = 1;
    int maximumHitPoints = core::kDefaultPlayerMaximumHitPoints;
    int hitPoints = core::kDefaultPlayerMaximumHitPoints;
    int level = 0;
    bool active = true;
    bool moving = false;
    bool hasBoat = false;
    float shieldTimer = 0.0f;
    float creationTimer = 0.0f;
    float respawnTimer = 0.0f;
    float deathTimer = 0.0f;
    float fireCooldown = 0.0f;
    float dustCooldown = 0.0f;
    float iceSlipTimer = 0.0f;
    bool onIce = false;
    int score = 0;
    int directKillStreak = 0;
    float streakPopupTimer = 0.0f;
    StageTally stageTally;

    void creditDirectEnemyHit(int enemyType, int points, bool destroyedNow)
    {
        score += points;
        stageTally.creditEnemy(enemyType, points, destroyedNow);
        // A shell from a tank that has already been destroyed can still finish
        // its flight. It keeps its score, but cannot start a new-life streak.
        if (destroyedNow && active)
        {
            ++directKillStreak;
            streakPopupTimer = kStreakPopupDuration;
        }
    }

    void resetDirectKillStreak()
    {
        directKillStreak = 0;
        streakPopupTimer = 0.0f;
    }

    bool needsHealing() const
    {
        return maximumHitPoints > 1 && active && hitPoints > 0 &&
               hitPoints < maximumHitPoints;
    }

    bool takeUnprotectedHit()
    {
        hitPoints = std::max(0, hitPoints - 1);
        return hitPoints == 0;
    }

    bool healOneHitPoint()
    {
        if (!needsHealing())
            return false;
        hitPoints = std::min(maximumHitPoints, hitPoints + 1);
        return true;
    }
};

inline bool playerMeetsBonusTypeEligibility(const Player &player,
                                            BonusType type)
{
    return isValidBonusType(type) &&
           (type != BonusType::Bandage || player.needsHealing());
}

enum class PlayerHitResult
{
    Shielded,
    BoatAbsorbed,
    Damaged,
    Destroyed
};

inline PlayerHitResult resolvePlayerHit(Player &player)
{
    if (player.shieldTimer > 0.0f)
        return PlayerHitResult::Shielded;
    if (player.hasBoat)
    {
        player.hasBoat = false;
        return PlayerHitResult::BoatAbsorbed;
    }
    return player.takeUnprotectedHit() ? PlayerHitResult::Destroyed
                                       : PlayerHitResult::Damaged;
}

struct Enemy
{
    int id = -1;
    XZ position{};
    XZ target{};
    float yaw = 3.14159265358979323846f;
    CardinalDirection driveDirection = CardinalDirection::South;
    CardinalDirection movementDirection = CardinalDirection::South;
    int type = 0;
    int armor = 1;
    bool carriesBonus = false;
    bool destroyed = false;
    bool moving = false;
    float frozenTimer = 0.0f;
    float fireCooldown = 0.0f;
    float creationTimer = 0.0f;
    float deathTimer = 0.0f;
    float blockedTimer = 0.0f;
    float dustCooldown = 0.0f;
    float directionTimer = 0.0f;
    float directionDecisionInterval = 0.1f;
    float movementDelay = 0.1f;
    float iceSlipTimer = 0.0f;
    bool onIce = false;
};

enum class ShellOwner
{
    Player,
    Enemy
};

struct Shell
{
    XZ position{};
    XZ velocity{};
    ShellOwner owner = ShellOwner::Player;
    int ownerIndex = -1;
    bool power = false;
    bool impacting = false;
    float life = 4.0f;
};

inline bool enemyDestructionComplete(const Enemy &enemy,
                                     const std::vector<Shell> &shells)
{
    if (!enemy.destroyed || enemy.deathTimer > 0.0f)
        return false;
    return std::none_of(shells.begin(), shells.end(),
                        [&](const Shell &shell) {
                            return shell.owner == ShellOwner::Enemy &&
                                   shell.ownerIndex == enemy.id;
                        });
}
} // namespace tanks3d::game

#endif
