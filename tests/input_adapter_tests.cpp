#include "app/input_adapter.h"
#include "platform/gamepad_event_accumulator.h"
#include "test_support.h"

#include <array>
#include <cmath>
#include <limits>
#include <string>

namespace
{
using tanks3d::app::ButtonSnapshot;
using tanks3d::app::CameraPlanarBasis;
using tanks3d::app::GamepadActionFrame;
using tanks3d::app::GamepadAssignments;
using tanks3d::app::GamepadInputState;
using tanks3d::app::GamepadSnapshot;
using tanks3d::app::UiInputFrame;
using tanks3d::app::cameraPlanarBasis;
using tanks3d::app::kCameraYawMaximumDegrees;
using tanks3d::app::kCameraYawMinimumDegrees;
using tanks3d::app::kCameraYawStepDegrees;
using tanks3d::app::kGamepadStickEngageThreshold;
using tanks3d::app::kGamepadStickReleaseThreshold;
using tanks3d::app::kGamepadStickTurnAxisRatio;
using tanks3d::app::mapGamepadInput;
using tanks3d::app::mergePlayerControlFrame;
using tanks3d::app::mergeUiInputFrame;
using tanks3d::app::normalizedCameraYawDegrees;
using tanks3d::core::CardinalDirection;
using tanks3d::game::DirectionButtonFrame;
using tanks3d::game::PlayerControlFrame;
using tanks3d::platform::GamepadEventAccumulator;
using tanks3d::platform::GamepadEventButton;

bool active(const DirectionButtonFrame &button)
{
    return button.held || button.pressed;
}

int activeDirectionCount(const PlayerControlFrame &controls)
{
    int count = 0;
    for (const DirectionButtonFrame *button : {
             &controls.north, &controls.south,
             &controls.west, &controls.east})
    {
        if (active(*button))
            ++count;
    }
    return count;
}

bool noPlayerInput(const PlayerControlFrame &controls)
{
    return activeDirectionCount(controls) == 0 && !controls.fireHeld;
}

bool noUiInput(const UiInputFrame &ui)
{
    return !ui.upPressed && !ui.downPressed && !ui.leftPressed &&
           !ui.rightPressed && !ui.confirmPressed && !ui.cancelPressed &&
           !ui.quitPressed && !ui.pausePressed && !ui.restartPressed &&
           !ui.resetPressed && !ui.coarseAdjustment &&
           !ui.selectOnePlayerPressed && !ui.selectTwoPlayerPressed;
}

const DirectionButtonFrame &directionButton(
    const PlayerControlFrame &controls, CardinalDirection direction)
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

bool uiDirectionPressed(const UiInputFrame &ui,
                        CardinalDirection direction)
{
    switch (direction)
    {
    case CardinalDirection::North: return ui.upPressed;
    case CardinalDirection::South: return ui.downPressed;
    case CardinalDirection::West: return ui.leftPressed;
    case CardinalDirection::East: return ui.rightPressed;
    case CardinalDirection::None: return false;
    }
    return false;
}

bool isSingleDirection(const GamepadActionFrame &actions,
                       CardinalDirection direction,
                       bool pressed)
{
    const DirectionButtonFrame &button =
        directionButton(actions.player, direction);
    return activeDirectionCount(actions.player) == 1 && button.held &&
           button.pressed == pressed &&
           uiDirectionPressed(actions.ui, direction) == pressed;
}

GamepadSnapshot connectedGamepad()
{
    GamepadSnapshot snapshot;
    snapshot.available = true;
    return snapshot;
}
} // namespace

int main()
{
    tanks3d_test::Reporter reporter;
    reporter.reset();
    bool passed = true;
    const auto expect = [&](bool condition, const std::string &message) {
        if (!reporter.check(condition, message))
            passed = false;
    };

    reporter.beginSuite("gamepad-event-cache-latches-frame-between-taps");
    GamepadEventAccumulator eventCache;
    expect(!eventCache.consume().available,
           "inactive event cache reported a connected controller");

    GamepadEventAccumulator::HeldButtons initialHeld{};
    initialHeld[static_cast<std::size_t>(
        GamepadEventButton::FaceDown)] = true;
    eventCache.activate(0.30f, -0.40f, initialHeld);
    const GamepadSnapshot initialEventFrame = eventCache.consume();
    expect(initialEventFrame.available &&
               initialEventFrame.leftStickX == 0.30f &&
               initialEventFrame.leftStickY == -0.40f &&
               initialEventFrame.faceDown.held &&
               initialEventFrame.faceDown.pressed,
           "event cache lost its seeded state or initial held edge");
    const GamepadSnapshot persistentHeldFrame = eventCache.consume();
    expect(persistentHeldFrame.faceDown.held &&
               !persistentHeldFrame.faceDown.pressed,
           "event cache repeated a consumed edge or lost held state");

    eventCache.updateButton(GamepadEventButton::FaceDown, false);
    eventCache.updateButton(GamepadEventButton::FaceDown, true);
    eventCache.updateButton(GamepadEventButton::FaceDown, false);
    const GamepadSnapshot quickTapFrame = eventCache.consume();
    expect(!quickTapFrame.faceDown.held && quickTapFrame.faceDown.pressed,
           "press/release between frames was not latched exactly once");
    expect(!eventCache.consume().faceDown.pressed,
           "frame-between tap edge survived more than one consume");

    eventCache.updateDpad(true, true, false, false);
    const GamepadSnapshot diagonalDpadFrame = eventCache.consume();
    expect(diagonalDpadFrame.dpadUp.held &&
               diagonalDpadFrame.dpadUp.pressed &&
               diagonalDpadFrame.dpadRight.held &&
               diagonalDpadFrame.dpadRight.pressed,
           "event cache lost one side of a D-pad diagonal");
    eventCache.updateStick(std::numeric_limits<float>::infinity(),
                           std::numeric_limits<float>::quiet_NaN());
    const GamepadSnapshot sanitizedEventFrame = eventCache.consume();
    expect(sanitizedEventFrame.leftStickX == 0.0f &&
               sanitizedEventFrame.leftStickY == 0.0f,
           "event cache retained non-finite stick values");
    eventCache.deactivate();
    eventCache.updateButton(GamepadEventButton::FaceDown, true);
    expect(!eventCache.consume().available,
           "late callback reactivated a disconnected event cache");

    reporter.beginSuite("gamepad-disconnect-clears-input-and-edge-state");
    GamepadInputState disconnectState{CardinalDirection::East};
    GamepadSnapshot disconnected;
    disconnected.leftStickX = 1.0f;
    disconnected.dpadUp.pressed = true;
    disconnected.faceDown.pressed = true;
    const GamepadActionFrame disconnectedActions =
        mapGamepadInput(disconnected, disconnectState);
    expect(noPlayerInput(disconnectedActions.player) &&
               noUiInput(disconnectedActions.ui) &&
               disconnectState.previousStickDirection ==
                   CardinalDirection::None,
           "unavailable controller emitted input or retained stick state");

    GamepadSnapshot reconnected = connectedGamepad();
    reconnected.leftStickX = 0.8f;
    const GamepadActionFrame reconnectActions =
        mapGamepadInput(reconnected, disconnectState);
    expect(isSingleDirection(reconnectActions, CardinalDirection::East,
                             true) &&
               disconnectState.previousStickDirection ==
                   CardinalDirection::East,
           "reconnected deflected stick did not emit a fresh edge");
    const GamepadActionFrame reconnectHeld =
        mapGamepadInput(reconnected, disconnectState);
    expect(isSingleDirection(reconnectHeld, CardinalDirection::East,
                             false),
           "unchanged reconnected stick repeated its pressed edge");

    reporter.beginSuite("gamepad-dpad-cardinal-map-and-priority");
    struct DpadCase
    {
        ButtonSnapshot GamepadSnapshot::*input;
        CardinalDirection direction;
    };
    const std::array<DpadCase, 4> dpadCases{{
        {&GamepadSnapshot::dpadUp, CardinalDirection::North},
        {&GamepadSnapshot::dpadDown, CardinalDirection::South},
        {&GamepadSnapshot::dpadLeft, CardinalDirection::West},
        {&GamepadSnapshot::dpadRight, CardinalDirection::East}}};
    bool dpadMapCorrect = true;
    for (const DpadCase &testCase : dpadCases)
    {
        GamepadSnapshot snapshot = connectedGamepad();
        (snapshot.*testCase.input).pressed = true;
        GamepadInputState state;
        const GamepadActionFrame actions = mapGamepadInput(snapshot, state);
        dpadMapCorrect = dpadMapCorrect &&
            isSingleDirection(actions, testCase.direction, true);
    }
    expect(dpadMapCorrect,
           "a D-pad edge did not map to one held-and-pressed cardinal action");

    GamepadSnapshot heldDpad = connectedGamepad();
    heldDpad.dpadLeft.held = true;
    GamepadInputState heldDpadState;
    expect(isSingleDirection(mapGamepadInput(heldDpad, heldDpadState),
                             CardinalDirection::West, false),
           "held D-pad direction incorrectly emitted an edge");

    GamepadSnapshot dpadPriority = connectedGamepad();
    dpadPriority.leftStickX = -1.0f;
    dpadPriority.dpadUp.pressed = true;
    GamepadInputState dpadPriorityState;
    const GamepadActionFrame dpadPriorityActions =
        mapGamepadInput(dpadPriority, dpadPriorityState);
    expect(isSingleDirection(dpadPriorityActions,
                             CardinalDirection::North, true) &&
               !active(dpadPriorityActions.player.west),
           "left stick leaked through while the D-pad was active");
    dpadPriority.dpadUp = {};
    const GamepadActionFrame stickAfterDpad =
        mapGamepadInput(dpadPriority, dpadPriorityState);
    expect(isSingleDirection(stickAfterDpad,
                             CardinalDirection::West, true),
           "deflected stick did not emit a fresh edge after D-pad release");

    reporter.beginSuite("gamepad-stick-deadzone-hysteresis-and-edges");
    GamepadInputState thresholdState;
    GamepadSnapshot threshold = connectedGamepad();
    threshold.leftStickX = std::nextafter(
        kGamepadStickEngageThreshold, 0.0f);
    expect(noPlayerInput(mapGamepadInput(threshold, thresholdState).player) &&
               thresholdState.previousStickDirection ==
                   CardinalDirection::None,
           "stick engaged below the engage threshold");
    threshold.leftStickX = kGamepadStickEngageThreshold;
    expect(isSingleDirection(mapGamepadInput(threshold, thresholdState),
                             CardinalDirection::East, true),
           "stick did not engage at the engage threshold");
    expect(isSingleDirection(mapGamepadInput(threshold, thresholdState),
                             CardinalDirection::East, false),
           "held stick repeated its initial edge");
    threshold.leftStickX = kGamepadStickReleaseThreshold;
    expect(isSingleDirection(mapGamepadInput(threshold, thresholdState),
                             CardinalDirection::East, false),
           "stick released at the inclusive release threshold");
    threshold.leftStickX = std::nextafter(
        kGamepadStickReleaseThreshold, 0.0f);
    expect(noPlayerInput(mapGamepadInput(threshold, thresholdState).player) &&
               thresholdState.previousStickDirection ==
                   CardinalDirection::None,
           "stick remained active below the release threshold");
    threshold.leftStickX =
        (kGamepadStickEngageThreshold + kGamepadStickReleaseThreshold) /
        2.0f;
    expect(noPlayerInput(mapGamepadInput(threshold, thresholdState).player),
           "released stick re-engaged inside the hysteresis band");

    GamepadSnapshot radialDiagonal = connectedGamepad();
    const float diagonalComponent =
        kGamepadStickEngageThreshold * 0.72f;
    radialDiagonal.leftStickX = diagonalComponent;
    radialDiagonal.leftStickY = -diagonalComponent;
    GamepadInputState radialDiagonalState;
    expect(isSingleDirection(
               mapGamepadInput(radialDiagonal, radialDiagonalState),
               CardinalDirection::North, true),
           "radial stick engagement rejected a diagonal outside the "
           "per-axis dead zone");

    struct StickCase
    {
        float x;
        float y;
        CardinalDirection direction;
    };
    const std::array<StickCase, 4> stickCases{{
        {0.8f, 0.0f, CardinalDirection::East},
        {-0.8f, 0.0f, CardinalDirection::West},
        {0.0f, -0.8f, CardinalDirection::North},
        {0.0f, 0.8f, CardinalDirection::South}}};
    bool stickSignsCorrect = true;
    for (const StickCase &testCase : stickCases)
    {
        GamepadSnapshot snapshot = connectedGamepad();
        snapshot.leftStickX = testCase.x;
        snapshot.leftStickY = testCase.y;
        GamepadInputState state;
        stickSignsCorrect = stickSignsCorrect &&
            isSingleDirection(mapGamepadInput(snapshot, state),
                              testCase.direction, true);
    }
    expect(stickSignsCorrect,
           "left-stick axis sign did not map to the expected cardinal "
           "direction");

    reporter.beginSuite("camera-yaw-basis-range-and-stick-main-axes");
    expect(kCameraYawMinimumDegrees == -45 &&
               kCameraYawMaximumDegrees == 45 &&
               kCameraYawStepDegrees == 5 &&
               normalizedCameraYawDegrees(-99) == -45 &&
               normalizedCameraYawDegrees(99) == 45 &&
               normalizedCameraYawDegrees(17) == 17,
           "camera yaw range, step, or clamping contract changed");
    const std::array<int, 5> cameraAngles{{-45, -30, 0, 30, 45}};
    for (const int angle : cameraAngles)
    {
        const CameraPlanarBasis basis = cameraPlanarBasis(angle);
        expect(std::fabs(basis.rightX * basis.rightX +
                         basis.rightZ * basis.rightZ - 1.0f) < 0.0001f &&
                   std::fabs(basis.offsetX * basis.offsetX +
                             basis.offsetZ * basis.offsetZ - 1.0f) < 0.0001f &&
                   std::fabs(basis.rightX * basis.offsetX +
                             basis.rightZ * basis.offsetZ) < 0.0001f,
               "camera yaw basis was not orthonormal");
        for (const StickCase &testCase : stickCases)
        {
            GamepadSnapshot snapshot = connectedGamepad();
            snapshot.leftStickX = testCase.x;
            snapshot.leftStickY = testCase.y;
            GamepadInputState state;
            expect(isSingleDirection(
                       mapGamepadInput(snapshot, state, angle),
                       testCase.direction, true),
                   "camera endpoint or intermediate yaw collapsed a "
                   "screen-cardinal stick direction");
        }
    }

    reporter.beginSuite("camera-yaw-stick-follows-visible-world-lanes");
    struct WorldLaneCase
    {
        float x;
        float z;
        CardinalDirection direction;
    };
    const std::array<WorldLaneCase, 4> worldLanes{{
        {0.0f, -0.8f, CardinalDirection::North},
        {0.8f, 0.0f, CardinalDirection::East},
        {0.0f, 0.8f, CardinalDirection::South},
        {-0.8f, 0.0f, CardinalDirection::West}}};
    for (const int angle : cameraAngles)
    {
        const CameraPlanarBasis basis = cameraPlanarBasis(angle);
        for (const WorldLaneCase &lane : worldLanes)
        {
            GamepadSnapshot snapshot = connectedGamepad();
            // Invert the orthonormal screen-to-world basis so the stick points
            // along the lane as it appears at this camera azimuth.
            snapshot.leftStickX =
                lane.x * basis.rightX + lane.z * basis.rightZ;
            snapshot.leftStickY =
                lane.x * basis.offsetX + lane.z * basis.offsetZ;
            GamepadInputState state;
            expect(isSingleDirection(
                       mapGamepadInput(snapshot, state, angle),
                       lane.direction, true),
                   "a visible map lane did not resolve to its world cardinal "
                   "direction at the selected camera yaw");
        }
    }

    reporter.beginSuite("camera-yaw-change-clears-stick-hysteresis");
    GamepadSnapshot yawChange = connectedGamepad();
    yawChange.leftStickX = 0.8f;
    GamepadInputState yawChangeState;
    expect(isSingleDirection(mapGamepadInput(yawChange, yawChangeState, 0),
                             CardinalDirection::East, true),
           "camera-yaw transition setup did not engage east");
    expect(isSingleDirection(mapGamepadInput(yawChange, yawChangeState, 45),
                             CardinalDirection::East, true) &&
               yawChangeState.cameraYawDegrees == 45,
           "camera-yaw change retained the old stick edge state");
    expect(isSingleDirection(mapGamepadInput(yawChange, yawChangeState, 99),
                             CardinalDirection::East, false) &&
               yawChangeState.cameraYawDegrees == 45,
           "equivalent clamped camera yaw repeated a stick edge");

    reporter.beginSuite("camera-yaw-endpoints-tolerate-main-axis-drift");
    struct DriftCase
    {
        float x;
        float y;
        CardinalDirection direction;
    };
    const std::array<DriftCase, 8> endpointDriftCases{{
        {0.80f, -0.02f, CardinalDirection::East},
        {0.80f, 0.02f, CardinalDirection::East},
        {-0.80f, -0.02f, CardinalDirection::West},
        {-0.80f, 0.02f, CardinalDirection::West},
        {-0.02f, -0.80f, CardinalDirection::North},
        {0.02f, -0.80f, CardinalDirection::North},
        {-0.02f, 0.80f, CardinalDirection::South},
        {0.02f, 0.80f, CardinalDirection::South}}};
    for (const int angle : std::array<int, 2>{{-45, 45}})
    {
        for (const DriftCase &testCase : endpointDriftCases)
        {
            GamepadSnapshot snapshot = connectedGamepad();
            snapshot.leftStickX = testCase.x;
            snapshot.leftStickY = testCase.y;
            GamepadInputState state;
            expect(isSingleDirection(
                       mapGamepadInput(snapshot, state, angle),
                       testCase.direction, true),
                   "minor endpoint stick drift crossed into an adjacent "
                   "world lane");
        }
    }

    reporter.beginSuite("menu-transition-suppresses-held-stick-edge");
    GamepadSnapshot heldOnReturn = connectedGamepad();
    heldOnReturn.leftStickX = 0.80f;
    heldOnReturn.faceDown.pressed = true;
    GamepadInputState heldOnReturnState;
    heldOnReturnState.suppressStickUntilRelease = true;
    const GamepadActionFrame suppressedReturn =
        mapGamepadInput(heldOnReturn, heldOnReturnState, 0);
    expect(activeDirectionCount(suppressedReturn.player) == 0 &&
               suppressedReturn.player.fireHeld &&
               suppressedReturn.ui.confirmPressed &&
               heldOnReturnState.suppressStickUntilRelease,
           "menu-transition suppression lost button input or leaked a held "
           "stick edge");
    heldOnReturn.faceDown = {};
    heldOnReturn.leftStickX = std::nextafter(
        kGamepadStickReleaseThreshold, 0.0f);
    expect(noPlayerInput(
               mapGamepadInput(heldOnReturn, heldOnReturnState, 0).player) &&
               !heldOnReturnState.suppressStickUntilRelease,
           "centred stick did not clear menu-transition suppression");
    heldOnReturn.leftStickX = 0.80f;
    expect(isSingleDirection(
               mapGamepadInput(heldOnReturn, heldOnReturnState, 0),
               CardinalDirection::East, true),
           "fresh deflection after menu-transition suppression lacked an "
           "edge");

    reporter.beginSuite("menu-transition-stick-release-during-dpad-use");
    GamepadSnapshot returnWithDpad = connectedGamepad();
    returnWithDpad.leftStickX = 0.80f;
    returnWithDpad.dpadUp.held = true;
    GamepadInputState returnWithDpadState;
    returnWithDpadState.suppressStickUntilRelease = true;
    expect(isSingleDirection(
               mapGamepadInput(returnWithDpad, returnWithDpadState),
               CardinalDirection::North, false) &&
               returnWithDpadState.suppressStickUntilRelease,
           "D-pad lost priority or cleared suppression without a stick "
           "release");
    returnWithDpad.dpadUp = {};
    expect(noPlayerInput(
               mapGamepadInput(returnWithDpad, returnWithDpadState).player) &&
               returnWithDpadState.suppressStickUntilRelease,
           "D-pad release enabled a stick that had not returned to centre");
    returnWithDpad.dpadUp.held = true;
    returnWithDpad.leftStickX = 0.0f;
    expect(isSingleDirection(
               mapGamepadInput(returnWithDpad, returnWithDpadState),
               CardinalDirection::North, false) &&
               !returnWithDpadState.suppressStickUntilRelease,
           "D-pad input hid the stick release needed to clear suppression");
    returnWithDpad.leftStickX = 0.80f;
    expect(isSingleDirection(
               mapGamepadInput(returnWithDpad, returnWithDpadState),
               CardinalDirection::North, false),
           "fresh stick deflection overrode the held D-pad after suppression");
    returnWithDpad.dpadUp = {};
    expect(isSingleDirection(
               mapGamepadInput(returnWithDpad, returnWithDpadState),
               CardinalDirection::East, true),
           "stick centred during D-pad use needed another release to recover");
    expect(isSingleDirection(
               mapGamepadInput(returnWithDpad, returnWithDpadState),
               CardinalDirection::East, false),
           "recovered stick lost held input or repeated its pressed edge");

    reporter.beginSuite("camera-yaw-keeps-dpad-world-cardinal");
    for (const int angle : std::array<int, 2>{{-45, 45}})
    {
        for (const DpadCase &testCase : dpadCases)
        {
            GamepadSnapshot snapshot = connectedGamepad();
            (snapshot.*testCase.input).pressed = true;
            GamepadInputState state;
            expect(isSingleDirection(mapGamepadInput(snapshot, state, angle),
                                     testCase.direction, true),
                   "camera yaw rotated or collapsed a D-pad world lane");
        }
    }

    reporter.beginSuite("gamepad-stick-diagonal-lock-and-axis-turn");
    GamepadSnapshot tie = connectedGamepad();
    tie.leftStickX = 0.7f;
    tie.leftStickY = 0.7f;
    GamepadInputState tieState;
    expect(isSingleDirection(mapGamepadInput(tie, tieState),
                             CardinalDirection::South, true),
           "exact diagonal did not use the deterministic vertical tie break");

    GamepadSnapshot dominant = connectedGamepad();
    dominant.leftStickX = 0.8f;
    dominant.leftStickY = 0.6f;
    GamepadInputState turnState;
    expect(isSingleDirection(mapGamepadInput(dominant, turnState),
                             CardinalDirection::East, true),
           "initial diagonal did not choose its dominant axis");
    dominant.leftStickX = 0.50f;
    dominant.leftStickY =
        dominant.leftStickX *
        std::nextafter(kGamepadStickTurnAxisRatio, 0.0f);
    expect(isSingleDirection(mapGamepadInput(dominant, turnState),
                             CardinalDirection::East, false),
           "stick changed axes without the angular hysteresis margin");
    dominant.leftStickY =
        dominant.leftStickX * kGamepadStickTurnAxisRatio;
    expect(isSingleDirection(mapGamepadInput(dominant, turnState),
                             CardinalDirection::South, true),
           "stick did not change axes at the angular hysteresis margin");

    GamepadSnapshot tolerantNorth = connectedGamepad();
    tolerantNorth.leftStickX = 0.58f;
    tolerantNorth.leftStickY = -0.72f;
    GamepadInputState tolerantNorthState;
    expect(isSingleDirection(
               mapGamepadInput(tolerantNorth, tolerantNorthState),
               CardinalDirection::North, true),
           "off-axis north deflection was not accepted by its wide sector");

    GamepadSnapshot reversal = connectedGamepad();
    reversal.leftStickX = 0.8f;
    GamepadInputState reversalState;
    mapGamepadInput(reversal, reversalState);
    reversal.leftStickX = -0.8f;
    expect(isSingleDirection(mapGamepadInput(reversal, reversalState),
                             CardinalDirection::West, true),
           "same-axis reversal did not emit a new direction edge");

    reporter.beginSuite("gamepad-stick-non-finite-values-are-neutral");
    const float infinity = std::numeric_limits<float>::infinity();
    const float nan = std::numeric_limits<float>::quiet_NaN();
    GamepadSnapshot malformed = connectedGamepad();
    malformed.leftStickX = infinity;
    malformed.leftStickY = nan;
    GamepadInputState malformedState{CardinalDirection::East};
    expect(noPlayerInput(mapGamepadInput(malformed, malformedState).player) &&
               malformedState.previousStickDirection ==
                   CardinalDirection::None,
           "non-finite axes produced input or retained hysteresis state");
    malformed.leftStickX = nan;
    malformed.leftStickY = -0.8f;
    expect(isSingleDirection(mapGamepadInput(malformed, malformedState),
                             CardinalDirection::North, true),
           "one malformed axis suppressed the other finite axis");

    reporter.beginSuite("gamepad-fire-buttons-and-ui-actions");
    const std::array<ButtonSnapshot GamepadSnapshot::*, 4> fireButtons{{
        &GamepadSnapshot::faceDown,
        &GamepadSnapshot::faceLeft,
        &GamepadSnapshot::rightShoulder,
        &GamepadSnapshot::rightTrigger}};
    bool allFireButtonsWork = true;
    for (ButtonSnapshot GamepadSnapshot::*button : fireButtons)
    {
        GamepadSnapshot snapshot = connectedGamepad();
        (snapshot.*button).held = true;
        GamepadInputState state;
        allFireButtonsWork = allFireButtonsWork &&
            mapGamepadInput(snapshot, state).player.fireHeld;
    }
    expect(allFireButtonsWork,
           "one of the four configured fire controls did not hold fire");

    GamepadSnapshot faceConfirm = connectedGamepad();
    faceConfirm.faceDown.pressed = true;
    GamepadInputState faceConfirmState;
    const GamepadActionFrame faceConfirmActions =
        mapGamepadInput(faceConfirm, faceConfirmState);
    expect(faceConfirmActions.player.fireHeld &&
               faceConfirmActions.ui.confirmPressed &&
               !faceConfirmActions.ui.pausePressed,
           "south face button did not combine fire with confirm");

    GamepadSnapshot start = connectedGamepad();
    start.middleRight.pressed = true;
    GamepadInputState startState;
    const GamepadActionFrame startActions =
        mapGamepadInput(start, startState);
    expect(startActions.ui.confirmPressed && startActions.ui.pausePressed &&
               !startActions.player.fireHeld,
           "middle-right button did not combine confirm with pause");

    GamepadSnapshot cancel = connectedGamepad();
    cancel.faceRight.pressed = true;
    GamepadInputState cancelState;
    const GamepadActionFrame cancelActions =
        mapGamepadInput(cancel, cancelState);
    expect(!cancelActions.ui.cancelPressed && !cancelActions.ui.quitPressed,
           "east face button could accidentally cancel or quit");
    cancel = connectedGamepad();
    cancel.middleLeft.pressed = true;
    const GamepadActionFrame backActions =
        mapGamepadInput(cancel, cancelState);
    expect(backActions.ui.cancelPressed && backActions.ui.quitPressed,
           "Minus/Back button did not map to contextual cancel and quit");

    GamepadSnapshot reset = connectedGamepad();
    reset.faceUp.pressed = true;
    GamepadInputState resetState;
    const GamepadActionFrame resetActions =
        mapGamepadInput(reset, resetState);
    expect(!resetActions.ui.restartPressed && resetActions.ui.resetPressed,
           "north face button did not map to safe menu-only reset");

    GamepadSnapshot coarse = connectedGamepad();
    coarse.leftShoulder.held = true;
    GamepadInputState coarseState;
    const GamepadActionFrame leftCoarse =
        mapGamepadInput(coarse, coarseState);
    expect(leftCoarse.ui.coarseAdjustment &&
               !leftCoarse.player.fireHeld,
           "left shoulder did not enable only coarse adjustment");
    coarse = connectedGamepad();
    coarse.rightShoulder.pressed = true;
    const GamepadActionFrame rightCoarse =
        mapGamepadInput(coarse, coarseState);
    expect(rightCoarse.ui.coarseAdjustment &&
               rightCoarse.player.fireHeld,
           "right shoulder did not combine coarse adjustment with fire");
    expect(!rightCoarse.ui.selectOnePlayerPressed &&
               !rightCoarse.ui.selectTwoPlayerPressed,
           "gamepad mapping unexpectedly emitted keyboard player-count "
           "shortcuts");

    GamepadSnapshot heldEdges = connectedGamepad();
    heldEdges.faceRight.held = true;
    heldEdges.faceUp.held = true;
    heldEdges.middleRight.held = true;
    GamepadInputState heldEdgesState;
    const UiInputFrame heldEdgeActions =
        mapGamepadInput(heldEdges, heldEdgesState).ui;
    expect(!heldEdgeActions.confirmPressed &&
               !heldEdgeActions.cancelPressed &&
               !heldEdgeActions.quitPressed &&
               !heldEdgeActions.pausePressed &&
               !heldEdgeActions.restartPressed &&
               !heldEdgeActions.resetPressed,
           "held UI buttons repeated edge-triggered commands");

    reporter.beginSuite("input-source-merges-are-additive-and-normalized");
    PlayerControlFrame targetPlayer;
    targetPlayer.north.pressed = true;
    PlayerControlFrame sourcePlayer;
    sourcePlayer.south.pressed = true;
    sourcePlayer.east.held = true;
    sourcePlayer.fireHeld = true;
    mergePlayerControlFrame(targetPlayer, sourcePlayer);
    expect(targetPlayer.north.held && targetPlayer.north.pressed &&
               targetPlayer.south.held && targetPlayer.south.pressed &&
               targetPlayer.east.held && !targetPlayer.east.pressed &&
               !active(targetPlayer.west) && targetPlayer.fireHeld,
           "player merge lost a source or failed to normalize pressed => held");

    UiInputFrame targetUi;
    targetUi.upPressed = true;
    targetUi.confirmPressed = true;
    targetUi.selectOnePlayerPressed = true;
    UiInputFrame sourceUi;
    sourceUi.downPressed = true;
    sourceUi.leftPressed = true;
    sourceUi.rightPressed = true;
    sourceUi.cancelPressed = true;
    sourceUi.quitPressed = true;
    sourceUi.pausePressed = true;
    sourceUi.restartPressed = true;
    sourceUi.resetPressed = true;
    sourceUi.coarseAdjustment = true;
    sourceUi.selectTwoPlayerPressed = true;
    mergeUiInputFrame(targetUi, sourceUi);
    expect(targetUi.upPressed && targetUi.downPressed &&
               targetUi.leftPressed && targetUi.rightPressed &&
               targetUi.confirmPressed && targetUi.cancelPressed &&
               targetUi.quitPressed && targetUi.pausePressed &&
               targetUi.restartPressed && targetUi.resetPressed &&
               targetUi.coarseAdjustment &&
               targetUi.selectOnePlayerPressed &&
               targetUi.selectTwoPlayerPressed,
           "UI merge did not OR every application command");

    reporter.beginSuite("gamepad-assignments-compact-menu-and-stable-gameplay");
    GamepadAssignments assignments;
    expect(assignments.playerIndexForPhysicalSlot(0U) == -1 &&
               assignments.playerIndexForPhysicalSlot(4U) == -1 &&
               !assignments.gameplayStable,
           "default or out-of-range assignment was not empty");
    assignments.update({{false, true, false, true}}, false);
    expect(assignments.playerIndexForPhysicalSlot(0U) == -1 &&
               assignments.playerIndexForPhysicalSlot(1U) == 0 &&
               assignments.playerIndexForPhysicalSlot(2U) == -1 &&
               assignments.playerIndexForPhysicalSlot(3U) == 1 &&
               !assignments.gameplayStable,
           "menu did not compact sparse physical slots into P1/P2");
    assignments.update({{true, false, true, true}}, false);
    expect(assignments.playerIndexForPhysicalSlot(0U) == 0 &&
               assignments.playerIndexForPhysicalSlot(1U) == -1 &&
               assignments.playerIndexForPhysicalSlot(2U) == 1 &&
               assignments.playerIndexForPhysicalSlot(3U) == -1,
           "menu assignment did not recompact after availability changed");
    assignments.update({{true, true, false, false}}, true);
    assignments.update({{false, true, true, false}}, true);
    expect(assignments.gameplayStable &&
               assignments.playerIndexForPhysicalSlot(0U) == -1 &&
               assignments.playerIndexForPhysicalSlot(1U) == 1 &&
               assignments.playerIndexForPhysicalSlot(2U) == 0 &&
               assignments.playerIndexForPhysicalSlot(3U) == -1,
           "gameplay moved the surviving player instead of filling only the vacancy");
    assignments.update({{false, true, true, false}}, false);
    expect(!assignments.gameplayStable &&
               assignments.playerIndexForPhysicalSlot(0U) == -1 &&
               assignments.playerIndexForPhysicalSlot(1U) == 0 &&
               assignments.playerIndexForPhysicalSlot(2U) == 1 &&
               assignments.playerIndexForPhysicalSlot(3U) == -1,
           "returning to the menu did not unlock and compact assignments");

    GamepadAssignments directGameplay;
    directGameplay.update({{false, false, true, true}}, true);
    expect(directGameplay.gameplayStable &&
               directGameplay.playerIndexForPhysicalSlot(2U) == 0 &&
               directGameplay.playerIndexForPhysicalSlot(3U) == 1,
           "first gameplay update did not establish a stable compact mapping");

    GamepadAssignments lateConnection;
    lateConnection.update({{false, false, false, false}}, true);
    lateConnection.update({{false, false, true, false}}, true);
    expect(lateConnection.playerIndexForPhysicalSlot(2U) == 0,
           "a controller connected during gameplay did not fill vacant P1");

    reporter.finish();
    return passed ? 0 : 1;
}
