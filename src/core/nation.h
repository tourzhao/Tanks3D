#ifndef TANKS3D_CORE_NATION_H
#define TANKS3D_CORE_NATION_H

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
