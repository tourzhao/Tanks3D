#ifndef TANKS3D_BASE_MODEL_H
#define TANKS3D_BASE_MODEL_H

#include "game/stage_map.h"

#include <raylib.h>
#include <rlgl.h>

#include <algorithm>
#include <array>
#include <cmath>

// Original procedural field headquarters. The command core stays inside the
// classic two-by-two base; eight low wall tiles follow simulation ownership.
namespace tanks3d::base_model
{
inline constexpr float kFoundationRadius = 0.92f;
inline constexpr float kCourtyardRadius = 0.76f;
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

inline void ruin(float length, float thickness, const Palette &p, int index)
{
    // Destroyed walls leave a low rubble footprint, never another standing
    // fragment across the simulation's now-open one-cell passage.
    box({0, 0.019f, 0}, {length*0.82f, 0.025f, thickness*0.82f}, p.shadow);
    for (int piece = 0; piece < 4; ++piece)
    {
        const float x = (piece & 1) == 0 ? -0.26f : 0.24f;
        const float z = (piece & 2) == 0 ? -0.23f : 0.27f;
        const float height = ((piece + index) & 1) == 0 ? 0.095f : 0.065f;
        armoredBlock({x*length, height*0.5f + 0.025f, z*thickness},
                     {length*0.23f, height, thickness*0.20f},
                     (piece & 1) == 0 ? p.wall : p.trim);
    }
    box({-length*0.02f, 0.048f, thickness*0.06f},
        {length*0.19f, 0.035f, thickness*0.16f}, p.wall);
}

inline void wallTile(float length, float thickness, const Palette &p,
                     bool steel, bool shadowPass, int health, int index)
{
    const Color brick = shadowPass ? WHITE : pigment({157, 79, 47, 255}, 6);
    const Color mortar = shadowPass ? WHITE : pigment({196, 155, 110, 255});
    const Color recess = shadowPass ? WHITE : pigment({72, 48, 35, 255});
    const Color body = steel ? p.wall : brick;
    const Color cap = steel ? p.trim : mortar;
    // All trim stays strictly inside the segment's footprint. Adjacent cells
    // meet without extending a foundation into any original map tile.
    armoredBlock({0, 0.255f, 0}, {length*0.96f, 0.47f, thickness*0.96f}, body);
    box({0, 0.493f, 0}, {length*0.97f, 0.05f, thickness*0.97f}, cap);
    if (steel)
        armoredBlock({0, 0.535f, 0},
                     {length*0.74f, 0.055f, thickness*0.74f}, p.metal);
    if (shadowPass)
        return;

    for (int face = 0; face < 4; ++face)
    {
        const float span = (face & 1) == 0 ? length : thickness;
        const float depth = (face & 1) == 0 ? thickness : length;
        const float front = depth*0.482f;
        rlPushMatrix();
        rlRotatef(face*90.0f, 0, 1, 0);
        if (steel)
        {
            box({0, 0.275f, front}, {span*0.51f, 0.25f, 0.008f}, p.metal);
            box({0, 0.275f, front + 0.005f},
                {span*0.33f, 0.026f, 0.004f}, p.shadow);
            for (float side : {-1.0f, 1.0f})
                for (float y : {0.19f, 0.36f})
                    box({side*span*0.21f, y, front + 0.005f},
                        {0.025f, 0.025f, 0.008f}, p.trim);
        }
        else
        {
            for (int course = 0; course < 3; ++course)
            {
                const float y = 0.095f + course*0.14f;
                if (course > 0)
                    box({0, y - 0.073f, front},
                        {span*0.88f, 0.013f, 0.006f}, mortar);
                for (int joint = 0; joint < 2; ++joint)
                {
                    const float x = span*((joint - 0.5f)*0.42f +
                                           ((course & 1) == 0 ? -0.07f : 0.07f));
                    box({x, y, front}, {0.013f, 0.12f, 0.006f}, mortar);
                }
            }
        }
        // Health 1..4 still occupies the entire collider. Damage is painted
        // cracks and missing surface finish; only zero health opens a gap.
        const int missing = game::kGovernmentWallMaximumHealth - health;
        for (int crack = 0; crack < missing; ++crack)
        {
            const float x = span*(0.22f - crack*0.17f);
            const float y = 0.33f - ((crack + index) & 1)*0.065f;
            const Color scar = steel ? p.shadow : recess;
            box({x, y, front + 0.009f}, {0.028f, 0.13f, 0.005f}, scar);
            box({x - 0.028f, y + 0.056f, front + 0.009f},
                {0.075f, 0.025f, 0.005f}, scar);
        }
        rlPopMatrix();
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
    Palette debrisPaint = p;
    if (!steel && !shadowPass)
    {
        debrisPaint.wall = pigment({157, 79, 47, 255}, 6);
        debrisPaint.trim = pigment({196, 155, 110, 255});
        debrisPaint.shadow = pigment({72, 48, 35, 255});
    }
    const auto center = game::kGovernmentBaseCenter;
    rlPushMatrix();
    rlTranslatef(center.x, 0, center.z);
    armoredBlock({0, 0.016f, 0},
                 {kFoundationRadius*2.0f, 0.06f, kFoundationRadius*2.0f}, p.shadow);
    box({0, 0.046f, 0},
        {kCourtyardRadius*2.0f, 0.012f, kCourtyardRadius*2.0f},
        shadowPass ? WHITE : pigment({132, 124, 90, 255}));
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
            ruin(segment.halfLength*2.0f, segment.halfThickness*2.0f,
                 debrisPaint, index);
        else
            wallTile(segment.halfLength*2.0f, segment.halfThickness*2.0f,
                     p, steel, shadowPass, health, index);
        rlPopMatrix();
    }
}
} // namespace tanks3d::base_model

#endif
