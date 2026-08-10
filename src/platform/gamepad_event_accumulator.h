#ifndef TANKS3D_PLATFORM_GAMEPAD_EVENT_ACCUMULATOR_H
#define TANKS3D_PLATFORM_GAMEPAD_EVENT_ACCUMULATOR_H

#include "app/input_adapter.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <mutex>

namespace tanks3d::platform
{
enum class GamepadEventButton : std::size_t
{
    DpadUp,
    DpadRight,
    DpadDown,
    DpadLeft,
    FaceUp,
    FaceRight,
    FaceDown,
    FaceLeft,
    LeftShoulder,
    RightShoulder,
    RightTrigger,
    MiddleLeft,
    MiddleRight,
    Count
};

inline constexpr std::size_t kGamepadEventButtonCount =
    static_cast<std::size_t>(GamepadEventButton::Count);

// Thread-safe handoff between platform callbacks and the frame loop. Rising
// edges remain latched until consume(), so a complete press/release between
// two rendered frames is still observed exactly once.
class GamepadEventAccumulator
{
  public:
    using HeldButtons = std::array<bool, kGamepadEventButtonCount>;

    void activate(float stickX, float stickY,
                  const HeldButtons &heldButtons)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        active_ = true;
        stickX_ = normalizedAxis(stickX);
        stickY_ = normalizedAxis(stickY);
        held_ = heldButtons;
        // Preserve the polling backend's connection semantics: controls that
        // are already held when a device is attached emit one initial edge.
        pressedLatched_ = heldButtons;
    }

    void deactivate()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        active_ = false;
        stickX_ = 0.0f;
        stickY_ = 0.0f;
        held_.fill(false);
        pressedLatched_.fill(false);
    }

    void updateStick(float stickX, float stickY)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!active_)
            return;
        stickX_ = normalizedAxis(stickX);
        stickY_ = normalizedAxis(stickY);
    }

    void updateButton(GamepadEventButton button, bool held)
    {
        const std::size_t index = static_cast<std::size_t>(button);
        if (index >= held_.size())
            return;

        std::lock_guard<std::mutex> lock(mutex_);
        if (!active_)
            return;
        updateButtonUnlocked(index, held);
    }

    void updateDpad(bool up, bool right, bool down, bool left)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!active_)
            return;
        updateButtonUnlocked(
            static_cast<std::size_t>(GamepadEventButton::DpadUp), up);
        updateButtonUnlocked(
            static_cast<std::size_t>(GamepadEventButton::DpadRight), right);
        updateButtonUnlocked(
            static_cast<std::size_t>(GamepadEventButton::DpadDown), down);
        updateButtonUnlocked(
            static_cast<std::size_t>(GamepadEventButton::DpadLeft), left);
    }

    app::GamepadSnapshot consume()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        app::GamepadSnapshot snapshot;
        if (!active_)
            return snapshot;

        snapshot.available = true;
        snapshot.leftStickX = stickX_;
        snapshot.leftStickY = stickY_;
        snapshot.dpadUp = consumeButton(GamepadEventButton::DpadUp);
        snapshot.dpadRight = consumeButton(GamepadEventButton::DpadRight);
        snapshot.dpadDown = consumeButton(GamepadEventButton::DpadDown);
        snapshot.dpadLeft = consumeButton(GamepadEventButton::DpadLeft);
        snapshot.faceUp = consumeButton(GamepadEventButton::FaceUp);
        snapshot.faceRight = consumeButton(GamepadEventButton::FaceRight);
        snapshot.faceDown = consumeButton(GamepadEventButton::FaceDown);
        snapshot.faceLeft = consumeButton(GamepadEventButton::FaceLeft);
        snapshot.leftShoulder =
            consumeButton(GamepadEventButton::LeftShoulder);
        snapshot.rightShoulder =
            consumeButton(GamepadEventButton::RightShoulder);
        snapshot.rightTrigger =
            consumeButton(GamepadEventButton::RightTrigger);
        snapshot.middleLeft = consumeButton(GamepadEventButton::MiddleLeft);
        snapshot.middleRight =
            consumeButton(GamepadEventButton::MiddleRight);
        return snapshot;
    }

  private:
    void updateButtonUnlocked(std::size_t index, bool held)
    {
        pressedLatched_[index] =
            pressedLatched_[index] || (held && !held_[index]);
        held_[index] = held;
    }

    static float normalizedAxis(float value)
    {
        if (!std::isfinite(value))
            return 0.0f;
        return std::clamp(value, -1.0f, 1.0f);
    }

    app::ButtonSnapshot consumeButton(GamepadEventButton button)
    {
        const std::size_t index = static_cast<std::size_t>(button);
        const app::ButtonSnapshot result{held_[index],
                                         pressedLatched_[index]};
        pressedLatched_[index] = false;
        return result;
    }

    std::mutex mutex_;
    bool active_ = false;
    float stickX_ = 0.0f;
    float stickY_ = 0.0f;
    HeldButtons held_{};
    HeldButtons pressedLatched_{};
};
} // namespace tanks3d::platform

#endif
