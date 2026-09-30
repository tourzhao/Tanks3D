#ifndef TANKS3D_CORE_NATION_H
#define TANKS3D_CORE_NATION_H

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>

namespace tanks3d::core
{
enum class Nation
{
    UnitedStates,
    SovietUnion,
    Germany,
    Count
};

inline constexpr std::array<Nation, 3> kSelectableNations{{
    Nation::UnitedStates, Nation::SovietUnion, Nation::Germany}};

inline Nation normalizedNation(Nation nation)
{
    for (Nation candidate : kSelectableNations)
        if (candidate == nation)
            return nation;
    return Nation::UnitedStates;
}

// Pick a national identity from the unselected sides using a separate,
// deterministic cosmetic hash. The construction seed, stage and successful
// spawn ID make this independent of lookup order, player death, spawn retries
// and the shared gameplay random stream. Same-stage restarts repeat the picks.
inline Nation opposingNationForPlayers(
    const std::array<Nation, 2> &playerNations, int playerCount, int enemyId,
    std::uint32_t sessionSeed, int stage)
{
    const int enabledPlayers = std::clamp(playerCount, 1, 2);
    std::array<Nation, kSelectableNations.size()> opponents{};
    std::size_t opponentCount = 0U;
    for (Nation candidate : kSelectableNations)
    {
        bool selectedByPlayer = false;
        for (int index = 0; index < enabledPlayers; ++index)
        {
            if (candidate == normalizedNation(
                                 playerNations[static_cast<std::size_t>(index)]))
            {
                selectedByPlayer = true;
                break;
            }
        }
        if (!selectedByPlayer)
            opponents[opponentCount++] = candidate;
    }
    // At most two player nations are excluded from the three choices, so
    // opponentCount is always nonzero, including after input normalization.
    // Fixed-width unsigned arithmetic keeps LAN peers and replays identical
    // across platforms. SplitMix64's finalizer mixes adjacent IDs instead of
    // alternating them; consecutive enemies may legitimately share a nation.
    const auto spawnId = static_cast<std::uint64_t>(std::max(enemyId, 0));
    const auto stageId = static_cast<std::uint64_t>(std::max(stage, 1));
    std::uint64_t choice = static_cast<std::uint64_t>(sessionSeed) ^
                           (stageId << 32U) ^ UINT64_C(0x4e4154494f4e5333);
    choice += (spawnId + 1U) * UINT64_C(0x9e3779b97f4a7c15);
    choice = (choice ^ (choice >> 30U)) * UINT64_C(0xbf58476d1ce4e5b9);
    choice = (choice ^ (choice >> 27U)) * UINT64_C(0x94d049bb133111eb);
    choice ^= choice >> 31U;
    return opponents[static_cast<std::size_t>(choice % opponentCount)];
}

inline Nation cycleNation(Nation nation, int direction)
{
    int index = 0;
    for (int candidate = 0;
         candidate < static_cast<int>(kSelectableNations.size()); ++candidate)
    {
        if (kSelectableNations[static_cast<std::size_t>(candidate)] == nation)
            index = candidate;
    }
    const int count = static_cast<int>(kSelectableNations.size());
    index = (index + direction % count + count) % count;
    return kSelectableNations[static_cast<std::size_t>(index)];
}

inline const char *nationName(Nation nation)
{
    switch (nation)
    {
    case Nation::UnitedStates: return "USA";
    case Nation::SovietUnion: return "USSR";
    case Nation::Germany: return "GERMANY";
    case Nation::Count: break;
    }
    return "UNKNOWN";
}
} // namespace tanks3d::core

#endif
