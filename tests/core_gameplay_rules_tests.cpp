#include "core/gameplay_rules.h"
#include "core/nation.h"

#include "test_support.h"

#include <array>
#include <cmath>
#include <string>

namespace
{
using tanks3d::core::AdvancedGameSettings;
using tanks3d::core::Nation;
using tanks3d::core::PlayerLevelStats;
using tanks3d::core::cycleNation;
using tanks3d::core::kBasePlayerSpeed;
using tanks3d::core::kBaseShellSpeed;
using tanks3d::core::kClassicBaseShellSpeed;
using tanks3d::core::kDefaultPlayerMaximumHitPoints;
using tanks3d::core::kEnemyTuningMaximumPercent;
using tanks3d::core::kEnemyTuningMinimumPercent;
using tanks3d::core::kEnemyTuningPercentStep;
using tanks3d::core::kFastPlayerSpeed;
using tanks3d::core::kFastShellSpeed;
using tanks3d::core::kMaximumPlayerMaximumHitPoints;
using tanks3d::core::kMinimumPlayerMaximumHitPoints;
using tanks3d::core::kSelectableNations;
using tanks3d::core::kShellPacingScale;
using tanks3d::core::nationName;
using tanks3d::core::normalizedAdvancedSettings;
using tanks3d::core::normalizedEnemyTuningPercent;
using tanks3d::core::normalizedNation;
using tanks3d::core::playerLevelStats;
using tanks3d::core::upgradedPlayerLevel;

bool nearlyEqual(float first, float second, float tolerance = 0.00001f)
{
    return std::fabs(first - second) <= tolerance;
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

    reporter.beginSuite("core-advanced-settings");
    expect(kDefaultPlayerMaximumHitPoints == 3 &&
               kMinimumPlayerMaximumHitPoints == 1 &&
               kMaximumPlayerMaximumHitPoints == 6,
           "player HP constants changed");
    expect(kEnemyTuningMinimumPercent == -30 &&
               kEnemyTuningMaximumPercent == 30 &&
               kEnemyTuningPercentStep == 5,
           "enemy tuning constants changed");

    struct IntegerCase
    {
        int requested;
        int expected;
    };
    const std::array<IntegerCase, 8> hitPointCases{{
        {0, 1}, {1, 1}, {2, 2}, {3, 3},
        {4, 4}, {5, 5}, {6, 6}, {7, 6},
    }};
    for (const IntegerCase &test : hitPointCases)
    {
        AdvancedGameSettings settings;
        settings.playerMaximumHitPoints = test.requested;
        const AdvancedGameSettings normalized =
            normalizedAdvancedSettings(settings);
        expect(normalized.playerMaximumHitPoints == test.expected &&
                   normalized.enemySpeedPercent == 0 &&
                   normalized.enemyFireRatePercent == 0 &&
                   normalized.enemySpawnRatePercent == 0,
               "HP normalization changed for " +
                   std::to_string(test.requested));
    }

    for (int percent = kEnemyTuningMinimumPercent;
         percent <= kEnemyTuningMaximumPercent;
         percent += kEnemyTuningPercentStep)
    {
        expect(normalizedEnemyTuningPercent(percent) == percent,
               "valid tuning step changed at " + std::to_string(percent));
    }

    const std::array<IntegerCase, 20> tuningBoundaryCases{{
        {-100, -30}, {-31, -30}, {-30, -30}, {-29, -30}, {-28, -30},
        {-27, -25}, {-26, -25}, {-24, -25}, {-23, -25}, {-22, -20},
        {-3, -5}, {-2, 0}, {2, 0}, {3, 5}, {22, 20},
        {23, 25}, {27, 25}, {28, 30}, {31, 30}, {100, 30},
    }};
    for (const IntegerCase &test : tuningBoundaryCases)
    {
        expect(normalizedEnemyTuningPercent(test.requested) == test.expected,
               "tuning rounding changed for " +
                   std::to_string(test.requested));
    }

    for (int percent = kEnemyTuningMinimumPercent;
         percent <= kEnemyTuningMaximumPercent;
         percent += kEnemyTuningPercentStep)
    {
        AdvancedGameSettings settings;
        settings.enemySpeedPercent = percent;
        settings.enemyFireRatePercent = percent;
        settings.enemySpawnRatePercent = percent;
        const AdvancedGameSettings normalized =
            normalizedAdvancedSettings(settings);
        expect(normalized.playerMaximumHitPoints == 3 &&
                   normalized.enemySpeedPercent == percent &&
                   normalized.enemyFireRatePercent == percent &&
                   normalized.enemySpawnRatePercent == percent,
               "an advanced tuning field changed at " +
                   std::to_string(percent));
    }

    AdvancedGameSettings oneField;
    oneField.playerMaximumHitPoints = 0;
    AdvancedGameSettings normalized = normalizedAdvancedSettings(oneField);
    expect(normalized.playerMaximumHitPoints == 1 &&
               normalized.enemySpeedPercent == 0 &&
               normalized.enemyFireRatePercent == 0 &&
               normalized.enemySpawnRatePercent == 0,
           "HP normalization changed another advanced field");
    oneField = {};
    oneField.enemySpeedPercent = 3;
    normalized = normalizedAdvancedSettings(oneField);
    expect(normalized.playerMaximumHitPoints == 3 &&
               normalized.enemySpeedPercent == 5 &&
               normalized.enemyFireRatePercent == 0 &&
               normalized.enemySpawnRatePercent == 0,
           "speed normalization changed another advanced field");
    oneField = {};
    oneField.enemyFireRatePercent = -3;
    normalized = normalizedAdvancedSettings(oneField);
    expect(normalized.playerMaximumHitPoints == 3 &&
               normalized.enemySpeedPercent == 0 &&
               normalized.enemyFireRatePercent == -5 &&
               normalized.enemySpawnRatePercent == 0,
           "fire-rate normalization changed another advanced field");
    oneField = {};
    oneField.enemySpawnRatePercent = 28;
    normalized = normalizedAdvancedSettings(oneField);
    expect(normalized.playerMaximumHitPoints == 3 &&
               normalized.enemySpeedPercent == 0 &&
               normalized.enemyFireRatePercent == 0 &&
               normalized.enemySpawnRatePercent == 30,
           "spawn-rate normalization changed another advanced field");

    AdvancedGameSettings composite;
    composite.playerMaximumHitPoints = 99;
    composite.enemySpeedPercent = -99;
    composite.enemyFireRatePercent = 2;
    composite.enemySpawnRatePercent = 29;
    normalized = normalizedAdvancedSettings(composite);
    expect(normalized.playerMaximumHitPoints == 6 &&
               normalized.enemySpeedPercent == -30 &&
               normalized.enemyFireRatePercent == 0 &&
               normalized.enemySpawnRatePercent == 30,
           "combined advanced-setting normalization changed");

    reporter.beginSuite("core-nations-and-progression");
    const std::array<Nation, 4> everyNation{{
        Nation::UnitedStates, Nation::SovietUnion,
        Nation::Germany, Nation::Count}};
    for (std::size_t index = 0; index < everyNation.size(); ++index)
    {
        expect(static_cast<int>(everyNation[index]) == static_cast<int>(index),
               "Nation enum order changed at index " +
                   std::to_string(index));
    }

    expect(kSelectableNations.size() == 3U,
           "selectable nation count changed");
    for (std::size_t index = 0; index < kSelectableNations.size(); ++index)
    {
        expect(kSelectableNations[index] == everyNation[index],
               "selectable nation order changed at index " +
                   std::to_string(index));
    }

    struct NationNameCase
    {
        Nation nation;
        const char *expected;
    };
    const std::array<NationNameCase, 5> nationNameCases{{
        {Nation::UnitedStates, "USA"},
        {Nation::SovietUnion, "USSR"},
        {Nation::Germany, "GERMANY"},
        {Nation::Count, "UNKNOWN"},
        {static_cast<Nation>(99), "UNKNOWN"},
    }};
    for (const NationNameCase &test : nationNameCases)
    {
        expect(std::string(nationName(test.nation)) == test.expected,
               "nation name mapping changed");
    }

    const std::array<Nation, 5> normalizationInputs{{
        Nation::UnitedStates, Nation::SovietUnion, Nation::Germany,
        Nation::Count, static_cast<Nation>(99)}};
    const std::array<Nation, 5> normalizationExpected{{
        Nation::UnitedStates, Nation::SovietUnion, Nation::Germany,
        Nation::UnitedStates, Nation::UnitedStates}};
    for (std::size_t index = 0; index < normalizationInputs.size(); ++index)
    {
        expect(normalizedNation(normalizationInputs[index]) ==
                   normalizationExpected[index],
               "nation normalization changed at index " +
                   std::to_string(index));
    }

    for (std::size_t nationIndex = 0;
         nationIndex < kSelectableNations.size(); ++nationIndex)
    {
        for (int direction = -4; direction <= 4; ++direction)
        {
            const int count = static_cast<int>(kSelectableNations.size());
            const int expectedIndex =
                (static_cast<int>(nationIndex) + direction % count + count) %
                count;
            expect(cycleNation(kSelectableNations[nationIndex], direction) ==
                       kSelectableNations[static_cast<std::size_t>(expectedIndex)],
                   "nation cycle changed for a selectable nation");
        }
    }
    for (int direction = -4; direction <= 4; ++direction)
    {
        const int count = static_cast<int>(kSelectableNations.size());
        const int expectedIndex = (direction % count + count) % count;
        expect(cycleNation(Nation::Count, direction) ==
                   kSelectableNations[static_cast<std::size_t>(expectedIndex)],
               "invalid nation cycle behavior changed");
    }

    struct LevelCase
    {
        int requested;
        PlayerLevelStats expected;
        int upgraded;
    };
    const std::array<LevelCase, 8> levelCases{{
        {-2, {5.0f, 9.775f, 2, false}, 1},
        {-1, {5.0f, 9.775f, 2, false}, 1},
        {0, {5.0f, 9.775f, 2, false}, 1},
        {1, {6.5f, 12.7075f, 2, false}, 2},
        {2, {6.5f, 12.7075f, 3, false}, 3},
        {3, {6.5f, 12.7075f, 4, true}, 3},
        {4, {6.5f, 12.7075f, 4, true}, 3},
        {9, {6.5f, 12.7075f, 4, true}, 3},
    }};
    for (const LevelCase &test : levelCases)
    {
        const PlayerLevelStats stats = playerLevelStats(test.requested);
        expect(nearlyEqual(stats.movementSpeed,
                           test.expected.movementSpeed) &&
                   nearlyEqual(stats.shellSpeed, test.expected.shellSpeed) &&
                   stats.maximumShells == test.expected.maximumShells &&
                   stats.powerShell == test.expected.powerShell,
               "player level stats changed for " +
                   std::to_string(test.requested));
        expect(upgradedPlayerLevel(test.requested) == test.upgraded,
               "Star upgrade changed for " +
                   std::to_string(test.requested));
    }

    expect(nearlyEqual(kClassicBaseShellSpeed, 14.375f) &&
               nearlyEqual(kShellPacingScale, 0.68f) &&
               nearlyEqual(kBaseShellSpeed, 9.775f) &&
               nearlyEqual(kFastShellSpeed, 12.7075f) &&
               nearlyEqual(kBasePlayerSpeed, 5.0f) &&
               nearlyEqual(kFastPlayerSpeed, 6.5f),
           "player progression speed constants changed");

    reporter.finish();
    return passed ? 0 : 1;
}
