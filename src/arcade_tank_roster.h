#ifndef TANKS3D_ARCADE_TANK_ROSTER_H
#define TANKS3D_ARCADE_TANK_ROSTER_H

#include "chaffee_sample_model.h"

// Original, editable section profiles extend the Chaffee art study to the
// roster. This module only emits local-space geometry; vehicle selection,
// gameplay attachments, motion transforms and shadow submission stay outside.
namespace arcade_tank_roster
{
enum class Family { American, Soviet, German };

struct Design
{
    chaffee_sample_model::detail::Shape shape = chaffee_sample_model::detail::kShape;
    Family family = Family::American;
    float base = .55f;
    float corner = .17f;
    float roofWidth = .62f;
    float roofShift = .08f;
    float cabinX = 0.0f;
    float muzzleY = .77f;
    float muzzleZ = -.5624f;
    int wheels = 5;
    bool wheeled = false;
    bool casemate = false;
    bool skirts = false;
    bool auxiliary = false;
    bool coaxial = false;
    bool brake = false;
    bool chaffee = false;
};

namespace detail
{
namespace mesh = chaffee_sample_model::detail;
using mesh::Section;
using mesh::casting;
using mesh::quad;
using mesh::tagged;
using mesh::tone;
using mesh::triangle;

inline Color blend(Color a, Color b, float t)
{
    return {static_cast<unsigned char>(a.r + (b.r - a.r) * t),
            static_cast<unsigned char>(a.g + (b.g - a.g) * t),
            static_cast<unsigned char>(a.b + (b.b - a.b) * t), a.a};
}

// A connected recoil jacket, collar and hollow muzzle. The last ring is at
// the pre-existing attachment, irrespective of cabin proportions or family.
inline void cannon(float root, float tip, float y, float radius,
                   Color paint, bool brake, float x = 0.0f)
{
    const float length = root - tip;
    const std::array<float, 6> z{{root, root - length * .27f,
        root - length * .44f, tip + length * .29f,
        tip + length * .20f, tip}};
    const std::array<float, 6> r{{radius * 1.55f, radius * 1.65f,
        radius * 1.18f, radius, radius * (brake ? 1.35f : 1.17f),
        radius * (brake ? 1.30f : 1.12f)}};
    const Color steel = tagged({116, 130, 125, 255}, 15);
    const Color bore = tagged({22, 32, 33, 255}, 15);
    const auto p = [&](float depth, float rad, float a) {
        return Vector3{x + rad * std::cos(a), y + rad * std::sin(a), depth};
    };
    rlBegin(RL_TRIANGLES);
    for (std::size_t level = 0; level + 1 < z.size(); ++level)
        for (int i = 0; i < 12; ++i)
        {
            const float a = 2 * mesh::kPi * i / 12, b = 2 * mesh::kPi * (i + 1) / 12;
            quad(p(z[level], r[level], a), p(z[level + 1], r[level + 1], a),
                 p(z[level + 1], r[level + 1], b), p(z[level], r[level], b),
                 level < 2 ? paint : steel);
        }
    for (int i = 0; i < 12; ++i)
    {
        const float a = 2 * mesh::kPi * i / 12, b = 2 * mesh::kPi * (i + 1) / 12;
        const float hole = radius * .70f;
        quad(p(tip, r.back(), a), p(tip, hole, a),
             p(tip, hole, b), p(tip, r.back(), b), steel);
        quad(p(tip, hole, a), p(tip + .072f, hole, a),
             p(tip + .072f, hole, b), p(tip, hole, b), bore);
        triangle({x, y, tip + .074f}, p(tip + .074f, hole, b),
                 p(tip + .074f, hole, a), bore);
    }
    rlEnd();
}

inline void tire(float x, float z, float radius, float width, float side, bool moving)
{
    const Color rubber = tagged({39, 45, 43, 255}, 16);
    const Color steel = tagged({112, 122, 109, 255}, 15);
    const float phase = moving ? static_cast<float>(GetTime()) * 4.5f : 0.0f;
    const auto p = [&](float r, float angle, float depth) {
        return Vector3{x + side * depth, radius + std::cos(angle) * r,
                        z + std::sin(angle) * r};
    };
    rlBegin(RL_TRIANGLES);
    for (int i = 0; i < 16; ++i)
    {
        const float a = 2 * mesh::kPi * i / 16, b = 2 * mesh::kPi * (i + 1) / 16;
        const std::array<float, 4> depths{{-width * .5f, -width * .34f,
                                          width * .34f, width * .5f}};
        const std::array<float, 4> radii{{radius * .80f, radius, radius, radius * .80f}};
        for (std::size_t ring = 0; ring + 1 < depths.size(); ++ring)
        {
            const Vector3 v0 = p(radii[ring], a, depths[ring]);
            const Vector3 v1 = p(radii[ring], b, depths[ring]);
            const Vector3 v2 = p(radii[ring + 1], b, depths[ring + 1]);
            const Vector3 v3 = p(radii[ring + 1], a, depths[ring + 1]);
            if (side > 0) quad(v0, v1, v2, v3, rubber);
            else quad(v3, v2, v1, v0, rubber);
        }
        const Vector3 hub = p(0, 0, width * .51f);
        const Color spoke = i % 4 == 0 ? steel : tone(steel, .47f);
        const auto a0 = p(radius * .80f, a, width * .5f);
        const auto b0 = p(radius * .80f, b, width * .5f);
        const auto a1 = p(radius * .54f, a, width * .51f);
        const auto b1 = p(radius * .54f, b, width * .51f);
        if (side > 0)
        {
            quad(a0, b0, b1, a1, rubber);
            triangle(hub, p(radius * .54f, a + phase, width * .51f),
                     p(radius * .54f, b + phase, width * .51f), spoke);
        }
        else
        {
            quad(b0, a0, a1, b1, rubber);
            triangle(hub, p(radius * .54f, b + phase, width * .51f),
                     p(radius * .54f, a + phase, width * .51f), spoke);
        }
    }
    rlEnd();
}

inline std::array<Section, 6> cabinSections(const Design &d)
{
    const auto &s = d.shape;
    const float w = s.cabinWidth * .5f, h = s.cabinHeight;
    const float f = s.cabinZ - s.cabinLength * .5f;
    const float r = s.cabinZ + s.cabinLength * .5f;
    const float c = d.corner, shift = d.roofShift;
    return {{{d.base, w * .80f, f + .045f, r - .02f, c * .65f},
             {d.base + h * .17f, w * .94f, f - .03f, r + .015f, c * .94f},
             {d.base + h * .53f, w * 1.04f, f - .04f, r + .045f, c},
             {d.base + h * .80f, w * .91f, f + shift * .69f, r + .005f, c * .82f},
             {d.base + h * .96f, w * (d.roofWidth + .10f),
              f + .06f + shift, r - .065f, c * .68f},
             {d.base + h, w * d.roofWidth,
              f + .10f + shift, r - .115f, c * .53f}}};
}

inline void draw(const Design &d, Color armor, Color identity, bool moving, bool enemy)
{
    const auto &s = d.shape;
    Color national{109, 127, 104, 255};
    if (d.family == Family::Soviet) national = {98, 122, 88, 255};
    if (d.family == Family::German) national = {147, 133, 103, 255};
    // Preserve enemy armor/bonus carrier colors on the broad panels. Rubber,
    // exposed steel and optics are deliberately independent of faction paint.
    const Color paint = tagged(enemy ? blend(armor, national, .24f) : national);
    const Color deep = tagged(blend(tone(paint, .48f), {39, 62, 65, 255}, .38f));
    const Color light = tagged(blend(paint, {217, 210, 161, 255}, .37f));
    const Color ink = tagged({23, 35, 37, 255}, 15);
    const Color mark = tagged(identity);
    if (d.chaffee)
    {
        mesh::drawShape(s, identity, moving, paint, deep, light);
        return;
    }
    for (float side : {-1.0f, 1.0f})
    {
        if (d.wheeled)
        {
            const float radius = s.trackHeight * .43f;
            const float axle = s.trackLength * .5f - radius - .015f;
            for (float z : {-axle, 0.0f, axle})
                tire(side * s.trackCenter, z, radius, s.trackWidth, side, moving);
        }
        else
        {
            // T95 has two narrow belts in the same original track footprint.
            if (d.casemate)
            {
                auto half = s; half.trackWidth = s.trackWidth * .46f;
                for (float offset : {-.25f, .25f})
                {
                    half.trackCenter = s.trackCenter + offset * s.trackWidth;
                    mesh::belt(half, side, moving);
                }
            }
            else mesh::belt(s, side, moving);
            for (int i = 0; i < d.wheels; ++i)
                mesh::wheel(side * (s.trackCenter + s.trackWidth * .5f - .015f),
                            (static_cast<float>(i) / (d.wheels - 1) - .5f) *
                                (s.trackLength - s.trackHeight),
                            (s.trackHeight * .5f - .07f) *
                                (i == 0 || i == d.wheels - 1 ? 1.0f : .80f), side);
        }
    }
    const float inner = s.trackCenter - s.trackWidth * .5f - .01f;
    const float length = s.trackLength;
    casting(std::array<Section, 5>{{
        {.17f, inner * .86f, -length * .33f, length * .33f, .075f},
        {.27f, inner, -length * .42f, length * .41f, .09f},
        {.38f, inner, -length * .40f, length * .42f, .09f},
        {d.base - .02f, inner * .96f, -length * .31f, length * .38f, .075f},
        {d.base + .01f, inner * .94f, -length * .26f, length * .32f, .07f}}}, paint);

    const auto cabin = cabinSections(d);
    const float roof = cabin.back().y;
    rlPushMatrix();
    rlTranslatef(d.cabinX, 0, 0);
    const float w = s.cabinWidth * .5f;
    casting(std::array<Section, 3>{{
        {d.base - .055f, w * .72f, cabin[0].front + .025f, cabin[0].rear - .025f, .09f},
        {d.base, w * .86f, cabin[0].front, cabin[0].rear, .11f},
        {d.base + .035f, w * .84f, cabin[0].front, cabin[0].rear, .11f}}}, deep);
    casting(cabin, paint);
    // A brow and visor fitted to one sloping shoulder panel. The upper surface
    // is sampled from the actual section, so variants cannot bury the window.
    const auto front = [&](float t) {
        return Vector3{0, cabin[2].y + (cabin[3].y - cabin[2].y) * t,
            cabin[2].front + (cabin[3].front - cabin[2].front) * t - .004f};
    };
    const auto band = [&](float t0, float t1, float width, float relief, Color c) {
        const auto a = front(t0), b = front(t1);
        quad({-width, a.y, a.z - relief}, {-width, b.y, b.z - relief},
             {width, b.y, b.z - relief}, {width, a.y, a.z - relief}, c);
    };
    rlBegin(RL_TRIANGLES);
    band(.54f, .88f, w * .38f, 0.0f, ink);
    band(.64f, .76f, w * .33f, .004f, tagged({100, 166, 139, 255}, 17));
    band(.90f, 1.0f, w * .43f, .003f, light);
    const float stripeX = w * .32f;
    quad({stripeX, roof + .003f, cabin[5].front + .018f},
         {stripeX, roof + .003f, cabin[5].rear - .018f},
         {stripeX + .08f, roof + .003f, cabin[5].rear - .018f},
         {stripeX + .08f, roof + .003f, cabin[5].front + .018f}, mark);
    rlEnd();
    // A broad six-sided service cover follows the lower belly, full cheek and
    // receding shoulder. Its perimeter bevel and inner fan do not overlap.
    for (float side : {-1.0f, 1.0f})
    {
        const float centerZ = s.cabinZ + .02f;
        const auto point = [&](int ring, float z) {
            return Vector3{side * (cabin[ring].halfWidth + .006f),
                           cabin[ring].y, centerZ + z};
        };
        const std::array<Vector3, 6> rim{{point(1, -.085f), point(2, -.135f),
            point(3, -.08f), point(3, .08f), point(2, .135f), point(1, .085f)}};
        const Vector3 center{side * (w * 1.04f + .018f), d.base + s.cabinHeight * .49f, centerZ};
        const auto inset = [&](Vector3 p) {
            return Vector3{p.x + side * .003f,
                           center.y + (p.y - center.y) * .85f,
                           center.z + (p.z - center.z) * .84f};
        };
        rlBegin(RL_TRIANGLES);
        for (std::size_t i = 0; i < rim.size(); ++i)
        {
            const std::size_t j = (i + 1) % rim.size();
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
    rlPushMatrix();
    rlTranslatef(-.085f, roof, (cabin[5].front + cabin[5].rear) * .5f);
    const float hatchHeight = d.family == Family::German ? .10f : .065f;
    casting(std::array<Section, 3>{{
        {0, .115f, -.105f, .105f, .045f},
        {.024f, .12f, -.11f, .11f, .045f},
        {hatchHeight, .095f, -.085f, .085f, .04f}}}, light);
    rlPopMatrix();
    // The back face is deliberately plainer than the front: a wide dark grille
    // and three structural ribs read at game size without competing with gun.
    const auto a = cabin[2], b = cabin[3];
    rlBegin(RL_TRIANGLES);
    quad({-.16f, a.y, a.rear + .004f}, {.16f, a.y, a.rear + .004f},
         {.16f, b.y, b.rear + .004f}, {-.16f, b.y, b.rear + .004f}, ink);
    for (int rib = 0; rib < 3; ++rib)
    {
        const float x = -.12f + rib * .12f;
        quad({x - .014f, a.y, a.rear + .008f}, {x + .014f, a.y, a.rear + .008f},
             {x + .014f, b.y, b.rear + .008f}, {x - .014f, b.y, b.rear + .008f}, deep);
    }
    rlEnd();
    rlPopMatrix();

    cannon(cabin[1].front + .10f, d.muzzleZ, d.muzzleY,
           s.gunRadius, paint, d.brake);
    casting(std::array<Section, 3>{{
        {d.base - .085f, inner, length * .23f, length * .45f, .055f},
        {d.base + .035f, inner * .93f, length * .26f, length * .43f, .05f},
        {d.base + .06f, inner * .77f, length * .28f, length * .40f, .04f}}}, deep);
    for (float side : {-1.0f, 1.0f})
    {
        rlPushMatrix();
        rlTranslatef(side * s.trackCenter, 0, .025f);
        const float cover = d.skirts ? .39f : d.wheeled ? .22f : .28f;
        const float half = s.trackWidth * .51f;
        casting(std::array<Section, 4>{{
            {d.base - .27f, half * .96f, -cover + .045f, cover - .025f, .04f},
            {d.base - .11f, half, -cover, cover, .05f},
            {d.base - .01f, half * .92f, -cover + .025f, cover - .025f, .055f},
            {d.base + .015f, half * .60f, -cover + .09f, cover - .08f, .05f}}}, paint);
        rlPopMatrix();
        const float x = side * (s.trackCenter + half + .003f);
        rlBegin(RL_TRIANGLES);
        const auto panel = [&](float y0, float y1, float z0, float z1, Color color) {
            if (side > 0) quad({x,y0,z0}, {x,y1,z0}, {x,y1,z1}, {x,y0,z1}, color);
            else quad({x,y0,z1}, {x,y1,z1}, {x,y1,z0}, {x,y0,z0}, color);
        };
        panel(d.base - .235f, d.base - .215f, -cover + .10f, cover - .08f, ink);
        panel(d.base - .17f, d.base - .12f, -.04f, .14f, mark);
        rlEnd();
    }
    const float exhaustX = s.trackCenter * .77f;
    const float exhaustZ = s.cabinZ + s.cabinLength * .5f + .04f;
    DrawCylinderEx({exhaustX, d.base - .06f, exhaustZ},
                   {exhaustX, roof - .15f, exhaustZ}, .047f, .040f, 10, ink);
    DrawCylinderEx({exhaustX, d.base + .04f, exhaustZ},
                   {exhaustX, roof - .27f, exhaustZ}, .062f, .055f, 10, deep);
    DrawCylinderEx({exhaustX, roof - .18f, exhaustZ},
                   {exhaustX, roof - .12f, exhaustZ + .035f}, .043f, .043f, 10,
                   tagged({109, 116, 100, 255}, 15));
    if (d.auxiliary)
    {
        rlPushMatrix(); rlTranslatef(-s.trackCenter, d.base, -.29f);
        casting(std::array<Section, 3>{{
            {0, .115f, -.11f, .11f, .04f},
            {.15f, .13f, -.12f, .12f, .06f},
            {.22f, .085f, -.05f, .085f, .04f}}}, light);
        rlPopMatrix();
        cannon(-.32f, -.50f, d.base + .115f, .030f, deep, false, -s.trackCenter);
    }
    if (d.coaxial)
        cannon(cabin[1].front + .04f, cabin[1].front - .19f,
               d.muzzleY - .015f, .033f, deep, false, .23f);
}
} // namespace detail

inline void draw(const Design &design, Color armor, Color identity, bool moving, bool enemy)
{
    detail::draw(design, armor, identity, moving, enemy);
}
} // namespace arcade_tank_roster

#endif
