#ifndef TANKS3D_BASE_MODEL_H
#define TANKS3D_BASE_MODEL_H

#include "game/stage_map.h"

#include <raylib.h>
#include <rlgl.h>

#include <algorithm>
#include <array>
#include <cmath>

// Original procedural field headquarters. All five standing/destroyed wings
// use the simulation's wall segments; decorative pieces never own collision.
namespace tanks3d::base_model
{
inline constexpr float kFoundationRadius = 2.62f;
inline constexpr float kCourtyardRadius = 1.02f;
inline constexpr float kCommandCoreFootprint = 1.36f;

namespace detail
{
inline constexpr float kPi = 3.14159265358979323846f;

inline Color pigment(Color color, unsigned char tag = 7)
{
    color.a = tag;
    return color;
}

inline void box(Vector3 p, Vector3 size, Color color)
{
    DrawCubeV(p, size, color);
}

inline void triangle(Vector3 a, Vector3 b, Vector3 c, Color color)
{
    const Vector3 u{b.x - a.x, b.y - a.y, b.z - a.z};
    const Vector3 v{c.x - a.x, c.y - a.y, c.z - a.z};
    Vector3 n{u.y*v.z - u.z*v.y, u.z*v.x - u.x*v.z,
              u.x*v.y - u.y*v.x};
    const float length = std::sqrt(n.x*n.x + n.y*n.y + n.z*n.z);
    if (length > 0.00001f)
    {
        n.x /= length;
        n.y /= length;
        n.z /= length;
    }
    rlColor4ub(color.r, color.g, color.b, color.a);
    rlNormal3f(n.x, n.y, n.z);
    rlVertex3f(a.x, a.y, a.z);
    rlVertex3f(b.x, b.y, b.z);
    rlVertex3f(c.x, c.y, c.z);
}

inline void quad(Vector3 a, Vector3 b, Vector3 c, Vector3 d, Color color)
{
    triangle(a, b, c, color);
    triangle(a, c, d, color);
}

// Chamfered corners and a sloped shoulder give steel/concrete real thickness
// without depending on a wireframe outline or overlapping scaled shells.
inline void armoredBlock(Vector3 center, Vector3 size, Color color)
{
    const float x = size.x*0.5f;
    const float z = size.z*0.5f;
    const float bevel = std::min(x, z)*0.26f;
    const float shoulder = std::min(size.y*0.22f, 0.13f);
    const std::array<Vector2, 8> contour{{
        {-x + bevel, -z}, {x - bevel, -z}, {x, -z + bevel},
        {x, z - bevel}, {x - bevel, z}, {-x + bevel, z},
        {-x, z - bevel}, {-x, -z + bevel}}};
    const float bottom = center.y - size.y*0.5f;
    const float top = center.y + size.y*0.5f;
    rlBegin(RL_TRIANGLES);
    for (std::size_t i = 0; i < contour.size(); ++i)
    {
        const Vector2 a = contour[i];
        const Vector2 b = contour[(i + 1U) % contour.size()];
        const Vector3 lowA{center.x + a.x, bottom, center.z + a.y};
        const Vector3 lowB{center.x + b.x, bottom, center.z + b.y};
        const Vector3 midA{lowA.x, top - shoulder, lowA.z};
        const Vector3 midB{lowB.x, top - shoulder, lowB.z};
        const Vector3 topA{center.x + a.x*0.90f, top, center.z + a.y*0.90f};
        const Vector3 topB{center.x + b.x*0.90f, top, center.z + b.y*0.90f};
        quad(lowB, lowA, midA, midB, color);
        quad(midB, midA, topA, topB, color);
        triangle({center.x, top, center.z}, topB, topA, color);
        triangle({center.x, bottom, center.z}, lowA, lowB, color);
    }
    rlEnd();
}

inline void roof(float length, float width, float eave, float rise, Color color)
{
    const float x = length*0.5f;
    const float z = width*0.5f;
    rlBegin(RL_TRIANGLES);
    quad({-x, eave, -z}, {-x, eave + rise, 0},
         {x, eave + rise, 0}, {x, eave, -z}, color);
    quad({x, eave, z}, {x, eave + rise, 0},
         {-x, eave + rise, 0}, {-x, eave, z}, color);
    triangle({-x, eave, z}, {-x, eave + rise, 0}, {-x, eave, -z}, color);
    triangle({x, eave, -z}, {x, eave + rise, 0}, {x, eave, z}, color);
    rlEnd();
}

struct Palette
{
    Color wall;
    Color trim;
    Color shadow;
    Color roof;
    Color metal;
    Color accent;
};

inline Palette palette(game::GovernmentBaseTheme theme, bool steel, bool alive,
                       bool shadowPass)
{
    if (shadowPass)
        return {WHITE, WHITE, WHITE, WHITE, WHITE, WHITE};
    if (!alive)
        return {pigment({91, 78, 61, 255}), pigment({127, 108, 82, 255}),
                pigment({42, 43, 38, 255}), pigment({59, 63, 57, 255}),
                pigment({87, 91, 81, 255}), pigment({123, 81, 48, 255})};
    if (steel)
        return {pigment({103, 134, 130, 255}), pigment({170, 183, 150, 255}),
                pigment({34, 53, 54, 255}), pigment({59, 83, 81, 255}),
                pigment({147, 165, 144, 255}), pigment({231, 168, 63, 255})};
    if (theme == game::GovernmentBaseTheme::SovietRingCastle)
        return {pigment({159, 83, 53, 255}, 6), pigment({222, 184, 123, 255}),
                pigment({76, 48, 35, 255}), pigment({67, 97, 74, 255}),
                pigment({110, 132, 94, 255}), pigment({211, 91, 45, 255})};
    if (theme == game::GovernmentBaseTheme::GermanParliament)
        return {pigment({182, 153, 109, 255}), pigment({225, 206, 160, 255}),
                pigment({86, 76, 55, 255}), pigment({73, 99, 92, 255}),
                pigment({113, 139, 121, 255}), pigment({170, 76, 45, 255})};
    return {pigment({182, 171, 123, 255}), pigment({231, 212, 162, 255}),
            pigment({81, 84, 59, 255}), pigment({80, 102, 81, 255}),
            pigment({132, 147, 106, 255}), pigment({218, 169, 67, 255})};
}

inline void star(Vector3 p, float radius, Color color)
{
    rlBegin(RL_TRIANGLES);
    for (int i = 0; i < 10; ++i)
    {
        const float a = i*kPi/5.0f;
        const float b = (i + 1)*kPi/5.0f;
        const float ra = (i & 1) == 0 ? radius : radius*0.44f;
        const float rb = (i & 1) == 0 ? radius*0.44f : radius;
        triangle(p, {p.x + std::sin(b)*rb, p.y + std::cos(b)*rb, p.z},
                 {p.x + std::sin(a)*ra, p.y + std::cos(a)*ra, p.z}, color);
    }
    rlEnd();
}

inline void ruin(float length, const Palette &p, int index)
{
    // Scatter stays inside the former wall. The center of every destroyed
    // wing remains visibly open, matching the route that shells can now take.
    for (float side : {-1.0f, 1.0f})
    {
        armoredBlock({side*length*0.36f, 0.20f, 0.0f},
                     {length*0.23f, 0.30f, 1.06f}, p.wall);
        box({side*length*0.37f, 0.37f, -0.12f}, {0.19f, 0.10f, 0.23f}, p.trim);
    }
    for (int piece = 0; piece < 5; ++piece)
    {
        rlPushMatrix();
        rlTranslatef((piece - 2)*length*0.13f, 0.05f,
                     ((piece + index) % 3 - 1)*0.24f);
        rlRotatef(piece*37.0f + index*11.0f, 0, 1, 0);
        box({0, 0.025f, 0}, {0.22f, 0.10f, 0.16f},
            (piece & 1) == 0 ? p.trim : p.shadow);
        rlPopMatrix();
    }
}

inline void wing(float length, const Palette &p, game::GovernmentBaseTheme theme,
                 bool steel, bool shadowPass, int health, int index)
{
    const bool redoubt = theme == game::GovernmentBaseTheme::SovietRingCastle;
    const bool depot = theme == game::GovernmentBaseTheme::UnitedStatesPentagon;
    const float top = redoubt ? 0.79f : 0.72f;
    const float face = game::kGovernmentWallThickness*0.5f;
    armoredBlock({0, 0.10f, 0}, {length + 0.05f, 0.18f, 1.24f}, p.shadow);
    armoredBlock({0, (top + 0.12f)*0.5f, 0},
                 {length, top - 0.12f, game::kGovernmentWallThickness}, p.wall);
    box({0, top - 0.025f, 0}, {length + 0.07f, 0.09f, 1.22f}, p.trim);
    if (redoubt)
    {
        box({0, top + 0.025f, 0}, {length - 0.12f, 0.07f, 1.00f}, p.roof);
        for (int i = -2; i <= 2; ++i)
            armoredBlock({i*length*0.19f, top + 0.12f, face - 0.03f},
                         {length*0.12f, 0.24f, 0.20f}, p.wall);
    }
    else
    {
        roof(length + 0.05f, 1.18f, top + 0.025f,
             depot ? 0.18f : 0.28f, p.roof);
        box({0, top + (depot ? 0.205f : 0.305f), 0},
            {length + 0.09f, 0.035f, 0.085f}, p.metal);
    }

    // Recessed bays are large enough to read as openings from the play camera.
    // Steel reinforcement closes those same bays with chunky bolted shutters.
    for (int bay = -1; bay <= 1; ++bay)
    {
        const float x = bay*length*0.285f;
        box({x, 0.42f, face + 0.008f}, {0.49f, 0.35f, 0.045f}, p.shadow);
        if (steel)
        {
            box({x, 0.43f, face + 0.042f}, {0.43f, 0.31f, 0.065f}, p.metal);
            box({x, 0.43f, face + 0.079f}, {0.33f, 0.045f, 0.012f}, p.shadow);
        }
        else
        {
            box({x, 0.43f, face + 0.035f}, {0.37f, 0.23f, 0.016f},
                shadowPass ? WHITE : pigment({43, 75, 71, 255}));
            box({x, 0.43f, face + 0.048f}, {0.027f, 0.25f, 0.020f}, p.trim);
            box({x, 0.44f, face + 0.048f}, {0.39f, 0.027f, 0.020f}, p.trim);
        }
        box({x, 0.245f, face + 0.055f}, {0.53f, 0.055f, 0.11f}, p.trim);
        box({x, 0.615f, face + 0.055f}, {0.54f, 0.055f, 0.11f}, p.trim);
        if (!shadowPass)
        {
            for (float side : {-1.0f, 1.0f})
                DrawSphereEx({x + side*0.195f, 0.54f, face + 0.089f},
                             0.024f, 4, 6, p.trim);
        }
    }
    for (float side : {-1.0f, 1.0f})
        armoredBlock({side*length*0.46f, 0.43f, face - 0.008f},
                     {0.17f, 0.67f, 0.19f}, p.trim);

    // A single oversized duct on alternating wings breaks the repetitive roof.
    if ((index & 1) == 0)
    {
        box({-length*0.25f, top + 0.13f, -0.16f}, {0.30f, 0.24f, 0.30f}, p.shadow);
        armoredBlock({-length*0.25f, top + 0.27f, -0.16f},
                     {0.39f, 0.16f, 0.38f}, p.metal);
        if (!shadowPass)
            for (int vent = -1; vent <= 1; ++vent)
                box({-length*0.25f, top + 0.24f + vent*0.04f, 0.036f},
                    {0.25f, 0.018f, 0.012f}, p.shadow);
    }
    if (!shadowPass)
    {
        // Broken plaster, exposed ochre bricks and short seam marks are
        // deterministic decoration, never an input to the gameplay RNG.
        for (int chip = 0; chip < 4; ++chip)
            box({-length*0.41f + chip*0.19f, 0.17f + (chip % 2)*0.045f,
                 face + 0.018f}, {0.13f, 0.033f, 0.026f}, p.accent);
        const int missing = game::kGovernmentWallMaximumHealth - health;
        for (int crack = 0; crack < missing; ++crack)
        {
            const float x = length*0.22f - crack*0.22f;
            box({x, 0.55f, face + 0.090f}, {0.08f, 0.23f, 0.017f}, p.shadow);
            box({x + 0.07f, 0.66f, face + 0.09f}, {0.19f, 0.05f, 0.018f}, p.shadow);
        }
    }
}

inline void commandCore(const Palette &p, game::GovernmentBaseTheme theme,
                        bool alive, bool shadowPass)
{
    armoredBlock({0, 0.17f, 0},
                 {kCommandCoreFootprint, 0.24f, kCommandCoreFootprint}, p.shadow);
    if (!alive)
    {
        armoredBlock({0.10f, 0.34f, 0}, {0.91f, 0.30f, 0.87f}, p.wall);
        rlPushMatrix();
        rlRotatef(24, 0, 0, 1);
        box({-0.25f, 0.46f, -0.04f}, {0.72f, 0.16f, 0.68f}, p.metal);
        rlPopMatrix();
        return;
    }
    armoredBlock({0, 0.55f, 0}, {1.09f, 0.70f, 1.00f}, p.wall);
    armoredBlock({0, 0.90f, 0}, {1.20f, 0.16f, 1.12f}, p.trim);
    box({0, 0.56f, 0.506f}, {0.73f, 0.38f, 0.044f}, p.shadow);
    box({0, 0.57f, 0.533f}, {0.64f, 0.28f, 0.021f}, p.roof);
    const Color badge = shadowPass ? WHITE : pigment({241, 202, 115, 255});
    if (!shadowPass)
    {
        if (theme == game::GovernmentBaseTheme::GermanParliament)
        {
            box({0, 0.57f, 0.55f}, {0.086f, 0.21f, 0.014f}, badge);
            box({0, 0.57f, 0.55f}, {0.21f, 0.086f, 0.014f}, badge);
        }
        else
            star({0, 0.57f, 0.55f}, 0.12f, badge);
    }
    // Nationally distinct roof silhouettes: a compact communications cupola,
    // redoubt watch turret, or oxidized copper command-room dome.
    if (theme == game::GovernmentBaseTheme::SovietRingCastle)
    {
        DrawCylinder({0, 0.98f, -0.08f}, 0.32f, 0.35f, 0.33f, 8, p.wall);
        DrawCylinder({0, 1.30f, -0.08f}, 0.035f, 0.44f, 0.35f, 8, p.roof);
        DrawSphereEx({0, 1.69f, -0.08f}, 0.055f, 5, 8, badge);
    }
    else if (theme == game::GovernmentBaseTheme::GermanParliament)
    {
        DrawCylinder({0, 0.98f, -0.06f}, 0.41f, 0.44f, 0.12f, 12, p.shadow);
        rlPushMatrix();
        rlTranslatef(0, 1.10f, -0.06f);
        rlScalef(0.40f, 0.30f, 0.37f);
        DrawSphereEx({0, 0, 0}, 1.0f, 8, 16, p.roof);
        rlPopMatrix();
        DrawCylinder({0, 1.37f, -0.06f}, 0.06f, 0.09f, 0.13f, 8, p.accent);
        if (!shadowPass)
            for (float side : {-1.0f, 1.0f})
                DrawCylinderEx({side*0.34f, 1.13f, -0.06f},
                               {side*0.09f, 1.38f, -0.06f}, 0.018f, 0.018f, 6, p.trim);
    }
    else
    {
        armoredBlock({0.10f, 1.10f, -0.06f}, {0.65f, 0.32f, 0.61f}, p.roof);
        box({0.10f, 1.11f, 0.247f}, {0.40f, 0.095f, 0.021f},
            shadowPass ? WHITE : pigment({49, 91, 87, 255}));
        DrawCylinderEx({0.10f, 1.25f, -0.06f}, {0.10f, 1.68f, -0.06f},
                       0.025f, 0.014f, 6, p.shadow);
        box({0.10f, 1.56f, -0.06f}, {0.39f, 0.022f, 0.032f}, p.metal);
        DrawSphereEx({0.10f, 1.70f, -0.06f}, 0.045f, 5, 8, badge);
    }
    // Offset exhaust and an exterior cabinet supply a readable asymmetry.
    DrawCylinderEx({-0.46f, 0.80f, -0.32f}, {-0.46f, 1.22f, -0.32f},
                   0.062f, 0.055f, 8, p.shadow);
    DrawCylinder({-0.46f, 1.22f, -0.32f}, 0.09f, 0.09f, 0.05f, 8, p.metal);
    armoredBlock({0.48f, 0.48f, -0.15f}, {0.18f, 0.34f, 0.32f}, p.roof);
}
} // namespace detail

inline void draw(const game::StageMap &map, bool alive, bool shadowPass)
{
    using namespace detail;
    const auto theme = map.governmentBaseTheme();
    const bool steel = alive && map.governmentSteelVisible();
    const Palette p = palette(theme, steel, alive, shadowPass);
    const auto center = game::kGovernmentBaseCenter;
    rlPushMatrix();
    rlTranslatef(center.x, 0, center.z);
    rlPushMatrix();
    rlRotatef(game::kGovernmentPentagonYaw*RAD2DEG, 0, 1, 0);
    DrawCylinder({0, -0.015f, 0}, kFoundationRadius, kFoundationRadius, 0.10f, 5, p.shadow);
    DrawCylinder({0, 0.086f, 0}, kCourtyardRadius, kCourtyardRadius, 0.035f, 5,
                 shadowPass ? WHITE : pigment({132, 124, 90, 255}));
    rlPopMatrix();
    commandCore(p, theme, alive, shadowPass);
    rlPopMatrix();

    for (int index = 0; index < game::kGovernmentWallCount; ++index)
    {
        const auto segment = game::governmentWallSegment(index);
        rlPushMatrix();
        rlTranslatef(segment.center.x, 0, segment.center.z);
        rlRotatef(-segment.yaw*RAD2DEG, 0, 1, 0);
        const int health = map.governmentWallHealth(index);
        if (health <= 0)
            ruin(segment.halfLength*2.0f, p, index);
        else
            wing(segment.halfLength*2.0f, p, theme, steel, shadowPass, health, index);
        rlPopMatrix();
    }
}
} // namespace tanks3d::base_model

#endif
