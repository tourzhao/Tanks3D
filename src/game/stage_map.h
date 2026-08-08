#ifndef TANKS3D_GAME_STAGE_MAP_H
#define TANKS3D_GAME_STAGE_MAP_H

#include "core/coordinates.h"
#include "core/nation.h"
#include "game/stage_generator.h"

#include <array>
#include <filesystem>
#include <string>

namespace tanks3d::game
{
using core::CardinalDirection;
using core::Nation;
using core::XZ;

inline constexpr float kShellHalfSize = 0.25f;
inline constexpr float kGovernmentSteelDuration = 20.0f;
inline constexpr float kGovernmentSteelWarningDuration = 3.0f;
inline constexpr float kGovernmentSteelFlashPeriod = 0.18f;
inline constexpr int kGovernmentWallCount = 5;
inline constexpr int kGovernmentWallMaximumHealth = 4;
inline constexpr int kGovernmentPowerShellDamage = 2;
inline constexpr XZ kGovernmentBaseCenter{13.0f, 23.60f};
inline constexpr float kGovernmentWallRadius = 1.95f;
inline constexpr float kGovernmentWallThickness = 1.16f;
inline constexpr float kGovernmentWallEndOverlap = 0.08f;
inline constexpr float kGovernmentPentagonYaw =
    45.0f * (3.14159265358979323846f / 180.0f);
inline constexpr float kGovernmentCoreRadius = 0.92f;

enum class GovernmentBaseTheme
{
    UnitedStatesPentagon,
    SovietRingCastle,
    GermanParliament
};

constexpr GovernmentBaseTheme governmentBaseThemeForNation(Nation nation)
{
    switch (nation)
    {
    case Nation::SovietUnion:
        return GovernmentBaseTheme::SovietRingCastle;
    case Nation::Germany:
        return GovernmentBaseTheme::GermanParliament;
    case Nation::UnitedStates:
    default:
        return GovernmentBaseTheme::UnitedStatesPentagon;
    }
}

enum class ImpactKind
{
    None,
    Brick,
    GovernmentWall,
    Steel,
    Boundary
};

struct BrickDamage
{
    int row = -1;
    int column = -1;
    unsigned char beforeMask = 0U;
    unsigned char afterMask = 0U;
};

struct ShellImpactDetails
{
    std::array<BrickDamage, 2> bricks{};
    int brickCount = 0;
    int governmentWallIndex = -1;
    int governmentWallHealthBefore = 0;
    int governmentWallHealthAfter = 0;

    bool destroyedBrick() const;
    bool destroyedGovernmentWall() const;
};

struct GovernmentWallSegment
{
    XZ start{};
    XZ end{};
    XZ center{};
    XZ along{};
    XZ outward{};
    float length = 0.0f;
    float halfLength = 0.0f;
    float halfThickness = kGovernmentWallThickness * 0.5f;
    float yaw = 0.0f;
};

bool bonusOverlapsGovernmentBase(XZ position);
XZ governmentPentagonCorner(int requestedIndex);
GovernmentWallSegment governmentWallSegment(int index);
bool governmentWallOverlapsShell(const GovernmentWallSegment &segment,
                                 XZ shellCenter);
bool governmentWallOverlapsAabb(const GovernmentWallSegment &segment,
                                XZ position, float halfExtent);
bool aabbOverlapsRectangle(XZ center, float halfExtent,
                           float minimumX, float minimumZ,
                           float maximumX, float maximumZ);
int normalizedStage(int stage);

class StageMap
{
public:
    void setGovernmentNation(Nation nation);
    Nation governmentNation() const;
    GovernmentBaseTheme governmentBaseTheme() const;
    bool load(const std::filesystem::path &resourceRoot,
              int requestedStage, std::string &error);
    int stage() const;
    char tile(int row, int column) const;
    unsigned char brickMask(int row, int column) const;
    unsigned char brickHitCount(int row, int column) const;
    CardinalDirection brickFirstDirection(int row, int column) const;
    bool wallOccupies(XZ position) const;
    bool solidSeparatesShells(XZ firstPosition, XZ secondPosition) const;
    void prepareShowcaseArena();
    void prepareGovernmentBase();
    void repairGovernmentWalls();
    void activateGovernmentSteel();
    void updateGovernmentProtection(float dt);
    bool governmentWallsSteel() const;
    float governmentSteelTimeRemaining() const;
    bool governmentSteelVisible() const;
    int governmentWallHealth(int index) const;
    bool isInsideBase(XZ position) const;
    bool shellHitsGovernmentCore(XZ shellCenter) const;
    bool collidesWithTank(XZ position, float halfExtent,
                          bool allowWater = false) const;
    bool hasTankRoute(XZ start, XZ goal) const;
    bool isIce(XZ position) const;
    ImpactKind impactShell(XZ position, bool powerShell,
                           CardinalDirection direction,
                           ShellImpactDetails *details = nullptr);

private:
    void resetTerrainDamageState();
    bool generatedStageIsPlayable() const;

    int stage_ = 1;
    Nation governmentNation_ = Nation::UnitedStates;
    StageTileGrid tiles_{};
    std::array<std::array<unsigned char, kMapSize>, kMapSize> brickMask_{};
    std::array<std::array<unsigned char, kMapSize>, kMapSize> brickHitCount_{};
    std::array<std::array<unsigned char, kMapSize>, kMapSize>
        brickFirstDirection_{};
    float governmentSteelTimer_ = 0.0f;
    std::array<int, kGovernmentWallCount> governmentWallHealth_{{
        kGovernmentWallMaximumHealth, kGovernmentWallMaximumHealth,
        kGovernmentWallMaximumHealth, kGovernmentWallMaximumHealth,
        kGovernmentWallMaximumHealth}};
};
} // namespace tanks3d::game

#endif
