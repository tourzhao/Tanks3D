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

struct NationSequenceCase
{
    std::array<Nation, 2> players;
    std::array<Nation, 4> expected;
};
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
    const std::array<NationSequenceCase, 3> soloCases{{
        {{{kUsa, kUsa}}, {{kUssr, kGermany, kUssr, kGermany}}},
        {{{kUssr, kUsa}}, {{kUsa, kGermany, kUsa, kGermany}}},
        {{{kGermany, kUsa}}, {{kUsa, kUssr, kUsa, kUssr}}}}};
    for (const NationSequenceCase &testCase : soloCases)
    {
        for (Nation disabledPlayer :
             std::array<Nation, 4>{{kUsa, kUssr, kGermany, Nation::Count}})
        {
            const std::array<Nation, 2> players{{
                testCase.players[0], disabledPlayer}};
            for (std::size_t id = 0; id < testCase.expected.size(); ++id)
            {
                expect(opposingNationForPlayers(
                           players, 1, static_cast<int>(id)) ==
                           testCase.expected[id],
                       "solo opponent sequence changed with an unused P2 nation");
            }
        }
    }

    reporter.beginSuite("opposing-nations-all-coop-pairs-and-slot-order");
    const std::array<NationSequenceCase, 9> coopCases{{
        {{{kUsa, kUsa}}, {{kUssr, kGermany, kUssr, kGermany}}},
        {{{kUsa, kUssr}}, {{kGermany, kGermany, kGermany, kGermany}}},
        {{{kUsa, kGermany}}, {{kUssr, kUssr, kUssr, kUssr}}},
        {{{kUssr, kUsa}}, {{kGermany, kGermany, kGermany, kGermany}}},
        {{{kUssr, kUssr}}, {{kUsa, kGermany, kUsa, kGermany}}},
        {{{kUssr, kGermany}}, {{kUsa, kUsa, kUsa, kUsa}}},
        {{{kGermany, kUsa}}, {{kUssr, kUssr, kUssr, kUssr}}},
        {{{kGermany, kUssr}}, {{kUsa, kUsa, kUsa, kUsa}}},
        {{{kGermany, kGermany}}, {{kUsa, kUssr, kUsa, kUssr}}}}};
    for (const NationSequenceCase &testCase : coopCases)
    {
        const std::array<Nation, 2> swapped{{
            testCase.players[1], testCase.players[0]}};
        for (std::size_t id = 0; id < testCase.expected.size(); ++id)
        {
            expect(opposingNationForPlayers(
                       testCase.players, 2, static_cast<int>(id)) ==
                           testCase.expected[id] &&
                       opposingNationForPlayers(
                           swapped, 2, static_cast<int>(id)) ==
                           testCase.expected[id],
                   "co-op selection failed to exclude both nations in stable order");
        }
    }

    reporter.beginSuite("opposing-nations-spawn-id-boundaries");
    for (const NationSequenceCase &testCase : soloCases)
    {
        expect(opposingNationForPlayers(testCase.players, 1, -1) ==
                       testCase.expected[0] &&
                   opposingNationForPlayers(
                       testCase.players, 1, std::numeric_limits<int>::min()) ==
                       testCase.expected[0] &&
                   opposingNationForPlayers(
                       testCase.players, 1, std::numeric_limits<int>::max()) ==
                       testCase.expected[1],
               "solo spawn ID normalization overflowed or changed the cycle");
    }
    for (const NationSequenceCase &testCase : coopCases)
    {
        expect(opposingNationForPlayers(testCase.players, 2, -1) ==
                       testCase.expected[0] &&
                   opposingNationForPlayers(
                       testCase.players, 2, std::numeric_limits<int>::min()) ==
                       testCase.expected[0] &&
                   opposingNationForPlayers(
                       testCase.players, 2, std::numeric_limits<int>::max()) ==
                       testCase.expected[1],
               "co-op spawn ID normalization overflowed or changed the cycle");
    }

    reporter.beginSuite("opposing-nations-invalid-input-normalization");
    struct NormalizationCase
    {
        std::array<Nation, 2> players;
        int playerCount;
        int enemyId;
        Nation expected;
    };
    const std::array<NormalizationCase, 12> normalizationCases{{
        {{{Nation::Count, kGermany}}, 1, 0, kUssr},
        {{{Nation::Count, kGermany}}, 1, 1, kGermany},
        {{{kUssr, Nation::Count}}, 2, 0, kGermany},
        {{{static_cast<Nation>(-4), kGermany}}, 2, 0, kUssr},
        {{{static_cast<Nation>(999), kUssr}}, 2, 0, kGermany},
        {{{Nation::Count, static_cast<Nation>(999)}}, 2, 1, kGermany},
        {{{kGermany, kUssr}}, 0, 1, kUssr},
        {{{kUsa, kUssr}}, -7, 0, kUssr},
        {{{kUssr, kGermany}}, std::numeric_limits<int>::min(), 1, kGermany},
        {{{kUssr, kGermany}}, 3, 1, kUsa},
        {{{kUsa, kGermany}}, std::numeric_limits<int>::max(), 1, kUssr},
        {{{Nation::Count, kUssr}}, std::numeric_limits<int>::max(), -1, kGermany}}};
    for (const NormalizationCase &testCase : normalizationCases)
    {
        expect(opposingNationForPlayers(testCase.players, testCase.playerCount,
                                        testCase.enemyId) == testCase.expected,
               "invalid nation or player count did not normalize before exclusion");
    }

    if (!passed)
        return 1;
    reporter.finish();
    return 0;
}
