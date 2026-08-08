#include "game/stage_generator.h"

#include "stage_layout_expectations.h"
#include "test_support.h"

#include <algorithm>
#include <array>
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
            if (grid[static_cast<std::size_t>(row)]
                    [static_cast<std::size_t>(column)] != '.')
                return false;
    return true;
}

bool spawnAreaIsOpen(const StageTileGrid &grid, XZ center)
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
    return rectangleIsOpen(grid, top, left, bottom, right);
}

bool stageMarkerMatches(const StageTileGrid &grid, int stage)
{
    for (int bit = 0; bit < 6; ++bit)
    {
        const int column = 2 + bit * 4;
        const char expected = (stage & (1 << bit)) != 0 ? '%' : '-';
        if (grid[12][static_cast<std::size_t>(column)] != expected)
            return false;
    }
    return true;
}

bool stageTwoFixturesMatch(const StageTileGrid &grid)
{
    for (int row = 2; row <= 3; ++row)
        if (grid[static_cast<std::size_t>(row)][19] != '@' ||
            grid[static_cast<std::size_t>(row)][22] != '@')
            return false;
    if (grid[4][20] != '@' || grid[4][21] != '@' ||
        grid[5][12] != '@' || grid[5][13] != '@' ||
        grid[3][14] != '@' || grid[4][14] != '@')
        return false;
    return rectangleIsOpen(grid, 3, 10, 4, 11);
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
    bool everyBaseFootprintOpen = true;
    bool everyBaseApproachOpen = true;
    bool everyStageMarkerCorrect = true;
    for (int stage = 1; stage <= kStageCount; ++stage)
    {
        const StageTileGrid grid = StageGenerator::generate(stage);
        for (XZ spawn : kEnemySpawnPoints)
            everyEnemySpawnOpen = everyEnemySpawnOpen &&
                                  spawnAreaIsOpen(grid, spawn);
        for (XZ spawn : kPlayerSpawnPoints)
            everyPlayerSpawnOpen = everyPlayerSpawnOpen &&
                                   spawnAreaIsOpen(grid, spawn);
        everyBaseFootprintOpen = everyBaseFootprintOpen &&
                                 rectangleIsOpen(grid, 21, 10, 25, 15);
        everyBaseApproachOpen = everyBaseApproachOpen &&
                                rectangleIsOpen(grid, 18, 12, 22, 13);
        everyStageMarkerCorrect = everyStageMarkerCorrect &&
                                  stageMarkerMatches(grid, stage);
    }
    expect(everyEnemySpawnOpen,
           "an enemy spawn clearance contains generated terrain");
    expect(everyPlayerSpawnOpen,
           "a player spawn clearance contains generated terrain");
    expect(everyBaseFootprintOpen,
           "the government footprint contains a legacy terrain tile");
    expect(everyBaseApproachOpen,
           "the northern government approach is no longer open");
    expect(everyStageMarkerCorrect,
           "a generated stage lost its six-bit identity marker");
    expect(stageTwoFixturesMatch(StageGenerator::generate(2)),
           "stage 2 steering fixtures changed or sealed their bypass");

    reporter.finish();
    return passed ? 0 : 1;
}
