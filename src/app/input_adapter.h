#ifndef TANKS3D_APP_INPUT_ADAPTER_H
#define TANKS3D_APP_INPUT_ADAPTER_H

#include "game/player_system.h"

#include <array>
#include <cstddef>

namespace tanks3d::app
{
struct ButtonSnapshot
{
    bool held = false;
    bool pressed = false;
};

struct GamepadSnapshot
{
    bool available = false;
    float leftStickX = 0.0f;
    float leftStickY = 0.0f;
    ButtonSnapshot dpadUp;
    ButtonSnapshot dpadRight;
    ButtonSnapshot dpadDown;
    ButtonSnapshot dpadLeft;
    ButtonSnapshot faceUp;
    ButtonSnapshot faceRight;
    ButtonSnapshot faceDown;
    ButtonSnapshot faceLeft;
    ButtonSnapshot leftShoulder;
    ButtonSnapshot rightShoulder;
    ButtonSnapshot rightTrigger;
    ButtonSnapshot middleLeft;
    ButtonSnapshot middleRight;
};

struct GamepadInputState
{
    core::CardinalDirection previousStickDirection =
        core::CardinalDirection::None;
    int cameraYawDegrees = 0;
    bool suppressStickUntilRelease = false;
};

struct UiInputFrame
{
    bool upPressed = false;
    bool downPressed = false;
    bool leftPressed = false;
    bool rightPressed = false;
    bool confirmPressed = false;
    bool cancelPressed = false;
    bool quitPressed = false;
    bool pausePressed = false;
    bool restartPressed = false;
    bool resetPressed = false;
    bool coarseAdjustment = false;
    bool selectOnePlayerPressed = false;
    bool selectTwoPlayerPressed = false;
};

struct GamepadActionFrame
{
    game::PlayerControlFrame player;
    UiInputFrame ui;
};

// Four-way movement still accepts the full analogue circle: the nearest
// cardinal direction wins.  A modest radial dead zone filters centre drift,
// while the smaller release threshold and narrow turn ratio prevent sluggish
// starts without making diagonal boundary noise flip direction every frame.
inline constexpr float kGamepadStickEngageThreshold = 0.20f;
inline constexpr float kGamepadStickReleaseThreshold = 0.12f;
inline constexpr float kGamepadStickTurnAxisRatio = 1.08f;
inline constexpr std::size_t kPhysicalGamepadSlotCount = 4U;
inline constexpr std::size_t kPlayerGamepadSlotCount = 2U;
inline constexpr int kCameraYawMinimumDegrees = -45;
inline constexpr int kCameraYawMaximumDegrees = 45;
inline constexpr int kCameraYawStepDegrees = 5;

struct CameraPlanarBasis
{
    float rightX = 1.0f;
    float rightZ = 0.0f;
    float offsetX = 0.0f;
    float offsetZ = 1.0f;
};

int normalizedCameraYawDegrees(int requestedDegrees);
CameraPlanarBasis cameraPlanarBasis(int requestedDegrees);

struct GamepadAssignments
{
    std::array<int, kPhysicalGamepadSlotCount> playerByPhysicalSlot{{
        -1, -1, -1, -1}};
    bool gameplayStable = false;

    // Menu assignments compact the available physical slots into P1/P2.
    // Gameplay preserves every still-connected player's slot, but a newly
    // connected pad may fill a vacancy left by a disconnect without moving the
    // other player.
    void update(
        const std::array<bool, kPhysicalGamepadSlotCount> &available,
        bool gameplayActive);
    int playerIndexForPhysicalSlot(std::size_t physicalSlot) const;
};

// Converts one raylib-free device snapshot into cardinal player and UI
// commands. During gameplay, cameraYawDegrees rotates the analogue stick from
// screen space into the four world lanes before quantization. D-pad and
// keyboard directions remain one-to-one world-cardinal controls. A disconnect
// or camera-angle change clears the stick edge state. Returning to a menu can
// suppress a held stick until it crosses the release threshold.
GamepadActionFrame mapGamepadInput(
    const GamepadSnapshot &snapshot, GamepadInputState &state,
    int cameraYawDegrees = 0);

// OR-merges independent physical sources while preserving pressed => held for
// canonical player direction buttons.
void mergePlayerControlFrame(
    game::PlayerControlFrame &target,
    const game::PlayerControlFrame &source);
void mergeUiInputFrame(UiInputFrame &target, const UiInputFrame &source);
} // namespace tanks3d::app

#endif
