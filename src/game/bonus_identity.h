#ifndef TANKS3D_GAME_BONUS_IDENTITY_H
#define TANKS3D_GAME_BONUS_IDENTITY_H

#include "game/bonus_system.h"

namespace tanks3d::game
{
inline const char *bonusName(BonusType type)
{
    switch (type)
    {
    case BonusType::Grenade: return "GRENADE";
    case BonusType::Helmet: return "HELMET";
    case BonusType::Clock: return "CLOCK";
    case BonusType::Shovel: return "SHOVEL";
    case BonusType::Tank: return "1-UP TANK";
    case BonusType::Star: return "STAR";
    case BonusType::Gun: return "MAX GUN";
    case BonusType::Boat: return "BOAT";
    case BonusType::Bandage: return "BANDAGE";
    case BonusType::Count: return "UNKNOWN";
    }
    return "UNKNOWN";
}

} // namespace tanks3d::game

#endif
