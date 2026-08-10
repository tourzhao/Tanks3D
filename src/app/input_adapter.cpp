#include "app/input_adapter.h"

#include <algorithm>
#include <cmath>

namespace tanks3d::app
{
namespace
{
using core::CardinalDirection;
using game::DirectionButtonFrame;

constexpr float kInverseSquareRootOfTwo = 0.70710678118f;

bool active(const ButtonSnapshot &button)
{
    return button.held || button.pressed;
}

float finiteAxis(float value)
{
    return std::isfinite(value) ? value : 0.0f;
}

bool directionStillHeld(CardinalDirection direction, float x, float y)
{
    switch (direction)
    {
    case CardinalDirection::North:
        return y <= -kGamepadStickReleaseThreshold;
    case CardinalDirection::South:
        return y >= kGamepadStickReleaseThreshold;
    case CardinalDirection::West:
        return x <= -kGamepadStickReleaseThreshold;
    case CardinalDirection::East:
        return x >= kGamepadStickReleaseThreshold;
    case CardinalDirection::None:
        return false;
    }
    return false;
}

bool horizontal(CardinalDirection direction)
{
    return direction == CardinalDirection::West ||
           direction == CardinalDirection::East;
}

float squaredMagnitude(float x, float y)
{
    return x * x + y * y;
}

CardinalDirection horizontalDirection(float x)
{
    return x < 0.0f ? CardinalDirection::West : CardinalDirection::East;
}

CardinalDirection verticalDirection(float y)
{
    return y < 0.0f ? CardinalDirection::North : CardinalDirection::South;
}

CardinalDirection newlyEngagedDirection(float x, float y)
{
    const float absoluteX = std::fabs(x);
    const float absoluteY = std::fabs(y);
    const float engageSquared =
        kGamepadStickEngageThreshold * kGamepadStickEngageThreshold;
    if (squaredMagnitude(x, y) < engageSquared)
        return CardinalDirection::None;
    // Vertical wins an exact 45-degree tie. The fixed choice prevents tiny
    // platform-dependent differences from producing non-deterministic input.
    return absoluteX > absoluteY ? horizontalDirection(x)
                                 : verticalDirection(y);
}

CardinalDirection resolveStickDirection(
    float x, float y, CardinalDirection previous)
{
    const bool previousHeld = directionStillHeld(previous, x, y);
    if (!previousHeld)
        return newlyEngagedDirection(x, y);

    const float currentMagnitude = horizontal(previous) ? std::fabs(x)
                                                        : std::fabs(y);
    const float turningMagnitude = horizontal(previous) ? std::fabs(y)
                                                        : std::fabs(x);
    // Only a narrow angular lead is needed to cross a 45-degree boundary.
    // That gives imperfect analogue input a wide cardinal sector, while the
    // ratio supplies enough hysteresis to avoid chatter near the diagonal.
    if (squaredMagnitude(x, y) >=
            kGamepadStickEngageThreshold *
                kGamepadStickEngageThreshold &&
        turningMagnitude >=
            currentMagnitude * kGamepadStickTurnAxisRatio)
    {
        return horizontal(previous) ? verticalDirection(y)
                                    : horizontalDirection(x);
    }
    return previous;
}

DirectionButtonFrame &buttonForDirection(
    game::PlayerControlFrame &controls, CardinalDirection direction)
{
    switch (direction)
    {
    case CardinalDirection::North: return controls.north;
    case CardinalDirection::South: return controls.south;
    case CardinalDirection::West: return controls.west;
    case CardinalDirection::East: return controls.east;
    case CardinalDirection::None: return controls.north;
    }
    return controls.north;
}

void mapDigitalButton(DirectionButtonFrame &target,
                      const ButtonSnapshot &source)
{
    target.held = active(source);
    target.pressed = source.pressed;
}

void mergeDirectionButton(DirectionButtonFrame &target,
                          const DirectionButtonFrame &source)
{
    target.held = target.held || target.pressed || source.held ||
                  source.pressed;
    target.pressed = target.pressed || source.pressed;
}
} // namespace

void GamepadAssignments::update(
    const std::array<bool, kPhysicalGamepadSlotCount> &available,
    bool gameplayActive)
{
    if (gameplayActive && gameplayStable)
    {
        std::array<bool, kPlayerGamepadSlotCount> playerOccupied{};
        for (std::size_t physicalSlot = 0;
             physicalSlot < available.size(); ++physicalSlot)
        {
            const int playerIndex = playerByPhysicalSlot[physicalSlot];
            if (!available[physicalSlot] || playerIndex < 0 ||
                playerIndex >= static_cast<int>(playerOccupied.size()))
            {
                playerByPhysicalSlot[physicalSlot] = -1;
                continue;
            }
            playerOccupied[static_cast<std::size_t>(playerIndex)] = true;
        }

        for (std::size_t physicalSlot = 0;
             physicalSlot < available.size(); ++physicalSlot)
        {
            if (!available[physicalSlot] ||
                playerByPhysicalSlot[physicalSlot] >= 0)
            {
                continue;
            }
            for (std::size_t playerIndex = 0;
                 playerIndex < playerOccupied.size(); ++playerIndex)
            {
                if (playerOccupied[playerIndex])
                    continue;
                playerByPhysicalSlot[physicalSlot] =
                    static_cast<int>(playerIndex);
                playerOccupied[playerIndex] = true;
                break;
            }
        }
        return;
    }

    playerByPhysicalSlot.fill(-1);
    int nextPlayer = 0;
    for (std::size_t physicalSlot = 0;
         physicalSlot < available.size() &&
         nextPlayer < static_cast<int>(kPlayerGamepadSlotCount);
         ++physicalSlot)
    {
        if (available[physicalSlot])
            playerByPhysicalSlot[physicalSlot] = nextPlayer++;
    }
    gameplayStable = gameplayActive;
}

int GamepadAssignments::playerIndexForPhysicalSlot(
    std::size_t physicalSlot) const
{
    return physicalSlot < playerByPhysicalSlot.size()
               ? playerByPhysicalSlot[physicalSlot]
               : -1;
}

GamepadActionFrame mapGamepadInput(
    const GamepadSnapshot &snapshot, GamepadInputState &state,
    GamepadStickOrientation orientation)
{
    GamepadActionFrame actions;
    if (state.stickOrientation != orientation)
    {
        state.previousStickDirection = CardinalDirection::None;
        state.stickOrientation = orientation;
    }
    if (!snapshot.available)
    {
        state.previousStickDirection = CardinalDirection::None;
        return actions;
    }

    const bool dpadActive = active(snapshot.dpadUp) ||
        active(snapshot.dpadDown) || active(snapshot.dpadLeft) ||
        active(snapshot.dpadRight);
    if (dpadActive)
    {
        // Treat the stick as a fresh input after D-pad use. This keeps the
        // D-pad authoritative and lets a still-deflected stick emit a menu
        // edge as soon as the D-pad is released.
        state.previousStickDirection = CardinalDirection::None;
        mapDigitalButton(actions.player.north, snapshot.dpadUp);
        mapDigitalButton(actions.player.south, snapshot.dpadDown);
        mapDigitalButton(actions.player.west, snapshot.dpadLeft);
        mapDigitalButton(actions.player.east, snapshot.dpadRight);
    }
    else
    {
        const float rawStickX = finiteAxis(snapshot.leftStickX);
        const float rawStickY = finiteAxis(snapshot.leftStickY);
        const float stickX = orientation ==
                                     GamepadStickOrientation::Isometric45
                                 ? (rawStickX + rawStickY) *
                                       kInverseSquareRootOfTwo
                                 : rawStickX;
        const float stickY = orientation ==
                                     GamepadStickOrientation::Isometric45
                                 ? (rawStickY - rawStickX) *
                                       kInverseSquareRootOfTwo
                                 : rawStickY;
        const CardinalDirection previousStick = state.previousStickDirection;
        const CardinalDirection stickDirection =
            resolveStickDirection(stickX, stickY, previousStick);
        state.previousStickDirection = stickDirection;
        if (stickDirection != CardinalDirection::None)
        {
            DirectionButtonFrame &button =
                buttonForDirection(actions.player, stickDirection);
            button.held = true;
            button.pressed = stickDirection != previousStick;
        }
    }

    actions.ui.upPressed = actions.player.north.pressed;
    actions.ui.downPressed = actions.player.south.pressed;
    actions.ui.leftPressed = actions.player.west.pressed;
    actions.ui.rightPressed = actions.player.east.pressed;

    actions.player.fireHeld = active(snapshot.faceDown) ||
        active(snapshot.faceLeft) || active(snapshot.rightShoulder) ||
        active(snapshot.rightTrigger);
    actions.ui.confirmPressed = snapshot.faceDown.pressed ||
        snapshot.middleRight.pressed;
    // Keep menu/back actions off the face cluster so an ordinary fire or
    // confirm press cannot accidentally leave the battle. The controller's
    // dedicated Minus/Back button is the only gamepad cancel source.
    actions.ui.cancelPressed = snapshot.middleLeft.pressed;
    actions.ui.quitPressed = actions.ui.cancelPressed;
    actions.ui.pausePressed = snapshot.middleRight.pressed;
    actions.ui.resetPressed = snapshot.faceUp.pressed;
    actions.ui.coarseAdjustment = active(snapshot.leftShoulder) ||
        active(snapshot.rightShoulder);
    return actions;
}

void mergePlayerControlFrame(
    game::PlayerControlFrame &target,
    const game::PlayerControlFrame &source)
{
    mergeDirectionButton(target.north, source.north);
    mergeDirectionButton(target.south, source.south);
    mergeDirectionButton(target.west, source.west);
    mergeDirectionButton(target.east, source.east);
    target.fireHeld = target.fireHeld || source.fireHeld;
}

void mergeUiInputFrame(UiInputFrame &target, const UiInputFrame &source)
{
    target.upPressed = target.upPressed || source.upPressed;
    target.downPressed = target.downPressed || source.downPressed;
    target.leftPressed = target.leftPressed || source.leftPressed;
    target.rightPressed = target.rightPressed || source.rightPressed;
    target.confirmPressed = target.confirmPressed || source.confirmPressed;
    target.cancelPressed = target.cancelPressed || source.cancelPressed;
    target.quitPressed = target.quitPressed || source.quitPressed;
    target.pausePressed = target.pausePressed || source.pausePressed;
    target.restartPressed = target.restartPressed || source.restartPressed;
    target.resetPressed = target.resetPressed || source.resetPressed;
    target.coarseAdjustment =
        target.coarseAdjustment || source.coarseAdjustment;
    target.selectOnePlayerPressed =
        target.selectOnePlayerPressed || source.selectOnePlayerPressed;
    target.selectTwoPlayerPressed =
        target.selectTwoPlayerPressed || source.selectTwoPlayerPressed;
}
} // namespace tanks3d::app
