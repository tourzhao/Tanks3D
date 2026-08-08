#ifndef TANKS3D_APP_PRESENTATION_VALUES_H
#define TANKS3D_APP_PRESENTATION_VALUES_H

#include <cstdint>

namespace tanks3d::app
{
// Renderer-independent values shared by presentation command modules.
struct Float3
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct Rgba8
{
    std::uint8_t r = 0U;
    std::uint8_t g = 0U;
    std::uint8_t b = 0U;
    std::uint8_t a = 0U;
};
} // namespace tanks3d::app

#endif
