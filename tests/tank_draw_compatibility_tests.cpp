#include "tank_assets.h"
#include "test_support.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <map>
#include <stdexcept>
#include <vector>

namespace
{
// Record the actual production renderer's calls without opening a window or
// linking raylib. Comparing complete command streams catches incorrect wrapper
// forwarding of the nation, pose, paint, animation and protection arguments.
enum class Command : std::uint32_t
{
    PushMatrix,
    PopMatrix,
    Translate,
    Scale,
    Rotate,
    Begin,
    End,
    Color,
    Normal,
    Vertex,
    Cube,
    CubeV,
    Cylinder,
    CylinderEx,
    Sphere,
    SphereEx
};

using Stream = std::vector<std::uint32_t>;
Stream commands;
constexpr std::array<std::size_t, 16> kCommandArguments{
    0, 0, 3, 3, 4, 1, 0, 1, 3, 3, 7, 7, 8, 10, 5, 7};

void append(float value)
{
    std::uint32_t bits = 0U;
    static_assert(sizeof(bits) == sizeof(value));
    std::memcpy(&bits, &value, sizeof(bits));
    commands.push_back(bits);
}

void append(int value)
{
    commands.push_back(static_cast<std::uint32_t>(value));
}

void append(Vector3 value)
{
    append(value.x);
    append(value.y);
    append(value.z);
}

void append(Color value)
{
    commands.push_back((static_cast<std::uint32_t>(value.r) << 24U) |
                       (static_cast<std::uint32_t>(value.g) << 16U) |
                       (static_cast<std::uint32_t>(value.b) << 8U) |
                       static_cast<std::uint32_t>(value.a));
}

template <typename... Arguments>
void record(Command command, Arguments... arguments)
{
    commands.push_back(static_cast<std::uint32_t>(command));
    (append(arguments), ...);
}

template <typename Draw>
Stream capture(Draw draw)
{
    commands.clear();
    draw();
    return commands;
}

Stream geometryOnly(const Stream &stream)
{
    Stream geometry;
    int matrixDepth = 0, beginDepth = 0;
    for (std::size_t offset = 0; offset < stream.size();)
    {
        const auto command = stream[offset];
        if (command >= kCommandArguments.size() ||
            offset + kCommandArguments[command] >= stream.size())
            throw std::runtime_error("invalid render command framing");
        if (command == static_cast<unsigned>(Command::PushMatrix)) ++matrixDepth;
        if (command == static_cast<unsigned>(Command::PopMatrix)) --matrixDepth;
        if (command == static_cast<unsigned>(Command::Begin)) ++beginDepth;
        if (command == static_cast<unsigned>(Command::End)) --beginDepth;
        if (matrixDepth < 0 || beginDepth < 0 || beginDepth > 1)
            throw std::runtime_error("unbalanced render state");
        if (command != static_cast<unsigned>(Command::Color))
        {
            geometry.insert(geometry.end(), stream.begin() + offset,
                            stream.begin() + offset + 1 + kCommandArguments[command]);
            // raylib primitives embed their color as the final argument.
            if (command >= static_cast<unsigned>(Command::Cube)) geometry.back() = 0U;
        }
        offset += 1 + kCommandArguments[command];
    }
    if (matrixDepth != 0 || beginDepth != 0)
        throw std::runtime_error("leaked render state");
    return geometry;
}

Stream materialColors(const Stream &stream, unsigned char firstTag,
                      unsigned char lastTag)
{
    Stream colors;
    for (std::size_t offset = 0; offset < stream.size();)
    {
        const auto command = stream[offset];
        if (command >= kCommandArguments.size() ||
            offset + kCommandArguments[command] >= stream.size())
            throw std::runtime_error("invalid render command framing");
        if (command == static_cast<unsigned>(Command::Color) ||
            command >= static_cast<unsigned>(Command::Cube))
        {
            const auto color = stream[offset + kCommandArguments[command]];
            const auto tag = color & 255U;
            if (tag >= firstTag && tag <= lastTag)
                colors.push_back(color);
        }
        offset += 1 + kCommandArguments[command];
    }
    return colors;
}

float unpack(std::uint32_t bits);

std::map<std::uint32_t, double> armorColorAreas(const Stream &stream)
{
    std::map<std::uint32_t, double> areas;
    std::array<Vector3, 3> triangle{};
    std::size_t vertex = 0;
    std::uint32_t color = 0;
    for (std::size_t offset = 0; offset < stream.size();)
    {
        const auto command = static_cast<Command>(stream[offset]);
        if (command == Command::Color) color = stream[offset + 1];
        if (command == Command::Begin) vertex = 0;
        if (command == Command::Vertex)
        {
            triangle[vertex++] = {unpack(stream[offset + 1]), unpack(stream[offset + 2]), unpack(stream[offset + 3])};
            if (vertex == 3)
            {
                vertex = 0;
                if ((color & 255U) == 14U)
                {
                    const Vector3 a{triangle[1].x - triangle[0].x, triangle[1].y - triangle[0].y,
                                    triangle[1].z - triangle[0].z};
                    const Vector3 b{triangle[2].x - triangle[0].x, triangle[2].y - triangle[0].y,
                                    triangle[2].z - triangle[0].z};
                    const Vector3 n{a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
                    areas[color] += std::sqrt(n.x * n.x + n.y * n.y + n.z * n.z) * .5;
                }
            }
        }
        offset += 1 + kCommandArguments[static_cast<std::size_t>(command)];
    }
    return areas;
}

bool nationalArmorDominates(const Stream &stream, tanks3d::core::Nation nation)
{
    const auto colors = armorColorAreas(stream);
    double national = 0.0, total = 0.0;
    for (const auto &entry : colors)
    {
        const auto color = entry.first;
        const double area = entry.second;
        total += area;
        const int r = static_cast<int>((color >> 24U) & 255U);
        const int g = static_cast<int>((color >> 16U) & 255U);
        const int b = static_cast<int>((color >> 8U) & 255U);
        switch (nation)
        {
        case tanks3d::core::Nation::UnitedStates:
            if (g > r + 5 && r > b + 10) national += area;
            break;
        case tanks3d::core::Nation::SovietUnion:
            if (std::min({r, g, b}) > 150 && std::max({r, g, b}) - std::min({r, g, b}) < 40)
                national += area;
            break;
        case tanks3d::core::Nation::Germany:
            if (std::max({r, g, b}) < 150 && std::max({r, g, b}) - std::min({r, g, b}) <= 30)
                national += area;
            break;
        case tanks3d::core::Nation::Count:
            break;
        }
    }
    return total > .2 && national > total * .5;
}

bool hasNationalCamouflage(const Stream &stream, tanks3d::core::Nation nation)
{
    const auto colors = armorColorAreas(stream);
    double total = 0;
    for (const auto &entry : colors) total += entry.second;
    const auto area = [&](std::uint32_t color) {
        const auto found = colors.find(color);
        return found == colors.end() ? 0.0 : found->second;
    };
    // Area-weighted real triangles prevent many tiny paint fragments from
    // satisfying the broad patch requirement.
    if (nation == tanks3d::core::Nation::SovietUnion)
        return area(0xb8bcaf0eU) > total * .45 && area(0x4755410eU) > total * .05;
    if (nation == tanks3d::core::Nation::UnitedStates)
        return area(0x594b370eU) > total * .03 && area(0x292e290eU) > total * .03;
    return false;
}

Stream withoutPlayerMarks(Stream colors)
{
    colors.erase(std::remove_if(colors.begin(), colors.end(), [](std::uint32_t color) {
        return color == 0xffdb4e0eU || color == 0x5bffac0eU;
    }), colors.end());
    return colors;
}

// Project the actual submitted upper armor, independently of the profile
// generator. A common horizontal cut excludes the running-gear covers so that
// changing belt width alone cannot establish upper-armor national identity.
using Silhouette = std::array<bool, 128 * 128>;

float unpack(std::uint32_t bits)
{
    float value;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

Vector3 emittedSize(const Stream &stream)
{
    Vector3 low{}, high{};
    bool first = true;
    for (std::size_t offset = 0; offset < stream.size();)
    {
        const auto command = static_cast<Command>(stream[offset]);
        if (command == Command::Vertex)
        {
            const Vector3 point{unpack(stream[offset + 1]), unpack(stream[offset + 2]), unpack(stream[offset + 3])};
            if (first) low = high = point;
            first = false;
            low = {std::min(low.x, point.x), std::min(low.y, point.y), std::min(low.z, point.z)};
            high = {std::max(high.x, point.x), std::max(high.y, point.y), std::max(high.z, point.z)};
        }
        offset += 1 + kCommandArguments[static_cast<std::size_t>(command)];
    }
    return {high.x - low.x, high.y - low.y, high.z - low.z};
}

bool validPaintRoof(const Stream &stream, chaffee_sample_model::detail::Section cap,
                    const std::array<std::uint32_t, 3> &palette,
                    arcade_tank_roster::detail::ArmorPlan plan = {},
                    const chaffee_sample_model::detail::Section *reference = nullptr,
                    bool refinedCast = false)
{
    std::vector<std::array<Vector3, 3>> triangles;
    std::array<Vector3, 3> face;
    std::array<Vector3, 3> normals;
    Vector3 normal{};
    std::size_t vertices = 0;
    std::uint32_t color = 0;
    double area = 0;
    const auto edge = [](Vector3 a, Vector3 b, Vector3 p) {
        return (b.x - a.x) * (p.z - a.z) - (b.z - a.z) * (p.x - a.x);
    };
    const auto original = arcade_tank_roster::detail::armorSection(cap,plan,reference);
    std::vector<Vector3> boundary(original.begin(),original.end());
    if (refinedCast)
    {
        const auto curved=arcade_tank_roster::detail::castTurretSection(cap,plan);
        boundary.assign(curved.begin(),curved.end());
    }
    for (std::size_t offset = 0; offset < stream.size();)
    {
        const auto command = static_cast<Command>(stream[offset]);
        if (command == Command::Color) color = stream[offset + 1];
        if (command == Command::Normal)
            normal = {unpack(stream[offset + 1]), unpack(stream[offset + 2]), unpack(stream[offset + 3])};
        if (command == Command::Vertex)
        {
            face[vertices % 3] = {unpack(stream[offset + 1]), unpack(stream[offset + 2]), unpack(stream[offset + 3])};
            normals[vertices++ % 3] = normal;
            if (vertices % 3 == 0 && std::all_of(face.begin(), face.end(), [&](Vector3 p) {
                return std::abs(p.y - cap.y) < 1e-6f;
            }))
            {
                for (std::size_t i = 0; i < face.size(); ++i)
                {
                    if (normals[i].x != 0 || normals[i].y != 1 || normals[i].z != 0) return false;
                    for (std::size_t j = 0; j < boundary.size(); ++j)
                        if (edge(boundary[j], boundary[(j + 1) % boundary.size()], face[i]) < -1e-6f)
                            return false;
                }
                const double faceArea = -edge(face[0], face[1], face[2]) * .5;
                if (faceArea <= 0) return false;
                if (std::find(palette.begin(), palette.end(), color) == palette.end()) return false;
                area += faceArea;
                triangles.push_back(face);
            }
        }
        offset += 1 + kCommandArguments[static_cast<unsigned>(command)];
    }
    double expectedArea = 0;
    for (std::size_t i = 1; i + 1 < boundary.size(); ++i)
        expectedArea += edge(boundary[0], boundary[i], boundary[i + 1]) * .5;
    if (std::abs(area - expectedArea) > 1e-6)
        return false;
    // A coverage grid catches overlaps and holes that an area sum could hide.
    for (int x = 0; x < 32; ++x)
    for (int z = 0; z < 32; ++z)
    {
        const Vector3 p{-cap.halfWidth + (x + .37f) / 32 * cap.halfWidth * 2,
                        cap.y, cap.front + (z + .61f) / 32 * (cap.rear - cap.front)};
        bool inside = true;
        for (std::size_t i = 0; i < boundary.size(); ++i)
            inside = inside && edge(boundary[i], boundary[(i + 1) % boundary.size()], p) >= 0;
        int covering = 0;
        for (const auto &t : triangles)
            if (edge(t[0], t[1], p) <= 0 && edge(t[1], t[2], p) <= 0 && edge(t[2], t[0], p) <= 0)
                ++covering;
        if (covering != (inside ? 1 : 0)) return false;
    }
    return true;
}

Silhouette armorSilhouette(const Stream &stream, float heading, float elevation, float cut)
{
    constexpr float radians = 3.14159265358979323846f / 180.0f;
    const float c = std::cos(heading * radians), s = std::sin(heading * radians);
    const float cy = std::cos(elevation * radians), sy = std::sin(elevation * radians);
    struct LocalPose
    {
        Vector3 translation{};
        float yaw = 0.0f;
    };
    std::vector<LocalPose> transforms(1);
    const auto transform = [](Vector3 point, const LocalPose &pose) {
        const float c = std::cos(pose.yaw), s = std::sin(pose.yaw);
        return Vector3{point.x * c + point.z * s + pose.translation.x,
                       point.y + pose.translation.y,
                       -point.x * s + point.z * c + pose.translation.z};
    };
    std::array<Vector3, 3> triangle;
    std::size_t vertexCount = 0;
    unsigned material = 0;
    Silhouette result{};
    const auto edge = [](Vector3 a, Vector3 b, float x, float y) {
        return (b.x - a.x) * (y - a.y) - (b.y - a.y) * (x - a.x);
    };
    const auto project = [&](Vector3 p) {
        return Vector3{(p.x * c - p.z * s) * 48.0f + 64.0f,
                       (p.y * cy + (p.x * s + p.z * c) * sy) * 48.0f + 32.0f,
                       0.0f};
    };
    for (std::size_t offset = 0; offset < stream.size();)
    {
        const auto command = static_cast<Command>(stream[offset]);
        switch (command)
        {
        case Command::PushMatrix:
            transforms.push_back(transforms.back());
            break;
        case Command::PopMatrix:
            if (transforms.size() <= 1) throw std::runtime_error("silhouette matrix underflow");
            transforms.pop_back();
            break;
        case Command::Translate:
            transforms.back().translation = transform({unpack(stream[offset + 1]),
                unpack(stream[offset + 2]), unpack(stream[offset + 3])}, transforms.back());
            break;
        case Command::Rotate:
            if (unpack(stream[offset + 2]) != 0.0f || unpack(stream[offset + 3]) != 1.0f ||
                unpack(stream[offset + 4]) != 0.0f)
                throw std::runtime_error("local roster silhouette received an unsupported rotation axis");
            transforms.back().yaw += unpack(stream[offset + 1]) * radians;
            break;
        case Command::Scale:
            throw std::runtime_error("local roster silhouette received an unexpected scale");
        case Command::Color:
            material = stream[offset + 1] & 255U;
            break;
        case Command::Begin:
            vertexCount = 0;
            break;
        case Command::Vertex:
        {
            const Vector3 p = transform({unpack(stream[offset + 1]), unpack(stream[offset + 2]),
                                         unpack(stream[offset + 3])}, transforms.back());
            triangle[vertexCount++] = p;
            if (vertexCount != 3) break;
            vertexCount = 0;
            if (material != 14U) break;
            std::array<Vector3, 4> clipped;
            std::size_t clippedCount = 0;
            for (std::size_t i = 0; i < triangle.size(); ++i)
            {
                const auto from = triangle[i], to = triangle[(i + 1) % triangle.size()];
                if (from.y >= cut) clipped[clippedCount++] = from;
                if ((from.y >= cut) != (to.y >= cut))
                {
                    const float t = (cut - from.y) / (to.y - from.y);
                    clipped[clippedCount++] = {from.x + (to.x - from.x) * t, cut,
                                               from.z + (to.z - from.z) * t};
                }
            }
            for (std::size_t i = 1; i + 1 < clippedCount; ++i)
            {
                const auto a = project(clipped[0]), b = project(clipped[i]), d = project(clipped[i + 1]);
                const float area = edge(a, b, d.x, d.y);
                if (std::abs(area) < .00001f) continue;
                const int x0 = std::max(0, static_cast<int>(std::floor(std::min({a.x, b.x, d.x}))));
                const int x1 = std::min(127, static_cast<int>(std::ceil(std::max({a.x, b.x, d.x}))));
                const int y0 = std::max(0, static_cast<int>(std::floor(std::min({a.y, b.y, d.y}))));
                const int y1 = std::min(127, static_cast<int>(std::ceil(std::max({a.y, b.y, d.y}))));
                for (int y = y0; y <= y1; ++y)
                for (int x = x0; x <= x1; ++x)
                {
                    const float ab = edge(a, b, x + .5f, y + .5f);
                    const float bd = edge(b, d, x + .5f, y + .5f);
                    const float da = edge(d, a, x + .5f, y + .5f);
                    if ((ab >= 0 && bd >= 0 && da >= 0) || (ab <= 0 && bd <= 0 && da <= 0))
                        result[static_cast<std::size_t>(y) * 128 + x] = true;
                }
            }
            break;
        }
        default:
            break;
        }
        offset += 1 + kCommandArguments[static_cast<std::size_t>(command)];
    }
    if (transforms.size() != 1) throw std::runtime_error("silhouette matrix leak");
    return result;
}

float silhouetteDifference(const Silhouette &a, const Silhouette &b)
{
    std::size_t intersection = 0, combined = 0;
    for (std::size_t i = 0; i < a.size(); ++i)
    {
        intersection += a[i] && b[i];
        combined += a[i] || b[i];
    }
    return combined == 0 ? 0.0f : 1.0f - static_cast<float>(intersection) / combined;
}
} // namespace

void rlPushMatrix()
{
    record(Command::PushMatrix);
}
void rlPopMatrix()
{
    record(Command::PopMatrix);
}
void rlTranslatef(float x, float y, float z)
{
    record(Command::Translate, x, y, z);
}
void rlScalef(float x, float y, float z)
{
    record(Command::Scale, x, y, z);
}
void rlRotatef(float angle, float x, float y, float z)
{
    record(Command::Rotate, angle, x, y, z);
}
void rlBegin(int mode)
{
    record(Command::Begin, mode);
}
void rlEnd()
{
    record(Command::End);
}
void rlColor4ub(unsigned char r, unsigned char g, unsigned char b,
                unsigned char a)
{
    record(Command::Color, Color{r, g, b, a});
}
void rlNormal3f(float x, float y, float z)
{
    record(Command::Normal, x, y, z);
}
void rlVertex3f(float x, float y, float z)
{
    record(Command::Vertex, x, y, z);
}
void DrawCube(Vector3 position, float width, float height, float length,
              Color color)
{
    record(Command::Cube, position, width, height, length, color);
}
void DrawCubeV(Vector3 position, Vector3 size, Color color)
{
    record(Command::CubeV, position, size, color);
}
void DrawCylinder(Vector3 position, float top, float bottom, float height,
                  int slices, Color color)
{
    record(Command::Cylinder, position, top, bottom, height, slices, color);
}
void DrawCylinderEx(Vector3 start, Vector3 end, float startRadius,
                    float endRadius, int sides, Color color)
{
    record(Command::CylinderEx, start, end, startRadius, endRadius, sides, color);
}
void DrawSphere(Vector3 center, float radius, Color color)
{
    record(Command::Sphere, center, radius, color);
}
void DrawSphereEx(Vector3 center, float radius, int rings, int slices, Color color)
{
    record(Command::SphereEx, center, radius, rings, slices, color);
}
double GetTime()
{
    return 12.375;
}

int main()
{
    using tanks3d::core::Nation;
    tanks3d_test::Reporter reporter;
    reporter.reset();
    bool passed = true;
    const auto expect = [&](bool condition, const char *message)
    {
        if (!reporter.check(condition, message))
            passed = false;
    };

    TankAssets assets;
    constexpr float x = 2.75f;
    constexpr float z = 7.125f;
    constexpr float yaw = 0.73f;
    constexpr Color paint{127, 51, 83, 241};

    for (bool enemy : {false, true})
    {
        reporter.beginSuite(enemy ? "legacy-enemy-draw-wrapper-compatibility"
                                  : "legacy-player-draw-wrapper-compatibility");
        for (int role = 0; role < 4; ++role)
        {
            for (bool moving : {false, true})
            {
                const int armor = enemy ? 4 - role : role;
                const int identity = enemy ? role : role % 2;
                const float shield = moving ? 0.8f : 0.0f;
                const Nation nation = enemy ? Nation::Germany
                                            : Nation::UnitedStates;
                const Stream expected = capture([&]
                {
                    wwii_tank_model::DrawTank(x, z, yaw, paint, enemy, armor,
                                              shield, identity, moving, nation);
                });
                expect(!expected.empty(), "explicit draw produced no commands");
                expect(capture([&]
                       {
                           wwii_tank_model::DrawTank(
                               x, z, yaw, paint, enemy, armor, shield,
                               identity, moving);
                       }) == expected,
                       "legacy model draw changed its national roster or arguments");
                expect(capture([&]
                       {
                           assets.draw(x, z, yaw, paint, enemy, armor, shield,
                                       identity, moving);
                       }) == expected,
                       "legacy facade draw changed its national roster or arguments");
            }
        }
    }

    reporter.beginSuite("explicit-national-enemy-draw-wrapper-compatibility");
    for (int role = 0; role < 4; ++role)
    {
        const Stream german = capture([&]
        {
            wwii_tank_model::DrawTank(x, z, yaw, paint, true, 4, 0.0f, role,
                                      true, Nation::Germany);
        });
        for (Nation nation : tanks3d::core::kSelectableNations)
        {
            const Stream expected = capture([&]
            {
                wwii_tank_model::DrawTank(x, z, yaw, paint, true, 4, 0.0f,
                                          role, true, nation);
            });
            expect(capture([&]
                   {
                       assets.draw(x, z, yaw, paint, true, 4, 0.0f, role,
                                   true, nation);
                   }) == expected,
                   "explicit facade draw lost the requested enemy nation");
            expect(capture([&]
                   {
                       assets.draw(x, z, yaw, paint, true, 4, 0.0f, role,
                                   true, nation, true);
                   }) == expected,
                   "explicit shadow draw lost the requested enemy nation");
            if (nation != Nation::Germany)
            {
                expect(expected != german,
                       "explicit opposing nation incorrectly drew the German model");
            }
        }
    }

    reporter.beginSuite("arcade-roster-visible-shadow-identity");
    std::vector<Stream> playerShapes;
    for (Nation nation : tanks3d::core::kSelectableNations)
    for (int level = 0; level < 4; ++level)
    for (bool moving : {false, true})
    {
        Stream firstPlayer;
        for (int identity : {0, 1})
        {
            const Stream visible = capture([&]
            {
                assets.draw(x, z, yaw, paint, false, level, 0.0f,
                            identity, moving, nation, false);
            });
            expect(!visible.empty(), "roster player produced no geometry");
            expect(capture([&]
                   {
                       assets.draw(x, z, yaw, paint, false, level, 0.0f,
                                   identity, moving, nation, true);
                   }) == visible,
                   "roster visible and shadow geometry or transforms diverged");
            if (identity == 0)
                firstPlayer = visible;
            else
            {
                expect(firstPlayer != visible, "roster lost the second player's identity paint");
                // The existing moving transform has an identity-dependent phase.
                if (!moving)
                    expect(geometryOnly(firstPlayer) == geometryOnly(visible),
                           "identity recoloring changed the neutral tank geometry");
            }
        }
        if (!moving) playerShapes.push_back(geometryOnly(firstPlayer));
    }

    for (std::size_t i = 0; i < playerShapes.size(); ++i)
        for (std::size_t j = i + 1; j < playerShapes.size(); ++j)
            expect(playerShapes[i] != playerShapes[j],
                   "distinct player vehicles collapsed to one geometry");

    reporter.beginSuite("national-armor-silhouettes-at-game-size");
    for (int level = 0; level < 4; ++level)
    {
        std::array<std::array<float, 3>, 3> differences{};
        for (float heading : {-45.0f, 0.0f, 45.0f})
        for (float elevation : {40.0f, 50.0f, 70.0f})
        {
            std::array<Silhouette, 3> silhouettes;
            for (Nation nation : tanks3d::core::kSelectableNations)
            {
                const auto vehicle = wwii_tank_model::playerVehicle(nation, level);
                const auto design = wwii_tank_model::detail::rosterDesign(vehicle);
                const Stream local = capture([&] {
                    arcade_tank_roster::draw(design, WHITE, WHITE, false);
                });
                silhouettes[static_cast<std::size_t>(nation)] = armorSilhouette(local, heading, elevation, design.base + .02f);
            }
            for (std::size_t a = 0; a < silhouettes.size(); ++a)
                for (std::size_t b = a + 1; b < silhouettes.size(); ++b)
                {
                    const float difference = silhouetteDifference(silhouettes[a], silhouettes[b]);
                    differences[a][b] += difference;
                    expect(difference > .04f,
                           "national upper armor collapsed to one silhouette at a camera boundary");
                }
        }
        // A modest anti-collapse guard, not an aesthetic grade. Lit faces and
        // actual game screenshots remain necessary for judging the model design.
        for (std::size_t a = 0; a < differences.size(); ++a)
            for (std::size_t b = a + 1; b < differences.size(); ++b)
                expect(differences[a][b] / 9.0f > .075f,
                       "national upper armor differs only by a small fixture across the camera range");
    }

    reporter.beginSuite("tier-chassis-and-turret-proportions");
    std::array<std::array<Vector3, 4>, 3> hullSizes{}, turretSizes{};
    std::array<std::array<float, 4>, 3> hullTops{}, turretRoofs{}, chassisAreas{};
    for (Nation nation : tanks3d::core::kSelectableNations)
    for (int tier = 0; tier < 4; ++tier)
    {
        namespace roster = arcade_tank_roster::detail;
        const auto design = wwii_tank_model::detail::rosterDesign(wwii_tank_model::playerVehicle(nation, tier));
        const auto measure = [&](const auto &sections, float &top, bool turret) {
            const Stream armor = capture([&] {
                roster::cartoonCasting(sections, roster::tagged(WHITE), design.family, {1,1},
                                       roster::armorPlan(design.cartoon,turret));
            });
            for (std::size_t offset = 0; offset < armor.size();)
            {
                const auto command = static_cast<Command>(armor[offset]);
                if (command == Command::Vertex)
                    top = std::max(top, unpack(armor[offset + 2]));
                offset += 1 + kCommandArguments[static_cast<std::size_t>(command)];
            }
            return emittedSize(armor);
        };
        const auto index = static_cast<std::size_t>(nation);
        hullSizes[index][tier] = measure(roster::hullSections(design), hullTops[index][tier], false);
        chassisAreas[index][tier] = design.shape.trackLength *
            (2 * design.shape.trackCenter + design.shape.trackWidth);
        expect(design.shape.trackLength < 2.01f,
               "model-specific chassis exceeded the visual length envelope");
        turretSizes[index][tier] = measure(roster::cabinSections(design), turretRoofs[index][tier], true);
        if (design.cartoon == arcade_tank_roster::CartoonModel::T34)
        {
            const auto turret = roster::cabinSections(design);
            float front = turret.front().front, rear = turret.front().rear;
            for (const auto &section : turret)
            {
                front = std::min(front, section.front);
                rear = std::max(rear, section.rear);
            }
            const float station = .5f + (front + rear) * .5f / design.shape.trackLength;
            expect(station > .32f && station < .42f,
                   "T-34 turret casting lost its forward seat on the hull");
            expect(std::abs(wwii_tank_model::playerMuzzleDistance(nation, tier) - .5152f) < .00001f &&
                       std::abs(wwii_tank_model::playerMuzzleHeight(nation, tier) - .75f) < .00001f,
                   "the visual T-34 gun changed its legacy attachment contract");
        }
        if (tier > 0)
        {
            expect(hullSizes[index][tier].x * hullSizes[index][tier].z >
                       hullSizes[index][tier - 1].x * hullSizes[index][tier - 1].z * 1.10f &&
                       chassisAreas[index][tier] > chassisAreas[index][tier - 1] * 1.10f,
                   "an upgrade no longer enlarges the main hull and running-gear plan area");
            expect(turretSizes[index][tier].x > turretSizes[index][tier - 1].x * 1.05f &&
                       turretSizes[index][tier].z > turretSizes[index][tier - 1].z * 1.05f,
                   "an upgrade no longer enlarges the submitted turret casting");
        }
    }
    // Compare submitted main armor, excluding cupolas, guns and accessories:
    // a tall fixture cannot substitute for the selected vehicle's hull mass.
    const auto american = static_cast<std::size_t>(Nation::UnitedStates);
    expect(hullTops[american][0] > hullTops[american][1] + .055f,
           "Sherman lost its visibly taller main hull relative to Pershing");
    expect(hullTops[american][2] > hullTops[american][1] + .035f &&
               turretRoofs[american][2] > turretRoofs[american][1] + .065f,
           "M60 lost its higher hull and turret roof relative to Pershing");
    expect(hullTops[american][3] < hullTops[american][2] - .055f &&
               turretRoofs[american][3] < turretRoofs[american][2] - .1f,
           "Abrams lost its low main hull and turret relative to M60");
    for (int tier = 0; tier < 4; ++tier)
    for (std::size_t a = 0; a < hullSizes.size(); ++a)
    for (std::size_t b = a + 1; b < hullSizes.size(); ++b)
    {
        const auto close = [](float a, float b, float fraction) {
            return std::max(a, b) <= std::min(a, b) * fraction + .0001f;
        };
        expect(close(hullSizes[a][tier].x, hullSizes[b][tier].x, 1.12f) &&
                   close(hullSizes[a][tier].z, hullSizes[b][tier].z, 1.12f) &&
                   close(chassisAreas[a][tier], chassisAreas[b][tier], 1.01f),
               "same-tier chassis lost comparable plan area or exceeded its per-axis envelope");
        // More faithful short Soviet towers and long American/German
        // bustles intentionally replace the former identical-length target.
        expect(close(turretSizes[a][tier].x, turretSizes[b][tier].x, 1.10f) &&
                   close(turretSizes[a][tier].z, turretSizes[b][tier].z, 1.16f),
               "same-tier turret width or length left the researched game envelope");
    }

    const auto aspect = [&](Nation nation, int tier) {
        const auto &hull = hullSizes[static_cast<std::size_t>(nation)][tier];
        return hull.z / hull.x;
    };
    expect(aspect(Nation::UnitedStates, 0) > aspect(Nation::UnitedStates, 1) * 1.10f,
           "Sherman lost its narrower, taller chassis relative to Pershing");
    expect(aspect(Nation::SovietUnion, 1) > aspect(Nation::SovietUnion, 3) * 1.10f,
           "IS-2 lost its longer, narrower chassis relative to T-90");
    expect(aspect(Nation::Germany, 2) > aspect(Nation::Germany, 3) * 1.08f,
           "Leopard 1 lost its slender chassis relative to Leopard 2");

    // Preserve the Abrams' swept front shelves when thicker tracks reduce
    // the visible gap between modern national hulls at steep camera pitches.
    std::array<float, 2> frontShelfWidths{}, totalShelfWidths{};
    for (std::size_t index = 0; index < 2; ++index)
    {
        namespace roster = arcade_tank_roster::detail;
        const auto nation = index == 0 ? Nation::UnitedStates : Nation::Germany;
        const auto design = wwii_tank_model::detail::rosterDesign(wwii_tank_model::playerVehicle(nation, 3));
        const auto fenders = capture([&] {
            for (float side : {-1.0f, 1.0f})
                roster::curvedFender(design, side, WHITE, WHITE, WHITE);
        });
        for (std::size_t offset = 0; offset < fenders.size();)
        {
            const auto command = static_cast<Command>(fenders[offset]);
            if (command == Command::Vertex &&
                unpack(fenders[offset + 3]) < -design.shape.trackLength * .48f)
                frontShelfWidths[index] = std::max(frontShelfWidths[index],
                    2.0f * std::abs(unpack(fenders[offset + 1])));
            offset += 1 + kCommandArguments[static_cast<std::size_t>(command)];
        }
        totalShelfWidths[index] = emittedSize(fenders).x;
    }
    expect(frontShelfWidths[0] < frontShelfWidths[1] * .65f,
           "the Abrams' swept front fenders collapsed into Leopard 2's straight shelf");
    expect(std::abs(totalShelfWidths[0] / .96f - totalShelfWidths[1] / .98f) < .0001f,
           "the front-fender distinction changed its model-specific outer chassis envelope");

    reporter.beginSuite("model-specific-chassis-and-running-gear");
    constexpr std::array<std::array<int,4>,3> expectedRoad{{
        {{6,6,6,7}},{{5,6,5,6}},{{8,9,7,7}}}};
    constexpr std::array<std::array<int,4>,3> expectedReturns{{
        {{3,5,3,2}},{{0,3,0,3}},{{0,0,4,4}}}};
    std::vector<std::vector<int>> hullPlans;
    for (Nation nation : tanks3d::core::kSelectableNations)
    for (int tier=0;tier<4;++tier)
    {
        namespace roster=arcade_tank_roster::detail;
        const auto design=wwii_tank_model::detail::rosterDesign(wwii_tank_model::playerVehicle(nation,tier));
        const auto gear=roster::runningGear(design);
        const auto n=static_cast<std::size_t>(nation);
        expect(design.wheels==expectedRoad[n][tier] && gear.returns==expectedReturns[n][tier],
               "vehicle lost its researched road-wheel or return-roller count");
        expect(gear.frontDrive==((nation==Nation::UnitedStates && tier==0) ||
                   (nation==Nation::Germany && tier<2)),
               "drive sprocket moved to the wrong end of the historical chassis");
        for (int i=1;i<design.wheels;++i)
            expect(gear.centers[i]>gear.centers[i-1],"wheel stations overlap on the same axle");
        if (nation==Nation::SovietUnion && tier==2)
        {
            const float frontGap=gear.centers[1]-gear.centers[0];
            expect(gear.centers[3]-gear.centers[2]>frontGap*1.10f &&
                       gear.centers[4]-gear.centers[3]>frontGap*1.10f,
                   "T-62 lost both enlarged rear wheel gaps");
        }
        const Stream running=capture([&] {
            for (float side : {-1.0f,1.0f})
                roster::drawRunningGear(design,side,roster::tagged(WHITE),roster::tagged(GRAY),true);
        });
        bool within=true;
        for (std::size_t offset=0;offset<running.size();)
        {
            const auto command=static_cast<Command>(running[offset]);
            if (command==Command::Vertex)
            {
                const Vector3 point{unpack(running[offset+1]),unpack(running[offset+2]),unpack(running[offset+3])};
                within=within && std::isfinite(point.x) && std::isfinite(point.y) && std::isfinite(point.z) &&
                    std::abs(point.x)<=design.shape.trackCenter+design.shape.trackWidth*.5f+.03f &&
                    point.y>=-.001f && point.y<=design.shape.trackHeight+.07f &&
                    std::abs(point.z)<=design.shape.trackLength*.5f+.015f;
            }
            offset+=1+kCommandArguments[static_cast<std::size_t>(command)];
        }
        expect(within,"submitted running gear extends beyond its belt or ground envelope");
        // Inspect the first emitted road-wheel disk after the belt. Repacking
        // the hull axes must move axle stations without enlarging or squeezing
        // the circular wheels or raising their existing ground clearance.
        const auto oneSide = capture([&] {
            roster::drawRunningGear(design, 1.0f, WHITE, GRAY, false);
        });
        int batch = -1;
        Vector3 wheelLow{100,100,100}, wheelHigh{-100,-100,-100};
        for (std::size_t offset = 0; offset < oneSide.size();)
        {
            const auto command = static_cast<Command>(oneSide[offset]);
            if (command == Command::Begin) ++batch;
            if (command == Command::Vertex && batch == 1)
            {
                wheelLow.y = std::min(wheelLow.y, unpack(oneSide[offset + 2]));
                wheelLow.z = std::min(wheelLow.z, unpack(oneSide[offset + 3]));
                wheelHigh.y = std::max(wheelHigh.y, unpack(oneSide[offset + 2]));
                wheelHigh.z = std::max(wheelHigh.z, unpack(oneSide[offset + 3]));
            }
            offset += 1 + kCommandArguments[static_cast<std::size_t>(command)];
        }
        constexpr std::array<float,4> originalTierScale{{.82f,.88f,.94f,1.0f}};
        const float diameter = 3.40f * originalTierScale[tier] * gear.radius;
        expect(std::abs(wheelHigh.y - wheelLow.y - diameter) < .00001f &&
                   std::abs(wheelHigh.z - wheelLow.z - diameter) < .00001f &&
                   std::abs(wheelLow.y - .041f * originalTierScale[tier]) < .00001f,
               "chassis proportion edit stretched a road wheel or changed its original radius/clearance");
        const float firstStation = gear.centers[0] * (design.shape.trackLength - design.shape.trackHeight);
        expect(std::abs((wheelHigh.z + wheelLow.z) * .5f - firstStation) < .00001f,
               "a road wheel no longer follows the vehicle's authored axle station");
        // Submit bare armor, then normalise actual vertex X/Z coordinates.
        // This catches reuse of the same hull at another tier scale; paints,
        // turrets, lamps, and accessory counts cannot make this guard pass.
        const Stream hull=capture([&] { roster::casting(roster::hullSections(design),WHITE); });
        std::vector<int> plan;
        for (std::size_t offset=0;offset<hull.size();)
        {
            const auto command=static_cast<Command>(hull[offset]);
            if (command==Command::Vertex)
            {
                plan.push_back(static_cast<int>(std::lround(unpack(hull[offset+1])/design.shape.trackLength*10000)));
                plan.push_back(static_cast<int>(std::lround(unpack(hull[offset+3])/design.shape.trackLength*10000)));
            }
            offset+=1+kCommandArguments[static_cast<std::size_t>(command)];
        }
        for (const auto &prior:hullPlans)
            expect(plan!=prior,"two vehicles reuse the same scale-normalised main hull geometry");
        hullPlans.push_back(std::move(plan));
        const Stream whole=capture([&] { arcade_tank_roster::draw(design,WHITE,WHITE,false); });
        const auto hullSections = roster::hullSections(design);
        const auto deck = hullSections.back();
        const auto outline = roster::armorSection(deck,roster::armorPlan(design.cartoon,false),&hullSections[1]);
        const auto fixtureSeated = [&](float x, float z, float width, float length) {
            if (design.shape.trackHeight + .037f > deck.y + .020f &&
                std::abs(x) + width * .5f >
                    design.shape.trackCenter - design.shape.trackWidth * .55f - .008f)
                return false;
            for (float sideX : {-1.0f, 1.0f})
            for (float sideZ : {-1.0f, 1.0f})
            {
                const float px = x + sideX * width * .5f;
                const float pz = z + sideZ * length * .5f;
                for (std::size_t edge = 0; edge < outline.size(); ++edge)
                {
                    const auto a = outline[edge], b = outline[(edge + 1) % outline.size()];
                    if ((b.x - a.x) * (pz - a.z) - (b.z - a.z) * (px - a.x) < -.00001f)
                        return false;
                }
            }
            return true;
        };
        std::size_t vertices=0,primitiveTriangles=0;
        for (std::size_t offset=0;offset<whole.size();)
        {
            const auto command=static_cast<Command>(whole[offset]);
            if (command==Command::Vertex) ++vertices;
            if (command==Command::CubeV && std::abs(unpack(whole[offset+5])-.020f)<.00001f &&
                (std::abs(unpack(whole[offset+2])-deck.y-.011f)<.00001f ||
                 std::abs(unpack(whole[offset+2])-deck.y-.061f)<.00001f))
                expect(fixtureSeated(unpack(whole[offset+1]),unpack(whole[offset+3]),
                                      unpack(whole[offset+4]),unpack(whole[offset+6])),
                       "a radiator base overhangs the actual upper deck");
            if (command==Command::CylinderEx &&
                std::abs(unpack(whole[offset+2])-deck.y-.004f)<.00001f &&
                std::abs(unpack(whole[offset+5])-deck.y-.028f)<.00001f)
                expect(fixtureSeated(unpack(whole[offset+1]),unpack(whole[offset+3]),
                                      unpack(whole[offset+7])*2,unpack(whole[offset+7])*2),
                       "an engine fan overhangs the actual upper deck");
            if (command==Command::Cube || command==Command::CubeV) primitiveTriangles+=12;
            if (command==Command::CylinderEx) primitiveTriangles+=4*whole[offset+9];
            if (command==Command::Cylinder) primitiveTriangles+=4*whole[offset+7];
            offset+=1+kCommandArguments[static_cast<std::size_t>(command)];
        }
        expect(vertices%3==0 && vertices/3+primitiveTriangles<=4000,
               "detailed chassis exceeded the 4000-triangle vehicle budget");
    }

    reporter.beginSuite("historical-turret-layout-and-gun-envelope");
    for (Nation nation : tanks3d::core::kSelectableNations)
    for (int tier = 0; tier < 4; ++tier)
    {
        namespace roster = arcade_tank_roster::detail;
        const auto design = wwii_tank_model::detail::rosterDesign(wwii_tank_model::playerVehicle(nation, tier));
        const auto cabin = roster::cabinSections(design);
        float front = cabin.front().front, rear = cabin.front().rear;
        for (const auto &section : cabin)
        {
            front = std::min(front, section.front);
            rear = std::max(rear, section.rear);
        }
        const float chassisWidth = 2 * design.shape.trackCenter + design.shape.trackWidth;
        expect(chassisWidth / design.shape.trackLength > .42f &&
                   chassisWidth / design.shape.trackLength < .58f,
               "the chassis returned to a short, oversized cartoon footprint");
        expect(design.shape.cabinWidth / chassisWidth > .60f &&
                   design.shape.cabinWidth / chassisWidth < .90f &&
                   design.shape.cabinHeight / design.shape.trackLength < .15f,
               "turret mass no longer fits the lower realistic proportions");
        const auto renderedMount = wwii_tank_model::renderedPlayerMuzzle(nation, tier);
        expect(std::abs(design.muzzleZ + renderedMount.x) < .00001f &&
                   std::abs(design.muzzleY - renderedMount.y) < .00001f &&
                   renderedMount.x > design.shape.trackLength * .50f &&
                   renderedMount.x < design.shape.trackLength * .91f,
               "the visible gun no longer agrees with its distinct art attachment");
        const float station = .5f + (front + rear) * .5f / design.shape.trackLength;
        expect(station > .32f && station < .55f,
               "a historical turret moved back over the rear engine compartment");
        const Stream whole = capture([&] { arcade_tank_roster::draw(design, WHITE, WHITE, false); });
        // Observe the submitted cupola transform rather than trusting a model
        // flag. Right is +X with the standard -Z gun-facing convention.
        const bool right = nation == Nation::UnitedStates ||
            (nation == Nation::Germany && tier >= 2) ||
            (nation == Nation::SovietUnion && tier == 3);
        bool commanderLocated = false, evacuatorLocated = false;
        int muzzleRimVertices = 0;
        for (std::size_t offset = 0; offset < whole.size();)
        {
            const auto command = static_cast<Command>(whole[offset]);
            if (command == Command::Vertex)
            {
                const float x = unpack(whole[offset + 1]);
                const float y = unpack(whole[offset + 2]) - design.muzzleY;
                const float z = unpack(whole[offset + 3]);
                const float radius = std::sqrt(x * x + y * y);
                if (std::abs(z - design.muzzleZ) < .00001f &&
                    radius > design.shape.gunRadius * .68f &&
                    radius < design.shape.gunRadius * 1.80f)
                    ++muzzleRimVertices;
            }
            if (command == Command::Translate)
            {
                const float x = unpack(whole[offset + 1]);
                const float y = unpack(whole[offset + 2]);
                const float z = unpack(whole[offset + 3]);
                if (std::abs(y - cabin.back().y) < .00001f &&
                    std::abs(z - (cabin.back().front + cabin.back().rear) * .5f) < .05f)
                {
                    if (design.cartoon == arcade_tank_roster::CartoonModel::T34)
                        commanderLocated = commanderLocated || std::abs(x) < .00001f;
                    else if (std::abs(x) > .08f && std::abs(x) < .25f)
                        commanderLocated = right ? x > 0 : x < 0;
                }
            }
            if (command == Command::CylinderEx && tier >= 2 &&
                std::abs(unpack(whole[offset + 1])) < .00001f &&
                std::abs(unpack(whole[offset + 4])) < .00001f &&
                std::abs(unpack(whole[offset + 2]) - design.muzzleY) < .00001f &&
                std::abs(unpack(whole[offset + 5]) - design.muzzleY) < .00001f)
            {
                const float back = unpack(whole[offset + 3]);
                const float tip = unpack(whole[offset + 6]);
                const float radius = unpack(whole[offset + 7]);
                if (radius > design.shape.gunRadius * 1.10f)
                {
                    expect(tip > design.muzzleZ && back < front + .25f && tip < back,
                           "the bore evacuator floats ahead of the muzzle or behind the gun mount");
                    evacuatorLocated = true;
                }
            }
            offset += 1 + kCommandArguments[static_cast<std::size_t>(command)];
        }
        expect(muzzleRimVertices >= 36,
               "the rendered barrel rim no longer reaches the art-only muzzle attachment");
        expect(commanderLocated, "the selected variant lost its shared hatch or correctly sided commander hatch");
        if (tier >= 2)
            expect(evacuatorLocated, "the modern gun lost its separate bore evacuator");
    }

    reporter.beginSuite("stylized-gun-readability-and-fixed-mounts");
    constexpr std::array<std::array<Vector2, 4>, 3> fixedArtMounts{{
        {{{.87f,.5974f},{1.36f,.4872f},{1.44f,.5714f},{1.38f,.4550f}}},
        {{{.82f,.4952f},{1.49f,.5218f},{1.40f,.4456f},{1.52f,.4504f}}},
        {{{1.19f,.5622f},{1.43f,.6064f},{1.48f,.4774f},{1.43f,.5194f}}}}};
    for (Nation nation : tanks3d::core::kSelectableNations)
    for (int tier = 0; tier < 4; ++tier)
    {
        namespace roster = arcade_tank_roster::detail;
        const auto design = wwii_tank_model::detail::rosterDesign(wwii_tank_model::playerVehicle(nation, tier));
        const auto expected = fixedArtMounts[static_cast<std::size_t>(nation)][tier];
        expect(std::abs(design.muzzleY - expected.y) < .00001f &&
                   std::abs(design.muzzleZ + expected.x) < .00001f,
               "stylization moved a visible gun mount used by shell presentation");
        const Color paint = roster::tagged(wwii_tank_model::detail::nationalArmorPaint(nation));
        // Measure submitted shaft faces, not just the radius in the profile.
        // The long middle span should be painted armor; steel is local to the
        // end collar/muzzle, and neither can cover the recessed dark bore.
        const Stream gun = capture([&] {
            roster::cannon(0.0f, design.muzzleZ, design.muzzleY,
                           design.shape.gunRadius, paint, design.brake, 0.0f, true);
        });
        std::array<Vector3, 3> triangle{};
        std::size_t vertex = 0, shaftTriangles = 0;
        std::uint32_t color = 0;
        float minimumRadius = 1.0f;
        bool shaftPainted = true;
        const std::uint32_t armor = (static_cast<std::uint32_t>(paint.r) << 24U) |
            (static_cast<std::uint32_t>(paint.g) << 16U) |
            (static_cast<std::uint32_t>(paint.b) << 8U) | 14U;
        for (std::size_t offset = 0; offset < gun.size();)
        {
            const auto command = static_cast<Command>(gun[offset]);
            if (command == Command::Color) color = gun[offset + 1];
            if (command == Command::Vertex)
            {
                triangle[vertex++ % 3] = {unpack(gun[offset + 1]),
                    unpack(gun[offset + 2]), unpack(gun[offset + 3])};
                if (vertex % 3 == 0)
                {
                    const float progress = (triangle[0].z + triangle[1].z + triangle[2].z) /
                        (3.0f * design.muzzleZ);
                    if (progress > .30f && progress < .75f)
                    {
                        ++shaftTriangles;
                        shaftPainted = shaftPainted && color == armor;
                        for (const auto &point : triangle)
                        {
                            const float y = point.y - design.muzzleY;
                            minimumRadius = std::min(minimumRadius, std::sqrt(point.x * point.x + y * y));
                        }
                    }
                }
            }
            offset += 1 + kCommandArguments[static_cast<std::size_t>(command)];
        }
        // At 720 px / 18.5 world units the old thin shafts were marginal.
        // Require >2 px of submitted diameter without returning to toy barrels.
        const float projectedDiameter = 2.0f * minimumRadius * 720.0f / 18.5f;
        expect(shaftTriangles == 24 && shaftPainted &&
                   projectedDiameter > 2.3f && projectedDiameter < 3.5f,
               "the submitted main barrel became thin, oversized or uniformly bare metal");
        const auto mechanical = materialColors(gun, 15, 15);
        expect(std::find(mechanical.begin(), mechanical.end(), 0x6876710fU) != mechanical.end() &&
                   std::find(mechanical.begin(), mechanical.end(), 0x1620210fU) != mechanical.end(),
               "the gun lost its muted steel lip or dark recessed bore");
    }
    const Color winterPaint = wwii_tank_model::detail::nationalArmorPaint(Nation::SovietUnion);
    expect(std::min({winterPaint.r, winterPaint.g, winterPaint.b}) >= 170 &&
               std::max({winterPaint.r, winterPaint.g, winterPaint.b}) <= 195 &&
               winterPaint.g >= winterPaint.r && winterPaint.r > winterPaint.b,
           "winter armor returned to clipped near-white or lost its warm gray-green tint");

    reporter.beginSuite("national-camouflage-coplanar-coverage");
    for (Nation nation : tanks3d::core::kSelectableNations)
    for (bool enemy : {false, true})
    for (int role = 0; role < 4; ++role)
    {
        namespace roster = arcade_tank_roster::detail;
        const auto vehicle = enemy ? wwii_tank_model::enemyVehicle(nation, role)
                                   : wwii_tank_model::playerVehicle(nation, role);
        const auto design = wwii_tank_model::detail::rosterDesign(vehicle);
        const Color paint = wwii_tank_model::detail::nationalPlayerPaint(WHITE, nation);
        const std::array<std::uint32_t, 3> palette = nation == Nation::UnitedStates
            ? std::array<std::uint32_t, 3>{{0x55613d0eU, 0x594b370eU, 0x292e290eU}}
            : nation == Nation::SovietUnion
                ? std::array<std::uint32_t, 3>{{0xb8bcaf0eU, 0x4755410eU, 0x4755410eU}}
                : std::array<std::uint32_t, 3>{{0x45494b0eU, 0x45494b0eU, 0x45494b0eU}};
        const auto verify = [&](const auto &sections, bool turret) {
            const auto plan = roster::armorPlan(design.cartoon,turret);
            const Stream painted = capture([&] {
                roster::cartoonCasting(sections, roster::tagged(paint), design.family, {1,1},plan);
            });
            expect(validPaintRoof(painted, sections.back(), palette,plan,&sections[1],
                                  turret && !roster::hardArmor(plan)),
                   "camouflage roof lost its exact coverage, plane, normals or footprint");
        };
        verify(roster::cabinSections(design),true);
        verify(roster::hullSections(design),false);
    }

    reporter.beginSuite("cast-turret-curves-and-continuous-shoulders");
    for (Nation nation : tanks3d::core::kSelectableNations)
    for (int tier=0;tier<4;++tier)
    {
        namespace roster=arcade_tank_roster::detail;
        const auto design=wwii_tank_model::detail::rosterDesign(wwii_tank_model::playerVehicle(nation,tier));
        const auto plan=roster::armorPlan(design.cartoon,true);
        if (roster::hardArmor(plan)) continue;
        const auto sections=roster::cabinSections(design);
        constexpr std::array<std::size_t,12> originalIndices{{0,1,3,5,6,8,10,11,13,15,16,18}};
        std::array<std::array<Vector3,20>,6> rings{};
        bool originalNodes=true, envelope=true, convex=true;
        std::size_t shoulder=1;
        for (std::size_t level=2;level+1<sections.size();++level)
            if (sections[level].halfWidth>sections[shoulder].halfWidth) shoulder=level;
        for (std::size_t level=0;level<sections.size();++level)
        {
            const auto original=roster::armorSection(sections[level],plan);
            auto &ring=rings[level];
            ring=roster::castTurretSection(sections[level],plan);
            for (std::size_t i=0;i<original.size();++i)
            {
                const auto p=ring[originalIndices[i]], q=original[i];
                originalNodes=originalNodes && p.x==q.x && p.y==q.y && p.z==q.z;
            }
            for (std::size_t i=0;i<ring.size();++i)
            {
                const auto p=ring[i], q=ring[(i+1)%ring.size()], r=ring[(i+2)%ring.size()];
                envelope=envelope && p.y==sections[level].y &&
                    std::abs(p.x)<=sections[level].halfWidth+.000001f &&
                    p.z>=sections[level].front-.000001f && p.z<=sections[level].rear+.000001f;
                convex=convex && (q.x-p.x)*(r.z-q.z)-(q.z-p.z)*(r.x-q.x)>=-.000001f;
            }
        }
        expect(originalNodes,"cast refinement moved an original vehicle-specific shoulder or roof station");
        expect(envelope && convex,"cast arc left the protected envelope or folded its outline");
        const auto stream=capture([&] {
            roster::cartoonCasting(sections,roster::tagged(WHITE),design.family,{1,1},plan,design.cartoon);
        });
        std::array<std::array<std::array<Vector3,20>,6>,2> shared{};
        std::array<std::array<std::array<int,20>,6>,2> hits{};
        std::array<Vector3,3> points{},normals{};
        std::size_t vertices=0;
        Vector3 normal{};
        bool continuous=true;
        for (std::size_t offset=0;offset<stream.size();)
        {
            const auto command=static_cast<Command>(stream[offset]);
            if (command==Command::Normal)
                normal={unpack(stream[offset+1]),unpack(stream[offset+2]),unpack(stream[offset+3])};
            if (command==Command::Vertex)
            {
                points[vertices%3]={unpack(stream[offset+1]),unpack(stream[offset+2]),unpack(stream[offset+3])};
                normals[vertices++%3]=normal;
                if (vertices%3==0 &&
                    std::max({points[0].y,points[1].y,points[2].y})-
                    std::min({points[0].y,points[1].y,points[2].y})>.000001f)
                {
                    const std::size_t group=std::min({points[0].y,points[1].y,points[2].y})<
                        sections[shoulder].y-.000001f ? 0 : 1;
                    for (std::size_t p=0;p<3;++p)
                    for (std::size_t level=0;level<rings.size();++level)
                    for (std::size_t index=0;index<rings[level].size();++index)
                    {
                        const auto delta=roster::mesh::minus(points[p],rings[level][index]);
                        if (roster::dot(delta,delta)>1e-12f) continue;
                        auto &prior=shared[group][level][index];
                        auto &count=hits[group][level][index];
                        if (count) continuous=continuous && roster::dot(prior,normals[p])>.9999f;
                        else prior=normals[p];
                        ++count;
                    }
                }
            }
            offset+=1+kCommandArguments[static_cast<std::size_t>(command)];
        }
        bool covered=true;
        std::size_t neckBreaks=0;
        for (std::size_t level=0;level<rings.size();++level)
        for (std::size_t i=0;i<20;++i)
        {
            if (level<=shoulder) covered=covered && hits[0][level][i]>=2;
            if (level>=shoulder) covered=covered && hits[1][level][i]>=2;
            if (level==shoulder && roster::dot(shared[0][level][i],shared[1][level][i])<.995f)
                ++neckBreaks;
        }
        expect(continuous && covered,"cast shoulder normals split along a triangle or omit an authored curve station");
        expect(neckBreaks>=4,"cast neck lost its structural crease at the widest section");
    }

    reporter.beginSuite("vehicle-armor-planes-normals-and-roof-support");
    for (Nation nation : tanks3d::core::kSelectableNations)
    for (int tier=0;tier<4;++tier)
    {
        namespace roster=arcade_tank_roster::detail;
        const auto design=wwii_tank_model::detail::rosterDesign(wwii_tank_model::playerVehicle(nation,tier));
        const auto inspect=[&](const auto &sections,bool turret) {
            const auto plan=roster::armorPlan(design.cartoon,turret);
            const bool hard=roster::hardArmor(plan);
            bool envelope=true, planar=true;
            constexpr std::array<std::size_t,8> corners{{0,1,3,4,6,7,9,10}};
            for (std::size_t level=0;level<sections.size();++level)
            {
                const auto ring=roster::armorSection(sections[level],plan,&sections[1]);
                float low=100,high=-100,width=0;
                for (const auto &p:ring)
                {
                    low=std::min(low,p.z);high=std::max(high,p.z);width=std::max(width,std::abs(p.x));
                    envelope=envelope && std::abs(p.y-sections[level].y)<.000001f;
                }
                envelope=envelope && std::abs(low-sections[level].front)<.000001f &&
                    std::abs(high-sections[level].rear)<.000001f &&
                    std::abs(width-sections[level].halfWidth)<.000001f;
                if (hard && level+1<sections.size())
                {
                    const auto next=roster::armorSection(sections[level+1],plan,&sections[1]);
                    for (std::size_t i=0;i<8;++i)
                    {
                        const auto a=ring[corners[i]],b=ring[corners[(i+1)%8]],c=next[corners[i]],e=next[corners[(i+1)%8]];
                        const auto n=chaffee_sample_model::detail::normal(b,a,c);
                        const float distance=std::abs(roster::dot(n,chaffee_sample_model::detail::minus(e,b)));
                        planar=planar && distance<.000002f;
                    }
                }
            }
            expect(envelope,"authored armor facet change moved its protected width/height/front/rear envelope");
            expect(planar,"welded armor contains a twisted quad instead of a true planar plate");
            const Stream stream=capture([&] {
                roster::cartoonCasting(sections,roster::tagged(WHITE),design.family,{1,1},plan);
            });
            std::array<Vector3,3> points{},normals{};
            std::size_t vertex=0,soft=0;Vector3 normal{};bool valid=true;
            for (std::size_t offset=0;offset<stream.size();)
            {
                const auto command=static_cast<Command>(stream[offset]);
                if (command==Command::Normal) normal={unpack(stream[offset+1]),unpack(stream[offset+2]),unpack(stream[offset+3])};
                if (command==Command::Vertex)
                {
                    points[vertex%3]={unpack(stream[offset+1]),unpack(stream[offset+2]),unpack(stream[offset+3])};
                    normals[vertex++%3]=normal;
                    if (vertex%3==0)
                    {
                        const auto ab=chaffee_sample_model::detail::minus(points[1],points[0]);
                        const auto ac=chaffee_sample_model::detail::minus(points[2],points[0]);
                        const Vector3 cross{ab.y*ac.z-ab.z*ac.y,ab.z*ac.x-ab.x*ac.z,ab.x*ac.y-ab.y*ac.x};
                        if (roster::dot(cross,cross)>.000000000001f)
                        {
                            const auto face=roster::unit(cross);
                            for (const auto &n:normals)
                            {
                                const float agreement=roster::dot(n,face);
                                valid=valid && std::isfinite(agreement) && std::abs(roster::dot(n,n)-1.0f)<.00001f &&
                                    agreement>(hard ? .9999f : .83f);
                                soft+=agreement<.998f;
                            }
                        }
                    }
                }
                offset+=1+kCommandArguments[static_cast<std::size_t>(command)];
            }
            expect(valid && (hard || soft>40),"armor normals became invalid, smooth across welded seams, or flat on cast shoulders");
            const auto cached=capture([&] {
                roster::cartoonCasting(sections,roster::tagged(WHITE),design.family,{1,1},plan,design.cartoon);
            });
            expect(cached==stream && cached==capture([&] {
                roster::cartoonCasting(sections,roster::tagged(WHITE),design.family,{1,1},plan,design.cartoon);
            }),"cached armor preparation changed first/repeated submitted geometry or normals");
            const auto recolored=capture([&] {
                roster::cartoonCasting(sections,roster::tagged(GREEN),design.family,{.97f,1.03f},plan,design.cartoon);
            });
            expect(recolored==capture([&] {
                roster::cartoonCasting(sections,roster::tagged(GREEN),design.family,{.97f,1.03f},plan);
            }),"armor cache retained stale paint or camouflage coordinates");
            auto changed=sections;
            changed.back().front+=.001f;
            const auto alternate=capture([&] {
                roster::cartoonCasting(changed,roster::tagged(WHITE),design.family,{1,1},plan,design.cartoon);
            });
            expect(alternate==capture([&] {
                roster::cartoonCasting(changed,roster::tagged(WHITE),design.family,{1,1},plan);
            }),"armor cache reused a different diagnostic section under the same model key");
        };
        inspect(roster::hullSections(design),false);
        inspect(roster::cabinSections(design),true);
        const auto cabin=roster::cabinSections(design);
        if (design.cartoon!=arcade_tank_roster::CartoonModel::T34)
        {
            const auto stream=capture([&] { arcade_tank_roster::draw(design,WHITE,{255,219,78,255},false); });
            std::uint32_t color=0;bool supported=true;int marked=0;
            for (std::size_t offset=0;offset<stream.size();)
            {
                const auto command=static_cast<Command>(stream[offset]);
                if (command==Command::Color) color=stream[offset+1];
                if (command==Command::Vertex && color==0xffdb4e0eU &&
                    std::abs(unpack(stream[offset+2])-cabin.back().y-.005f)<.00001f)
                {
                    ++marked;
                    const float x=unpack(stream[offset+1]),z=unpack(stream[offset+3]);
                    supported=supported && std::abs(x)<=roster::armorHalfWidth(cabin.back(),
                        roster::armorPlan(design.cartoon,true),z,&cabin[1])+.00001f;
                }
                offset+=1+kCommandArguments[static_cast<std::size_t>(command)];
            }
            expect(marked==6 && supported,"player roof stripe floats beyond the actual authored armor polygon");
        }
    }

    reporter.beginSuite("arcade-enemy-armor-paint-preserves-shape");
    for (Nation nation : tanks3d::core::kSelectableNations)
    for (int role = 0; role < 4; ++role)
    {
        const auto drawArmor = [&](Color color, int armor, bool shadow) {
            return capture([&] {
                assets.draw(x, z, yaw, color, true, armor, 0.0f, role,
                            false, nation, shadow);
            });
        };
        const Stream blue = drawArmor({161, 194, 207, 255}, 1, false);
        const Stream teal = drawArmor({39, 151, 112, 255}, 4, false);
        expect(blue != teal, "enemy armor lost its visual color cue");
        expect(geometryOnly(blue) == geometryOnly(teal),
               "enemy damage changed the selected model or pose");
        expect(teal == drawArmor({39, 151, 112, 255}, 4, true),
               "enemy shadow geometry diverged from visible geometry");
    }

    reporter.beginSuite("national-armor-and-independent-materials");
    for (Nation nation : tanks3d::core::kSelectableNations)
    for (int role = 0; role < 4; ++role)
    {
        Stream firstPlayer;
        for (int identity : {0, 1})
        {
            const Stream player = capture([&] {
                assets.draw(x, z, yaw, paint, false, role, 0.0f,
                            identity, false, nation);
            });
            expect(nationalArmorDominates(player, nation),
                   "player broad armor lost its national hue");
            if (nation != Nation::Germany)
                expect(hasNationalCamouflage(player, nation),
                       "American/Soviet armor lost its broad camouflage patches");
            const Stream armorColors = materialColors(player, 14, 14);
            const std::uint32_t identityPaint = identity == 0 ? 0xffdb4e0eU
                                                             : 0x5bffac0eU;
            expect(std::find(armorColors.begin(), armorColors.end(), identityPaint) !=
                       armorColors.end(), "player lost its gold/green identity markings");
            if (identity == 0)
                firstPlayer = player;
            else
            {
                expect(materialColors(firstPlayer, 15, 17) ==
                           materialColors(player, 15, 17),
                       "player identity recolored rubber, metal or optics");
                expect(withoutPlayerMarks(materialColors(firstPlayer, 14, 14)) ==
                           withoutPlayerMarks(materialColors(player, 14, 14)),
                       "player identity changed broad armor or camouflage");
            }
            if (nation != Nation::Germany)
            {
                const Stream moved = capture([&] {
                    assets.draw(x + 12.0f, z - 7.0f, yaw + 90.0f, paint, false,
                                role, 0.0f, identity, false, nation);
                });
                expect(materialColors(player, 14, 17) == materialColors(moved, 14, 17),
                       "national camouflage swims when the vehicle moves or turns");
            }
        }

        Stream intact;
        for (int armor = 1; armor <= 4; ++armor)
        for (Color status : {Color{161, 194, 207, 255},
                             Color{39, 151, 112, 255},
                             Color{231, 108, 49, 255}})
        {
            const Stream enemy = capture([&] {
                assets.draw(x, z, yaw, status, true, armor, 0.0f,
                            role, false, nation);
            });
            expect(nationalArmorDominates(enemy, nation),
                   "enemy armor/status flash replaced its national hue");
            const Stream neutral = materialColors(enemy, 15, 17);
            expect(!neutral.empty(), "enemy lost separate mechanical materials");
            if (intact.empty())
                intact = enemy;
            else
            {
                expect(geometryOnly(intact) == geometryOnly(enemy),
                       "enemy status changed its geometry or pose");
                expect(materialColors(intact, 15, 17) == neutral,
                       "enemy status recolored rubber, metal or optics");
            }
        }
    }

    if (!passed)
        return 1;
    reporter.finish();
    return 0;
}
