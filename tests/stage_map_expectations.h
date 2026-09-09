#ifndef TANKS3D_STAGE_MAP_EXPECTATIONS_H
#define TANKS3D_STAGE_MAP_EXPECTATIONS_H

#include "game/stage_map.h"
#include "stage_layout_expectations.h"

#include <cstdint>

namespace tanks3d::game
{
// Deliberate test-only seam: fixture terrain must never leak into real levels.
struct StageMapTestAccess
{
    static void install(StageMap &map, const StageTileGrid &tiles)
    {
        map.tiles_ = tiles;
        map.resetTerrainDamageState();
        map.prepareGovernmentBase();
    }
};
} // namespace tanks3d::game

namespace tanks3d_test
{
inline tanks3d::game::StageTileGrid terrainFixtureGrid()
{
    tanks3d::game::StageTileGrid grid{};
    for (auto &row : grid)
        row.fill('.');
    const auto rectangle = [&](int top, int left, int bottom, int right,
                               char value) {
        for (int row = top; row <= bottom; ++row)
            for (int column = left; column <= right; ++column)
                grid[row][column] = value;
    };
    rectangle(8, 4, 11, 7, '#');
    rectangle(8, 10, 9, 11, '@');
    rectangle(14, 4, 17, 7, '~');
    rectangle(14, 10, 17, 13, '%');
    rectangle(14, 18, 17, 21, '-');
    // The historical steering cases belong to the test, not original stage 2.
    for (int row = 2; row <= 3; ++row)
        grid[row][19] = grid[row][22] = '@';
    grid[4][20] = grid[4][21] = '@';
    grid[5][12] = grid[5][13] = '@';
    grid[3][14] = grid[4][14] = '@';
    for (const auto &cell : tanks3d::game::kGovernmentWallCells)
        grid[cell[0]][cell[1]] = '#';
    return grid;
}

inline bool loadTerrainFixture(tanks3d::game::StageMap &map,
                               std::string &error)
{
    tanks3d::game::StageMapTestAccess::install(map, terrainFixtureGrid());
    error.clear();
    return true;
}

inline std::uint64_t stageLayoutSignature(
    const tanks3d::game::StageMap &map)
{
    std::uint64_t signature = detail::initialStageLayoutSignature();
    for (int row = 0; row < tanks3d::game::kMapSize; ++row)
    {
        for (int column = 0; column < tanks3d::game::kMapSize; ++column)
        {
            detail::mixStageLayoutByte(
                signature,
                static_cast<unsigned char>(map.tile(row, column)));
            detail::mixStageLayoutByte(
                signature, map.brickMask(row, column));
        }
    }
    return signature;
}
} // namespace tanks3d_test

#endif
