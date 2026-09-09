#include "tank_assets.h"
#include "test_support.h"

#include <cstdint>
#include <cstring>
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

    if (!passed)
        return 1;
    reporter.finish();
    return 0;
}
