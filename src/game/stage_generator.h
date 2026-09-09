#ifndef TANKS3D_GAME_STAGE_GENERATOR_H
#define TANKS3D_GAME_STAGE_GENERATOR_H

#include "core/coordinates.h"

#include <array>

namespace tanks3d::game
{
using core::XZ;

inline constexpr int kMapSize = 26;
inline constexpr int kStageCount = 35;
inline constexpr float kTankRadius = 0.875f;

inline constexpr std::array<XZ, 3> kEnemySpawnPoints{{
    {1.0f, 1.0f}, {13.0f, 1.0f}, {25.0f, 1.0f}}};
inline constexpr std::array<XZ, 2> kPlayerSpawnPoints{{
    {9.0f, 25.0f}, {17.0f, 25.0f}}};

using StageTileGrid =
    std::array<std::array<char, kMapSize>, kMapSize>;

class StageGenerator
{
public:
    // StageMap owns requested-stage wrapping. This seam accepts its canonical
    // 1..kStageCount value and returns a fresh, fully initialized tile grid.
    // All 35 layouts follow the original NES display order and preserve the
    // verified initial terrain, including half walls and base-guard bricks.
    // No spawn clearing, road carving, theme variation or gameplay RNG is used.
    static StageTileGrid generate(int canonicalStage);
};
} // namespace tanks3d::game

#endif
