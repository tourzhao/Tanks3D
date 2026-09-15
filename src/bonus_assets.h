#ifndef TANKS3D_BONUS_ASSETS_H
#define TANKS3D_BONUS_ASSETS_H

#include "game/bonus_system.h"

#include <raylib.h>
#include <rlgl.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <filesystem>

namespace bonus_assets
{
using Type = tanks3d::game::BonusType;
using Pickup = tanks3d::game::Pickup;
using tanks3d::game::kBandageWeight;
using tanks3d::game::kClassicPickupTypeCount;
using tanks3d::game::kPickupFastBlinkStart;
using tanks3d::game::kPickupLifetime;
using tanks3d::game::typeForWeightedSlot;
using tanks3d::game::weightedTypeSlotCount;

constexpr float kPickupModelScale = 1.53f;
constexpr float kPickupIconSize = 0.90f;

inline const char *name(Type type)
{
    switch (type)
    {
    case Type::Grenade: return "GRENADE";
    case Type::Helmet: return "HELMET";
    case Type::Clock: return "CLOCK";
    case Type::Shovel: return "SHOVEL";
    case Type::Tank: return "1-UP TANK";
    case Type::Star: return "STAR";
    case Type::Gun: return "MAX GUN";
    case Type::Boat: return "BOAT";
    case Type::Bandage: return "BANDAGE";
    case Type::Count: return "UNKNOWN";
    }
    return "UNKNOWN";
}

inline Color accent(Type type)
{
    static constexpr std::array<Color, static_cast<std::size_t>(Type::Count)> colors{{
        Color{255, 104, 55, 255}, Color{85, 205, 255, 255},
        Color{255, 218, 74, 255}, Color{174, 206, 224, 255},
        Color{87, 235, 126, 255}, Color{255, 221, 62, 255},
        Color{255, 145, 47, 255}, Color{74, 191, 255, 255},
        Color{42, 220, 190, 255}}};
    return colors[std::min(static_cast<std::size_t>(type), colors.size() - 1U)];
}

inline bool visible(const Pickup &pickup)
{
    const float interval = pickup.age < kPickupFastBlinkStart ? 0.35f : 0.175f;
    return static_cast<int>(pickup.age / interval) % 2 == 0;
}

inline Color shade(Color color, float amount)
{
    return Color{
        static_cast<unsigned char>(std::clamp(color.r * amount, 0.0f, 255.0f)),
        static_cast<unsigned char>(std::clamp(color.g * amount, 0.0f, 255.0f)),
        static_cast<unsigned char>(std::clamp(color.b * amount, 0.0f, 255.0f)),
        color.a};
}

inline Vector3 yawPoint(Vector3 center, Vector3 local, float yaw)
{
    const float cosine = std::cos(yaw);
    const float sine = std::sin(yaw);
    return {center.x + local.x * cosine + local.z * sine,
            center.y + local.y,
            center.z - local.x * sine + local.z * cosine};
}

inline void drawYawBox(Vector3 center, Vector3 size, float yaw, Color color)
{
    rlPushMatrix();
    rlTranslatef(center.x, center.y, center.z);
    rlRotatef(-yaw * RAD2DEG, 0.0f, 1.0f, 0.0f);
    DrawCubeV({0.0f, 0.0f, 0.0f}, size, color);
    rlPopMatrix();
}

inline void drawEllipsoid(Vector3 center, Vector3 radii, Color color,
                          float yaw = 0.0f)
{
    rlPushMatrix();
    rlTranslatef(center.x, center.y, center.z);
    rlRotatef(-yaw * RAD2DEG, 0.0f, 1.0f, 0.0f);
    rlScalef(radii.x, radii.y, radii.z);
    DrawSphereEx({0.0f, 0.0f, 0.0f}, 1.0f, 7, 10, color);
    rlPopMatrix();
}

inline void drawThickStar(Vector3 center, float yaw, float outerRadius,
                          float innerRadius, float halfDepth, Color face,
                          Color edge)
{
    constexpr float pi = 3.14159265358979323846f;
    std::array<Vector3, 10> front{};
    std::array<Vector3, 10> back{};
    for (int point = 0; point < 10; ++point)
    {
        const float angle = pi * 0.5f + point * pi / 5.0f;
        const float radius = point % 2 == 0 ? outerRadius : innerRadius;
        const Vector3 local{std::cos(angle) * radius,
                            std::sin(angle) * radius, -halfDepth};
        front[point] = yawPoint(center, local, yaw);
        back[point] = yawPoint(center, {local.x, local.y, halfDepth}, yaw);
    }
    const Vector3 frontCenter = yawPoint(center, {0.0f, 0.0f, -halfDepth}, yaw);
    const Vector3 backCenter = yawPoint(center, {0.0f, 0.0f, halfDepth}, yaw);
    for (int point = 0; point < 10; ++point)
    {
        const int next = (point + 1) % 10;
        DrawTriangle3D(frontCenter, front[point], front[next], face);
        DrawTriangle3D(backCenter, back[next], back[point], shade(face, 0.67f));
        DrawTriangle3D(front[point], back[point], back[next], edge);
        DrawTriangle3D(front[point], back[next], front[next], edge);
    }
}

inline void drawGrenadeModel(Vector3 center, float yaw, Color glow)
{
    const Color olive{85, 111, 57, 255};
    drawEllipsoid({center.x, center.y - 0.01f, center.z},
                  {0.22f, 0.27f, 0.20f}, olive, yaw);
    for (int band = -2; band <= 2; ++band)
    {
        const float latitude = band * 0.085f;
        const float width = 0.17f + (2 - std::abs(band)) * 0.018f;
        const Vector3 start = yawPoint(center, {-width, latitude, 0.0f}, yaw);
        const Vector3 end = yawPoint(center, {width, latitude, 0.0f}, yaw);
        DrawCylinderEx(start, end, 0.014f, 0.014f, 6, shade(olive, 0.54f));
    }
    for (int rib = -2; rib <= 2; ++rib)
        DrawCylinderEx(yawPoint(center, {rib * 0.074f, -0.20f, 0.0f}, yaw),
                       yawPoint(center, {rib * 0.074f, 0.20f, 0.0f}, yaw),
                       0.012f, 0.012f, 6, shade(olive, 0.62f));
    DrawCylinderEx({center.x, center.y + 0.22f, center.z},
                   {center.x, center.y + 0.34f, center.z},
                   0.075f, 0.068f, 8, Color{70, 75, 61, 255});
    drawYawBox(yawPoint(center, {0.055f, 0.37f, 0.0f}, yaw),
               {0.18f, 0.075f, 0.11f}, yaw, Color{57, 62, 57, 255});
    drawYawBox(yawPoint(center, {0.13f, 0.31f, 0.0f}, yaw),
               {0.055f, 0.25f, 0.045f}, yaw - 0.24f,
               Color{177, 165, 102, 255});
    const Vector3 ringCenter = yawPoint(center, {-0.13f, 0.39f, 0.0f}, yaw);
    DrawSphere(ringCenter, 0.046f, glow);
    DrawCylinderWires(ringCenter, 0.075f, 0.075f, 0.018f, 12,
                      Color{235, 218, 132, 255});
}

inline void drawHelmetModel(Vector3 center, float yaw, Color glow)
{
    const Color shell{71, 119, 151, 255};
    DrawCylinderEx({center.x, center.y - 0.20f, center.z},
                   {center.x, center.y - 0.13f, center.z},
                   0.31f, 0.31f, 14, shade(shell, 0.72f));
    drawEllipsoid({center.x, center.y, center.z}, {0.27f, 0.25f, 0.25f},
                  shell, yaw);
    drawEllipsoid(yawPoint(center, {-0.075f, 0.07f, -0.15f}, yaw),
                  {0.12f, 0.12f, 0.035f}, Color{124, 184, 211, 255}, yaw);
    drawYawBox(yawPoint(center, {0.0f, -0.08f, -0.235f}, yaw),
               {0.43f, 0.15f, 0.075f}, yaw, Color{30, 45, 56, 255});
    DrawCylinderEx({center.x, center.y + 0.19f, center.z},
                   {center.x, center.y + 0.225f, center.z},
                   0.12f, 0.10f, 10, shade(glow, 0.82f));
    DrawCylinderEx(yawPoint(center, {-0.18f, -0.10f, 0.0f}, yaw),
                   yawPoint(center, {-0.13f, -0.34f, 0.0f}, yaw),
                   0.018f, 0.014f, 6, Color{201, 163, 83, 255});
    DrawCylinderEx(yawPoint(center, {0.18f, -0.10f, 0.0f}, yaw),
                   yawPoint(center, {0.13f, -0.34f, 0.0f}, yaw),
                   0.018f, 0.014f, 6, Color{201, 163, 83, 255});
    DrawCylinderEx(yawPoint(center, {-0.13f, -0.34f, 0.0f}, yaw),
                   yawPoint(center, {0.13f, -0.34f, 0.0f}, yaw),
                   0.015f, 0.015f, 6, shade(shell, 0.52f));
}

inline void drawClockModel(Vector3 center, float yaw, Color glow)
{
    const Vector3 rear = yawPoint(center, {0.0f, 0.0f, 0.085f}, yaw);
    const Vector3 face = yawPoint(center, {0.0f, 0.0f, -0.105f}, yaw);
    DrawCylinderEx(rear, face, 0.285f, 0.285f, 16,
                   Color{183, 126, 42, 255});
    DrawCylinderEx(face, yawPoint(center, {0.0f, 0.0f, -0.123f}, yaw),
                   0.235f, 0.235f, 16, Color{242, 234, 198, 255});
    for (int mark = 0; mark < 12; ++mark)
    {
        const float angle = mark * 2.0f * 3.14159265358979323846f / 12.0f;
        const Vector3 tick = yawPoint(center,
            {std::sin(angle) * 0.195f, std::cos(angle) * 0.195f, -0.150f}, yaw);
        DrawSphere(tick, mark % 3 == 0 ? 0.018f : 0.011f,
                   Color{60, 54, 46, 255});
    }
    const Vector3 handRoot = yawPoint(center, {0.0f, 0.0f, -0.145f}, yaw);
    DrawCylinderEx(handRoot, yawPoint(center, {0.0f, 0.16f, -0.145f}, yaw),
                   0.014f, 0.010f, 6, Color{44, 50, 53, 255});
    DrawCylinderEx(handRoot, yawPoint(center, {0.13f, -0.055f, -0.145f}, yaw),
                   0.014f, 0.008f, 6, Color{44, 50, 53, 255});
    DrawSphere(handRoot, 0.025f, glow);
    DrawCylinderEx({center.x, center.y + 0.29f, center.z},
                   {center.x, center.y + 0.35f, center.z},
                   0.075f, 0.055f, 8, Color{183, 126, 42, 255});
    for (float side : {-0.16f, 0.16f})
    {
        DrawSphere(yawPoint(center, {side, 0.275f, 0.0f}, yaw), 0.085f,
                   Color{211, 151, 48, 255});
        DrawCylinderEx(yawPoint(center, {side, 0.24f, 0.0f}, yaw),
                       yawPoint(center, {side * 1.18f, 0.34f, 0.0f}, yaw),
                       0.018f, 0.014f, 6, shade(glow, 0.72f));
    }
    for (float side : {-0.16f, 0.16f})
        DrawCylinderEx(yawPoint(center, {side, -0.22f, 0.0f}, yaw),
                       yawPoint(center, {side * 1.22f, -0.34f, 0.0f}, yaw),
                       0.026f, 0.018f, 6, Color{119, 76, 34, 255});
}

inline void drawShovelModel(Vector3 center, float yaw, Color glow)
{
    const Vector3 handleBottom = yawPoint(center, {0.0f, -0.19f, 0.0f}, yaw);
    const Vector3 handleTop = yawPoint(center, {0.0f, 0.31f, 0.0f}, yaw);
    DrawCylinderEx(handleBottom, handleTop, 0.027f, 0.027f, 7,
                   Color{137, 82, 39, 255});
    drawYawBox(yawPoint(center, {-0.09f, 0.35f, 0.0f}, yaw),
               {0.045f, 0.21f, 0.06f}, yaw, Color{137, 82, 39, 255});
    drawYawBox(yawPoint(center, {0.09f, 0.35f, 0.0f}, yaw),
               {0.045f, 0.21f, 0.06f}, yaw, Color{137, 82, 39, 255});
    drawYawBox(yawPoint(center, {0.0f, 0.455f, 0.0f}, yaw),
               {0.22f, 0.045f, 0.06f}, yaw, Color{164, 103, 49, 255});
    drawYawBox(yawPoint(center, {0.0f, -0.27f, 0.0f}, yaw),
               {0.27f, 0.20f, 0.075f}, yaw, shade(glow, 0.76f));
    DrawCylinderEx(yawPoint(center, {-0.13f, -0.37f, 0.0f}, yaw),
                   yawPoint(center, {0.13f, -0.37f, 0.0f}, yaw),
                   0.037f, 0.037f, 6, Color{74, 90, 99, 255});
    drawYawBox(yawPoint(center, {0.0f, -0.26f, -0.045f}, yaw),
               {0.055f, 0.18f, 0.025f}, yaw,
               Color{224, 233, 235, 255});
    DrawCylinderEx(yawPoint(center, {-0.11f, -0.18f, -0.048f}, yaw),
                   yawPoint(center, {0.11f, -0.18f, -0.048f}, yaw),
                   0.012f, 0.012f, 5, shade(glow, 0.95f));
}

inline void drawTankModel(Vector3 center, float yaw, Color glow)
{
    const Color track{42, 49, 48, 255};
    for (float side : {-0.22f, 0.22f})
    {
        drawYawBox(yawPoint(center, {side, -0.16f, 0.0f}, yaw),
                   {0.14f, 0.20f, 0.49f}, yaw, track);
        for (int block = -3; block <= 3; ++block)
            drawYawBox(yawPoint(center, {side, -0.16f, block * 0.071f}, yaw),
                       {0.17f, 0.225f, 0.058f}, yaw,
                       block % 2 == 0 ? Color{50, 59, 56, 255}
                                      : Color{37, 43, 42, 255});
        for (float z : {-0.16f, 0.0f, 0.16f})
        {
            const Vector3 near = yawPoint(center, {side - 0.091f, -0.16f, z}, yaw);
            const Vector3 far = yawPoint(center, {side + 0.091f, -0.16f, z}, yaw);
            DrawCylinderEx(near, far, 0.067f, 0.067f, 10,
                           Color{138, 145, 123, 255});
            DrawCylinderEx(near, far, 0.025f, 0.025f, 8,
                           Color{43, 48, 45, 255});
        }
    }
    drawYawBox(yawPoint(center, {0.0f, -0.08f, 0.0f}, yaw),
               {0.35f, 0.20f, 0.43f}, yaw, Color{51, 130, 72, 255});
    drawYawBox(yawPoint(center, {0.0f, 0.015f, -0.045f}, yaw),
               {0.32f, 0.10f, 0.34f}, yaw, Color{76, 182, 99, 255});
    drawEllipsoid(yawPoint(center, {0.0f, 0.10f, -0.02f}, yaw),
                  {0.23f, 0.19f, 0.21f}, Color{70, 178, 94, 255}, yaw);
    drawEllipsoid(yawPoint(center, {-0.075f, 0.16f, -0.17f}, yaw),
                  {0.075f, 0.055f, 0.025f}, Color{170, 241, 153, 255}, yaw);
    DrawCylinderEx(yawPoint(center, {0.0f, 0.12f, -0.12f}, yaw),
                   yawPoint(center, {0.0f, 0.12f, -0.39f}, yaw),
                   0.085f, 0.065f, 10, shade(glow, 0.82f));
    DrawCylinderEx(yawPoint(center, {0.0f, 0.12f, -0.38f}, yaw),
                   yawPoint(center, {0.0f, 0.12f, -0.47f}, yaw),
                   0.105f, 0.105f, 10, Color{48, 66, 52, 255});
    DrawCylinderEx(yawPoint(center, {0.14f, 0.22f, 0.06f}, yaw),
                   yawPoint(center, {0.16f, 0.48f, 0.08f}, yaw),
                   0.010f, 0.006f, 5, Color{226, 219, 167, 255});
    DrawSphere(yawPoint(center, {0.16f, 0.49f, 0.08f}, yaw), 0.018f, glow);
}

inline void drawStarModel(Vector3 center, float yaw, Color glow)
{
    drawThickStar(center, yaw, 0.36f, 0.155f, 0.075f,
                  Color{255, 231, 62, 255}, Color{161, 91, 23, 255});
    drawThickStar(yawPoint(center, {-0.025f, 0.035f, -0.081f}, yaw), yaw,
                  0.245f, 0.105f, 0.008f, Color{255, 249, 157, 255},
                  shade(glow, 0.90f));
    DrawSphere(yawPoint(center, {-0.07f, 0.13f, -0.105f}, yaw), 0.032f,
               RAYWHITE);
}

inline void drawGunModel(Vector3 center, float yaw, Color glow)
{
    drawYawBox(yawPoint(center, {0.0f, 0.03f, 0.0f}, yaw),
               {0.25f, 0.21f, 0.31f}, yaw, Color{63, 76, 83, 255});
    drawYawBox(yawPoint(center, {0.0f, 0.085f, -0.05f}, yaw),
               {0.19f, 0.08f, 0.32f}, yaw, Color{119, 136, 137, 255});
    DrawCylinderEx(yawPoint(center, {0.0f, 0.08f, -0.10f}, yaw),
                   yawPoint(center, {0.0f, 0.08f, -0.43f}, yaw),
                   0.095f, 0.065f, 9, glow);
    for (float z : {-0.20f, -0.29f, -0.38f})
        DrawCylinderEx(yawPoint(center, {0.0f, 0.08f, z}, yaw),
                       yawPoint(center, {0.0f, 0.08f, z - 0.025f}, yaw),
                       0.104f, 0.104f, 10, Color{49, 56, 58, 255});
    DrawCylinderEx(yawPoint(center, {0.0f, 0.08f, -0.43f}, yaw),
                   yawPoint(center, {0.0f, 0.08f, -0.51f}, yaw),
                   0.12f, 0.12f, 9, Color{45, 49, 50, 255});
    drawYawBox(yawPoint(center, {0.0f, -0.19f, 0.08f}, yaw),
               {0.13f, 0.30f, 0.15f}, yaw, Color{101, 58, 34, 255});
    DrawCylinderEx(yawPoint(center, {-0.15f, -0.01f, 0.05f}, yaw),
                   yawPoint(center, {0.15f, -0.01f, 0.05f}, yaw),
                   0.14f, 0.14f, 12, Color{45, 49, 50, 255});
    DrawCylinderEx(yawPoint(center, {-0.158f, -0.01f, 0.05f}, yaw),
                   yawPoint(center, {-0.168f, -0.01f, 0.05f}, yaw),
                   0.095f, 0.095f, 12, Color{222, 125, 42, 255});
    drawYawBox(yawPoint(center, {-0.13f, -0.20f, -0.10f}, yaw),
               {0.085f, 0.26f, 0.10f}, yaw - 0.28f,
               Color{82, 48, 31, 255});
    DrawCylinderEx(yawPoint(center, {-0.11f, 0.18f, 0.02f}, yaw),
                   yawPoint(center, {0.11f, 0.18f, 0.02f}, yaw),
                   0.035f, 0.035f, 7, Color{222, 203, 116, 255});
}

inline Vector3 boatNormal(Vector3 a, Vector3 b, Vector3 c)
{
    const Vector3 u{b.x - a.x, b.y - a.y, b.z - a.z},
        v{c.x - a.x, c.y - a.y, c.z - a.z};
    const Vector3 n{u.y * v.z - u.z * v.y, u.z * v.x - u.x * v.z,
                    u.x * v.y - u.y * v.x};
    const float length = std::sqrt(n.x * n.x + n.y * n.y + n.z * n.z);
    return length > 0.000001f ? Vector3{n.x / length, n.y / length, n.z / length}
                             : Vector3{0, 1, 0};
}

inline void boatTriangle(Vector3 a, Vector3 b, Vector3 c, Color color)
{
    const Vector3 n = boatNormal(a, b, c);
    rlColor4ub(color.r, color.g, color.b, color.a);
    rlNormal3f(n.x, n.y, n.z);
    rlVertex3f(a.x, a.y, a.z);
    rlVertex3f(b.x, b.y, b.z);
    rlVertex3f(c.x, c.y, c.z);
}

template <std::size_t Rings, std::size_t Points>
inline void drawBoatCasting(Vector3 center, float yaw,
                            const std::array<std::array<Vector3, Points>, Rings> &rings,
                            const std::array<Color, Rings - 1> &paints, Color topPaint)
{
    // Pickups are intentionally unlit in the game. Author the broad warm top,
    // blue side and deep keel colors into the connected hull panels themselves.
    rlBegin(RL_TRIANGLES);
    for (std::size_t level = 0; level + 1 < Rings; ++level)
        for (std::size_t i = 0; i < Points; ++i)
        {
            const std::size_t j = (i + 1) % Points;
            const Vector3 n =
                boatNormal(rings[level][j], rings[level][i], rings[level + 1][i]);
            const Color paint = shade(paints[level], n.x > 0.1f   ? 0.83f
                                                     : n.z > 0.1f ? 0.91f
                                                                 : 1.0f);
            const auto a = yawPoint(center, rings[level][j], yaw),
                       b = yawPoint(center, rings[level][i], yaw);
            const auto c = yawPoint(center, rings[level + 1][i], yaw),
                       d = yawPoint(center, rings[level + 1][j], yaw);
            boatTriangle(a, b, c, paint);
            boatTriangle(a, c, d, paint);
        }
    for (std::size_t i = 1; i + 1 < Points; ++i)
    {
        boatTriangle(yawPoint(center, rings.back()[0], yaw),
                     yawPoint(center, rings.back()[i + 1], yaw),
                     yawPoint(center, rings.back()[i], yaw), topPaint);
        boatTriangle(yawPoint(center, rings.front()[0], yaw),
                     yawPoint(center, rings.front()[i], yaw),
                     yawPoint(center, rings.front()[i + 1], yaw),
                     Color{25, 49, 61, 255});
    }
    rlEnd();
}

inline void drawBoatModel(Vector3 center, float yaw, Color glow)
{
    const Color cream{227, 224, 192, 255}, ink{24, 56, 73, 255};
    // A full displacement hull, with a narrow keel, curved shoulders, raised
    // bow and one continuous cream gunwale. No intersecting ellipsoid/cone nose.
    constexpr std::array<Vector2, 10> plan{{{-0.05f, -0.47f},
                                            {0.05f, -0.47f},
                                            {0.20f, -0.34f},
                                            {0.29f, -0.13f},
                                            {0.27f, 0.31f},
                                            {0.19f, 0.40f},
                                            {-0.19f, 0.40f},
                                            {-0.27f, 0.31f},
                                            {-0.29f, -0.13f},
                                            {-0.20f, -0.34f}}};
    std::array<std::array<Vector3, 10>, 5> hull{};
    constexpr std::array<float, 5> heights{{-0.25f, -0.19f, -0.07f, 0.015f, 0.045f}};
    constexpr std::array<float, 5> widths{{0.45f, 0.75f, 1.0f, 0.96f, 1.0f}};
    constexpr std::array<float, 5> lengths{{0.72f, 0.91f, 1.0f, 0.985f, 1.0f}};
    for (std::size_t ring = 0; ring < hull.size(); ++ring)
        for (std::size_t i = 0; i < plan.size(); ++i)
        {
            const float bowRise =
                ring >= 2 ? std::max(0.0f, -plan[i].y - 0.20f) * 0.11f : 0.0f;
            hull[ring][i] = {plan[i].x * widths[ring], heights[ring] + bowRise,
                             plan[i].y * lengths[ring]};
        }
    drawBoatCasting(
        center, yaw, hull,
        std::array<Color, 4>{
            {{27, 63, 86, 255}, {33, 100, 139, 255}, {59, 143, 175, 255}, cream}},
        Color{95, 164, 177, 255});
    const auto section = [](float y, float w, float front, float rear, float bevel)
    {
        return std::array<Vector3, 8>{{{-w + bevel, y, front},
                                       {w - bevel, y, front},
                                       {w, y, front + bevel},
                                       {w, y, rear - bevel},
                                       {w - bevel, y, rear},
                                       {-w + bevel, y, rear},
                                       {-w, y, rear - bevel},
                                       {-w, y, front + bevel}}};
    };
    const std::array<std::array<Vector3, 8>, 4> cabin{
        {section(0.045f, 0.18f, -0.12f, 0.23f, 0.045f),
         section(0.24f, 0.155f, -0.025f, 0.215f, 0.04f),
         section(0.275f, 0.13f, 0.0f, 0.20f, 0.035f),
         section(0.305f, 0.145f, -0.022f, 0.225f, 0.035f)}};
    drawBoatCasting(
        center, yaw, cabin,
        std::array<Color, 3>{{cream, {247, 235, 197, 255}, {61, 108, 123, 255}}},
        Color{250, 237, 199, 255});
    const auto face = [&](Vector3 a, Vector3 b, Vector3 c, Vector3 d, Color color)
    {
        a = yawPoint(center, a, yaw);
        b = yawPoint(center, b, yaw);
        c = yawPoint(center, c, yaw);
        d = yawPoint(center, d, yaw);
        boatTriangle(a, b, c, color);
        boatTriangle(a, c, d, color);
    };
    const auto frontZ = [](float y)
    { return -0.12f + (y - 0.045f) / 0.195f * 0.095f - 0.004f; };
    rlBegin(RL_TRIANGLES);
    for (float side : {-1.0f, 1.0f})
    {
        const float left = side < 0 ? -0.11f : 0.015f, right = side < 0 ? -0.015f : 0.11f;
        face({left, 0.12f, frontZ(0.12f)}, {left, 0.205f, frontZ(0.205f)},
             {right, 0.205f, frontZ(0.205f)}, {right, 0.12f, frontZ(0.12f)}, ink);
        face({left + 0.01f, 0.188f, frontZ(0.188f) - 0.002f},
             {left + 0.01f, 0.20f, frontZ(0.20f) - 0.002f},
             {right - 0.01f, 0.20f, frontZ(0.20f) - 0.002f},
             {right - 0.01f, 0.188f, frontZ(0.188f) - 0.002f}, Color{126, 203, 211, 255});
        const auto sideX = [&](float y)
        { return side * (0.18f - (y - 0.045f) / 0.195f * 0.025f + 0.004f); };
        const Vector3 a{sideX(0.12f), 0.12f, -0.015f}, b{sideX(0.205f), 0.205f, -0.015f};
        const Vector3 c{sideX(0.205f), 0.205f, 0.145f}, d{sideX(0.12f), 0.12f, 0.145f};
        if (side > 0)
            face(a, b, c, d, ink);
        else
            face(d, c, b, a, ink);
        const Vector3 e{sideX(0.185f) + side * 0.002f, 0.185f, 0.01f};
        const Vector3 f{sideX(0.20f) + side * 0.002f, 0.20f, 0.01f};
        const Vector3 g{sideX(0.20f) + side * 0.002f, 0.20f, 0.12f};
        const Vector3 h{sideX(0.185f) + side * 0.002f, 0.185f, 0.12f};
        const Color glassLight{126, 203, 211, 255};
        if (side > 0)
            face(e, f, g, h, glassLight);
        else
            face(h, g, f, e, glassLight);
    }
    rlEnd();
    // One enlarged funnel replaces the old hair-thin mast and rail framework.
    const std::array<std::array<Vector3, 8>, 4> funnel{
        {section(0.046f, 0.056f, 0.265f, 0.375f, 0.018f),
         section(0.31f, 0.060f, 0.265f, 0.375f, 0.018f),
         section(0.35f, 0.073f, 0.253f, 0.387f, 0.025f),
         section(0.405f, 0.068f, 0.257f, 0.383f, 0.025f)}};
    drawBoatCasting(center, yaw, funnel,
                    std::array<Color, 3>{
                        {{205, 116, 64, 255}, {236, 172, 105, 255}, {52, 78, 87, 255}}},
                    ink);
    // A small open life ring is attached to the port hull, not painted over the
    // wheelhouse. The dark hole and four cream segments remain clear while rotating.
    rlBegin(RL_TRIANGLES);
    constexpr float pi = 3.14159265358979323846f;
    for (int i = 0; i < 12; ++i)
    {
        const float a = i * pi / 6, b = (i + 1) * pi / 6;
        const auto point = [](float angle, float radius)
        {
            return Vector3{-0.297f, -0.035f + std::cos(angle) * radius,
                           -0.035f + std::sin(angle) * radius};
        };
        const Color paint = i % 3 == 0 ? cream : Color{229, 139, 68, 255};
        face(point(b, 0.080f), point(a, 0.080f), point(a, 0.042f), point(b, 0.042f), paint);
    }
    rlEnd();
    // A restrained blue deck fitting carries the existing Boat accent.
    const std::array<std::array<Vector3, 8>, 2> deckFitting{
        {section(0.06f, 0.045f, -0.355f, -0.245f, 0.014f),
         section(0.095f, 0.045f, -0.355f, -0.245f, 0.014f)}};
    drawBoatCasting(center, yaw, deckFitting, std::array<Color, 1>{{shade(glow, 0.50f)}},
                    shade(glow, 0.66f));
}

inline void drawBandageModel(Vector3 center, float yaw, Color glow)
{
    const Color cloth{239, 233, 211, 255};
    const Color clothLight{255, 252, 237, 255};
    const Color clothShade{184, 174, 151, 255};
    const Color medicalTeal{34, 188, 174, 255};
    const Vector3 rollLeft = yawPoint(center, {-0.25f, 0.02f, 0.0f}, yaw);
    const Vector3 rollRight = yawPoint(center, {0.25f, 0.02f, 0.0f}, yaw);
    DrawCylinderEx(rollLeft, rollRight, 0.205f, 0.205f, 16, cloth);
    DrawCylinderEx(yawPoint(center, {-0.070f, 0.02f, 0.0f}, yaw),
                   yawPoint(center, {0.070f, 0.02f, 0.0f}, yaw),
                   0.211f, 0.211f, 16, medicalTeal);
    for (float side : {-0.255f, 0.255f})
    {
        const float inward = side < 0.0f ? 0.020f : -0.020f;
        const Vector3 outer = yawPoint(center, {side, 0.02f, 0.0f}, yaw);
        const Vector3 inner = yawPoint(center, {side + inward, 0.02f, 0.0f}, yaw);
        DrawCylinderEx(outer, inner, 0.155f, 0.155f, 16, clothLight);
        DrawCylinderEx(outer, inner, 0.082f, 0.082f, 14, clothShade);
        DrawCylinderEx(outer, inner, 0.035f, 0.035f, 12,
                       Color{116, 104, 87, 255});
    }

    // A loose folded tail makes the pickup read as fabric rather than a can.
    drawYawBox(yawPoint(center, {0.18f, -0.13f, 0.19f}, yaw),
               {0.20f, 0.045f, 0.42f}, yaw - 0.16f, clothLight);
    drawYawBox(yawPoint(center, {0.08f, -0.18f, 0.36f}, yaw),
               {0.19f, 0.040f, 0.25f}, yaw + 0.18f, cloth);
    for (int seam = -1; seam <= 1; ++seam)
    {
        drawYawBox(yawPoint(center, {0.08f + seam * 0.055f,
                                     -0.155f, 0.24f}, yaw),
                   {0.012f, 0.051f, 0.31f}, yaw - 0.10f,
                   shade(clothShade, 0.92f));
    }

    // A blue-green medical mark avoids restricted real-world aid emblems.
    drawYawBox(yawPoint(center, {0.0f, 0.02f, -0.205f}, yaw),
               {0.29f, 0.27f, 0.035f}, yaw, clothLight);
    drawYawBox(yawPoint(center, {0.0f, 0.02f, -0.228f}, yaw),
               {0.070f, 0.19f, 0.020f}, yaw, medicalTeal);
    drawYawBox(yawPoint(center, {0.0f, 0.02f, -0.229f}, yaw),
               {0.19f, 0.070f, 0.021f}, yaw, medicalTeal);
    DrawSphere(yawPoint(center, {-0.10f, 0.12f, -0.235f}, yaw),
               0.023f, ColorLerp(glow, WHITE, 0.62f));
}

inline void drawPickupModel(Type type, Vector3 center, float yaw, Color glow)
{
    switch (type)
    {
    case Type::Grenade: drawGrenadeModel(center, yaw, glow); break;
    case Type::Helmet: drawHelmetModel(center, yaw, glow); break;
    case Type::Clock: drawClockModel(center, yaw, glow); break;
    case Type::Shovel: drawShovelModel(center, yaw, glow); break;
    case Type::Tank: drawTankModel(center, yaw, glow); break;
    case Type::Star: drawStarModel(center, yaw, glow); break;
    case Type::Gun: drawGunModel(center, yaw, glow); break;
    case Type::Boat: drawBoatModel(center, yaw, glow); break;
    case Type::Bandage: drawBandageModel(center, yaw, glow); break;
    case Type::Count: DrawSphere(center, 0.22f, glow); break;
    }
}

constexpr int kIconResolution = 64;

inline void drawIconThickLine(Image &image, int startX, int startY,
                              int endX, int endY, int thickness, Color color)
{
    const int deltaX = endX - startX;
    const int deltaY = endY - startY;
    const int steps = std::max(std::abs(deltaX), std::abs(deltaY));
    const int radius = std::max(1, thickness / 2);
    if (steps == 0)
    {
        ImageDrawCircle(&image, startX, startY, radius, color);
        return;
    }
    for (int step = 0; step <= steps; ++step)
    {
        const float amount = static_cast<float>(step) /
                             static_cast<float>(steps);
        const int x = static_cast<int>(std::lround(startX + deltaX * amount));
        const int y = static_cast<int>(std::lround(startY + deltaY * amount));
        ImageDrawCircle(&image, x, y, radius, color);
    }
}

template <std::size_t PointCount>
inline void drawIconPolygon(Image &image,
                            const std::array<Vector2, PointCount> &points,
                            Color fill, Color outline)
{
    int minimumY = kIconResolution;
    int maximumY = 0;
    for (const Vector2 point : points)
    {
        minimumY = std::min(minimumY, static_cast<int>(std::floor(point.y)));
        maximumY = std::max(maximumY, static_cast<int>(std::ceil(point.y)));
    }

    for (int y = minimumY; y <= maximumY; ++y)
    {
        const float scanY = static_cast<float>(y) + 0.5f;
        std::array<float, PointCount> intersections{};
        std::size_t intersectionCount = 0;
        for (std::size_t index = 0; index < PointCount; ++index)
        {
            const Vector2 first = points[index];
            const Vector2 second = points[(index + 1U) % PointCount];
            if ((first.y <= scanY && second.y > scanY) ||
                (second.y <= scanY && first.y > scanY))
            {
                intersections[intersectionCount++] =
                    first.x + (scanY - first.y) *
                                  (second.x - first.x) /
                                  (second.y - first.y);
            }
        }
        std::sort(intersections.begin(),
                  intersections.begin() + intersectionCount);
        for (std::size_t index = 0; index + 1U < intersectionCount;
             index += 2U)
        {
            ImageDrawLine(&image,
                          static_cast<int>(std::ceil(intersections[index])), y,
                          static_cast<int>(std::floor(intersections[index + 1U])), y,
                          fill);
        }
    }

    for (std::size_t index = 0; index < PointCount; ++index)
    {
        const Vector2 first = points[index];
        const Vector2 second = points[(index + 1U) % PointCount];
        drawIconThickLine(image, static_cast<int>(std::lround(first.x)),
                          static_cast<int>(std::lround(first.y)),
                          static_cast<int>(std::lround(second.x)),
                          static_cast<int>(std::lround(second.y)), 3, outline);
    }
}

// A common enamel frame and deliberately broad, pixel-aligned color regions.
// The symbols use the same ink, warm highlights and cool recesses; the rim
// retains the established pickup accent without tinting the whole pictogram.
inline void drawIconPanel(Image &image, Color glow)
{
    const std::array<Vector2, 8> frame{
        {{8, 2}, {55, 2}, {61, 8}, {61, 55}, {55, 61}, {8, 61}, {2, 55}, {2, 8}}};
    const std::array<Vector2, 8> inset{
        {{10, 6}, {53, 6}, {57, 10}, {57, 53}, {53, 57}, {10, 57}, {6, 53}, {6, 10}}};
    drawIconPolygon(image, frame, Color{46, 67, 72, 255}, Color{10, 20, 26, 255});
    drawIconPolygon(image, inset, Color{21, 35, 43, 255}, shade(glow, 0.52f));
    ImageDrawRectangle(&image, 11, 4, 38, 2,
                       ColorLerp(glow, Color{216, 224, 203, 255}, 0.55f));
    ImageDrawRectangle(&image, 4, 11, 2, 34, Color{119, 154, 154, 255});
    ImageDrawRectangle(&image, 13, 59, 38, 2, Color{12, 26, 33, 255});
}

inline void drawGrenadeIcon(Image &image)
{
    const Color ink{19, 31, 33, 255}, olive{112, 139, 69, 255};
    const std::array<Vector2, 8> shell{{{25, 23},
                                        {37, 23},
                                        {43, 30},
                                        {44, 42},
                                        {37, 52},
                                        {24, 52},
                                        {17, 44},
                                        {18, 31}}};
    drawIconPolygon(image, shell, olive, ink);
    drawIconPolygon(
        image,
        std::array<Vector2, 5>{{{24, 26}, {30, 25}, {29, 47}, {24, 48}, {21, 41}}},
        Color{163, 185, 98, 255}, Color{163, 185, 98, 255});
    ImageDrawRectangle(&image, 20, 33, 22, 3, Color{56, 81, 47, 255});
    ImageDrawRectangle(&image, 20, 42, 21, 3, Color{56, 81, 47, 255});
    ImageDrawRectangle(&image, 32, 26, 3, 23, Color{56, 81, 47, 255});
    ImageDrawRectangle(&image, 26, 17, 12, 8, ink);
    ImageDrawRectangle(&image, 28, 18, 9, 5, Color{192, 190, 127, 255});
    drawIconThickLine(image, 35, 18, 43, 27, 4, Color{208, 156, 74, 255});
    drawIconThickLine(image, 43, 27, 45, 38, 4, Color{208, 156, 74, 255});
    ImageDrawCircle(&image, 44, 16, 7, ink);
    ImageDrawCircle(&image, 44, 16, 5, Color{229, 197, 114, 255});
    ImageDrawCircle(&image, 44, 16, 2, Color{21, 35, 43, 255});
}

inline void drawHelmetIcon(Image &image)
{
    const Color ink{19, 31, 38, 255}, shell{65, 135, 163, 255};
    const std::array<Vector2, 10> dome{{{13, 38},
                                        {15, 26},
                                        {21, 19},
                                        {28, 15},
                                        {36, 15},
                                        {45, 20},
                                        {50, 28},
                                        {51, 39},
                                        {44, 44},
                                        {20, 44}}};
    drawIconPolygon(image, dome, shell, ink);
    drawIconPolygon(
        image,
        std::array<Vector2, 5>{{{18, 28}, {23, 21}, {31, 18}, {35, 18}, {25, 29}}},
        Color{160, 211, 211, 255}, Color{160, 211, 211, 255});
    drawIconPolygon(image,
                    std::array<Vector2, 4>{{{43, 24}, {48, 30}, {48, 38}, {41, 39}}},
                    Color{36, 84, 113, 255}, Color{36, 84, 113, 255});
    ImageDrawRectangle(&image, 14, 36, 37, 7, ink);
    drawIconPolygon(image,
                    std::array<Vector2, 6>{
                        {{10, 40}, {52, 40}, {55, 45}, {49, 49}, {15, 49}, {9, 45}}},
                    Color{85, 166, 187, 255}, ink);
    ImageDrawRectangle(&image, 15, 42, 33, 3, Color{178, 220, 215, 255});
    ImageDrawRectangle(&image, 24, 49, 17, 3, Color{142, 111, 66, 255});
}

inline void drawClockIcon(Image &image)
{
    const Color ink{32, 37, 38, 255}, gold{207, 145, 54, 255};
    ImageDrawRectangle(&image, 27, 8, 11, 8, ink);
    ImageDrawRectangle(&image, 29, 10, 7, 5, Color{250, 216, 125, 255});
    ImageDrawCircle(&image, 32, 34, 22, ink);
    ImageDrawCircle(&image, 32, 34, 19, gold);
    ImageDrawCircle(&image, 31, 33, 16, Color{252, 237, 190, 255});
    drawIconThickLine(image, 20, 23, 25, 19, 2, Color{255, 248, 213, 255});
    for (int x : {19, 42})
        ImageDrawRectangle(&image, x, 32, 3, 4, ink);
    for (int y : {20, 44})
        ImageDrawRectangle(&image, 30, y, 4, 3, ink);
    drawIconThickLine(image, 32, 34, 32, 25, 3, ink);
    drawIconThickLine(image, 32, 34, 40, 38, 3, ink);
    ImageDrawRectangle(&image, 30, 32, 5, 5, Color{204, 102, 51, 255});
}

inline void drawShovelIcon(Image &image)
{
    const Color ink{22, 34, 39, 255};
    drawIconThickLine(image, 40, 24, 24, 44, 9, ink);
    drawIconThickLine(image, 40, 24, 24, 44, 6, Color{174, 115, 64, 255});
    drawIconThickLine(image, 38, 25, 26, 40, 2, Color{225, 173, 105, 255});
    const std::array<Vector2, 6> handle{
        {{37, 10}, {47, 11}, {53, 18}, {44, 29}, {36, 26}, {31, 18}}};
    drawIconPolygon(image, handle, Color{211, 160, 94, 255}, ink);
    drawIconPolygon(image,
                    std::array<Vector2, 4>{{{39, 16}, {44, 16}, {47, 19}, {42, 24}}},
                    Color{21, 35, 43, 255}, Color{21, 35, 43, 255});
    const std::array<Vector2, 6> blade{
        {{20, 32}, {35, 42}, {32, 49}, {23, 54}, {12, 50}, {11, 40}}};
    drawIconPolygon(image, blade, Color{118, 160, 176, 255}, ink);
    drawIconPolygon(image,
                    std::array<Vector2, 4>{{{20, 36}, {27, 40}, {21, 50}, {15, 47}}},
                    Color{224, 232, 211, 255}, Color{224, 232, 211, 255});
}

inline void drawTankIcon(Image &image)
{
    const Color ink{19, 31, 33, 255};
    ImageDrawCircle(&image, 18, 44, 10, ink);
    ImageDrawCircle(&image, 46, 44, 10, ink);
    ImageDrawRectangle(&image, 18, 34, 28, 20, ink);
    ImageDrawCircle(&image, 18, 44, 7, Color{76, 91, 81, 255});
    ImageDrawCircle(&image, 46, 44, 7, Color{76, 91, 81, 255});
    ImageDrawRectangle(&image, 18, 37, 28, 14, Color{76, 91, 81, 255});
    for (int x : {18, 32, 46})
    {
        ImageDrawCircle(&image, x, 44, 4, Color{170, 189, 140, 255});
        ImageDrawRectangle(&image, x - 1, 43, 3, 3, ink);
    }
    drawIconPolygon(image,
                    std::array<Vector2, 6>{
                        {{16, 32}, {43, 31}, {50, 36}, {48, 40}, {13, 40}, {11, 37}}},
                    Color{82, 157, 83, 255}, ink);
    drawIconPolygon(image,
                    std::array<Vector2, 8>{{{21, 17},
                                            {34, 15},
                                            {41, 20},
                                            {42, 30},
                                            {38, 34},
                                            {22, 34},
                                            {17, 29},
                                            {17, 22}}},
                    Color{103, 178, 92, 255}, ink);
    drawIconThickLine(image, 38, 24, 52, 22, 7, ink);
    drawIconThickLine(image, 39, 24, 51, 22, 3, Color{174, 202, 143, 255});
    ImageDrawRectangle(&image, 50, 19, 5, 7, Color{79, 98, 90, 255});
    ImageDrawRectangle(&image, 52, 21, 3, 3, ink);
    ImageDrawRectangle(&image, 23, 17, 10, 3, Color{201, 228, 153, 255});
    ImageDrawRectangle(&image, 23, 25, 9, 3, Color{30, 72, 62, 255});
}

inline void drawStarIcon(Image &image)
{
    constexpr float pi = 3.14159265358979323846f;
    std::array<Vector2, 10> points{};
    for (int i = 0; i < 10; ++i)
    {
        const float a = -pi * 0.5f + i * pi / 5;
        const float r = i % 2 == 0 ? 23.0f : 10.0f;
        points[i] = {32 + std::cos(a) * r, 33 + std::sin(a) * r};
    }
    drawIconPolygon(image, points, Color{238, 181, 53, 255}, Color{49, 43, 32, 255});
    for (int i = 0; i < 10; ++i)
    {
        const Vector2 a{32 + (points[i].x - 32) * 0.84f, 33 + (points[i].y - 33) * 0.84f};
        const auto p = points[(i + 1) % 10];
        const Vector2 b{32 + (p.x - 32) * 0.84f, 33 + (p.y - 33) * 0.84f};
        const Color face = i < 4 ? Color{255, 229, 123, 255} : Color{194, 122, 42, 255};
        drawIconPolygon(image, std::array<Vector2, 3>{{{32, 33}, a, b}}, face, face);
    }
    ImageDrawRectangle(&image, 28, 24, 4, 5, Color{255, 248, 185, 255});
}

inline void drawGunIcon(Image &image)
{
    const Color ink{21, 32, 38, 255};
    drawIconPolygon(image,
                    std::array<Vector2, 6>{
                        {{12, 23}, {34, 20}, {40, 25}, {39, 39}, {16, 39}, {11, 34}}},
                    Color{106, 137, 145, 255}, ink);
    drawIconPolygon(image,
                    std::array<Vector2, 4>{{{34, 35}, {43, 37}, {38, 53}, {28, 51}}},
                    Color{161, 98, 52, 255}, ink);
    ImageDrawRectangle(&image, 16, 24, 17, 4, Color{204, 221, 207, 255});
    drawIconThickLine(image, 36, 28, 53, 24, 9, ink);
    drawIconThickLine(image, 38, 28, 51, 25, 5, Color{210, 150, 66, 255});
    ImageDrawRectangle(&image, 49, 19, 8, 13, ink);
    ImageDrawRectangle(&image, 49, 21, 4, 8, Color{136, 159, 157, 255});
    ImageDrawCircle(&image, 22, 40, 10, ink);
    ImageDrawCircle(&image, 22, 40, 7, Color{191, 124, 54, 255});
    ImageDrawCircle(&image, 22, 40, 3, Color{58, 67, 64, 255});
    ImageDrawRectangle(&image, 17, 17, 13, 5, ink);
    ImageDrawRectangle(&image, 20, 17, 8, 3, Color{234, 201, 127, 255});
}

inline void drawBoatIcon(Image &image)
{
    const Color ink{19, 34, 42, 255}, cream{231, 226, 194, 255};
    // A compact river tug: raised bow, full hull, slanted wheelhouse and a
    // chunky funnel. The two water strokes make the water-travel meaning clear.
    ImageDrawRectangle(&image, 16, 12, 12, 19, ink);
    ImageDrawRectangle(&image, 19, 16, 6, 15, Color{203, 110, 57, 255});
    ImageDrawRectangle(&image, 17, 12, 10, 5, Color{82, 107, 112, 255});
    drawIconPolygon(
        image,
        std::array<Vector2, 5>{{{28, 20}, {40, 20}, {46, 30}, {45, 35}, {24, 35}}},
        cream, ink);
    ImageDrawRectangle(&image, 28, 24, 5, 7, Color{36, 89, 113, 255});
    drawIconPolygon(image,
                    std::array<Vector2, 4>{{{36, 24}, {39, 24}, {42, 30}, {36, 30}}},
                    Color{36, 89, 113, 255}, Color{36, 89, 113, 255});
    ImageDrawRectangle(&image, 27, 19, 14, 3, Color{255, 243, 207, 255});
    drawIconPolygon(image,
                    std::array<Vector2, 6>{
                        {{9, 34}, {55, 34}, {51, 44}, {43, 50}, {20, 50}, {13, 44}}},
                    Color{48, 130, 166, 255}, ink);
    drawIconPolygon(image,
                    std::array<Vector2, 4>{{{13, 40}, {49, 40}, {42, 47}, {20, 47}}},
                    Color{31, 85, 122, 255}, Color{31, 85, 122, 255});
    ImageDrawRectangle(&image, 12, 35, 40, 3, cream);
    ImageDrawCircle(&image, 23, 41, 6, ink);
    ImageDrawCircle(&image, 23, 41, 4, Color{228, 144, 73, 255});
    ImageDrawRectangle(&image, 21, 39, 4, 4, cream);
    ImageDrawRectangle(&image, 22, 40, 2, 2, Color{31, 85, 122, 255});
    ImageDrawRectangle(&image, 14, 53, 13, 3, Color{97, 188, 208, 255});
    ImageDrawRectangle(&image, 34, 53, 15, 3, Color{156, 221, 224, 255});
}

inline void drawBandageIcon(Image &image)
{
    const Color ink{23, 40, 44, 255}, cloth{220, 211, 176, 255},
        teal{56, 185, 164, 255};
    drawIconPolygon(image,
                    std::array<Vector2, 8>{{{13, 22},
                                            {49, 22},
                                            {55, 28},
                                            {55, 40},
                                            {49, 47},
                                            {13, 47},
                                            {8, 40},
                                            {8, 28}}},
                    cloth, ink);
    ImageDrawRectangle(&image, 14, 25, 34, 4, Color{255, 240, 201, 255});
    ImageDrawRectangle(&image, 13, 41, 37, 3, Color{169, 157, 125, 255});
    ImageDrawRectangle(&image, 28, 25, 8, 20, Color{37, 110, 105, 255});
    ImageDrawRectangle(&image, 23, 31, 18, 8, Color{37, 110, 105, 255});
    ImageDrawRectangle(&image, 29, 26, 6, 17, teal);
    ImageDrawRectangle(&image, 24, 32, 16, 5, teal);
    for (int x : {15, 46})
        for (int y : {33, 38})
            ImageDrawRectangle(&image, x, y, 3, 3, Color{127, 120, 100, 255});
}

inline Texture2D loadGeneratedIconTexture(Type type)
{
    Image image = GenImageColor(kIconResolution, kIconResolution, BLANK);
    drawIconPanel(image, accent(type));
    switch (type)
    {
    case Type::Grenade: drawGrenadeIcon(image); break;
    case Type::Helmet: drawHelmetIcon(image); break;
    case Type::Clock: drawClockIcon(image); break;
    case Type::Shovel: drawShovelIcon(image); break;
    case Type::Tank: drawTankIcon(image); break;
    case Type::Star: drawStarIcon(image); break;
    case Type::Gun: drawGunIcon(image); break;
    case Type::Boat: drawBoatIcon(image); break;
    case Type::Bandage: drawBandageIcon(image); break;
    case Type::Count: break;
    }
    const Texture2D texture = LoadTextureFromImage(image);
    UnloadImage(image);
    return texture;
}

class Assets
{
public:
    Assets() = default;
    Assets(const Assets &) = delete;
    Assets &operator=(const Assets &) = delete;

    void load(const std::filesystem::path &resourceRoot)
    {
        unload();
        (void)resourceRoot;
        for (std::size_t index = 0; index < textures_.size(); ++index)
        {
            textures_[index] =
                loadGeneratedIconTexture(static_cast<Type>(index));
            if (IsTextureValid(textures_[index]))
                SetTextureFilter(textures_[index], TEXTURE_FILTER_POINT);
        }
    }

    void unload()
    {
        for (Texture2D &texture : textures_)
        {
            if (IsTextureValid(texture))
                UnloadTexture(texture);
            texture = {};
        }
    }

    void draw(const Pickup &pickup, Camera3D camera, float time) const
    {
        if (!visible(pickup))
            return;

        const Color glow = accent(pickup.type);
        const float pulse = 0.5f + 0.5f * std::sin(time * 4.2f + pickup.position.x * 0.7f);
        const float bob = std::sin(time * 2.8f + pickup.position.z) * 0.075f;
        const float yaw = time * 0.72f + pickup.position.x * 0.19f;
        const Vector3 icon{pickup.position.x, 0.28f, pickup.position.z};
        const Vector3 model{pickup.position.x, 0.96f + bob, pickup.position.z};

        // A soft code-native contact shadow anchors the floating model without
        // adding it to the global shadow-map pass.
        DrawCircle3D({pickup.position.x, 0.025f, pickup.position.z},
                     0.43f + pulse * 0.035f, {1.0f, 0.0f, 0.0f}, 90.0f,
                     Color{6, 10, 12, 92});

        BeginBlendMode(BLEND_ADDITIVE);
        DrawCircle3D({pickup.position.x, 0.035f, pickup.position.z},
                     0.49f + pulse * 0.055f,
                     {1.0f, 0.0f, 0.0f}, 90.0f, Fade(glow, 0.48f));
        DrawCircle3D({pickup.position.x, 0.04f, pickup.position.z},
                     0.34f + pulse * 0.045f,
                     {1.0f, 0.0f, 0.0f}, 90.0f, Fade(RAYWHITE, 0.24f));
        EndBlendMode();

        // A code-native arcade badge identifies the pickup below its 3D model.
        // Point filtering keeps the generated 64x64 artwork crisp at a distance.
        const std::size_t index = static_cast<std::size_t>(pickup.type);
        if (index < textures_.size() && IsTextureValid(textures_[index]))
        {
            DrawBillboardPro(camera, textures_[index],
                             {0.0f, 0.0f,
                              static_cast<float>(textures_[index].width),
                              static_cast<float>(textures_[index].height)},
                             icon, {0.0f, 1.0f, 0.0f},
                             {kPickupIconSize, kPickupIconSize},
                             {kPickupIconSize * 0.5f,
                              kPickupIconSize * 0.5f},
                             0.0f, WHITE);
        }
        else
        {
            DrawCubeV(icon, {0.58f, 0.58f, 0.025f}, shade(glow, 0.72f));
        }

        rlPushMatrix();
        rlTranslatef(model.x, model.y, model.z);
        rlScalef(kPickupModelScale, kPickupModelScale, kPickupModelScale);
        rlTranslatef(-model.x, -model.y, -model.z);
        drawPickupModel(pickup.type, model, yaw, glow);
        rlPopMatrix();
    }

private:
    std::array<Texture2D, static_cast<std::size_t>(Type::Count)> textures_{};
};
} // namespace bonus_assets

#endif
