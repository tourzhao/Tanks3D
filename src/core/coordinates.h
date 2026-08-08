#ifndef TANKS3D_CORE_COORDINATES_H
#define TANKS3D_CORE_COORDINATES_H

#include <algorithm>
#include <cmath>

namespace tanks3d::core
{
struct XZ
{
    float x = 0.0f;
    float z = 0.0f;
};

inline XZ operator+(XZ a, XZ b)
{
    return {a.x + b.x, a.z + b.z};
}

inline XZ operator-(XZ a, XZ b)
{
    return {a.x - b.x, a.z - b.z};
}

inline XZ operator*(XZ a, float scalar)
{
    return {a.x * scalar, a.z * scalar};
}

inline float lengthSquared(XZ value)
{
    return value.x * value.x + value.z * value.z;
}

inline float distanceSquared(XZ a, XZ b)
{
    return lengthSquared(a - b);
}

inline bool axisAlignedCentersOverlap(XZ first, XZ second, float extent)
{
    return std::fabs(first.x - second.x) < extent &&
           std::fabs(first.z - second.z) < extent;
}

enum class CardinalDirection : unsigned char
{
    None,
    North,
    South,
    West,
    East
};

inline XZ cardinalVector(CardinalDirection direction)
{
    switch (direction)
    {
    case CardinalDirection::North: return {0.0f, -1.0f};
    case CardinalDirection::South: return {0.0f, 1.0f};
    case CardinalDirection::West: return {-1.0f, 0.0f};
    case CardinalDirection::East: return {1.0f, 0.0f};
    case CardinalDirection::None: return {};
    }
    return {};
}

inline float cardinalYaw(CardinalDirection direction)
{
    constexpr float kHalfTurnRadians = 3.14159265358979323846f;
    switch (direction)
    {
    case CardinalDirection::North: return 0.0f;
    case CardinalDirection::South: return kHalfTurnRadians;
    case CardinalDirection::West: return -kHalfTurnRadians * 0.5f;
    case CardinalDirection::East: return kHalfTurnRadians * 0.5f;
    case CardinalDirection::None: return 0.0f;
    }
    return 0.0f;
}

inline constexpr float kIceSlipDuration = 0.380f;

inline bool resolveIceTravel(bool onIce, bool propelling, bool blocked,
                             bool hadMomentum, CardinalDirection facing,
                             float dt, CardinalDirection &travel,
                             float &slipTimer, bool &wasOnIce)
{
    if (!onIce)
    {
        wasOnIce = false;
        slipTimer = 0.0f;
        travel = facing;
        return propelling;
    }

    if (!wasOnIce)
    {
        wasOnIce = true;
        slipTimer = (propelling || hadMomentum) ? kIceSlipDuration : 0.0f;
        if (travel == CardinalDirection::None)
            travel = facing;
    }
    if (blocked)
    {
        slipTimer = 0.0f;
        travel = facing;
        return false;
    }

    if (propelling && facing == travel)
        slipTimer = kIceSlipDuration;
    else
        slipTimer = std::max(0.0f, slipTimer - dt);

    if (slipTimer <= 0.0f)
    {
        travel = facing;
        return propelling;
    }
    return propelling || hadMomentum;
}

inline CardinalDirection cardinalToward(XZ from, XZ target)
{
    const float dx = target.x - from.x;
    const float dz = target.z - from.z;
    if (std::fabs(dx) > std::fabs(dz))
        return dx < 0.0f ? CardinalDirection::West : CardinalDirection::East;
    return dz < 0.0f ? CardinalDirection::North : CardinalDirection::South;
}

inline XZ snappedToCardinalLane(XZ position, CardinalDirection direction)
{
    constexpr float kSnapDistance = 5.0f / 16.0f;
    if (direction == CardinalDirection::North ||
        direction == CardinalDirection::South)
    {
        const float lane = std::round(position.x);
        if (std::fabs(position.x - lane) < kSnapDistance)
            position.x = lane;
    }
    else if (direction == CardinalDirection::West ||
             direction == CardinalDirection::East)
    {
        const float lane = std::round(position.z);
        if (std::fabs(position.z - lane) < kSnapDistance)
            position.z = lane;
    }
    return position;
}

inline CardinalDirection oppositeCardinalDirection(
    CardinalDirection direction)
{
    switch (direction)
    {
    case CardinalDirection::North: return CardinalDirection::South;
    case CardinalDirection::South: return CardinalDirection::North;
    case CardinalDirection::West: return CardinalDirection::East;
    case CardinalDirection::East: return CardinalDirection::West;
    case CardinalDirection::None: return CardinalDirection::None;
    }
    return CardinalDirection::None;
}
} // namespace tanks3d::core

#endif
