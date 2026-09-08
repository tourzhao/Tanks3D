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
    // Pull saturated sprite colors toward plausible period field paint while
    // retaining the instantly readable P1/P2/enemy identity of the 2D game.
    const Color militaryNeutral = enemy ? Color{86, 95, 82, 255}
                                        : Color{104, 101, 77, 255};
    const Color paint = mix(arcadeColor, militaryNeutral, enemy ? 0.40f : 0.12f);
    // Tags 9..13 select the vehicle-only cel-lighting path in SceneLighting;
    // world materials keep their original 1..8 PBR tags.
    // Military-cartoon contrast: broad panels remain plausible field paint,
    // while recesses and exposed edges separate cleanly at gameplay scale.
    return {material(paint, 9),
            material(shade(paint, enemy ? 0.34f : 0.48f), 9),
            material(mix(shade(paint, enemy ? 1.30f : 1.42f),
                         Color{230, 213, 157, 255}, enemy ? 0.18f : 0.12f), 9),
            material(shade(paint, 0.18f), 10),
            material(Color{12, 15, 14, 255}, 11),
            material(Color{55, 62, 61, 255}, 10),
            material(Color{94, 101, 88, 255}, 10),
            material(Color{42, 137, 154, 255}, 12),
            material(mix(paint, Color{67, 71, 57, 255}, 0.55f), 13)};
}

inline Color nationalPlayerPaint(Color identity, Nation nation)
{
    Color national = Color{112, 116, 70, 255};
    switch (nation)
    {
    case Nation::SovietUnion:
        national = Color{66, 105, 63, 255};
        break;
    case Nation::Germany:
        national = Color{154, 125, 72, 255};
        break;
    case Nation::UnitedStates:
    case Nation::Count:
        break;
    }
    // P1/P2 identity remains on lamps and trim, while the broad armor panels
    // finally carry a nation-readable field color.
    return mix(identity, national, 0.72f);
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

// Runtime vehicles use a deliberately caricatured arcade skeleton.  The
// detailed WWII builders above remain the source for model-specific cues, but
// their realistic hull proportions cannot produce the big-head/small-body
// silhouette of a hand-drawn 1990s action-game vehicle at gameplay scale.
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

inline void drawArcadeRunningGear(const ArcadeVehicleSpec &spec,
                                  const Palette &p, bool moving, bool enemy)
{
    const float centerY = 0.035f + spec.trackHeight * 0.5f;
    const float outerX = spec.trackHalfWidth + 0.185f;
    const int phase = moving ? static_cast<int>(GetTime() * 16.0) : 0;
    const Color treadBase = enemy
                                ? material(Color{50, 55, 55, 255}, 10)
                                : material(Color{105, 70, 43, 255}, 10);

    for (float side : {-1.0f, 1.0f})
    {
        if (!spec.wheeled)
        {
            const float capZ = spec.trackLength * 0.225f;
            const float endZ = spec.trackLength * 0.315f;
            box({side * spec.trackHalfWidth, centerY, 0.0f},
                {0.340f, spec.trackHeight * 0.90f,
                 spec.trackLength * 0.64f},
                p.rubber);
            for (float end : {-1.0f, 1.0f})
                arcadeEllipsoid({side * spec.trackHalfWidth, centerY,
                                 end * endZ},
                                {0.174f, spec.trackHeight * 0.50f, capZ},
                                p.rubber, 8, 16);

            if (!enemy)
            {
                if (spec.nationStyle == ArcadeNationStyle::American)
                {
                    // American suspension keeps the rounded painted pods and
                    // paired medium wheels of the supplied arcade reference.
                    arcadeEllipsoid({side * (spec.trackHalfWidth + 0.012f),
                                     centerY + spec.trackHeight * 0.25f,
                                     spec.trackLength * 0.060f},
                                    {0.190f, spec.trackHeight * 0.33f,
                                     spec.trackLength * 0.375f},
                                    p.paint, 7, 14);
                    box({side * (spec.trackHalfWidth + 0.105f),
                         centerY + spec.trackHeight * 0.25f,
                         spec.trackLength * 0.065f},
                        {0.070f, spec.trackHeight * 0.40f,
                         spec.trackLength * 0.62f}, p.lightPaint);
                }
                else if (spec.nationStyle == ArcadeNationStyle::Soviet)
                {
                    // Soviet tanks expose their large Christie-style wheels
                    // beneath one thin sloping fender, making the track run
                    // visibly lighter than the American pod.
                    box({side * (spec.trackHalfWidth + 0.035f),
                         centerY + spec.trackHeight * 0.43f,
                         -spec.trackLength * 0.015f},
                        {0.315f, 0.085f, spec.trackLength * 0.82f},
                        p.paint);
                    box({side * (spec.trackHalfWidth + 0.115f),
                         centerY + spec.trackHeight * 0.36f,
                         -spec.trackLength * 0.035f},
                        {0.050f, 0.115f, spec.trackLength * 0.72f},
                        p.lightPaint);
                }
                else
                {
                    // German running gear uses squared side armor and many
                    // small overlapping wheels, a hard planar counterpoint to
                    // both other national silhouettes.
                    box({side * (spec.trackHalfWidth + 0.040f),
                         centerY + spec.trackHeight * 0.31f,
                         spec.trackLength * 0.025f},
                        {0.330f, spec.trackHeight * 0.28f,
                         spec.trackLength * 0.72f}, p.paint);
                    for (int seam = -1; seam <= 1; ++seam)
                        box({side * (spec.trackHalfWidth + 0.216f),
                             centerY + spec.trackHeight * 0.31f,
                             seam * spec.trackLength * 0.235f},
                            {0.025f, spec.trackHeight * 0.22f, 0.025f},
                            p.edge);
                }
            }
            else
            {
                // Enemy vehicles expose their mechanical side plates instead
                // of sharing the friendly painted-pod silhouette.
                box({side * (outerX + 0.025f), centerY + 0.010f, 0.025f},
                    {0.055f, spec.trackHeight * 0.66f,
                     spec.trackLength * 0.66f}, p.steel);
                for (float rivetZ : {-0.24f, 0.0f, 0.24f})
                    discOnSide(side, outerX + 0.050f, outerX + 0.075f,
                               centerY + spec.trackHeight * 0.25f,
                               rivetZ * spec.trackLength, 0.030f,
                               p.lightPaint, 8);
            }
        }

        const float wheelRange = spec.trackLength *
            (spec.wheelCount >= 5 ? 0.68f : 0.56f);
        float wheelScale = 1.0f;
        if (!spec.wheeled && spec.wheelCount >= 8)
            wheelScale = spec.nationStyle == ArcadeNationStyle::German
                             ? 0.58f : 0.64f;
        else if (!spec.wheeled && spec.wheelCount >= 6)
            wheelScale = spec.nationStyle == ArcadeNationStyle::Soviet
                             ? 0.88f : 0.76f;
        else if (!spec.wheeled && spec.wheelCount >= 5 &&
                 spec.nationStyle != ArcadeNationStyle::Soviet)
            wheelScale = 0.88f;
        const float visualWheelRadius = spec.wheelRadius * wheelScale;
        for (int wheel = 0; wheel < spec.wheelCount; ++wheel)
        {
            const float amount = spec.wheelCount == 1
                                     ? 0.5f
                                     : static_cast<float>(wheel) /
                                           static_cast<float>(spec.wheelCount - 1);
            const float wheelZ = -wheelRange * 0.5f + amount * wheelRange;
            const float innerX = spec.trackHalfWidth - 0.145f;
            discOnSide(side, innerX, outerX, centerY - 0.010f,
                       wheelZ,
                       visualWheelRadius * (spec.wheeled ? 1.12f : 1.02f),
                       p.rubber, 20);
            discOnSide(side, outerX + 0.002f, outerX + 0.020f,
                       centerY - 0.010f, wheelZ,
                       visualWheelRadius * (spec.wheeled ? 0.92f : 0.78f),
                       ((wheel + phase) & 1) == 0 ? p.lightPaint : p.wheel,
                       18);
            discOnSide(side, outerX + 0.021f, outerX + 0.030f,
                       centerY - 0.010f, wheelZ,
                       visualWheelRadius * 0.25f, p.edge, 14);
        }

        if (!spec.wheeled)
        {
            constexpr int shoes = 10;
            for (int shoe = 0; shoe < shoes; ++shoe)
            {
                const float amount = static_cast<float>(shoe) /
                                     static_cast<float>(shoes - 1);
                const float shoeZ = -spec.trackLength * 0.36f +
                                    amount * spec.trackLength * 0.72f;
                const Color shoeColor = ((shoe + phase) & 1) == 0
                                            ? shade(treadBase, 1.12f)
                                            : shade(treadBase, 0.70f);
                box({side * (spec.trackHalfWidth + 0.020f),
                     centerY - spec.trackHeight * 0.47f, shoeZ},
                    {0.340f, 0.075f, spec.trackLength * 0.078f},
                    shoeColor);
                box({side * (spec.trackHalfWidth + 0.020f),
                     centerY + spec.trackHeight * 0.47f, shoeZ},
                    {0.340f, 0.075f, spec.trackLength * 0.078f},
                    shoeColor);
            }

            // Five chunky links wrap each end.  This is what makes each side
            // read as one tall tread pod in the elevated gameplay camera rather
            // than as several unrelated wheel discs.
            constexpr int wrapLinks = 5;
            for (float end : {-1.0f, 1.0f})
            {
                for (int link = 0; link < wrapLinks; ++link)
                {
                    const float vertical = -1.0f +
                        2.0f * static_cast<float>(link) /
                        static_cast<float>(wrapLinks - 1);
                    const float bulge = std::sqrt(std::max(
                        0.0f, 1.0f - vertical * vertical));
                    const float linkY = centerY +
                                        vertical * spec.trackHeight * 0.43f;
                    const float linkZ = end *
                        (spec.trackLength * 0.355f +
                         bulge * spec.trackLength * 0.145f);
                    const Color linkColor = ((link + phase) & 1) == 0
                                                ? shade(treadBase, 1.08f)
                                                : shade(treadBase, 0.68f);
                    box({side * (spec.trackHalfWidth + 0.020f), linkY, linkZ},
                        {0.340f, 0.095f, spec.trackLength * 0.085f},
                        linkColor);
                }
            }
        }

        if (spec.skirts && !spec.wheeled)
        {
            for (int panel = 0; panel < 3; ++panel)
            {
                const float panelZ = -spec.trackLength * 0.27f +
                                     panel * spec.trackLength * 0.27f;
                box({side * (outerX + 0.052f), centerY + 0.090f, panelZ},
                    {0.038f, spec.trackHeight * 0.53f,
                     spec.trackLength * 0.245f},
                    panel == 1 ? p.paint : p.lightPaint);
            }
        }
    }
}

inline void drawArcadeHead(const ArcadeVehicleSpec &spec, const Palette &p,
                           bool enemy)
{
    const float headWidth = spec.headWidth * (enemy ? 0.96f : 1.04f);
    const float headLength = spec.headLength * (enemy ? 1.02f : 0.98f);
    const float headHeight = spec.headHeight * (enemy ? 0.82f : 0.92f);
    const Vector3 center{0.0f,
                         spec.headY - spec.headHeight * (enemy ? 0.055f : 0.025f),
                         spec.headZ};

    if (enemy)
    {
        // Enemy armor is a low riveted machine casing.  It intentionally
        // avoids the friendly vehicle's cheeks, dome and face-like muzzle.
        const float chamfer = spec.headShape == ArcadeHeadShape::Casemate
                                  ? 0.18f : 0.12f;
        chamferedFrustum(center,
                         headWidth + 0.060f, headLength + 0.050f,
                         headWidth * 0.80f, headLength * 0.77f,
                         headHeight + 0.045f, -0.006f,
                         chamfer, 0.10f, p.edge);
        chamferedFrustum({0.0f, center.y + 0.015f, center.z - 0.010f},
                         headWidth, headLength,
                         headWidth * 0.79f, headLength * 0.76f,
                         headHeight, -0.006f,
                         chamfer, 0.10f, p.darkPaint);
        box({0.0f, center.y - headHeight * 0.02f,
             center.z - headLength * 0.475f},
            {headWidth * 0.66f, headHeight * 0.53f, 0.045f}, p.paint);
        for (float side : {-1.0f, 1.0f})
        {
            DrawSphere({side * headWidth * 0.245f,
                        center.y + headHeight * 0.145f,
                        center.z - headLength * 0.505f},
                       0.030f, p.lightPaint);
            DrawSphere({side * headWidth * 0.245f,
                        center.y - headHeight * 0.145f,
                        center.z - headLength * 0.505f},
                       0.030f, p.lightPaint);
        }
        return;
    }

    if (spec.nationStyle == ArcadeNationStyle::Soviet &&
        spec.headShape == ArcadeHeadShape::Teardrop)
    {
        // The T-34/IS family is a low swept casting, not the same round dome
        // used by the American family.  Broad rear shoulders taper into a
        // narrow gun face and keep the familiar Soviet wedge in the chibi
        // proportions.
        chamferedFrustum(center,
                         headWidth + 0.055f, headLength + 0.055f,
                         headWidth * 0.72f, headLength * 0.72f,
                         headHeight * 0.92f, -0.070f,
                         0.29f, 0.18f, p.edge);
        chamferedFrustum({0.0f, center.y + 0.018f,
                          center.z - 0.025f},
                         headWidth, headLength,
                         headWidth * 0.69f, headLength * 0.68f,
                         headHeight * 0.84f, -0.068f,
                         0.30f, 0.18f, p.paint);
        arcadeEllipsoid({-headWidth * 0.13f,
                         center.y + headHeight * 0.22f,
                         center.z + headLength * 0.10f},
                        {headWidth * 0.27f, headHeight * 0.13f,
                         headLength * 0.23f},
                        p.lightPaint, 5, 12);
    }
    else if (spec.headShape == ArcadeHeadShape::Cast ||
             spec.headShape == ArcadeHeadShape::Teardrop)
    {
        arcadeEllipsoid(center,
                        {headWidth * 0.54f,
                         headHeight * 0.55f,
                         headLength * 0.54f},
                        p.edge, 9, 20);
        arcadeEllipsoid({0.0f, center.y + 0.018f,
                         spec.headZ - (spec.headShape == ArcadeHeadShape::Teardrop
                                           ? 0.035f : 0.0f)},
                        {headWidth * 0.50f,
                         headHeight * 0.50f,
                         headLength * 0.50f},
                        p.paint, 9, 20);
        arcadeEllipsoid({-headWidth * 0.10f,
                         center.y + headHeight * 0.22f,
                         spec.headZ - headLength * 0.10f},
                        {headWidth * 0.29f,
                         headHeight * 0.15f,
                         headLength * 0.25f},
                        p.lightPaint, 6, 14);
    }
    else if (spec.headShape == ArcadeHeadShape::Casemate)
    {
        chamferedFrustum(center,
                         headWidth + 0.070f, headLength + 0.060f,
                         headWidth * 0.83f + 0.050f,
                         headLength * 0.78f + 0.045f,
                         headHeight + 0.055f, -0.010f,
                         0.18f, 0.12f, p.edge);
        chamferedFrustum({0.0f, center.y + 0.018f,
                          spec.headZ - 0.012f},
                         headWidth, headLength,
                         headWidth * 0.82f, headLength * 0.77f,
                         headHeight, -0.010f,
                         0.18f, 0.12f, p.paint);
    }
    else
    {
        if (spec.nationStyle == ArcadeNationStyle::Soviet)
        {
            // Early Soviet and KV heads taper aggressively toward the gun,
            // with cast shoulder bulges rather than German vertical plates.
            chamferedFrustum(center,
                             headWidth + 0.045f, headLength + 0.060f,
                             headWidth * 0.68f, headLength * 0.66f,
                             headHeight + 0.040f, -0.055f,
                             0.23f, 0.16f, p.edge);
            chamferedFrustum({0.0f, center.y + 0.018f,
                              center.z - 0.028f},
                             headWidth, headLength,
                             headWidth * 0.66f, headLength * 0.64f,
                             headHeight, -0.052f,
                             0.23f, 0.16f, p.paint);
            for (float side : {-1.0f, 1.0f})
                arcadeEllipsoid({side * headWidth * 0.34f,
                                 center.y - headHeight * 0.05f,
                                 center.z + headLength * 0.08f},
                                {headWidth * 0.16f, headHeight * 0.30f,
                                 headLength * 0.28f},
                                side < 0.0f ? p.lightPaint : p.darkPaint,
                                6, 12);
        }
        else
        {
            // German turrets use crisp vertical faces, a long rear bustle and
            // squared side plates; no rounded friendly cheek silhouette.
            chamferedFrustum(center,
                             headWidth + 0.055f, headLength + 0.080f,
                             headWidth * 0.89f, headLength * 0.86f,
                             headHeight + 0.040f, 0.025f,
                             0.075f, 0.055f, p.edge);
            chamferedFrustum({0.0f, center.y + 0.015f,
                              center.z + 0.010f},
                             headWidth, headLength,
                             headWidth * 0.87f, headLength * 0.84f,
                             headHeight, 0.025f,
                             0.080f, 0.055f, p.paint);
            for (float side : {-1.0f, 1.0f})
                box({side * headWidth * 0.455f,
                     center.y - headHeight * 0.015f,
                     center.z + headLength * 0.12f},
                    {headWidth * 0.095f, headHeight * 0.58f,
                     headLength * 0.54f},
                    side < 0.0f ? p.lightPaint : p.darkPaint);
            box({0.0f, center.y, center.z + headLength * 0.47f},
                {headWidth * 0.72f, headHeight * 0.58f,
                 headLength * 0.18f}, p.darkPaint);
        }
    }

    box({-headWidth * 0.12f,
         center.y + headHeight * 0.40f,
         spec.headZ - headLength * 0.08f},
        {headWidth * 0.48f, 0.032f,
         headLength * 0.42f}, p.lightPaint);
}

inline void drawArcadeVehicle(Vehicle vehicle, const Palette &p,
                              Color identity, bool moving, bool enemy)
{
    const ArcadeVehicleSpec spec = arcadeVehicleSpec(vehicle);
    drawArcadeRunningGear(spec, p, moving, enemy);

    // The player uses a narrow, long central nose between two oversized track
    // pods.  Enemy armor instead stays on a broad, exposed mechanical deck.
    // Both paths fit the original gameplay footprint and collision radius.
    const float hullWidth = spec.hullWidth * (enemy ? 1.10f : 1.02f);
    const float hullLength = enemy
                                 ? std::max(spec.hullLength * 1.22f,
                                            spec.trackLength * 0.80f)
                                 : spec.trackLength * 0.94f;
    const float hullHeight = enemy
                                 ? std::max(0.225f, spec.hullHeight * 1.27f)
                                 : std::max(0.285f, spec.hullHeight * 1.55f);
    const float trackTop = 0.035f + spec.trackHeight;
    const float hullY = trackTop + (enemy ? 0.025f : 0.055f);
    if (enemy)
    {
        chamferedFrustum({0.0f, hullY, 0.005f},
                         hullWidth + 0.055f,
                         hullLength + 0.045f,
                         hullWidth * 0.88f,
                         hullLength * 0.86f,
                         hullHeight + 0.040f, -0.005f,
                         0.14f, 0.10f, p.edge);
        chamferedFrustum({0.0f, hullY + 0.020f, -0.015f},
                         hullWidth,
                         hullLength,
                         hullWidth * 0.87f,
                         hullLength * 0.85f,
                         hullHeight, -0.005f,
                         0.14f, 0.10f, p.darkPaint);

        // A separate front plate and exposed engine spine make the enemy
        // family read as industrial machinery even at the 14-tile zoom.
        box({0.0f, hullY + 0.012f, -hullLength * 0.515f},
            {hullWidth * 0.72f, hullHeight * 0.54f, 0.055f}, p.steel);
        box({0.0f, hullY + hullHeight * 0.56f,
             hullLength * 0.24f},
            {hullWidth * 0.58f, 0.055f, hullLength * 0.28f}, p.paint);
    }
    else
    {
        const float noseZ = -spec.trackLength * 0.105f;
        if (spec.nationStyle == ArcadeNationStyle::American)
        {
            arcadeEllipsoid({0.0f, hullY, noseZ},
                            {hullWidth * 0.55f,
                             hullHeight * 0.61f,
                             hullLength * 0.54f}, p.edge, 8, 18);
            arcadeEllipsoid({0.0f, hullY + 0.018f, noseZ - 0.020f},
                            {hullWidth * 0.50f,
                             hullHeight * 0.53f,
                             hullLength * 0.50f}, p.paint, 8, 18);
            slopedGlacis(hullWidth * 0.54f, hullWidth * 0.91f,
                         noseZ - hullLength * 0.50f,
                         noseZ - hullLength * 0.04f,
                         hullY - hullHeight * 0.04f,
                         hullY + hullHeight * 0.47f,
                         0.040f, p.lightPaint);
            arcadeEllipsoid({-hullWidth * 0.09f,
                             hullY + hullHeight * 0.56f,
                             noseZ - hullLength * 0.20f},
                            {hullWidth * 0.29f, hullHeight * 0.15f,
                             hullLength * 0.20f},
                            p.lightPaint, 5, 12);
        }
        else if (spec.nationStyle == ArcadeNationStyle::Soviet)
        {
            // A narrow pointed glacis and broad rear shoulders retain the
            // T-34/IS family wedge even under the big-head caricature.
            chamferedFrustum({0.0f, hullY, noseZ + 0.015f},
                             hullWidth * 1.02f, hullLength * 1.16f,
                             hullWidth * 0.76f, hullLength * 0.86f,
                             hullHeight * 1.14f, 0.075f,
                             0.18f, 0.11f, p.edge);
            chamferedFrustum({0.0f, hullY + 0.018f, noseZ - 0.010f},
                             hullWidth * 0.96f, hullLength * 1.10f,
                             hullWidth * 0.69f, hullLength * 0.79f,
                             hullHeight, 0.070f,
                             0.20f, 0.12f, p.paint);
            slopedGlacis(hullWidth * 0.26f, hullWidth * 0.83f,
                         noseZ - hullLength * 0.63f,
                         noseZ - hullLength * 0.04f,
                         hullY - hullHeight * 0.05f,
                         hullY + hullHeight * 0.52f,
                         0.055f, p.lightPaint);
        }
        else
        {
            // German hulls remain long, full-width and upright with a flat
            // transmission plate instead of another rounded center nose.
            chamferedFrustum({0.0f, hullY, noseZ + 0.025f},
                             hullWidth * 1.18f, hullLength * 1.19f,
                             hullWidth * 1.02f, hullLength * 0.99f,
                             hullHeight * 1.12f, 0.010f,
                             0.075f, 0.050f, p.edge);
            chamferedFrustum({0.0f, hullY + 0.018f, noseZ + 0.010f},
                             hullWidth * 1.10f, hullLength * 1.12f,
                             hullWidth * 0.98f, hullLength * 0.94f,
                             hullHeight, 0.010f,
                             0.080f, 0.055f, p.paint);
            box({0.0f, hullY + 0.010f,
                 noseZ - hullLength * 0.585f},
                {hullWidth * 0.86f, hullHeight * 0.58f, 0.060f},
                p.lightPaint);
        }
    }

    if (spec.headShape != ArcadeHeadShape::Casemate)
        drawTurretRing(spec.headY - spec.headHeight * 0.46f,
                       spec.headWidth * 0.35f, p);
    drawArcadeHead(spec, p, enemy);

    const float gunY = spec.headY - (enemy ? 0.040f : 0.012f);
    const float gunStart = arcadeGunStartZ(spec);
    const float arcadeGunRadius = std::max(
        {enemy ? 0.070f : 0.086f,
         spec.headHeight * (enemy ? 0.17f : 0.21f),
         spec.gunRadius * (enemy ? 1.85f : 2.25f)});
    if (!enemy && spec.headShape != ArcadeHeadShape::Casemate &&
        spec.nationStyle == ArcadeNationStyle::American)
    {
        // Rounded cheek plates are an American visual signature here; they
        // frame the huge mantlet without being copied onto every nation.
        for (float side : {-1.0f, 1.0f})
        {
            arcadeEllipsoid({side * spec.headWidth * 0.29f,
                             spec.headY - spec.headHeight * 0.08f,
                             gunStart + 0.020f},
                            {spec.headWidth * 0.14f,
                             spec.headHeight * 0.19f,
                             spec.headLength * 0.13f},
                            p.edge, 6, 12);
            arcadeEllipsoid({side * spec.headWidth * 0.29f,
                             spec.headY - spec.headHeight * 0.065f,
                             gunStart - 0.006f},
                            {spec.headWidth * 0.115f,
                             spec.headHeight * 0.155f,
                             spec.headLength * 0.105f},
                            side < 0.0f ? p.lightPaint : p.darkPaint,
                            6, 12);
        }
    }
    else if (!enemy && spec.headShape != ArcadeHeadShape::Casemate &&
             spec.nationStyle == ArcadeNationStyle::German)
    {
        // Flat bolted cheeks reinforce the German rolled-plate construction.
        for (float side : {-1.0f, 1.0f})
        {
            box({side * spec.headWidth * 0.30f,
                 spec.headY - spec.headHeight * 0.075f,
                 gunStart + 0.012f},
                {spec.headWidth * 0.16f, spec.headHeight * 0.30f,
                 spec.headLength * 0.12f},
                side < 0.0f ? p.lightPaint : p.darkPaint);
            DrawSphere({side * spec.headWidth * 0.30f,
                        spec.headY + spec.headHeight * 0.045f,
                        gunStart - spec.headLength * 0.055f},
                       0.022f, p.edge);
        }
    }
    const float mantletWidth = spec.headWidth *
        (enemy ? 0.23f
               : spec.nationStyle == ArcadeNationStyle::German ? 0.25f
               : spec.nationStyle == ArcadeNationStyle::Soviet ? 0.27f
                                                               : 0.30f);
    const float mantletRadius = spec.headHeight *
        (enemy ? 0.24f
               : spec.nationStyle == ArcadeNationStyle::German ? 0.28f
               : spec.nationStyle == ArcadeNationStyle::Soviet ? 0.30f
                                                               : 0.32f);
    drawGunMantlet(gunY, gunStart + 0.035f,
                   mantletWidth, mantletRadius, p);
    const float muzzleZ = gunStart - spec.gunLength - 0.050f;
    const Color muzzleLight = enemy
                                  ? material(Color{150, 158, 151, 255}, 9)
                                  : material(Color{193, 200, 181, 255}, 9);
    const Color muzzleShade = enemy
                                  ? material(Color{47, 52, 51, 255}, 10)
                                  : material(Color{101, 113, 108, 255}, 10);
    const Color muzzleRim = material(
        enemy ? Color{200, 207, 196, 255} : Color{224, 222, 191, 255}, 12);
    const float muzzleScale = enemy ? 1.24f : 1.52f;
    cylinder({0.0f, gunY, gunStart - 0.020f},
             {0.0f, gunY, muzzleZ + 0.155f},
             arcadeGunRadius, muzzleShade, 18);
    cylinder({0.0f, gunY, muzzleZ + 0.180f},
             {0.0f, gunY, muzzleZ - 0.008f},
             arcadeGunRadius * muzzleScale, muzzleLight, 20);
    cylinder({0.0f, gunY, muzzleZ + 0.182f},
             {0.0f, gunY, muzzleZ + 0.145f},
             arcadeGunRadius * (muzzleScale + 0.10f), muzzleShade, 20);
    cylinder({0.0f, gunY, muzzleZ - 0.008f},
             {0.0f, gunY, muzzleZ - 0.038f},
             arcadeGunRadius * (muzzleScale - 0.08f), muzzleRim, 20);
    cylinder({0.0f, gunY, muzzleZ - 0.031f},
             {0.0f, gunY, muzzleZ - 0.045f},
             arcadeGunRadius * 0.70f, Color{8, 9, 8, 10}, 18);
    if (spec.muzzleBrake)
    {
        if (!enemy && spec.nationStyle == ArcadeNationStyle::German)
        {
            // Wide twin baffles evoke the conspicuous German double-baffle
            // brake and remain readable from the fixed oblique camera.
            for (float offset : {0.155f, 0.080f})
                box({0.0f, gunY, muzzleZ + offset},
                    {arcadeGunRadius * 3.65f,
                     arcadeGunRadius * 2.70f, 0.040f}, p.edge);
        }
        else if (!enemy && spec.nationStyle == ArcadeNationStyle::Soviet)
        {
            cylinder({0.0f, gunY, muzzleZ + 0.190f},
                     {0.0f, gunY, muzzleZ + 0.085f},
                     arcadeGunRadius * 1.72f, p.darkPaint, 18);
            box({0.0f, gunY, muzzleZ + 0.115f},
                {arcadeGunRadius * 3.10f,
                 arcadeGunRadius * 2.35f, 0.034f}, p.edge);
        }
        else
        {
            cylinder({0.0f, gunY, muzzleZ + 0.175f},
                     {0.0f, gunY, muzzleZ + 0.120f},
                     arcadeGunRadius * 1.20f, p.darkPaint, 18);
        }
    }

    const float headTop = spec.headY + spec.headHeight *
                                             (enemy ? 0.37f : 0.45f);
    if (!enemy)
    {
        if (spec.headShape == ArcadeHeadShape::Casemate)
        {
            // The T28/T95 has no rotating turret: keep its roof furniture low
            // and sparse so the fixed fighting compartment stays obvious.
            drawCupola(spec.headWidth * 0.18f,
                       spec.headZ + spec.headLength * 0.20f,
                       headTop - 0.055f, 0.090f, p);
            drawAntenna(-spec.headWidth * 0.28f,
                        spec.headZ + spec.headLength * 0.31f,
                        headTop - 0.055f, 0.34f, p);
        }
        else if (spec.nationStyle == ArcadeNationStyle::American)
        {
            drawCupola(spec.headWidth * 0.18f, spec.headZ + 0.035f,
                       headTop - 0.020f, 0.108f, p);
            box({-spec.headWidth * 0.18f, headTop + 0.002f,
                 spec.headZ + 0.025f},
                {0.190f, 0.030f, 0.145f}, p.lightPaint);
            drawAntenna(-spec.headWidth * 0.31f,
                        spec.headZ + spec.headLength * 0.22f,
                        headTop - 0.025f, 0.43f, p);
            drawAntenna(-spec.headWidth * 0.18f,
                        spec.headZ + spec.headLength * 0.28f,
                        headTop - 0.020f, 0.34f, p);

            // A chunky side weapon and exhaust provide playful American
            // asymmetry without being repeated on the other countries.
            const float sideGunX = spec.headWidth * 0.43f;
            arcadeEllipsoid({sideGunX, spec.headY + 0.010f,
                             spec.headZ - spec.headLength * 0.08f},
                            {0.105f, 0.115f, 0.105f}, p.edge, 6, 12);
            for (float offset : {-0.030f, 0.030f})
                cylinder({sideGunX + offset, spec.headY + 0.025f,
                          spec.headZ - spec.headLength * 0.12f},
                         {sideGunX + offset, spec.headY + 0.025f,
                          spec.headZ - spec.headLength * 0.43f},
                         0.019f, p.steel, 8);
            cylinder({spec.headWidth * 0.40f, hullY + 0.02f,
                      spec.hullLength * 0.22f},
                     {spec.headWidth * 0.48f, hullY + 0.30f,
                      spec.hullLength * 0.31f},
                     0.052f, p.steel, 10);
            DrawSphere({spec.headWidth * 0.48f, hullY + 0.30f,
                        spec.hullLength * 0.31f}, 0.060f, p.edge);
        }
        else if (spec.nationStyle == ArcadeNationStyle::Soviet)
        {
            drawCupola(spec.headWidth * 0.06f, spec.headZ + 0.055f,
                       headTop - 0.040f, 0.092f, p);
            drawAntenna(-spec.headWidth * 0.30f,
                        spec.headZ + spec.headLength * 0.28f,
                        headTop - 0.035f, 0.40f, p);
            // One oversized searchlight and exposed rear fuel drums give the
            // Soviet family a simple, rugged silhouette.
            DrawSphere({spec.headWidth * 0.31f,
                        spec.headY + spec.headHeight * 0.11f,
                        gunStart - 0.010f}, 0.075f, p.edge);
            DrawSphere({spec.headWidth * 0.31f,
                        spec.headY + spec.headHeight * 0.115f,
                        gunStart - 0.060f}, 0.048f, p.optic);
            drawFuelDrums(spec.trackHalfWidth + 0.155f,
                          hullY + 0.13f,
                          spec.hullLength * 0.15f, p);
        }
        else
        {
            // A tall commander's cupola, rear stowage bustle and paired smoke
            // banks make German vehicles read as busy, angular machinery.
            drawCupola(spec.headWidth * 0.18f,
                       spec.headZ + spec.headLength * 0.08f,
                       headTop - 0.010f, 0.112f, p);
            DrawCylinder({spec.headWidth * 0.18f, headTop + 0.065f,
                          spec.headZ + spec.headLength * 0.08f},
                         0.090f, 0.082f, 0.072f, 14, p.edge);
            drawAntenna(-spec.headWidth * 0.32f,
                        spec.headZ + spec.headLength * 0.31f,
                        headTop - 0.020f, 0.43f, p);
            drawSmokeBanks(spec.headWidth * 0.40f,
                           spec.headY - spec.headHeight * 0.02f,
                           spec.headZ - spec.headLength * 0.04f, p);
            box({0.0f, spec.headY - spec.headHeight * 0.08f,
                 spec.headZ + spec.headLength * 0.50f},
                {spec.headWidth * 0.60f, spec.headHeight * 0.28f,
                 spec.headLength * 0.16f}, p.canvas);
            if (vehicle == Vehicle::Maus)
            {
                // Maus carries a visible coaxial 75 mm beside its main gun,
                // rather than the KV-5-style auxiliary turret.
                const float coaxX = spec.headWidth * 0.18f;
                cylinder({coaxX, gunY - 0.025f, gunStart - 0.010f},
                         {coaxX, gunY - 0.025f,
                          gunStart - spec.gunLength * 0.56f},
                         arcadeGunRadius * 0.34f, p.steel, 10);
                cylinder({coaxX, gunY - 0.025f,
                          gunStart - spec.gunLength * 0.56f},
                         {coaxX, gunY - 0.025f,
                          gunStart - spec.gunLength * 0.62f},
                         arcadeGunRadius * 0.48f, p.edge, 10);
            }
        }
    }
    else
    {
        // Enemy silhouettes receive a low hatch, exposed power cylinder and
        // rear brace rather than the player's antenna-and-side-gun face.
        box({-spec.headWidth * 0.16f, headTop + 0.020f,
             spec.headZ + spec.headLength * 0.10f},
            {0.240f, 0.075f, 0.185f}, p.steel);
        const float machineX = spec.headWidth * 0.43f;
        cylinder({machineX, hullY + 0.115f, spec.hullLength * 0.31f},
                 {machineX, hullY + 0.115f, -spec.hullLength * 0.02f},
                 0.075f, p.steel, 12);
        box({-spec.headWidth * 0.43f, hullY + 0.22f,
             spec.hullLength * 0.31f},
            {0.075f, 0.40f, 0.090f}, p.edge);
        box({-spec.headWidth * 0.33f, hullY + 0.38f,
             spec.hullLength * 0.31f},
            {0.26f, 0.075f, 0.090f}, p.steel);
        for (float bandZ : {0.05f, 0.20f})
            box({machineX, hullY + 0.115f,
                 spec.hullLength * bandZ},
                {0.165f, 0.165f, 0.035f}, p.lightPaint);
    }

    if (!enemy && spec.auxiliaryTurret)
    {
        arcadeEllipsoid({-spec.headWidth * 0.27f,
                         spec.headY - spec.headHeight * 0.10f,
                         spec.headZ - spec.headLength * 0.39f},
                        {0.165f, 0.135f, 0.150f}, p.edge, 6, 12);
        arcadeEllipsoid({-spec.headWidth * 0.27f,
                         spec.headY - spec.headHeight * 0.08f,
                         spec.headZ - spec.headLength * 0.40f},
                        {0.140f, 0.112f, 0.126f}, p.lightPaint, 6, 12);
        cylinder({-spec.headWidth * 0.27f,
                  spec.headY - spec.headHeight * 0.08f,
                  spec.headZ - spec.headLength * 0.49f},
                 {-spec.headWidth * 0.27f,
                  spec.headY - spec.headHeight * 0.08f,
                  spec.headZ - spec.headLength * 0.72f},
                 0.024f, p.steel, 10);
    }

    if (!enemy)
    {
        // Large identity lamp remains readable after the vehicle-only pixel
        // ramp and sits at the tip of the friendly central nose.
        box({0.0f, trackTop + 0.030f, -hullLength * 0.50f},
            {0.245f, 0.080f, 0.045f}, p.edge);
        box({0.0f, trackTop + 0.035f, -hullLength * 0.525f},
            {0.185f, 0.045f, 0.020f}, mix(identity, WHITE, 0.28f));
    }
    else
    {
        const Color weakPoint = material(Color{255, 91, 38, 255}, 12);
        for (float side : {-1.0f, 1.0f})
        {
            DrawSphere({side * hullWidth * 0.27f,
                        trackTop + 0.055f,
                        -hullLength * 0.535f},
                       0.057f, p.edge);
            DrawSphere({side * hullWidth * 0.27f,
                        trackTop + 0.058f,
                        -hullLength * 0.555f},
                       0.038f, weakPoint);
        }
    }
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
    const Color outer{88, 224, 255,
                      static_cast<unsigned char>(74 + 30 * (0.5f + 0.5f * std::sin(phase)))};
    const Vector3 center{0.0f, 0.55f, 0.0f};
    DrawSphereWires(center, 1.02f + pulse, 10, 16, outer);
    for (int ring = 0; ring < 3; ++ring)
        DrawCircle3D(center, 0.90f + ring * 0.075f + pulse,
                     {1.0f, 0.0f, 0.0f}, 90.0f,
                     Fade(Color{125, 236, 255, 255}, 0.28f));
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
    const Vehicle vehicle = enemy ? enemyVehicle(identity)
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

    // The visual body crawls like an elastic arcade machine: alternating
    // compression, fore-aft stretch, head lag, and track-driven body roll.
    // Simulation coordinates and collision remain completely unchanged.
    rlPushMatrix();
    float visualScaleX = 1.04f;
    float visualScaleY = 1.0f;
    float visualScaleZ = 1.02f;
    if (moving)
    {
        const float phase = static_cast<float>(GetTime()) * 8.8f +
                            identity * 0.71f + x * 0.31f + z * 0.17f +
                            (enemy ? 1.37f : 0.0f);
        const float stride = std::sin(phase);
        const float compression = std::sin(phase * 2.0f + 0.55f);
        const float bounce = 0.018f +
                             (0.5f + 0.5f * std::sin(phase + 0.35f)) * 0.045f;
        rlTranslatef(stride * 0.020f, bounce, 0.0f);
        rlRotatef(stride * 2.35f,
                  0.0f, 0.0f, 1.0f);
        rlRotatef(std::sin(phase + 0.95f) * 2.75f,
                  1.0f, 0.0f, 0.0f);
        visualScaleX *= 1.0f - compression * 0.025f;
        visualScaleY *= 1.0f + compression * 0.070f;
        visualScaleZ *= 1.0f - compression * 0.045f;
    }
    rlScalef(visualScaleX, visualScaleY, visualScaleZ);

    detail::drawArcadeVehicle(vehicle, colors, identityColor, moving, enemy);

    detail::drawRecognitionMarker(identityColor, enemy,
                                  (identity % 4 + 4) % 4);
    rlPopMatrix();
    detail::drawShield(shield, identity);
    rlPopMatrix();
}

} // namespace wwii_tank_model

#endif // TANKS3D_WWII_TANK_MODEL_H
