#ifndef TANKS3D_PLATFORM_GAMEPAD_BACKEND_H
#define TANKS3D_PLATFORM_GAMEPAD_BACKEND_H

#include "app/input_adapter.h"

#include <array>
#include <memory>
#include <string>

namespace tanks3d::platform
{
struct GamepadBackendFrame
{
    std::array<app::GamepadSnapshot, app::kPhysicalGamepadSlotCount>
        snapshots{};
    std::array<std::string, app::kPhysicalGamepadSlotCount> names{};
};

// Platform gamepad polling is kept behind a C++ interface so Objective-C and
// GameController types do not leak into the application or gameplay layers.
// Construction does not query the operating system; poll() performs discovery
// and preserves a controller's physical slot until it disconnects.
class GamepadBackend
{
  public:
    GamepadBackend();
    ~GamepadBackend();

    GamepadBackend(const GamepadBackend &) = delete;
    GamepadBackend &operator=(const GamepadBackend &) = delete;
    GamepadBackend(GamepadBackend &&) = delete;
    GamepadBackend &operator=(GamepadBackend &&) = delete;

    GamepadBackendFrame poll();

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace tanks3d::platform

#endif
