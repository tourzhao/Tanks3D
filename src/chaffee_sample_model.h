#ifndef TANKS3D_CHAFFEE_SAMPLE_MODEL_H
#define TANKS3D_CHAFFEE_SAMPLE_MODEL_H

#include <raylib.h>
#include <rlgl.h>

#include <algorithm>
#include <array>
#include <cmath>

// Original arcade casting for the USA level-zero player and Fast enemy. Local forward
// is -Z; the original neutral muzzle stays at (0, .77, -.5624). No gameplay,
// random source, camera, asset loading or shader state lives in this model.
namespace chaffee_sample_model
{
namespace detail
{
constexpr float kPi = 3.14159265358979323846f;

inline Color tagged(Color c, unsigned char tag = 14)
{
    c.a = tag;
    return c;
}

inline Color tone(Color c, float value)
{
    c.r = static_cast<unsigned char>(std::clamp(c.r * value, 0.0f, 255.0f));
    c.g = static_cast<unsigned char>(std::clamp(c.g * value, 0.0f, 255.0f));
    c.b = static_cast<unsigned char>(std::clamp(c.b * value, 0.0f, 255.0f));
    return c;
}

inline Vector3 minus(Vector3 a, Vector3 b)
{
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

inline Vector3 normal(Vector3 a, Vector3 b, Vector3 c)
{
    const Vector3 u = minus(b, a), v = minus(c, a);
    const Vector3 n{u.y * v.z - u.z * v.y,
                    u.z * v.x - u.x * v.z, u.x * v.y - u.y * v.x};
    const float length = std::sqrt(n.x * n.x + n.y * n.y + n.z * n.z);
    return length > 0.000001f ? Vector3{n.x / length, n.y / length, n.z / length}
                             : Vector3{0.0f, 1.0f, 0.0f};
}

inline void vertex(Vector3 p, Vector3 n, Color c)
{
    rlColor4ub(c.r, c.g, c.b, c.a);
    rlNormal3f(n.x, n.y, n.z);
    rlVertex3f(p.x, p.y, p.z);
}

inline void triangle(Vector3 a, Vector3 b, Vector3 c, Color color)
{
    const Vector3 n = normal(a, b, c);
    vertex(a, n, color); vertex(b, n, color); vertex(c, n, color);
}

inline void quad(Vector3 a, Vector3 b, Vector3 c, Vector3 d, Color color)
{
    // One normal per authored panel avoids a diagonal lighting seam. Rounded
    // shoulders are successive narrow panels, broad cheeks remain planar.
    const Vector3 n = normal(a, b, d);
    vertex(a, n, color); vertex(b, n, color); vertex(c, n, color);
    vertex(a, n, color); vertex(c, n, color); vertex(d, n, color);
}

struct Section
{
    float y, halfWidth, front, rear, corner;
};

inline std::array<Vector3, 12> section(const Section &s)
{
    const float x = s.halfWidth, c = s.corner;
    return {{{-x + c, s.y, s.front}, {x - c, s.y, s.front},
             {x - c * .28f, s.y, s.front + c * .28f},
             {x, s.y, s.front + c}, {x, s.y, s.rear - c},
             {x - c * .28f, s.y, s.rear - c * .28f},
             {x - c, s.y, s.rear}, {-x + c, s.y, s.rear},
             {-x + c * .28f, s.y, s.rear - c * .28f},
             {-x, s.y, s.rear - c}, {-x, s.y, s.front + c},
             {-x + c * .28f, s.y, s.front + c * .28f}}};
}

template<std::size_t N>
inline void casting(const std::array<Section, N> &s, Color paint)
{
    rlBegin(RL_TRIANGLES);
    for (std::size_t level = 0; level + 1 < N; ++level)
    {
        const auto lower = section(s[level]), upper = section(s[level + 1]);
        for (std::size_t i = 0; i < lower.size(); ++i)
        {
            const std::size_t j = (i + 1) % lower.size();
            quad(lower[j], lower[i], upper[i], upper[j], paint);
        }
    }
    const auto bottom = section(s.front()), top = section(s.back());
    for (std::size_t i = 1; i + 1 < top.size(); ++i)
    {
        triangle(top[0], top[i + 1], top[i], paint);
        triangle(bottom[0], bottom[i], bottom[i + 1], tone(paint, .65f));
    }
    rlEnd();
}

struct Shape
{
    float trackLength, trackHeight, trackCenter, trackWidth;
    float cabinWidth, cabinLength, cabinHeight, cabinZ;
    float gunRadius;
};

constexpr Shape kShape{1.42f, .48f, .375f, .29f, .80f, .69f, .56f, .085f, .095f};

// A racetrack contour parameterized by true perimeter distance: straight lower
// runs meet semicircular ends tangentially. Shoe spacing is uniform on bends.
inline Vector3 beltPoint(float travel, const Shape &s, float inset = 0.0f)
{
    const float radius = s.trackHeight * .5f;
    const float straight = s.trackLength - s.trackHeight;
    const float arc = kPi * radius;
    const float perimeter = 2.0f * (straight + arc);
    float p = std::fmod(travel, perimeter);
    if (p < 0.0f) p += perimeter;
    // The .006 shoe relief meets Y=0; wheels sit inside the hollow belt.
    const float r = radius - inset, y = radius + .006f;
    if (p < straight) return {0, y + r, -straight * .5f + p};
    p -= straight;
    if (p < arc)
    {
        const float a = p / radius;
        return {0, y + r * std::cos(a), straight * .5f + r * std::sin(a)};
    }
    p -= arc;
    if (p < straight) return {0, y - r, straight * .5f - p};
    p -= straight;
    const float a = p / radius;
    return {0, y - r * std::cos(a), -straight * .5f - r * std::sin(a)};
}

inline void belt(const Shape &s, float side, bool moving)
{
    const Color rubber = tagged({44, 49, 48, 255}, 16);
    const Color shoe = tagged({78, 79, 75, 255}, 15);
    const float outside = side * (s.trackCenter + s.trackWidth * .5f);
    const float inside = side * (s.trackCenter - s.trackWidth * .5f);
    const float perimeter = 2 * (s.trackLength - s.trackHeight + kPi * s.trackHeight * .5f);
    const float phase = moving ? static_cast<float>(GetTime()) * .63f : 0.0f;
    const auto point = [](Vector3 p, float x) { p.x = x; return p; };
    constexpr int kSegments = 48;
    rlBegin(RL_TRIANGLES);
    for (int i = 0; i < kSegments; ++i)
    {
        const float p = perimeter * i / kSegments, q = perimeter * (i + 1) / kSegments;
        const auto a = beltPoint(p, s), b = beltPoint(q, s);
        const auto c = beltPoint(p, s, .07f), d = beltPoint(q, s, .07f);
        // Each side of the hollow belt is explicitly wound outward.
        if (side > 0)
            quad(point(a, outside), point(b, outside), point(d, outside), point(c, outside), shoe);
        else
            quad(point(b, outside), point(a, outside), point(c, outside), point(d, outside), shoe);
        quad(point(a, inside), point(b, inside), point(b, outside), point(a, outside), rubber);
    }
    // Fewer, broad shoes. Their edges describe the bends; no rivet noise.
    constexpr int kShoes = 24;
    for (int i = 0; i < kShoes; ++i)
    {
        const float p = perimeter * i / kShoes + phase;
        const float q = p + perimeter / kShoes * .80f;
        const auto a = beltPoint(p, s, -.006f), b = beltPoint(q, s, -.006f);
        if (side > 0)
            quad(point(a, inside), point(b, inside), point(b, outside), point(a, outside), shoe);
        else
            quad(point(a, outside), point(b, outside), point(b, inside), point(a, inside), shoe);
    }
    rlEnd();
}

inline void wheel(float x, float z, float radius, float side)
{
    const Color steel = tagged({110, 118, 102, 255}, 15);
    const Color shadow = tagged({45, 59, 56, 255}, 15);
    const float y = radius + .085f;
    const auto p = [&](float r, float angle, float depth) {
        return Vector3{x + side * depth, y + std::cos(angle) * r,
                        z + std::sin(angle) * r};
    };
    rlBegin(RL_TRIANGLES);
    for (int i = 0; i < 12; ++i)
    {
        const float a = 2 * kPi * i / 12, b = 2 * kPi * (i + 1) / 12;
        if (side > 0)
        {
            quad(p(radius, a, 0), p(radius, b, 0), p(radius * .77f, b, .018f), p(radius * .77f, a, .018f), steel);
            triangle(p(0, 0, .018f), p(radius * .77f, a, .018f), p(radius * .77f, b, .018f), shadow);
        }
        else
        {
            quad(p(radius, b, 0), p(radius, a, 0), p(radius * .77f, a, .018f), p(radius * .77f, b, .018f), steel);
            triangle(p(0, 0, .018f), p(radius * .77f, b, .018f), p(radius * .77f, a, .018f), shadow);
        }
    }
    rlEnd();
    DrawCylinderEx({x, y, z}, {x + side * .03f, y, z}, radius * .30f, radius * .30f, 8, steel);
}

inline void gun(float radius, Color paint)
{
    constexpr float y = .77f, tip = -.5624f;
    // The gun axis and end are the existing gameplay presentation contract.
    // Contoured rings connect the casting to a short, hollow steel muzzle.
    const std::array<float, 6> z{{-.18f, -.285f, -.335f, -.43f, -.47f, tip}};
    const std::array<float, 6> r{{radius * 1.60f, radius * 1.72f,
                                radius * 1.24f, radius, radius * 1.17f, radius * 1.12f}};
    const auto p = [&](float depth, float rad, float a) {
        return Vector3{rad * std::cos(a), y + rad * std::sin(a), depth};
    };
    const Color steel = tagged({116, 130, 125, 255}, 15);
    const Color bore = tagged({22, 32, 33, 255}, 15);
    rlBegin(RL_TRIANGLES);
    for (std::size_t level = 0; level + 1 < z.size(); ++level)
        for (int i = 0; i < 12; ++i)
        {
            const float a = 2 * kPi * i / 12, b = 2 * kPi * (i + 1) / 12;
            quad(p(z[level], r[level], a), p(z[level + 1], r[level + 1], a),
                 p(z[level + 1], r[level + 1], b), p(z[level], r[level], b),
                 level < 2 ? paint : steel);
        }
    for (int i = 0; i < 12; ++i)
    {
        const float a = 2 * kPi * i / 12, b = 2 * kPi * (i + 1) / 12;
        const float hole = radius * .72f;
        quad(p(tip, r.back(), a), p(tip, hole, a), p(tip, hole, b), p(tip, r.back(), b), steel);
        quad(p(tip, hole, a), p(tip + .075f, hole, a), p(tip + .075f, hole, b), p(tip, hole, b), bore);
        triangle({0, y, tip + .078f}, p(tip + .078f, hole, b), p(tip + .078f, hole, a), bore);
    }
    rlEnd();
}

inline void drawShape(const Shape &s, Color identity, bool moving,
                      Color armor = {109, 127, 104, 255},
                      Color recess = {42, 63, 61, 255},
                      Color highlight = {153, 166, 126, 255})
{
    const Color paint = tagged(armor);
    const Color deep = tagged(recess);
    const Color light = tagged(highlight);
    const Color ink = tagged({23, 35, 37, 255}, 15);
    const Color mark = tagged(identity);
    for (float side : {-1.0f, 1.0f})
    {
        belt(s, side, moving);
        for (int i = 0; i < 5; ++i)
            wheel(side * (s.trackCenter + s.trackWidth * .5f - .015f),
                  (i - 2) * (s.trackLength - s.trackHeight) * .24f,
                  i == 0 || i == 4 ? .16f : .128f, side);
    }
    casting(std::array<Section, 5>{{
        {.17f, .25f, -.47f, .49f, .10f},
        {.27f, .28f, -.60f, .58f, .10f},
        {.39f, .29f, -.57f, .60f, .10f},
        {.52f, .26f, -.43f, .55f, .09f},
        {.56f, .25f, -.36f, .46f, .09f}}}, paint);
    const float w = s.cabinWidth * .5f, f = s.cabinZ - s.cabinLength * .5f;
    const float r = s.cabinZ + s.cabinLength * .5f, h = s.cabinHeight;
    casting(std::array<Section, 3>{{
        {.49f, w * .73f, f + .06f, r - .05f, .09f},
        {.56f, w * .86f, f + .02f, r, .11f},
        {.59f, w * .84f, f + .02f, r, .11f}}}, deep);
    casting(std::array<Section, 6>{{
        {.55f, w * .80f, f + .045f, r - .02f, .11f},
        {.55f + h * .17f, w * .94f, f - .03f, r + .015f, .16f},
        {.55f + h * .53f, w * 1.04f, f - .04f, r + .045f, .17f},
        {.55f + h * .80f, w * .91f, f + .055f, r + .005f, .14f},
        {.55f + h * .96f, w * .72f, f + .14f, r - .065f, .115f},
        {.55f + h, w * .62f, f + .18f, r - .115f, .09f}}}, paint);
    gun(s.gunRadius, paint);
    // One offset hatch and an exposed engine deck establish hierarchy before
    // any decorative work. These are connected chamfered sections, not cubes.
    rlPushMatrix();
    rlTranslatef(-.085f, .55f + h, s.cabinZ + .06f);
    casting(std::array<Section, 3>{{
        {0, .115f, -.105f, .105f, .045f},
        {.022f, .12f, -.11f, .11f, .045f},
        {.065f, .10f, -.085f, .085f, .04f}}}, light);
    rlPopMatrix();
    casting(std::array<Section, 3>{{
        {.46f, .26f, .33f, .64f, .06f},
        {.59f, .24f, .36f, .61f, .06f},
        {.62f, .19f, .39f, .57f, .045f}}}, deep);

    // A wide recessed visor and brow follow the forward-sloping shoulder.
    // They provide a face without suggesting a second working gun.
    rlBegin(RL_TRIANGLES);
    quad({-.15f, .926f, -.259f}, {-.15f, .967f, -.233f},
         {.15f, .967f, -.233f}, {.15f, .926f, -.259f}, ink);
    quad({-.135f, .941f, -.252f}, {-.135f, .953f, -.244f},
         {.135f, .953f, -.244f}, {.135f, .941f, -.252f},
         tagged({100, 166, 139, 255}, 17));
    quad({-.17f, .974f, -.243f}, {-.16f, .991f, -.213f},
         {.16f, .991f, -.213f}, {.17f, .974f, -.243f}, light);
    // P1/P2 paint only touches armor. Roof stripe remains readable overhead.
    quad({.105f, .55f + h + .002f, -.005f}, {.105f, .55f + h + .002f, .24f},
         {.185f, .55f + h + .002f, .24f}, {.185f, .55f + h + .002f, -.005f}, mark);
    rlEnd();
    for (float side : {-1.0f, 1.0f})
    {
        // The short saddle leaves both complete tread turns exposed. A deep
        // lower seam separates armored shoulder, rubber and wheel metal.
        rlPushMatrix();
        rlTranslatef(side * s.trackCenter, 0, .025f);
        casting(std::array<Section, 4>{{
            {.27f, .149f, -.215f, .255f, .04f},
            {.43f, .15f, -.26f, .29f, .05f},
            {.53f, .14f, -.22f, .27f, .06f},
            {.555f, .09f, -.15f, .20f, .05f}}}, paint);
        rlPopMatrix();
        const float x = side * (s.trackCenter + .152f);
        rlBegin(RL_TRIANGLES);
        const auto sidePanel = [&](float y0, float y1, float z0, float z1, Color color) {
            if (side > 0) quad({x, y0, z0}, {x, y1, z0}, {x, y1, z1}, {x, y0, z1}, color);
            else quad({x, y0, z1}, {x, y1, z1}, {x, y1, z0}, {x, y0, z0}, color);
        };
        sidePanel(.292f, .308f, -.16f, .22f, ink);
        sidePanel(.375f, .435f, -.025f, .13f, mark);
        // A single inset service door follows the belly/shoulder curvature.
        // The bevel is modeled, including the seam; it is not a floating decal.
        const std::array<Vector3, 6> rim{{
            {side * .394f, .70f, -.045f}, {side * .427f, .845f, -.07f},
            {side * .404f, .93f, -.005f}, {side * .404f, .93f, .24f},
            {side * .427f, .845f, .285f}, {side * .394f, .70f, .24f}}};
        const Vector3 center{side * .435f, .817f, .11f};
        for (std::size_t i = 0; i < rim.size(); ++i)
        {
            const std::size_t j = (i + 1) % rim.size();
            const auto inset = [&](Vector3 p) {
                return Vector3{p.x + side * .003f,
                               center.y + (p.y - center.y) * .85f,
                               center.z + (p.z - center.z) * .86f};
            };
            if (side > 0)
            {
                quad(rim[i], rim[j], inset(rim[j]), inset(rim[i]), deep);
                triangle(center, inset(rim[i]), inset(rim[j]), tone(paint, .93f));
            }
            else
            {
                quad(rim[j], rim[i], inset(rim[i]), inset(rim[j]), deep);
                triangle(center, inset(rim[j]), inset(rim[i]), tone(paint, .93f));
            }
        }
        rlEnd();
    }
    rlBegin(RL_TRIANGLES);
    quad({-.17f, .80f, .479f}, {.17f, .80f, .479f},
         {.17f, .93f, .466f}, {-.17f, .93f, .466f}, ink);
    for (int rib = 0; rib < 3; ++rib)
    {
        const float x = -.12f + rib * .12f;
        quad({x - .016f, .805f, .484f}, {x + .016f, .805f, .484f},
             {x + .016f, .925f, .471f}, {x - .016f, .925f, .471f}, deep);
    }
    rlEnd();
    // Rear machinery is deliberately asymmetric and below the hatch high point.
    DrawCylinderEx({.29f, .49f, .49f}, {.29f, .93f, .49f}, .049f, .040f, 10, ink);
    DrawCylinderEx({.29f, .59f, .49f}, {.29f, .81f, .49f}, .065f, .059f, 10, deep);
    DrawCylinderEx({.29f, .91f, .49f}, {.29f, .97f, .535f}, .044f, .044f, 10, tagged({109, 116, 100, 255}, 15));
    for (int vent = 0; vent < 3; ++vent)
        DrawCubeV({-.02f, .623f, .435f + vent * .046f}, {.25f, .012f, .02f}, ink);
}
} // namespace detail

inline void draw(Color identity, bool moving)
{
    detail::drawShape(detail::kShape, identity, moving);
}
} // namespace chaffee_sample_model

#endif
