#ifndef TANKS3D_ARCADE_TANK_ROSTER_H
#define TANKS3D_ARCADE_TANK_ROSTER_H

#include "chaffee_sample_model.h"

// Original, editable section profiles extend the Chaffee art study to the
// roster. This module only emits local-space geometry; vehicle selection,
// gameplay attachments, motion transforms and shadow submission stay outside.
namespace arcade_tank_roster
{
enum class Family { American, Soviet, German };
enum class CartoonModel
{
    None, Sherman, Pershing, Patton, Abrams, T34, Stalin, T62, T90,
    Panther, TigerII, Leopard1, Leopard2
};

struct Design
{
    chaffee_sample_model::detail::Shape shape = chaffee_sample_model::detail::kShape;
    Family family = Family::American;
    float base = .55f;
    float hullTop = .50f;
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
    CartoonModel cartoon = CartoonModel::None;
    int tier = 0;
    // Separate authored chassis axes: fixture placement follows hull proportions,
    // while wheel radii and armor heights retain their original tier scale.
    float chassisScale = 1.0f;
    float chassisWidth = 1.0f;
    float chassisLength = 1.0f;
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

struct ArmorPlan
{
    float frontCutX = -1, frontCutZ = 0, rearCutX = 0, rearCutZ = 0;
    float frontCurve = .5f, rearCurve = .5f, rearWidth = 1;
};

inline ArmorPlan armorPlan(CartoonModel model, bool turret)
{
    using M = CartoonModel;
    if (turret)
    {
        switch (model)
        {
        case M::Sherman: return {0.68f, 0.24f, 0.49f, 0.19f, 0.28f, 0.28f, 0.87f};
        case M::Pershing: return {0.72f, 0.24f, 0.52f, 0.19f, 0.28f, 0.28f, 0.96f};
        case M::Patton: return {0.88f, 0.39f, 0.48f, 0.14f, 0.28f, 0.28f, 0.88f};
        case M::Abrams: return {0.55f, 0.3f, 0.05f, 0.035f, 0.5f, 0.5f, 0.9f};
        case M::T34: return {0.3f, 0.13f, 0.2f, 0.08f, 0.5f, 0.5f, 0.84f};
        case M::Stalin: return {0.78f, 0.25f, 0.62f, 0.22f, 0.28f, 0.28f, 0.88f};
        case M::T62: return {0.9f, 0.36f, 0.74f, 0.32f, 0.28f, 0.28f, 0.9f};
        case M::T90: return {0.57f, 0.29f, 0.26f, 0.11f, 0.5f, 0.5f, 0.8f};
        case M::Panther: return {0.18f, 0.08f, 0.15f, 0.055f, 0.5f, 0.5f, 0.91f};
        case M::TigerII: return {0.12f, 0.045f, 0.1f, 0.055f, 0.5f, 0.5f, 0.85f};
        case M::Leopard1: return {0.81f, 0.31f, 0.58f, 0.21f, 0.28f, 0.28f, 0.9f};
        case M::Leopard2: return {0.08f, 0.035f, 0.08f, 0.035f, 0.5f, 0.5f, 1.0f};
        case M::None: return {};
        }
    }
    switch (model)
    {
    case M::Sherman: return {0.1f, 0.027f, 0.055f, 0.018f, 0.5f, 0.5f, 1.0f};
    case M::Pershing: return {0.35f, 0.092f, 0.13f, 0.038f, 0.28f, 0.28f, 0.97f};
    case M::Patton: return {0.1f, 0.025f, 0.13f, 0.038f, 0.5f, 0.5f, 0.96f};
    case M::Abrams: return {0.1f, 0.05f, 0.06f, 0.015f, 0.5f, 0.5f, 1.0f};
    case M::T34: return {0.04f, 0.015f, 0.06f, 0.022f, 0.5f, 0.5f, 1.0f};
    case M::Stalin: return {0.4f, 0.08f, 0.14f, 0.035f, 0.28f, 0.28f, 0.96f};
    case M::T62: return {0.045f, 0.016f, 0.08f, 0.024f, 0.5f, 0.5f, 1.0f};
    case M::T90: return {0.08f, 0.025f, 0.06f, 0.02f, 0.5f, 0.5f, 1.0f};
    case M::Panther: return {0.045f, 0.012f, 0.04f, 0.015f, 0.5f, 0.5f, 1.0f};
    case M::TigerII: return {0.035f, 0.012f, 0.04f, 0.015f, 0.5f, 0.5f, 1.0f};
    case M::Leopard1: return {0.095f, 0.025f, 0.08f, 0.02f, 0.5f, 0.5f, 1.0f};
    case M::Leopard2: return {0.065f, 0.025f, 0.05f, 0.015f, 0.5f, 0.5f, 1.0f};
    case M::None: return {};
    }
    return {};
}

inline bool hardArmor(const ArmorPlan &plan)
{
    return plan.frontCurve == .5f && plan.rearCurve == .5f;
}

inline std::array<Vector3, 12> armorSection(const Section &s, const ArmorPlan &plan,
                                           const Section *reference = nullptr)
{
    if (plan.frontCutX < 0) return mesh::section(s);
    // Independent fore/aft shoulders replace the common rounded rectangle.
    // Fractions are authored from the variant cards, not measured plate angles.
    const float x = s.halfWidth, y = s.y, f = s.front, r = s.rear;
    const float fx = x * plan.frontCutX, fz = (r - f) * plan.frontCutZ;
    const float rx = x * plan.rearCutX, rz = (r - f) * plan.rearCutZ;
    const float back = x * plan.rearWidth, qf = plan.frontCurve, qr = plan.rearCurve;
    std::array<Vector3,12> result{{{-x+fx,y,f}, {x-fx,y,f}, {x-qf*fx,y,f+qf*fz},
             {x,y,f+fz}, {back,y,r-rz}, {back-qr*rx,y,r-qr*rz},
             {back-rx,y,r}, {-back+rx,y,r}, {-back+qr*rx,y,r-qr*rz},
             {-back,y,r-rz}, {-x,y,f+fz}, {-x+qf*fx,y,f+qf*fz}}};
    if (reference && hardArmor(plan))
    {
        // Keep corresponding ring edges parallel. Their planes then meet as
        // real welded plates instead of twisted quads with a false diagonal.
        constexpr std::array<std::size_t,8> corners{{0,1,3,4,6,7,9,10}};
        constexpr std::array<std::size_t,8> anchors{{0,2,2,3,4,6,7,7}};
        const auto basis=armorSection(*reference,plan);
        std::array<std::array<double,2>,8> normals{};
        std::array<double,8> offsets{};
        for (std::size_t i=0;i<8;++i)
        {
            const auto a=basis[corners[i]], b=basis[corners[(i+1)%8]];
            const double dx=b.x-a.x, dz=b.z-a.z, length=std::hypot(dx,dz);
            normals[i]={{dz/length,-dx/length}};
            const auto anchor=result[corners[anchors[i]]];
            offsets[i]=normals[i][0]*anchor.x+normals[i][1]*anchor.z;
        }
        for (std::size_t i=0;i<8;++i)
        {
            const auto &a=normals[(i+7)%8], &b=normals[i];
            const double da=offsets[(i+7)%8], db=offsets[i], determinant=a[0]*b[1]-a[1]*b[0];
            result[corners[i]]={static_cast<float>((da*b[1]-a[1]*db)/determinant),y,
                                static_cast<float>((a[0]*db-da*b[0])/determinant)};
        }
        for (std::size_t i : {std::size_t{2},std::size_t{5},std::size_t{8},std::size_t{11}})
        {
            const auto a=result[i-1], b=result[(i+1)%12];
            result[i]={(a.x+b.x)*.5f,y,(a.z+b.z)*.5f};
        }
    }
    return result;
}

inline std::array<Vector3,20> castTurretSection(const Section &section, const ArmorPlan &plan)
{
    const auto ring=armorSection(section,plan);
    std::array<Vector3,20> result{};
    std::size_t count=0;
    result[count++]=ring[0];
    // Retain every authored station, adding a quarter point on each side of
    // each shoulder midpoint. The rational arc follows the original curve
    // weight without turning the six distinct cast turrets into one ellipse.
    for (std::size_t start : {std::size_t{1},std::size_t{4},std::size_t{7},std::size_t{10}})
    {
        const auto a=ring[start], b=ring[(start+1)%12], c=ring[(start+2)%12];
        const float q=(start==1 || start==10) ? plan.frontCurve : plan.rearCurve;
        const float weight=.5f/q-1.0f;
        const Vector3 control{((2+2*weight)*b.x-a.x-c.x)/(2*weight),section.y,
                              ((2+2*weight)*b.z-a.z-c.z)/(2*weight)};
        const auto sample=[&](float t) {
            const float u=1-t, denominator=u*u+2*weight*t*u+t*t;
            return Vector3{(u*u*a.x+2*weight*t*u*control.x+t*t*c.x)/denominator,
                           section.y,
                           (u*u*a.z+2*weight*t*u*control.z+t*t*c.z)/denominator};
        };
        result[count++]=a;
        result[count++]=sample(.25f);
        result[count++]=b;
        result[count++]=sample(.75f);
        if (start!=10) result[count++]=c;
    }
    return result;
}

inline float armorHalfWidth(const Section &section, const ArmorPlan &plan, float z,
                            const Section *reference = nullptr)
{
    const auto ring = armorSection(section, plan, reference);
    float width = 0;
    for (std::size_t i = 0; i < ring.size(); ++i)
    {
        const auto a = ring[i], b = ring[(i + 1) % ring.size()];
        if (z < std::min(a.z,b.z)-.00001f || z > std::max(a.z,b.z)+.00001f) continue;
        const float span = b.z - a.z;
        const float x = std::abs(span) < .00001f ? std::max(a.x,b.x) :
            a.x + (b.x-a.x) * (z-a.z) / span;
        width = std::max(width, x);
    }
    return width;
}

template<std::size_t N>
inline Section armorAtHeight(const std::array<Section,N> &sections, float height)
{
    for (std::size_t i=0;i+1<N;++i)
    {
        const auto &a=sections[i], &b=sections[i+1];
        if (height<a.y || height>b.y) continue;
        const float t=(height-a.y)/(b.y-a.y);
        return {height,a.halfWidth+(b.halfWidth-a.halfWidth)*t,
                a.front+(b.front-a.front)*t,a.rear+(b.rear-a.rear)*t,
                a.corner+(b.corner-a.corner)*t};
    }
    return height<sections.front().y ? sections.front() : sections.back();
}

template<std::size_t N>
inline void fittedCasting(std::array<Section, N> sections, Vector3 dimensions, Color paint)
{
    // Bake fixture proportions into the section vertices and normals, rather
    // than applying a nonuniform render transform to a finished mesh.
    for (auto &section : sections)
    {
        section.y *= dimensions.y;
        section.halfWidth *= dimensions.x;
        section.front *= dimensions.z;
        section.rear *= dimensions.z;
        section.corner *= std::min(dimensions.x, dimensions.z);
    }
    casting(sections, paint);
}

inline Color blend(Color a, Color b, float t)
{
    return {static_cast<unsigned char>(a.r + (b.r - a.r) * t),
            static_cast<unsigned char>(a.g + (b.g - a.g) * t),
            static_cast<unsigned char>(a.b + (b.b - a.b) * t), a.a};
}

inline Color snowPaint(Vector3 p, Color white)
{
    // Two broad, asymmetric bands wrap the vehicle in model space. There is
    // no screen/world coordinate, animation clock or random source here.
    // Existing panels stay flat-colored instead of splitting into triangles.
    const float front = p.z + p.x * .50f + p.y * .08f;
    const float rear = p.z - p.x * .65f - p.y * .12f;
    const bool gray = std::abs(front + .25f + .12f * std::abs(p.x + .06f)) < .115f ||
                      (rear > .19f && rear < .38f);
    return gray ? tone(tagged({101, 113, 112, 255}), white.r / 216.0f) : white;
}

struct RoofPolygon
{
    // A 20-corner cap clipped by two six-edge patches needs at most 32 points.
    // Fixed storage avoids allocations in the immediate-mode draw path.
    std::array<Vector3, 32> points{};
    std::size_t count = 0;
};

inline void splitRoof(const RoofPolygon &polygon, Vector3 a, Vector3 b,
                      RoofPolygon &inside, RoofPolygon &outside)
{
    const auto distance = [&](Vector3 p) {
        return (b.x - a.x) * (p.z - a.z) - (b.z - a.z) * (p.x - a.x);
    };
    for (std::size_t i = 0; i < polygon.count; ++i)
    {
        const Vector3 p = polygon.points[i], q = polygon.points[(i + 1) % polygon.count];
        const float dp = distance(p), dq = distance(q);
        if (dp >= 0) inside.points[inside.count++] = p;
        if (dp <= 0) outside.points[outside.count++] = p;
        if ((dp < 0 && dq > 0) || (dp > 0 && dq < 0))
        {
            const float t = dp / (dp - dq);
            const Vector3 crossing{p.x + (q.x - p.x) * t,
                                   p.y + (q.y - p.y) * t,
                                   p.z + (q.z - p.z) * t};
            inside.points[inside.count++] = crossing;
            outside.points[outside.count++] = crossing;
        }
    }
}

inline void emitRoof(const RoofPolygon &polygon, Color paint)
{
    for (std::size_t i = 1; i + 1 < polygon.count; ++i)
    {
        const Vector3 a = polygon.points[0], b = polygon.points[i + 1], c = polygon.points[i];
        if (std::abs((b.x - a.x) * (c.z - a.z) - (b.z - a.z) * (c.x - a.x)) < 1e-10f)
            continue;
        mesh::vertex(a, {0, 1, 0}, paint);
        mesh::vertex(b, {0, 1, 0}, paint);
        mesh::vertex(c, {0, 1, 0}, paint);
    }
}

inline void paintRoof(const RoofPolygon &polygon, const Section &cap,
                      Color white, std::size_t patch = 0)
{
    // Large authored islands cross the old triangle boundaries. Clip their
    // white complement as well as their gray interior, so every square unit
    // of the original cap is drawn once with its original plane and normal.
    constexpr std::array<std::array<Vector2, 6>, 2> patches{{
        {{{-1.20f, -.90f}, {-.48f, -1.10f}, {.06f, -.62f},
          {-.12f, -.22f}, {-.56f, .20f}, {-1.20f, -.02f}}},
        {{{.42f, -.10f}, {1.15f, .10f}, {1.20f, .90f},
          {.88f, 1.12f}, {.58f, 1.15f}, {.24f, .56f}}}}};
    if (polygon.count < 3) return;
    if (patch == patches.size())
    {
        emitRoof(polygon, white);
        return;
    }
    const auto point = [&](Vector2 p) {
        return Vector3{p.x * cap.halfWidth, cap.y,
                       (cap.front + cap.rear) * .5f + p.y * (cap.rear - cap.front) * .5f};
    };
    RoofPolygon remaining = polygon;
    for (std::size_t i = 0; i < patches[patch].size(); ++i)
    {
        RoofPolygon inside, outside;
        splitRoof(remaining, point(patches[patch][i]),
                  point(patches[patch][(i + 1) % patches[patch].size()]), inside, outside);
        paintRoof(outside, cap, white, patch + 1);
        remaining = inside;
        if (remaining.count < 3) return;
    }
    emitRoof(remaining, tone(tagged({101, 113, 112, 255}), white.r / 216.0f));
}

template<std::size_t N>
inline void armorCasting(const std::array<Section, N> &s, Color paint,
                         bool snow, Vector3 offset = {0, 0, 0})
{
    if (!snow)
    {
        casting(s, paint);
        return;
    }
    const auto colorAt = [&](Vector3 p) {
        return snowPaint({p.x + offset.x, p.y + offset.y, p.z + offset.z}, paint);
    };
    // Side panels keep their original topology; caps are partitioned in their
    // existing plane. Identity and mechanical materials do not enter here.
    // Visible and shadow passes submit the same coverage and geometry.
    rlBegin(RL_TRIANGLES);
    for (std::size_t level = 0; level + 1 < N; ++level)
    {
        const auto lower = mesh::section(s[level]), upper = mesh::section(s[level + 1]);
        for (std::size_t i = 0; i < lower.size(); ++i)
        {
            const std::size_t j = (i + 1) % lower.size();
            const Vector3 center{(lower[i].x + lower[j].x + upper[i].x + upper[j].x) * .25f,
                (lower[i].y + upper[i].y) * .5f,
                (lower[i].z + lower[j].z + upper[i].z + upper[j].z) * .25f};
            quad(lower[j], lower[i], upper[i], upper[j], colorAt(center));
        }
    }
    const auto bottom = mesh::section(s.front()), top = mesh::section(s.back());
    RoofPolygon cap;
    std::copy(top.begin(), top.end(), cap.points.begin());
    cap.count = top.size();
    paintRoof(cap, s.back(), paint);
    for (std::size_t i = 1; i + 1 < top.size(); ++i)
    {
        triangle(bottom[0], bottom[i], bottom[i + 1], tone(paint, .65f));
    }
    rlEnd();
}

// A connected recoil jacket, collar and hollow muzzle. The last ring is at
// the pre-existing attachment, irrespective of cabin proportions or family.
inline void cannon(float root, float tip, float y, float radius,
                   Color paint, bool brake, float x = 0.0f, bool slender = false)
{
    const float length = root - tip;
    const std::array<float, 6> z = slender
        ? std::array<float, 6>{{root, root - length * .10f,
            root - length * .18f, tip + length * .16f, tip + length * .13f, tip}}
        : std::array<float, 6>{{root, root - length * .27f,
            root - length * .44f, tip + length * .29f, tip + length * .20f, tip}};
    const std::array<float, 6> r = slender
        ? std::array<float, 6>{{radius * 1.30f, radius * 1.30f,
            radius, radius * .95f, radius * (brake ? 1.70f : 1.03f),
            radius * (brake ? 1.60f : 1.00f)}}
        : std::array<float, 6>{{radius * 1.55f, radius * 1.65f,
            radius * 1.18f, radius, radius * (brake ? 1.35f : 1.17f),
            radius * (brake ? 1.30f : 1.12f)}};
    const Color steel = tagged(slender ? Color{104, 118, 113, 255}
                                      : Color{116, 130, 125, 255}, 15);
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
                 level < (slender ? 3U : 2U) ? paint : steel);
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
    if (slender && brake)
    {
        const float brakeLength = length * .13f;
        for (float side : {-1.0f, 1.0f})
        for (float station : {.27f, .72f})
        {
            const float center = tip + brakeLength * station;
            const float x = side * radius * (1.60f + .10f * station);
            const float y0 = y - radius * .40f, y1 = y + radius * .40f;
            const float z0 = center - brakeLength * .13f, z1 = center + brakeLength * .13f;
            if (side > 0)
                quad({x,y0,z0},{x,y1,z0},{x,y1,z1},{x,y0,z1},bore);
            else
                quad({x,y0,z1},{x,y1,z1},{x,y1,z0},{x,y0,z0},bore);
        }
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
    if (d.cartoon != CartoonModel::None)
    {
        // Connected plate/casting sections retain historical mass landmarks:
        // cast oval, hexagonal welded crown, sloping wedge or upright slab.
        // Equal maximum footprints keep upgrades comparable across nations.
        using M = CartoonModel;
        std::array<float, 6> widths{{.71f, .94f, 1.0f, .92f, .76f, .67f}};
        std::array<float, 6> fronts{{.15f, .035f, 0, .065f, .20f, .25f}};
        std::array<float, 6> rears{{.18f, .035f, 0, .04f, .14f, .20f}};
        std::array<float, 6> heights{{0, .17f, .43f, .73f, .94f, 1}};
        float roundness = .47f;
        switch (d.cartoon)
        {
        case M::Sherman:
            widths = {{.72f, .95f, 1, .96f, .82f, .77f}};
            fronts = {{.16f, .04f, 0, .025f, .12f, .16f}};
            rears = {{.19f, .06f, 0, 0, .07f, .10f}};
            heights = {{0, .15f, .43f, .77f, .95f, 1}};
            roundness = .62f;
            break;
        case M::Pershing:
            widths = {{.81f, .97f, 1, .99f, .91f, .88f}};
            fronts = {{.15f, .04f, 0, .02f, .085f, .12f}};
            rears = {{.20f, .07f, 0, 0, .025f, .035f}};
            heights = {{0, .12f, .36f, .80f, .96f, 1}};
            roundness = .56f;
            break;
        case M::Patton:
            widths = {{.68f, .93f, 1, .87f, .73f, .69f}};
            fronts = {{.13f, .018f, 0, .085f, .24f, .28f}};
            rears = {{.20f, .035f, 0, 0, .025f, .04f}};
            heights = {{0, .10f, .36f, .76f, .96f, 1}};
            roundness = .88f;
            break;
        case M::Abrams:
            widths = {{.90f, 1, 1, .99f, .97f, .97f}};
            fronts = {{.04f, 0, .03f, .26f, .29f, .29f}};
            rears = {{.07f, .01f, 0, 0, .006f, .006f}};
            heights = {{0, .06f, .18f, .92f, .99f, 1}};
            roundness = .035f;
            break;
        case M::T34:
            widths = {{.81f, 1, .98f, .89f, .80f, .78f}};
            fronts = {{.09f, 0, .02f, .10f, .16f, .175f}};
            rears = {{.10f, 0, .02f, .06f, .10f, .115f}};
            heights = {{0, .10f, .25f, .81f, .98f, 1}};
            roundness = .40f;
            break;
        case M::Stalin:
            widths = {{.73f, .97f, 1, .94f, .80f, .70f}};
            fronts = {{.16f, .035f, 0, .035f, .14f, .18f}};
            rears = {{.17f, .03f, 0, .02f, .055f, .08f}};
            heights = {{0, .15f, .40f, .81f, .96f, 1}};
            roundness = .64f;
            break;
        case M::T62:
            widths = {{.70f, .94f, 1, .88f, .68f, .52f}};
            fronts = {{.10f, .016f, 0, .070f, .176f, .240f}};
            rears = {{.117f, .019f, 0, .08f, .20f, .26f}};
            heights = {{0, .12f, .27f, .56f, .85f, 1}};
            roundness = .87f;
            break;
        case M::T90:
            widths = {{.78f, 1, .99f, .88f, .82f, .80f}};
            fronts = {{.07f, 0, .045f, .17f, .223f, .229f}};
            rears = {{.048f, 0, .008f, .055f, .077f, .08f}};
            heights = {{0, .12f, .28f, .65f, .96f, 1}};
            roundness = .17f;
            break;
        case M::Panther:
            widths = {{.88f, 1, .97f, .80f, .76f, .75f}};
            fronts = {{.045f, 0, .015f, .105f, .125f, .13f}};
            rears = {{.055f, 0, .005f, .015f, .025f, .03f}};
            heights = {{0, .08f, .22f, .91f, .98f, 1}};
            roundness = .13f;
            break;
        case M::TigerII:
            widths = {{.90f, 1, 1, .90f, .87f, .86f}};
            fronts = {{.045f, 0, .01f, .075f, .085f, .09f}};
            rears = {{.045f, 0, 0, .015f, .02f, .025f}};
            heights = {{0, .08f, .22f, .92f, .98f, 1}};
            roundness = .12f;
            break;
        case M::Leopard1:
            widths = {{.74f, .96f, 1, .93f, .84f, .79f}};
            fronts = {{.14f, .025f, 0, .065f, .175f, .21f}};
            rears = {{.17f, .035f, 0, .012f, .025f, .04f}};
            heights = {{0, .11f, .31f, .76f, .95f, 1}};
            roundness = .72f;
            break;
        case M::Leopard2:
            widths = {{.94f, 1, 1, .99f, .96f, .95f}};
            fronts = {{.025f, 0, 0, .01f, .02f, .025f}};
            rears = {{.035f, 0, 0, .005f, .015f, .02f}};
            heights = {{0, .06f, .20f, .91f, .98f, 1}};
            roundness = .065f;
            break;
        case M::None:
            break;
        }
        // Welded armor uses four structural rings. Extra attachment stations
        // lie exactly on their connecting planes, not separate rounded strips.
        std::array<std::array<float, 5>, 4> controls{};
        bool welded = true;
        switch (d.cartoon)
        {
        case M::Abrams:
            controls = {{{{0.0f,0.88f,0.035f,-0.04f,0.64f}},
                         {{0.12f,1.0f,0.0f,0.0f,0.78f}},
                         {{0.86f,0.99f,0.18f,-0.004f,0.64f}},
                         {{1.0f,0.94f,0.205f,-0.02f,0.59f}}}}; break;
        case M::T34:
            controls = {{{{0.0f,0.78f,0.055f,-0.035f,0.46f}},
                         {{0.13f,1.0f,0.0f,0.0f,0.52f}},
                         {{0.94f,0.76f,0.145f,-0.062f,0.45f}},
                         {{1.0f,0.74f,0.15f,-0.068f,0.44f}}}}; break;
        case M::T90:
            controls = {{{{0.0f,0.79f,0.05f,-0.03f,0.32f}},
                         {{0.12f,1.0f,0.0f,0.0f,0.44f}},
                         {{0.92f,0.84f,0.145f,-0.055f,0.34f}},
                         {{1.0f,0.82f,0.16f,-0.065f,0.33f}}}}; break;
        case M::Panther:
            controls = {{{{0.0f,0.87f,0.025f,-0.025f,0.19f}},
                         {{0.11f,1.0f,0.0f,0.0f,0.22f}},
                         {{0.94f,0.73f,0.15f,-0.075f,0.16f}},
                         {{1.0f,0.71f,0.165f,-0.09f,0.15f}}}}; break;
        case M::TigerII:
            controls = {{{{0.0f,0.88f,0.025f,-0.012f,0.25f}},
                         {{0.12f,1.0f,0.0f,0.0f,0.29f}},
                         {{0.94f,0.85f,0.11f,-0.06f,0.25f}},
                         {{1.0f,0.83f,0.12f,-0.066f,0.24f}}}}; break;
        case M::Leopard2:
            controls = {{{{0.0f,0.91f,0.015f,-0.012f,0.13f}},
                         {{0.1f,1.0f,0.0f,0.0f,0.15f}},
                         {{0.94f,1.0f,0.025f,0.0f,0.14f}},
                         {{1.0f,0.97f,0.045f,-0.02f,0.13f}}}}; break;
        default: welded = false; break;
        }
        if (welded)
        {
            std::array<std::array<float, 5>, 6> rings{};
            rings[0] = controls[0]; rings[1] = controls[1]; rings[3] = controls[2]; rings[5] = controls[3];
            for (std::size_t j = 0; j < 5; ++j)
            {
                rings[2][j] = controls[1][j] + (controls[2][j] - controls[1][j]) * .25f;
                rings[4][j] = controls[2][j] + (controls[3][j] - controls[2][j]) * .80f;
            }
            for (std::size_t i = 0; i < 6; ++i)
            {
                heights[i] = rings[i][0]; widths[i] = rings[i][1];
                fronts[i] = rings[i][2]; rears[i] = -rings[i][3];
            }
        }
        std::array<Section, 6> result{};
        for (std::size_t i = 0; i < result.size(); ++i)
        {
            const float half = w * widths[i];
            const float front = f + s.cabinLength * fronts[i];
            const float rear = r - s.cabinLength * rears[i];
            result[i] = {d.base + h * heights[i], half, front, rear,
                         std::min(half * roundness, (rear - front) * .35f)};
        }
        return result;
    }
    // These are three different armor constructions, rather than one casting
    // recolored for each nation. A low Soviet wedge recedes across most of its
    // height; German slab cheeks remain upright until the small roof bevel.
    if (d.family == Family::Soviet)
        return {{{d.base, w * .82f, f + .035f, r - .08f, c * .65f},
                 {d.base + h * .16f, w * 1.06f, f - .06f, r + .03f, c * .98f},
                 {d.base + h * .44f, w * .99f, f + .015f, r + .015f, c},
                 {d.base + h * .78f, w * .76f, f + .14f, r - .045f, c * .75f},
                 {d.base + h * .96f, w * .64f, f + .18f, r - .08f, c * .60f},
                 {d.base + h, w * .60f, f + .19f, r - .09f, c * .56f}}};
    if (d.family == Family::German)
        return {{{d.base, w * .89f, f + .05f, r - .025f, c * .85f},
                 {d.base + h * .08f, w, f - .008f, r + .015f, c},
                 {d.base + h * .48f, w, f - .006f, r + .015f, c},
                 {d.base + h * .88f, w * .98f, f + .015f, r + .01f, c * .95f},
                 {d.base + h * .97f, w * .95f, f + .035f, r - .016f, c * .80f},
                 {d.base + h, w * .94f, f + .042f, r - .025f, c * .75f}}};
    if (d.casemate)
        return {{{d.base, w * .89f, f + .025f, r - .025f, c * .80f},
                 {d.base + h * .12f, w, f - .045f, r + .02f, c},
                 {d.base + h * .42f, w, f - .025f, r + .02f, c},
                 {d.base + h * .82f, w * .90f, f + .10f, r - .025f, c * .85f},
                 {d.base + h * .96f, w * .84f, f + .15f, r - .065f, c * .70f},
                 {d.base + h, w * .82f, f + .16f, r - .08f, c * .65f}}};
    return {{{d.base, w * .80f, f + .045f, r - .02f, c * .65f},
             {d.base + h * .17f, w * .94f, f - .03f, r + .015f, c * .94f},
             {d.base + h * .53f, w * 1.04f, f - .04f, r + .045f, c},
             {d.base + h * .80f, w * .91f, f + shift * .69f, r + .005f, c * .82f},
             {d.base + h * .96f, w * (d.roofWidth + .10f),
              f + .06f + shift, r - .065f, c * .68f},
             {d.base + h, w * d.roofWidth,
              f + .10f + shift, r - .115f, c * .53f}}};
}

inline std::array<Section, 5> authoredHullSections(const Design &d)
{
    const auto &s = d.shape;
    const float inner = s.trackCenter - s.trackWidth * .5f - .01f;
    const float length = s.trackLength;
    if (d.cartoon != CartoonModel::None)
    {
        const float base = .47f + d.tier * .022f;
        const float shoulder = .35f * d.chassisWidth;
        using M = CartoonModel;
        if (d.cartoon == M::T34)
            return {{{.14f, shoulder * .68f, -length * .34f, length * .32f, shoulder * .09f},
                     {.25f, shoulder * .94f, -length * .45f, length * .45f, shoulder * .12f},
                     {base - .17f, shoulder, -length * .42f, length * .44f, shoulder * .10f},
                     {base - .020f, shoulder * .84f, -length * .23f, length * .38f, shoulder * .08f},
                     {base + .006f, shoulder * .81f, -length * .20f, length * .37f, shoulder * .07f}}};
        if (d.cartoon == M::Stalin)
            return {{{.14f, shoulder * .73f, -length * .34f, length * .35f, shoulder * .25f},
                     {.25f, shoulder, -length * .45f, length * .45f, shoulder * .35f},
                     {base - .13f, shoulder, -length * .41f, length * .44f, shoulder * .32f},
                     {base - .012f, shoulder * .88f, -length * .27f, length * .42f, shoulder * .20f},
                     {base + .006f, shoulder * .84f, -length * .25f, length * .40f, shoulder * .17f}}};
        if (d.cartoon == M::T62)
            return {{{.14f, shoulder * .83f, -length * .34f, length * .37f, shoulder * .10f},
                     {.25f, shoulder, -length * .45f, length * .45f, shoulder * .12f},
                     {base - .14f, shoulder, -length * .43f, length * .44f, shoulder * .12f},
                     {base - .015f, shoulder * .96f, -length * .30f, length * .43f, shoulder * .09f},
                     {base + .006f, shoulder * .94f, -length * .28f, length * .42f, shoulder * .08f}}};
        if (d.cartoon == M::T90)
            return {{{.14f, shoulder * .80f, -length * .35f, length * .36f, shoulder * .09f},
                     {.25f, shoulder, -length * .45f, length * .45f, shoulder * .12f},
                     {base - .14f, shoulder, -length * .41f, length * .44f, shoulder * .11f},
                     {base - .024f, shoulder * .96f, -length * .28f, length * .42f, shoulder * .09f},
                     {base + .008f, shoulder * .92f, -length * .25f, length * .41f, shoulder * .08f}}};
        if (d.cartoon == M::Panther)
            return {{{.14f, shoulder * .78f, -length * .32f, length * .34f, shoulder * .045f},
                     {.25f, shoulder, -length * .45f, length * .45f, shoulder * .07f},
                     {base - .15f, shoulder, -length * .43f, length * .45f, shoulder * .065f},
                     {base - .015f, shoulder * .92f, -length * .23f, length * .43f, shoulder * .05f},
                     {base + .012f, shoulder * .89f, -length * .205f, length * .41f, shoulder * .045f}}};
        if (d.cartoon == M::TigerII)
            return {{{.14f, shoulder * .88f, -length * .36f, length * .38f, shoulder * .055f},
                     {.25f, shoulder, -length * .45f, length * .45f, shoulder * .075f},
                     {base - .12f, shoulder, -length * .425f, length * .45f, shoulder * .07f},
                     {base - .014f, shoulder * .97f, -length * .28f, length * .44f, shoulder * .045f},
                     {base + .014f, shoulder * .94f, -length * .25f, length * .42f, shoulder * .04f}}};
        if (d.cartoon == M::Leopard1)
            return {{{.14f, shoulder * .78f, -length * .34f, length * .35f, shoulder * .10f},
                     {.25f, shoulder, -length * .45f, length * .45f, shoulder * .13f},
                     {base - .16f, shoulder, -length * .40f, length * .44f, shoulder * .13f},
                     {base - .03f, shoulder * .89f, -length * .26f, length * .42f, shoulder * .09f},
                     {base + .008f, shoulder * .85f, -length * .235f, length * .40f, shoulder * .085f}}};
        if (d.cartoon == M::Leopard2)
            return {{{.14f, shoulder * .89f, -length * .37f, length * .38f, shoulder * .045f},
                     {.25f, shoulder, -length * .45f, length * .45f, shoulder * .065f},
                     {base - .12f, shoulder, -length * .435f, length * .45f, shoulder * .06f},
                     {base - .01f, shoulder * .98f, -length * .335f, length * .44f, shoulder * .045f},
                     {base + .015f, shoulder * .96f, -length * .31f, length * .43f, shoulder * .04f}}};
        if (d.cartoon == CartoonModel::Sherman)
            // A tall welded glacis meets a broad flat upper deck. Rounded
            // transmission cover remains below it instead of rounding the hull.
            return {{{.14f, shoulder * .75f, -length * .32f, length * .34f, shoulder * .18f},
                     {.25f, shoulder, -length * .45f, length * .45f, shoulder * .25f},
                     {base - .16f, shoulder, -length * .40f, length * .44f, shoulder * .13f},
                     {base + .005f, shoulder * .91f, -length * .25f, length * .43f, shoulder * .08f},
                     {base + .035f, shoulder * .89f, -length * .23f, length * .41f, shoulder * .065f}}};
        if (d.cartoon == CartoonModel::Abrams)
            return {{{.14f, shoulder * .81f, -length * .34f, length * .38f, shoulder * .08f},
                     {.25f, shoulder, -length * .45f, length * .45f, shoulder * .10f},
                     {base - .13f, shoulder, -length * .43f, length * .45f, shoulder * .09f},
                     {base - .012f, shoulder * .98f, -length * .31f, length * .44f, shoulder * .07f},
                     {base + .014f, shoulder * .95f, -length * .29f, length * .43f, shoulder * .06f}}};
        const bool patton = d.cartoon == CartoonModel::Patton;
        const float topWidth = patton ? .90f : .85f;
        const float topCorner = patton ? shoulder * .16f : .16f * (1.90f * d.chassisWidth / 1.70f);
        return {{{.14f, shoulder * .73f, -length * .31f, length * .34f, shoulder * .24f},
                 {.25f, shoulder, -length * .45f, length * .45f, shoulder * .27f},
                 {base - .14f, shoulder, -length * .41f, length * .45f, shoulder * .25f},
                 {base - .018f, shoulder * .93f, -length * (patton ? .25f : .30f), length * .43f, shoulder * .19f},
                 {base + .015f, shoulder * topWidth, -length * (patton ? .22f : .28f), length * .41f, topCorner}}};
    }
    if (d.family == Family::Soviet)
        return {{{.17f, inner * .72f, -length * .34f, length * .33f, .06f},
                 {.27f, inner * .96f, -length * .47f, length * .41f, .075f},
                 {.34f, inner, -length * .42f, length * .42f, .075f},
                 {d.base - .025f, inner * .92f, -length * .22f, length * .36f, .06f},
                 {d.base + .01f, inner * .90f, -length * .18f, length * .32f, .055f}}};
    if (d.family == Family::German)
        return {{{.17f, inner * .88f, -length * .35f, length * .35f, .025f},
                 {.27f, inner, -length * .40f, length * .42f, .035f},
                 {d.base - .085f, inner, -length * .395f, length * .42f, .035f},
                 {d.base - .025f, inner * .95f, -length * .36f, length * .39f, .03f},
                 {d.base + .01f, inner * .93f, -length * .34f, length * .36f, .025f}}};
    return {{{.17f, inner * .86f, -length * .33f, length * .33f, .075f},
             {.27f, inner, -length * .42f, length * .41f, .09f},
             {.38f, inner, -length * .40f, length * .42f, .09f},
             {d.base - .02f, inner * .96f, -length * .31f, length * .38f, .075f},
             {d.base + .01f, inner * .94f, -length * .26f, length * .32f, .07f}}};
}

inline std::array<Section, 5> hullSections(const Design &d)
{
    auto hull = authoredHullSections(d);
    if (d.cartoon != CartoonModel::None)
    {
        // Keep each researched glacis/shoulder plan, but fit its ordered
        // vertical sections to the actual deck height of that vehicle.
        const float bottom = .12f;
        const float oldBottom = hull.front().y;
        const float oldHeight = hull.back().y - oldBottom;
        for (auto &section : hull)
            section.y = bottom + (section.y - oldBottom) / oldHeight * (d.hullTop - bottom);
    }
    return hull;
}

inline float dot(Vector3 a, Vector3 b)
{
    return a.x*b.x + a.y*b.y + a.z*b.z;
}

inline Vector3 unit(Vector3 p)
{
    const float length = std::sqrt(dot(p,p));
    return length > .0000001f ? Vector3{p.x/length,p.y/length,p.z/length} : Vector3{0,1,0};
}

struct PanelNormals
{
    std::array<Vector3,3> points{}, normals{};

    Vector3 at(Vector3 point) const
    {
        const auto u = mesh::minus(points[1],points[0]);
        const auto v = mesh::minus(points[2],points[0]);
        const auto p = mesh::minus(point,points[0]);
        const float uu=dot(u,u), uv=dot(u,v), vv=dot(v,v), pu=dot(p,u), pv=dot(p,v);
        const float determinant=uu*vv-uv*uv;
        if (std::abs(determinant)<.00000000001f) return normals[0];
        const float b=(pu*vv-pv*uv)/determinant, c=(pv*uu-pu*uv)/determinant, a=1-b-c;
        return unit({normals[0].x*a+normals[1].x*b+normals[2].x*c,
                     normals[0].y*a+normals[1].y*b+normals[2].y*c,
                     normals[0].z*a+normals[1].z*b+normals[2].z*c});
    }
};

inline void emitPaintedPanel(const RoofPolygon &polygon, Vector3 normal, Color paint,
                             const PanelNormals *smooth = nullptr)
{
    for (std::size_t i = 1; i + 1 < polygon.count; ++i)
    {
        for (std::size_t index : {std::size_t{0}, i, i+1})
            mesh::vertex(polygon.points[index], smooth ? smooth->at(polygon.points[index]) : normal, paint);
    }
}

inline void cartoonPanel(const RoofPolygon &polygon, Vector3 normal,
                         Color paint, Family family, std::size_t patch = 0,
                         Vector2 paintScale = {1.0f, 1.0f},
                         const PanelNormals *smooth = nullptr)
{
    // Broad model-space islands, cut into the same plane instead of layered
    // decals: no z-fighting, shadow duplication, texture swimming or RNG.
    constexpr std::array<std::array<Vector2, 6>, 2> patches{{
        {{{-.90f, -.84f}, {-.19f, -.94f}, {.025f, -.43f},
          {-.17f, -.15f}, {-.43f, .04f}, {-.86f, -.19f}}},
        {{{.14f, .02f}, {.79f, .25f}, {.87f, .87f},
          {.45f, .93f}, {.30f, .48f}, {.03f, .24f}}}}};
    if (polygon.count < 3) return;
    if (family == Family::German || patch == patches.size())
    {
        emitPaintedPanel(polygon, normal, paint, smooth);
        return;
    }
    RoofPolygon remaining = polygon;
    for (std::size_t i = 0; i < patches[patch].size(); ++i)
    {
        const auto a = patches[patch][i], b = patches[patch][(i + 1) % patches[patch].size()];
        RoofPolygon inside, outside;
        splitRoof(remaining, {a.x * paintScale.x, 0, a.y * paintScale.y},
                  {b.x * paintScale.x, 0, b.y * paintScale.y}, inside, outside);
        cartoonPanel(outside, normal, paint, family, patch + 1, paintScale, smooth);
        remaining = inside;
        if (remaining.count < 3) return;
    }
    const Color camouflage = family == Family::Soviet
        ? tagged({71, 85, 65, 255})
        : patch == 0 ? tagged({89, 75, 55, 255}) : tagged({41, 46, 41, 255});
    emitPaintedPanel(remaining, normal, camouflage, smooth);
}

template<std::size_t N>
inline void profiledArmor(const std::array<Section,N> &sections, Color paint,
                          Family family, Vector2 paintScale, const ArmorPlan &plan,
                          CartoonModel model)
{
    constexpr std::size_t kRingCapacity=N==6 ? 20 : 12;
    const bool refinedCast=N==6 && !hardArmor(plan);
    std::size_t shoulder=1;
    if (refinedCast)
        for (std::size_t level=2;level+1<N;++level)
            if (sections[level].halfWidth>sections[shoulder].halfWidth) shoulder=level;
    struct Prepared
    {
        std::array<Section,N> sections{};
        ArmorPlan plan{};
        std::array<std::array<Vector3,kRingCapacity>,N> rings{}, normals{}, neckNormals{};
        bool ready=false;
    };
    // Exactly twelve hull and twelve turret entries; colors and player state
    // never enter the cache. Alternate diagnostic sections use a local entry.
    static std::array<Prepared,12> cached;
    Prepared local;
    Prepared *prepared=&local;
    const int slot=static_cast<int>(model)-1;
    if ((N==5 || N==6) && slot>=0 && slot<12)
    {
        auto &candidate=cached[static_cast<std::size_t>(slot)];
        const auto samePlan=[](const ArmorPlan &a,const ArmorPlan &b) {
            return a.frontCutX==b.frontCutX && a.frontCutZ==b.frontCutZ &&
                a.rearCutX==b.rearCutX && a.rearCutZ==b.rearCutZ &&
                a.frontCurve==b.frontCurve && a.rearCurve==b.rearCurve && a.rearWidth==b.rearWidth;
        };
        bool same=samePlan(plan,candidate.plan);
        for (std::size_t i=0;i<N;++i)
        {
            const auto &a=sections[i], &b=candidate.sections[i];
            same=same && a.y==b.y && a.halfWidth==b.halfWidth && a.front==b.front &&
                a.rear==b.rear && a.corner==b.corner;
        }
        if (!candidate.ready || same) prepared=&candidate;
    }
    auto &rings=prepared->rings, &smoothNormals=prepared->normals;
    if (!prepared->ready)
    {
        prepared->sections=sections;prepared->plan=plan;
        for (std::size_t level=0;level<N;++level)
        {
            if constexpr (N==6)
                if (refinedCast)
                {
                    rings[level]=castTurretSection(sections[level],plan);
                    continue;
                }
            const auto ring=armorSection(sections[level],plan,&sections[1]);
            std::copy(ring.begin(),ring.end(),rings[level].begin());
        }
    }
    const bool hard = hardArmor(plan);
    std::array<std::size_t,kRingCapacity> corners{};
    std::size_t count=0;
    for (std::size_t i=0;i<(refinedCast ? 20U : 12U);++i)
        if (!hard || (i!=2 && i!=5 && i!=8 && i!=11)) corners[count++]=i;
    const auto visitFaces = [&](const auto &face) {
        for (std::size_t level=0;level+1<N;++level)
        for (std::size_t i=0;i<count;++i)
        {
            const auto a=corners[i], b=corners[(i+1)%count];
            face(std::array<std::size_t,3>{{level,level,level+1}},std::array<std::size_t,3>{{b,a,a}});
            face(std::array<std::size_t,3>{{level,level+1,level+1}},std::array<std::size_t,3>{{b,a,b}});
        }
    };
    if (!hard && !prepared->ready)
        visitFaces([&](const auto &levels,const auto &indices) {
            const std::array<Vector3,3> p{{rings[levels[0]][indices[0]],rings[levels[1]][indices[1]],rings[levels[2]][indices[2]]}};
            const auto n=mesh::normal(p[0],p[1],p[2]);
            for (std::size_t i=0;i<3;++i)
            {
                const auto u=unit(mesh::minus(p[(i+1)%3],p[i])), v=unit(mesh::minus(p[(i+2)%3],p[i]));
                const float weight=std::acos(std::clamp(dot(u,v),-1.0f,1.0f));
                auto &group=refinedCast && levels[0]<shoulder ? prepared->neckNormals : smoothNormals;
                auto &sum=group[levels[i]][indices[i]];
                sum.x+=n.x*weight;sum.y+=n.y*weight;sum.z+=n.z*weight;
            }
        });
    prepared->ready=true;
    rlBegin(RL_TRIANGLES);
    if (hard)
    {
        // Corresponding welded edges are parallel by construction, so clip
        // each true planar quad once. This also avoids artificial diagonal
        // paint partitions and repeated work in the immediate renderer.
        for (std::size_t level=0;level+1<N;++level)
        for (std::size_t i=0;i<count;++i)
        {
            const auto a=corners[i], b=corners[(i+1)%count];
            RoofPolygon polygon;
            polygon.count=4;
            polygon.points[0]=rings[level][b];polygon.points[1]=rings[level][a];
            polygon.points[2]=rings[level+1][a];polygon.points[3]=rings[level+1][b];
            cartoonPanel(polygon,mesh::normal(polygon.points[0],polygon.points[1],polygon.points[3]),
                         paint,family,0,paintScale);
        }
    }
    else visitFaces([&](const auto &levels,const auto &indices) {
        PanelNormals shading;
        RoofPolygon polygon;
        polygon.count=3;
        for (std::size_t i=0;i<3;++i) polygon.points[i]=shading.points[i]=rings[levels[i]][indices[i]];
        const auto n=mesh::normal(polygon.points[0],polygon.points[1],polygon.points[2]);
        for (std::size_t i=0;i<3;++i)
        {
            const auto &group=refinedCast && levels[0]<shoulder ? prepared->neckNormals : smoothNormals;
            const auto averaged=unit(group[levels[i]][indices[i]]);
            if (refinedCast)
            {
                // One normal per shared side vertex within the lower neck or
                // main shoulder. The widest station is their structural seam;
                // roof and lower cap stay separate hard surfaces as well.
                shading.normals[i]=averaged;
                continue;
            }
            auto softened=unit({n.x*.58f+averaged.x*.42f,n.y*.58f+averaged.y*.42f,n.z*.58f+averaged.z*.42f});
            if (dot(n,softened)<.84f)
                softened=unit({n.x*.65f+softened.x*.35f,n.y*.65f+softened.y*.35f,n.z*.65f+softened.z*.35f});
            shading.normals[i]=hard ? n : softened;
        }
        // Clip colors on each true triangle plane; preserve curved vertex
        // normals through clipping, never smooth a welded armor seam.
        cartoonPanel(polygon,n,paint,family,0,paintScale,hard ? nullptr : &shading);
    });
    RoofPolygon cap;
    cap.count=count;
    for (std::size_t i=0;i<count;++i) cap.points[i]=rings.back()[corners[count-1-i]];
    cartoonPanel(cap,{0,1,0},paint,family,0,paintScale);
    for (std::size_t i=1;i+1<count;++i)
        triangle(rings.front()[corners[0]],rings.front()[corners[i]],rings.front()[corners[i+1]],tone(paint,.57f));
    rlEnd();
}

template<std::size_t N>
inline void cartoonCasting(const std::array<Section, N> &sections, Color paint,
                            Family family, Vector2 paintScale = {1.0f, 1.0f},
                            ArmorPlan plan = {}, CartoonModel model = CartoonModel::None)
{
    if (plan.frontCutX >= 0)
    {
        profiledArmor(sections,paint,family,paintScale,plan,model);
        return;
    }
    rlBegin(RL_TRIANGLES);
    const auto panel = [&](Vector3 a, Vector3 b, Vector3 c, Vector3 d) {
        RoofPolygon polygon;
        polygon.points[0] = a; polygon.points[1] = b;
        polygon.points[2] = c; polygon.points[3] = d;
        polygon.count = 4;
        cartoonPanel(polygon, mesh::normal(a, b, d), paint, family, 0, paintScale);
    };
    for (std::size_t level = 0; level + 1 < N; ++level)
    {
        const auto a = mesh::section(sections[level]), b = mesh::section(sections[level + 1]);
        for (std::size_t i = 0; i < a.size(); ++i)
        {
            const std::size_t j = (i + 1) % a.size();
            panel(a[j], a[i], b[i], b[j]);
        }
    }
    const auto bottom = mesh::section(sections.front()), top = mesh::section(sections.back());
    RoofPolygon cap;
    std::reverse_copy(top.begin(), top.end(), cap.points.begin());
    cap.count = top.size();
    cartoonPanel(cap, {0, 1, 0}, paint, family, 0, paintScale);
    for (std::size_t i = 1; i + 1 < bottom.size(); ++i)
        triangle(bottom[0], bottom[i], bottom[i + 1], tone(paint, .57f));
    rlEnd();
}

inline void curvedFender(const Design &d, float side, Color paint, Color light, Color ink)
{
    const auto &s = d.shape;
    const bool slab = d.family == Family::German && d.cartoon != CartoonModel::Leopard1;
    const std::array<float, 7> depths = slab
        ? std::array<float, 7>{{-.5f, -.44f, -.40f, 0, .40f, .44f, .49f}}
        : std::array<float, 7>{{-.5f, -.42f, -.31f, 0, .31f, .42f, .49f}};
    const std::array<float, 7> elevations = slab
        ? std::array<float, 7>{{-.045f, .05f, .055f, .055f, .055f, .05f, -.035f}}
        : std::array<float, 7>{{-.09f, .025f, .055f, .055f, .055f, .025f, -.07f}};
    const float inner = side * (s.trackCenter - s.trackWidth * .55f);
    const float outer = side * (s.trackCenter + s.trackWidth *
                               (d.family == Family::Soviet ? .36f : .53f));
    const auto point = [&](std::size_t row, float x, float lower) {
        // Pershing's clipped, curved plan separates its cast shoulder from
        // the German straight shelf, especially at high gameplay elevations.
        constexpr std::array<float, 7> roundPlan{{.50f, .87f, 1.0f, 1.0f, .98f, .92f, .66f}};
        constexpr std::array<float, 7> sweptPlan{{.15f, .52f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f}};
        const float contour = d.cartoon == CartoonModel::Pershing ? roundPlan[row] :
                              d.cartoon == CartoonModel::Abrams ? sweptPlan[row] : 1.0f;
        return Vector3{inner + (x - inner) * contour,
                       s.trackHeight + .015f + elevations[row] * .40f - lower * .60f,
                       s.trackLength * depths[row]};
    };
    rlBegin(RL_TRIANGLES);
    for (std::size_t i = 0; i + 1 < depths.size(); ++i)
    {
        const auto a = point(i, inner, 0), b = point(i + 1, inner, 0);
        const auto c = point(i + 1, outer, .016f), e = point(i, outer, .016f);
        if (side > 0) quad(a, b, c, e, paint);
        else quad(e, c, b, a, paint);
        const auto f = point(i, outer, .048f), g = point(i + 1, outer, .048f);
        if (side > 0) quad(e, c, g, f, ink);
        else quad(f, g, c, e, ink);
        const float rim = outer - side * .018f;
        const auto u = point(i, rim, .013f), v = point(i + 1, rim, .013f);
        if (side > 0) quad(u, v, c, e, light);
        else quad(e, c, v, u, light);
    }
    // Closed underside and lips keep the shadow silhouette identical from
    // both sides, including the downturned nose around the track's bend.
    for (std::size_t i : {std::size_t{0}, depths.size() - 1})
    {
        const auto a = point(i, inner, 0), b = point(i, outer, .016f);
        const auto c = point(i, outer, .048f), e = point(i, inner, .032f);
        if ((i == 0) == (side > 0)) quad(a, b, c, e, ink);
        else quad(e, c, b, a, ink);
    }
    rlEnd();
}

inline void headlight(Vector3 center, float radius, Color paint)
{
    const Color ink = tagged({28, 35, 33, 255}, 15);
    const Color cream = tagged({250, 238, 176, 255});
    const Color highlight = tagged({255, 251, 216, 255});
    const auto point = [&](float r, float angle, float z) {
        return Vector3{center.x + r * std::cos(angle),
                       center.y + r * std::sin(angle), center.z + z};
    };
    rlBegin(RL_TRIANGLES);
    for (int i = 0; i < 12; ++i)
    {
        const float a = mesh::kPi * 2 * i / 12, b = mesh::kPi * 2 * (i + 1) / 12;
        triangle(point(0, 0, .025f), point(radius, a, .025f),
                 point(radius, b, .025f), paint);
        quad(point(radius, a, .025f), point(radius, b, .025f),
             point(radius * .91f, b, -.025f), point(radius * .91f, a, -.025f), paint);
        quad(point(radius * .91f, a, -.025f), point(radius * .91f, b, -.025f),
             point(radius * .75f, b, -.030f), point(radius * .75f, a, -.030f), ink);
        quad(point(radius * .75f, a, -.030f), point(radius * .75f, b, -.030f),
             point(radius * .57f, b, -.042f), point(radius * .57f, a, -.042f), cream);
        triangle(point(0, 0, -.045f), point(radius * .57f, b, -.042f),
                 point(radius * .57f, a, -.042f), highlight);
    }
    rlEnd();
}

struct RunningGear
{
    std::array<float, 9> centers{}; // Fractions of the straight belt run, front -Z.
    int returns = 0;
    float radius = .06f; // Fraction of the authored 1.70-unit chassis, scaled by tier only.
    bool frontDrive = false;
    bool overlap = false;
};

inline RunningGear runningGear(const Design &d)
{
    using M = CartoonModel;
    switch (d.cartoon)
    {
    case M::Sherman: return {{{-.45f,-.29f,-.08f,.08f,.29f,.45f}},3,.060f,true,false};
    case M::Pershing: return {{{-.47f,-.282f,-.094f,.094f,.282f,.47f}},5,.065f,false,false};
    case M::Patton: return {{{-.47f,-.282f,-.094f,.094f,.282f,.47f}},3,.066f,false,false};
    case M::Abrams: return {{{-.49f,-.327f,-.164f,0,.164f,.327f,.49f}},2,.055f,false,false};
    case M::T34: return {{{-.445f,-.2225f,0,.2225f,.445f}},0,.077f,false,false};
    case M::Stalin: return {{{-.47f,-.282f,-.094f,.094f,.282f,.47f}},3,.065f,false,false};
    // The three front stations are close; both rear gaps are larger. This
    // does not reuse the T-54/55's distinctive enlarged front gap.
    case M::T62: return {{{-.46f,-.245f,-.03f,.225f,.485f}},0,.074f,false,false};
    case M::T90: return {{{-.47f,-.282f,-.094f,.094f,.282f,.47f}},3,.067f,false,false};
    case M::Panther: return {{{-.48f,-.343f,-.206f,-.069f,.069f,.206f,.343f,.48f}},0,.101f,true,true};
    case M::TigerII: return {{{-.49f,-.3675f,-.245f,-.1225f,0,.1225f,.245f,.3675f,.49f}},0,.091f,true,true};
    case M::Leopard1: return {{{-.49f,-.327f,-.164f,0,.164f,.327f,.49f}},4,.058f,false,false};
    case M::Leopard2: return {{{-.49f,-.327f,-.164f,0,.164f,.327f,.49f}},4,.059f,false,false};
    case M::None: return {};
    }
    return {};
}

inline void runningWheel(float x, float y, float z, float radius, float side,
                         Color paint, int kind, bool moving)
{
    // A single connected annulus and inset disk. Raised drive teeth, rubber
    // tires and open idler spokes distinguish the three mechanical roles.
    const Color rubber = tagged({35, 43, 42, 255}, 16);
    const Color steel = tagged({122, 132, 127, 255}, 15);
    const Color dark = tagged({37, 48, 45, 255}, 15);
    const int count = kind == 3 ? 8 : 12;
    const float phase = moving ? static_cast<float>(GetTime()) * 2.0f : 0;
    const auto p = [&](float r, float angle, float depth) {
        return Vector3{x + side * depth, y + r * std::cos(angle), z + r * std::sin(angle)};
    };
    const auto face = [&](Vector3 a, Vector3 b, Vector3 c, Color color) {
        if (side > 0) triangle(a,b,c,color); else triangle(c,b,a,color);
    };
    rlBegin(RL_TRIANGLES);
    for (int i = 0; i < count; ++i)
    {
        const float a = 2 * mesh::kPi * i / count + phase;
        const float b = 2 * mesh::kPi * (i + 1) / count + phase;
        const float r0 = radius * (kind == 1 && i % 2 == 0 ? 1.08f : 1.0f);
        const float r1 = radius * (kind == 1 && i % 2 != 0 ? 1.08f : 1.0f);
        const Color rim = kind == 0 || kind == 3 ? rubber : steel;
        const auto a0 = p(r0,a,0), b0 = p(r1,b,0);
        const auto a1 = p(radius*.78f,a,.023f), b1 = p(radius*.78f,b,.023f);
        face(a0,b0,b1,rim); face(a0,b1,a1,rim);
        face(p(0,0,.024f),a1,b1,kind == 2 && i % 2 == 0 ? dark : paint);
        face(p(0,0,.029f),p(radius*.25f,a,.029f),p(radius*.25f,b,.029f),steel);
    }
    rlEnd();
}

inline void drawRunningGear(const Design &d, float side, Color paint, Color deep, bool moving)
{
    const auto &s = d.shape;
    const auto gear = runningGear(d);
    const float outer = s.trackCenter + s.trackWidth * .5f;
    const float radius = 1.70f * d.chassisScale * gear.radius;
    const float y = radius + .041f * d.chassisScale;
    const float run = s.trackLength - s.trackHeight;
    const Color wheelPaint=d.family == Family::Soviet ? paint :
        tagged(blend(paint,{119,132,120,255},.23f));
    mesh::belt(s, side, moving);
    // Alternating depth expresses the Panther's interleaved and Tiger II's
    // overlapping stations. Both preserve the same outer belt footprint.
    for (int layer = 0; layer < (gear.overlap ? 2 : 1); ++layer)
    for (int i = 0; i < d.wheels; ++i)
    {
        if (gear.overlap && i % 2 != layer) continue;
        const float inset = gear.overlap && layer == 0 ? .036f : .006f;
        runningWheel(side*(outer-inset),y,gear.centers[i]*run,
                     radius,side,wheelPaint,(d.cartoon == CartoonModel::TigerII || d.cartoon == CartoonModel::Stalin) ? 4 : 0,moving);
    }
    if (d.cartoon == CartoonModel::Sherman)
    {
        for (int bogie = 0; bogie < 3; ++bogie)
        {
            const float z = (gear.centers[bogie*2]+gear.centers[bogie*2+1])*.5f*run;
            // Vertical spring block joins two wheels to one VVSS bogie.
            DrawCubeV({side*(outer+.010f),y+.066f,z}, {.038f,.105f,.095f},deep);
            const float arm = (gear.centers[bogie*2+1]-gear.centers[bogie*2])*.5f*run;
            DrawCylinderEx({side*(outer+.028f),y+.042f,z-arm},
                           {side*(outer+.028f),y+.042f,z+arm},.022f,.022f,6,paint);
        }
    }
    for (int i = 0; i < gear.returns; ++i)
    {
        const float z = d.cartoon == CartoonModel::Sherman ?
            (gear.centers[i*2]+gear.centers[i*2+1])*.5f*run :
            gear.returns == 1 ? 0 : (-.34f+.68f*i/(gear.returns-1))*run;
        runningWheel(side*(outer-.025f),s.trackHeight-.080f,z,
                     s.trackHeight*.095f,side,deep,3,moving);
    }
    const float endZ = (s.trackLength-s.trackHeight)*.5f;
    for (float end : {-1.0f,1.0f})
    {
        const bool drive = (end < 0) == gear.frontDrive;
        const float axleHeight=d.cartoon == CartoonModel::Pershing && end<0 ? .62f : .54f;
        runningWheel(side*(outer-.010f),s.trackHeight*axleHeight+.006f,end*endZ,
                     s.trackHeight*(drive ? .32f : .29f),side,deep,drive ? 1 : 2,moving);
    }
}

inline void chassisDetails(const Design &d, const std::array<Section,5> &hull,
                           Color paint, Color deep, Color light, Color ink)
{
    using M = CartoonModel;
    const float scale = d.chassisWidth;
    const float depth = d.chassisLength / d.chassisScale;
    const float top = hull.back().y, rear = hull[2].rear;
    const Color steel = tagged({115,124,119,255},15);
    const auto plate = [&](float x, float width, float low, float high, Color color, float relief = .007f) {
        const auto point = [&](float px, float t, float offset) {
            return Vector3{px,hull[2].y+(hull[3].y-hull[2].y)*t,
                hull[2].front+(hull[3].front-hull[2].front)*t-offset};
        };
        rlBegin(RL_TRIANGLES);
        quad(point(x-width*.5f,low,relief),point(x-width*.5f,high,relief),
             point(x+width*.5f,high,relief),point(x+width*.5f,low,relief),color);
        rlEnd();
    };
    const auto deckPosition = [&](float x, float z, float width, float length) {
        const auto &cap = hull.back();
        const auto plan = armorPlan(d.cartoon,false);
        const float front = cap.front + length*.5f + .008f;
        const float rearLimit = cap.rear - length*.5f - .008f;
        z = std::clamp(z,front,rearLimit);
        float available = 0;
        const float requested = z;
        for (int step=0;step<=16;++step)
        {
            z = requested + ((front+rearLimit)*.5f-requested)*step/16.0f;
            available = std::min(armorHalfWidth(cap,plan,z-length*.5f,&hull[1]),
                                 armorHalfWidth(cap,plan,z+length*.5f,&hull[1]));
            if (d.shape.trackHeight+.037f > top+.020f)
                available = std::min(available,d.shape.trackCenter-d.shape.trackWidth*.55f-.008f);
            if (available >= width*.5f+.012f) break;
        }
        const float limit = std::max(0.0f,available-width*.5f-.012f);
        return Vector2{std::clamp(x,-limit,limit),z};
    };
    const auto deckVent = [&](float x, float z, float width, float length, int slats, float lift = 0) {
        const auto seated = deckPosition(x,z,width,length);
        x = seated.x;
        z = seated.y;
        DrawCubeV({x,top+.011f+lift,z},{width,.020f,length},ink);
        for (int i=0;i<slats;++i)
            DrawCubeV({x,top+.025f+lift,z-length*.38f+length*.76f*i/(slats-1)},
                      {width*.86f,.013f,length/(slats*2.4f)},deep);
    };
    const auto rearGrille = [&](float x,float y,float width,int slats) {
        y = std::min(y, top - .068f);
        DrawCubeV({x,y,rear+.010f},{width,.115f,.019f},ink);
        for (int i=0;i<slats;++i)
            DrawCubeV({x,y-.043f+.086f*i/(slats-1),rear+.025f},{width*.91f,.013f,.015f},deep);
    };
    const auto pipe = [&](float x,float y,float height,float radius) {
        DrawCylinderEx({x,y,rear+.022f},{x,y+height,rear+.022f},radius,radius,8,deep);
        DrawCylinderEx({x,y+height,rear+.022f},{x,y+height+.008f,rear+.022f},
                       radius*.73f,radius*.73f,8,ink);
    };
    const auto tow = [&](float x) {
        DrawCubeV({x,.20f,rear+.023f},{.045f,.037f,.052f},steel);
    };
    if (d.cartoon == M::Sherman)
    {
        casting(std::array<Section,4>{{
            {.155f,hull[2].halfWidth*.69f,hull[1].front+.055f*depth,hull[1].front+.17f*depth,.085f},
            {.205f,hull[2].halfWidth*.83f,hull[1].front+.004f*depth,hull[1].front+.17f*depth,.095f},
            {.265f,hull[2].halfWidth*.82f,hull[1].front+.012f*depth,hull[1].front+.17f*depth,.092f},
            {.305f,hull[2].halfWidth*.70f,hull[1].front+.075f*depth,hull[1].front+.16f*depth,.078f}}},deep);
        for (float side : {-1.0f,1.0f})
        {
            // Raised driver/co-driver hoods above the high welded glacis.
            DrawCubeV({side*.16f*scale,top+.015f,hull.back().front+.045f*depth},
                      {.14f*scale,.045f,.15f*d.chassisLength},paint);
            DrawCubeV({side*.16f*scale,top+.04f,hull.back().front-.012f*depth},
                      {.075f,.027f,.033f},ink);
            deckVent(side*.25f*scale,rear-.095f*depth,.12f*scale,.14f,3);
            tow(side*.22f*scale);
        }
        // The rear exhaust deflector and split access doors are recognisable
        // larger forms; no uniform field of tiny bolts is added.
        DrawCubeV({0,.23f,rear+.025f},{.48f*scale,.060f,.05f},deep);
        rearGrille(0,.35f,.40f*scale,3);
        plate(0,.38f*scale,.05f,.34f,deep);
    }
    else if (d.cartoon == M::Pershing)
    {
        plate(-.15f*scale,.17f*scale,.63f,.90f,deep);
        plate(.15f*scale,.17f*scale,.63f,.90f,deep);
        for (float side : {-1.0f,1.0f})
        {
            deckVent(side*.275f*scale,rear-.105f*depth,.115f,.19f,4);
            rearGrille(side*.14f*scale,.30f,.22f*scale,3);
            tow(side*.25f*scale);
        }
    }
    else if (d.cartoon == M::Patton)
    {
        plate(0,.19f*scale,.52f,.86f,deep);
        const auto bay = deckPosition(0,rear-.08f*depth,.53f*scale,.15f);
        DrawCubeV({0,top+.024f,bay.y},{.53f*scale,.052f,.15f},deep);
        deckVent(0,bay.y-.010f,.48f*scale,.13f,4,.05f);
        rearGrille(0,.34f,.64f*scale,4);
        for (float side : {-1.0f,1.0f})
            DrawCubeV({side*.29f*scale,top-.014f,rear-.19f*depth},{.11f,.045f,.20f},deep);
    }
    else if (d.cartoon == M::Abrams)
    {
        plate(0,.20f*scale,.64f,.96f,deep);
        // Wide turbine exhaust below the angular bustle, divided vertically.
        rearGrille(0,.32f,.70f*scale,4);
        for (float side : {-1.0f,1.0f})
        {
            DrawCubeV({side*.22f*scale,top-.068f,rear+.039f},{.025f,.13f,.024f},steel);
            deckVent(side*.285f*scale,rear-.10f*depth,.12f,.16f,3);
        }
    }
    else if (d.cartoon == M::T34)
    {
        plate(-.15f*scale,.24f*scale,.29f,.86f,deep);
        plate(-.15f*scale,.18f*scale,.37f,.77f,light,.012f);
        // Bow machine-gun socket contrasts with the flat driver's hatch.
        const float t=.49f, y=hull[2].y+(hull[3].y-hull[2].y)*t;
        const float z=hull[2].front+(hull[3].front-hull[2].front)*t;
        DrawCylinderEx({.19f*scale,y,z+.016f},{.19f*scale,y,z-.035f},.043f,.033f,8,deep);
        deckVent(0,rear-.10f*depth,.40f*scale,.17f,4);
        for (float side : {-1.0f,1.0f})
        {
            pipe(side*.18f*scale,.30f,.072f,.041f);
            tow(side*.27f*scale);
        }
    }
    else if (d.cartoon == M::Stalin)
    {
        // Early IS-2 stepped driver's nose, not the later one-plane glacis.
        casting(std::array<Section,3>{{
            {hull[2].y+.035f,.16f*scale,hull[2].front+.070f*depth,hull[3].front+.12f*depth,.045f},
            {hull[3].y+.015f,.15f*scale,hull[2].front+.11f*depth,hull[3].front+.12f*depth,.040f},
            {hull[3].y+.025f,.13f*scale,hull[3].front+.015f*depth,hull[3].front+.11f*depth,.035f}}},paint);
        plate(0,.28f*scale,.35f,.65f,deep);
        for (float side : {-1.0f,1.0f})
        {
            deckVent(side*.255f*scale,rear-.10f*depth,.14f,.19f,3);
            pipe(side*.22f*scale,.27f,.072f,.045f);
            tow(side*.30f*scale);
        }
        rearGrille(0,.36f,.25f*scale,3);
    }
    else if (d.cartoon == M::T62)
    {
        plate(-.15f*scale,.18f*scale,.60f,.91f,deep);
        deckVent(-.25f*scale,rear-.11f*depth,.17f,.18f,4);
        deckVent(.18f*scale,rear-.08f*depth,.25f,.13f,3);
        // Right fender stowage and the left exhaust distinguish the basic
        // hull without inventing a mandatory pair of optional rear drums.
        DrawCubeV({d.shape.trackCenter,d.shape.trackHeight+.088f,rear-.19f*depth},
                  {d.shape.trackWidth*.68f,.105f,.31f},deep);
        // Left rear exhaust outlet, separate from auxiliary fuel tanks.
        DrawCubeV({-hull[2].halfWidth-.012f,top-.075f,rear-.14f*depth},{.027f,.075f,.16f},ink);
        tow(-.23f*scale);tow(.23f*scale);
    }
    else if (d.cartoon == M::T90)
    {
        plate(0,.12f*scale,.79f,.98f,deep);
        for (int row=0;row<2;++row)
        for (float side : {-1.0f,1.0f})
            plate(side*.16f*scale,.24f*scale,.18f+row*.34f,.43f+row*.34f,light);
        deckVent(-.25f*scale,rear-.115f*depth,.14f,.18f,4);
        deckVent(.20f*scale,rear-.085f*depth,.22f,.14f,3);
        DrawCubeV({-hull[2].halfWidth-.013f,top-.09f,rear-.14f*depth},{.031f,.073f,.145f},ink);
    }
    else if (d.cartoon == M::Panther || d.cartoon == M::TigerII)
    {
        const bool panther=d.cartoon == M::Panther;
        plate(-.18f*scale,.15f*scale,.60f,.81f,deep);
        plate(.18f*scale,.10f*scale,.42f,.63f,deep);
        for (float side : {-1.0f,1.0f})
        {
            // Twin circular engine fans, rectilinear radiator slots and tall
            // rear exhausts give both hulls a recognisable German rear deck.
            const auto seated = deckPosition(side*.26f*scale,rear-.105f*depth,
                                              .144f*scale,.144f*scale);
            const float x=seated.x, z=seated.y;
            DrawCylinderEx({x,top+.004f,z},{x,top+.028f,z},.072f*scale,.072f*scale,10,ink);
            DrawCylinderEx({x,top+.029f,z},{x,top+.039f,z},.022f,.022f,8,steel);
            pipe(side*(panther ? .20f:.15f)*scale,.23f,panther ? .24f:.28f,panther ? .035f:.048f);
            DrawCubeV({side*.29f*scale,.25f,rear+.031f},{.10f,.16f,.045f},deep);
        }
    }
    else if (d.cartoon == M::Leopard1)
    {
        plate(.14f*scale,.20f*scale,.60f,.92f,deep);
        for (float side : {-1.0f,1.0f})
        {
            deckVent(side*.24f*scale,rear-.10f*depth,.15f,.19f,4);
            // Broad rear-quarter exhaust louvres are on the hull sides.
            const float x=side*(hull[2].halfWidth+.01f);
            DrawCubeV({x,top-.06f,rear-.14f*depth},{.025f,.09f,.19f},ink);
            for (int i=0;i<3;++i)
                DrawCubeV({x+side*.012f,top-.085f+i*.025f,rear-.14f*depth},{.012f,.01f,.17f},deep);
        }
    }
    else if (d.cartoon == M::Leopard2)
    {
        plate(.14f*scale,.20f*scale,.56f,.93f,deep);
        for (float side : {-1.0f,1.0f})
        {
            deckVent(side*.24f*scale,rear-.10f*depth,.16f,.18f,4);
            rearGrille(side*.215f*scale,.34f,.25f,4);
            DrawCubeV({side*.29f*scale,.21f,rear+.030f},{.065f,.037f,.045f},steel);
        }
    }
}

inline void drawCartoon(const Design &d, Color armor, Color identity, bool moving)
{
    const auto &s = d.shape;
    const Color paint = tagged(armor);
    const Color ink = tagged({25, 33, 31, 255}, 15);
    const Color deep = tagged(blend(armor, {27, 43, 42, 255}, .60f));
    const Color light = tagged(blend(armor, {231, 224, 183, 255}, .38f));
    const Color metal = tagged({119, 129, 120, 255}, 15);
    const Color mark = tagged(identity);
    const auto hull = hullSections(d);
    const auto cabin = cabinSections(d);
    const float roof = cabin.back().y;
    const float outer = s.trackCenter + s.trackWidth * .5f;
    for (float side : {-1.0f, 1.0f})
    {
        drawRunningGear(d, side, paint, deep, moving);
        curvedFender(d, side, paint, light, ink);
    }
    cartoonCasting(hull, paint, d.family,
                   {d.chassisWidth / d.chassisScale, d.chassisLength / d.chassisScale}, armorPlan(d.cartoon,false),d.cartoon);
    chassisDetails(d,hull,paint,deep,light,ink);
    const float raceHalf = s.cabinWidth * .5f;
    casting(std::array<Section, 2>{{
        {hull.back().y - .045f, raceHalf * .58f, cabin.front().front + .10f,
         cabin.front().rear - .10f, raceHalf * .19f},
        {d.base - .005f, raceHalf * .70f, cabin.front().front + .065f,
         cabin.front().rear - .065f, raceHalf * .27f}}}, tone(paint, .90f));
    casting(std::array<Section, 2>{{
        {d.base - .008f, raceHalf * .71f, cabin.front().front + .06f,
         cabin.front().rear - .06f, raceHalf * .27f},
        {d.base + .010f, raceHalf * .72f, cabin.front().front + .055f,
         cabin.front().rear - .055f, raceHalf * .27f}}}, ink);
    cartoonCasting(cabin, paint, d.family, {1,1}, armorPlan(d.cartoon,true),d.cartoon);

    const float hullBase = d.hullTop;
    // Small protected lamps leave the glacis silhouette readable. Modern
    // tanks use flush rectangular optics instead of raised face-like eyes.
    const bool insetLamps = d.tier >= 2 || d.family == Family::German;
    for (float side : {-1.0f, 1.0f})
    {
        if (d.family == Family::Soviet && side > 0) continue;
        const float lampX = side * hull[3].halfWidth * .68f;
        if (insetLamps)
        {
            DrawCubeV({lampX, hullBase + .008f, hull[3].front - .029f}, {.062f,.038f,.036f}, ink);
            DrawCubeV({lampX, hullBase + .008f, hull[3].front - .050f}, {.040f,.022f,.008f},
                      tagged({247,238,182,255}));
        }
        else
        {
            DrawCylinderEx({lampX, hullBase - .005f, hull[3].front - .016f},
                           {lampX, hullBase + .024f, hull[3].front - .032f},
                           .010f, .010f, 8, ink);
            headlight({lampX, hullBase + .024f, hull[3].front - .032f},
                      (.050f + d.tier * .003f) * .65f, light);
        }
    }
    rlBegin(RL_TRIANGLES);
    // The identity stripe is bounded by dark paint on white and green armor.
    // The commander's seat is a vehicle feature, not a faction convention.
    // The early T-34 instead carries one shared roof hatch and a mark on its lid.
    const bool commanderRight = d.family == Family::American ||
        d.cartoon == CartoonModel::Leopard1 || d.cartoon == CartoonModel::Leopard2 ||
        d.cartoon == CartoonModel::T90;
    const float commanderSide = commanderRight ? 1.0f : -1.0f;
    const bool earlyT34 = d.cartoon == CartoonModel::T34;
    const bool lowT62 = d.cartoon == CartoonModel::T62;
    const float roofCenter = (cabin.back().front + cabin.back().rear) * .5f;
    float stripeX = earlyT34 ? .035f : lowT62 ? -commanderSide*s.cabinWidth*.11f-.036f : commanderRight ?
        -cabin.back().halfWidth * .35f - .072f : cabin.back().halfWidth * .35f;
    const float stripeFront = earlyT34 ? roofCenter - .070f : lowT62 ? roofCenter-.0925f : cabin.back().front + .018f;
    const float stripeRear = earlyT34 ? roofCenter + .130f : lowT62 ? roofCenter+.0925f : cabin.back().rear - .16f;
    const float stripeY = roof + (earlyT34 ? .024f : .004f);
    if (!earlyT34)
    {
        const auto plan=armorPlan(d.cartoon,true);
        const float available=std::min(armorHalfWidth(cabin.back(),plan,stripeFront,&cabin[1]),
                                       armorHalfWidth(cabin.back(),plan,stripeRear,&cabin[1]));
        const float limit=std::max(0.0f,available-.0495f);
        stripeX=std::clamp(stripeX+.0365f,-limit,limit)-.0365f;
    }
    quad({stripeX - .009f, stripeY, stripeFront},
         {stripeX - .009f, stripeY, stripeRear},
         {stripeX + .082f, stripeY, stripeRear},
         {stripeX + .082f, stripeY, stripeFront}, ink);
    quad({stripeX, stripeY + .001f, stripeFront}, {stripeX, stripeY + .001f, stripeRear},
         {stripeX + .072f, stripeY + .001f, stripeRear},
         {stripeX + .072f, stripeY + .001f, stripeFront}, mark);
    rlEnd();

    // Seat the mantlet on the actual casting at the immutable gun height.
    // Interpolation also keeps the shorter German enemy-slot axes connected.
    float face = cabin.back().front;
    for (std::size_t i = 0; i + 1 < cabin.size(); ++i)
    {
        if (d.muzzleY >= cabin[i].y && d.muzzleY <= cabin[i + 1].y)
        {
            const float t = (d.muzzleY - cabin[i].y) / (cabin[i + 1].y - cabin[i].y);
            face = cabin[i].front + (cabin[i + 1].front - cabin[i].front) * t;
            break;
        }
    }
    const float gunRoot = face + .025f;
    rlPushMatrix();
    rlTranslatef(0, d.muzzleY, face - .015f);
    if (d.cartoon == CartoonModel::Panther || d.cartoon == CartoonModel::Leopard1)
        // Rounded transverse mantlet survives both neutral-color and side views.
        fittedCasting(std::array<Section, 5>{{
            {-.095f, .150f, -.018f, .085f, .02f},
            {-.06f, .185f, -.056f, .10f, .035f},
            {0, .195f, -.070f, .105f, .045f},
            {.06f, .185f, -.056f, .10f, .035f},
            {.095f, .150f, -.018f, .085f, .02f}}}, {1.0f, .65f, .65f}, deep);
    else if (d.cartoon == CartoonModel::TigerII)
    {
        // Production Tiger II has a thick sloped front plate and a compact
        // circular gun collar. A second wide rectangular plate obscures it.
        DrawCylinderEx({0, 0, .049f}, {0, 0, -.039f}, .075f, .055f, 12, deep);
    }
    else if (d.cartoon == CartoonModel::Leopard2 || d.cartoon == CartoonModel::Abrams)
        fittedCasting(std::array<Section, 4>{{
            {-.080f, .155f, -.022f, .080f, .012f},
            {-.065f, .175f, -.040f, .095f, .014f},
            {.065f, .175f, -.040f, .095f, .014f},
            {.080f, .155f, -.022f, .080f, .012f}}}, {1.0f, .65f, .65f}, deep);
    else if (d.cartoon == CartoonModel::Stalin)
        fittedCasting(std::array<Section, 5>{{
            {-.102f, .115f, .006f, .095f, .043f},
            {-.068f, .185f, -.037f, .108f, .060f},
            {0, .198f, -.066f, .113f, .072f},
            {.062f, .180f, -.052f, .105f, .060f},
            {.096f, .122f, -.006f, .088f, .041f}}}, {1.0f, .65f, .65f}, deep);
    else if (d.family == Family::Soviet)
        fittedCasting(std::array<Section, 4>{{
            {-.080f, .095f, -.020f, .075f, .040f},
            {-.045f, .125f, -.052f, .085f, .060f},
            {.045f, .12f, -.045f, .085f, .055f},
            {.075f, .085f, -.015f, .080f, .038f}}}, {1.0f, .65f, .65f}, deep);
    else if (d.cartoon == CartoonModel::Pershing)
    {
        // The Pershing's broad curved gun shield spans most of the front
        // casting; it should not read as an enlarged Sherman's narrow mount.
        const float half = s.cabinWidth * .32f;
        fittedCasting(std::array<Section, 3>{{
            {-.075f, half * .88f, .015f, .12f, .045f},
            {0, half, -.060f, .13f, .060f},
            {.055f, half * .93f, -.040f, .10f, .050f}}}, {1.0f, .65f, .65f}, deep);
    }
    else
    {
        fittedCasting(std::array<Section, 4>{{
            {-.095f, .12f, -.025f, .075f, .035f},
            {-.052f, .15f, -.055f, .085f, .055f},
            {.055f, .15f, -.055f, .085f, .055f},
            {.090f, .11f, -.020f, .075f, .030f}}}, {1.0f, .65f, .65f}, deep);
    }
    rlPopMatrix();
    cannon(gunRoot, d.muzzleZ, d.muzzleY, s.gunRadius, paint, d.brake, 0, true);

    if (d.cartoon == CartoonModel::Patton || d.cartoon == CartoonModel::Abrams ||
        d.cartoon == CartoonModel::T62 || d.cartoon == CartoonModel::T90 ||
        d.cartoon == CartoonModel::Leopard1 || d.cartoon == CartoonModel::Leopard2)
    {
        // The bore evacuator follows the existing barrel span. It never
        // extends the muzzle or moves the simulated projectile origin.
        const float span = gunRoot - d.muzzleZ;
        const float station = d.cartoon == CartoonModel::T62 ? .65f : .53f;
        const float center = gunRoot - span * station;
        const float half = span * .12f;
        const float radius = s.gunRadius * (d.cartoon == CartoonModel::T62 ? 1.47f : 1.35f);
        DrawCylinderEx({0, d.muzzleY, center + half}, {0, d.muzzleY, center - half},
                       radius, radius * .95f, 12, tone(paint, .92f));
    }

    const float hatchZ = (cabin.back().front + cabin.back().rear) * .5f;
    const float cupola = d.cartoon == CartoonModel::Patton ? .12f :
                         d.cartoon == CartoonModel::Abrams ? .035f :
                         d.family == Family::American ? .035f :
                         d.family == Family::German ? .050f : .025f;
    if (earlyT34)
    {
        // The selected early L-11 two-man turret has one broad shared lid,
        // not the later paired hatches or a raised commander's cupola.
        rlPushMatrix();
        rlTranslatef(0, roof, hatchZ + .025f);
        casting(std::array<Section, 2>{{
            {.004f,.149f,-.125f,.135f,.031f},
            {.020f,.140f,-.112f,.122f,.025f}}}, tone(paint,.94f));
        rlPopMatrix();
        DrawCubeV({0,roof+.028f,hatchZ+.140f},{.16f,.013f,.020f},metal);
        DrawCubeV({-.055f,roof+.028f,cabin.back().front+.058f},{.045f,.022f,.037f},ink);
    }
    else
    {
        rlPushMatrix();
        rlTranslatef(commanderSide * s.cabinWidth * (lowT62 ? .11f : .16f), roof,
                     hatchZ + (lowT62 ? .025f : .018f));
        fittedCasting(std::array<Section, 4>{{
            {0, .105f, -.095f, .105f, .05f},
            {.026f, .115f, -.105f, .11f, .05f},
            {cupola, .102f, -.09f, .09f, .05f},
            {cupola + .018f, .080f, -.071f, .073f, .04f}}}, {.65f, .75f, .65f}, light);
        rlPopMatrix();

        // A shallow second crew hatch, hinge and periscope articulate the
        // roof without restoring the earlier model's large stacked cupolas.
        const float loaderX = lowT62 ? 0.0f : -commanderSide * cabin.back().halfWidth * .45f;
        const float loaderZ = cabin.back().rear - (lowT62 ? .073f : .10f);
        rlPushMatrix();
        rlTranslatef(loaderX, roof, loaderZ);
        const float hatchWidth=lowT62 ? .044f : .052f;
        const float hatchLength=lowT62 ? .040f : .047f;
        casting(std::array<Section, 2>{{
            {.004f,hatchWidth,-hatchLength,hatchLength,.019f},
            {.014f,hatchWidth*.90f,-hatchLength*.90f,hatchLength*.90f,.017f}}}, tone(paint,.93f));
        rlPopMatrix();
        DrawCubeV({loaderX,roof+.018f,loaderZ+.040f},{.071f,.012f,.013f},metal);
        DrawCubeV({commanderSide*s.cabinWidth*(lowT62 ? .11f : .16f),roof+cupola*.75f+.018f,hatchZ-.047f},
                  {.044f,.017f,.025f},ink);
    }

    if (d.family == Family::American && d.cartoon != CartoonModel::Abrams)
    {
        // A broad overhanging rear bustle, not a tiny accessory. The collar
        // sits entirely inside the existing maximum turret footprint.
        const float back = s.cabinZ + s.cabinLength * .5f;
        const float w = s.cabinWidth * .5f;
        casting(std::array<Section, 4>{{
            {d.base + s.cabinHeight * .24f, w * .54f, back - s.cabinLength * .31f, back - .022f, w * .26f},
            {d.base + s.cabinHeight * .37f, w * .75f, back - s.cabinLength * .35f, back, w * .29f},
            {d.base + s.cabinHeight * .70f, w * .73f, back - s.cabinLength * .31f, back - .009f, w * .27f},
            {d.base + s.cabinHeight * .78f, w * .63f, back - s.cabinLength * .27f, back - .023f, w * .22f}}}, paint);
    }

    if (d.cartoon == CartoonModel::Patton)
    {
        // M60A3's right rangefinder/laser-sight housing and high M19 cupola
        // are retained; the earlier M60A1's large searchlight is not reused.
        const float opticY=d.base+s.cabinHeight*.66f;
        const auto at=armorAtHeight(cabin,opticY);
        const auto plan=armorPlan(d.cartoon,true);
        const float opticZ=at.front+(at.rear-at.front)*plan.frontCutZ+.018f;
        const float opticX=armorHalfWidth(at,plan,opticZ)-.004f;
        DrawCubeV({opticX,opticY,opticZ}, {.074f,.060f,.12f},deep);
        DrawCubeV({opticX+.039f,opticY,opticZ}, {.010f,.032f,.050f},ink);
        DrawCubeV({commanderSide * s.cabinWidth * .16f, roof + cupola * .54f,
                   hatchZ - .070f}, {.042f, .039f, .10f}, deep);
        DrawCylinderEx({commanderSide * s.cabinWidth * .16f, roof + cupola * .54f, hatchZ - .08f},
                       {commanderSide * s.cabinWidth * .16f, roof + cupola * .54f, hatchZ - .18f},
                       .010f, .008f, 6, metal);
    }
    if (d.cartoon == CartoonModel::Leopard1)
    {
        // The early cast-turret Leopard carries its rectangular searchlight
        // above the gun mantlet, on the centreline rather than on one cheek.
        const float lampY = d.muzzleY + .079f;
        DrawCubeV({0, lampY, face + .003f}, {.108f, .077f, .073f}, deep);
        DrawCubeV({0, lampY, face - .037f}, {.077f, .049f, .010f},
                  tagged({192, 196, 157, 255}, 17));
    }
    if (d.cartoon == CartoonModel::Abrams)
    {
        // Open bustle basket rails leave the rear armor visible through the
        // rack. The former five pale bars falsely resembled engine exhaust.
        const float back = cabin[2].rear + .037f;
        const float y0 = d.base + s.cabinHeight * .48f;
        for (float y : {y0, y0 + .088f})
            DrawCubeV({0, y, back}, {s.cabinWidth * .76f, .016f, .018f}, metal);
        for (float side : {-1.0f, 1.0f})
        {
            DrawCubeV({side * s.cabinWidth * .37f, y0 + .043f, back},
                      {.017f, .10f, .018f}, metal);
            DrawCubeV({side * s.cabinWidth * .37f, y0, back - .045f},
                      {.017f, .016f, .105f}, metal);
        }
        DrawCubeV({s.cabinWidth * .21f, roof + .029f, cabin.back().front + .070f},
                  {.086f, .058f, .079f}, deep);
        DrawCubeV({s.cabinWidth * .21f, roof + .029f, cabin.back().front + .028f},
                  {.055f, .027f, .010f}, ink);
    }
    if (d.cartoon == CartoonModel::Leopard2)
    {
        // A4 retains upright cheeks and its conspicuous offset EMES sight;
        // no later Leopard 2A5 arrowhead add-on armor is applied.
        DrawCubeV({s.cabinWidth * .30f, roof - .034f, face - .009f},
                  {.105f, .096f, .052f}, deep);
        DrawCubeV({s.cabinWidth * .30f, roof - .022f, face - .038f},
                  {.069f, .047f, .009f}, ink);
        for (float side : {-1.0f, 1.0f})
            DrawCubeV({side * s.cabinWidth * .19f, d.base + s.cabinHeight * .44f,
                       cabin[2].rear + .015f}, {s.cabinWidth * .35f, .095f, .060f}, deep);
    }
    if (d.cartoon == CartoonModel::T90)
    {
        // Two large reactive-armor banks and a sensor pair are readable as
        // designed masses, rather than a carpet of tiny identical cubes.
        for (float side : {-1.0f, 1.0f})
        {
            const auto plan=armorPlan(d.cartoon,true);
            const auto lower=armorSection(cabin[1],plan,&cabin[1]);
            const auto upper=armorSection(cabin[3],plan,&cabin[1]);
            const auto mix=[](Vector3 a,Vector3 b,float t) {
                return Vector3{a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t,a.z+(b.z-a.z)*t};
            };
            const auto cheek=[&](float across,float up) {
                auto p=mix(mix(lower[1],lower[3],across),mix(upper[1],upper[3],across),up);
                p.x*=side;
                return p;
            };
            const auto a=cheek(.18f,.13f), b=cheek(.87f,.13f);
            const auto c=cheek(.87f,.78f), e=cheek(.18f,.78f);
            const auto n=side>0 ? mesh::normal(b,a,e) : mesh::normal(a,b,c);
            const auto raised=[&](Vector3 p) {
                return Vector3{p.x+n.x*.030f,p.y+n.y*.030f,p.z+n.z*.030f};
            };
            const auto ar=raised(a), br=raised(b), cr=raised(c), er=raised(e);
            rlBegin(RL_TRIANGLES);
            if (side>0)
            {
                quad(br,ar,er,cr,light);quad(a,b,c,e,deep);
                quad(a,ar,br,b,deep);quad(b,br,cr,c,deep);
                quad(c,cr,er,e,deep);quad(e,er,ar,a,deep);
            }
            else
            {
                quad(ar,br,cr,er,light);quad(b,a,e,c,deep);
                quad(ar,a,b,br,deep);quad(br,b,c,cr,deep);
                quad(cr,c,e,er,deep);quad(er,e,a,ar,deep);
            }
            rlEnd();
            // Paired Shtora housings sit low beside the gun. Use the subdued
            // mechanical response locally: the legacy optic shader adds cyan
            // emission, which is inappropriate for these dark red filters.
            const float eyeX = side * s.cabinWidth * .22f;
            DrawCubeV({eyeX, d.muzzleY - .015f, face - .035f},
                      {.083f, .068f, .060f}, deep);
            DrawCubeV({eyeX, d.muzzleY - .015f, face - .068f},
                      {.055f, .045f, .008f}, tagged({124, 34, 23, 255}, 15));
        }
    }
    if (d.family == Family::German && d.skirts)
    {
        // Four separate armor plates form a continuous broad flank. The
        // notched lower edges and gaps survive a neutral-color render.
        for (float side : {-1.0f, 1.0f})
        for (int panel = 0; panel < 4; ++panel)
        {
            const float z = s.trackLength * (-.285f + panel * .19f);
            const float half = s.trackLength * .09f;
            rlPushMatrix();
            rlTranslatef(side * (outer - .012f), 0, z);
            const bool rubberRear=d.cartoon == CartoonModel::Leopard2 && panel>=2;
            const float thickness=d.cartoon == CartoonModel::Leopard2 && panel<2 ? .036f : .020f;
            casting(std::array<Section, 3>{{
                {s.trackHeight * (d.cartoon == CartoonModel::Leopard2 ? (rubberRear ? .70f : .60f) : .81f) + (panel % 2) * .012f, thickness*.75f, -half + .022f, half - .014f, .010f},
                {s.trackHeight + .045f, thickness, -half, half, .014f},
                {s.trackHeight + .062f, thickness*.7f, -half + .012f, half - .012f, .009f}}},
                rubberRear ? tagged({44,49,48,255},16) : paint);
            rlPopMatrix();
        }
    }
    else if (d.skirts)
    {
        const int panels=d.cartoon == CartoonModel::Abrams ? 5 : 4;
        for (float side : {-1.0f, 1.0f})
        for (int panel=0;panel<panels;++panel)
        {
            const float span=s.trackLength*.72f/panels;
            const float z=-s.trackLength*.36f+span*(panel+.5f);
            const float bottom=s.trackHeight*(d.cartoon == CartoonModel::Abrams ? .53f:.65f);
            if (d.cartoon == CartoonModel::Abrams && panel == 0)
            {
                // The leading side plate sweeps inward with the front fender;
                // Leopard 2 keeps its straight armored shelf and skirts.
                const float leading = side * (s.trackCenter - .020f * d.chassisWidth);
                const float trailing = side * (outer - .005f);
                const float front = z - span * .475f, rear = z + span * .475f;
                const float top = s.trackHeight + .041f;
                const Vector3 a{leading-.016f,bottom,front}, b{leading+.016f,bottom,front};
                const Vector3 c{leading+.016f,top,front}, e{leading-.016f,top,front};
                const Vector3 f{trailing-.016f,bottom,rear}, g{trailing+.016f,bottom,rear};
                const Vector3 h{trailing+.016f,top,rear}, j{trailing-.016f,top,rear};
                rlBegin(RL_TRIANGLES);
                quad(a,e,c,b,paint); quad(f,g,h,j,paint);
                quad(b,c,h,g,paint); quad(a,f,j,e,paint);
                quad(a,b,g,f,paint); quad(e,j,h,c,paint);
                rlEnd();
            }
            else
            {
                DrawCubeV({side*(outer-.005f),(bottom+s.trackHeight+.041f)*.5f,z},
                          {.032f,s.trackHeight+.041f-bottom,span*.95f},paint);
            }
        }
    }
    for (float side : {-1.0f, 1.0f})
    {
        const float x = side * (outer + .020f), y = s.trackHeight + .004f;
        DrawCubeV({x, y, .16f * d.chassisLength / d.chassisScale}, {.023f, .083f, .165f}, ink);
        DrawCubeV({x + side * .013f, y, .16f * d.chassisLength / d.chassisScale}, {.009f, .056f, .125f}, mark);
    }
    // Only the selected IS-2 carries these compact side tanks. Do not put
    // the same optional equipment on the early T-34 and both later hulls.
    if (d.cartoon == CartoonModel::Stalin)
    {
        for (float side : {-1.0f,1.0f})
        {
            const float x=side*s.trackCenter*.81f;
            const float z0=s.trackLength*.26f, z1=s.trackLength*.455f;
            DrawCylinderEx({x,hullBase+.054f,z0},{x,hullBase+.054f,z1},.062f,.062f,10,deep);
            DrawCylinderEx({x,hullBase+.054f,z1},{x,hullBase+.054f,z1+.012f},.050f,.050f,10,metal);
        }
    }
    DrawCylinderEx({-s.cabinWidth * .28f, roof - .06f, cabin[4].rear - .03f},
                   {-s.cabinWidth * .28f - .014f, roof + .20f, cabin[4].rear - .007f},
                   .007f, .004f, 5, metal);
}

inline void draw(const Design &d, Color armor, Color identity, bool moving)
{
    if (d.cartoon != CartoonModel::None)
    {
        drawCartoon(d, armor, identity, moving);
        return;
    }
    const auto &s = d.shape;
    // The caller resolves national armor independently of player/status
    // markings. Rubber, exposed steel and optics keep their own materials.
    const Color paint = tagged(armor);
    const Color deep = tagged(blend(tone(paint, .48f), {39, 62, 65, 255}, .38f));
    const bool snow = d.family == Family::German;
    const Color light = tagged(blend(paint, snow ? Color{241, 241, 232, 255}
                                                : Color{217, 210, 161, 255}, .37f));
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
    armorCasting(hullSections(d), paint, snow);

    const auto cabin = cabinSections(d);
    const float roof = cabin.back().y;
    rlPushMatrix();
    rlTranslatef(d.cabinX, 0, 0);
    const float w = s.cabinWidth * .5f;
    casting(std::array<Section, 3>{{
        {d.base - .055f, w * .72f, cabin[0].front + .025f, cabin[0].rear - .025f, .09f},
        {d.base, w * .86f, cabin[0].front, cabin[0].rear, .11f},
        {d.base + .035f, w * .84f, cabin[0].front, cabin[0].rear, .11f}}}, deep);
    armorCasting(cabin, paint, snow, {d.cabinX, 0, 0});
    // A brow and visor fitted to one sloping shoulder panel. The upper surface
    // is sampled from the actual section, so variants cannot bury the window.
    const int visorSection = d.family == Family::Soviet ? 3 : 2;
    const float visorX = d.family == Family::Soviet ? w * .10f : 0.0f;
    const float visorWidth = w * (d.family == Family::American ? .38f : .23f);
    const auto front = [&](float t) {
        const auto &lower = cabin[visorSection], &upper = cabin[visorSection + 1];
        return Vector3{visorX, lower.y + (upper.y - lower.y) * t,
            lower.front + (upper.front - lower.front) * t - .004f};
    };
    const auto band = [&](float t0, float t1, float width, float relief, Color c) {
        const auto a = front(t0), b = front(t1);
        quad({a.x - width, a.y, a.z - relief}, {b.x - width, b.y, b.z - relief},
             {b.x + width, b.y, b.z - relief}, {a.x + width, a.y, a.z - relief}, c);
    };
    rlBegin(RL_TRIANGLES);
    band(.54f, .88f, visorWidth, 0.0f, ink);
    band(.64f, .76f, visorWidth * .86f, .004f, tagged({100, 166, 139, 255}, 17));
    band(.90f, 1.0f, visorWidth * 1.13f, .003f, light);
    const float stripeX = std::min(w * .32f,
        cabin[5].halfWidth - cabin[5].corner - .09f);
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
        const std::array<Vector3, 6> rim = d.family == Family::German
            ? std::array<Vector3, 6>{{point(1, -.11f), point(2, -.11f),
                point(3, -.11f), point(3, .11f), point(2, .11f), point(1, .11f)}}
            : std::array<Vector3, 6>{{point(1, -.085f), point(2, -.135f),
                point(3, -.08f), point(3, .08f), point(2, .135f), point(1, .085f)}};
        const float centerY = (cabin[2].y + cabin[3].y) * .5f;
        const Vector3 center{side * ((cabin[2].halfWidth + cabin[3].halfWidth) * .5f + .012f),
                             centerY, centerZ};
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
    if (d.family == Family::German)
        casting(std::array<Section, 4>{{
            {0, .11f, -.10f, .10f, .035f},
            {.035f, .115f, -.105f, .105f, .035f},
            {.14f, .11f, -.10f, .10f, .035f},
            {.16f, .095f, -.085f, .085f, .027f}}}, light);
    else
        casting(std::array<Section, 3>{{
            {0, .115f, -.105f, .105f, .045f},
            {.024f, .12f, -.11f, .11f, .045f},
            {d.family == Family::Soviet ? .05f : .065f,
             .095f, -.085f, .085f, .04f}}}, light);
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

    // The wedge retreats above the gun axis. Seat the rear collar inside its
    // shoulder so its upper edge cannot float ahead of the sloping armor.
    const float gunRoot = d.family == Family::Soviet ? cabin[3].front + .07f
                                                    : cabin[1].front + .10f;
    cannon(gunRoot, d.muzzleZ, d.muzzleY,
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
        armorCasting(std::array<Section, 4>{{
            {d.base - .27f, half * .96f, -cover + .045f, cover - .025f, .04f},
            {d.base - .11f, half, -cover, cover, .05f},
            {d.base - .01f, half * .92f, -cover + .025f, cover - .025f, .055f},
            {d.base + .015f, half * .60f, -cover + .09f, cover - .08f, .05f}}}, paint,
            snow, {side * s.trackCenter, 0, .025f});
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

inline void draw(const Design &design, Color armor, Color identity, bool moving)
{
    detail::draw(design, armor, identity, moving);
}
} // namespace arcade_tank_roster

#endif
