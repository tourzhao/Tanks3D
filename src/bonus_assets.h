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

inline void drawBoatModel(Vector3 center, float yaw, Color glow)
{
    drawEllipsoid(yawPoint(center, {0.0f, -0.13f, 0.0f}, yaw),
                  {0.30f, 0.15f, 0.43f}, Color{34, 105, 153, 255}, yaw);
    drawYawBox(yawPoint(center, {0.0f, -0.06f, 0.06f}, yaw),
               {0.47f, 0.11f, 0.52f}, yaw, Color{65, 165, 205, 255});
    drawYawBox(yawPoint(center, {-0.255f, 0.0f, 0.065f}, yaw),
               {0.035f, 0.14f, 0.54f}, yaw, Color{225, 237, 224, 255});
    drawYawBox(yawPoint(center, {0.255f, 0.0f, 0.065f}, yaw),
               {0.035f, 0.14f, 0.54f}, yaw, Color{225, 237, 224, 255});
    DrawCylinderEx(yawPoint(center, {0.0f, -0.04f, -0.20f}, yaw),
                   yawPoint(center, {0.0f, -0.04f, -0.48f}, yaw),
                   0.20f, 0.025f, 8, Color{71, 183, 219, 255});
    drawYawBox(yawPoint(center, {0.0f, 0.08f, 0.07f}, yaw),
               {0.27f, 0.18f, 0.23f}, yaw, Color{223, 231, 218, 255});
    drawYawBox(yawPoint(center, {0.0f, 0.10f, -0.06f}, yaw),
               {0.21f, 0.10f, 0.025f}, yaw, Color{27, 71, 92, 255});
    drawYawBox(yawPoint(center, {0.0f, 0.20f, 0.08f}, yaw),
               {0.20f, 0.035f, 0.20f}, yaw, Color{31, 93, 126, 255});
    DrawCylinderEx(yawPoint(center, {0.0f, 0.17f, 0.06f}, yaw),
                   yawPoint(center, {0.0f, 0.42f, 0.06f}, yaw),
                   0.014f, 0.010f, 6, Color{218, 222, 203, 255});
    DrawCylinderEx(yawPoint(center, {-0.20f, 0.09f, 0.14f}, yaw),
                   yawPoint(center, {-0.20f, 0.24f, 0.14f}, yaw),
                   0.009f, 0.009f, 5, Color{225, 231, 207, 255});
    DrawCylinderEx(yawPoint(center, {0.20f, 0.09f, 0.14f}, yaw),
                   yawPoint(center, {0.20f, 0.24f, 0.14f}, yaw),
                   0.009f, 0.009f, 5, Color{225, 231, 207, 255});
    DrawCylinderEx(yawPoint(center, {-0.20f, 0.24f, 0.14f}, yaw),
                   yawPoint(center, {0.20f, 0.24f, 0.14f}, yaw),
                   0.009f, 0.009f, 5, glow);
    DrawSphere(yawPoint(center, {0.0f, 0.43f, 0.06f}, yaw), 0.022f, glow);
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

inline void drawIconPanel(Image &image, Color glow)
{
    const Color frame{5, 11, 16, 242};
    const Color panel{18, 29, 37, 250};
    ImageDrawRectangle(&image, 2, 3, 60, 59, Color{0, 0, 0, 100});
    ImageDrawRectangle(&image, 2, 1, 60, 59, frame);
    ImageDrawRectangle(&image, 4, 3, 56, 55, shade(glow, 0.54f));
    ImageDrawRectangle(&image, 7, 6, 50, 49, panel);
    ImageDrawRectangle(&image, 9, 8, 46, 3, Fade(glow, 0.72f));
    ImageDrawRectangle(&image, 9, 52, 46, 2, shade(glow, 0.34f));
}

inline void drawGrenadeIcon(Image &image)
{
    const Color edge{25, 32, 25, 255};
    const Color shell{103, 135, 70, 255};
    ImageDrawCircle(&image, 30, 37, 17, edge);
    ImageDrawCircle(&image, 30, 37, 14, shell);
    ImageDrawRectangle(&image, 24, 17, 13, 9, edge);
    ImageDrawRectangle(&image, 27, 18, 10, 7, Color{151, 160, 111, 255});
    for (int offset : {-8, 0, 8})
        drawIconThickLine(image, 17, 37 + offset, 43, 37 + offset, 2, edge);
    for (int offset : {-7, 0, 7})
        drawIconThickLine(image, 30 + offset, 24, 30 + offset, 50, 2, edge);
    drawIconThickLine(image, 37, 20, 45, 15, 4,
                      Color{205, 179, 91, 255});
    ImageDrawCircle(&image, 49, 16, 6, Color{205, 179, 91, 255});
    ImageDrawCircle(&image, 49, 16, 3, Color{18, 29, 37, 255});
    ImageDrawCircle(&image, 23, 29, 3, Color{184, 201, 125, 255});
}

inline void drawHelmetIcon(Image &image)
{
    const Color edge{20, 37, 48, 255};
    const Color shell{68, 157, 205, 255};
    ImageDrawCircle(&image, 32, 33, 20, edge);
    ImageDrawCircle(&image, 32, 33, 16, shell);
    ImageDrawRectangle(&image, 12, 34, 40, 13, edge);
    ImageDrawRectangle(&image, 15, 34, 34, 9, shell);
    ImageDrawRectangle(&image, 9, 43, 46, 6, edge);
    ImageDrawRectangle(&image, 13, 43, 38, 3,
                       Color{112, 204, 233, 255});
    ImageDrawRectangle(&image, 28, 17, 5, 25,
                       Color{42, 116, 157, 255});
    drawIconThickLine(image, 19, 26, 26, 20, 3,
                      Color{155, 226, 244, 255});
}

inline void drawClockIcon(Image &image)
{
    const Color edge{73, 49, 20, 255};
    const Color gold{226, 165, 47, 255};
    ImageDrawCircle(&image, 32, 33, 23, edge);
    ImageDrawCircle(&image, 32, 33, 19, gold);
    ImageDrawCircle(&image, 32, 33, 15, Color{246, 235, 190, 255});
    for (int marker = 0; marker < 12; ++marker)
    {
        const float angle = marker * 2.0f * 3.14159265358979323846f / 12.0f;
        const int x = static_cast<int>(std::lround(32.0f + std::sin(angle) * 12.0f));
        const int y = static_cast<int>(std::lround(33.0f - std::cos(angle) * 12.0f));
        ImageDrawCircle(&image, x, y, marker % 3 == 0 ? 2 : 1, edge);
    }
    drawIconThickLine(image, 32, 33, 32, 23, 3, edge);
    drawIconThickLine(image, 32, 33, 42, 38, 3, edge);
    ImageDrawCircle(&image, 32, 33, 3, Color{237, 105, 45, 255});
    ImageDrawRectangle(&image, 27, 8, 10, 5, edge);
    ImageDrawRectangle(&image, 29, 9, 6, 4, gold);
}

inline void drawShovelIcon(Image &image)
{
    const Color edge{38, 42, 42, 255};
    drawIconThickLine(image, 40, 16, 24, 45, 8, edge);
    drawIconThickLine(image, 40, 16, 24, 45, 4,
                      Color{145, 86, 42, 255});
    drawIconThickLine(image, 35, 15, 43, 11, 4,
                      Color{145, 86, 42, 255});
    drawIconThickLine(image, 43, 11, 48, 17, 4,
                      Color{145, 86, 42, 255});
    const std::array<Vector2, 4> blade{{
        {21.0f, 40.0f}, {31.0f, 46.0f}, {27.0f, 57.0f}, {13.0f, 51.0f}}};
    drawIconPolygon(image, blade, Color{172, 201, 214, 255}, edge);
    drawIconThickLine(image, 18, 48, 27, 52, 2,
                      Color{232, 246, 247, 255});
}

inline void drawTankIcon(Image &image)
{
    const Color edge{19, 31, 27, 255};
    const Color track{53, 62, 58, 255};
    ImageDrawCircle(&image, 18, 43, 11, edge);
    ImageDrawCircle(&image, 46, 43, 11, edge);
    ImageDrawRectangle(&image, 18, 32, 28, 22, edge);
    ImageDrawCircle(&image, 18, 43, 7, track);
    ImageDrawCircle(&image, 46, 43, 7, track);
    ImageDrawRectangle(&image, 18, 36, 28, 14, track);
    for (int x : {18, 27, 37, 46})
    {
        ImageDrawCircle(&image, x, 43, 4, Color{158, 170, 132, 255});
        ImageDrawCircle(&image, x, 43, 2, edge);
    }
    const std::array<Vector2, 4> hull{{
        {13.0f, 32.0f}, {48.0f, 30.0f}, {53.0f, 39.0f}, {11.0f, 39.0f}}};
    drawIconPolygon(image, hull, Color{64, 166, 89, 255}, edge);
    ImageDrawCircle(&image, 31, 26, 10, edge);
    ImageDrawCircle(&image, 31, 26, 7, Color{80, 195, 105, 255});
    drawIconThickLine(image, 36, 25, 55, 22, 7, edge);
    drawIconThickLine(image, 36, 25, 55, 22, 3,
                      Color{96, 208, 115, 255});
    ImageDrawCircle(&image, 27, 23, 2, Color{190, 247, 173, 255});
}

inline void drawStarIcon(Image &image)
{
    constexpr float pi = 3.14159265358979323846f;
    std::array<Vector2, 10> points{};
    for (int point = 0; point < 10; ++point)
    {
        const float radius = point % 2 == 0 ? 24.0f : 10.0f;
        const float angle = -pi * 0.5f + point * pi / 5.0f;
        points[point] = {32.0f + std::cos(angle) * radius,
                         33.0f + std::sin(angle) * radius};
    }
    drawIconPolygon(image, points, Color{255, 222, 54, 255},
                    Color{126, 70, 19, 255});
    ImageDrawCircle(&image, 26, 25, 4, Color{255, 250, 177, 255});
}

inline void drawGunIcon(Image &image)
{
    const Color edge{35, 38, 40, 255};
    ImageDrawRectangle(&image, 12, 24, 33, 18, edge);
    ImageDrawRectangle(&image, 16, 27, 28, 12,
                       Color{113, 133, 141, 255});
    drawIconThickLine(image, 41, 30, 57, 27, 9, edge);
    drawIconThickLine(image, 42, 30, 57, 27, 4,
                      Color{224, 139, 52, 255});
    ImageDrawRectangle(&image, 54, 22, 5, 12, edge);
    ImageDrawCircle(&image, 22, 41, 10, edge);
    ImageDrawCircle(&image, 22, 41, 6, Color{211, 126, 43, 255});
    const std::array<Vector2, 4> grip{{
        {31.0f, 39.0f}, {42.0f, 39.0f}, {38.0f, 56.0f}, {28.0f, 54.0f}}};
    drawIconPolygon(image, grip, Color{120, 69, 38, 255}, edge);
    ImageDrawRectangle(&image, 19, 28, 15, 3,
                       Color{192, 211, 211, 255});
}

inline void drawBoatIcon(Image &image)
{
    const Color edge{18, 55, 72, 255};
    const std::array<Vector2, 5> hull{{
        {8.0f, 36.0f}, {55.0f, 36.0f}, {48.0f, 51.0f},
        {20.0f, 54.0f}, {12.0f, 47.0f}}};
    drawIconPolygon(image, hull, Color{48, 161, 210, 255}, edge);
    ImageDrawRectangle(&image, 21, 24, 25, 13, edge);
    ImageDrawRectangle(&image, 24, 26, 19, 10,
                       Color{225, 237, 221, 255});
    ImageDrawRectangle(&image, 28, 28, 11, 7,
                       Color{46, 103, 130, 255});
    drawIconThickLine(image, 34, 24, 34, 12, 3,
                      Color{218, 224, 204, 255});
    drawIconThickLine(image, 34, 13, 46, 19, 2,
                      Color{255, 209, 60, 255});
    drawIconThickLine(image, 14, 57, 28, 57, 2,
                      Color{133, 221, 244, 255});
    drawIconThickLine(image, 36, 55, 52, 53, 2,
                      Color{133, 221, 244, 255});
}

inline void drawBandageIcon(Image &image)
{
    const Color edge{23, 54, 59, 255};
    const Color cloth{229, 218, 183, 255};
    const Color teal{38, 218, 190, 255};
    ImageDrawCircle(&image, 18, 34, 12, edge);
    ImageDrawCircle(&image, 46, 34, 12, edge);
    ImageDrawRectangle(&image, 18, 22, 28, 24, edge);
    ImageDrawCircle(&image, 18, 34, 9, cloth);
    ImageDrawCircle(&image, 46, 34, 9, cloth);
    ImageDrawRectangle(&image, 18, 25, 28, 18, cloth);
    ImageDrawRectangle(&image, 27, 25, 10, 18, Color{32, 99, 99, 255});
    ImageDrawRectangle(&image, 29, 27, 6, 14, teal);
    ImageDrawRectangle(&image, 25, 31, 14, 6, teal);
    for (int x : {14, 20, 44, 50})
    {
        ImageDrawCircle(&image, x, 31, 1, Color{153, 141, 112, 255});
        ImageDrawCircle(&image, x, 37, 1, Color{153, 141, 112, 255});
    }
    ImageDrawRectangle(&image, 13, 26, 10, 2,
                       Color{255, 245, 208, 255});
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
