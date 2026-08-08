#ifndef TANKS3D_CORE_GAMEPLAY_RULES_H
#define TANKS3D_CORE_GAMEPLAY_RULES_H

#include <algorithm>

namespace tanks3d::core
{
// The 2D game moves an 8 px projectile at 0.23 px/ms, or 14.375 tiles/s.
// The local 3D camera uses 68% of that speed (15% below the previous 80%
// pacing) so a round remains readable on screen, while upgraded player rounds
// and Tank C retain the original 1.3x relationship.
inline constexpr float kClassicBaseShellSpeed = 14.375f;
inline constexpr float kShellPacingScale = 0.68f;
inline constexpr float kBaseShellSpeed =
    kClassicBaseShellSpeed * kShellPacingScale;
inline constexpr float kFastShellSpeed = kBaseShellSpeed * 1.3f;

// Player movement uses the same px/ms-to-tiles/s conversion as projectiles:
// 0.08 px/ms at level 0, then the original 1.3x Star boost at levels 1-3.
inline constexpr float kBasePlayerSpeed = 5.0f;
inline constexpr float kFastPlayerSpeed = kBasePlayerSpeed * 1.3f;

inline constexpr int kDefaultPlayerMaximumHitPoints = 3;
inline constexpr int kMinimumPlayerMaximumHitPoints = 1;
inline constexpr int kMaximumPlayerMaximumHitPoints = 6;
inline constexpr int kEnemyTuningMinimumPercent = -30;
inline constexpr int kEnemyTuningMaximumPercent = 30;
inline constexpr int kEnemyTuningPercentStep = 5;

struct AdvancedGameSettings
{
    int playerMaximumHitPoints = kDefaultPlayerMaximumHitPoints;
    int enemySpeedPercent = 0;
    int enemyFireRatePercent = 0;
    int enemySpawnRatePercent = 0;
};

inline constexpr int normalizedEnemyTuningPercent(int requestedPercent)
{
    const int clamped = std::clamp(requestedPercent,
                                   kEnemyTuningMinimumPercent,
                                   kEnemyTuningMaximumPercent);
    const int offset = clamped - kEnemyTuningMinimumPercent;
    return kEnemyTuningMinimumPercent +
           ((offset + kEnemyTuningPercentStep / 2) /
            kEnemyTuningPercentStep) * kEnemyTuningPercentStep;
}

inline constexpr AdvancedGameSettings normalizedAdvancedSettings(
    AdvancedGameSettings settings)
{
    settings.playerMaximumHitPoints = std::clamp(
        settings.playerMaximumHitPoints, kMinimumPlayerMaximumHitPoints,
        kMaximumPlayerMaximumHitPoints);
    settings.enemySpeedPercent = normalizedEnemyTuningPercent(
        settings.enemySpeedPercent);
    settings.enemyFireRatePercent = normalizedEnemyTuningPercent(
        settings.enemyFireRatePercent);
    settings.enemySpawnRatePercent = normalizedEnemyTuningPercent(
        settings.enemySpawnRatePercent);
    return settings;
}

inline int upgradedPlayerLevel(int level)
{
    return std::min(3, std::clamp(level, 0, 3) + 1);
}

struct PlayerLevelStats
{
    float movementSpeed;
    float shellSpeed;
    int maximumShells;
    bool powerShell;
};

inline constexpr PlayerLevelStats playerLevelStats(int requestedLevel)
{
    const int level = std::clamp(requestedLevel, 0, 3);
    return {
        level > 0 ? kFastPlayerSpeed : kBasePlayerSpeed,
        level > 0 ? kFastShellSpeed : kBaseShellSpeed,
        level >= 2 ? level + 1 : 2,
        level >= 3};
}
} // namespace tanks3d::core

#endif
