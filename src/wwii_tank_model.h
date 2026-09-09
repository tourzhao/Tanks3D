#ifndef TANKS3D_WWII_TANK_MODEL_H
#define TANKS3D_WWII_TANK_MODEL_H

#include "core/nation.h"

#include <raylib.h>
#include <rlgl.h>

#include <algorithm>
#include <array>
#include <cmath>

// Detailed, code-native interpretations of recognizable Second World War
// vehicles. Geometry is original rather than copied from manufacturer CAD;
// proportions and silhouettes deliberately follow the four 2D gameplay roles.
namespace wwii_tank_model
{
using Nation = tanks3d::core::Nation;
using tanks3d::core::nationName;

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
    PanzerIIIL
};

namespace detail
{
inline unsigned char channel(float value)
{
    return static_cast<unsigned char>(std::clamp(value, 0.0f, 255.0f));
}

inline Color shade(Color color, float factor)
{
    return {channel(color.r * factor), channel(color.g * factor),
            channel(color.b * factor), color.a};
}

inline Color mix(Color first, Color second, float amount)
{
    amount = std::clamp(amount, 0.0f, 1.0f);
    const float inverse = 1.0f - amount;
    return {channel(first.r * inverse + second.r * amount),
            channel(first.g * inverse + second.g * amount),
            channel(first.b * inverse + second.b * amount),
            channel(first.a * inverse + second.a * amount)};
}

inline Color material(Color color, unsigned char tag)
{
    color.a = tag;
    return color;
}

struct Palette
{
    Color paint;
    Color darkPaint;
    Color lightPaint;
    Color edge;
    Color rubber;
    Color steel;
    Color wheel;
    Color optic;
    Color canvas;
};

inline Palette palette(Color arcadeColor, bool enemy)
{
    // Warm muted armor, cool mechanical recesses and broad cream highlights
    // evoke hand-painted arcade machinery while keeping team colors readable.
    const Color militaryNeutral = enemy ? Color{86, 95, 89, 255}
                                        : Color{117, 126, 119, 255};
    const Color paint = mix(arcadeColor, militaryNeutral, enemy ? 0.40f : 0.12f);
    // Tags 9..13 select the vehicle-only cel-lighting path in SceneLighting;
    // world materials keep their original 1..8 PBR tags.
    // Military-cartoon contrast: broad panels remain plausible field paint,
    // while recesses and exposed edges separate cleanly at gameplay scale.
    return {material(paint, 9),
            material(shade(paint, enemy ? 0.34f : 0.48f), 9),
            material(mix(shade(paint, enemy ? 1.22f : 1.24f),
                         Color{211, 221, 204, 255}, enemy ? 0.18f : 0.12f), 9),
            material(shade(paint, 0.18f), 10),
            material(Color{12, 15, 14, 255}, 11),
            material(Color{55, 62, 61, 255}, 10),
            material(Color{94, 101, 88, 255}, 10),
            material(Color{42, 137, 154, 255}, 12),
            material(mix(paint, Color{67, 71, 57, 255}, 0.55f), 13)};
}

inline Color nationalPlayerPaint(Color identity, Nation nation)
{
    Color national = Color{128, 143, 135, 255};
    switch (nation)
    {
    case Nation::SovietUnion:
        national = Color{99, 126, 94, 255};
        break;
    case Nation::Germany:
        national = Color{151, 139, 110, 255};
        break;
    case Nation::UnitedStates:
    case Nation::Count:
        break;
    }
    // P1/P2 identity remains on lamps and trim, while the broad armor panels
    // finally carry a nation-readable field color.
    return mix(identity, national, 0.94f);
}

inline void box(Vector3 center, Vector3 size, Color color)
{
    DrawCubeV(center, size, color);
}

inline Vector3 subtract(Vector3 a, Vector3 b)
{
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

inline Vector3 cross(Vector3 a, Vector3 b)
{
    return {a.y * b.z - a.z * b.y,
            a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x};
}

inline Vector3 normalized(Vector3 value)
{
    const float length = std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
    if (length < 0.00001f)
        return {0.0f, 1.0f, 0.0f};
    return {value.x / length, value.y / length, value.z / length};
}

inline void emitTriangle(Vector3 a, Vector3 b, Vector3 c, Color color)
{
    const Vector3 normal = normalized(cross(subtract(b, a), subtract(c, a)));
    rlColor4ub(color.r, color.g, color.b, color.a);
    rlNormal3f(normal.x, normal.y, normal.z);
    rlVertex3f(a.x, a.y, a.z);
    rlVertex3f(b.x, b.y, b.z);
    rlVertex3f(c.x, c.y, c.z);
}

inline void emitQuad(Vector3 a, Vector3 b, Vector3 c, Vector3 d, Color color)
{
    emitTriangle(a, b, c, color);
    emitTriangle(a, c, d, color);
}

// Eight-vertex truncated box.  Different top and bottom footprints create the
// broad sloped glacis and faceted turrets that plain cubes cannot express.
inline void frustum(Vector3 center, float bottomWidth, float bottomLength,
                    float topWidth, float topLength, float height,
                    float topZOffset, Color color)
{
    const float y0 = center.y - height * 0.5f;
    const float y1 = center.y + height * 0.5f;
    const float bz0 = center.z - bottomLength * 0.5f;
    const float bz1 = center.z + bottomLength * 0.5f;
    const float tz0 = center.z + topZOffset - topLength * 0.5f;
    const float tz1 = center.z + topZOffset + topLength * 0.5f;
    const float bx = bottomWidth * 0.5f;
    const float tx = topWidth * 0.5f;
    const std::array<Vector3, 8> v{{
        {center.x - bx, y0, bz0}, {center.x + bx, y0, bz0},
        {center.x + bx, y0, bz1}, {center.x - bx, y0, bz1},
        {center.x - tx, y1, tz0}, {center.x + tx, y1, tz0},
        {center.x + tx, y1, tz1}, {center.x - tx, y1, tz1}}};

    rlBegin(RL_TRIANGLES);
    emitQuad(v[0], v[3], v[2], v[1], color);
    emitQuad(v[4], v[5], v[6], v[7], color);
    emitQuad(v[0], v[1], v[5], v[4], color);
    emitQuad(v[1], v[2], v[6], v[5], color);
    emitQuad(v[2], v[3], v[7], v[6], color);
    emitQuad(v[3], v[0], v[4], v[7], color);
    rlEnd();
}

// A plan-view chamfered frustum avoids the toy-like rectangular footprint of
// a cube while retaining the crisp rolled-plate construction of WWII hulls and
// German turrets. Local forward is -Z, matching every muzzle API below.
inline void chamferedFrustum(Vector3 center, float bottomWidth,
                            float bottomLength, float topWidth,
                            float topLength, float height, float topZOffset,
                            float frontChamfer, float rearChamfer, Color color)
{
    const auto ring = [&](float width, float length, float y, float zOffset) {
        const float halfWidth = width * 0.5f;
        const float front = center.z + zOffset - length * 0.5f;
        const float rear = center.z + zOffset + length * 0.5f;
        const float frontCut = std::min(width * frontChamfer, length * 0.22f);
        const float rearCut = std::min(width * rearChamfer, length * 0.18f);
        return std::array<Vector3, 8>{{
            {center.x - halfWidth + frontCut, y, front},
            {center.x + halfWidth - frontCut, y, front},
            {center.x + halfWidth, y, front + frontCut},
            {center.x + halfWidth, y, rear - rearCut},
            {center.x + halfWidth - rearCut, y, rear},
            {center.x - halfWidth + rearCut, y, rear},
            {center.x - halfWidth, y, rear - rearCut},
            {center.x - halfWidth, y, front + frontCut}}};
    };

    const auto bottom = ring(bottomWidth, bottomLength,
                             center.y - height * 0.5f, 0.0f);
    const auto top = ring(topWidth, topLength,
                          center.y + height * 0.5f, topZOffset);
    rlBegin(RL_TRIANGLES);
    for (std::size_t index = 1; index + 1 < bottom.size(); ++index)
    {
        emitTriangle(bottom[0], bottom[index], bottom[index + 1], color);
        emitTriangle(top[0], top[index + 1], top[index], color);
    }
    for (std::size_t index = 0; index < bottom.size(); ++index)
    {
        const std::size_t next = (index + 1U) % bottom.size();
        emitQuad(bottom[next], bottom[index], top[index], top[next], color);
    }
    rlEnd();

    // Upper hulls receive a low, rounded shoulder sunk into the rolled plate.
    // This produces the compact, barrel-chested arcade mass without erasing
    // the model-specific glacis or changing its logical footprint.
    if (center.y > 0.33f && center.y < 0.59f && bottomLength > 1.05f)
    {
        const Vector3 shoulderCenter{
            center.x,
            center.y + height * 0.40f,
            center.z + topZOffset + topLength * 0.10f};
        rlPushMatrix();
        rlTranslatef(shoulderCenter.x, shoulderCenter.y, shoulderCenter.z);
        rlScalef(topWidth * 0.46f, height * 0.34f, topLength * 0.30f);
        DrawSphereEx({0.0f, 0.0f, 0.0f}, 1.0f, 5, 12,
                     shade(color, 1.10f));
        rlPopMatrix();
    }

    const Color seam = shade(color, 0.44f);
    for (std::size_t index = 0; index < top.size(); ++index)
    {
        const std::size_t next = (index + 1U) % top.size();
        DrawCylinderEx(top[index], top[next], 0.013f, 0.013f,
                       5, seam);
    }
}

// A four-sided wedge with a genuinely inclined top plane. It sits over the
// upper hull so highlights describe the glacis angle instead of another box.
inline void slopedGlacis(float frontWidth, float rearWidth, float frontZ,
                         float rearZ, float frontY, float rearY,
                         float thickness, Color color)
{
    const std::array<Vector3, 4> top{{
        {-frontWidth * 0.5f, frontY, frontZ},
        { frontWidth * 0.5f, frontY, frontZ},
        { rearWidth * 0.5f, rearY, rearZ},
        {-rearWidth * 0.5f, rearY, rearZ}}};
    std::array<Vector3, 4> bottom = top;
    for (Vector3 &point : bottom)
        point.y -= thickness;

    rlBegin(RL_TRIANGLES);
    emitQuad(top[0], top[3], top[2], top[1], color);
    emitQuad(bottom[0], bottom[1], bottom[2], bottom[3], color);
    emitQuad(bottom[0], top[0], top[1], bottom[1], color);
    emitQuad(bottom[1], top[1], top[2], bottom[2], color);
    emitQuad(bottom[2], top[2], top[3], bottom[3], color);
    emitQuad(bottom[3], top[3], top[0], bottom[0], color);
    rlEnd();

    const Color seam = shade(color, 0.42f);
    for (std::size_t index = 0; index < top.size(); ++index)
    {
        const std::size_t next = (index + 1U) % top.size();
        DrawCylinderEx(top[index], top[next], 0.015f, 0.015f, 5, seam);
    }
}

// Flattened spheres give American and Soviet cast turrets continuous curved
// highlights. Segment counts remain deliberately modest to preserve a subtly
// faceted period-casting look and keep the procedural renderer lightweight.
inline void ellipsoid(Vector3 center, Vector3 radii, Color color,
                      int rings = 8, int slices = 16)
{
    radii.x *= 1.14f;
    radii.y *= 1.06f;
    radii.z *= 1.14f;
    rlPushMatrix();
    rlTranslatef(center.x, center.y, center.z);
    rlScalef(radii.x, radii.y, radii.z);
    DrawSphereEx({0.0f, 0.0f, 0.0f}, 1.0f, rings, slices, color);
    rlPopMatrix();
}

// Strong chibi proportions for cast turrets: grow mostly upward and sideways,
// with only a modest longitudinal increase so the original vehicle silhouette
// and exact gun endpoint remain legible.
inline void chibiCastTurret(Vector3 center, Vector3 radii, Color color,
                            int rings = 8, int slices = 16)
{
    center.y += radii.y * 0.22f;
    radii.x *= 1.10f;
    radii.y *= 1.12f;
    radii.z *= 1.12f;
    ellipsoid(center, radii, color, rings, slices);
}

// Welded German/Soviet turrets stay angular, but receive the same oversized
// arcade mass as the cast vehicles instead of being replaced by round domes.
inline void chibiTurretFrustum(Vector3 center, float bottomWidth,
                               float bottomLength, float topWidth,
                               float topLength, float height,
                               float topZOffset, float frontChamfer,
                               float rearChamfer, Color color)
{
    center.y += height * 0.06f;
    chamferedFrustum(center,
                     bottomWidth * 1.12f, bottomLength * 1.12f,
                     topWidth * 1.12f, topLength * 1.12f,
                     height * 1.12f, topZOffset,
                     frontChamfer, rearChamfer, color);
}

inline void rotatedBox(Vector3 center, Vector3 size, float yawDegrees, Color color)
{
    rlPushMatrix();
    rlTranslatef(center.x, center.y, center.z);
    rlRotatef(yawDegrees, 0.0f, 1.0f, 0.0f);
    DrawCubeV({0.0f, 0.0f, 0.0f}, size, color);
    rlPopMatrix();
}

inline void cylinder(Vector3 start, Vector3 end, float radius, Color color,
                     int sides = 12)
{
    DrawCylinderEx(start, end, radius, radius, sides, color);
}

inline void discOnSide(float side, float xInner, float xOuter, float y,
                       float z, float radius, Color color, int sides = 14)
{
    cylinder({side * xInner, y, z}, {side * xOuter, y, z}, radius, color, sides);
}

struct RunningGearSpec
{
    int roadWheels;
    float length;
    float halfWidth;
    float wheelRadius;
    bool fullSkirts;
    bool spacedArmorSkirts;
};

inline void drawRunningGear(const RunningGearSpec &spec, const Palette &p,
                            bool moving)
{
    const float outerX = spec.halfWidth + 0.055f;
    const int phase = moving ? static_cast<int>(GetTime() * 13.0) : 0;
    for (float side : {-1.0f, 1.0f})
    {
        // A short central belt plus two swollen end caps creates the rounded
        // capsule silhouette seen in hand-drawn arcade armor. The road-wheel
        // count and exact wheel positions remain model-specific below.
        box({side * spec.halfWidth, 0.245f, 0.035f},
            {0.235f, 0.380f, spec.length * 0.64f}, p.rubber);
        for (int end : {-1, 1})
        {
            ellipsoid({side * spec.halfWidth, 0.245f,
                       0.035f + end * spec.length * 0.405f},
                      {0.112f, 0.195f, spec.wheelRadius * 1.42f},
                      p.rubber, 7, 14);
        }
        box({side * outerX, 0.405f, 0.035f},
            {0.065f, 0.060f, spec.length * 0.62f}, p.edge);

        // Large round idler and drive sprocket terminate the track run, so its
        // side silhouette reads as a tracked vehicle rather than a long box.
        for (int end : {-1, 1})
        {
            const float z = 0.035f + end * spec.length * 0.405f;
            const float endRadius = spec.wheelRadius * 1.56f;
            discOnSide(side, spec.halfWidth - 0.105f, outerX + 0.044f,
                       0.235f, z, endRadius, p.rubber, 18);
            discOnSide(side, outerX + 0.045f, outerX + 0.058f,
                       0.235f, z, endRadius * 0.78f,
                       end < 0 ? p.lightPaint : p.steel, 16);
            discOnSide(side, outerX + 0.059f, outerX + 0.066f,
                       0.235f, z, endRadius * 0.20f, p.edge, 12);
        }

        if (!spec.fullSkirts)
        {
            box({side * (spec.halfWidth + 0.025f), 0.445f, 0.025f},
                {0.290f, 0.036f, spec.length * 0.94f}, p.lightPaint);
            box({side * (outerX + 0.082f), 0.430f, -spec.length * 0.395f},
                {0.085f, 0.070f, spec.length * 0.15f}, p.darkPaint);
        }

        const float wheelRange = spec.length * 0.72f;
        for (int wheel = 0; wheel < spec.roadWheels; ++wheel)
        {
            const float amount = spec.roadWheels == 1
                                     ? 0.5f
                                     : static_cast<float>(wheel) / static_cast<float>(spec.roadWheels - 1);
            const float z = -wheelRange * 0.5f + amount * wheelRange + 0.035f;
            const float visualRadius = spec.wheelRadius * 1.50f;
            discOnSide(side, spec.halfWidth - 0.105f, outerX + 0.042f,
                       0.235f, z, visualRadius, p.rubber, 18);
            discOnSide(side, outerX + 0.043f, outerX + 0.056f,
                       0.235f, z, visualRadius * 0.84f, p.wheel, 16);
            discOnSide(side, outerX + 0.057f, outerX + 0.064f,
                       0.235f, z, visualRadius * 0.49f,
                       p.lightPaint, 14);
            discOnSide(side, outerX + 0.065f, outerX + 0.071f,
                       0.235f, z, visualRadius * 0.18f, p.edge, 12);
        }

        const int shoeCount = 12;
        for (int shoe = 0; shoe < shoeCount; ++shoe)
        {
            const float amount = static_cast<float>(shoe) / static_cast<float>(shoeCount - 1);
            const float z = -spec.length * 0.405f +
                            amount * spec.length * 0.81f + 0.035f;
            const Color shoeColor = ((shoe + phase) & 1) == 0
                                        ? shade(p.steel, 0.74f)
                                        : shade(p.steel, 0.58f);
            box({side * (outerX + 0.068f), 0.083f, z},
                {0.065f, 0.065f, spec.length * 0.066f}, shoeColor);
            box({side * (outerX + 0.068f), 0.415f, z},
                {0.065f, 0.060f, spec.length * 0.066f}, shoeColor);
        }

        // Five links wrap around each elliptical end instead of forming the
        // old vertical, squared-off wall of track shoes.
        constexpr int arcSteps = 5;
        for (int end : {-1, 1})
        {
            for (int step = 0; step < arcSteps; ++step)
            {
                const float vertical = -1.0f +
                                       2.0f * step / static_cast<float>(arcSteps - 1);
                const float outward = std::sqrt(std::max(
                    0.0f, 1.0f - vertical * vertical));
                const float y = 0.245f + vertical * 0.180f;
                const float z = 0.035f + end *
                                (spec.length * 0.405f +
                                 outward * spec.wheelRadius * 1.62f);
                const Color linkColor = ((step + phase) & 1) == 0
                                            ? shade(p.steel, 0.76f)
                                            : shade(p.steel, 0.56f);
                box({side * (outerX + 0.068f), y, z},
                    {0.065f, 0.065f, spec.length * 0.055f}, linkColor);
            }
        }

        if (spec.fullSkirts)
        {
            constexpr int panels = 6;
            for (int panel = 0; panel < panels; ++panel)
            {
                const float z = -spec.length * 0.395f +
                                panel * (spec.length * 0.79f / (panels - 1));
                box({side * (outerX + 0.085f), 0.455f, z},
                    {0.038f, 0.235f, spec.length * 0.145f},
                    panel < 2 ? p.lightPaint : p.paint);
            }
        }

        if (spec.spacedArmorSkirts)
        {
            constexpr int panels = 5;
            for (int panel = 0; panel < panels; ++panel)
            {
                const float z = -spec.length * 0.35f + panel * spec.length * 0.175f;
                box({side * (outerX + 0.115f), 0.475f, z},
                    {0.070f, 0.255f, spec.length * 0.145f}, p.lightPaint);
                box({side * (outerX + 0.154f), 0.475f, z},
                    {0.012f, 0.185f, spec.length * 0.105f}, p.edge);
            }
        }
    }
}

inline void drawEngineDeck(float width, float z, float y, const Palette &p,
                           int grilleCount)
{
    box({0.0f, y, z}, {width, 0.055f, 0.43f}, p.darkPaint);
    for (int grille = 0; grille < grilleCount; ++grille)
    {
        const float x = -width * 0.39f +
                        grille * width * 0.78f / std::max(1, grilleCount - 1);
        box({x, y + 0.034f, z}, {0.035f, 0.018f, 0.34f}, p.steel);
    }
    box({0.0f, y + 0.039f, z - 0.205f}, {width * 0.92f, 0.015f, 0.018f},
        p.lightPaint);
}

inline void drawHeadlights(float halfWidth, float frontZ, float y,
                           const Palette &p, Color identity)
{
    for (float side : {-1.0f, 1.0f})
    {
        box({side * halfWidth, y, frontZ}, {0.155f, 0.115f, 0.042f}, p.edge);
        box({side * halfWidth, y + 0.006f, frontZ - 0.020f},
            {0.105f, 0.069f, 0.021f}, mix(identity, RAYWHITE, 0.60f));
        cylinder({side * (halfWidth + 0.085f), y - 0.055f, frontZ + 0.025f},
                 {side * (halfWidth + 0.085f), y + 0.095f, frontZ + 0.025f},
                 0.012f, p.steel, 8);
    }
}

inline void drawGun(float y, float startZ, float length, float radius,
                    const Palette &p, bool thermalSleeve)
{
    // The fixed oblique camera reduces apparent barrel width. Keep each
    // vehicle's historically distinct calibre while giving all main guns a
    // consistent readability boost at normal gameplay zoom.
    const float visualRadius = radius * 1.72f;
    cylinder({0.0f, y, startZ + 0.10f}, {0.0f, y, startZ - 0.10f},
             visualRadius * 2.20f, p.edge, 14);
    cylinder({0.0f, y, startZ}, {0.0f, y, startZ - length},
             visualRadius, p.steel, 16);
    // A chunky breech sleeve makes the cannon read as short and forceful even
    // though its tip stays at the exact muzzle position used by gameplay.
    cylinder({0.0f, y, startZ - length * 0.08f},
             {0.0f, y, startZ - length * 0.43f},
             visualRadius * 1.24f, p.darkPaint, 16);
    cylinder({0.0f, y, startZ - length * 0.38f},
             {0.0f, y, startZ - length * 0.49f},
             visualRadius * 1.39f, p.edge, 16);
    if (thermalSleeve)
    {
        cylinder({0.0f, y, startZ - length * 0.20f},
                 {0.0f, y, startZ - length * 0.63f},
                 visualRadius * 1.23f, p.darkPaint, 16);
        cylinder({0.0f, y, startZ - length * 0.47f},
                 {0.0f, y, startZ - length * 0.60f},
                 visualRadius * 1.50f, p.edge, 16);
    }
    cylinder({0.0f, y, startZ - length + 0.025f},
             {0.0f, y, startZ - length - 0.035f},
             visualRadius * 1.18f, shade(p.steel, 0.66f), 16);
    cylinder({0.0f, y, startZ - length - 0.036f},
             {0.0f, y, startZ - length - 0.050f},
             visualRadius * 0.68f, Color{10, 12, 12, 255}, 16);
}

inline void drawSmokeBanks(float x, float y, float z, const Palette &p)
{
    for (float side : {-1.0f, 1.0f})
    {
        for (int tube = 0; tube < 4; ++tube)
        {
            const float dz = tube * 0.055f;
            const Vector3 start{side * (x + tube * 0.012f), y + tube * 0.010f, z + dz};
            const Vector3 end{side * (x + 0.105f + tube * 0.014f),
                              y + 0.080f + tube * 0.010f,
                              z - 0.095f + dz};
            cylinder(start, end, 0.027f, p.steel, 9);
            DrawSphereEx(end, 0.030f, 4, 7, shade(p.steel, 0.60f));
        }
    }
}

inline void drawAntenna(float x, float z, float baseY, float height,
                        const Palette &p)
{
    cylinder({x, baseY, z}, {x * 1.04f, baseY + height, z + 0.025f},
             0.008f, p.edge, 6);
    DrawSphere({x * 1.04f, baseY + height, z + 0.025f}, 0.014f, p.steel);
}

inline void drawTurretRing(float y, float radius, const Palette &p)
{
    const float visualRadius = radius * 1.20f;
    DrawCylinder({0.0f, y, -0.015f}, visualRadius,
                 visualRadius * 0.96f, 0.082f, 24, p.edge);
    DrawCylinder({0.0f, y + 0.028f, -0.015f}, visualRadius * 0.91f,
                 visualRadius * 0.88f, 0.035f, 24, p.darkPaint);
}

inline void drawCupola(float x, float z, float y, float radius,
                       const Palette &p)
{
    const float visualRadius = radius * 1.13f;
    DrawCylinder({x, y, z}, visualRadius * 0.92f, visualRadius,
                 0.115f, 14, p.darkPaint);
    ellipsoid({x, y + 0.105f, z},
              {visualRadius * 0.72f, 0.055f, visualRadius * 0.66f},
              p.lightPaint, 5, 12);
    box({x, y + 0.055f, z - 0.015f},
        {visualRadius * 1.45f, 0.028f, visualRadius * 1.08f}, p.lightPaint);
    for (int slit = -1; slit <= 1; ++slit)
        box({x + slit * visualRadius * 0.48f,
             y + 0.015f, z - visualRadius * 0.88f},
            {visualRadius * 0.22f, 0.027f, 0.014f}, p.optic);
}

inline void drawPintleMachineGun(float x, float z, float y, const Palette &p)
{
    cylinder({x, y, z}, {x, y + 0.095f, z}, 0.020f, p.steel, 8);
    box({x, y + 0.115f, z - 0.035f}, {0.105f, 0.090f, 0.150f}, p.edge);
    cylinder({x, y + 0.140f, z - 0.090f},
             {x, y + 0.140f, z - 0.420f}, 0.012f, p.steel, 8);
    box({x + 0.070f, y + 0.115f, z - 0.010f},
        {0.070f, 0.105f, 0.075f}, p.darkPaint);
}

inline void drawJerryCans(float side, float z, float y, const Palette &p)
{
    for (int can = 0; can < 2; ++can)
    {
        const float dz = z + can * 0.145f;
        box({side * 0.515f, y, dz}, {0.105f, 0.175f, 0.125f}, p.canvas);
        box({side * 0.518f, y, dz}, {0.112f, 0.018f, 0.020f}, p.edge);
        box({side * 0.518f, y, dz}, {0.112f, 0.020f, 0.018f}, p.edge);
    }
}

inline void drawSpareTrack(float y, float z, float width, const Palette &p)
{
    constexpr int links = 7;
    for (int link = 0; link < links; ++link)
    {
        const float x = -width * 0.5f + (link + 0.5f) * width / links;
        box({x, y, z}, {width / links * 0.86f, 0.075f, 0.035f},
            (link & 1) == 0 ? p.steel : shade(p.steel, 0.72f));
    }
}

inline void drawMuzzleBrake(float y, float z, float radius, const Palette &p)
{
    const float visualRadius = radius * 1.72f;
    cylinder({0.0f, y, z + 0.075f}, {0.0f, y, z - 0.075f},
             visualRadius * 1.48f, shade(p.steel, 0.68f), 16);
    box({0.0f, y, z + 0.020f},
        {visualRadius * 3.15f, visualRadius * 2.55f, 0.035f}, p.edge);
    box({0.0f, y, z - 0.045f},
        {visualRadius * 3.15f, visualRadius * 2.55f, 0.035f}, p.edge);
    cylinder({0.0f, y, z - 0.078f}, {0.0f, y, z - 0.095f},
             visualRadius * 0.70f, Color{10, 12, 12, 255}, 14);
}

// Exaggerated but still period-correct shield around the gun trunnions. The
// dark outer oval and lighter inset read clearly in the oblique camera and
// visually anchor the thicker cannon to the turret face.
inline void drawGunMantlet(float y, float z, float halfWidth, float radius,
                           const Palette &p)
{
    ellipsoid({0.0f, y, z},
              {halfWidth * 1.25f, radius * 1.52f, radius * 0.98f},
              p.edge, 6, 16);
    ellipsoid({0.0f, y + radius * 0.07f, z - radius * 0.18f},
              {halfWidth * 0.91f, radius * 1.02f, radius * 0.70f},
              p.lightPaint, 5, 14);
    cylinder({0.0f, y, z - radius * 0.45f},
             {0.0f, y, z - radius * 1.45f},
             radius * 0.72f, p.darkPaint, 12);
}

inline void drawFuelDrums(float halfWidth, float y, float z,
                          const Palette &p)
{
    for (float side : {-1.0f, 1.0f})
    {
        for (int drum = 0; drum < 2; ++drum)
        {
            const float drumZ = z + drum * 0.225f;
            cylinder({side * halfWidth, y, drumZ - 0.085f},
                     {side * halfWidth, y, drumZ + 0.085f},
                     0.075f, p.canvas, 12);
            box({side * halfWidth, y, drumZ},
                {0.020f, 0.170f, 0.025f}, p.edge);
        }
    }
}

inline void drawTowEyes(float halfWidth, float y, float frontZ,
                        const Palette &p)
{
    for (float side : {-1.0f, 1.0f})
    {
        cylinder({side * halfWidth, y, frontZ + 0.025f},
                 {side * halfWidth, y, frontZ - 0.045f},
                 0.035f, p.steel, 10);
        cylinder({side * halfWidth, y, frontZ - 0.046f},
                 {side * halfWidth, y, frontZ - 0.057f},
                 0.019f, Color{12, 14, 14, 255}, 10);
    }
}

inline void drawFenderBoxes(float halfWidth, float y, float z,
                            const Palette &p)
{
    for (float side : {-1.0f, 1.0f})
    {
        box({side * halfWidth, y, z}, {0.145f, 0.145f, 0.380f}, p.darkPaint);
        box({side * halfWidth, y + 0.079f, z},
            {0.155f, 0.018f, 0.390f}, p.lightPaint);
        box({side * (halfWidth + 0.076f), y, z},
            {0.014f, 0.105f, 0.330f}, p.edge);
    }
}

inline void drawSherman(const Palette &p, Color identity, bool moving)
{
    // M4A3(76)W HVSS: six paired road wheels, rounded 76 mm turret and the
    // tall rear-deck silhouette shared by P1 and P2 in the 2D atlas.
    drawRunningGear({6, 1.62f, 0.585f, 0.132f, false, false}, p, moving);
    chamferedFrustum({0.0f, 0.245f, 0.035f}, 1.07f, 1.50f,
                     1.01f, 1.43f, 0.300f, 0.0f,
                     0.11f, 0.07f, p.darkPaint);
    chamferedFrustum({0.0f, 0.445f, -0.045f}, 1.12f, 1.45f,
                     0.88f, 1.12f, 0.285f, 0.080f,
                     0.14f, 0.08f, p.paint);
    slopedGlacis(0.78f, 0.92f, -0.765f, -0.205f,
                 0.365f, 0.570f, 0.055f, p.lightPaint);
    drawEngineDeck(0.82f, 0.485f, 0.575f, p, 5);
    drawHeadlights(0.375f, -0.765f, 0.430f, p, identity);
    drawSpareTrack(0.515f, -0.740f, 0.54f, p);

    drawTurretRing(0.605f, 0.40f, p);
    chibiCastTurret({0.0f, 0.735f, -0.020f},
                    {0.415f, 0.175f, 0.390f}, p.paint, 8, 18);
    ellipsoid({0.0f, 0.805f, 0.000f}, {0.315f, 0.075f, 0.285f},
              p.lightPaint, 6, 16);
    chamferedFrustum({0.0f, 0.710f, 0.285f}, 0.66f, 0.26f,
                     0.56f, 0.20f, 0.165f, -0.015f,
                     0.10f, 0.16f, p.darkPaint);
    drawGunMantlet(0.745f, -0.305f, 0.255f, 0.105f, p);
    drawGun(0.748f, -0.340f, 1.04f, 0.030f, p, false);
    drawCupola(0.195f, 0.035f, 0.855f, 0.105f, p);
    box({-0.185f, 0.846f, 0.075f}, {0.190f, 0.025f, 0.145f}, p.lightPaint);
    drawPintleMachineGun(0.205f, 0.115f, 0.905f, p);
    drawJerryCans(-1.0f, 0.485f, 0.500f, p);
    drawAntenna(-0.315f, 0.260f, 0.805f, 0.42f, p);
}

inline void drawM24Chaffee(const Palette &p, Color identity, bool moving)
{
    // M24: low five-wheel chassis, pronounced glacis and compact 75 mm
    // turret. The dimensions are normalized to the same one-tile footprint
    // as every other player vehicle.
    drawRunningGear({5, 1.43f, 0.515f, 0.126f, false, false}, p, moving);
    chamferedFrustum({0.0f, 0.225f, 0.035f}, 0.92f, 1.33f,
                     0.86f, 1.27f, 0.260f, 0.0f,
                     0.14f, 0.08f, p.darkPaint);
    chamferedFrustum({0.0f, 0.405f, -0.045f}, 0.98f, 1.29f,
                     0.77f, 1.02f, 0.245f, 0.085f,
                     0.16f, 0.09f, p.paint);
    slopedGlacis(0.62f, 0.79f, -0.690f, -0.195f,
                 0.335f, 0.515f, 0.050f, p.lightPaint);
    drawEngineDeck(0.66f, 0.430f, 0.520f, p, 4);
    drawHeadlights(0.305f, -0.690f, 0.390f, p, identity);
    drawTowEyes(0.285f, 0.230f, -0.700f, p);
    drawFenderBoxes(0.470f, 0.465f, 0.285f, p);

    drawTurretRing(0.545f, 0.315f, p);
    chibiCastTurret({0.0f, 0.680f, -0.040f},
                    {0.345f, 0.150f, 0.315f}, p.paint, 7, 16);
    ellipsoid({0.0f, 0.735f, -0.035f}, {0.255f, 0.064f, 0.225f},
              p.lightPaint, 5, 14);
    chamferedFrustum({0.0f, 0.655f, 0.205f}, 0.55f, 0.18f,
                     0.47f, 0.14f, 0.145f, -0.010f,
                     0.10f, 0.18f, p.darkPaint);
    drawGunMantlet(0.690f, -0.320f, 0.205f, 0.090f, p);
    drawGun(0.705f, -0.330f, 0.78f, 0.022f, p, false);
    drawCupola(0.145f, 0.025f, 0.800f, 0.090f, p);
    box({-0.145f, 0.790f, 0.045f}, {0.150f, 0.022f, 0.120f}, p.lightPaint);
    drawPintleMachineGun(0.155f, 0.095f, 0.845f, p);
    drawAntenna(-0.245f, 0.190f, 0.730f, 0.43f, p);
}

inline void drawM26Pershing(const Palette &p, Color identity, bool moving)
{
    // M26: broad cast turret, six large wheels and a long 90 mm gun with a
    // double-baffle brake distinguish it from the Sherman one tier below.
    drawRunningGear({6, 1.68f, 0.595f, 0.126f, false, false}, p, moving);
    chamferedFrustum({0.0f, 0.245f, 0.040f}, 1.09f, 1.56f,
                     1.03f, 1.48f, 0.300f, 0.0f,
                     0.11f, 0.07f, p.darkPaint);
    chamferedFrustum({0.0f, 0.455f, -0.055f}, 1.14f, 1.50f,
                     0.90f, 1.17f, 0.300f, 0.095f,
                     0.14f, 0.08f, p.paint);
    slopedGlacis(0.78f, 0.96f, -0.800f, -0.220f,
                 0.370f, 0.590f, 0.060f, p.lightPaint);
    drawEngineDeck(0.86f, 0.500f, 0.595f, p, 6);
    drawHeadlights(0.390f, -0.800f, 0.440f, p, identity);
    drawTowEyes(0.360f, 0.255f, -0.820f, p);
    drawSpareTrack(0.545f, -0.765f, 0.61f, p);
    drawFenderBoxes(0.545f, 0.515f, 0.345f, p);

    drawTurretRing(0.620f, 0.425f, p);
    chibiCastTurret({0.0f, 0.765f, -0.035f},
                    {0.455f, 0.190f, 0.425f}, p.paint, 9, 20);
    ellipsoid({0.0f, 0.835f, -0.025f}, {0.340f, 0.080f, 0.315f},
              p.lightPaint, 6, 18);
    chamferedFrustum({0.0f, 0.745f, 0.320f}, 0.75f, 0.25f,
                     0.65f, 0.19f, 0.190f, -0.015f,
                     0.10f, 0.17f, p.darkPaint);
    drawGunMantlet(0.770f, -0.370f, 0.285f, 0.120f, p);
    drawGun(0.775f, -0.400f, 1.08f, 0.032f, p, false);
    drawMuzzleBrake(0.775f, -1.500f, 0.032f, p);
    drawCupola(0.225f, 0.045f, 0.920f, 0.112f, p);
    box({-0.210f, 0.900f, 0.025f}, {0.190f, 0.027f, 0.150f}, p.lightPaint);
    drawPintleMachineGun(0.230f, 0.115f, 0.970f, p);
    drawJerryCans(-1.0f, 0.490f, 0.520f, p);
    drawAntenna(-0.335f, 0.260f, 0.850f, 0.49f, p);
}

inline void drawT28T95(const Palette &p, Color identity, bool moving)
{
    // T28/T95: a very low casemate carried between four track runs. The
    // outer silhouette remains inside the classic collision circle.
    drawRunningGear({8, 1.76f, 0.605f, 0.102f, true, false}, p, moving);
    for (float side : {-1.0f, 1.0f})
    {
        box({side * 0.390f, 0.225f, 0.040f},
            {0.145f, 0.285f, 1.64f}, p.rubber);
        for (int wheel = 0; wheel < 7; ++wheel)
        {
            const float z = -0.600f + wheel * 0.200f;
            discOnSide(side, 0.315f, 0.460f, 0.220f, z, 0.085f,
                       wheel & 1 ? p.steel : p.wheel, 12);
            discOnSide(side, 0.461f, 0.474f, 0.220f, z, 0.112f,
                       p.rubber, 14);
            discOnSide(side, 0.475f, 0.482f, 0.220f, z, 0.074f,
                       wheel & 1 ? p.lightPaint : p.wheel, 12);
        }
    }
    chamferedFrustum({0.0f, 0.260f, 0.050f}, 1.08f, 1.67f,
                     1.03f, 1.58f, 0.340f, 0.0f,
                     0.12f, 0.07f, p.darkPaint);
    chamferedFrustum({0.0f, 0.475f, -0.015f}, 1.16f, 1.56f,
                     0.88f, 1.24f, 0.285f, 0.055f,
                     0.17f, 0.09f, p.paint);
    slopedGlacis(0.72f, 0.84f, -0.855f, -0.315f,
                 0.345f, 0.635f, 0.070f, p.lightPaint);
    chamferedFrustum({0.0f, 0.615f, 0.315f}, 0.70f, 0.39f,
                     0.61f, 0.30f, 0.215f, -0.010f,
                     0.12f, 0.14f, p.darkPaint);
    drawEngineDeck(0.70f, 0.510f, 0.635f, p, 6);
    drawHeadlights(0.385f, -0.830f, 0.420f, p, identity);
    drawTowEyes(0.345f, 0.250f, -0.855f, p);
    drawSpareTrack(0.580f, -0.790f, 0.61f, p);

    drawGunMantlet(0.680f, -0.575f, 0.260f, 0.145f, p);
    drawGun(0.680f, -0.600f, 1.06f, 0.038f, p, false);
    drawCupola(0.235f, 0.030f, 0.830f, 0.115f, p);
    box({-0.215f, 0.815f, -0.005f}, {0.190f, 0.028f, 0.155f}, p.lightPaint);
    drawPintleMachineGun(0.240f, 0.100f, 0.875f, p);
    for (float side : {-1.0f, 1.0f})
        cylinder({side * 0.350f, 0.535f, 0.715f},
                 {side * 0.350f, 0.790f, 0.715f}, 0.043f, p.steel, 10);
    drawAntenna(-0.310f, 0.310f, 0.760f, 0.47f, p);
}

inline void drawT70(const Palette &p, Color identity, bool moving)
{
    drawRunningGear({5, 1.35f, 0.490f, 0.119f, false, false}, p, moving);
    chamferedFrustum({0.0f, 0.215f, 0.025f}, 0.88f, 1.25f,
                     0.82f, 1.18f, 0.250f, 0.0f,
                     0.16f, 0.08f, p.darkPaint);
    chamferedFrustum({0.0f, 0.390f, -0.055f}, 0.92f, 1.20f,
                     0.70f, 0.91f, 0.240f, 0.080f,
                     0.18f, 0.10f, p.paint);
    slopedGlacis(0.53f, 0.71f, -0.650f, -0.185f,
                 0.320f, 0.495f, 0.048f, p.lightPaint);
    drawEngineDeck(0.62f, 0.390f, 0.505f, p, 3);
    drawHeadlights(0.265f, -0.650f, 0.370f, p, identity);
    drawTowEyes(0.250f, 0.220f, -0.665f, p);

    drawTurretRing(0.510f, 0.275f, p);
    chibiTurretFrustum({0.0f, 0.615f, -0.050f}, 0.59f, 0.55f,
                       0.43f, 0.39f, 0.205f, -0.005f,
                       0.18f, 0.14f, p.paint);
    chamferedFrustum({0.0f, 0.620f, 0.190f}, 0.48f, 0.17f,
                     0.41f, 0.13f, 0.155f, -0.005f,
                     0.12f, 0.18f, p.darkPaint);
    drawGunMantlet(0.635f, -0.300f, 0.155f, 0.075f, p);
    drawGun(0.650f, -0.310f, 0.67f, 0.017f, p, false);
    cylinder({-0.090f, 0.635f, -0.315f},
             {-0.090f, 0.635f, -0.650f}, 0.009f, p.steel, 8);
    drawCupola(0.095f, 0.035f, 0.755f, 0.078f, p);
    box({-0.120f, 0.745f, 0.035f}, {0.130f, 0.020f, 0.105f}, p.lightPaint);
    drawAntenna(-0.195f, 0.175f, 0.680f, 0.43f, p);
}

inline void drawT3485(const Palette &p, Color identity, bool moving)
{
    // Five large Christie wheels, sharply sloped glacis and the broad 85 mm
    // three-man turret create the familiar T-34-85 silhouette.
    drawRunningGear({5, 1.60f, 0.565f, 0.145f, false, false}, p, moving);
    chamferedFrustum({0.0f, 0.235f, 0.040f}, 1.01f, 1.48f,
                     0.94f, 1.40f, 0.275f, 0.0f,
                     0.16f, 0.08f, p.darkPaint);
    chamferedFrustum({0.0f, 0.440f, -0.055f}, 1.08f, 1.43f,
                     0.76f, 1.10f, 0.300f, 0.095f,
                     0.20f, 0.10f, p.paint);
    slopedGlacis(0.62f, 0.82f, -0.755f, -0.165f,
                 0.320f, 0.575f, 0.055f, p.lightPaint);
    drawEngineDeck(0.77f, 0.470f, 0.555f, p, 5);
    drawHeadlights(0.330f, -0.755f, 0.410f, p, identity);
    drawTowEyes(0.310f, 0.235f, -0.775f, p);
    drawFuelDrums(0.555f, 0.490f, 0.365f, p);

    drawTurretRing(0.575f, 0.390f, p);
    chibiCastTurret({0.0f, 0.715f, -0.045f},
                    {0.405f, 0.170f, 0.365f}, p.paint, 7, 14);
    ellipsoid({0.0f, 0.780f, -0.035f}, {0.290f, 0.070f, 0.255f},
              p.lightPaint, 5, 12);
    chamferedFrustum({0.0f, 0.695f, 0.255f}, 0.63f, 0.22f,
                     0.53f, 0.17f, 0.170f, -0.010f,
                     0.12f, 0.18f, p.darkPaint);
    drawGunMantlet(0.710f, -0.360f, 0.245f, 0.105f, p);
    drawGun(0.720f, -0.370f, 1.00f, 0.027f, p, false);
    drawCupola(0.205f, 0.025f, 0.845f, 0.100f, p);
    box({-0.195f, 0.835f, 0.010f}, {0.175f, 0.024f, 0.135f}, p.lightPaint);
    drawAntenna(-0.285f, 0.240f, 0.790f, 0.48f, p);
}

inline void drawIS2(const Palette &p, Color identity, bool moving)
{
    drawRunningGear({6, 1.70f, 0.600f, 0.127f, false, false}, p, moving);
    chamferedFrustum({0.0f, 0.245f, 0.040f}, 1.09f, 1.58f,
                     1.02f, 1.49f, 0.300f, 0.0f,
                     0.15f, 0.07f, p.darkPaint);
    chamferedFrustum({0.0f, 0.460f, -0.060f}, 1.14f, 1.51f,
                     0.82f, 1.16f, 0.315f, 0.105f,
                     0.21f, 0.09f, p.paint);
    // The narrow leading edge and broad shoulders reproduce the IS-2's pike
    // nose in the overhead silhouette instead of another rectangular bow.
    slopedGlacis(0.34f, 0.91f, -0.830f, -0.190f,
                 0.350f, 0.600f, 0.065f, p.lightPaint);
    drawEngineDeck(0.82f, 0.500f, 0.595f, p, 6);
    drawHeadlights(0.365f, -0.805f, 0.430f, p, identity);
    drawTowEyes(0.340f, 0.245f, -0.830f, p);
    drawFuelDrums(0.585f, 0.515f, 0.390f, p);
    drawSpareTrack(0.550f, -0.780f, 0.59f, p);

    drawTurretRing(0.630f, 0.425f, p);
    chibiCastTurret({0.0f, 0.765f, -0.030f},
                    {0.455f, 0.190f, 0.420f}, p.paint, 8, 18);
    ellipsoid({0.0f, 0.840f, -0.020f}, {0.325f, 0.078f, 0.295f},
              p.lightPaint, 6, 16);
    chamferedFrustum({0.0f, 0.750f, 0.315f}, 0.73f, 0.26f,
                     0.62f, 0.19f, 0.195f, -0.015f,
                     0.10f, 0.18f, p.darkPaint);
    drawGunMantlet(0.770f, -0.380f, 0.280f, 0.125f, p);
    drawGun(0.785f, -0.410f, 1.10f, 0.035f, p, false);
    drawMuzzleBrake(0.785f, -1.530f, 0.035f, p);
    drawCupola(0.225f, 0.045f, 0.930f, 0.110f, p);
    box({-0.215f, 0.910f, 0.030f}, {0.190f, 0.027f, 0.150f}, p.lightPaint);
    drawAntenna(-0.330f, 0.270f, 0.860f, 0.51f, p);
}

inline void drawKV5Project(const Palette &p, Color identity, bool moving)
{
    // KV-5 was a 1941 design project. Its towering main turret, front 45 mm
    // auxiliary turret and extremely long hull make the speculative nature
    // legible without exceeding the shared gameplay footprint.
    drawRunningGear({8, 1.78f, 0.610f, 0.104f, false, false}, p, moving);
    chamferedFrustum({0.0f, 0.255f, 0.050f}, 1.12f, 1.66f,
                     1.06f, 1.58f, 0.315f, 0.0f,
                     0.11f, 0.06f, p.darkPaint);
    chamferedFrustum({0.0f, 0.465f, 0.050f}, 1.15f, 1.56f,
                     1.04f, 1.43f, 0.285f, 0.035f,
                     0.13f, 0.07f, p.paint);
    slopedGlacis(0.83f, 0.99f, -0.845f, -0.285f,
                 0.385f, 0.610f, 0.065f, p.lightPaint);
    drawEngineDeck(0.86f, 0.510f, 0.625f, p, 7);
    drawHeadlights(0.390f, -0.845f, 0.450f, p, identity);
    drawTowEyes(0.355f, 0.255f, -0.870f, p);
    drawSpareTrack(0.560f, -0.815f, 0.63f, p);
    drawFuelDrums(0.600f, 0.525f, 0.425f, p);

    // Offset auxiliary turret; frustum intentionally exercises center.x.
    drawTurretRing(0.610f, 0.455f, p);
    chibiTurretFrustum({0.0f, 0.765f, 0.020f}, 0.96f, 0.88f,
                       0.73f, 0.63f, 0.300f, -0.010f,
                       0.16f, 0.13f, p.paint);
    ellipsoid({0.0f, 0.910f, 0.010f}, {0.355f, 0.085f, 0.315f},
              p.lightPaint, 6, 16);
    chamferedFrustum({0.0f, 0.775f, 0.390f}, 0.82f, 0.26f,
                     0.68f, 0.19f, 0.250f, -0.015f,
                     0.10f, 0.16f, p.darkPaint);
    drawGunMantlet(0.790f, -0.400f, 0.315f, 0.135f, p);
    drawGun(0.815f, -0.440f, 1.15f, 0.038f, p, false);
    drawMuzzleBrake(0.815f, -1.610f, 0.038f, p);
    drawCupola(0.255f, 0.060f, 0.970f, 0.125f, p);
    box({-0.245f, 0.945f, 0.055f}, {0.215f, 0.030f, 0.165f}, p.lightPaint);

    chamferedFrustum({-0.310f, 0.655f, -0.565f}, 0.31f, 0.34f,
                     0.24f, 0.25f, 0.190f, 0.000f,
                     0.16f, 0.12f, p.lightPaint);
    cylinder({-0.310f, 0.690f, -0.730f},
             {-0.310f, 0.690f, -1.000f}, 0.014f, p.steel, 9);
    DrawCylinder({0.305f, 0.640f, 0.540f}, 0.12f, 0.14f, 0.165f, 12,
                 p.darkPaint);
    cylinder({0.305f, 0.695f, 0.555f},
             {0.305f, 0.695f, 0.820f}, 0.010f, p.steel, 8);
    drawPintleMachineGun(0.265f, 0.135f, 1.010f, p);
    drawAntenna(-0.365f, 0.320f, 0.910f, 0.54f, p);
}

inline void drawPanzerII(const Palette &p, Color identity, bool moving)
{
    drawRunningGear({5, 1.42f, 0.525f, 0.122f, false, false}, p, moving);
    chamferedFrustum({0.0f, 0.225f, 0.025f}, 0.94f, 1.32f,
                     0.88f, 1.24f, 0.270f, 0.0f,
                     0.13f, 0.07f, p.darkPaint);
    chamferedFrustum({0.0f, 0.405f, -0.040f}, 0.99f, 1.27f,
                     0.80f, 1.03f, 0.240f, 0.070f,
                     0.15f, 0.08f, p.paint);
    slopedGlacis(0.66f, 0.81f, -0.680f, -0.205f,
                 0.335f, 0.520f, 0.050f, p.lightPaint);
    chamferedFrustum({0.0f, 0.505f, 0.335f}, 0.78f, 0.43f,
                     0.69f, 0.36f, 0.105f, 0.0f,
                     0.09f, 0.10f, p.darkPaint);
    drawEngineDeck(0.69f, 0.435f, 0.535f, p, 4);
    drawHeadlights(0.325f, -0.680f, 0.385f, p, identity);

    drawTurretRing(0.545f, 0.32f, p);
    chibiTurretFrustum({0.0f, 0.650f, -0.035f}, 0.68f, 0.62f,
                       0.51f, 0.45f, 0.215f, 0.0f,
                       0.17f, 0.14f, p.paint);
    chamferedFrustum({0.0f, 0.655f, 0.225f}, 0.57f, 0.17f,
                     0.49f, 0.13f, 0.175f, -0.005f,
                     0.10f, 0.18f, p.darkPaint);
    drawGunMantlet(0.665f, -0.320f, 0.185f, 0.080f, p);
    drawGun(0.690f, -0.345f, 0.72f, 0.018f, p, false);
    cylinder({-0.105f, 0.670f, -0.345f},
             {-0.105f, 0.670f, -0.740f}, 0.010f, p.steel, 8);
    drawCupola(0.115f, 0.065f, 0.790f, 0.088f, p);
    drawAntenna(-0.235f, 0.220f, 0.710f, 0.46f, p);
}

inline void drawPanzerIVH(const Palette &p, Color identity, bool moving)
{
    // Eight small paired wheels and segmented Schuerzen define the Ausf. H;
    // turret skirts are kept high enough to leave the running gear readable.
    drawRunningGear({8, 1.60f, 0.570f, 0.098f, false, true}, p, moving);
    chamferedFrustum({0.0f, 0.235f, 0.030f}, 1.03f, 1.48f,
                     0.98f, 1.41f, 0.285f, 0.0f,
                     0.10f, 0.06f, p.darkPaint);
    chamferedFrustum({0.0f, 0.425f, 0.005f}, 1.07f, 1.39f,
                     0.94f, 1.25f, 0.260f, 0.035f,
                     0.11f, 0.07f, p.paint);
    slopedGlacis(0.84f, 0.95f, -0.755f, -0.285f,
                 0.355f, 0.545f, 0.052f, p.lightPaint);
    drawEngineDeck(0.79f, 0.465f, 0.560f, p, 5);
    drawHeadlights(0.350f, -0.755f, 0.405f, p, identity);
    drawTowEyes(0.330f, 0.235f, -0.785f, p);
    drawSpareTrack(0.520f, -0.720f, 0.52f, p);
    drawFenderBoxes(0.520f, 0.505f, 0.360f, p);

    drawTurretRing(0.570f, 0.375f, p);
    chibiTurretFrustum({0.0f, 0.690f, -0.020f}, 0.79f, 0.71f,
                       0.59f, 0.51f, 0.240f, 0.0f,
                       0.16f, 0.13f, p.paint);
    chamferedFrustum({0.0f, 0.690f, 0.300f}, 0.70f, 0.22f,
                     0.60f, 0.16f, 0.205f, -0.010f,
                     0.10f, 0.18f, p.darkPaint);
    drawGunMantlet(0.710f, -0.365f, 0.255f, 0.090f, p);
    drawGun(0.725f, -0.390f, 0.93f, 0.026f, p, false);
    drawMuzzleBrake(0.725f, -1.340f, 0.026f, p);
    cylinder({-0.155f, 0.705f, -0.390f},
             {-0.155f, 0.705f, -0.755f}, 0.010f, p.steel, 8);

    for (float side : {-1.0f, 1.0f})
    {
        for (int panel = 0; panel < 3; ++panel)
        {
            const float z = -0.225f + panel * 0.225f;
            box({side * 0.455f, 0.690f, z},
                {0.045f, 0.255f, 0.195f}, p.lightPaint);
            box({side * 0.482f, 0.690f, z},
                {0.012f, 0.185f, 0.145f}, p.edge);
        }
    }
    drawCupola(0.205f, 0.055f, 0.840f, 0.105f, p);
    box({-0.200f, 0.835f, 0.045f}, {0.175f, 0.025f, 0.135f}, p.lightPaint);
    drawSmokeBanks(0.350f, 0.700f, -0.150f, p);
    drawJerryCans(-1.0f, 0.425f, 0.490f, p);
    drawAntenna(-0.295f, 0.260f, 0.790f, 0.49f, p);
}

inline void drawSdkfz231(const Palette &p, Color identity, bool moving)
{
    // The original B sprite has three separated wheel groups per side. Keep
    // that defining 6-Rad silhouette instead of substituting a tracked tank.
    const int phase = moving ? static_cast<int>(GetTime() * 12.0f) : 0;
    for (float side : {-1.0f, 1.0f})
    {
        for (int axle = 0; axle < 3; ++axle)
        {
            const float z = -0.475f + axle * 0.475f;
            discOnSide(side, 0.455f, 0.610f, 0.205f, z, 0.205f,
                       p.rubber, 18);
            discOnSide(side, 0.611f, 0.625f, 0.205f, z, 0.088f,
                       ((axle + phase) & 1) == 0 ? p.lightPaint : p.steel, 14);
            box({side * 0.535f, 0.385f, z}, {0.205f, 0.075f, 0.325f},
                p.darkPaint);
        }
        box({side * 0.535f, 0.465f, 0.000f}, {0.155f, 0.055f, 1.38f},
            p.paint);
    }
    chamferedFrustum({0.0f, 0.250f, 0.010f}, 0.88f, 1.43f,
                     0.80f, 1.33f, 0.275f, 0.0f,
                     0.20f, 0.15f, p.darkPaint);
    chamferedFrustum({0.0f, 0.445f, -0.020f}, 0.94f, 1.38f,
                     0.69f, 1.08f, 0.265f, 0.0f,
                     0.23f, 0.17f, p.paint);
    slopedGlacis(0.48f, 0.70f, -0.735f, -0.245f,
                 0.340f, 0.555f, 0.048f, p.lightPaint);
    drawHeadlights(0.300f, -0.735f, 0.415f, p, identity);
    box({0.0f, 0.548f, 0.560f}, {0.58f, 0.050f, 0.245f}, p.darkPaint);

    drawTurretRing(0.575f, 0.29f, p);
    chibiTurretFrustum({0.0f, 0.660f, -0.040f}, 0.60f, 0.55f,
                       0.42f, 0.38f, 0.185f, 0.0f,
                       0.20f, 0.15f, p.paint);
    drawGunMantlet(0.665f, -0.300f, 0.160f, 0.067f, p);
    drawGun(0.685f, -0.320f, 0.70f, 0.016f, p, false);
    drawCupola(0.0f, 0.045f, 0.785f, 0.082f, p);
    drawAntenna(-0.205f, 0.180f, 0.725f, 0.55f, p);
    drawJerryCans(1.0f, 0.380f, 0.480f, p);
}

inline void drawPanzerIII(const Palette &p, Color identity, bool moving)
{
    drawRunningGear({6, 1.58f, 0.565f, 0.113f, false, false}, p, moving);
    chamferedFrustum({0.0f, 0.235f, 0.025f}, 1.02f, 1.47f,
                     0.96f, 1.39f, 0.285f, 0.0f,
                     0.11f, 0.06f, p.darkPaint);
    chamferedFrustum({0.0f, 0.420f, -0.035f}, 1.07f, 1.40f,
                     0.88f, 1.20f, 0.250f, 0.070f,
                     0.13f, 0.08f, p.paint);
    slopedGlacis(0.76f, 0.90f, -0.755f, -0.260f,
                 0.345f, 0.540f, 0.052f, p.lightPaint);
    drawEngineDeck(0.78f, 0.435f, 0.555f, p, 5);
    drawHeadlights(0.350f, -0.755f, 0.405f, p, identity);
    drawSpareTrack(0.515f, -0.700f, 0.50f, p);

    drawTurretRing(0.575f, 0.38f, p);
    chibiTurretFrustum({0.0f, 0.690f, -0.015f}, 0.80f, 0.73f,
                       0.61f, 0.53f, 0.235f, 0.0f,
                       0.16f, 0.13f, p.paint);
    chamferedFrustum({0.0f, 0.690f, 0.305f}, 0.71f, 0.23f,
                     0.61f, 0.17f, 0.205f, -0.010f,
                     0.10f, 0.18f, p.darkPaint);
    drawGunMantlet(0.710f, -0.370f, 0.265f, 0.092f, p);
    drawGun(0.735f, -0.405f, 1.08f, 0.026f, p, false);
    cylinder({-0.160f, 0.710f, -0.395f},
             {-0.160f, 0.710f, -0.775f}, 0.010f, p.steel, 8);
    drawCupola(0.205f, 0.050f, 0.835f, 0.105f, p);
    box({-0.205f, 0.835f, 0.035f}, {0.175f, 0.025f, 0.135f}, p.lightPaint);
    drawAntenna(-0.295f, 0.265f, 0.790f, 0.48f, p);
    drawJerryCans(-1.0f, 0.420f, 0.485f, p);
}

inline void drawTigerI(const Palette &p, Color identity, bool moving)
{
    drawRunningGear({8, 1.72f, 0.610f, 0.118f, false, false}, p, moving);
    chamferedFrustum({0.0f, 0.250f, 0.030f}, 1.13f, 1.62f,
                     1.08f, 1.55f, 0.315f, 0.0f,
                     0.08f, 0.05f, p.darkPaint);
    // Tiger armor remains characteristically upright, but cut corners and a
    // separately lit glacis keep its mass from collapsing into one cuboid.
    chamferedFrustum({0.0f, 0.445f, 0.015f}, 1.16f, 1.50f,
                     1.08f, 1.39f, 0.295f, 0.025f,
                     0.08f, 0.05f, p.paint);
    slopedGlacis(0.94f, 1.04f, -0.830f, -0.335f,
                 0.365f, 0.565f, 0.060f, p.lightPaint);
    drawEngineDeck(0.93f, 0.500f, 0.610f, p, 7);
    drawHeadlights(0.405f, -0.830f, 0.435f, p, identity);
    drawSpareTrack(0.525f, -0.775f, 0.68f, p);

    for (float side : {-1.0f, 1.0f})
    {
        cylinder({side * 0.355f, 0.530f, 0.690f},
                 {side * 0.355f, 0.810f, 0.690f}, 0.050f, p.steel, 12);
        box({side * 0.350f, 0.725f, 0.600f}, {0.205f, 0.180f, 0.145f},
            p.canvas);
    }

    drawTurretRing(0.620f, 0.44f, p);
    chibiCastTurret({0.0f, 0.750f, -0.015f},
                    {0.465f, 0.150f, 0.415f}, p.paint, 7, 18);
    chamferedFrustum({0.0f, 0.755f, 0.355f}, 0.84f, 0.27f,
                     0.73f, 0.20f, 0.235f, -0.015f,
                     0.09f, 0.18f, p.darkPaint);
    ellipsoid({0.0f, 0.835f, -0.005f}, {0.345f, 0.075f, 0.305f},
              p.lightPaint, 5, 16);
    drawGunMantlet(0.750f, -0.370f, 0.290f, 0.125f, p);
    drawGun(0.780f, -0.405f, 1.18f, 0.033f, p, false);
    cylinder({0.0f, 0.780f, -1.500f},
             {0.0f, 0.780f, -1.655f}, 0.048f, p.darkPaint, 14);
    drawCupola(0.225f, 0.060f, 0.925f, 0.120f, p);
    box({-0.210f, 0.905f, 0.035f}, {0.195f, 0.030f, 0.150f}, p.lightPaint);
    box({-0.365f, 0.765f, 0.180f}, {0.065f, 0.125f, 0.190f}, p.edge);
    drawAntenna(-0.350f, 0.300f, 0.860f, 0.50f, p);
}

inline void drawMaus(const Palette &p, Color identity, bool moving)
{
    // Maus: slab-sided hull, almost fully hidden suspension and a massive
    // curved-front turret. Width is visually maximized without changing the
    // shared 0.72-tile collision radius.
    drawRunningGear({8, 1.80f, 0.610f, 0.100f, true, false}, p, moving);
    chamferedFrustum({0.0f, 0.260f, 0.055f}, 1.14f, 1.69f,
                     1.09f, 1.61f, 0.330f, 0.0f,
                     0.08f, 0.05f, p.darkPaint);
    chamferedFrustum({0.0f, 0.485f, 0.010f}, 1.18f, 1.64f,
                     1.04f, 1.44f, 0.310f, 0.045f,
                     0.10f, 0.06f, p.paint);
    slopedGlacis(0.90f, 1.03f, -0.860f, -0.340f,
                 0.390f, 0.625f, 0.070f, p.lightPaint);
    drawEngineDeck(0.91f, 0.540f, 0.650f, p, 8);
    drawHeadlights(0.405f, -0.860f, 0.455f, p, identity);
    drawTowEyes(0.370f, 0.260f, -0.890f, p);
    drawSpareTrack(0.585f, -0.830f, 0.67f, p);
    for (float side : {-1.0f, 1.0f})
    {
        box({side * 0.595f, 0.530f, 0.350f},
            {0.065f, 0.250f, 0.690f}, p.lightPaint);
        cylinder({side * 0.390f, 0.555f, 0.705f},
                 {side * 0.390f, 0.830f, 0.705f}, 0.052f, p.steel, 12);
    }

    drawTurretRing(0.665f, 0.465f, p);
    chibiCastTurret({0.0f, 0.805f, -0.020f},
                    {0.505f, 0.205f, 0.445f}, p.paint, 9, 22);
    ellipsoid({0.0f, 0.895f, -0.010f}, {0.375f, 0.085f, 0.330f},
              p.lightPaint, 6, 18);
    chamferedFrustum({0.0f, 0.790f, 0.375f}, 0.86f, 0.28f,
                     0.74f, 0.20f, 0.235f, -0.015f,
                     0.08f, 0.18f, p.darkPaint);
    drawGunMantlet(0.805f, -0.410f, 0.330f, 0.145f, p);
    drawGun(0.835f, -0.440f, 1.15f, 0.040f, p, false);
    drawMuzzleBrake(0.835f, -1.610f, 0.040f, p);
    // Coaxial 75 mm gun mounted to the right of the 128 mm main gun.
    cylinder({0.145f, 0.785f, -0.440f},
             {0.145f, 0.785f, -1.190f}, 0.018f, p.steel, 12);
    cylinder({0.145f, 0.785f, -1.185f},
             {0.145f, 0.785f, -1.235f}, 0.024f, p.edge, 12);
    drawCupola(0.255f, 0.065f, 0.985f, 0.125f, p);
    box({-0.250f, 0.960f, 0.055f}, {0.220f, 0.030f, 0.170f}, p.lightPaint);
    box({-0.380f, 0.855f, 0.190f}, {0.070f, 0.135f, 0.205f}, p.edge);
    drawPintleMachineGun(0.265f, 0.150f, 1.025f, p);
    drawAntenna(-0.375f, 0.315f, 0.925f, 0.56f, p);
}

// Stable attachment specifications preserve the gameplay muzzle positions.
// Runtime art derives a broad chassis, compact turret, and chunky running gear
// from them; the original vehicle families remain readable across all tiers.
enum class ArcadeHeadShape
{
    Cast,
    Teardrop,
    Angular,
    Casemate
};

enum class ArcadeNationStyle
{
    American,
    Soviet,
    German
};

struct ArcadeVehicleSpec
{
    int wheelCount = 3;
    float trackLength = 1.12f;
    float trackHalfWidth = 0.52f;
    float trackHeight = 0.49f;
    float wheelRadius = 0.205f;
    float hullWidth = 0.76f;
    float hullLength = 0.70f;
    float hullHeight = 0.19f;
    float headWidth = 1.08f;
    float headLength = 0.76f;
    float headHeight = 0.49f;
    float headY = 0.81f;
    // A rearward head exposes the small body's nose instead of hiding it.
    float headZ = 0.100f;
    float gunLength = 0.40f;
    float gunRadius = 0.037f;
    ArcadeHeadShape headShape = ArcadeHeadShape::Cast;
    bool wheeled = false;
    bool muzzleBrake = false;
    bool auxiliaryTurret = false;
    bool skirts = false;
    ArcadeNationStyle nationStyle = ArcadeNationStyle::American;
};

inline ArcadeNationStyle arcadeNationStyle(Vehicle vehicle)
{
    switch (vehicle)
    {
    case Vehicle::T70:
    case Vehicle::T3485:
    case Vehicle::IS2:
    case Vehicle::KV5Project:
        return ArcadeNationStyle::Soviet;
    case Vehicle::PanzerIIF:
    case Vehicle::PanzerIVH:
    case Vehicle::TigerIE:
    case Vehicle::Maus:
    case Vehicle::Sdkfz231SixRad:
    case Vehicle::PanzerIIIL:
        return ArcadeNationStyle::German;
    case Vehicle::M24Chaffee:
    case Vehicle::M4A3Sherman:
    case Vehicle::M26Pershing:
    case Vehicle::T28T95:
    default:
        return ArcadeNationStyle::American;
    }
}

inline ArcadeVehicleSpec arcadeVehicleSpec(Vehicle vehicle)
{
    ArcadeVehicleSpec spec;
    spec.nationStyle = arcadeNationStyle(vehicle);
    switch (vehicle)
    {
    case Vehicle::M24Chaffee:
        spec.wheelCount = 5;
        spec.trackLength = 1.06f;
        spec.trackHalfWidth = 0.49f;
        spec.trackHeight = 0.47f;
        spec.wheelRadius = 0.195f;
        spec.hullWidth = 0.69f;
        spec.hullLength = 0.64f;
        spec.hullHeight = 0.17f;
        spec.headWidth = 1.00f;
        spec.headLength = 0.68f;
        spec.headHeight = 0.44f;
        spec.headY = 0.77f;
        spec.gunLength = 0.32f;
        spec.gunRadius = 0.034f;
        break;
    case Vehicle::M4A3Sherman:
        spec.wheelCount = 6;
        spec.headWidth = 1.10f;
        spec.headLength = 0.78f;
        spec.headHeight = 0.51f;
        spec.headY = 0.82f;
        spec.gunLength = 0.39f;
        spec.gunRadius = 0.038f;
        break;
    case Vehicle::M26Pershing:
        spec.wheelCount = 6;
        spec.trackLength = 1.17f;
        spec.trackHalfWidth = 0.54f;
        spec.trackHeight = 0.51f;
        spec.wheelRadius = 0.215f;
        spec.hullWidth = 0.80f;
        spec.headWidth = 1.16f;
        spec.headLength = 0.82f;
        spec.headHeight = 0.55f;
        spec.headY = 0.85f;
        spec.gunLength = 0.48f;
        spec.gunRadius = 0.043f;
        spec.muzzleBrake = true;
        break;
    case Vehicle::T28T95:
        spec.wheelCount = 8;
        spec.trackLength = 1.20f;
        spec.trackHalfWidth = 0.56f;
        spec.trackHeight = 0.51f;
        spec.wheelRadius = 0.215f;
        spec.hullWidth = 0.76f;
        spec.hullLength = 0.74f;
        spec.hullHeight = 0.16f;
        spec.headWidth = 0.98f;
        spec.headLength = 0.80f;
        spec.headHeight = 0.34f;
        spec.headY = 0.70f;
        spec.gunLength = 0.45f;
        spec.gunRadius = 0.046f;
        spec.headShape = ArcadeHeadShape::Casemate;
        spec.muzzleBrake = true;
        break;
    case Vehicle::T70:
        spec.wheelCount = 5;
        spec.trackLength = 1.02f;
        spec.trackHalfWidth = 0.48f;
        spec.trackHeight = 0.46f;
        spec.wheelRadius = 0.190f;
        spec.hullWidth = 0.66f;
        spec.hullLength = 0.61f;
        spec.hullHeight = 0.16f;
        spec.headWidth = 0.94f;
        spec.headLength = 0.64f;
        spec.headHeight = 0.41f;
        spec.headY = 0.75f;
        spec.gunLength = 0.29f;
        spec.gunRadius = 0.032f;
        spec.headShape = ArcadeHeadShape::Angular;
        break;
    case Vehicle::T3485:
        spec.wheelCount = 5;
        spec.headWidth = 1.07f;
        spec.headLength = 0.74f;
        spec.headHeight = 0.48f;
        spec.headY = 0.81f;
        spec.gunLength = 0.38f;
        spec.gunRadius = 0.038f;
        spec.headShape = ArcadeHeadShape::Teardrop;
        break;
    case Vehicle::IS2:
        spec.wheelCount = 6;
        spec.trackLength = 1.17f;
        spec.trackHalfWidth = 0.54f;
        spec.trackHeight = 0.51f;
        spec.wheelRadius = 0.215f;
        spec.hullWidth = 0.78f;
        spec.headWidth = 1.15f;
        spec.headLength = 0.80f;
        spec.headHeight = 0.55f;
        spec.headY = 0.85f;
        spec.gunLength = 0.47f;
        spec.gunRadius = 0.044f;
        spec.headShape = ArcadeHeadShape::Teardrop;
        spec.muzzleBrake = true;
        break;
    case Vehicle::KV5Project:
        spec.wheelCount = 8;
        spec.trackLength = 1.21f;
        spec.trackHalfWidth = 0.56f;
        spec.trackHeight = 0.52f;
        spec.wheelRadius = 0.220f;
        spec.hullWidth = 0.80f;
        spec.headWidth = 1.18f;
        spec.headLength = 0.84f;
        spec.headHeight = 0.60f;
        spec.headY = 0.88f;
        spec.gunLength = 0.49f;
        spec.gunRadius = 0.046f;
        spec.headShape = ArcadeHeadShape::Angular;
        spec.muzzleBrake = true;
        spec.auxiliaryTurret = true;
        break;
    case Vehicle::PanzerIIF:
        spec.wheelCount = 5;
        spec.trackLength = 1.04f;
        spec.trackHalfWidth = 0.49f;
        spec.trackHeight = 0.47f;
        spec.wheelRadius = 0.192f;
        spec.hullWidth = 0.68f;
        spec.hullLength = 0.63f;
        spec.hullHeight = 0.17f;
        spec.headWidth = 0.99f;
        spec.headLength = 0.66f;
        spec.headHeight = 0.43f;
        spec.headY = 0.77f;
        spec.gunLength = 0.27f;
        spec.gunRadius = 0.033f;
        spec.headShape = ArcadeHeadShape::Angular;
        break;
    case Vehicle::PanzerIVH:
        spec.wheelCount = 8;
        spec.headWidth = 1.08f;
        spec.headLength = 0.74f;
        spec.headHeight = 0.49f;
        spec.headY = 0.81f;
        spec.gunLength = 0.37f;
        spec.gunRadius = 0.039f;
        spec.headShape = ArcadeHeadShape::Angular;
        spec.muzzleBrake = true;
        spec.skirts = true;
        break;
    case Vehicle::TigerIE:
        spec.wheelCount = 8;
        spec.trackLength = 1.18f;
        spec.trackHalfWidth = 0.55f;
        spec.trackHeight = 0.52f;
        spec.wheelRadius = 0.220f;
        spec.hullWidth = 0.80f;
        spec.headWidth = 1.17f;
        spec.headLength = 0.82f;
        spec.headHeight = 0.56f;
        spec.headY = 0.86f;
        spec.gunLength = 0.47f;
        spec.gunRadius = 0.045f;
        spec.headShape = ArcadeHeadShape::Angular;
        spec.muzzleBrake = true;
        break;
    case Vehicle::Maus:
        spec.wheelCount = 8;
        spec.trackLength = 1.23f;
        spec.trackHalfWidth = 0.57f;
        spec.trackHeight = 0.54f;
        spec.wheelRadius = 0.230f;
        spec.hullWidth = 0.82f;
        spec.hullLength = 0.74f;
        spec.headWidth = 1.21f;
        spec.headLength = 0.87f;
        spec.headHeight = 0.61f;
        spec.headY = 0.90f;
        spec.gunLength = 0.51f;
        spec.gunRadius = 0.049f;
        spec.headShape = ArcadeHeadShape::Angular;
        spec.muzzleBrake = true;
        spec.skirts = true;
        break;
    case Vehicle::Sdkfz231SixRad:
        spec.wheelCount = 3;
        spec.trackLength = 1.08f;
        spec.trackHalfWidth = 0.49f;
        spec.trackHeight = 0.44f;
        spec.wheelRadius = 0.215f;
        spec.hullWidth = 0.70f;
        spec.hullLength = 0.68f;
        spec.hullHeight = 0.18f;
        spec.headWidth = 0.92f;
        spec.headLength = 0.62f;
        spec.headHeight = 0.38f;
        spec.headY = 0.73f;
        spec.gunLength = 0.26f;
        spec.gunRadius = 0.030f;
        spec.headShape = ArcadeHeadShape::Angular;
        spec.wheeled = true;
        break;
    case Vehicle::PanzerIIIL:
        spec.wheelCount = 6;
        spec.trackLength = 1.10f;
        spec.trackHalfWidth = 0.51f;
        spec.trackHeight = 0.49f;
        spec.wheelRadius = 0.202f;
        spec.headWidth = 1.06f;
        spec.headLength = 0.73f;
        spec.headHeight = 0.48f;
        spec.headY = 0.80f;
        spec.gunLength = 0.37f;
        spec.gunRadius = 0.038f;
        spec.headShape = ArcadeHeadShape::Angular;
        break;
    }
    return spec;
}

inline void arcadeEllipsoid(Vector3 center, Vector3 radii, Color color,
                            int rings = 8, int slices = 16)
{
    rlPushMatrix();
    rlTranslatef(center.x, center.y, center.z);
    rlScalef(radii.x, radii.y, radii.z);
    DrawSphereEx({0.0f, 0.0f, 0.0f}, 1.0f, rings, slices, color);
    rlPopMatrix();
}

inline float arcadeGunStartZ(const ArcadeVehicleSpec &spec)
{
    return spec.headZ - spec.headLength * 0.43f;
}

inline float arcadeMuzzleDistance(const ArcadeVehicleSpec &spec)
{
    return -(arcadeGunStartZ(spec) - spec.gunLength - 0.050f);
}

// A single watertight casting with rounded plan corners and bevelled shoulders.
// Four profile rings replace the old intersecting spheres and duplicate shells.
inline void drawArmorCasting(Vector3 center, Vector3 size, float squareness,
                             Color paint, float roofTaper = 0.81f,
                             float frontTaper = 1.0f, float roofZOffset = 0.0f,
                             float shoulderTaper = 0.97f, float noseDrop = 0.0f)
{
    constexpr int sides = 16;
    constexpr std::array<float, 4> heights{{-0.50f, -0.28f, 0.23f, 0.50f}};
    const std::array<float, 4> scales{{0.83f, 1.0f, shoulderTaper, roofTaper}};
    std::array<Vector3, sides> outline{};
    for (int index = 0; index < sides; ++index)
    {
        const float angle = static_cast<float>(index) * 2.0f * PI / sides;
        const float sine = std::sin(angle);
        const float cosine = std::cos(angle);
        const float x = std::copysign(std::pow(std::abs(sine), squareness), sine);
        const float z = -std::copysign(std::pow(std::abs(cosine), squareness), cosine);
        outline[index] = {x, 1.0f + (frontTaper - 1.0f) * (1.0f - z) * 0.5f, z};
    }
    std::array<std::array<Vector3, sides>, 4> rings{};
    for (std::size_t ring = 0; ring < rings.size(); ++ring)
    {
        for (int index = 0; index < sides; ++index)
        {
            const Vector3 point = outline[index];
            const float roofShift = ring == 3 ? roofZOffset : ring == 2 ? roofZOffset * 0.32f : 0.0f;
            const float drop = noseDrop * std::max(0.0f, -point.z) *
                               (ring == 3 ? 1.0f : ring == 2 ? 0.55f : 0.0f);
            rings[ring][index] = {center.x + point.x * size.x * 0.5f * scales[ring] * point.y,
                                  center.y + heights[ring] * size.y - drop,
                                  center.z + point.z * size.z * 0.5f * scales[ring] + roofShift};
        }
    }
    rlBegin(RL_TRIANGLES);
    for (int index = 0; index < sides; ++index)
    {
        const int next = (index + 1) % sides;
        for (std::size_t ring = 0; ring + 1 < rings.size(); ++ring)
        {
            const Color surface = shade(paint, ring == 0 ? 0.71f : ring == 1 ? 1.0f : 1.14f);
            emitQuad(rings[ring][index], rings[ring + 1][index],
                     rings[ring + 1][next], rings[ring][next], surface);
        }
        const Vector3 top{center.x, center.y + size.y * 0.5f, center.z + roofZOffset};
        const Vector3 bottom{center.x, center.y - size.y * 0.5f, center.z};
        emitTriangle(top, rings.back()[next], rings.back()[index], shade(paint, 1.10f));
        emitTriangle(bottom, rings.front()[index], rings.front()[next], shade(paint, 0.65f));
    }
    rlEnd();
}

inline void drawArmorRivet(Vector3 center, Color color, float radius = 0.019f)
{
    DrawSphereEx(center, radius, 3, 6, color);
}

// The belt is one extruded capsule annulus. Wheels sit inside its opening;
// continuous shoes follow the tangents around both ends instead of box piles.
inline void drawTrackBelt(float side, float x, float centerY, float length,
                          float height, const Palette &p, bool moving,
                          float widthScale = 1.0f)
{
    // A recessed carrier keeps the curved shoes connected without making the
    // entire end turn a solid black disc. The shoe tips define the silhouette.
    constexpr int halfSegments = 9;
    constexpr int points = halfSegments * 2;
    const float radius = height * 0.5f - 0.014f;
    const float straight = length * 0.5f - height * 0.5f;
    const float thickness = 0.040f;
    const float width = 0.323f * widthScale;
    const float leftX = side * x - width * 0.5f;
    const float rightX = side * x + width * 0.5f;
    std::array<Vector3, points> outer{};
    std::array<Vector3, points> inner{};
    for (int index = 0; index < points; ++index)
    {
        const bool rear = index < halfSegments;
        const int local = index % halfSegments;
        const float angle = (rear ? -PI * 0.5f : PI * 0.5f) +
                            PI * static_cast<float>(local) / (halfSegments - 1);
        const float end = rear ? straight : -straight;
        outer[index] = {rightX, centerY + std::sin(angle) * radius,
                        end + std::cos(angle) * radius};
        inner[index] = {rightX, centerY + std::sin(angle) * (radius - thickness),
                        end + std::cos(angle) * (radius - thickness)};
    }
    const Color carrier = material(Color{47, 49, 41, 255}, 10);
    const Color shoePaint = material(Color{104, 99, 85, 255}, 10);
    rlBegin(RL_TRIANGLES);
    for (int index = 0; index < points; ++index)
    {
        const int next = (index + 1) % points;
        Vector3 back = outer[index];
        Vector3 backNext = outer[next];
        Vector3 innerBack = inner[index];
        Vector3 innerBackNext = inner[next];
        back.x = backNext.x = innerBack.x = innerBackNext.x = leftX;
        emitQuad(outer[next], outer[index], inner[index], inner[next], carrier);
        emitQuad(back, backNext, innerBackNext, innerBack, carrier);
        emitQuad(back, outer[index], outer[next], backNext, carrier);
        emitQuad(innerBackNext, inner[next], inner[index], innerBack, carrier);
    }

    // Follow the entire capsule by arc length: every shoe moves through the
    // straight runs and both turns with the same pitch and orientation.
    constexpr int shoes = 24;
    const float run = straight * 2.0f;
    const float arc = PI * radius;
    const float perimeter = (run + arc) * 2.0f;
    const float pitch = perimeter / shoes;
    const float phase = moving ? std::fmod(static_cast<float>(GetTime()) * 0.63f,
                                           perimeter) : 0.0f;
    const std::array<Vector2, 5> profile{{
        {-pitch * 0.47f, -0.012f}, {-pitch * 0.32f, 0.013f},
        {pitch * 0.16f, 0.036f}, {pitch * 0.48f, 0.020f},
        {pitch * 0.45f, -0.012f}}};
    for (int shoe = 0; shoe < shoes; ++shoe)
    {
        float distance = std::fmod((shoe + 0.5f) * pitch + phase, perimeter);
        Vector3 center{side * x, centerY, 0.0f};
        Vector3 tangent{};
        Vector3 outward{};
        if (distance < run)
        {
            center.y += radius;
            center.z = -straight + distance;
            tangent = {0, 0, 1};
            outward = {0, 1, 0};
        }
        else if ((distance -= run) < arc)
        {
            const float angle = PI * 0.5f - distance / radius;
            center.y += std::sin(angle) * radius;
            center.z = straight + std::cos(angle) * radius;
            tangent = {0, -std::cos(angle), std::sin(angle)};
            outward = {0, std::sin(angle), std::cos(angle)};
        }
        else if ((distance -= arc) < run)
        {
            center.y -= radius;
            center.z = straight - distance;
            tangent = {0, 0, -1};
            outward = {0, -1, 0};
        }
        else
        {
            distance -= run;
            const float angle = -PI * 0.5f + distance / radius;
            center.y += std::sin(angle) * radius;
            center.z = -straight - std::cos(angle) * radius;
            tangent = {0, std::cos(angle), std::sin(angle)};
            outward = {0, std::sin(angle), -std::cos(angle)};
        }
        std::array<Vector3, 5> left{};
        std::array<Vector3, 5> right{};
        for (std::size_t point = 0; point < profile.size(); ++point)
        {
            const Vector2 offset = profile[point];
            left[point] = {leftX,
                center.y + tangent.y * offset.x + outward.y * offset.y,
                center.z + tangent.z * offset.x + outward.z * offset.y};
            right[point] = left[point];
            right[point].x = rightX;
        }
        for (std::size_t point = 0; point < profile.size(); ++point)
        {
            const std::size_t next = (point + 1) % profile.size();
            emitQuad(right[point], left[point], left[next], right[next], shoePaint);
        }
        for (std::size_t point = 1; point + 1 < profile.size(); ++point)
        {
            emitTriangle(right[0], right[point], right[point + 1], shoePaint);
            emitTriangle(left[0], left[point + 1], left[point], shade(shoePaint, 0.84f));
        }
    }
    rlEnd();
    box({side * x, centerY - 0.018f, 0.0f},
        {width * 0.62f, height * 0.28f, length * 0.73f}, p.edge);
}

// These categories affect art only; attachment specifications remain immutable.
inline int arcadeVisualTier(const ArcadeVehicleSpec &spec)
{
    if (spec.headShape == ArcadeHeadShape::Casemate || spec.auxiliaryTurret ||
        spec.headWidth > 1.20f)
        return 3;
    if (spec.headY >= 0.85f)
        return 2;
    return spec.headY <= 0.77f ? 0 : 1;
}

// Model-space dimensions use the same units on all three axes. The running
// gear is longer than it is wide; a compact crew cabin sits between a deep
// nose and a visible rear engine deck. Attachment offsets remain separate.
struct ArcadeVisualProfile
{
    float trackLength;
    float trackHeight;
    float trackHalfWidth;
    float trackWidth;
    float hullWidth;
    float hullLength;
    float hullHeight;
    float hullY;
    float turretWidth;
    float turretLength;
    float turretHeight;
    float turretY;
    float turretZ;
    float gunRadius;
    float fenderTop;
};

inline ArcadeVisualProfile arcadeVisualProfile(const ArcadeVehicleSpec &spec)
{
    const int tier = arcadeVisualTier(spec);
    const bool casemate = spec.headShape == ArcadeHeadShape::Casemate;
    const bool soviet = spec.nationStyle == ArcadeNationStyle::Soviet;
    const bool german = spec.nationStyle == ArcadeNationStyle::German;
    constexpr std::array<float, 4> lengths{{1.42f, 1.50f, 1.57f, 1.64f}};
    constexpr std::array<float, 4> halfWidths{{0.375f, 0.392f, 0.408f, 0.426f}};
    constexpr std::array<float, 4> heights{{0.46f, 0.48f, 0.50f, 0.52f}};
    constexpr std::array<float, 4> hullWidths{{0.75f, 0.80f, 0.85f, 0.90f}};
    constexpr std::array<float, 4> hullHeights{{0.32f, 0.34f, 0.355f, 0.37f}};
    constexpr std::array<float, 4> cabinWidths{{0.68f, 0.73f, 0.78f, 0.84f}};
    constexpr std::array<float, 4> cabinLengths{{0.66f, 0.70f, 0.75f, 0.80f}};
    constexpr std::array<float, 4> cabinHeights{{0.41f, 0.435f, 0.46f, 0.485f}};
    constexpr std::array<float, 4> gunRadii{{0.079f, 0.085f, 0.092f, 0.10f}};
    ArcadeVisualProfile art{};
    art.trackLength = lengths[tier] + (soviet ? -0.015f : german ? 0.015f : 0.0f);
    art.trackHeight = heights[tier];
    art.trackHalfWidth = halfWidths[tier];
    art.trackWidth = 0.29f + tier * 0.01f;
    if (spec.wheeled)
    {
        art.trackLength = 1.47f;
        art.trackHeight = 0.44f;
        art.trackWidth = 0.25f;
    }
    art.hullWidth = hullWidths[tier];
    art.hullLength = art.trackLength * 0.88f;
    art.hullHeight = hullHeights[tier];
    art.hullY = 0.385f + tier * 0.025f;
    art.turretWidth = cabinWidths[tier] * (soviet ? 0.97f : 1.0f);
    art.turretLength = cabinLengths[tier];
    art.turretHeight = cabinHeights[tier];
    art.turretY = 0.75f + tier * 0.043f;
    art.turretZ = 0.055f - tier * 0.010f;
    if (casemate)
    {
        art.turretWidth = 0.86f;
        art.turretLength = 0.99f;
        art.turretHeight = 0.38f;
        art.turretY = 0.79f;
        art.turretZ = 0.045f;
    }
    art.gunRadius = gunRadii[tier];
    art.fenderTop = art.trackHeight + 0.065f;
    return art;
}

inline void drawArcadeRunningGear(const ArcadeVehicleSpec &spec,
                                  const Palette &p, bool moving, bool enemy)
{
    const ArcadeVisualProfile art = arcadeVisualProfile(spec);
    const float height = art.trackHeight;
    const float centerY = height * 0.5f + 0.030f;
    const float outer = art.trackHalfWidth + art.trackWidth * 0.5f;
    const float endZ = art.trackLength * 0.5f - height * 0.5f;
    const bool german = spec.nationStyle == ArcadeNationStyle::German;
    const bool casemate = spec.headShape == ArcadeHeadShape::Casemate;
    const float rotation = moving ? static_cast<float>(GetTime()) * 4.5f : 0.0f;
    const Color wheelPaint = shade(p.paint, enemy ? 0.77f : 0.86f);

    // Explicit normals keep the new narrow wheels independent of preceding
    // batches; raylib's immediate cylinder helper does not supply them.
    const auto disc = [](float side, float innerX, float outerX, float y,
                         float z, float radius, Color color, int segments) {
        const Vector3 innerCenter{side * innerX, y, z};
        const Vector3 outerCenter{side * outerX, y, z};
        rlBegin(RL_TRIANGLES);
        for (int segment = 0; segment < segments; ++segment)
        {
            const float angle = segment * 2.0f * PI / segments;
            const float next = ((segment + 1) % segments) * 2.0f * PI / segments;
            const Vector3 a{innerCenter.x, y + std::cos(angle) * radius,
                            z + std::sin(angle) * radius};
            const Vector3 b{innerCenter.x, y + std::cos(next) * radius,
                            z + std::sin(next) * radius};
            const Vector3 c{outerCenter.x, b.y, b.z};
            const Vector3 d{outerCenter.x, a.y, a.z};
            if (side > 0.0f)
            {
                emitQuad(a, b, c, d, color);
                emitTriangle(outerCenter, d, c, color);
                emitTriangle(innerCenter, b, a, color);
            }
            else
            {
                emitQuad(a, d, c, b, color);
                emitTriangle(outerCenter, c, d, color);
                emitTriangle(innerCenter, a, b, color);
            }
        }
        rlEnd();
    };
    for (float side : {-1.0f, 1.0f})
    {
        if (!spec.wheeled)
        {
            if (casemate)
            {
                // Each assault-vehicle side has two distinct narrow belts.
                const float beltWidth = (art.trackWidth - 0.018f) * 0.5f;
                const float offset = (art.trackWidth + 0.018f) * 0.25f;
                for (float beltSide : {-1.0f, 1.0f})
                    drawTrackBelt(side, art.trackHalfWidth + beltSide * offset,
                                   centerY, art.trackLength, height, p, moving,
                                   beltWidth / 0.323f);
            }
            else
                drawTrackBelt(side, art.trackHalfWidth, centerY,
                               art.trackLength, height, p, moving,
                               art.trackWidth / 0.323f);
        }
        const float wheelRadius = height * (spec.wheeled ? 0.43f : 0.315f);
        const float axleZ = spec.wheeled
            ? art.trackLength * 0.5f - wheelRadius - 0.015f : endZ;
        const int wheels = spec.wheeled ? 3 : 2;
        for (int wheel = 0; wheel < wheels; ++wheel)
        {
            const float z = -axleZ + 2.0f * axleZ * wheel / (wheels - 1);
            const float y = spec.wheeled ? wheelRadius + 0.008f
                                         : centerY - 0.004f;
            disc(side, art.trackHalfWidth - art.trackWidth * 0.36f,
                 outer + 0.003f, y, z, wheelRadius, p.rubber, 16);
            disc(side, outer + 0.003f, outer + 0.009f,
                 y, z, wheelRadius * 0.91f, p.edge, 16);
            disc(side, outer + 0.009f, outer + 0.017f,
                 y, z, wheelRadius * 0.77f, wheelPaint, 16);
            disc(side, outer + 0.017f, outer + 0.022f,
                 y, z, wheelRadius * 0.25f, p.steel, 10);
            for (int bolt = 0; bolt < 4; ++bolt)
            {
                const float angle = rotation + bolt * PI * 0.5f;
                disc(side, outer + 0.017f, outer + 0.020f,
                     y + std::cos(angle) * wheelRadius * 0.51f,
                     z + std::sin(angle) * wheelRadius * 0.51f,
                     wheelRadius * 0.075f, p.edge, 6);
            }
        }
        if (!spec.wheeled)
        {
            for (float end : {-1.0f, 1.0f})
            {
                const float z = end * endZ * 0.31f;
                disc(side, art.trackHalfWidth - art.trackWidth * 0.25f,
                     outer - 0.012f, centerY - 0.032f, z,
                     height * 0.16f, p.rubber, 12);
                disc(side, outer - 0.012f, outer - 0.007f,
                     centerY - 0.032f, z, height * 0.068f, p.edge, 8);
            }
        }

        // A curved shoulder wraps the center only. Both large tread turns
        // remain exposed; the full chassis no longer sits on broad flat wings.
        drawArmorCasting({side * art.trackHalfWidth,
                           art.fenderTop - 0.053f, 0.035f},
                          {art.trackWidth * 0.98f, 0.106f,
                           art.trackLength * (spec.wheeled ? 0.71f : 0.46f)},
                          german ? 0.38f : 0.57f, p.paint, 0.84f);
        if (!spec.wheeled)
        {
            const float apronX = side * (outer - 0.018f);
            const float apronLength = art.trackLength * (spec.skirts ? 0.38f : 0.33f);
            drawArmorCasting({apronX, art.fenderTop - 0.112f, 0.035f},
                              {0.058f, 0.220f, apronLength},
                              german ? 0.35f : 0.55f, p.paint, 0.91f);
            drawArmorCasting({apronX, art.fenderTop - 0.296f, 0.035f},
                              {0.054f, 0.132f, apronLength * 0.96f},
                              0.45f, shade(p.paint, 0.86f), 0.94f);
            for (float end : {-1.0f, 1.0f})
                box({side * (outer + 0.012f), art.fenderTop - 0.310f,
                     0.035f + end * apronLength * 0.35f},
                    {0.005f, 0.052f, 0.018f}, p.edge);
        }
    }
}

inline float arcadeVisualTurretWidth(const ArcadeVehicleSpec &spec)
{
    return arcadeVisualProfile(spec).turretWidth;
}

inline float arcadeVisualTurretHeight(const ArcadeVehicleSpec &spec)
{
    return arcadeVisualProfile(spec).turretHeight;
}

inline float arcadeVisualTurretLength(const ArcadeVehicleSpec &spec)
{
    return arcadeVisualProfile(spec).turretLength;
}

inline float arcadeVisualTurretOffsetX(const ArcadeVehicleSpec &spec)
{
    return arcadeVisualTier(spec) == 0 ? -0.030f : 0.0f;
}

inline float arcadeVisualRoofOffsetZ(const ArcadeVehicleSpec &spec)
{
    return spec.headShape == ArcadeHeadShape::Teardrop ? 0.025f : 0.0f;
}

inline void drawStencilDigit(float side, float x, float y, float z,
                             int digit, Color color)
{
    // Original seven-segment paint stencil, built as flush side-face geometry.
    constexpr std::array<unsigned char, 10> segments{{
        0x3f, 0x06, 0x5b, 0x4f, 0x66, 0x6d, 0x7d, 0x07, 0x7f, 0x6f}};
    constexpr std::array<Vector3, 7> centers{{
        {0.0f, 0.041f, 0.0f}, {0.0f, 0.021f, 0.027f},
        {0.0f, -0.021f, 0.027f}, {0.0f, -0.041f, 0.0f},
        {0.0f, -0.021f, -0.027f}, {0.0f, 0.021f, -0.027f},
        {0.0f, 0.0f, 0.0f}}};
    for (int segment = 0; segment < 7; ++segment)
    {
        if ((segments[digit % 10] & (1 << segment)) == 0)
            continue;
        const bool horizontal = segment == 0 || segment == 3 || segment == 6;
        box({side * x, y + centers[segment].y, z + centers[segment].z},
            {0.005f, horizontal ? 0.010f : 0.034f,
             horizontal ? 0.047f : 0.009f}, color);
    }
}

// Rounded crew enclosure with a full vertical middle and a curved crown.
// The six contour rings give the cabin volume in elevation as well as plan.
inline void drawCrewCabin(Vector3 center, Vector3 size, float squareness,
                          float crownShift, Color paint)
{
    constexpr int sides = 16;
    constexpr std::array<float, 6> heights{{-0.50f, -0.37f, -0.10f, 0.24f, 0.43f, 0.50f}};
    constexpr std::array<float, 6> scales{{0.76f, 0.94f, 1.00f, 0.98f, 0.84f, 0.57f}};
    std::array<std::array<Vector3, sides>, heights.size()> rings{};
    for (int index = 0; index < sides; ++index)
    {
        const float angle = index * 2.0f * PI / sides;
        const float sine = std::sin(angle), cosine = std::cos(angle);
        const float x = std::copysign(std::pow(std::fabs(sine), squareness), sine);
        const float z = -std::copysign(std::pow(std::fabs(cosine), squareness), cosine);
        for (std::size_t ring = 0; ring < rings.size(); ++ring)
            rings[ring][index] = {
                center.x + x * size.x * 0.5f * scales[ring],
                center.y + heights[ring] * size.y,
                center.z + z * size.z * 0.5f * scales[ring] +
                    crownShift * std::max(0.0f, heights[ring] * 2.0f)};
    }
    // Smooth normals across the cast shoulders retain a rounded volume at the
    // same mesh resolution, rather than lighting every ring like a separate lid.
    std::array<std::array<Vector3, sides>, heights.size()> normals{};
    for (std::size_t ring = 0; ring < rings.size(); ++ring)
    {
        const std::size_t below = ring == 0 ? 0 : ring - 1;
        const std::size_t above = std::min(ring + 1, rings.size() - 1);
        for (int index = 0; index < sides; ++index)
        {
            const Vector3 tangent = subtract(rings[ring][(index + 1) % sides],
                                               rings[ring][(index + sides - 1) % sides]);
            const Vector3 vertical = subtract(rings[above][index], rings[below][index]);
            normals[ring][index] = normalized(cross(vertical, tangent));
        }
    }
    const auto vertex = [](Vector3 point, Vector3 normal, Color color) {
        rlColor4ub(color.r, color.g, color.b, color.a);
        rlNormal3f(normal.x, normal.y, normal.z);
        rlVertex3f(point.x, point.y, point.z);
    };
    rlBegin(RL_TRIANGLES);
    for (int index = 0; index < sides; ++index)
    {
        const int next = (index + 1) % sides;
        for (std::size_t ring = 0; ring + 1 < rings.size(); ++ring)
        {
            const Color color = shade(paint, ring == 0 ? 0.80f : ring >= 3 ? 1.04f : 1.0f);
            vertex(rings[ring][index], normals[ring][index], color);
            vertex(rings[ring + 1][index], normals[ring + 1][index], color);
            vertex(rings[ring + 1][next], normals[ring + 1][next], color);
            vertex(rings[ring][index], normals[ring][index], color);
            vertex(rings[ring + 1][next], normals[ring + 1][next], color);
            vertex(rings[ring][next], normals[ring][next], color);
        }
        vertex({center.x, center.y + size.y * 0.5f, center.z + crownShift}, {0, 1, 0}, paint);
        vertex(rings.back()[next], normals.back()[next], paint);
        vertex(rings.back()[index], normals.back()[index], paint);
        emitTriangle({center.x, center.y - size.y * 0.5f, center.z},
                     rings.front()[index], rings.front()[next], shade(paint, 0.75f));
    }
    rlEnd();
}

inline void drawArcadeHead(const ArcadeVehicleSpec &spec, const Palette &p, bool enemy)
{
    const ArcadeVisualProfile art = arcadeVisualProfile(spec);
    const float width = art.turretWidth, length = art.turretLength;
    const float height = art.turretHeight;
    const bool casemate = spec.headShape == ArcadeHeadShape::Casemate;
    const bool german = spec.nationStyle == ArcadeNationStyle::German;
    const bool soviet = spec.nationStyle == ArcadeNationStyle::Soviet;
    const bool teardrop = spec.headShape == ArcadeHeadShape::Teardrop;
    const float offsetX = arcadeVisualTurretOffsetX(spec);
    const Color paint = enemy ? shade(p.paint, 0.95f) : p.paint;
    if (casemate)
    {
        chamferedFrustum({0.0f, art.turretY, art.turretZ}, width, length,
                         width * 0.86f, length * 0.82f, height, 0.025f,
                         0.17f, 0.10f, paint);
    }
    else
    {
        const float bottom = art.turretY - height * 0.5f;
        cylinder({offsetX, bottom - 0.025f, art.turretZ},
                 {offsetX, bottom + 0.025f, art.turretZ},
                 width * 0.33f, p.darkPaint, 16);
        drawCrewCabin({offsetX, art.turretY, art.turretZ},
                       {width, height, length}, german ? 0.38f : teardrop ? 0.78f : 0.58f,
                       teardrop ? 0.045f : german ? 0.010f : 0.020f, paint);
    }

    // Inset service doors sit on the rear cheeks, with small brass fasteners.
    // They read as part of the enclosure instead of broad floating black plates.
    for (float side : {-1.0f, 1.0f})
    {
        const float plateX = offsetX + side * width * 0.479f;
        const float plateY = art.turretY - height * 0.015f;
        const float plateZ = art.turretZ + length * 0.16f;
        drawArmorCasting({plateX, plateY, plateZ},
                          {0.052f, height * 0.52f, length * 0.29f},
                          0.70f, shade(paint, 0.88f), 0.84f);
        for (float vertical : {-1.0f, 1.0f})
        {
            cylinder({plateX, plateY + vertical * height * 0.135f, plateZ - length * 0.06f},
                     {plateX + side * 0.032f, plateY + vertical * height * 0.135f, plateZ - length * 0.06f},
                     0.022f, material(Color{165, 145, 92, 255}, 10), 8);
        }
        const Color stencil = material(Color{207, 211, 181, 255}, 9);
        drawStencilDigit(side, side * plateX + 0.030f, plateY,
                          plateZ + length * 0.055f, german ? 3 : soviet ? 2 : 1, stencil);
    }
    if (!casemate)
    {
        // One recessed viewing slit above the cannon, under the rounded brow.
        const float visorY = art.turretY + height * 0.40f;
        const float visorZ = art.turretZ - length * 0.432f +
                            (teardrop ? 0.036f : german ? 0.008f : 0.016f);
        box({offsetX, visorY, visorZ}, {width * 0.40f, 0.032f, 0.028f}, p.edge);
        box({offsetX, visorY + 0.002f, visorZ - 0.016f},
            {width * 0.32f, 0.018f, 0.009f}, p.optic);
        // Protective tubular frames tie the front cheeks to the lower collar.
        for (float side : {-1.0f, 1.0f})
        {
            const Vector3 foot{offsetX + side * width * 0.38f,
                               art.turretY - height * 0.39f,
                               art.turretZ - length * 0.40f};
            const Vector3 elbow{offsetX + side * width * 0.43f,
                                art.turretY + height * 0.12f,
                                art.turretZ - length * 0.45f};
            const Vector3 brow{offsetX + side * width * 0.34f,
                               art.turretY + height * 0.39f,
                               art.turretZ - length * 0.31f};
            cylinder(foot, elbow, 0.020f, p.darkPaint, 8);
            cylinder(elbow, brow, 0.020f, p.paint, 8);
            DrawSphereEx(elbow, 0.022f, 4, 8, p.paint);
        }
    }
}

// Hollow muzzle with an actual recessed bore, so its opening stays circular
// from every angle. The rim ends at the established visual muzzle plane.
inline void drawArcadeGun(const ArcadeVehicleSpec &spec, const Palette &p,
                          bool enemy)
{
    const ArcadeVisualProfile art = arcadeVisualProfile(spec);
    const float y = spec.headY;
    const float start = art.turretZ - art.turretLength * 0.475f;
    const float tip = -arcadeMuzzleDistance(spec);
    const int tier = arcadeVisualTier(spec);
    const float radius = art.gunRadius;
    const float mantletRadius = radius * (tier >= 2 ? 1.36f : 1.42f);
    const bool german = spec.nationStyle == ArcadeNationStyle::German;
    const Color barrel = shade(p.paint, enemy ? 0.68f : 0.84f);
    const Color brake = material(Color{62, 71, 65, 255}, 10);
    const Color lip = material(Color{118, 130, 110, 255}, 10);
    if (german && tier >= 2)
        drawArmorCasting({0.0f, y, start + 0.023f},
                          {mantletRadius * 2.55f, mantletRadius * 1.82f, 0.14f},
                          0.25f, p.darkPaint, 0.88f);
    cylinder({0.0f, y, start + 0.13f}, {0.0f, y, start - 0.052f},
             mantletRadius, p.darkPaint, 16);
    cylinder({0.0f, y, start + 0.080f}, {0.0f, y, start - 0.021f},
             mantletRadius * 1.08f, p.paint, 16);
    cylinder({0.0f, y, start - 0.035f}, {0.0f, y, tip + 0.125f},
             radius, barrel, 16);
    // A broad painted recoil sleeve and a narrow steel collar replace the
    // uniform bright tube. Its darker brake has a visibly separate mechanism.
    cylinder({0.0f, y, start - 0.047f},
             {0.0f, y, std::max(tip + 0.14f, start - (start - tip) * 0.43f)},
             radius * 1.12f, p.paint, 12);
    cylinder({0.0f, y, start - 0.045f}, {0.0f, y, start - 0.076f},
             radius * 1.17f, p.edge, 16);

    const float outerRadius = radius * (spec.muzzleBrake ? 1.27f : 1.12f);
    const float boreRadius = radius * 0.65f;
    const int sides = german && spec.muzzleBrake ? 8 : 12;
    rlBegin(RL_TRIANGLES);
    for (int index = 0; index < sides; ++index)
    {
        const float angle = index * 2.0f * PI / sides;
        const float next = (index + 1) * 2.0f * PI / sides;
        const auto vertex = [&](float a, float r, float z) {
            return Vector3{std::cos(a) * r, y + std::sin(a) * r, z};
        };
        const auto outerVertex = [&](float a, float z) {
            if (!german || !spec.muzzleBrake)
                return vertex(a, outerRadius, z);
            const float x = std::cos(a);
            const float up = std::sin(a);
            return Vector3{std::clamp(x * 1.20f, -1.0f, 1.0f) * outerRadius,
                           y + std::clamp(up * 1.20f, -1.0f, 1.0f) * outerRadius * 0.85f, z};
        };
        const Vector3 a = outerVertex(angle, tip);
        const Vector3 b = outerVertex(next, tip);
        const Vector3 c = outerVertex(next, tip + 0.13f);
        const Vector3 d = outerVertex(angle, tip + 0.13f);
        emitQuad(a, b, c, d, spec.muzzleBrake ? brake : barrel);
        emitQuad(a, vertex(angle, boreRadius, tip), vertex(next, boreRadius, tip), b, lip);
        emitQuad(vertex(angle, boreRadius, tip), vertex(angle, boreRadius, tip + 0.10f),
                 vertex(next, boreRadius, tip + 0.10f),
                 vertex(next, boreRadius, tip), p.edge);
    }
    rlEnd();
    cylinder({0.0f, y, tip + 0.105f}, {0.0f, y, tip + 0.097f},
             boreRadius, p.rubber, sides);
    if (spec.muzzleBrake)
    {
        // Recessed side vents suggest a brake without turning the short cannon
        // into an oversized white ring or a science-fiction weapon.
        for (float side : {-1.0f, 1.0f})
            for (float offset : {0.038f, 0.091f})
                box({side * outerRadius * 0.975f, y, tip + offset},
                    {0.010f, radius * 0.75f, 0.026f}, p.edge);
    }
}

inline void drawArcadeVehicle(Vehicle vehicle, const Palette &p,
                              Color identity, bool moving, bool enemy)
{
    const ArcadeVehicleSpec spec = arcadeVehicleSpec(vehicle);
    const ArcadeVisualProfile art = arcadeVisualProfile(spec);
    const bool german = spec.nationStyle == ArcadeNationStyle::German;
    const bool soviet = spec.nationStyle == ArcadeNationStyle::Soviet;
    const bool casemate = spec.headShape == ArcadeHeadShape::Casemate;
    const float width = art.hullWidth, length = art.hullLength;
    const float hullTop = art.hullY + art.hullHeight * 0.5f;
    const float roofY = art.turretY + art.turretHeight * 0.5f;
    const float roofX = arcadeVisualTurretOffsetX(spec);
    const float roofZ = art.turretZ + (spec.headShape == ArcadeHeadShape::Teardrop ? 0.045f : 0.020f);
    const float noseZ = -length * 0.5f;

    drawArcadeRunningGear(spec, p, moving, enemy);
    // The hull has a deep belly between the tracks, a descending cast nose,
    // and an exposed engine deck. It is a continuous volume, not a high shelf.
    drawArmorCasting({0.0f, art.hullY - 0.055f, -0.015f},
                      {width * 0.77f, art.hullHeight * 0.87f, length * 0.94f},
                      0.65f, p.darkPaint, 0.91f);
    drawArmorCasting({0.0f, art.hullY, 0.0f},
                      {width, art.hullHeight, length},
                      german ? 0.36f : 0.65f, p.paint, 0.77f, 0.90f,
                      0.0f, 0.97f, 0.11f);
    drawArmorCasting({0.0f, art.hullY - 0.065f, noseZ + 0.055f},
                      {width * 0.73f, art.hullHeight * 0.92f, 0.255f},
                      0.68f, p.paint, 0.70f);
    // Three cast ribs wrap the lower transmission cover down toward the ground.
    for (int rib = -1; rib <= 1; ++rib)
        drawArmorCasting({rib * width * 0.185f, art.hullY - 0.072f, noseZ - 0.051f},
                          {0.069f, art.hullHeight * 0.64f, 0.055f},
                          0.68f, shade(p.paint, 0.86f), 0.78f);
    for (float side : {-1.0f, 1.0f})
    {
        drawArmorRivet({side * width * 0.27f, art.hullY + 0.051f, noseZ - 0.028f},
                       p.lightPaint, 0.023f);
        cylinder({side * width * 0.26f, art.hullY - 0.043f, noseZ - 0.045f},
                 {side * width * 0.26f, art.hullY - 0.043f, noseZ - 0.083f},
                 0.025f, p.steel, 8);
    }
    // A raised driver cover gives the nose its sloping top contour.
    rlPushMatrix();
    rlTranslatef(-width * 0.11f, hullTop - 0.075f, -length * 0.32f);
    rlRotatef(-12.0f, 1.0f, 0.0f, 0.0f);
    drawArmorCasting({0.0f, 0.0f, 0.0f},
                      {width * 0.34f, 0.070f, length * 0.23f},
                      0.62f, shade(p.paint, 0.94f), 0.70f);
    box({0.0f, 0.021f, -length * 0.111f},
        {width * 0.21f, 0.023f, 0.018f}, p.edge);
    rlPopMatrix();
    cylinder({width * 0.24f, art.hullY + 0.028f, noseZ - 0.010f},
             {width * 0.24f, art.hullY + 0.028f, noseZ - 0.046f},
             0.036f, p.edge, 10);
    cylinder({width * 0.24f, art.hullY + 0.028f, noseZ - 0.048f},
             {width * 0.24f, art.hullY + 0.028f, noseZ - 0.053f},
             0.024f, material(mix(identity, WHITE, 0.26f), 12), 10);
    box({0.0f, art.hullY + 0.044f, noseZ - 0.074f},
        {width * 0.19f, 0.026f, 0.008f}, material(identity, 12));

    drawArcadeHead(spec, p, enemy);
    drawArcadeGun(spec, p, enemy);

    const int hatches = casemate ? 2 : 1;
    for (int hatch = 0; hatch < hatches; ++hatch)
    {
        const float x = roofX + (casemate ? (hatch == 0 ? -0.21f : 0.21f) : -0.045f);
        const float z = roofZ + (casemate ? 0.10f : 0.025f);
        const float radius = casemate ? 0.082f : 0.112f;
        cylinder({x, roofY - 0.018f, z}, {x, roofY + 0.017f, z}, radius, p.darkPaint, 12);
        cylinder({x, roofY + 0.017f, z}, {x, roofY + 0.044f, z}, radius * 0.89f, p.paint, 12);
        box({x, roofY + 0.051f, z}, {0.067f, 0.016f, 0.017f}, p.steel);
    }
    if (!casemate)
    {
        // Offset roof equipment keeps the upper silhouette asymmetrical.
        const float x = roofX + art.turretWidth * 0.22f;
        if (german)
        {
            drawArmorCasting({x, roofY + 0.045f, roofZ - 0.065f},
                              {0.085f, 0.13f, 0.10f}, 0.30f, p.darkPaint, 0.70f);
            box({x, roofY + 0.064f, roofZ - 0.112f},
                {0.052f, 0.033f, 0.010f}, p.optic);
        }
        else
        {
            cylinder({x, roofY - 0.006f, roofZ - 0.051f},
                     {x, roofY + 0.063f, roofZ - 0.051f}, 0.029f, p.darkPaint, 8);
            cylinder({x, roofY + 0.099f, roofZ - 0.010f},
                     {x, roofY + 0.099f, roofZ - 0.105f}, 0.061f, p.paint, 12);
            cylinder({x, roofY + 0.099f, roofZ - 0.106f},
                     {x, roofY + 0.099f, roofZ - 0.112f}, 0.046f, p.edge, 12);
            cylinder({x, roofY + 0.099f, roofZ - 0.113f},
                     {x, roofY + 0.099f, roofZ - 0.116f}, 0.033f,
                     material(Color{161, 180, 155, 255}, 12), 12);
        }
    }

    // Rear machinery occupies the space behind the cabin instead of spreading
    // the silhouette sideways beyond the tracks.
    const float engineZ = length * 0.36f;
    drawArmorCasting({0.0f, hullTop + 0.012f, engineZ},
                      {width * 0.58f, 0.11f, length * 0.25f},
                      0.45f, shade(p.paint, 0.88f), 0.76f);
    for (int vent = 0; vent < 5; ++vent)
        box({0.0f, hullTop + 0.061f, engineZ - 0.075f + vent * 0.038f},
            {width * 0.35f, 0.008f, 0.018f}, p.edge);
    for (float side : {-1.0f, 1.0f})
    {
        const float x = side * width * 0.40f;
        const float z = length * 0.34f;
        cylinder({x, hullTop - 0.047f, z}, {x, hullTop + 0.21f, z},
                 soviet ? 0.035f : 0.041f, p.steel, 10);
        cylinder({x, hullTop + 0.19f, z}, {x, hullTop + 0.235f, z + 0.042f},
                 0.036f, p.edge, 10);
        cylinder({x, hullTop + 0.035f, z}, {x, hullTop + 0.12f, z},
                 0.050f, p.darkPaint, 10);
    }
    if (!enemy)
    {
        drawAntenna(roofX - art.turretWidth * 0.23f,
                    roofZ + art.turretLength * 0.19f,
                    roofY - 0.015f, 0.19f, p);
        if (soviet)
        {
            for (float side : {-1.0f, 1.0f})
            {
                const float x = side * art.trackHalfWidth;
                cylinder({x, art.fenderTop + 0.047f, length * 0.23f},
                         {x, art.fenderTop + 0.047f, length * 0.45f}, 0.058f, p.darkPaint, 10);
                for (float z : {length * 0.26f, length * 0.40f})
                    cylinder({x, art.fenderTop + 0.047f, z},
                             {x, art.fenderTop + 0.047f, z + 0.018f},
                             0.063f, p.steel, 10);
            }
        }
        else if (german)
        {
            drawArmorCasting({0.0f, art.turretY - 0.035f,
                               art.turretZ + art.turretLength * 0.43f},
                              {art.turretWidth * 0.60f, 0.115f, 0.115f},
                              0.35f, p.canvas, 0.82f);
        }
        else
        {
            cylinder({-width * 0.20f, hullTop + 0.085f, length * 0.45f},
                     {width * 0.20f, hullTop + 0.085f, length * 0.45f},
                     0.055f, p.canvas, 10);
        }
    }
    if (spec.auxiliaryTurret)
    {
        const float x = -art.trackHalfWidth;
        drawCrewCabin({x, art.fenderTop + 0.076f, -length * 0.25f},
                       {0.19f, 0.17f, 0.21f}, 0.60f, 0.0f, p.lightPaint);
        cylinder({x, art.fenderTop + 0.070f, -length * 0.31f},
                 {x, art.fenderTop + 0.070f, -length * 0.44f},
                 0.022f, p.steel, 8);
    }
    if (vehicle == Vehicle::Maus)
        cylinder({art.turretWidth * 0.20f, spec.headY - 0.025f,
                  art.turretZ - art.turretLength * 0.45f},
                 {art.turretWidth * 0.20f, spec.headY - 0.025f,
                  art.turretZ - art.turretLength * 0.45f - 0.19f},
                 0.026f, p.steel, 10);
}

inline void drawRecognitionMarker(Color identity, bool enemy, int type)
{
    if (!enemy)
    {
        box({0.0f, 0.585f, -0.694f}, {0.260f, 0.032f, 0.025f},
            mix(identity, WHITE, 0.24f));
        return;
    }
    static constexpr std::array<Color, 4> markers{{
        Color{80, 168, 255, 255}, Color{63, 255, 183, 255},
        Color{255, 89, 48, 255}, Color{255, 193, 62, 255}}};
    const Color marker = markers[static_cast<std::size_t>(std::clamp(type, 0, 3))];
    box({0.0f, 0.625f, 0.745f}, {0.245f, 0.045f, 0.026f}, marker);
}

inline void drawShield(float shield, int identity)
{
    if (shield <= 0.0f)
        return;
    const float phase = static_cast<float>(GetTime()) * 4.8f + identity * 0.73f;
    const float pulse = std::sin(phase) * 0.035f;
    const float fade = std::min(shield, 0.35f) / 0.35f;
    const Color glow{96, 215, 234,
                     static_cast<unsigned char>(155.0f * fade)};
    // A segmented protection ring leaves the vehicle readable. The scene
    // submits it unlit, with depth testing but without transparent depth writes.
    constexpr int segments = 32;
    rlBegin(RL_TRIANGLES);
    for (int segment = 0; segment < segments; ++segment)
    {
        if (segment % 8 >= 6)
            continue;
        const float a = segment * 2.0f * PI / segments;
        const float b = (segment + 1) * 2.0f * PI / segments;
        const auto point = [pulse](float angle, float radius) {
            return Vector3{std::sin(angle) * (radius + pulse), 0.065f,
                           std::cos(angle) * (radius + pulse)};
        };
        emitQuad(point(a, 0.965f), point(a, 1.015f),
                 point(b, 1.015f), point(b, 0.965f), glow);
    }
    rlEnd();
    for (int corner = 0; corner < 4; ++corner)
    {
        const float angle = (corner + 0.5f) * PI * 0.5f;
        const Vector3 mote{std::sin(angle) * 0.92f,
                           0.26f + 0.07f * std::sin(phase + corner),
                           std::cos(angle) * 0.92f};
        DrawCube(mote, 0.040f, 0.085f, 0.040f, glow);
    }
}

} // namespace detail

inline Vehicle playerVehicle(Nation nation, int level)
{
    const int tier = std::clamp(level, 0, 3);
    switch (nation)
    {
    case Nation::UnitedStates:
        switch (tier)
        {
        case 0: return Vehicle::M24Chaffee;
        case 1: return Vehicle::M4A3Sherman;
        case 2: return Vehicle::M26Pershing;
        default: return Vehicle::T28T95;
        }
    case Nation::SovietUnion:
        switch (tier)
        {
        case 0: return Vehicle::T70;
        case 1: return Vehicle::T3485;
        case 2: return Vehicle::IS2;
        default: return Vehicle::KV5Project;
        }
    case Nation::Germany:
        switch (tier)
        {
        case 0: return Vehicle::PanzerIIF;
        case 1: return Vehicle::PanzerIVH;
        case 2: return Vehicle::TigerIE;
        default: return Vehicle::Maus;
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
    if (nation == Nation::Germany)
        return enemyVehicle(type);

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
    case Vehicle::M4A3Sherman: return "M4A3(76)W SHERMAN";
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
    }
    return "WWII AFV";
}

inline const char *playerVehicleName(Nation nation, int level)
{
    return nameForVehicle(playerVehicle(nation, level));
}

inline const char *vehicleName(bool enemy, int identity)
{
    return nameForVehicle(vehicleFor(enemy, identity));
}

inline float muzzleDistanceForVehicle(Vehicle vehicle)
{
    return detail::arcadeMuzzleDistance(detail::arcadeVehicleSpec(vehicle));
}

inline float muzzleHeightForVehicle(Vehicle vehicle)
{
    return detail::arcadeVehicleSpec(vehicle).headY;
}

inline float playerMuzzleDistance(Nation nation, int level)
{
    return muzzleDistanceForVehicle(playerVehicle(nation, level));
}

inline float playerMuzzleHeight(Nation nation, int level)
{
    return muzzleHeightForVehicle(playerVehicle(nation, level));
}

inline float enemyMuzzleDistance(int type)
{
    return muzzleDistanceForVehicle(enemyVehicle(type));
}

inline float enemyMuzzleHeight(int type)
{
    return muzzleHeightForVehicle(enemyVehicle(type));
}

inline float enemyMuzzleDistance(Nation nation, int type)
{
    return muzzleDistanceForVehicle(enemyVehicle(nation, type));
}

inline float enemyMuzzleHeight(Nation nation, int type)
{
    return muzzleHeightForVehicle(enemyVehicle(nation, type));
}

inline float muzzleDistance(bool enemy, int identity)
{
    return muzzleDistanceForVehicle(vehicleFor(enemy, identity));
}

inline float muzzleHeight(bool enemy, int identity)
{
    return muzzleHeightForVehicle(vehicleFor(enemy, identity));
}

inline void DrawTank(float x, float z, float yaw, Color bodyColor, bool enemy,
                     int armor, float shield, int identity, bool moving,
                     Nation nation = Nation::UnitedStates)
{
    nation = tanks3d::core::normalizedNation(nation);
    const Vehicle vehicle = enemy ? enemyVehicle(nation, identity)
                                  : playerVehicle(nation, armor);
    const Color armorColor = enemy
                                 ? bodyColor
                                 : detail::nationalPlayerPaint(bodyColor,
                                                               nation);
    const detail::Palette colors = detail::palette(armorColor, enemy);
    const Color identityColor = enemy
                                    ? detail::mix(bodyColor, WHITE, 0.22f)
                                    : (identity & 1) == 0
                                          ? Color{255, 219, 78, 255}
                                          : Color{91, 255, 172, 255};

    rlPushMatrix();
    rlTranslatef(x, 0.0f, z);
    rlRotatef(-yaw * RAD2DEG, 0.0f, 1.0f, 0.0f);

    // Small suspension movement keeps the heavy chassis planted. Geometry is
    // never stretched and the neutral pose uses the exact attachment scale.
    rlPushMatrix();
    if (moving)
    {
        const float phase = static_cast<float>(GetTime()) * 10.8f +
                            identity * 0.71f + x * 0.31f + z * 0.17f;
        rlTranslatef(0.0f, 0.005f + std::sin(phase * 2.0f) * 0.005f, 0.0f);
        rlRotatef(std::sin(phase) * 0.55f, 0.0f, 0.0f, 1.0f);
        rlRotatef(std::sin(phase + 0.95f) * 0.65f, 1.0f, 0.0f, 0.0f);
    }

    detail::drawArcadeVehicle(vehicle, colors, identityColor, moving, enemy);

    rlPopMatrix();
    detail::drawShield(shield, identity);
    rlPopMatrix();
}

} // namespace wwii_tank_model

#endif // TANKS3D_WWII_TANK_MODEL_H
