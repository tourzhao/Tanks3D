#include "core/nation.h"
#include "test_support.h"

#include <array>
#include <limits>
#include <string>

namespace
{
using tanks3d::core::Nation;
using tanks3d::core::opposingNationForPlayers;

constexpr Nation kUsa = Nation::UnitedStates;
constexpr Nation kUssr = Nation::SovietUnion;
constexpr Nation kGermany = Nation::Germany;
constexpr std::uint32_t kSeed = 0xead10000U;
constexpr int kStage = 7;
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

    reporter.beginSuite("opposing-nations-solo-ignores-disabled-player-slot");
    for (Nation player : tanks3d::core::kSelectableNations)
    {
        std::array<bool, 3> observed{};
        for (Nation disabledPlayer :
             std::array<Nation, 4>{{kUsa, kUssr, kGermany, Nation::Count}})
        {
            for (int id = 0; id < 64; ++id)
            {
                const Nation opponent = opposingNationForPlayers(
                    {{player, disabledPlayer}}, 1, id, kSeed, kStage);
                expect(opponent != player && opponent != Nation::Count &&
                           opponent == opposingNationForPlayers(
                               {{player, player}}, 1, id, kSeed, kStage),
                       "solo enemy used its player's nation or the disabled P2 slot");
                observed[static_cast<std::size_t>(opponent)] = true;
            }
        }
        for (Nation candidate : tanks3d::core::kSelectableNations)
            expect(observed[static_cast<std::size_t>(candidate)] == (candidate != player),
                   "solo random sequence did not exercise both opposing nations");
    }

    reporter.beginSuite("opposing-nations-all-coop-pairs-and-slot-order");
    for (Nation first : tanks3d::core::kSelectableNations)
    {
        for (Nation second : tanks3d::core::kSelectableNations)
        {
            std::array<bool, 3> observed{};
            for (int id = 0; id < 64; ++id)
            {
                const Nation opponent = opposingNationForPlayers(
                    {{first, second}}, 2, id, kSeed, kStage);
                expect(opponent != first && opponent != second &&
                           opponent == opposingNationForPlayers(
                               {{second, first}}, 2, id, kSeed, kStage),
                       "co-op random choice failed to exclude both nations or changed with slot order");
                if (first == second)
                    expect(opponent == opposingNationForPlayers(
                               {{first, second}}, 1, id, kSeed, kStage),
                           "same-nation co-op and solo have different eligible pools");
                observed[static_cast<std::size_t>(opponent)] = true;
            }
            for (Nation candidate : tanks3d::core::kSelectableNations)
                expect(observed[static_cast<std::size_t>(candidate)] ==
                           (candidate != first && candidate != second),
                       "co-op sequence did not cover exactly its eligible nation pool");
        }
    }

    reporter.beginSuite("opposing-nations-randomized-stable-seed-stage-and-id");
    for (Nation player : tanks3d::core::kSelectableNations)
    {
        const std::array<Nation, 2> players{{player, player}};
        std::array<Nation, 64> choices{};
        bool adjacentRepeat = false;
        bool seedChanged = false;
        bool stageChanged = false;
        for (std::size_t id = 0; id < choices.size(); ++id)
        {
            choices[id] = opposingNationForPlayers(players, 1, static_cast<int>(id), kSeed, kStage);
            adjacentRepeat |= id > 0 && choices[id] == choices[id - 1];
            seedChanged |= choices[id] != opposingNationForPlayers(
                players, 1, static_cast<int>(id), kSeed + 1U, kStage);
            stageChanged |= choices[id] != opposingNationForPlayers(
                players, 1, static_cast<int>(id), kSeed, kStage + 1);
        }
        expect(adjacentRepeat && seedChanged && stageChanged,
               "enemy nations still alternate or ignore the session seed/stage");
        for (std::size_t count = choices.size(); count > 0; --count)
        {
            const int id = static_cast<int>(count - 1U);
            expect(opposingNationForPlayers(players, 1, id, kSeed, kStage) == choices[count - 1U] &&
                       opposingNationForPlayers(players, 1, id, kSeed, kStage) == choices[count - 1U],
                   "repeat or reverse-order enemy lookups changed an existing identity");
        }
    }

    reporter.beginSuite("opposing-nations-spawn-id-and-seed-boundaries");
    const std::array<Nation, 2> players{{kUsa, kGermany}};
    for (std::uint32_t seed : {0U, std::numeric_limits<std::uint32_t>::max()})
    {
        const Nation zero = opposingNationForPlayers(players, 1, 0, seed, 1);
        expect(opposingNationForPlayers(players, 1, -1, seed, 1) == zero &&
                   opposingNationForPlayers(players, 1, std::numeric_limits<int>::min(), seed, 1) == zero,
               "negative spawn IDs did not normalize to zero");
        for (int id : {0, std::numeric_limits<int>::max()})
        {
            for (int stage : {1, std::numeric_limits<int>::max()})
            {
                const Nation choice = opposingNationForPlayers(players, 1, id, seed, stage);
                expect((choice == kUssr || choice == kGermany) &&
                           opposingNationForPlayers(players, 2, id, seed, stage) == kUssr,
                       "large IDs, stages or seed values escaped their opposing pool");
            }
        }
        expect(opposingNationForPlayers(players, 1, 0, seed, 0) == zero &&
                   opposingNationForPlayers(players, 1, 0, seed, std::numeric_limits<int>::min()) == zero,
               "nonpositive stages did not normalize to stage one");
    }

    reporter.beginSuite("opposing-nations-invalid-input-normalization");
    for (Nation invalid : {Nation::Count, static_cast<Nation>(-4), static_cast<Nation>(999)})
    {
        for (int id = 0; id < 16; ++id)
        {
            expect(opposingNationForPlayers({{invalid, kGermany}}, 1, id, kSeed, kStage) ==
                       opposingNationForPlayers({{kUsa, kGermany}}, 1, id, kSeed, kStage) &&
                       opposingNationForPlayers({{kGermany, invalid}}, 2, id, kSeed, kStage) == kUssr,
                   "invalid nation did not normalize before exclusion");
        }
    }
    for (int count : {std::numeric_limits<int>::min(), -1, 0, 3, std::numeric_limits<int>::max()})
    {
        for (int id = 0; id < 16; ++id)
            expect(opposingNationForPlayers(players, count, id, kSeed, kStage) ==
                       opposingNationForPlayers(players, count < 1 ? 1 : 2, id, kSeed, kStage),
                   "invalid player count did not clamp before exclusion");
    }

    if (!passed)
        return 1;
    reporter.finish();
    return 0;
}
