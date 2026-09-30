#ifndef TANKS3D_GAME_VEHICLE_IDENTITY_H
#define TANKS3D_GAME_VEHICLE_IDENTITY_H

#include "core/nation.h"
#include <algorithm>
#include <array>

namespace wwii_tank_model
{
using Nation = tanks3d::core::Nation;
enum class Vehicle
{
    M24Chaffee,
    M4A3Sherman,
    M26Pershing,
    T28T95,
    T70,
    T3485,
    IS2,
    KV5Project,
    PanzerIIF,
    PanzerIVH,
    TigerIE,
    Maus,
    Sdkfz231SixRad,
    PanzerIIIL,
    // Append new presentation IDs; keep the original IDs stable for recorded
    // snapshots and tools which still name the legacy demonstration models.
    M60A3,
    M1A1,
    T3476,
    T62,
    T90A,
    PantherA,
    TigerII,
    Leopard1,
    Leopard2A4
};

inline Vehicle playerVehicle(Nation nation, int level)
{
    const int tier = std::clamp(level, 0, 3);
    switch (nation)
    {
    case Nation::UnitedStates:
        switch (tier)
        {
        case 0: return Vehicle::M4A3Sherman;
        case 1: return Vehicle::M26Pershing;
        case 2: return Vehicle::M60A3;
        default: return Vehicle::M1A1;
        }
    case Nation::SovietUnion:
        switch (tier)
        {
        case 0: return Vehicle::T3476;
        case 1: return Vehicle::IS2;
        case 2: return Vehicle::T62;
        default: return Vehicle::T90A;
        }
    case Nation::Germany:
        switch (tier)
        {
        case 0: return Vehicle::PantherA;
        case 1: return Vehicle::TigerII;
        case 2: return Vehicle::Leopard1;
        default: return Vehicle::Leopard2A4;
        }
    case Nation::Count:
        break;
    }
    return playerVehicle(Nation::UnitedStates, tier);
}

inline Vehicle enemyVehicle(int type)
{
    switch ((type % 4 + 4) % 4)
    {
    case 0: return Vehicle::PanzerIIF;          // A: basic light tank
    case 1: return Vehicle::Sdkfz231SixRad;     // B: fast wheeled armored car
    case 2: return Vehicle::PanzerIIIL;         // C: long-gun, fast projectile
    default: return Vehicle::TigerIE;           // D: broad heavy silhouette
    }
}

inline Vehicle enemyVehicle(Nation nation, int type)
{
    nation = tanks3d::core::normalizedNation(nation);
    // These tiers choose silhouettes only: Fast remains the compact light
    // tank, while the four classic enemy types retain their combat rules.
    static constexpr std::array<int, 4> roleTiers{{1, 0, 2, 3}};
    const int role = (type % 4 + 4) % 4;
    return playerVehicle(nation, roleTiers[static_cast<std::size_t>(role)]);
}

// Compatibility selector retained for the original enemy mapping tests.
// Legacy player callers deliberately continue to resolve to the Sherman.
inline Vehicle vehicleFor(bool enemy, int identity)
{
    return enemy ? enemyVehicle(identity) : Vehicle::M4A3Sherman;
}

inline const char *tierName(int level)
{
    switch (std::clamp(level, 0, 3))
    {
    case 0: return "LIGHT";
    case 1: return "MEDIUM";
    case 2: return "HEAVY";
    default: return "SUPER HEAVY";
    }
}

inline const char *nameForVehicle(Vehicle vehicle)
{
    switch (vehicle)
    {
    case Vehicle::M24Chaffee: return "M24 CHAFFEE";
    case Vehicle::M4A3Sherman: return "M4A3(75) SHERMAN";
    case Vehicle::M26Pershing: return "M26 PERSHING";
    case Vehicle::T28T95: return "T28/T95";
    case Vehicle::T70: return "T-70";
    case Vehicle::T3485: return "T-34-85";
    case Vehicle::IS2: return "IS-2";
    case Vehicle::KV5Project: return "KV-5 PROJECT";
    case Vehicle::PanzerIIF: return "PANZER II AUSF. F";
    case Vehicle::PanzerIVH: return "PANZER IV AUSF. H";
    case Vehicle::TigerIE: return "TIGER I AUSF. E";
    case Vehicle::Maus: return "PANZER VIII MAUS";
    case Vehicle::Sdkfz231SixRad: return "SD.KFZ. 231 6-RAD";
    case Vehicle::PanzerIIIL: return "PANZER III AUSF. L";
    case Vehicle::M60A3: return "M60A3";
    case Vehicle::M1A1: return "M1A1 ABRAMS";
    case Vehicle::T3476: return "T-34/76";
    case Vehicle::T62: return "T-62";
    case Vehicle::T90A: return "T-90A";
    case Vehicle::PantherA: return "PANTHER AUSF. A";
    case Vehicle::TigerII: return "TIGER II";
    case Vehicle::Leopard1: return "LEOPARD 1";
    case Vehicle::Leopard2A4: return "LEOPARD 2A4";
    }
    return "TANK";
}

inline const char *playerVehicleName(Nation nation, int level)
{
    return nameForVehicle(playerVehicle(nation, level));
}

inline const char *vehicleName(bool enemy, int identity)
{
    return nameForVehicle(vehicleFor(enemy, identity));
}

} // namespace wwii_tank_model

#endif
