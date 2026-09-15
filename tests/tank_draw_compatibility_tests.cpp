#include "tank_assets.h"
#include "test_support.h"

#include <array>
#include <cstdint>
#include <cstring>
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
    constexpr std::array<std::size_t, 16> arguments{
        0, 0, 3, 3, 4, 1, 0, 1, 3, 3, 7, 7, 8, 10, 5, 7};
    Stream geometry;
    int matrixDepth = 0, beginDepth = 0;
    for (std::size_t offset = 0; offset < stream.size();)
    {
        const auto command = stream[offset];
        if (command >= arguments.size() || offset + arguments[command] >= stream.size())
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
                            stream.begin() + offset + 1 + arguments[command]);
            // raylib primitives embed their color as the final argument.
            if (command >= static_cast<unsigned>(Command::Cube)) geometry.back() = 0U;
        }
        offset += 1 + arguments[command];
    }
    if (matrixDepth != 0 || beginDepth != 0)
        throw std::runtime_error("leaked render state");
    return geometry;
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

    if (!passed)
        return 1;
    reporter.finish();
    return 0;
}
