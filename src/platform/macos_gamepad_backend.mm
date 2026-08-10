#include "platform/gamepad_backend.h"
#include "platform/gamepad_event_accumulator.h"

#import <Foundation/Foundation.h>
#import <GameController/GameController.h>

#include <array>
#include <cstddef>
#include <memory>
#include <string>

namespace tanks3d::platform
{
namespace
{
using Accumulator = GamepadEventAccumulator;
using Button = GamepadEventButton;

struct SlotState
{
    GCController *__strong controller = nil;
    std::shared_ptr<Accumulator> input;
    std::string name;
};

dispatch_queue_t gamepadHandlerQueue()
{
    static dispatch_queue_t queue = dispatch_queue_create(
        "com.tourzhao.tanks3d.gamepad-input",
        dispatch_queue_attr_make_with_qos_class(
            DISPATCH_QUEUE_SERIAL, QOS_CLASS_USER_INTERACTIVE, 0));
    return queue;
}

bool hasExtendedGamepad(GCController *controller)
{
    return controller != nil && controller.extendedGamepad != nil;
}

bool isControllerPresent(NSArray<GCController *> *controllers,
                         GCController *target)
{
    if (target == nil)
        return false;

    for (GCController *controller in controllers)
    {
        if (hasExtendedGamepad(controller) && controller == target)
            return true;
    }
    return false;
}

std::string controllerName(GCController *controller)
{
    NSString *name = controller.vendorName;
    if (name.length == 0U)
        name = controller.productCategory;
    if (name.length == 0U)
        return "Unknown controller";

    const char *utf8 = name.UTF8String;
    return utf8 != nullptr && utf8[0] != '\0' ? utf8
                                               : "Unknown controller";
}

bool buttonHeld(GCControllerButtonInput *button)
{
    return button != nil && button.pressed;
}

Accumulator::HeldButtons liveHeldButtons(GCExtendedGamepad *gamepad)
{
    Accumulator::HeldButtons held{};
    const auto set = [&](Button button, bool value) {
        held[static_cast<std::size_t>(button)] = value;
    };

    set(Button::DpadUp, buttonHeld(gamepad.dpad.up));
    set(Button::DpadRight, buttonHeld(gamepad.dpad.right));
    set(Button::DpadDown, buttonHeld(gamepad.dpad.down));
    set(Button::DpadLeft, buttonHeld(gamepad.dpad.left));

    // GameController names the face elements by physical position. A is the
    // bottom face button even when a Nintendo controller prints B there.
    set(Button::FaceUp, buttonHeld(gamepad.buttonY));
    set(Button::FaceRight, buttonHeld(gamepad.buttonB));
    set(Button::FaceDown, buttonHeld(gamepad.buttonA));
    set(Button::FaceLeft, buttonHeld(gamepad.buttonX));
    set(Button::LeftShoulder, buttonHeld(gamepad.leftShoulder));
    set(Button::RightShoulder, buttonHeld(gamepad.rightShoulder));
    set(Button::RightTrigger, buttonHeld(gamepad.rightTrigger));
    set(Button::MiddleLeft, buttonHeld(gamepad.buttonOptions));
    set(Button::MiddleRight, buttonHeld(gamepad.buttonMenu));
    return held;
}

void updateButton(const std::shared_ptr<Accumulator> &input,
                  Button button, bool held)
{
    input->updateButton(button, held);
}

void installButtonHandler(GCControllerButtonInput *nativeButton,
                          const std::shared_ptr<Accumulator> &input,
                          Button button)
{
    if (nativeButton == nil)
        return;
    // The function parameter is a reference. Capture an owning local copy;
    // capturing `input` directly would leave the asynchronous block holding a
    // dangling reference after this helper returns.
    const std::shared_ptr<Accumulator> callbackInput = input;
    nativeButton.pressedChangedHandler =
        ^(GCControllerButtonInput *, float, BOOL pressed) {
            updateButton(callbackInput, button, pressed == YES);
        };
}

void clearHandlers(GCExtendedGamepad *gamepad)
{
    if (gamepad == nil)
        return;
    gamepad.leftThumbstick.valueChangedHandler = nil;
    gamepad.dpad.valueChangedHandler = nil;
    for (GCControllerButtonInput *button in @[
             gamepad.buttonA, gamepad.buttonB, gamepad.buttonX,
             gamepad.buttonY, gamepad.leftShoulder,
             gamepad.rightShoulder, gamepad.rightTrigger,
             gamepad.buttonMenu])
    {
        button.pressedChangedHandler = nil;
    }
    if (gamepad.buttonOptions != nil)
        gamepad.buttonOptions.pressedChangedHandler = nil;
}

void installHandlers(GCExtendedGamepad *gamepad,
                     const std::shared_ptr<Accumulator> &input)
{
    // Objective-C blocks copy non-reference C++ locals. Keep separate owning
    // copies for the long-lived callbacks instead of capturing the reference
    // parameter itself.
    const std::shared_ptr<Accumulator> stickInput = input;
    const std::shared_ptr<Accumulator> dpadInput = input;
    gamepad.leftThumbstick.valueChangedHandler =
        ^(GCControllerDirectionPad *, float x, float y) {
            // GameController uses positive Y for up; the raylib-free adapter
            // follows the raylib convention where negative Y means up.
            stickInput->updateStick(x, -y);
        };
    gamepad.dpad.valueChangedHandler =
        ^(GCControllerDirectionPad *, float x, float y) {
            dpadInput->updateDpad(y > 0.5f, x > 0.5f, y < -0.5f,
                                  x < -0.5f);
        };

    installButtonHandler(gamepad.buttonY, input, Button::FaceUp);
    installButtonHandler(gamepad.buttonB, input, Button::FaceRight);
    installButtonHandler(gamepad.buttonA, input, Button::FaceDown);
    installButtonHandler(gamepad.buttonX, input, Button::FaceLeft);
    installButtonHandler(gamepad.leftShoulder, input,
                         Button::LeftShoulder);
    installButtonHandler(gamepad.rightShoulder, input,
                         Button::RightShoulder);
    installButtonHandler(gamepad.rightTrigger, input,
                         Button::RightTrigger);
    installButtonHandler(gamepad.buttonOptions, input, Button::MiddleLeft);
    installButtonHandler(gamepad.buttonMenu, input, Button::MiddleRight);
}

void disableDeferredSystemGesture(GCControllerButtonInput *button)
{
    if (button != nil && button.boundToSystemGesture)
        button.preferredSystemGestureState = GCSystemGestureStateDisabled;
}

void attachController(SlotState &slot, GCController *controller)
{
    GCExtendedGamepad *gamepad = controller.extendedGamepad;
    auto input = std::make_shared<Accumulator>();
    slot.controller = controller;
    slot.input = input;
    slot.name = controllerName(controller);

    controller.handlerQueue = gamepadHandlerQueue();
    dispatch_sync(gamepadHandlerQueue(), ^{
        disableDeferredSystemGesture(gamepad.buttonOptions);
        disableDeferredSystemGesture(gamepad.buttonMenu);
        input->activate(gamepad.leftThumbstick.xAxis.value,
                        -gamepad.leftThumbstick.yAxis.value,
                        liveHeldButtons(gamepad));
        installHandlers(gamepad, input);

        // Close the tiny seed-to-handler installation window. Persistent
        // changes become cached now; subsequent changes arrive as callbacks.
        input->updateStick(gamepad.leftThumbstick.xAxis.value,
                           -gamepad.leftThumbstick.yAxis.value);
        const Accumulator::HeldButtons held = liveHeldButtons(gamepad);
        for (std::size_t index = 0; index < held.size(); ++index)
        {
            input->updateButton(static_cast<Button>(index), held[index]);
        }
    });
}

void detachController(SlotState &slot)
{
    if (slot.controller == nil)
    {
        slot = SlotState{};
        return;
    }

    const std::shared_ptr<Accumulator> input = slot.input;
    if (input)
        input->deactivate();
    GCExtendedGamepad *gamepad = slot.controller.extendedGamepad;
    dispatch_sync(gamepadHandlerQueue(), ^{
        clearHandlers(gamepad);
    });
    slot.controller.handlerQueue = dispatch_get_main_queue();
    slot = SlotState{};
}
} // namespace

struct GamepadBackend::Impl
{
    std::array<SlotState, app::kPhysicalGamepadSlotCount> slots{};

    ~Impl()
    {
        for (SlotState &slot : slots)
            detachController(slot);
    }
};

GamepadBackend::GamepadBackend() : impl_(std::make_unique<Impl>())
{
}

GamepadBackend::~GamepadBackend() = default;

GamepadBackendFrame GamepadBackend::poll()
{
    GamepadBackendFrame frame;
    @autoreleasepool
    {
        NSArray<GCController *> *controllers = GCController.controllers;

        // Release slots only after the corresponding controller disappears.
        // This prevents a remaining P2 controller from shifting into P1's
        // physical slot when P1 disconnects during play.
        for (SlotState &slot : impl_->slots)
        {
            if (slot.controller != nil &&
                !isControllerPresent(controllers, slot.controller))
            {
                detachController(slot);
            }
        }

        std::array<GCController *, app::kPhysicalGamepadSlotCount>
            controllersBySlot{};
        for (GCController *controller in controllers)
        {
            if (!hasExtendedGamepad(controller))
                continue;
            for (std::size_t slotIndex = 0;
                 slotIndex < impl_->slots.size(); ++slotIndex)
            {
                if (impl_->slots[slotIndex].controller == controller)
                {
                    controllersBySlot[slotIndex] = controller;
                    break;
                }
            }
        }

        for (GCController *controller in controllers)
        {
            if (!hasExtendedGamepad(controller))
                continue;

            bool alreadyAssigned = false;
            for (const SlotState &slot : impl_->slots)
            {
                if (slot.controller == controller)
                {
                    alreadyAssigned = true;
                    break;
                }
            }
            if (alreadyAssigned)
                continue;

            for (std::size_t slotIndex = 0;
                 slotIndex < impl_->slots.size(); ++slotIndex)
            {
                SlotState &slot = impl_->slots[slotIndex];
                if (slot.controller != nil)
                    continue;
                attachController(slot, controller);
                controllersBySlot[slotIndex] = controller;
                break;
            }
        }

        for (std::size_t slotIndex = 0;
             slotIndex < impl_->slots.size(); ++slotIndex)
        {
            GCController *controller = controllersBySlot[slotIndex];
            SlotState &slot = impl_->slots[slotIndex];
            if (controller == nil || !slot.input)
                continue;

            slot.name = controllerName(controller);
            frame.names[slotIndex] = slot.name;
            frame.snapshots[slotIndex] = slot.input->consume();
        }
    }
    return frame;
}
} // namespace tanks3d::platform
