#include "core/coordinates.h"

#include "test_support.h"

#include <array>
#include <cmath>
#include <string>

namespace
{
using tanks3d::core::CardinalDirection;
using tanks3d::core::XZ;
using tanks3d::core::axisAlignedCentersOverlap;
using tanks3d::core::cardinalToward;
using tanks3d::core::cardinalVector;
using tanks3d::core::cardinalYaw;
using tanks3d::core::distanceSquared;
using tanks3d::core::kIceSlipDuration;
using tanks3d::core::lengthSquared;
using tanks3d::core::oppositeCardinalDirection;
using tanks3d::core::resolveIceTravel;
using tanks3d::core::snappedToCardinalLane;

bool nearlyEqual(float first, float second, float tolerance = 0.00001f)
{
    return std::fabs(first - second) <= tolerance;
}

bool samePosition(XZ first, XZ second)
{
    return nearlyEqual(first.x, second.x) && nearlyEqual(first.z, second.z);
}
} // namespace

int main()
{
    tanks3d_test::Reporter reporter;
    reporter.reset();
    reporter.beginSuite("core-coordinates-and-directions");
    bool passed = true;
    const auto expect = [&](bool condition, const std::string &message) {
        if (!reporter.check(condition, message))
            passed = false;
    };

    expect(samePosition(XZ{2.0f, -3.0f} + XZ{-5.0f, 7.0f},
                        {-3.0f, 4.0f}),
           "XZ addition changed");
    expect(samePosition(XZ{2.0f, -3.0f} - XZ{-5.0f, 7.0f},
                        {7.0f, -10.0f}),
           "XZ subtraction changed");
    expect(samePosition(XZ{2.0f, -3.0f} * 2.5f, {5.0f, -7.5f}),
           "positive XZ scaling changed");
    expect(samePosition(XZ{2.0f, -3.0f} * -2.0f, {-4.0f, 6.0f}),
           "negative XZ scaling changed");
    expect(samePosition(XZ{2.0f, -3.0f} * 0.0f, {}),
           "zero XZ scaling changed");
    expect(nearlyEqual(lengthSquared({-3.0f, 4.0f}), 25.0f),
           "XZ squared length changed");
    expect(nearlyEqual(distanceSquared({-2.0f, 5.0f}, {1.0f, 1.0f}), 25.0f) &&
               nearlyEqual(distanceSquared({1.0f, 1.0f}, {-2.0f, 5.0f}), 25.0f),
           "XZ squared distance lost symmetry");

    struct DirectionCase
    {
        CardinalDirection direction;
        XZ vector;
        float yaw;
        CardinalDirection opposite;
        const char *name;
    };
    constexpr float kPi = 3.14159265358979323846f;
    const std::array<DirectionCase, 5> directionCases{{
        {CardinalDirection::None, {}, 0.0f, CardinalDirection::None, "None"},
        {CardinalDirection::North, {0.0f, -1.0f}, 0.0f,
         CardinalDirection::South, "North"},
        {CardinalDirection::South, {0.0f, 1.0f}, kPi,
         CardinalDirection::North, "South"},
        {CardinalDirection::West, {-1.0f, 0.0f}, -kPi * 0.5f,
         CardinalDirection::East, "West"},
        {CardinalDirection::East, {1.0f, 0.0f}, kPi * 0.5f,
         CardinalDirection::West, "East"},
    }};
    for (const DirectionCase &test : directionCases)
    {
        const std::string prefix = std::string(test.name) + " direction ";
        expect(samePosition(cardinalVector(test.direction), test.vector),
               prefix + "vector changed");
        expect(nearlyEqual(cardinalYaw(test.direction), test.yaw),
               prefix + "yaw changed");
        expect(oppositeCardinalDirection(test.direction) == test.opposite,
               prefix + "opposite changed");
        expect(oppositeCardinalDirection(
                   oppositeCardinalDirection(test.direction)) == test.direction,
               prefix + "double-opposite changed");
    }

    struct TargetCase
    {
        XZ target;
        CardinalDirection expected;
        const char *name;
    };
    const std::array<TargetCase, 8> targetCases{{
        {{4.0f, 1.0f}, CardinalDirection::East, "east-dominant"},
        {{-4.0f, 1.0f}, CardinalDirection::West, "west-dominant"},
        {{1.0f, -4.0f}, CardinalDirection::North, "north-dominant"},
        {{1.0f, 4.0f}, CardinalDirection::South, "south-dominant"},
        {{1.0f, 1.0f}, CardinalDirection::South, "positive tie"},
        {{-1.0f, 1.0f}, CardinalDirection::South, "mixed positive tie"},
        {{1.0f, -1.0f}, CardinalDirection::North, "mixed negative tie"},
        {{0.0f, 0.0f}, CardinalDirection::South, "same point"},
    }};
    for (const TargetCase &test : targetCases)
    {
        expect(cardinalToward({}, test.target) == test.expected,
               std::string("cardinal targeting changed at ") + test.name);
    }

    struct SnapCase
    {
        XZ position;
        CardinalDirection direction;
        XZ expected;
        const char *name;
    };
    constexpr float kSnapBoundary = 5.0f / 16.0f;
    const std::array<SnapCase, 13> snapCases{{
        {{4.18f, 7.73f}, CardinalDirection::North, {4.0f, 7.73f}, "north"},
        {{4.18f, 7.73f}, CardinalDirection::South, {4.0f, 7.73f}, "south"},
        {{4.18f, 7.73f}, CardinalDirection::West, {4.18f, 8.0f}, "west"},
        {{4.18f, 7.73f}, CardinalDirection::East, {4.18f, 8.0f}, "east"},
        {{4.18f, 7.73f}, CardinalDirection::None, {4.18f, 7.73f}, "none"},
        {{4.0f + kSnapBoundary - 0.0001f, 2.25f}, CardinalDirection::North,
         {4.0f, 2.25f}, "vertical inside positive boundary"},
        {{4.0f + kSnapBoundary, 2.25f}, CardinalDirection::South,
         {4.0f + kSnapBoundary, 2.25f}, "vertical exact positive boundary"},
        {{4.0f + kSnapBoundary + 0.0001f, 2.25f}, CardinalDirection::North,
         {4.0f + kSnapBoundary + 0.0001f, 2.25f},
         "vertical outside positive boundary"},
        {{4.0f - kSnapBoundary + 0.0001f, 2.25f}, CardinalDirection::South,
         {4.0f, 2.25f}, "vertical inside negative boundary"},
        {{2.25f, 8.0f - kSnapBoundary + 0.0001f}, CardinalDirection::West,
         {2.25f, 8.0f}, "horizontal inside negative boundary"},
        {{2.25f, 8.0f - kSnapBoundary}, CardinalDirection::East,
         {2.25f, 8.0f - kSnapBoundary}, "horizontal exact negative boundary"},
        {{2.25f, 8.0f - kSnapBoundary - 0.0001f}, CardinalDirection::West,
         {2.25f, 8.0f - kSnapBoundary - 0.0001f},
         "horizontal outside negative boundary"},
        {{-3.82f, -5.73f}, CardinalDirection::East, {-3.82f, -6.0f},
         "negative lane"},
    }};
    for (const SnapCase &test : snapCases)
    {
        expect(samePosition(
                   snappedToCardinalLane(test.position, test.direction),
                   test.expected),
               std::string("lane snapping changed at ") + test.name);
    }

    struct OverlapCase
    {
        XZ first;
        XZ second;
        float extent;
        bool expected;
        const char *name;
    };
    constexpr float kExtent = 1.75f;
    const std::array<OverlapCase, 12> overlapCases{{
        {{}, {kExtent - 0.0001f, 0.0f}, kExtent, true, "+X penetration"},
        {{}, {kExtent, 0.0f}, kExtent, false, "+X edge-only"},
        {{}, {kExtent + 0.0001f, 0.0f}, kExtent, false, "+X separation"},
        {{}, {-kExtent + 0.0001f, 0.0f}, kExtent, true, "-X penetration"},
        {{}, {-kExtent, 0.0f}, kExtent, false, "-X edge-only"},
        {{}, {0.0f, kExtent - 0.0001f}, kExtent, true, "+Z penetration"},
        {{}, {0.0f, kExtent}, kExtent, false, "+Z edge-only"},
        {{}, {0.0f, -kExtent + 0.0001f}, kExtent, true, "-Z penetration"},
        {{}, {0.0f, -kExtent}, kExtent, false, "-Z edge-only"},
        {{}, {kExtent - 0.0001f, kExtent - 0.0001f}, kExtent, true,
         "corner penetration"},
        {{}, {kExtent, kExtent}, kExtent, false, "corner edge-only"},
        {{1.0f, -2.0f}, {1.0f, -2.0f}, 0.0f, false, "zero extent"},
    }};
    for (const OverlapCase &test : overlapCases)
    {
        const bool overlap = axisAlignedCentersOverlap(
            test.first, test.second, test.extent);
        expect(overlap == test.expected,
               std::string("strict AABB changed at ") + test.name);
        expect(overlap == axisAlignedCentersOverlap(
                              test.second, test.first, test.extent),
               std::string("strict AABB lost symmetry at ") + test.name);
    }

    const std::array<CardinalDirection, 4> movingDirections{{
        CardinalDirection::North, CardinalDirection::East,
        CardinalDirection::South, CardinalDirection::West}};
    for (CardinalDirection direction : movingDirections)
    {
        CardinalDirection travel = CardinalDirection::None;
        float timer = 0.0f;
        bool wasOnIce = false;
        const bool moving = resolveIceTravel(
            true, true, false, false, direction, 0.05f,
            travel, timer, wasOnIce);
        expect(moving && wasOnIce && travel == direction &&
                   nearlyEqual(timer, kIceSlipDuration),
               "ice entry changed for a cardinal direction");

        timer = 0.2f;
        wasOnIce = true;
        const bool offIceMoving = resolveIceTravel(
            false, false, false, true, direction, 0.05f,
            travel, timer, wasOnIce);
        expect(!offIceMoving && !wasOnIce && travel == direction &&
                   nearlyEqual(timer, 0.0f),
               "leaving ice did not reset cardinal travel state");
    }

    const std::array<std::array<CardinalDirection, 2>, 4> turnCases{{
        {{CardinalDirection::North, CardinalDirection::East}},
        {{CardinalDirection::East, CardinalDirection::South}},
        {{CardinalDirection::South, CardinalDirection::West}},
        {{CardinalDirection::West, CardinalDirection::North}},
    }};
    for (const auto &turn : turnCases)
    {
        CardinalDirection travel = turn[0];
        float timer = kIceSlipDuration;
        bool wasOnIce = true;
        const bool sliding = resolveIceTravel(
            true, true, false, true, turn[1], kIceSlipDuration - 0.0001f,
            travel, timer, wasOnIce);
        expect(sliding && travel == turn[0] && timer > 0.0f,
               "ice turn stopped before the 380 ms boundary");
        const bool turned = resolveIceTravel(
            true, true, false, true, turn[1], 0.0001f,
            travel, timer, wasOnIce);
        expect(turned && travel == turn[1] && nearlyEqual(timer, 0.0f),
               "ice turn did not switch at the 380 ms boundary");
    }

    CardinalDirection travel = CardinalDirection::North;
    float timer = kIceSlipDuration;
    bool wasOnIce = true;
    const bool releasedAtBoundary = resolveIceTravel(
        true, false, false, true, CardinalDirection::North,
        kIceSlipDuration, travel, timer, wasOnIce);
    expect(!releasedAtBoundary && nearlyEqual(timer, 0.0f),
           "ice release did not stop at exactly 380 ms");

    travel = CardinalDirection::North;
    timer = kIceSlipDuration;
    wasOnIce = true;
    const bool blocked = resolveIceTravel(
        true, true, true, true, CardinalDirection::West, 0.0f,
        travel, timer, wasOnIce);
    expect(!blocked && travel == CardinalDirection::West &&
               nearlyEqual(timer, 0.0f),
           "blocking did not immediately stop ice propulsion");

    travel = CardinalDirection::None;
    timer = 1.0f;
    wasOnIce = false;
    const bool stationaryEntry = resolveIceTravel(
        true, false, false, false, CardinalDirection::South, 0.0f,
        travel, timer, wasOnIce);
    expect(!stationaryEntry && wasOnIce && travel == CardinalDirection::South &&
               nearlyEqual(timer, 0.0f),
           "stationary ice entry created momentum");

    travel = CardinalDirection::West;
    timer = 0.0f;
    wasOnIce = false;
    const bool momentumEntry = resolveIceTravel(
        true, false, false, true, CardinalDirection::East, 0.0f,
        travel, timer, wasOnIce);
    expect(momentumEntry && wasOnIce && travel == CardinalDirection::West &&
               nearlyEqual(timer, kIceSlipDuration),
           "existing momentum did not carry onto ice");

    travel = CardinalDirection::South;
    timer = 0.01f;
    wasOnIce = true;
    const bool refreshedPropulsion = resolveIceTravel(
        true, true, false, true, CardinalDirection::South, 0.05f,
        travel, timer, wasOnIce);
    expect(refreshedPropulsion && travel == CardinalDirection::South &&
               nearlyEqual(timer, kIceSlipDuration),
           "same-direction ice propulsion did not refresh slip time");

    reporter.finish();
    if (!passed)
        return 1;
    return 0;
}
