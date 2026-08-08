#include "game/stage_generator.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace tanks3d::game
{
namespace
{
std::uint32_t nextStageRandom(std::uint32_t &state)
{
    state ^= state << 13U;
    state ^= state >> 17U;
    state ^= state << 5U;
    return state;
}

void clearSpawnArea(StageTileGrid &tiles, XZ center)
{
    constexpr float clearance = kTankRadius + 0.20f;
    const int left = std::max(
        0, static_cast<int>(std::floor(center.x - clearance)));
    const int right = std::min(
        kMapSize - 1,
        static_cast<int>(std::floor(center.x + clearance)));
    const int top = std::max(
        0, static_cast<int>(std::floor(center.z - clearance)));
    const int bottom = std::min(
        kMapSize - 1,
        static_cast<int>(std::floor(center.z + clearance)));
    for (int row = top; row <= bottom; ++row)
        for (int column = left; column <= right; ++column)
            tiles[static_cast<std::size_t>(row)]
                 [static_cast<std::size_t>(column)] = '.';
}

void clearGovernmentBaseFootprint(StageTileGrid &tiles)
{
    for (int row = 21; row <= 25; ++row)
        for (int column = 10; column <= 15; ++column)
            tiles[static_cast<std::size_t>(row)]
                 [static_cast<std::size_t>(column)] = '.';
}
} // namespace

StageTileGrid StageGenerator::generate(int canonicalStage)
{
    StageTileGrid tiles{};
    for (auto &row : tiles)
        row.fill('.');

    std::array<char, 36> blockTerrain{};
    std::size_t terrainIndex = 0;
    for (int count = 0; count < 15; ++count)
        blockTerrain[terrainIndex++] = '#';
    for (int count = 0; count < 12; ++count)
        blockTerrain[terrainIndex++] = '%';
    for (int count = 0; count < 3; ++count)
        blockTerrain[terrainIndex++] = '@';
    for (int count = 0; count < 3; ++count)
        blockTerrain[terrainIndex++] = '~';
    for (int count = 0; count < 3; ++count)
        blockTerrain[terrainIndex++] = '-';

    std::uint32_t randomState =
        0x6d2b79f5U ^
        (static_cast<std::uint32_t>(canonicalStage) * 0x9e3779b9U);
    for (std::size_t remaining = blockTerrain.size(); remaining > 1;
         --remaining)
    {
        const std::size_t other = nextStageRandom(randomState) % remaining;
        std::swap(blockTerrain[remaining - 1], blockTerrain[other]);
    }

    terrainIndex = 0;
    for (int blockRow = 0; blockRow < 6; ++blockRow)
    {
        const int top = 2 + blockRow * 4;
        for (int blockColumn = 0; blockColumn < 6; ++blockColumn)
        {
            const int left = 2 + blockColumn * 4;
            const char terrain = blockTerrain[terrainIndex++];
            for (int rowOffset = 0; rowOffset < 2; ++rowOffset)
                for (int columnOffset = 0; columnOffset < 2;
                     ++columnOffset)
                    tiles[static_cast<std::size_t>(top + rowOffset)]
                         [static_cast<std::size_t>(left + columnOffset)] =
                        terrain;
        }
    }

    for (int bit = 0; bit < 6; ++bit)
    {
        const int column = 2 + bit * 4;
        tiles[12][static_cast<std::size_t>(column)] =
            (canonicalStage & (1 << bit)) != 0 ? '%' : '-';
    }

    for (XZ spawn : kEnemySpawnPoints)
        clearSpawnArea(tiles, spawn);
    for (XZ spawn : kPlayerSpawnPoints)
        clearSpawnArea(tiles, spawn);

    for (int row = 18; row <= 22; ++row)
        for (int column = 12; column <= 13; ++column)
            tiles[static_cast<std::size_t>(row)]
                 [static_cast<std::size_t>(column)] = '.';

    if (canonicalStage == 2)
    {
        for (int row = 2; row <= 3; ++row)
        {
            tiles[static_cast<std::size_t>(row)][19] = '@';
            tiles[static_cast<std::size_t>(row)][22] = '@';
        }
        tiles[4][20] = '@';
        tiles[4][21] = '@';

        tiles[5][12] = '@';
        tiles[5][13] = '@';
        tiles[3][14] = '@';
        tiles[4][14] = '@';
        for (int row = 3; row <= 4; ++row)
            for (int column = 10; column <= 11; ++column)
                tiles[static_cast<std::size_t>(row)]
                     [static_cast<std::size_t>(column)] = '.';
    }

    clearGovernmentBaseFootprint(tiles);
    return tiles;
}
} // namespace tanks3d::game
