#ifndef TANKS3D_CORE_NATION_H
#define TANKS3D_CORE_NATION_H

#include <algorithm>
#include <array>
#include <cstddef>

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

// Derive each enemy's national identity from the session's enabled player
// slots and its stable spawn ID. Player death and spawn retries do not alter
// this selection, and no gameplay random draw is needed.
inline Nation opposingNationForPlayers(
    const std::array<Nation, 2> &playerNations, int playerCount, int enemyId)
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
    const std::size_t spawnId = static_cast<std::size_t>(std::max(enemyId, 0));
    return opponents[spawnId % opponentCount];
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
