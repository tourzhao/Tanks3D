#include "game/stage_map.h"

#include "stage_map_expectations.h"
#include "test_support.h"

#include <array>
#include <cmath>
#include <filesystem>
#include <limits>
#include <string>

namespace
{
using tanks3d::core::CardinalDirection;
using tanks3d::core::Nation;
using tanks3d::core::XZ;
using tanks3d::game::BrickDamage;
using tanks3d::game::GovernmentBaseTheme;
using tanks3d::game::ImpactKind;
using tanks3d::game::ShellImpactDetails;
using tanks3d::game::StageGenerator;
using tanks3d::game::StageMap;
using tanks3d::game::governmentWallIndexForCell;
using tanks3d::game::isGovernmentWallCell;
using tanks3d::game::governmentWallSegment;
using tanks3d::game::kEnemySpawnPoints;
using tanks3d::game::kGovernmentBaseCenter;
using tanks3d::game::kGovernmentCoreHalfSize;
using tanks3d::game::kGovernmentPowerShellDamage;
using tanks3d::game::kGovernmentSteelDuration;
using tanks3d::game::kGovernmentSteelFlashPeriod;
using tanks3d::game::kGovernmentWallCount;
using tanks3d::game::kGovernmentWallMaximumHealth;
using tanks3d::game::kMapSize;
using tanks3d::game::kPlayerSpawnPoints;
using tanks3d::game::kShellHalfSize;
using tanks3d::game::kStageCount;
using tanks3d::game::kTankRadius;

struct Cell
{
    int row = -1;
    int column = -1;

    bool valid() const
    {
        return row >= 0 && column >= 0;
    }

    XZ center() const
    {
        return {column + 0.5f, row + 0.5f};
    }
};

bool nearlyEqual(float first, float second, float tolerance = 0.00001f)
{
    return std::fabs(first - second) <= tolerance;
}

bool samePosition(XZ first, XZ second, float tolerance = 0.00001f)
{
    return nearlyEqual(first.x, second.x, tolerance) &&
           nearlyEqual(first.z, second.z, tolerance);
}

Cell findTile(const StageMap &map, char requested)
{
    for (int row = 0; row < kMapSize; ++row)
        for (int column = 0; column < kMapSize; ++column)
            if (map.tile(row, column) == requested)
                return {row, column};
    return {};
}

Cell findHorizontalBrickPair(const StageMap &map)
{
    for (int row = 0; row < kMapSize; ++row)
        for (int column = 0; column + 1 < kMapSize; ++column)
            if (map.tile(row, column) == '#' &&
                map.tile(row, column + 1) == '#')
                return {row, column};
    return {};
}

Cell findBrickWithOpenLeft(const StageMap &map)
{
    for (int row = 0; row < kMapSize; ++row)
        for (int column = 1; column < kMapSize; ++column)
            if (map.tile(row, column) == '#' &&
                map.tile(row, column - 1) == '.')
                return {row, column};
    return {};
}

bool allSpawnFootprintsAreOpen(const StageMap &map)
{
    for (XZ spawn : kEnemySpawnPoints)
        if (map.collidesWithTank(spawn, kTankRadius))
            return false;
    for (XZ spawn : kPlayerSpawnPoints)
        if (map.collidesWithTank(spawn, kTankRadius))
            return false;
    return true;
}

bool sameDamage(const BrickDamage &damage, int row, int column,
                unsigned char beforeMask, unsigned char afterMask)
{
    return damage.row == row && damage.column == column &&
           damage.beforeMask == beforeMask &&
           damage.afterMask == afterMask;
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
    const std::filesystem::path noResourceDependency;

    reporter.beginSuite("stage-map-original-layouts-and-spawns");
    for (int stage = 1; stage <= kStageCount; ++stage)
    {
        StageMap map;
        std::string error = "not cleared";
        const bool loaded = map.load(noResourceDependency, stage, error);
        const auto generatedTiles = StageGenerator::generate(stage);
        bool matchesGeneratedInitialState = loaded;
        for (int row = 0; row < kMapSize; ++row)
        {
            for (int column = 0; column < kMapSize; ++column)
            {
                const char generated =
                    generatedTiles[static_cast<std::size_t>(row)]
                                  [static_cast<std::size_t>(column)];
                matchesGeneratedInitialState =
                    matchesGeneratedInitialState &&
                    map.tile(row, column) == generated &&
                    map.brickMask(row, column) ==
                        static_cast<unsigned char>(generated == '#'
                                                       ? 0x0fU
                                                       : 0U) &&
                    map.brickHitCount(row, column) == 0U &&
                    map.brickFirstDirection(row, column) ==
                        CardinalDirection::None;
            }
        }
        expect(loaded && error.empty() && map.stage() == stage &&
                   matchesGeneratedInitialState,
               "stage " + std::to_string(stage) +
                   " failed to consume the generated initial tile grid");
        expect(loaded && allSpawnFootprintsAreOpen(map),
               "stage " + std::to_string(stage) +
                   " lost an original spawn footprint");
    }
    expect(tanks3d::game::normalizedStage(1) == 1 &&
               tanks3d::game::normalizedStage(35) == 35 &&
               tanks3d::game::normalizedStage(36) == 1 &&
               tanks3d::game::normalizedStage(0) == 35 &&
               tanks3d::game::normalizedStage(-1) == 34 &&
               tanks3d::game::normalizedStage(-34) == 1 &&
               tanks3d::game::normalizedStage(-35) == 35 &&
               tanks3d::game::normalizedStage(
                   std::numeric_limits<int>::max()) == 22 &&
               tanks3d::game::normalizedStage(
                   std::numeric_limits<int>::min()) == 12,
           "stage wrapping changed at a positive or negative boundary");

    reporter.beginSuite("stage-map-terrain-and-boundaries");
    StageMap terrainMap;
    std::string terrainError;
    const bool terrainLoaded = tanks3d_test::loadTerrainFixture(terrainMap, terrainError);
    const Cell openCell = findTile(terrainMap, '.');
    const Cell brickCell = findTile(terrainMap, '#');
    const Cell steelCell = findTile(terrainMap, '@');
    const Cell waterCell = findTile(terrainMap, '~');
    const Cell forestCell = findTile(terrainMap, '%');
    const Cell iceCell = findTile(terrainMap, '-');
    expect(terrainLoaded && terrainError.empty() && openCell.valid() &&
               brickCell.valid() && steelCell.valid() && waterCell.valid() &&
               forestCell.valid() && iceCell.valid(),
           "terrain fixture stage did not expose every terrain family");

    constexpr float probeHalfExtent = 0.10f;
    expect(!terrainMap.collidesWithTank(openCell.center(), probeHalfExtent),
           "open ground became solid");
    expect(terrainMap.collidesWithTank(brickCell.center(), probeHalfExtent),
           "an intact brick stopped blocking tanks");
    expect(terrainMap.collidesWithTank(steelCell.center(), probeHalfExtent),
           "ordinary steel stopped blocking tanks");
    expect(terrainMap.collidesWithTank(waterCell.center(), probeHalfExtent) &&
               !terrainMap.collidesWithTank(waterCell.center(),
                                            probeHalfExtent, true),
           "Boat-style water permission no longer changes tank collision");
    expect(!terrainMap.collidesWithTank(forestCell.center(), probeHalfExtent),
           "forest cover became physically solid");
    expect(!terrainMap.collidesWithTank(iceCell.center(), probeHalfExtent),
           "ice became physically solid");
    expect(terrainMap.isIce(iceCell.center()) &&
               !terrainMap.isIce(openCell.center()) &&
               !terrainMap.isIce(waterCell.center()) &&
               !terrainMap.isIce(forestCell.center()),
           "ice query accepted a different terrain family");
    expect(terrainMap.wallOccupies(brickCell.center()) &&
               terrainMap.wallOccupies(steelCell.center()) &&
               !terrainMap.wallOccupies(openCell.center()) &&
               !terrainMap.wallOccupies(waterCell.center()) &&
               !terrainMap.wallOccupies(forestCell.center()) &&
               !terrainMap.wallOccupies(iceCell.center()),
           "point-wall occupancy no longer distinguishes cover and solids");

    StageMap normalSteelMap;
    std::string normalSteelError;
    const bool normalSteelLoaded = tanks3d_test::loadTerrainFixture(normalSteelMap, normalSteelError);
    expect(normalSteelLoaded &&
               normalSteelMap.impactShell(steelCell.center(), false,
                                          CardinalDirection::North) ==
                   ImpactKind::Steel &&
               normalSteelMap.tile(steelCell.row, steelCell.column) == '@',
           "a normal shell no longer stops at intact ordinary steel");

    StageMap powerSteelMap;
    std::string powerSteelError;
    const bool powerSteelLoaded = tanks3d_test::loadTerrainFixture(powerSteelMap, powerSteelError);
    expect(powerSteelLoaded &&
               powerSteelMap.impactShell(steelCell.center(), true,
                                         CardinalDirection::North) ==
                   ImpactKind::Steel &&
               powerSteelMap.tile(steelCell.row, steelCell.column) == '.',
           "a power shell no longer clears ordinary steel");

    StageMap forestMap;
    std::string forestError;
    const bool forestLoaded = tanks3d_test::loadTerrainFixture(forestMap, forestError);
    const ImpactKind normalForest = forestMap.impactShell(
        forestCell.center(), false, CardinalDirection::North);
    const bool forestSurvived =
        forestMap.tile(forestCell.row, forestCell.column) == '%';
    const ImpactKind powerForest = forestMap.impactShell(
        forestCell.center(), true, CardinalDirection::North);
    expect(forestLoaded && normalForest == ImpactKind::None &&
               forestSurvived && powerForest == ImpactKind::None &&
               forestMap.tile(forestCell.row, forestCell.column) == '.',
           "normal and power shells no longer treat forest differently");

    StageMap waterShellMap;
    std::string waterShellError;
    const bool waterShellLoaded = tanks3d_test::loadTerrainFixture(waterShellMap, waterShellError);
    expect(waterShellLoaded &&
               waterShellMap.impactShell(waterCell.center(), false,
                                         CardinalDirection::East) ==
                   ImpactKind::None &&
               waterShellMap.impactShell(waterCell.center(), true,
                                         CardinalDirection::West) ==
                   ImpactKind::None &&
               waterShellMap.tile(waterCell.row, waterCell.column) == '~',
           "a shell began stopping at or mutating water");

    StageMap iceShellMap;
    std::string iceShellError;
    const bool iceShellLoaded = tanks3d_test::loadTerrainFixture(iceShellMap, iceShellError);
    expect(iceShellLoaded &&
               iceShellMap.impactShell(iceCell.center(), false,
                                       CardinalDirection::South) ==
                   ImpactKind::None &&
               iceShellMap.impactShell(iceCell.center(), true,
                                       CardinalDirection::North) ==
                   ImpactKind::None &&
               iceShellMap.tile(iceCell.row, iceCell.column) == '-',
           "a shell began stopping at or mutating ice");

    StageMap boundaryMap;
    boundaryMap.prepareShowcaseArena();
    constexpr float boundaryEpsilon = 0.0001f;
    expect(!boundaryMap.collidesWithTank({kTankRadius, 10.0f}, kTankRadius) &&
               boundaryMap.collidesWithTank(
                   {kTankRadius - boundaryEpsilon, 10.0f}, kTankRadius),
           "left tank boundary lost its exact-inside rule");
    expect(!boundaryMap.collidesWithTank(
               {kMapSize - kTankRadius, 10.0f}, kTankRadius) &&
               boundaryMap.collidesWithTank(
                   {kMapSize - kTankRadius + boundaryEpsilon, 10.0f},
                   kTankRadius),
           "right tank boundary lost its exact-inside rule");
    expect(!boundaryMap.collidesWithTank({10.0f, kTankRadius}, kTankRadius) &&
               boundaryMap.collidesWithTank(
                   {10.0f, kTankRadius - boundaryEpsilon}, kTankRadius),
           "top tank boundary lost its exact-inside rule");
    expect(!boundaryMap.collidesWithTank(
               {5.0f, kMapSize - kTankRadius}, kTankRadius) &&
               boundaryMap.collidesWithTank(
                   {5.0f, kMapSize - kTankRadius + boundaryEpsilon},
                   kTankRadius),
           "bottom tank boundary lost its exact-inside rule");

    struct OutOfBoundsCase
    {
        int row;
        int column;
        XZ position;
    };
    const std::array<OutOfBoundsCase, 4> outOfBoundsCases{{
        {-1, 0, {0.5f, -0.5f}},
        {kMapSize, 0, {0.5f, kMapSize + 0.5f}},
        {0, -1, {-0.5f, 0.5f}},
        {0, kMapSize, {kMapSize + 0.5f, 0.5f}}}};
    for (const OutOfBoundsCase &testCase : outOfBoundsCases)
    {
        expect(terrainMap.tile(testCase.row, testCase.column) == '@' &&
                   terrainMap.brickMask(testCase.row, testCase.column) == 0U &&
                   terrainMap.brickHitCount(testCase.row,
                                            testCase.column) == 0U &&
                   terrainMap.brickFirstDirection(testCase.row,
                                                  testCase.column) ==
                       CardinalDirection::None &&
                   terrainMap.wallOccupies(testCase.position),
               "an out-of-bounds map query lost its safe default");
    }

    expect(!boundaryMap.hasTankRoute({0.0f, 1.0f}, {1.0f, 1.0f}),
           "navigation accepted a tank center on the left edge");
    expect(!boundaryMap.hasTankRoute({1.0f, 1.0f},
                                     {static_cast<float>(kMapSize), 1.0f}),
           "navigation accepted a tank center beyond the right edge");
    expect(!terrainMap.hasTankRoute(brickCell.center(),
                                    kPlayerSpawnPoints[0]),
           "navigation accepted a brick-blocked endpoint");
    expect(!terrainMap.hasTankRoute(kEnemySpawnPoints[1],
                                    waterCell.center()),
           "navigation accepted a water-blocked endpoint without a Boat");

    StageMap routeMap;
    tanks3d::game::StageTileGrid routeGrid{};
    for (auto &row : routeGrid)
        row.fill('.');
    routeGrid[12].fill('#');
    routeGrid[13].fill('#');
    tanks3d::game::StageMapTestAccess::install(routeMap, routeGrid);
    expect(!routeMap.hasTankRoute({1.0f, 1.0f}, {1.0f, 25.0f}),
           "route query ignored a continuous destructible barrier");
    for (int row = 12; row <= 13; ++row)
        for (int column = 0; column <= 1; ++column)
            routeMap.impactShell({column + 0.5f, row + 0.5f}, true,
                                 CardinalDirection::South);
    expect(routeMap.hasTankRoute({1.0f, 1.0f}, {1.0f, 25.0f}),
           "route query did not discover a real two-cell breach");

    StageMap showcaseMap;
    std::string showcaseError;
    const bool showcaseLoaded = tanks3d_test::loadTerrainFixture(showcaseMap, showcaseError);
    showcaseMap.impactShell(brickCell.center(), false,
                            CardinalDirection::North);
    showcaseMap.activateGovernmentSteel();
    showcaseMap.prepareShowcaseArena();
    bool showcaseCleared = showcaseLoaded;
    for (int row = 0; row < kMapSize; ++row)
        for (int column = 0; column < kMapSize; ++column)
            showcaseCleared = showcaseCleared &&
                              showcaseMap.tile(row, column) ==
                                  (isGovernmentWallCell(row, column) ? '#' : '.') &&
                              showcaseMap.brickMask(row, column) ==
                                  (isGovernmentWallCell(row, column) ? 0x0fU : 0U) &&
                              showcaseMap.brickHitCount(row, column) == 0U &&
                              showcaseMap.brickFirstDirection(row, column) ==
                                  CardinalDirection::None;
    for (int index = 0; index < kGovernmentWallCount; ++index)
        showcaseCleared = showcaseCleared &&
                          showcaseMap.governmentWallHealth(index) ==
                              kGovernmentWallMaximumHealth;
    expect(showcaseCleared && !showcaseMap.governmentWallsSteel() &&
               nearlyEqual(showcaseMap.governmentSteelTimeRemaining(), 0.0f),
           "showcase arena retained terrain damage or shovel state");

    reporter.beginSuite("stage-map-brick-damage");
    const std::array<CardinalDirection, 4> firstDirections{{
        CardinalDirection::North, CardinalDirection::East,
        CardinalDirection::South, CardinalDirection::West}};
    const std::array<unsigned char, 4> firstMasks{{
        0x03U, 0x0aU, 0x0cU, 0x05U}};
    const std::array<CardinalDirection, 4> crossingDirections{{
        CardinalDirection::East, CardinalDirection::South,
        CardinalDirection::West, CardinalDirection::North}};
    const std::array<unsigned char, 4> crossingMasks{{
        0x02U, 0x08U, 0x04U, 0x01U}};

    for (std::size_t index = 0; index < firstDirections.size(); ++index)
    {
        StageMap map;
        std::string error;
        const bool loaded = tanks3d_test::loadTerrainFixture(map, error);
        ShellImpactDetails details;
        const ImpactKind impact = map.impactShell(
            brickCell.center(), false, firstDirections[index], &details);
        expect(loaded && impact == ImpactKind::Brick &&
                   map.tile(brickCell.row, brickCell.column) == '#' &&
                   map.brickMask(brickCell.row, brickCell.column) ==
                       firstMasks[index] &&
                   map.brickHitCount(brickCell.row, brickCell.column) == 1U &&
                   map.brickFirstDirection(brickCell.row,
                                           brickCell.column) ==
                       firstDirections[index] &&
                   details.brickCount == 1 &&
                   sameDamage(details.bricks[0], brickCell.row,
                              brickCell.column, 0x0fU, firstMasks[index]) &&
                   !details.destroyedBrick(),
               "first directional brick hit changed its mask or payload");
    }

    for (std::size_t index = 0; index < firstDirections.size(); ++index)
    {
        StageMap map;
        std::string error;
        const bool loaded = tanks3d_test::loadTerrainFixture(map, error);
        map.impactShell(brickCell.center(), false, firstDirections[index]);
        ShellImpactDetails details;
        const ImpactKind impact = map.impactShell(
            brickCell.center(), false, crossingDirections[index], &details);
        expect(loaded && impact == ImpactKind::Brick &&
                   map.brickMask(brickCell.row, brickCell.column) ==
                       crossingMasks[index] &&
                   map.brickHitCount(brickCell.row, brickCell.column) == 2U &&
                   map.brickFirstDirection(brickCell.row,
                                           brickCell.column) ==
                       firstDirections[index] &&
                   details.brickCount == 1 &&
                   sameDamage(details.bricks[0], brickCell.row,
                              brickCell.column, firstMasks[index],
                              crossingMasks[index]) &&
                   !details.destroyedBrick(),
               "perpendicular second brick hit changed its quarter payload");
    }

    StageMap sameAxisMap;
    std::string sameAxisError;
    const bool sameAxisLoaded = tanks3d_test::loadTerrainFixture(sameAxisMap, sameAxisError);
    sameAxisMap.impactShell(brickCell.center(), false,
                            CardinalDirection::North);
    ShellImpactDetails sameAxisDetails;
    const ImpactKind sameAxisImpact = sameAxisMap.impactShell(
        brickCell.center(), false, CardinalDirection::South,
        &sameAxisDetails);
    expect(sameAxisLoaded && sameAxisImpact == ImpactKind::Brick &&
               sameAxisMap.tile(brickCell.row, brickCell.column) == '.' &&
               sameAxisMap.brickMask(brickCell.row, brickCell.column) == 0U &&
               sameAxisMap.brickHitCount(brickCell.row,
                                         brickCell.column) == 0U &&
               sameAxisMap.brickFirstDirection(brickCell.row,
                                               brickCell.column) ==
                   CardinalDirection::None &&
               sameAxisDetails.brickCount == 1 &&
               sameDamage(sameAxisDetails.bricks[0], brickCell.row,
                          brickCell.column, 0x03U, 0x00U) &&
               sameAxisDetails.destroyedBrick(),
           "same-axis second hit did not clear and reset a brick");

    StageMap thirdHitMap;
    std::string thirdHitError;
    const bool thirdHitLoaded = tanks3d_test::loadTerrainFixture(thirdHitMap, thirdHitError);
    thirdHitMap.impactShell(brickCell.center(), false,
                           CardinalDirection::North);
    thirdHitMap.impactShell(brickCell.center(), false,
                           CardinalDirection::East);
    ShellImpactDetails thirdHitDetails;
    const ImpactKind thirdHitImpact = thirdHitMap.impactShell(
        brickCell.center(), false, CardinalDirection::West,
        &thirdHitDetails);
    expect(thirdHitLoaded && thirdHitImpact == ImpactKind::Brick &&
               thirdHitMap.tile(brickCell.row, brickCell.column) == '.' &&
               thirdHitMap.brickHitCount(brickCell.row,
                                         brickCell.column) == 0U &&
               sameDamage(thirdHitDetails.bricks[0], brickCell.row,
                          brickCell.column, 0x02U, 0x00U) &&
               thirdHitDetails.destroyedBrick(),
           "third normal hit did not clear a surviving brick quarter");

    StageMap seamMap;
    std::string seamError;
    const bool seamLoaded = tanks3d_test::loadTerrainFixture(seamMap, seamError);
    const Cell seamCell = findHorizontalBrickPair(seamMap);
    ShellImpactDetails seamDetails;
    const ImpactKind seamImpact = seamMap.impactShell(
        {seamCell.column + 1.0f, seamCell.row + 0.5f}, false,
        CardinalDirection::North, &seamDetails);
    expect(seamLoaded && seamCell.valid() && seamImpact == ImpactKind::Brick &&
               seamDetails.brickCount == 2 &&
               sameDamage(seamDetails.bricks[0], seamCell.row,
                          seamCell.column, 0x0fU, 0x03U) &&
               sameDamage(seamDetails.bricks[1], seamCell.row,
                          seamCell.column + 1, 0x0fU, 0x03U) &&
               !seamDetails.destroyedBrick(),
           "normal seam hit lost its two row-major BrickDamage payloads");

    StageMap powerSeamMap;
    std::string powerSeamError;
    const bool powerSeamLoaded = tanks3d_test::loadTerrainFixture(powerSeamMap, powerSeamError);
    ShellImpactDetails powerSeamDetails;
    const ImpactKind powerSeamImpact = powerSeamMap.impactShell(
        {seamCell.column + 1.0f, seamCell.row + 0.5f}, true,
        CardinalDirection::North, &powerSeamDetails);
    expect(powerSeamLoaded && powerSeamImpact == ImpactKind::Brick &&
               powerSeamDetails.brickCount == 2 &&
               sameDamage(powerSeamDetails.bricks[0], seamCell.row,
                          seamCell.column, 0x0fU, 0x00U) &&
               sameDamage(powerSeamDetails.bricks[1], seamCell.row,
                          seamCell.column + 1, 0x0fU, 0x00U) &&
               powerSeamMap.tile(seamCell.row, seamCell.column) == '.' &&
               powerSeamMap.tile(seamCell.row, seamCell.column + 1) == '.' &&
               powerSeamDetails.destroyedBrick(),
           "power seam hit lost a cleared-brick payload");

    StageMap partialMap;
    std::string partialError;
    const bool partialLoaded = tanks3d_test::loadTerrainFixture(partialMap, partialError);
    partialMap.impactShell(brickCell.center(), false,
                           CardinalDirection::North);
    const XZ survivingHalf{brickCell.column + 0.5f,
                           brickCell.row + 0.25f};
    const XZ removedHalf{brickCell.column + 0.5f,
                         brickCell.row + 0.75f};
    expect(partialLoaded && partialMap.wallOccupies(survivingHalf) &&
               partialMap.collidesWithTank(survivingHalf, 0.10f) &&
               !partialMap.wallOccupies(removedHalf) &&
               !partialMap.collidesWithTank(removedHalf, 0.10f),
           "partial brick collision did not match its surviving quadrants");

    ShellImpactDetails resetDetails;
    resetDetails.brickCount = 2;
    resetDetails.governmentWallIndex = 4;
    resetDetails.governmentWallHealthBefore = 4;
    resetDetails.governmentWallHealthAfter = 1;
    resetDetails.bricks[0] = {7, 8, 0x0fU, 0x01U};
    const ImpactKind emptyImpact = terrainMap.impactShell(
        openCell.center(), false, CardinalDirection::North, &resetDetails);
    expect(emptyImpact == ImpactKind::None && resetDetails.brickCount == 0 &&
               resetDetails.governmentWallIndex == -1 &&
               resetDetails.governmentWallHealthBefore == 0 &&
               resetDetails.governmentWallHealthAfter == 0 &&
               sameDamage(resetDetails.bricks[0], -1, -1, 0U, 0U),
           "an empty impact retained a previous payload");

    resetDetails.brickCount = 1;
    resetDetails.governmentWallIndex = 2;
    const ImpactKind directionlessImpact = terrainMap.impactShell(
        openCell.center(), false, CardinalDirection::None, &resetDetails);
    expect(directionlessImpact == ImpactKind::Boundary &&
               resetDetails.brickCount == 0 &&
               resetDetails.governmentWallIndex == -1,
           "directionless shell did not reset details and stop at a boundary");

    StageMap exactTouchMap;
    std::string exactTouchError;
    const bool exactTouchLoaded = tanks3d_test::loadTerrainFixture(exactTouchMap, exactTouchError);
    const Cell exactTouchCell = findBrickWithOpenLeft(exactTouchMap);
    const ImpactKind exactTouchImpact = exactTouchMap.impactShell(
        {exactTouchCell.column - kShellHalfSize,
         exactTouchCell.row + 0.5f},
        false, CardinalDirection::North);
    expect(exactTouchLoaded && exactTouchCell.valid() &&
               exactTouchImpact == ImpactKind::None &&
               exactTouchMap.brickMask(exactTouchCell.row,
                                       exactTouchCell.column) == 0x0fU,
           "zero-area shell contact damaged a brick");

    StageMap nearTouchMap;
    std::string nearTouchError;
    const bool nearTouchLoaded = tanks3d_test::loadTerrainFixture(nearTouchMap, nearTouchError);
    const ImpactKind nearTouchImpact = nearTouchMap.impactShell(
        {exactTouchCell.column - kShellHalfSize + 0.001f,
         exactTouchCell.row + 0.5f},
        false, CardinalDirection::North);
    expect(nearTouchLoaded && nearTouchImpact == ImpactKind::Brick &&
               nearTouchMap.brickMask(exactTouchCell.row,
                                      exactTouchCell.column) == 0x03U,
           "positive-area grazing shell contact missed a brick");

    StageMap shellBoundaryMap;
    shellBoundaryMap.prepareShowcaseArena();
    expect(shellBoundaryMap.impactShell(
               {10.0f, kShellHalfSize}, false,
               CardinalDirection::North) == ImpactKind::None &&
               shellBoundaryMap.impactShell(
                   {10.0f, kShellHalfSize - boundaryEpsilon}, false,
                   CardinalDirection::North) == ImpactKind::Boundary,
           "north shell boundary lost its exact-inside rule");
    expect(shellBoundaryMap.impactShell(
               {10.0f, kMapSize - kShellHalfSize}, false,
               CardinalDirection::South) == ImpactKind::None &&
               shellBoundaryMap.impactShell(
                   {10.0f, kMapSize - kShellHalfSize + boundaryEpsilon},
                   false, CardinalDirection::South) == ImpactKind::Boundary,
           "south shell boundary lost its exact-inside rule");
    expect(shellBoundaryMap.impactShell(
               {kShellHalfSize, 10.0f}, false,
               CardinalDirection::West) == ImpactKind::None &&
               shellBoundaryMap.impactShell(
                   {kShellHalfSize - boundaryEpsilon, 10.0f}, false,
                   CardinalDirection::West) == ImpactKind::Boundary,
           "west shell boundary lost its exact-inside rule");
    expect(shellBoundaryMap.impactShell(
               {kMapSize - kShellHalfSize, 10.0f}, false,
               CardinalDirection::East) == ImpactKind::None &&
               shellBoundaryMap.impactShell(
                   {kMapSize - kShellHalfSize + boundaryEpsilon, 10.0f},
                   false, CardinalDirection::East) == ImpactKind::Boundary,
           "east shell boundary lost its exact-inside rule");

    reporter.beginSuite("stage-map-government-and-shovel");
    StageMap nationMap;
    nationMap.setGovernmentNation(Nation::SovietUnion);
    std::string nationError;
    const bool nationLoaded = nationMap.load(
        noResourceDependency, 1, nationError);
    const bool sovietTheme =
        nationMap.governmentNation() == Nation::SovietUnion &&
        nationMap.governmentBaseTheme() ==
            GovernmentBaseTheme::SovietRingCastle;
    nationMap.setGovernmentNation(Nation::Germany);
    const bool germanTheme =
        nationMap.governmentBaseTheme() ==
        GovernmentBaseTheme::GermanParliament;
    nationMap.setGovernmentNation(Nation::Count);
    expect(nationLoaded && sovietTheme && germanTheme &&
               nationMap.governmentNation() == Nation::UnitedStates &&
               nationMap.governmentBaseTheme() ==
                   GovernmentBaseTheme::UnitedStatesPentagon,
           "stage map lost national government-theme normalization");

    StageMap geometryMap;
    std::string geometryError;
    const bool geometryLoaded = geometryMap.load(
        noResourceDependency, 1, geometryError);
    static constexpr std::array<Cell, 8> expectedWallCells{{
        {23, 11}, {23, 12}, {23, 13}, {23, 14},
        {24, 11}, {24, 14}, {25, 11}, {25, 14}}};
    bool geometryValid = geometryLoaded && kGovernmentWallCount == 8 &&
                         samePosition(kGovernmentBaseCenter, {13.0f, 25.0f});
    for (int index = 0; index < kGovernmentWallCount; ++index)
    {
        const Cell cell = expectedWallCells[static_cast<std::size_t>(index)];
        const auto segment = governmentWallSegment(index);
        geometryValid = geometryValid &&
                        samePosition(segment.center, cell.center()) &&
                        nearlyEqual(segment.length, 1.0f) &&
                        nearlyEqual(segment.halfLength, 0.5f) &&
                        nearlyEqual(segment.halfThickness, 0.5f) &&
                        nearlyEqual(tanks3d::core::lengthSquared(
                                        segment.along), 1.0f) &&
                        nearlyEqual(tanks3d::core::lengthSquared(
                                        segment.outward), 1.0f) &&
                        governmentWallIndexForCell(cell.row, cell.column) == index &&
                        geometryMap.governmentWallHealth(index) ==
                            kGovernmentWallMaximumHealth &&
                        geometryMap.wallOccupies(segment.center) &&
                        geometryMap.collidesWithTank(segment.center, 0.05f);
    }
    for (int row = 0; row < kMapSize; ++row)
        for (int column = 0; column < kMapSize; ++column)
        {
            const bool expected = (row == 23 && column >= 11 && column <= 14) ||
                ((row == 24 || row == 25) && (column == 11 || column == 14));
            geometryValid = geometryValid &&
                isGovernmentWallCell(row, column) == expected;
        }
    geometryValid = geometryValid && geometryMap.wallOccupies({12.0f, 23.5f}) &&
        geometryMap.wallOccupies({11.5f, 24.0f});
    expect(geometryValid,
           "government wall geometry, health, or shared collision drifted");

    StageMap normalGovernmentMap;
    std::string normalGovernmentError;
    const bool normalGovernmentLoaded = normalGovernmentMap.load(
        noResourceDependency, 1, normalGovernmentError);
    const auto testedWall = governmentWallSegment(0);
    for (int hit = 0; hit < kGovernmentWallMaximumHealth; ++hit)
    {
        ShellImpactDetails details;
        const ImpactKind impact = normalGovernmentMap.impactShell(
            testedWall.center, false, CardinalDirection::North, &details);
        const int expectedBefore = kGovernmentWallMaximumHealth - hit;
        const int expectedAfter = expectedBefore - 1;
        expect(normalGovernmentLoaded &&
                   impact == ImpactKind::GovernmentWall &&
                   details.governmentWallIndex == 0 &&
                   details.governmentWallHealthBefore == expectedBefore &&
                   details.governmentWallHealthAfter == expectedAfter &&
                   details.destroyedGovernmentWall() ==
                       (expectedAfter == 0),
               "normal shell changed government wall damage or payload");
    }

    StageMap breachMap;
    std::string breachError;
    const bool breachLoaded = breachMap.load(
        noResourceDependency, 1, breachError);
    // Cross the outer face; after destruction both points are open and do
    // not enter a neighboring wall or the core.
    const float breachProbeDistance = testedWall.halfThickness + 0.02f;
    const XZ outsideWall = testedWall.center +
                           testedWall.outward * breachProbeDistance;
    const XZ insideWall = testedWall.center;
    const bool separatedBefore = breachMap.solidSeparatesShells(
        outsideWall, insideWall);
    for (int hit = 0; hit < kGovernmentWallMaximumHealth; ++hit)
        breachMap.impactShell(testedWall.center, false,
                              CardinalDirection::North);
    expect(breachLoaded && separatedBefore &&
               breachMap.governmentWallHealth(0) == 0 &&
               !breachMap.wallOccupies(testedWall.center) &&
               !breachMap.collidesWithTank(testedWall.center, 0.05f) &&
               !breachMap.solidSeparatesShells(outsideWall, insideWall),
           "destroyed government wall did not open a physical breach");

    StageMap narrowBreachMap;
    narrowBreachMap.prepareShowcaseArena();
    const auto firstTopWall = governmentWallSegment(1);
    const auto secondTopWall = governmentWallSegment(2);
    narrowBreachMap.impactShell(firstTopWall.center, true,
                                CardinalDirection::South);
    const bool partialHealthKeepsFullCell =
        narrowBreachMap.governmentWallHealth(1) == 2 &&
        narrowBreachMap.tile(23, 12) == '#' &&
        narrowBreachMap.brickMask(23, 12) == 0x0fU &&
        narrowBreachMap.collidesWithTank(firstTopWall.center, 0.1f);
    narrowBreachMap.impactShell(firstTopWall.center, true,
                                CardinalDirection::South);
    // The tank can enter a two-cell opening but must still stop at the core.
    const XZ tankAtOpening{13.0f, 23.0f};
    const bool oneCellBlocksTank = narrowBreachMap.collidesWithTank(
        tankAtOpening, kTankRadius);
    const bool firstCellGone = narrowBreachMap.tile(23, 12) == '.' &&
                              narrowBreachMap.brickMask(23, 12) == 0U;
    narrowBreachMap.impactShell(secondTopWall.center, true,
                                CardinalDirection::South);
    narrowBreachMap.impactShell(secondTopWall.center, true,
                                CardinalDirection::South);
    expect(partialHealthKeepsFullCell && firstCellGone && oneCellBlocksTank &&
               !narrowBreachMap.collidesWithTank(tankAtOpening, kTankRadius) &&
               narrowBreachMap.collidesWithTank({13.0f, 23.2f}, kTankRadius),
           "base cell damage, real tank-width breach, or core stop drifted");
    narrowBreachMap.repairGovernmentWalls();
    expect(narrowBreachMap.tile(23, 12) == '#' &&
               narrowBreachMap.brickMask(23, 12) == 0x0fU &&
               narrowBreachMap.tile(23, 13) == '#' &&
               narrowBreachMap.brickMask(23, 13) == 0x0fU &&
               narrowBreachMap.collidesWithTank(tankAtOpening, kTankRadius),
           "repair did not restore both the original cells and collision");

    StageMap powerGovernmentMap;
    std::string powerGovernmentError;
    const bool powerGovernmentLoaded = powerGovernmentMap.load(
        noResourceDependency, 1, powerGovernmentError);
    ShellImpactDetails firstPowerDetails;
    ShellImpactDetails secondPowerDetails;
    const ImpactKind firstPowerImpact = powerGovernmentMap.impactShell(
        testedWall.center, true, CardinalDirection::South,
        &firstPowerDetails);
    const ImpactKind secondPowerImpact = powerGovernmentMap.impactShell(
        testedWall.center, true, CardinalDirection::South,
        &secondPowerDetails);
    expect(powerGovernmentLoaded &&
               firstPowerImpact == ImpactKind::GovernmentWall &&
               secondPowerImpact == ImpactKind::GovernmentWall &&
               firstPowerDetails.governmentWallHealthBefore == 4 &&
               firstPowerDetails.governmentWallHealthAfter ==
                   4 - kGovernmentPowerShellDamage &&
               secondPowerDetails.governmentWallHealthBefore == 2 &&
               secondPowerDetails.governmentWallHealthAfter == 0 &&
               secondPowerDetails.destroyedGovernmentWall(),
           "power shell no longer removes two government-wall HP");

    StageMap cornerMap;
    std::string cornerError;
    const bool cornerLoaded = cornerMap.load(
        noResourceDependency, 1, cornerError);
    const ImpactKind cornerImpact = cornerMap.impactShell(
        XZ{12.0f, 23.5f}, false, CardinalDirection::North);
    int totalCornerHealth = 0;
    for (int index = 0; index < kGovernmentWallCount; ++index)
        totalCornerHealth += cornerMap.governmentWallHealth(index);
    expect(cornerLoaded && cornerImpact == ImpactKind::GovernmentWall &&
               totalCornerHealth ==
                   kGovernmentWallCount * kGovernmentWallMaximumHealth - 1,
           "one corner shell damaged more than one government wall");

    StageMap steelGovernmentMap;
    std::string steelGovernmentError;
    const bool steelGovernmentLoaded = steelGovernmentMap.load(
        noResourceDependency, 1, steelGovernmentError);
    for (int hit = 0; hit < kGovernmentWallMaximumHealth; ++hit)
        steelGovernmentMap.impactShell(testedWall.center, false,
                                       CardinalDirection::North);
    steelGovernmentMap.activateGovernmentSteel();
    bool allWallsRepaired = true;
    for (int index = 0; index < kGovernmentWallCount; ++index)
        allWallsRepaired = allWallsRepaired &&
                           steelGovernmentMap.governmentWallHealth(index) ==
                               kGovernmentWallMaximumHealth;
    ShellImpactDetails protectedNormalDetails;
    ShellImpactDetails protectedPowerDetails;
    const ImpactKind protectedNormal = steelGovernmentMap.impactShell(
        testedWall.center, false, CardinalDirection::North,
        &protectedNormalDetails);
    const ImpactKind protectedPower = steelGovernmentMap.impactShell(
        testedWall.center, true, CardinalDirection::South,
        &protectedPowerDetails);
    expect(steelGovernmentLoaded && allWallsRepaired &&
               steelGovernmentMap.governmentWallsSteel() &&
               steelGovernmentMap.governmentSteelVisible() &&
               nearlyEqual(steelGovernmentMap.governmentSteelTimeRemaining(),
                           kGovernmentSteelDuration) &&
               protectedNormal == ImpactKind::Steel &&
               protectedPower == ImpactKind::Steel &&
               protectedNormalDetails.governmentWallHealthBefore == 4 &&
               protectedNormalDetails.governmentWallHealthAfter == 4 &&
               protectedPowerDetails.governmentWallHealthBefore == 4 &&
               protectedPowerDetails.governmentWallHealthAfter == 4 &&
               steelGovernmentMap.governmentWallHealth(0) == 4,
           "shovel steel did not repair and protect every government wall");

    steelGovernmentMap.updateGovernmentProtection(-5.0f);
    const bool negativeTimeIgnored = nearlyEqual(
        steelGovernmentMap.governmentSteelTimeRemaining(),
        kGovernmentSteelDuration);
    steelGovernmentMap.updateGovernmentProtection(5.0f);
    const bool timeAdvanced = nearlyEqual(
        steelGovernmentMap.governmentSteelTimeRemaining(),
        kGovernmentSteelDuration - 5.0f);
    steelGovernmentMap.activateGovernmentSteel();
    expect(negativeTimeIgnored && timeAdvanced &&
               nearlyEqual(steelGovernmentMap.governmentSteelTimeRemaining(),
                           kGovernmentSteelDuration),
           "shovel timer accepted negative time or failed to reset");

    steelGovernmentMap.updateGovernmentProtection(17.10f);
    const bool firstWarningFrame =
        steelGovernmentMap.governmentSteelVisible();
    const ImpactKind warningImpact = steelGovernmentMap.impactShell(
        testedWall.center, true, CardinalDirection::North);
    steelGovernmentMap.updateGovernmentProtection(
        kGovernmentSteelFlashPeriod);
    const bool secondWarningFrame =
        steelGovernmentMap.governmentSteelVisible();
    expect(firstWarningFrame != secondWarningFrame &&
               warningImpact == ImpactKind::Steel &&
               steelGovernmentMap.governmentWallHealth(0) == 4,
           "final shovel warning stopped flashing or protecting walls");

    steelGovernmentMap.activateGovernmentSteel();
    steelGovernmentMap.updateGovernmentProtection(
        kGovernmentSteelDuration + 0.01f);
    const ImpactKind expiredImpact = steelGovernmentMap.impactShell(
        testedWall.center, false, CardinalDirection::North);
    expect(!steelGovernmentMap.governmentWallsSteel() &&
               !steelGovernmentMap.governmentSteelVisible() &&
               nearlyEqual(steelGovernmentMap.governmentSteelTimeRemaining(),
                           0.0f) &&
               expiredImpact == ImpactKind::GovernmentWall &&
               steelGovernmentMap.governmentWallHealth(0) == 3,
           "expired shovel steel did not restore ordinary wall damage");

    steelGovernmentMap.activateGovernmentSteel();
    std::string reloadError;
    const bool reloaded = steelGovernmentMap.load(
        noResourceDependency, 2, reloadError);
    bool reloadRepaired = reloaded && steelGovernmentMap.stage() == 2 &&
                          !steelGovernmentMap.governmentWallsSteel();
    for (int index = 0; index < kGovernmentWallCount; ++index)
        reloadRepaired = reloadRepaired &&
                         steelGovernmentMap.governmentWallHealth(index) ==
                             kGovernmentWallMaximumHealth;
    expect(reloadRepaired,
           "loading a stage retained shovel time or wall damage");

    const XZ coreInside{kGovernmentBaseCenter.x +
                            kGovernmentCoreHalfSize - 0.001f,
                        kGovernmentBaseCenter.z};
    const XZ coreOutside{kGovernmentBaseCenter.x +
                             kGovernmentCoreHalfSize + 0.001f,
                         kGovernmentBaseCenter.z};
    const XZ shellInside{kGovernmentBaseCenter.x + kGovernmentCoreHalfSize +
                             kShellHalfSize - 0.001f,
                         kGovernmentBaseCenter.z};
    const XZ shellOutside{kGovernmentBaseCenter.x + kGovernmentCoreHalfSize +
                              kShellHalfSize + 0.001f,
                          kGovernmentBaseCenter.z};
    expect(geometryMap.isInsideBase(kGovernmentBaseCenter) &&
               geometryMap.isInsideBase(coreInside) &&
               geometryMap.isInsideBase({13.99f, 25.99f}) &&
               !geometryMap.isInsideBase({14.0f, 26.0f}) &&
               !geometryMap.isInsideBase(coreOutside) &&
               geometryMap.shellHitsGovernmentCore(kGovernmentBaseCenter) &&
               geometryMap.shellHitsGovernmentCore(shellInside) &&
               !geometryMap.shellHitsGovernmentCore(shellOutside) &&
               !geometryMap.shellHitsGovernmentCore(testedWall.center),
           "government core point or shell AABB boundary drifted");

    StageMap coreSeparationMap;
    std::string coreSeparationError;
    const bool coreSeparationLoaded = coreSeparationMap.load(
        noResourceDependency, 1, coreSeparationError);
    for (int index = 0; index < kGovernmentWallCount; ++index)
    {
        const XZ center = governmentWallSegment(index).center;
        coreSeparationMap.impactShell(center, true,
                                      CardinalDirection::North);
        coreSeparationMap.impactShell(center, true,
                                      CardinalDirection::North);
    }
    expect(coreSeparationLoaded &&
               coreSeparationMap.solidSeparatesShells(
                   {kGovernmentBaseCenter.x - 1.10f,
                    kGovernmentBaseCenter.z},
                   {kGovernmentBaseCenter.x + 1.10f,
                    kGovernmentBaseCenter.z}) &&
               !coreSeparationMap.solidSeparatesShells(
                   {kGovernmentBaseCenter.x - 1.10f,
                    kGovernmentBaseCenter.z - 1.10f},
                   {kGovernmentBaseCenter.x + 1.10f,
                    kGovernmentBaseCenter.z - 1.10f}),
           "government core stopped separating opposing shells after a breach");

    expect(geometryMap.governmentWallHealth(-1) == 0 &&
               geometryMap.governmentWallHealth(kGovernmentWallCount) == 0 &&
               tanks3d::game::bonusOverlapsGovernmentBase(
                   kGovernmentBaseCenter) &&
               !tanks3d::game::bonusOverlapsGovernmentBase(
                   {kGovernmentBaseCenter.x + 2.0f,
                    kGovernmentBaseCenter.z}) &&
               tanks3d::game::bonusOverlapsGovernmentBase(
                   {kGovernmentBaseCenter.x + 1.999f,
                    kGovernmentBaseCenter.z}),
           "invalid wall health or strict bonus/base overlap boundary drifted");

    reporter.finish();
    return passed ? 0 : 1;
}
