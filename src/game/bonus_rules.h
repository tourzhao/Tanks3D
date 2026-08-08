#ifndef TANKS3D_GAME_BONUS_RULES_H
#define TANKS3D_GAME_BONUS_RULES_H

#include <algorithm>

namespace tanks3d::game
{
inline constexpr float kPickupLifetime = 12.5f;
inline constexpr float kPickupFastBlinkStart = kPickupLifetime * 0.75f;

enum class BonusType : unsigned char
{
    Grenade,
    Helmet,
    Clock,
    Shovel,
    Tank,
    Star,
    Gun,
    Boat,
    Bandage,
    Count
};

inline constexpr bool isValidBonusType(BonusType type)
{
    return static_cast<unsigned int>(type) <
           static_cast<unsigned int>(BonusType::Count);
}

inline constexpr int kClassicPickupTypeCount =
    static_cast<int>(BonusType::Bandage);
inline constexpr int kBandageWeight = 2;

inline constexpr int weightedTypeSlotCount(bool bandageEligible)
{
    return kClassicPickupTypeCount +
           (bandageEligible ? kBandageWeight : 0);
}

inline constexpr BonusType typeForWeightedSlot(int requestedSlot,
                                                bool bandageEligible)
{
    const int slot = std::clamp(
        requestedSlot, 0, weightedTypeSlotCount(bandageEligible) - 1);
    return slot < kClassicPickupTypeCount
               ? static_cast<BonusType>(slot)
               : BonusType::Bandage;
}
} // namespace tanks3d::game

#endif
