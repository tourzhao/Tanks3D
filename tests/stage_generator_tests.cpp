#include "game/stage_generator.h"

#include "stage_layout_expectations.h"
#include "test_support.h"

#include <algorithm>
#include <array>
#include <bitset>
#include <cmath>
#include <cstdint>
#include <string>

namespace
{
using tanks3d::core::XZ;
using tanks3d::game::StageGenerator;
using tanks3d::game::StageTileGrid;
using tanks3d::game::kEnemySpawnPoints;
using tanks3d::game::kMapSize;
using tanks3d::game::kPlayerSpawnPoints;
using tanks3d::game::kStageCount;
using tanks3d::game::kTankRadius;

bool isValidTerrain(char tile)
{
    switch (tile)
    {
    case '.':
    case '#':
    case '%':
    case '@':
    case '~':
    case '-':
        return true;
    }
    return false;
}

bool containsOnlyValidTerrain(const StageTileGrid &grid)
{
    for (const auto &row : grid)
        for (char tile : row)
            if (!isValidTerrain(tile))
                return false;
    return true;
}

bool rectangleIsOpen(const StageTileGrid &grid, int top, int left,
                     int bottom, int right)
{
    for (int row = top; row <= bottom; ++row)
        for (int column = left; column <= right; ++column)
        {
            const char tile = grid[static_cast<std::size_t>(row)]
                                  [static_cast<std::size_t>(column)];
            if (tile != '.' && tile != '%' && tile != '-')
                return false;
        }
    return true;
}

bool spawnAreaIsOpen(const StageTileGrid &grid, XZ center,
                     float safetyMargin)
{
    const float clearance = kTankRadius + safetyMargin;
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
    return rectangleIsOpen(grid, top, left, bottom, right);
}

using TerrainMask = std::bitset<kMapSize * kMapSize>;

TerrainMask terrainMask(const StageTileGrid &grid, bool blockingOnly)
{
    TerrainMask mask;
    for (int row = 0; row < kMapSize; ++row)
    {
        for (int column = 0; column < kMapSize; ++column)
        {
            const char tile = grid[static_cast<std::size_t>(row)]
                                  [static_cast<std::size_t>(column)];
            const bool occupied = blockingOnly
                                      ? tile == '#' || tile == '@' || tile == '~'
                                      : tile != '.';
            mask.set(static_cast<std::size_t>(row * kMapSize + column),
                     occupied);
        }
    }
    return mask;
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

    reporter.beginSuite("stage-generator-golden-layouts");
    std::array<std::uint64_t, kStageCount> generatedSignatures{};
    for (int stage = 1; stage <= kStageCount; ++stage)
    {
        const StageTileGrid first = StageGenerator::generate(stage);
        const StageTileGrid repeated = StageGenerator::generate(stage);
        const std::uint64_t signature =
            tanks3d_test::stageLayoutSignature(first);
        generatedSignatures[static_cast<std::size_t>(stage - 1)] =
            signature;
        expect(first == repeated && containsOnlyValidTerrain(first) &&
                   signature ==
                       tanks3d_test::kExpectedStageLayoutSignatures[
                           static_cast<std::size_t>(stage - 1)],
               "stage " + std::to_string(stage) +
                   " generator output is unstable, invalid, or changed");
    }

    reporter.beginSuite("stage-generator-structural-contracts");
    bool everyLayoutUnique = true;
    for (std::size_t first = 0; first < generatedSignatures.size(); ++first)
        for (std::size_t second = first + 1;
             second < generatedSignatures.size(); ++second)
            everyLayoutUnique = everyLayoutUnique &&
                                generatedSignatures[first] !=
                                    generatedSignatures[second];
    expect(everyLayoutUnique,
           "two canonical stages generated the same initial layout");

    const StageTileGrid original = StageGenerator::generate(1);
    StageTileGrid modifiedCopy = original;
    modifiedCopy[0][0] = '#';
    const StageTileGrid regenerated = StageGenerator::generate(1);
    expect(modifiedCopy != original && original[0][0] == '.' &&
               regenerated == original,
           "mutating a returned grid changed generator-owned state");

    bool everyEnemySpawnOpen = true;
    bool everyPlayerSpawnOpen = true;
    bool everyOriginalEnclosureIntact = true;
    for (int stage = 1; stage <= kStageCount; ++stage)
    {
        const StageTileGrid grid = StageGenerator::generate(stage);
        for (XZ spawn : kEnemySpawnPoints)
            everyEnemySpawnOpen = everyEnemySpawnOpen &&
                                  spawnAreaIsOpen(
                                      grid, spawn,
                                      0.0f);
        for (XZ spawn : kPlayerSpawnPoints)
            everyPlayerSpawnOpen = everyPlayerSpawnOpen &&
                                   spawnAreaIsOpen(
                                       grid, spawn,
                                       0.0f);
        for (int row = 23; row <= 25; ++row)
            for (int column = 11; column <= 14; ++column)
                if (row == 23 || column == 11 || column == 14)
                    everyOriginalEnclosureIntact = everyOriginalEnclosureIntact &&
                        grid[row][column] == '#';
    }
    expect(everyEnemySpawnOpen,
           "an enemy spawn clearance contains generated terrain");
    expect(everyPlayerSpawnOpen,
           "a player spawn clearance contains generated terrain");
    expect(everyOriginalEnclosureIntact,
           "an original base-enclosure cell was erased or moved");

    reporter.beginSuite("stage-generator-layout-diversity");
    // Exact tile hashes alone allowed the old fixed street grid to pass while
    // only shuffling materials. Compare the geometry after discarding terrain
    // identities, and separately check actual tank-blocking footprints.
    std::array<TerrainMask, kStageCount - 1> occupiedMasks{};
    std::array<TerrainMask, kStageCount - 1> blockingMasks{};
    for (int stage = 2; stage <= kStageCount; ++stage)
    {
        const StageTileGrid grid = StageGenerator::generate(stage);
        const std::size_t index = static_cast<std::size_t>(stage - 2);
        occupiedMasks[index] = terrainMask(grid, false);
        blockingMasks[index] = terrainMask(grid, true);
    }
    bool everyFootprintUnique = true;
    bool everyBlockingFootprintUnique = true;
    for (std::size_t first = 0; first < occupiedMasks.size(); ++first)
    {
        for (std::size_t second = first + 1;
             second < occupiedMasks.size(); ++second)
        {
            everyFootprintUnique = everyFootprintUnique &&
                occupiedMasks[first] != occupiedMasks[second];
            everyBlockingFootprintUnique = everyBlockingFootprintUnique &&
                blockingMasks[first] != blockingMasks[second];

        }
    }
    expect(everyFootprintUnique,
           "two original stages share the same terrain footprint");
    expect(everyBlockingFootprintUnique,
           "two original stages share the same tank-blocking footprint");

    reporter.finish();
    return passed ? 0 : 1;
}
