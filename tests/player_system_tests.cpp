#include "game/player_system.h"
#include "test_support.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

namespace
{
using tanks3d::core::CardinalDirection;
using tanks3d::core::XZ;
using tanks3d::game::DirectionButtonFrame;
using tanks3d::game::PlayerControlFrame;
using tanks3d::game::PlayerControlPlan;
using tanks3d::game::PlayerDeathState;
using tanks3d::game::PlayerDeathTransition;
using tanks3d::game::PlayerFireOutcome;
using tanks3d::game::PlayerFireParameters;
using tanks3d::game::PlayerFrameClockState;
using tanks3d::game::PlayerFramePhase;
using tanks3d::game::PlayerMovementParameters;
using tanks3d::game::PlayerMovementState;
using tanks3d::game::PlayerMovementUpdate;
using tanks3d::game::PlayerShellLaunch;
using tanks3d::game::PlayerShellLaunchIntent;
using tanks3d::game::PlayerSpawnOutcome;
using tanks3d::game::PlayerSpawnParameters;
using tanks3d::game::PlayerSpawnProgression;
using tanks3d::game::PlayerSpawnState;
using tanks3d::game::advanceActivePlayerMovement;
using tanks3d::game::advanceInactivePlayerDeath;
using tanks3d::game::advancePlayerFireTransaction;
using tanks3d::game::beginPlayerFrame;
using tanks3d::game::kPlayerTrackDustCooldown;
using tanks3d::game::planPlayerControl;
using tanks3d::game::preparePlayerSpawnState;

constexpr std::array<CardinalDirection, 5> kDirections{{
    CardinalDirection::None, CardinalDirection::North,
    CardinalDirection::South, CardinalDirection::West,
    CardinalDirection::East}};
constexpr std::array<unsigned int, 5> kDirectionBits{{0U, 1U, 2U, 4U, 8U}};

// Test-only golden tables. Pressed directions use the highest set bit;
// held fallback uses the lowest. They deliberately do not mirror the
// production function's ordered branches.
constexpr std::array<CardinalDirection, 16> kPressedWinner{{
    CardinalDirection::None,
    CardinalDirection::North,
    CardinalDirection::South,
    CardinalDirection::South,
    CardinalDirection::West,
    CardinalDirection::West,
    CardinalDirection::West,
    CardinalDirection::West,
    CardinalDirection::East,
    CardinalDirection::East,
    CardinalDirection::East,
    CardinalDirection::East,
    CardinalDirection::East,
    CardinalDirection::East,
    CardinalDirection::East,
    CardinalDirection::East}};
constexpr std::array<CardinalDirection, 16> kHeldFallback{{
    CardinalDirection::None,
    CardinalDirection::North,
    CardinalDirection::South,
    CardinalDirection::North,
    CardinalDirection::West,
    CardinalDirection::North,
    CardinalDirection::South,
    CardinalDirection::North,
    CardinalDirection::East,
    CardinalDirection::North,
    CardinalDirection::South,
    CardinalDirection::North,
    CardinalDirection::West,
    CardinalDirection::North,
    CardinalDirection::South,
    CardinalDirection::North}};

PlayerControlFrame controlFrame(unsigned int heldMask,
                                unsigned int pressedMask,
                                bool fireHeld)
{
    PlayerControlFrame controls;
    std::array<DirectionButtonFrame *, 4> buttons{{
        &controls.north, &controls.south, &controls.west, &controls.east}};
    for (std::size_t index = 0; index < buttons.size(); ++index)
    {
        const unsigned int bit = 1U << index;
        buttons[index]->held = (heldMask & bit) != 0U;
        buttons[index]->pressed = (pressedMask & bit) != 0U;
    }
    controls.fireHeld = fireHeld;
    return controls;
}

bool sameControls(const PlayerControlFrame &first,
                  const PlayerControlFrame &second)
{
    return first.north.held == second.north.held &&
           first.north.pressed == second.north.pressed &&
           first.south.held == second.south.held &&
           first.south.pressed == second.south.pressed &&
           first.west.held == second.west.held &&
           first.west.pressed == second.west.pressed &&
           first.east.held == second.east.held &&
           first.east.pressed == second.east.pressed &&
           first.fireHeld == second.fireHeld;
}

unsigned int directionBit(CardinalDirection direction)
{
    for (std::size_t index = 0; index < kDirections.size(); ++index)
    {
        if (kDirections[index] == direction)
            return kDirectionBits[index];
    }
    return 0U;
}

PlayerControlPlan expectedPlan(CardinalDirection currentDrive,
                               unsigned int heldMask,
                               unsigned int pressedMask,
                               bool fireHeld)
{
    const CardinalDirection pressed = kPressedWinner[pressedMask];
    const CardinalDirection preferred =
        pressed != CardinalDirection::None ? pressed : currentDrive;
    const bool preferredHeld =
        (heldMask & directionBit(preferred)) != 0U;
    const CardinalDirection active =
        preferredHeld ? preferred : kHeldFallback[heldMask];
    const bool propelling = active != CardinalDirection::None;
    return {propelling ? active : preferred, propelling, fireHeld};
}

bool samePlan(const PlayerControlPlan &first,
              const PlayerControlPlan &second)
{
    return first.driveDirection == second.driveDirection &&
           first.propelling == second.propelling &&
           first.fireHeld == second.fireHeld;
}

bool sameFrameClocks(const PlayerFrameClockState &first,
                     const PlayerFrameClockState &second)
{
    return first.creationTimer == second.creationTimer &&
           first.fireCooldown == second.fireCooldown &&
           first.dustCooldown == second.dustCooldown &&
           first.shieldTimer == second.shieldTimer &&
           first.streakPopupTimer == second.streakPopupTimer;
}

bool sameSnapshotFloat(float first, float second)
{
    static_assert(sizeof(float) == sizeof(std::uint32_t),
                  "snapshot comparison requires 32-bit floats");
    std::uint32_t firstBits = 0U;
    std::uint32_t secondBits = 0U;
    std::memcpy(&firstBits, &first, sizeof(firstBits));
    std::memcpy(&secondBits, &second, sizeof(secondBits));
    return firstBits == secondBits;
}

bool sameDeathState(const PlayerDeathState &first,
                    const PlayerDeathState &second)
{
    return sameSnapshotFloat(first.deathTimer, second.deathTimer) &&
           first.lives == second.lives;
}

bool sameSpawnState(const PlayerSpawnState &first,
                    const PlayerSpawnState &second)
{
    return sameSnapshotFloat(first.movement.position.x,
                             second.movement.position.x) &&
           sameSnapshotFloat(first.movement.position.z,
                             second.movement.position.z) &&
           sameSnapshotFloat(first.movement.yaw, second.movement.yaw) &&
           first.movement.driveDirection ==
               second.movement.driveDirection &&
           first.movement.movementDirection ==
               second.movement.movementDirection &&
           first.movement.moving == second.movement.moving &&
           sameSnapshotFloat(first.movement.iceSlipTimer,
                             second.movement.iceSlipTimer) &&
           first.movement.onIce == second.movement.onIce &&
           sameSnapshotFloat(first.clocks.creationTimer,
                             second.clocks.creationTimer) &&
           sameSnapshotFloat(first.clocks.fireCooldown,
                             second.clocks.fireCooldown) &&
           sameSnapshotFloat(first.clocks.dustCooldown,
                             second.clocks.dustCooldown) &&
           sameSnapshotFloat(first.clocks.shieldTimer,
                             second.clocks.shieldTimer) &&
           sameSnapshotFloat(first.clocks.streakPopupTimer,
                             second.clocks.streakPopupTimer) &&
           first.hitPoints == second.hitPoints &&
           first.level == second.level && first.active == second.active &&
           first.hasBoat == second.hasBoat &&
           sameSnapshotFloat(first.respawnTimer, second.respawnTimer) &&
           sameSnapshotFloat(first.deathTimer, second.deathTimer) &&
           first.directKillStreak == second.directKillStreak;
}

bool sameSpawnParameters(const PlayerSpawnParameters &first,
                         const PlayerSpawnParameters &second)
{
    return sameSnapshotFloat(first.spawnPosition.x,
                             second.spawnPosition.x) &&
           sameSnapshotFloat(first.spawnPosition.z,
                             second.spawnPosition.z) &&
           first.maximumHitPoints == second.maximumHitPoints &&
           first.progression == second.progression &&
           sameSnapshotFloat(first.creationDuration,
                             second.creationDuration) &&
           sameSnapshotFloat(first.initialFireCooldown,
                             second.initialFireCooldown) &&
           sameSnapshotFloat(first.shieldDuration,
                             second.shieldDuration);
}

PlayerSpawnState noisySpawnState()
{
    const float infinity = std::numeric_limits<float>::infinity();
    PlayerSpawnState state;
    state.movement.position = {4.125f, -7.75f};
    state.movement.yaw = std::numeric_limits<float>::quiet_NaN();
    state.movement.driveDirection = CardinalDirection::South;
    state.movement.movementDirection = CardinalDirection::East;
    state.movement.moving = true;
    state.movement.iceSlipTimer = -infinity;
    state.movement.onIce = true;
    state.clocks.creationTimer = infinity;
    state.clocks.fireCooldown = -infinity;
    state.clocks.dustCooldown = std::numeric_limits<float>::max();
    state.clocks.shieldTimer = -std::numeric_limits<float>::max();
    state.clocks.streakPopupTimer =
        std::numeric_limits<float>::quiet_NaN();
    state.hitPoints = std::numeric_limits<int>::max();
    state.level = -73;
    state.active = false;
    state.hasBoat = true;
    state.respawnTimer = infinity;
    state.deathTimer = -infinity;
    state.directKillStreak = std::numeric_limits<int>::min();
    return state;
}

PlayerSpawnParameters spawnParametersFixture()
{
    PlayerSpawnParameters parameters;
    parameters.spawnPosition = {18.25f, 24.75f};
    parameters.maximumHitPoints = 6;
    parameters.progression = PlayerSpawnProgression::Preserve;
    parameters.creationDuration = 0.625f;
    parameters.initialFireCooldown = 0.1875f;
    parameters.shieldDuration = 8.5f;
    return parameters;
}

PlayerFrameClockState frameClockFixture()
{
    PlayerFrameClockState clocks;
    clocks.creationTimer = 0.50f;
    clocks.fireCooldown = 0.625f;
    clocks.dustCooldown = 0.375f;
    clocks.shieldTimer = 0.75f;
    clocks.streakPopupTimer = 0.125f;
    return clocks;
}

bool nearlyEqual(float first, float second)
{
    return std::fabs(first - second) < 0.00001f;
}

bool samePosition(XZ first, XZ second)
{
    return nearlyEqual(first.x, second.x) &&
           nearlyEqual(first.z, second.z);
}

bool sameMovementState(const PlayerMovementState &first,
                       const PlayerMovementState &second)
{
    return samePosition(first.position, second.position) &&
           nearlyEqual(first.yaw, second.yaw) &&
           first.driveDirection == second.driveDirection &&
           first.movementDirection == second.movementDirection &&
           first.moving == second.moving &&
           nearlyEqual(first.iceSlipTimer, second.iceSlipTimer) &&
           first.onIce == second.onIce;
}

bool rejectedMovement(const PlayerMovementUpdate &update)
{
    return !update.valid && !update.blocked &&
           !update.movementAccepted && !update.dust.has_value();
}

PlayerMovementState movementStateFixture()
{
    PlayerMovementState state;
    state.position = {4.18f, 7.73f};
    state.yaw = 0.37f;
    state.driveDirection = CardinalDirection::North;
    state.movementDirection = CardinalDirection::North;
    state.moving = false;
    state.iceSlipTimer = 0.20f;
    state.onIce = false;
    return state;
}

PlayerMovementParameters movementParametersFixture()
{
    PlayerMovementParameters parameters;
    parameters.driveDirection = CardinalDirection::East;
    parameters.propelling = true;
    parameters.elapsed = 0.25f;
    parameters.movementSpeed = 2.0f;
    parameters.surfaceIsIce = false;
    parameters.dustCooldown = 0.30f;
    return parameters;
}

PlayerFireParameters fireParametersFixture()
{
    PlayerFireParameters parameters;
    parameters.requested = true;
    parameters.activeShellCount = 0;
    parameters.playerIndex = 7;
    parameters.playerLevel = 0;
    parameters.tankPosition = {3.25f, 8.5f};
    parameters.direction = CardinalDirection::East;
    parameters.reloadInterval = 0.120f;
    parameters.shellSpawnDistance = 0.625f;
    return parameters;
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

    reporter.beginSuite("player-frame-invalid-elapsed-is-atomic");
    const float frameNaN = std::numeric_limits<float>::quiet_NaN();
    const float frameInfinity = std::numeric_limits<float>::infinity();
    bool invalidFrameElapsedWasAtomic = true;
    for (float elapsed : {-0.01f, frameNaN, frameInfinity,
                          -frameInfinity})
    {
        const PlayerFrameClockState input = frameClockFixture();
        const auto frame = beginPlayerFrame(true, input, elapsed);
        invalidFrameElapsedWasAtomic = invalidFrameElapsedWasAtomic &&
            !frame.valid && frame.phase == PlayerFramePhase::Invalid &&
            sameFrameClocks(frame.clocks, input);
    }
    expect(invalidFrameElapsedWasAtomic,
           "invalid elapsed time changed clocks or returned a live phase");

    reporter.beginSuite("player-frame-signed-zero-phase-matrix");
    struct ZeroFrameCase
    {
        bool active;
        float creationTimer;
        PlayerFramePhase phase;
    };
    const std::array<ZeroFrameCase, 3> zeroFrameCases{{
        {false, 0.50f, PlayerFramePhase::Inactive},
        {true, 0.25f, PlayerFramePhase::Creating},
        {true, 0.0f, PlayerFramePhase::Ready}}};
    bool signedZeroFrameParity = true;
    int signedZeroFrameCases = 0;
    for (float elapsed : {0.0f, -0.0f})
    {
        for (const ZeroFrameCase &testCase : zeroFrameCases)
        {
            PlayerFrameClockState input = frameClockFixture();
            input.creationTimer = testCase.creationTimer;
            const auto frame = beginPlayerFrame(
                testCase.active, input, elapsed);
            signedZeroFrameParity = signedZeroFrameParity && frame.valid &&
                frame.phase == testCase.phase &&
                sameFrameClocks(frame.clocks, input);
            ++signedZeroFrameCases;
        }
    }
    expect(signedZeroFrameCases == 6 && signedZeroFrameParity,
           "positive/negative zero changed clocks or selected a wrong phase");

    reporter.beginSuite("player-frame-phase-clock-policy");
    const PlayerFrameClockState phaseInput = frameClockFixture();
    const auto inactiveFrame = beginPlayerFrame(false, phaseInput, 0.25f);
    PlayerFrameClockState expectedInactive = phaseInput;
    expectedInactive.dustCooldown = 0.125f;
    expectedInactive.shieldTimer = 0.50f;
    expectedInactive.streakPopupTimer = 0.0f;
    expect(inactiveFrame.valid &&
               inactiveFrame.phase == PlayerFramePhase::Inactive &&
               sameFrameClocks(inactiveFrame.clocks, expectedInactive),
           "inactive frame advanced creation/fire or missed shared clocks");

    PlayerFrameClockState creatingInput = frameClockFixture();
    creatingInput.creationTimer = 0.125f;
    const auto creatingFrame = beginPlayerFrame(
        true, creatingInput, 0.25f);
    PlayerFrameClockState expectedCreating = creatingInput;
    expectedCreating.creationTimer = 0.0f;
    expectedCreating.dustCooldown = 0.125f;
    expectedCreating.shieldTimer = 0.50f;
    expectedCreating.streakPopupTimer = 0.0f;
    expect(creatingFrame.valid &&
               creatingFrame.phase == PlayerFramePhase::Creating &&
               sameFrameClocks(creatingFrame.clocks, expectedCreating),
           "creation overshoot fell through or advanced the fire clock");

    PlayerFrameClockState readyInput = frameClockFixture();
    readyInput.creationTimer = 0.0f;
    const auto readyFrame = beginPlayerFrame(true, readyInput, 0.25f);
    PlayerFrameClockState expectedReady = readyInput;
    expectedReady.fireCooldown = 0.375f;
    expectedReady.dustCooldown = 0.125f;
    expectedReady.shieldTimer = 0.50f;
    expectedReady.streakPopupTimer = 0.0f;
    expect(readyFrame.valid && readyFrame.phase == PlayerFramePhase::Ready &&
               sameFrameClocks(readyFrame.clocks, expectedReady),
           "ready frame missed reload or shared-clock debit policy");

    reporter.beginSuite("player-frame-finite-overshoot-clamps");
    PlayerFrameClockState overshootInput;
    overshootInput.creationTimer = 0.0f;
    overshootInput.fireCooldown = 0.125f;
    overshootInput.dustCooldown = 0.125f;
    overshootInput.shieldTimer = 0.125f;
    overshootInput.streakPopupTimer = 0.125f;
    const auto overshootFrame = beginPlayerFrame(
        true, overshootInput, 0.25f);
    const PlayerFrameClockState zeroClocks{};
    expect(overshootFrame.valid &&
               overshootFrame.phase == PlayerFramePhase::Ready &&
               sameFrameClocks(overshootFrame.clocks, zeroClocks),
           "finite clock overshoot produced a negative countdown");

    reporter.beginSuite("player-frame-malformed-clock-parity");
    PlayerFrameClockState malformedDebits;
    malformedDebits.creationTimer = 0.0f;
    malformedDebits.fireCooldown = frameNaN;
    malformedDebits.dustCooldown = -frameInfinity;
    malformedDebits.shieldTimer = frameInfinity;
    malformedDebits.streakPopupTimer = frameNaN;
    const auto malformedDebitFrame = beginPlayerFrame(
        true, malformedDebits, 0.0f);
    expect(malformedDebitFrame.valid &&
               malformedDebitFrame.phase == PlayerFramePhase::Ready &&
               malformedDebitFrame.clocks.creationTimer == 0.0f &&
               malformedDebitFrame.clocks.fireCooldown == 0.0f &&
               malformedDebitFrame.clocks.dustCooldown == 0.0f &&
               std::isinf(malformedDebitFrame.clocks.shieldTimer) &&
               !std::signbit(malformedDebitFrame.clocks.shieldTimer) &&
               malformedDebitFrame.clocks.streakPopupTimer == 0.0f,
           "ordered countdown clamps changed malformed-clock parity");

    struct MalformedCreationCase
    {
        float creationTimer;
        PlayerFramePhase phase;
    };
    const std::array<MalformedCreationCase, 5> malformedCreationCases{{
        {frameNaN, PlayerFramePhase::Ready},
        {frameInfinity, PlayerFramePhase::Creating},
        {-frameInfinity, PlayerFramePhase::Ready},
        {-0.0f, PlayerFramePhase::Ready},
        {-0.25f, PlayerFramePhase::Ready}}};
    bool malformedCreationParity = true;
    for (const MalformedCreationCase &testCase : malformedCreationCases)
    {
        PlayerFrameClockState input = frameClockFixture();
        input.creationTimer = testCase.creationTimer;
        const auto frame = beginPlayerFrame(true, input, 0.0f);
        const bool creationPreserved = std::isnan(testCase.creationTimer)
            ? std::isnan(frame.clocks.creationTimer)
            : frame.clocks.creationTimer == testCase.creationTimer &&
                std::signbit(frame.clocks.creationTimer) ==
                    std::signbit(testCase.creationTimer);
        malformedCreationParity = malformedCreationParity && frame.valid &&
            frame.phase == testCase.phase && creationPreserved &&
            frame.clocks.fireCooldown == input.fireCooldown &&
            frame.clocks.dustCooldown == input.dustCooldown &&
            frame.clocks.shieldTimer == input.shieldTimer &&
            frame.clocks.streakPopupTimer == input.streakPopupTimer;
    }
    expect(malformedCreationParity,
           "creation entry gate validated or normalized malformed clocks");

    reporter.beginSuite("player-death-invalid-elapsed-is-atomic");
    struct InvalidDeathEntryCase
    {
        const char *label;
        PlayerDeathState state;
    };
    const std::array<InvalidDeathEntryCase, 5> invalidDeathEntries{{
        {"pending", {0.50f, 2}},
        {"positive-zero", {0.0f, std::numeric_limits<int>::min()}},
        {"negative-zero", {-0.0f, std::numeric_limits<int>::min()}},
        {"negative", {-0.25f, -1}},
        {"negative-infinity", {-frameInfinity,
                                std::numeric_limits<int>::min()}}}};
    struct InvalidDeathElapsedCase
    {
        const char *label;
        float elapsed;
    };
    const std::array<InvalidDeathElapsedCase, 4> invalidDeathElapsedCases{{
        {"negative", -0.01f},
        {"NaN", frameNaN},
        {"positive-infinity", frameInfinity},
        {"negative-infinity", -frameInfinity}}};
    std::string firstInvalidDeathMismatch;
    for (const InvalidDeathEntryCase &entry : invalidDeathEntries)
    {
        for (const InvalidDeathElapsedCase &elapsedCase :
             invalidDeathElapsedCases)
        {
            PlayerDeathState state = entry.state;
            const PlayerDeathTransition transition =
                advanceInactivePlayerDeath(state, elapsedCase.elapsed);
            if (firstInvalidDeathMismatch.empty() &&
                (transition != PlayerDeathTransition::Invalid ||
                 !sameDeathState(state, entry.state)))
            {
                firstInvalidDeathMismatch = std::string("timer=") +
                    entry.label + ", elapsed=" + elapsedCase.label;
            }
        }
    }
    expect(firstInvalidDeathMismatch.empty(),
           "invalid elapsed lost priority or changed death state at " +
               firstInvalidDeathMismatch);

    reporter.beginSuite("player-death-complete-entry-is-idempotent");
    const std::array<PlayerDeathState, 3> completeDeathCases{{
        {0.0f, 2},
        {-0.0f, std::numeric_limits<int>::min()},
        {-0.25f, -1}}};
    bool completeDeathWasIdempotent = true;
    for (const PlayerDeathState &input : completeDeathCases)
    {
        PlayerDeathState state = input;
        const PlayerDeathTransition first = advanceInactivePlayerDeath(
            state, std::numeric_limits<float>::max());
        const PlayerDeathTransition second = advanceInactivePlayerDeath(
            state, std::numeric_limits<float>::max());
        completeDeathWasIdempotent = completeDeathWasIdempotent &&
            first == PlayerDeathTransition::None &&
            second == PlayerDeathTransition::None &&
            sameDeathState(state, input);
    }
    expect(completeDeathWasIdempotent,
           "completed entry debited a life or normalized its timer");

    reporter.beginSuite("player-death-signed-zero-waits");
    bool signedZeroDeathWaitParity = true;
    for (float elapsed : {0.0f, -0.0f})
    {
        const PlayerDeathState input{0.50f, 2};
        PlayerDeathState state = input;
        const PlayerDeathTransition transition =
            advanceInactivePlayerDeath(state, elapsed);
        signedZeroDeathWaitParity = signedZeroDeathWaitParity &&
            transition == PlayerDeathTransition::Waiting &&
            sameDeathState(state, input);
    }
    expect(signedZeroDeathWaitParity,
           "signed-zero elapsed completed or changed a pending death");

    reporter.beginSuite("player-death-threshold-and-maximum-boundaries");
    constexpr float deathBoundary = 0.50f;
    PlayerDeathState belowBoundary{deathBoundary, 2};
    const float justBelowBoundary = std::nextafter(
        deathBoundary, 0.0f);
    const PlayerDeathTransition belowTransition =
        advanceInactivePlayerDeath(belowBoundary, justBelowBoundary);
    expect(belowTransition == PlayerDeathTransition::Waiting &&
               belowBoundary.deathTimer ==
                   deathBoundary - justBelowBoundary &&
               belowBoundary.deathTimer > 0.0f &&
               belowBoundary.lives == 2,
           "one-ULP-below elapsed did not leave a positive wait remainder");

    PlayerDeathState exactBoundary{deathBoundary, 2};
    const PlayerDeathTransition exactTransition =
        advanceInactivePlayerDeath(exactBoundary, deathBoundary);
    PlayerDeathState aboveBoundary{deathBoundary, 2};
    const PlayerDeathTransition aboveTransition =
        advanceInactivePlayerDeath(
            aboveBoundary,
            std::nextafter(deathBoundary, frameInfinity));
    expect(exactTransition == PlayerDeathTransition::Respawn &&
               sameDeathState(exactBoundary, {0.0f, 1}) &&
               aboveTransition == PlayerDeathTransition::Respawn &&
               sameDeathState(aboveBoundary, {0.0f, 1}),
           "exact or one-ULP overshoot missed same-frame life debit");

    const float maximumFinite = std::numeric_limits<float>::max();
    PlayerDeathState maximumBoundary{maximumFinite, 2};
    const PlayerDeathTransition maximumTransition =
        advanceInactivePlayerDeath(maximumBoundary, maximumFinite);
    expect(maximumTransition == PlayerDeathTransition::Respawn &&
               sameDeathState(maximumBoundary, {0.0f, 1}),
           "FLT_MAX exact completion failed to clamp and respawn");

    reporter.beginSuite("player-death-life-saturation-matrix");
    struct DeathLifeCase
    {
        int inputLives;
        int expectedLives;
        PlayerDeathTransition transition;
    };
    const std::array<DeathLifeCase, 6> deathLifeCases{{
        {std::numeric_limits<int>::min(), 0,
         PlayerDeathTransition::Eliminated},
        {-1, 0, PlayerDeathTransition::Eliminated},
        {0, 0, PlayerDeathTransition::Eliminated},
        {1, 0, PlayerDeathTransition::Eliminated},
        {2, 1, PlayerDeathTransition::Respawn},
        {std::numeric_limits<int>::max(),
         std::numeric_limits<int>::max() - 1,
         PlayerDeathTransition::Respawn}}};
    std::string firstDeathLifeMismatch;
    for (const DeathLifeCase &testCase : deathLifeCases)
    {
        PlayerDeathState state{0.125f, testCase.inputLives};
        const PlayerDeathTransition transition =
            advanceInactivePlayerDeath(state, 0.25f);
        if (firstDeathLifeMismatch.empty() &&
            (transition != testCase.transition ||
             !sameDeathState(state, {0.0f, testCase.expectedLives})))
        {
            firstDeathLifeMismatch =
                " input lives=" + std::to_string(testCase.inputLives);
        }
    }
    expect(firstDeathLifeMismatch.empty(),
           "life debit did not saturate or choose the expected transition at" +
               firstDeathLifeMismatch);

    reporter.beginSuite("player-death-malformed-timer-parity");
    PlayerDeathState nanDeath{frameNaN, 2};
    const PlayerDeathTransition nanDeathTransition =
        advanceInactivePlayerDeath(nanDeath, 0.0f);
    expect(nanDeathTransition == PlayerDeathTransition::Respawn &&
               sameDeathState(nanDeath, {0.0f, 1}),
           "NaN death timer no longer completes through ordered max");

    PlayerDeathState positiveInfiniteDeath{frameInfinity, 2};
    const PlayerDeathTransition positiveInfiniteTransition =
        advanceInactivePlayerDeath(positiveInfiniteDeath, maximumFinite);
    expect(positiveInfiniteTransition == PlayerDeathTransition::Waiting &&
               std::isinf(positiveInfiniteDeath.deathTimer) &&
               !std::signbit(positiveInfiniteDeath.deathTimer) &&
               positiveInfiniteDeath.lives == 2,
           "+Inf death timer no longer remains pending");

    const PlayerDeathState negativeInfiniteInput{-frameInfinity, 2};
    PlayerDeathState negativeInfiniteDeath = negativeInfiniteInput;
    const PlayerDeathTransition negativeInfiniteTransition =
        advanceInactivePlayerDeath(negativeInfiniteDeath, maximumFinite);
    expect(negativeInfiniteTransition == PlayerDeathTransition::None &&
               sameDeathState(negativeInfiniteDeath, negativeInfiniteInput),
           "-Inf death timer no longer returns None unchanged");

    reporter.beginSuite("player-spawn-invalid-input-is-atomic");
    const PlayerSpawnState invalidSpawnInput = noisySpawnState();
    std::string firstInvalidSpawnMismatch;
    const auto probeInvalidSpawn =
        [&](const std::string &label, PlayerSpawnParameters parameters) {
            const PlayerSpawnParameters parametersBefore = parameters;
            PlayerSpawnState state = invalidSpawnInput;
            const PlayerSpawnOutcome outcome =
                preparePlayerSpawnState(state, parameters);
            if (firstInvalidSpawnMismatch.empty() &&
                (outcome != PlayerSpawnOutcome::Invalid ||
                 !sameSpawnState(state, invalidSpawnInput) ||
                 !sameSpawnParameters(parameters, parametersBefore)))
            {
                firstInvalidSpawnMismatch = label;
            }
        };
    const auto probeInvalidSpawnInBothModes =
        [&](const std::string &label, PlayerSpawnParameters parameters) {
            for (PlayerSpawnProgression progression : {
                     PlayerSpawnProgression::Preserve,
                     PlayerSpawnProgression::Reset})
            {
                parameters.progression = progression;
                probeInvalidSpawn(
                    label + "/" +
                        (progression == PlayerSpawnProgression::Preserve
                             ? "Preserve"
                             : "Reset"),
                    parameters);
            }
        };
    struct InvalidSpawnScalarCase
    {
        const char *label;
        float value;
    };
    const std::array<InvalidSpawnScalarCase, 3> invalidCoordinateCases{{
        {"NaN", frameNaN},
        {"positive-infinity", frameInfinity},
        {"negative-infinity", -frameInfinity}}};
    for (const InvalidSpawnScalarCase &testCase : invalidCoordinateCases)
    {
        PlayerSpawnParameters parameters = spawnParametersFixture();
        parameters.spawnPosition.x = testCase.value;
        probeInvalidSpawnInBothModes(
            std::string("position.x/") + testCase.label, parameters);
        parameters = spawnParametersFixture();
        parameters.spawnPosition.z = testCase.value;
        probeInvalidSpawnInBothModes(
            std::string("position.z/") + testCase.label, parameters);
    }
    const std::array<InvalidSpawnScalarCase, 4> invalidDurationCases{{
        {"negative", -0.01f},
        {"NaN", frameNaN},
        {"positive-infinity", frameInfinity},
        {"negative-infinity", -frameInfinity}}};
    for (const InvalidSpawnScalarCase &testCase : invalidDurationCases)
    {
        PlayerSpawnParameters parameters = spawnParametersFixture();
        parameters.creationDuration = testCase.value;
        probeInvalidSpawnInBothModes(
            std::string("creation/") + testCase.label, parameters);
        parameters = spawnParametersFixture();
        parameters.initialFireCooldown = testCase.value;
        probeInvalidSpawnInBothModes(
            std::string("fire/") + testCase.label, parameters);
        parameters = spawnParametersFixture();
        parameters.shieldDuration = testCase.value;
        probeInvalidSpawnInBothModes(
            std::string("shield/") + testCase.label, parameters);
    }
    for (unsigned int rawMode : {2U, 255U})
    {
        PlayerSpawnParameters parameters = spawnParametersFixture();
        parameters.progression =
            static_cast<PlayerSpawnProgression>(rawMode);
        probeInvalidSpawn(
            "progression=" + std::to_string(rawMode), parameters);
    }
    expect(firstInvalidSpawnMismatch.empty(),
           "invalid spawn input mutated state/parameters or prepared at " +
               firstInvalidSpawnMismatch);

    reporter.beginSuite("player-spawn-preserve-full-write-mask");
    const PlayerSpawnState preserveInput = noisySpawnState();
    PlayerSpawnState preserveState = preserveInput;
    PlayerSpawnParameters preserveParameters = spawnParametersFixture();
    preserveParameters.maximumHitPoints = -91;
    const PlayerSpawnParameters preserveParametersBefore =
        preserveParameters;
    const PlayerSpawnOutcome preserveOutcome = preparePlayerSpawnState(
        preserveState, preserveParameters);
    PlayerSpawnState expectedPreserve;
    expectedPreserve.movement.position = {18.25f, 24.75f};
    expectedPreserve.movement.yaw = 0.0f;
    expectedPreserve.movement.driveDirection = CardinalDirection::North;
    expectedPreserve.movement.movementDirection = CardinalDirection::North;
    expectedPreserve.movement.moving = false;
    expectedPreserve.movement.iceSlipTimer = 0.0f;
    expectedPreserve.movement.onIce = false;
    expectedPreserve.clocks.creationTimer = 0.625f;
    expectedPreserve.clocks.fireCooldown = 0.1875f;
    expectedPreserve.clocks.dustCooldown = 0.0f;
    expectedPreserve.clocks.shieldTimer = 8.5f;
    expectedPreserve.clocks.streakPopupTimer =
        preserveInput.clocks.streakPopupTimer;
    expectedPreserve.hitPoints = std::numeric_limits<int>::max();
    expectedPreserve.level = -73;
    expectedPreserve.active = true;
    expectedPreserve.hasBoat = false;
    expectedPreserve.respawnTimer = 0.0f;
    expectedPreserve.deathTimer = 0.0f;
    expectedPreserve.directKillStreak = std::numeric_limits<int>::min();
    expect(preserveOutcome == PlayerSpawnOutcome::Prepared &&
               sameSpawnState(preserveState, expectedPreserve) &&
               sameSpawnParameters(preserveParameters,
                                   preserveParametersBefore),
           "Preserve did not apply the exact 19-field spawn write mask");

    reporter.beginSuite("player-spawn-reset-full-write-mask");
    const PlayerSpawnState resetInput = noisySpawnState();
    PlayerSpawnState resetState = resetInput;
    PlayerSpawnParameters resetParameters = spawnParametersFixture();
    resetParameters.progression = PlayerSpawnProgression::Reset;
    const PlayerSpawnParameters resetParametersBefore = resetParameters;
    const PlayerSpawnOutcome resetOutcome = preparePlayerSpawnState(
        resetState, resetParameters);
    PlayerSpawnState expectedReset;
    expectedReset.movement.position = {18.25f, 24.75f};
    expectedReset.movement.yaw = 0.0f;
    expectedReset.movement.driveDirection = CardinalDirection::North;
    expectedReset.movement.movementDirection = CardinalDirection::North;
    expectedReset.movement.moving = false;
    expectedReset.movement.iceSlipTimer = 0.0f;
    expectedReset.movement.onIce = false;
    expectedReset.clocks.creationTimer = 0.625f;
    expectedReset.clocks.fireCooldown = 0.1875f;
    expectedReset.clocks.dustCooldown = 0.0f;
    expectedReset.clocks.shieldTimer = 8.5f;
    expectedReset.clocks.streakPopupTimer = 0.0f;
    expectedReset.hitPoints = 6;
    expectedReset.level = 0;
    expectedReset.active = true;
    expectedReset.hasBoat = false;
    expectedReset.respawnTimer = 0.0f;
    expectedReset.deathTimer = 0.0f;
    expectedReset.directKillStreak = 0;
    expect(resetOutcome == PlayerSpawnOutcome::Prepared &&
               sameSpawnState(resetState, expectedReset) &&
               sameSpawnParameters(resetParameters, resetParametersBefore),
           "Reset did not apply the exact 19-field spawn write mask");

    reporter.beginSuite("player-spawn-hit-point-progression-matrix");
    struct SpawnHitPointCase
    {
        int inputHitPoints;
        int maximumHitPoints;
        int preserveHitPoints;
        int resetHitPoints;
    };
    const std::array<SpawnHitPointCase, 5> spawnHitPointCases{{
        {std::numeric_limits<int>::min(),
         std::numeric_limits<int>::max(),
         std::numeric_limits<int>::max(),
         std::numeric_limits<int>::max()},
        {-1, std::numeric_limits<int>::min(),
         std::numeric_limits<int>::min(),
         std::numeric_limits<int>::min()},
        {0, -1, -1, -1},
        {1, 0, 1, 0},
        {std::numeric_limits<int>::max(), 1,
         std::numeric_limits<int>::max(), 1}}};
    std::string firstSpawnHitPointMismatch;
    for (const SpawnHitPointCase &testCase : spawnHitPointCases)
    {
        for (PlayerSpawnProgression progression : {
                 PlayerSpawnProgression::Preserve,
                 PlayerSpawnProgression::Reset})
        {
            PlayerSpawnState state = noisySpawnState();
            state.hitPoints = testCase.inputHitPoints;
            PlayerSpawnParameters parameters = spawnParametersFixture();
            parameters.maximumHitPoints = testCase.maximumHitPoints;
            parameters.progression = progression;
            const PlayerSpawnParameters parametersBefore = parameters;
            const PlayerSpawnOutcome outcome = preparePlayerSpawnState(
                state, parameters);
            const int expectedHitPoints =
                progression == PlayerSpawnProgression::Preserve
                ? testCase.preserveHitPoints
                : testCase.resetHitPoints;
            if (firstSpawnHitPointMismatch.empty() &&
                (outcome != PlayerSpawnOutcome::Prepared ||
                 state.hitPoints != expectedHitPoints ||
                 !sameSpawnParameters(parameters, parametersBefore)))
            {
                firstSpawnHitPointMismatch =
                    std::string("mode=") +
                    (progression == PlayerSpawnProgression::Preserve
                         ? "Preserve"
                         : "Reset") +
                    ", hp=" + std::to_string(testCase.inputHitPoints) +
                    ", maximum=" +
                    std::to_string(testCase.maximumHitPoints);
            }
        }
    }
    expect(firstSpawnHitPointMismatch.empty(),
           "spawn HP legacy copy/comparison changed at " +
               firstSpawnHitPointMismatch);

    reporter.beginSuite("player-spawn-signed-zero-is-valid");
    std::string firstSpawnZeroMismatch;
    for (float zero : {0.0f, -0.0f})
    {
        PlayerSpawnState state = noisySpawnState();
        PlayerSpawnParameters parameters = spawnParametersFixture();
        parameters.spawnPosition = {zero, zero};
        parameters.creationDuration = zero;
        parameters.initialFireCooldown = zero;
        parameters.shieldDuration = zero;
        const PlayerSpawnParameters parametersBefore = parameters;
        const PlayerSpawnOutcome outcome = preparePlayerSpawnState(
            state, parameters);
        const bool assignedParameterValuesExactly =
            sameSnapshotFloat(state.movement.position.x, zero) &&
            sameSnapshotFloat(state.movement.position.z, zero) &&
            sameSnapshotFloat(state.clocks.creationTimer, zero) &&
            sameSnapshotFloat(state.clocks.fireCooldown, zero) &&
            sameSnapshotFloat(state.clocks.shieldTimer, zero);
        if (firstSpawnZeroMismatch.empty() &&
            (outcome != PlayerSpawnOutcome::Prepared ||
             !assignedParameterValuesExactly ||
             !sameSpawnParameters(parameters, parametersBefore)))
        {
            firstSpawnZeroMismatch =
                std::signbit(zero) ? "negative zero" : "positive zero";
        }
    }
    expect(firstSpawnZeroMismatch.empty(),
           "signed-zero spawn input was rejected, normalized, or mutated at " +
               firstSpawnZeroMismatch);

    reporter.beginSuite("player-spawn-maximum-finite-is-valid");
    PlayerSpawnState maximumFiniteSpawnState = noisySpawnState();
    PlayerSpawnParameters maximumFiniteSpawnParameters =
        spawnParametersFixture();
    maximumFiniteSpawnParameters.spawnPosition =
        {maximumFinite, -maximumFinite};
    maximumFiniteSpawnParameters.progression =
        PlayerSpawnProgression::Reset;
    maximumFiniteSpawnParameters.creationDuration = maximumFinite;
    maximumFiniteSpawnParameters.initialFireCooldown = maximumFinite;
    maximumFiniteSpawnParameters.shieldDuration = maximumFinite;
    const PlayerSpawnParameters maximumFiniteSpawnParametersBefore =
        maximumFiniteSpawnParameters;
    const PlayerSpawnOutcome maximumFiniteSpawnOutcome =
        preparePlayerSpawnState(maximumFiniteSpawnState,
                                maximumFiniteSpawnParameters);
    expect(maximumFiniteSpawnOutcome == PlayerSpawnOutcome::Prepared &&
               sameSnapshotFloat(maximumFiniteSpawnState.movement.position.x,
                                 maximumFinite) &&
               sameSnapshotFloat(maximumFiniteSpawnState.movement.position.z,
                                 -maximumFinite) &&
               sameSnapshotFloat(
                   maximumFiniteSpawnState.clocks.creationTimer,
                   maximumFinite) &&
               sameSnapshotFloat(
                   maximumFiniteSpawnState.clocks.fireCooldown,
                   maximumFinite) &&
               sameSnapshotFloat(maximumFiniteSpawnState.clocks.shieldTimer,
                                 maximumFinite) &&
               sameSpawnParameters(maximumFiniteSpawnParameters,
                                   maximumFiniteSpawnParametersBefore),
           "maximum finite spawn position or duration was rejected, changed, "
           "or mutated");

    reporter.beginSuite("player-control-fixed-priority-and-intent");
    expect(samePlan(
               planPlayerControl(CardinalDirection::East, {}),
               {CardinalDirection::East, false, false}),
           "empty input did not preserve facing without propulsion");
    expect(samePlan(
               planPlayerControl(CardinalDirection::North,
                                 controlFrame(15U, 15U, false)),
               {CardinalDirection::East, true, false}),
           "all pressed directions did not give East final priority");
    expect(samePlan(
               planPlayerControl(CardinalDirection::East,
                                 controlFrame(9U, 1U, false)),
               {CardinalDirection::North, true, false}),
           "new North press did not override held East");
    expect(samePlan(
               planPlayerControl(CardinalDirection::East,
                                 controlFrame(15U, 0U, false)),
               {CardinalDirection::East, true, false}),
           "selected held direction lost stickiness");
    expect(samePlan(
               planPlayerControl(CardinalDirection::East,
                                 controlFrame(7U, 0U, false)),
               {CardinalDirection::North, true, false}),
           "released selection did not use North-first held fallback");
    expect(samePlan(
               planPlayerControl(CardinalDirection::North,
                                 controlFrame(0U, 8U, false)),
               {CardinalDirection::East, false, false}),
           "pressed-only direction did not change facing without propulsion");
    expect(samePlan(
               planPlayerControl(CardinalDirection::West,
                                 controlFrame(3U, 8U, false)),
               {CardinalDirection::North, true, false}),
           "unheld pressed winner did not defer to held fallback");
    expect(samePlan(
               planPlayerControl(CardinalDirection::South,
                                 controlFrame(0U, 0U, true)),
               {CardinalDirection::South, false, true}),
           "fire intent no longer passes independently of propulsion");

    reporter.beginSuite("player-control-exhaustive-input-matrix");
    int visited = 0;
    std::string firstOutputMismatch;
    std::string firstInputMutation;
    for (std::size_t directionIndex = 0;
         directionIndex < kDirections.size(); ++directionIndex)
    {
        for (unsigned int heldMask = 0U; heldMask < 16U; ++heldMask)
        {
            for (unsigned int pressedMask = 0U;
                 pressedMask < 16U; ++pressedMask)
            {
                for (int fire = 0; fire < 2; ++fire)
                {
                    PlayerControlFrame controls = controlFrame(
                        heldMask, pressedMask, fire != 0);
                    const PlayerControlFrame before = controls;
                    const PlayerControlPlan actual = planPlayerControl(
                        kDirections[directionIndex], controls);
                    const PlayerControlPlan expected = expectedPlan(
                        kDirections[directionIndex], heldMask,
                        pressedMask, fire != 0);
                    const std::string label =
                        "direction=" + std::to_string(directionIndex) +
                        ", held=" + std::to_string(heldMask) +
                        ", pressed=" + std::to_string(pressedMask) +
                        ", fire=" + std::to_string(fire);
                    if (!samePlan(actual, expected) &&
                        firstOutputMismatch.empty())
                    {
                        firstOutputMismatch = label;
                    }
                    if (!sameControls(controls, before) &&
                        firstInputMutation.empty())
                    {
                        firstInputMutation = label;
                    }
                    ++visited;
                }
            }
        }
    }
    expect(visited == 2560,
           "exhaustive matrix did not visit exactly 2,560 cases");
    expect(firstOutputMismatch.empty(),
           "exhaustive plan mismatch at " + firstOutputMismatch);
    expect(firstInputMutation.empty(),
           "planner mutated input at " + firstInputMutation);

    reporter.beginSuite("player-control-invalid-current-direction-parity");
    const auto invalidFive = static_cast<CardinalDirection>(5U);
    PlayerControlFrame invalidFire = controlFrame(0U, 0U, true);
    const PlayerControlFrame invalidFireBefore = invalidFire;
    expect(samePlan(planPlayerControl(invalidFive, invalidFire),
                    {invalidFive, false, true}) &&
               sameControls(invalidFire, invalidFireBefore),
           "invalid current direction without input lost legacy parity");
    const auto invalidMaximum = static_cast<CardinalDirection>(255U);
    PlayerControlFrame invalidFallback = controlFrame(4U, 0U, false);
    const PlayerControlFrame invalidFallbackBefore = invalidFallback;
    expect(samePlan(planPlayerControl(invalidMaximum, invalidFallback),
                    {CardinalDirection::West, true, false}) &&
               sameControls(invalidFallback, invalidFallbackBefore),
           "invalid current direction did not recover through held fallback");

    reporter.beginSuite("player-movement-invalid-input-is-atomic");
    const float movementNaN = std::numeric_limits<float>::quiet_NaN();
    const float movementInfinity = std::numeric_limits<float>::infinity();
    int invalidMovementQueries = 0;
    const auto countingAvailability = [&](XZ) {
        ++invalidMovementQueries;
        return true;
    };
    bool invalidMovementWasAtomic = true;
    for (float elapsed : {-0.01f, movementNaN, movementInfinity,
                          -movementInfinity})
    {
        PlayerMovementState state = movementStateFixture();
        const PlayerMovementState before = state;
        PlayerMovementParameters parameters = movementParametersFixture();
        parameters.elapsed = elapsed;
        const PlayerMovementUpdate update = advanceActivePlayerMovement(
            state, parameters, countingAvailability);
        invalidMovementWasAtomic = invalidMovementWasAtomic &&
            rejectedMovement(update) && sameMovementState(state, before);
    }
    for (float speed : {-0.01f, movementNaN, movementInfinity,
                        -movementInfinity})
    {
        PlayerMovementState state = movementStateFixture();
        const PlayerMovementState before = state;
        PlayerMovementParameters parameters = movementParametersFixture();
        parameters.movementSpeed = speed;
        const PlayerMovementUpdate update = advanceActivePlayerMovement(
            state, parameters, countingAvailability);
        invalidMovementWasAtomic = invalidMovementWasAtomic &&
            rejectedMovement(update) && sameMovementState(state, before);
    }
    PlayerMovementState emptyQueryState = movementStateFixture();
    const PlayerMovementState emptyQueryBefore = emptyQueryState;
    const PlayerMovementUpdate emptyQueryUpdate =
        advanceActivePlayerMovement(
            emptyQueryState, movementParametersFixture(), {});
    PlayerMovementState overflowState = movementStateFixture();
    const PlayerMovementState overflowBefore = overflowState;
    PlayerMovementParameters overflowParameters =
        movementParametersFixture();
    overflowParameters.elapsed = std::numeric_limits<float>::max();
    overflowParameters.movementSpeed = 2.0f;
    const PlayerMovementUpdate overflowUpdate =
        advanceActivePlayerMovement(
            overflowState, overflowParameters, countingAvailability);
    expect(invalidMovementWasAtomic &&
               rejectedMovement(emptyQueryUpdate) &&
               sameMovementState(emptyQueryState, emptyQueryBefore) &&
               rejectedMovement(overflowUpdate) &&
               sameMovementState(overflowState, overflowBefore) &&
               invalidMovementQueries == 0,
           "invalid/overflowing movement changed state or queried availability");

    reporter.beginSuite("player-movement-facing-and-cardinal-commit");
    PlayerMovementState stationaryState = movementStateFixture();
    stationaryState.position = {2.25f, 6.75f};
    stationaryState.yaw = 0.73f;
    stationaryState.driveDirection = CardinalDirection::North;
    stationaryState.movementDirection = CardinalDirection::West;
    stationaryState.moving = false;
    stationaryState.iceSlipTimer = 0.30f;
    stationaryState.onIce = true;
    PlayerMovementParameters stationaryParameters =
        movementParametersFixture();
    stationaryParameters.driveDirection = CardinalDirection::West;
    stationaryParameters.propelling = false;
    stationaryParameters.surfaceIsIce = false;
    stationaryParameters.dustCooldown = 0.0f;
    int stationaryQueries = 0;
    const PlayerMovementUpdate stationaryUpdate =
        advanceActivePlayerMovement(
            stationaryState, stationaryParameters,
            [&](XZ) {
                ++stationaryQueries;
                return false;
            });
    expect(stationaryUpdate.valid && !stationaryUpdate.blocked &&
               !stationaryUpdate.movementAccepted &&
               !stationaryUpdate.dust.has_value() &&
               stationaryQueries == 0 &&
               samePosition(stationaryState.position, {2.25f, 6.75f}) &&
               nearlyEqual(stationaryState.yaw, 0.73f) &&
               stationaryState.driveDirection == CardinalDirection::West &&
               stationaryState.movementDirection ==
                   CardinalDirection::West &&
               !stationaryState.moving &&
               nearlyEqual(stationaryState.iceSlipTimer, 0.0f) &&
               !stationaryState.onIce,
           "stationary input changed yaw, queried, or lost facing commit");

    constexpr std::array<CardinalDirection, 7> movementDirections{{
        CardinalDirection::None, CardinalDirection::North,
        CardinalDirection::South, CardinalDirection::West,
        CardinalDirection::East, static_cast<CardinalDirection>(5U),
        static_cast<CardinalDirection>(255U)}};
    constexpr std::array<XZ, 7> movementVectors{{
        {0.0f, 0.0f}, {0.0f, -1.0f}, {0.0f, 1.0f},
        {-1.0f, 0.0f}, {1.0f, 0.0f}, {0.0f, 0.0f},
        {0.0f, 0.0f}}};
    constexpr std::array<float, 7> movementYaws{{
        0.0f, 0.0f, 3.14159265358979323846f,
        -1.57079632679489661923f, 1.57079632679489661923f,
        0.0f, 0.0f}};
    int cardinalMovementCases = 0;
    std::string firstCardinalMovementMismatch;
    for (std::size_t index = 0; index < movementDirections.size(); ++index)
    {
        PlayerMovementState state;
        state.position = {2.0f, 3.0f};
        state.yaw = 0.41f;
        state.driveDirection = movementDirections[index];
        state.movementDirection = movementDirections[index];
        PlayerMovementParameters parameters;
        parameters.driveDirection = movementDirections[index];
        parameters.propelling = true;
        parameters.elapsed = 0.25f;
        parameters.movementSpeed = 2.0f;
        parameters.dustCooldown = 0.30f;
        std::vector<XZ> queries;
        const PlayerMovementUpdate update = advanceActivePlayerMovement(
            state, parameters,
            [&](XZ candidate) {
                queries.push_back(candidate);
                return true;
            });
        const XZ expectedPosition{
            2.0f + movementVectors[index].x * 0.5f,
            3.0f + movementVectors[index].z * 0.5f};
        const bool matches = update.valid && !update.blocked &&
            update.movementAccepted && !update.dust.has_value() &&
            queries.size() == 1U &&
            samePosition(queries[0], expectedPosition) &&
            samePosition(state.position, expectedPosition) &&
            nearlyEqual(state.yaw, movementYaws[index]) &&
            state.driveDirection == movementDirections[index] &&
            state.movementDirection == movementDirections[index] &&
            state.moving;
        if (!matches && firstCardinalMovementMismatch.empty())
            firstCardinalMovementMismatch = std::to_string(index);
        ++cardinalMovementCases;
    }
    expect(cardinalMovementCases == 7 &&
               firstCardinalMovementMismatch.empty(),
           "cardinal movement mismatch at direction " +
               firstCardinalMovementMismatch);

    reporter.beginSuite("player-movement-lane-snap-query-order");
    PlayerMovementState acceptedSnapState = movementStateFixture();
    PlayerMovementParameters snapParameters = movementParametersFixture();
    std::vector<XZ> acceptedSnapQueries;
    const PlayerMovementUpdate acceptedSnap = advanceActivePlayerMovement(
        acceptedSnapState, snapParameters,
        [&](XZ candidate) {
            acceptedSnapQueries.push_back(candidate);
            return true;
        });
    expect(acceptedSnap.valid && !acceptedSnap.blocked &&
               acceptedSnap.movementAccepted &&
               acceptedSnapQueries.size() == 2U &&
               samePosition(acceptedSnapQueries[0], {4.18f, 8.0f}) &&
               samePosition(acceptedSnapQueries[1], {4.68f, 8.0f}) &&
               samePosition(acceptedSnapState.position, {4.68f, 8.0f}),
           "accepted lane snap did not precede the forward query");

    PlayerMovementState secondarySnapState = movementStateFixture();
    secondarySnapState.movementDirection = CardinalDirection::East;
    std::vector<XZ> secondarySnapQueries;
    const PlayerMovementUpdate secondarySnap = advanceActivePlayerMovement(
        secondarySnapState, snapParameters,
        [&](XZ candidate) {
            secondarySnapQueries.push_back(candidate);
            return true;
        });
    expect(secondarySnap.valid && secondarySnap.movementAccepted &&
               secondarySnapQueries.size() == 2U &&
               samePosition(secondarySnapQueries[0], {4.18f, 8.0f}) &&
               samePosition(secondarySnapQueries[1], {4.68f, 8.0f}),
           "drive-change snap branch was lost when travel already matched");

    PlayerMovementState rejectedSnapState = movementStateFixture();
    std::vector<XZ> rejectedSnapQueries;
    const PlayerMovementUpdate rejectedSnap = advanceActivePlayerMovement(
        rejectedSnapState, snapParameters,
        [&](XZ candidate) {
            rejectedSnapQueries.push_back(candidate);
            return rejectedSnapQueries.size() != 1U;
        });
    expect(rejectedSnap.valid && !rejectedSnap.blocked &&
               rejectedSnap.movementAccepted &&
               rejectedSnapQueries.size() == 2U &&
               samePosition(rejectedSnapQueries[0], {4.18f, 8.0f}) &&
               samePosition(rejectedSnapQueries[1], {4.68f, 7.73f}) &&
               samePosition(rejectedSnapState.position, {4.68f, 7.73f}),
           "rejected snap did not advance from the original position");

    PlayerMovementState blockedAfterSnapState = movementStateFixture();
    blockedAfterSnapState.iceSlipTimer = 0.33f;
    std::vector<XZ> blockedAfterSnapQueries;
    const PlayerMovementUpdate blockedAfterSnap =
        advanceActivePlayerMovement(
            blockedAfterSnapState, snapParameters,
            [&](XZ candidate) {
                blockedAfterSnapQueries.push_back(candidate);
                return blockedAfterSnapQueries.size() == 1U;
            });
    expect(blockedAfterSnap.valid && blockedAfterSnap.blocked &&
               !blockedAfterSnap.movementAccepted &&
               !blockedAfterSnap.dust.has_value() &&
               blockedAfterSnapQueries.size() == 2U &&
               samePosition(blockedAfterSnapQueries[0],
                            {4.18f, 8.0f}) &&
               samePosition(blockedAfterSnapQueries[1],
                            {4.68f, 8.0f}) &&
               samePosition(blockedAfterSnapState.position,
                            {4.18f, 8.0f}) &&
               !blockedAfterSnapState.moving &&
               blockedAfterSnapState.movementDirection ==
                   CardinalDirection::East &&
               nearlyEqual(blockedAfterSnapState.iceSlipTimer, 0.0f),
           "blocked forward move rolled back snap or retained ice carry");

    PlayerMovementState noSnapState = movementStateFixture();
    noSnapState.driveDirection = CardinalDirection::East;
    noSnapState.movementDirection = CardinalDirection::East;
    std::vector<XZ> noSnapQueries;
    const PlayerMovementUpdate noSnap = advanceActivePlayerMovement(
        noSnapState, snapParameters,
        [&](XZ candidate) {
            noSnapQueries.push_back(candidate);
            return true;
        });
    expect(noSnap.valid && noSnap.movementAccepted &&
               noSnapQueries.size() == 1U &&
               samePosition(noSnapQueries[0], {4.68f, 7.73f}) &&
               samePosition(noSnapState.position, {4.68f, 7.73f}),
           "straight movement introduced an opportunistic lane snap");

    reporter.beginSuite("player-movement-ice-carry-and-expiry");
    const auto iceState = [] {
        PlayerMovementState state;
        state.position = {4.0f, 5.0f};
        state.yaw = 0.0f;
        state.driveDirection = CardinalDirection::North;
        state.movementDirection = CardinalDirection::North;
        state.moving = true;
        state.iceSlipTimer = 0.25f;
        state.onIce = true;
        return state;
    };
    PlayerMovementParameters iceParameters;
    iceParameters.driveDirection = CardinalDirection::East;
    iceParameters.propelling = true;
    iceParameters.elapsed = 0.10f;
    iceParameters.movementSpeed = 1.5f;
    iceParameters.surfaceIsIce = true;
    iceParameters.dustCooldown = 0.30f;

    PlayerMovementState turningCarryState = iceState();
    std::vector<XZ> turningCarryQueries;
    const PlayerMovementUpdate turningCarry = advanceActivePlayerMovement(
        turningCarryState, iceParameters,
        [&](XZ candidate) {
            turningCarryQueries.push_back(candidate);
            return true;
        });
    expect(turningCarry.valid && turningCarry.movementAccepted &&
               turningCarryQueries.size() == 1U &&
               samePosition(turningCarryQueries[0], {4.0f, 4.85f}) &&
               samePosition(turningCarryState.position, {4.0f, 4.85f}) &&
               turningCarryState.driveDirection == CardinalDirection::East &&
               turningCarryState.movementDirection ==
                   CardinalDirection::North &&
               turningCarryState.moving && turningCarryState.onIce &&
               nearlyEqual(turningCarryState.iceSlipTimer, 0.15f) &&
               nearlyEqual(turningCarryState.yaw,
                           1.57079632679489661923f),
           "turn input no longer preserved active ice travel");

    PlayerMovementState releaseCarryState = iceState();
    releaseCarryState.driveDirection = CardinalDirection::East;
    PlayerMovementParameters releaseParameters = iceParameters;
    releaseParameters.propelling = false;
    int releaseCarryQueries = 0;
    const PlayerMovementUpdate releaseCarry = advanceActivePlayerMovement(
        releaseCarryState, releaseParameters,
        [&](XZ candidate) {
            ++releaseCarryQueries;
            return samePosition(candidate, {4.0f, 4.85f});
        });
    expect(releaseCarry.valid && releaseCarry.movementAccepted &&
               releaseCarryQueries == 1 && releaseCarryState.moving &&
               releaseCarryState.movementDirection ==
                   CardinalDirection::North &&
               nearlyEqual(releaseCarryState.iceSlipTimer, 0.15f),
           "released input stopped ice carry before its timer expired");

    PlayerMovementState turningExpiryState = iceState();
    turningExpiryState.iceSlipTimer = 0.10f;
    std::vector<XZ> turningExpiryQueries;
    const PlayerMovementUpdate turningExpiry = advanceActivePlayerMovement(
        turningExpiryState, iceParameters,
        [&](XZ candidate) {
            turningExpiryQueries.push_back(candidate);
            return true;
        });
    expect(turningExpiry.valid && turningExpiry.movementAccepted &&
               turningExpiryQueries.size() == 2U &&
               samePosition(turningExpiryQueries[0], {4.0f, 5.0f}) &&
               samePosition(turningExpiryQueries[1], {4.15f, 5.0f}) &&
               samePosition(turningExpiryState.position, {4.15f, 5.0f}) &&
               turningExpiryState.movementDirection ==
                   CardinalDirection::East && turningExpiryState.moving &&
               nearlyEqual(turningExpiryState.iceSlipTimer, 0.0f),
           "expired turn carry did not snap then move in the drive direction");

    PlayerMovementState releaseExpiryState = iceState();
    releaseExpiryState.driveDirection = CardinalDirection::East;
    releaseExpiryState.iceSlipTimer = 0.10f;
    int releaseExpiryQueries = 0;
    const PlayerMovementUpdate releaseExpiry = advanceActivePlayerMovement(
        releaseExpiryState, releaseParameters,
        [&](XZ) {
            ++releaseExpiryQueries;
            return true;
        });
    expect(releaseExpiry.valid && !releaseExpiry.movementAccepted &&
               !releaseExpiry.blocked && releaseExpiryQueries == 0 &&
               !releaseExpiryState.moving &&
               releaseExpiryState.movementDirection ==
                   CardinalDirection::East &&
               nearlyEqual(releaseExpiryState.iceSlipTimer, 0.0f) &&
               releaseExpiryState.onIce,
           "released ice carry moved or queried at exact expiry");

    PlayerMovementState leftIceState = iceState();
    leftIceState.driveDirection = CardinalDirection::East;
    PlayerMovementParameters leftIceParameters = releaseParameters;
    leftIceParameters.surfaceIsIce = false;
    int leftIceQueries = 0;
    const PlayerMovementUpdate leftIce = advanceActivePlayerMovement(
        leftIceState, leftIceParameters,
        [&](XZ) {
            ++leftIceQueries;
            return true;
        });
    expect(leftIce.valid && !leftIce.movementAccepted &&
               leftIceQueries == 0 && !leftIceState.moving &&
               leftIceState.movementDirection == CardinalDirection::East &&
               nearlyEqual(leftIceState.iceSlipTimer, 0.0f) &&
               !leftIceState.onIce,
           "leaving ice did not clear released momentum immediately");

    PlayerMovementState blockedIceState = iceState();
    blockedIceState.driveDirection = CardinalDirection::East;
    PlayerMovementParameters blockedIceParameters = releaseParameters;
    blockedIceParameters.movementSpeed = 1.0f;
    int blockedIceQueries = 0;
    const PlayerMovementUpdate blockedIce = advanceActivePlayerMovement(
        blockedIceState, blockedIceParameters,
        [&](XZ candidate) {
            ++blockedIceQueries;
            return !samePosition(candidate, {4.0f, 4.90f});
        });
    expect(blockedIce.valid && blockedIce.blocked &&
               !blockedIce.movementAccepted && blockedIceQueries == 1 &&
               samePosition(blockedIceState.position, {4.0f, 5.0f}) &&
               !blockedIceState.moving &&
               blockedIceState.movementDirection == CardinalDirection::East &&
               nearlyEqual(blockedIceState.iceSlipTimer, 0.0f) &&
               blockedIceState.onIce && !blockedIce.dust.has_value(),
           "blocked ice travel moved, emitted dust, or retained carry");

    reporter.beginSuite("player-movement-detached-dust-boundary");
    PlayerMovementState dustState;
    dustState.position = {3.0f, 4.0f};
    dustState.driveDirection = CardinalDirection::West;
    dustState.movementDirection = CardinalDirection::West;
    PlayerMovementParameters dustParameters;
    dustParameters.driveDirection = CardinalDirection::West;
    dustParameters.propelling = true;
    dustParameters.elapsed = 0.25f;
    dustParameters.movementSpeed = 2.0f;
    dustParameters.dustCooldown = 0.0f;
    const PlayerMovementUpdate dustUpdate = advanceActivePlayerMovement(
        dustState, dustParameters, [](XZ) { return true; });
    expect(dustUpdate.valid && dustUpdate.movementAccepted &&
               !dustUpdate.blocked && dustUpdate.dust.has_value() &&
               samePosition(dustState.position, {2.5f, 4.0f}) &&
               samePosition(dustUpdate.dust->position, {3.08f, 4.0f}) &&
               samePosition(dustUpdate.dust->velocity, {-2.0f, 0.0f}) &&
               nearlyEqual(dustParameters.dustCooldown, 0.0f) &&
               nearlyEqual(kPlayerTrackDustCooldown, 0.120f),
           "detached player dust changed position, velocity, or cooldown ownership");

    const std::array<float, 5> dustClocks{{
        0.0f, -movementInfinity,
        std::nextafter(0.0f, movementInfinity), movementNaN,
        movementInfinity}};
    const std::array<bool, 5> dustExpected{{true, true, false, false, false}};
    bool dustGateParity = true;
    for (std::size_t index = 0; index < dustClocks.size(); ++index)
    {
        PlayerMovementState state;
        state.position = {3.0f, 4.0f};
        state.driveDirection = CardinalDirection::West;
        state.movementDirection = CardinalDirection::West;
        PlayerMovementParameters parameters = dustParameters;
        parameters.dustCooldown = dustClocks[index];
        const PlayerMovementUpdate update = advanceActivePlayerMovement(
            state, parameters, [](XZ) { return true; });
        dustGateParity = dustGateParity && update.valid &&
            update.movementAccepted &&
            update.dust.has_value() == dustExpected[index];
    }
    expect(dustGateParity,
           "player dust strict due gate changed for a special clock value");

    bool zeroDistanceParity = true;
    for (float elapsed : {0.0f, -0.0f})
    {
        PlayerMovementState state;
        state.position = {3.0f, 4.0f};
        state.driveDirection = CardinalDirection::South;
        state.movementDirection = CardinalDirection::South;
        PlayerMovementParameters parameters = dustParameters;
        parameters.driveDirection = CardinalDirection::South;
        parameters.elapsed = elapsed;
        int queries = 0;
        const PlayerMovementUpdate update = advanceActivePlayerMovement(
            state, parameters,
            [&](XZ candidate) {
                ++queries;
                return samePosition(candidate, {3.0f, 4.0f});
            });
        zeroDistanceParity = zeroDistanceParity && update.valid &&
            update.movementAccepted && queries == 1 &&
            samePosition(state.position, {3.0f, 4.0f}) &&
            update.dust.has_value() &&
            samePosition(update.dust->position, {3.0f, 3.42f}) &&
            samePosition(update.dust->velocity, {0.0f, 2.0f});
    }
    for (float speed : {0.0f, -0.0f})
    {
        PlayerMovementState state;
        state.position = {3.0f, 4.0f};
        state.driveDirection = CardinalDirection::South;
        state.movementDirection = CardinalDirection::South;
        PlayerMovementParameters parameters = dustParameters;
        parameters.driveDirection = CardinalDirection::South;
        parameters.movementSpeed = speed;
        int queries = 0;
        const PlayerMovementUpdate update = advanceActivePlayerMovement(
            state, parameters,
            [&](XZ candidate) {
                ++queries;
                return samePosition(candidate, {3.0f, 4.0f});
            });
        zeroDistanceParity = zeroDistanceParity && update.valid &&
            update.movementAccepted && queries == 1 &&
            samePosition(state.position, {3.0f, 4.0f}) &&
            update.dust.has_value() &&
            samePosition(update.dust->position, {3.0f, 3.42f}) &&
            samePosition(update.dust->velocity, {0.0f, 0.0f});
    }
    expect(zeroDistanceParity,
           "zero/negative-zero distance stopped old query/dust parity");

    reporter.beginSuite("player-movement-unknown-direction-parity");
    constexpr std::array<CardinalDirection, 3> unknownDirections{{
        CardinalDirection::None, static_cast<CardinalDirection>(5U),
        static_cast<CardinalDirection>(255U)}};
    bool unknownMovementParity = true;
    int unknownMovementCases = 0;
    for (CardinalDirection direction : unknownDirections)
    {
        PlayerMovementState state;
        state.position = {3.0f, 4.0f};
        state.yaw = 0.75f;
        state.driveDirection = direction;
        state.movementDirection = direction;
        PlayerMovementParameters parameters = dustParameters;
        parameters.driveDirection = direction;
        int queries = 0;
        const PlayerMovementUpdate update = advanceActivePlayerMovement(
            state, parameters,
            [&](XZ candidate) {
                ++queries;
                return samePosition(candidate, {3.0f, 4.0f});
            });
        unknownMovementParity = unknownMovementParity && update.valid &&
            update.movementAccepted && !update.blocked && queries == 1 &&
            samePosition(state.position, {3.0f, 4.0f}) && state.moving &&
            state.driveDirection == direction &&
            state.movementDirection == direction &&
            nearlyEqual(state.yaw, 0.0f) && update.dust.has_value() &&
            samePosition(update.dust->position, {3.0f, 4.0f}) &&
            samePosition(update.dust->velocity, {0.0f, 0.0f});
        ++unknownMovementCases;
    }
    expect(unknownMovementCases == 3 && unknownMovementParity,
           "None/5/255 movement stopped zero-vector legacy parity");

    reporter.beginSuite("player-fire-invalid-static-input-is-atomic");
    int invalidCallbackCount = 0;
    const PlayerShellLaunch countingLaunch =
        [&](const PlayerShellLaunchIntent &) { ++invalidCallbackCount; };
    const auto rejectsAtomically = [&](PlayerFireParameters parameters,
                                       const PlayerShellLaunch &launch) {
        float cooldown = -0.25f;
        const PlayerFireOutcome outcome = advancePlayerFireTransaction(
            cooldown, parameters, launch);
        return outcome == PlayerFireOutcome::Invalid &&
               cooldown == -0.25f;
    };
    PlayerFireParameters nanReload = fireParametersFixture();
    nanReload.reloadInterval = std::numeric_limits<float>::quiet_NaN();
    PlayerFireParameters negativeReload = fireParametersFixture();
    negativeReload.reloadInterval = -0.001f;
    PlayerFireParameters infiniteSpawn = fireParametersFixture();
    infiniteSpawn.shellSpawnDistance =
        std::numeric_limits<float>::infinity();
    PlayerFireParameters negativeSpawn = fireParametersFixture();
    negativeSpawn.shellSpawnDistance = -0.001f;
    PlayerFireParameters negativeShellCount = fireParametersFixture();
    negativeShellCount.activeShellCount = -1;
    PlayerFireParameters negativePlayer = fireParametersFixture();
    negativePlayer.playerIndex = -1;
    expect(rejectsAtomically(nanReload, countingLaunch) &&
               rejectsAtomically(negativeReload, countingLaunch) &&
               rejectsAtomically(infiniteSpawn, countingLaunch) &&
               rejectsAtomically(negativeSpawn, countingLaunch) &&
               rejectsAtomically(negativeShellCount, countingLaunch) &&
               rejectsAtomically(negativePlayer, countingLaunch) &&
               rejectsAtomically(fireParametersFixture(), {}) &&
               invalidCallbackCount == 0,
           "invalid fire configuration, identity, count, or callback was not atomic");

    reporter.beginSuite("player-fire-request-and-cooldown-gates");
    int gatedCallbackCount = 0;
    const PlayerShellLaunch gatedLaunch =
        [&](const PlayerShellLaunchIntent &) { ++gatedCallbackCount; };
    PlayerFireParameters idle = fireParametersFixture();
    idle.requested = false;
    float idleCooldown = -0.75f;
    expect(advancePlayerFireTransaction(idleCooldown, idle, gatedLaunch) ==
                   PlayerFireOutcome::NotRequested &&
               idleCooldown == -0.75f && gatedCallbackCount == 0,
           "idle trigger changed cooldown or launched a shell");
    PlayerFireParameters cooling = fireParametersFixture();
    float positiveCooldown = 0.001f;
    float nanCooldown = std::numeric_limits<float>::quiet_NaN();
    float infiniteCooldown = std::numeric_limits<float>::infinity();
    expect(advancePlayerFireTransaction(positiveCooldown, cooling,
                                        gatedLaunch) ==
                   PlayerFireOutcome::CoolingDown &&
               advancePlayerFireTransaction(nanCooldown, cooling,
                                            gatedLaunch) ==
                   PlayerFireOutcome::CoolingDown &&
               advancePlayerFireTransaction(infiniteCooldown, cooling,
                                            gatedLaunch) ==
                   PlayerFireOutcome::CoolingDown &&
               positiveCooldown == 0.001f && std::isnan(nanCooldown) &&
               std::isinf(infiniteCooldown) && gatedCallbackCount == 0,
           "positive, NaN, or infinite cooldown stopped being inert");

    reporter.beginSuite("player-fire-shell-limit-resets-due-clock");
    constexpr std::array<int, 6> requestedLevels{{-1, 0, 1, 2, 3, 4}};
    constexpr std::array<int, 6> maximumShells{{2, 2, 2, 3, 4, 4}};
    bool shellLimitParity = true;
    int rejectedLaunchCount = 0;
    for (std::size_t index = 0; index < requestedLevels.size(); ++index)
    {
        PlayerFireParameters limited = fireParametersFixture();
        limited.playerLevel = requestedLevels[index];
        limited.activeShellCount = maximumShells[index];
        float cooldown = 0.0f;
        const PlayerFireOutcome atLimit = advancePlayerFireTransaction(
            cooldown, limited,
            [&](const PlayerShellLaunchIntent &) { ++rejectedLaunchCount; });
        limited.activeShellCount = maximumShells[index] + 1;
        float aboveLimitCooldown = -0.5f;
        const PlayerFireOutcome aboveLimit = advancePlayerFireTransaction(
            aboveLimitCooldown, limited,
            [&](const PlayerShellLaunchIntent &) { ++rejectedLaunchCount; });
        shellLimitParity = shellLimitParity &&
            atLimit == PlayerFireOutcome::ShellLimitReached &&
            aboveLimit == PlayerFireOutcome::ShellLimitReached &&
            nearlyEqual(cooldown, 0.120f) &&
            nearlyEqual(aboveLimitCooldown, 0.120f);
    }
    expect(shellLimitParity && rejectedLaunchCount == 0,
           "one player level changed its full-slot rejection or cooldown reset");

    reporter.beginSuite("player-fire-detached-launch-matrix");
    constexpr std::array<float, 6> shellSpeeds{{
        9.775f, 9.775f, 12.7075f, 12.7075f, 12.7075f, 12.7075f}};
    constexpr std::array<bool, 6> powerShells{{
        false, false, false, false, true, true}};
    constexpr std::array<CardinalDirection, 7> fireDirections{{
        CardinalDirection::None, CardinalDirection::North,
        CardinalDirection::South, CardinalDirection::West,
        CardinalDirection::East, static_cast<CardinalDirection>(5U),
        static_cast<CardinalDirection>(255U)}};
    constexpr std::array<XZ, 7> directionVectors{{
        {0.0f, 0.0f}, {0.0f, -1.0f}, {0.0f, 1.0f},
        {-1.0f, 0.0f}, {1.0f, 0.0f}, {0.0f, 0.0f},
        {0.0f, 0.0f}}};
    int launchCases = 0;
    std::string firstLaunchMismatch;
    for (std::size_t levelIndex = 0;
         levelIndex < requestedLevels.size(); ++levelIndex)
    {
        for (std::size_t directionIndex = 0;
             directionIndex < fireDirections.size(); ++directionIndex)
        {
            PlayerFireParameters parameters = fireParametersFixture();
            parameters.activeShellCount = maximumShells[levelIndex] - 1;
            parameters.playerIndex = 17;
            parameters.playerLevel = requestedLevels[levelIndex];
            parameters.tankPosition = {-2.25f, 11.75f};
            parameters.direction = fireDirections[directionIndex];
            float cooldown = launchCases == 0
                ? -std::numeric_limits<float>::infinity() : 0.0f;
            int callbackCount = 0;
            bool callbackSawReset = false;
            PlayerShellLaunchIntent actual;
            const PlayerFireOutcome outcome = advancePlayerFireTransaction(
                cooldown, parameters,
                [&](const PlayerShellLaunchIntent &intent) {
                    ++callbackCount;
                    callbackSawReset = nearlyEqual(cooldown, 0.120f);
                    actual = intent;
                });
            const XZ expectedPosition = parameters.tankPosition +
                directionVectors[directionIndex] * 0.625f;
            const XZ expectedVelocity =
                directionVectors[directionIndex] * shellSpeeds[levelIndex];
            const bool matches = outcome == PlayerFireOutcome::Fired &&
                callbackCount == 1 && callbackSawReset &&
                nearlyEqual(cooldown, 0.120f) &&
                samePosition(actual.tankPosition, parameters.tankPosition) &&
                samePosition(actual.position, expectedPosition) &&
                samePosition(actual.velocity, expectedVelocity) &&
                actual.direction == parameters.direction &&
                actual.playerIndex == parameters.playerIndex &&
                actual.playerLevel == parameters.playerLevel &&
                actual.power == powerShells[levelIndex];
            if (!matches && firstLaunchMismatch.empty())
            {
                firstLaunchMismatch =
                    "level=" + std::to_string(parameters.playerLevel) +
                    ", direction=" + std::to_string(directionIndex);
            }
            ++launchCases;
        }
    }
    expect(launchCases == 42,
           "launch matrix did not visit all 42 level/direction cases");
    expect(firstLaunchMismatch.empty(),
           "detached launch mismatch at " + firstLaunchMismatch);

    reporter.beginSuite("player-fire-explicit-clock-and-spawn-scalars");
    PlayerFireParameters distinctive = fireParametersFixture();
    distinctive.playerLevel = 1;
    distinctive.tankPosition = {-8.0f, 2.5f};
    distinctive.direction = CardinalDirection::East;
    distinctive.reloadInterval = 0.37f;
    distinctive.shellSpawnDistance = 0.42f;
    float distinctiveCooldown = 0.0f;
    bool distinctiveCallback = false;
    PlayerShellLaunchIntent distinctiveIntent;
    const PlayerFireOutcome distinctiveOutcome =
        advancePlayerFireTransaction(
            distinctiveCooldown, distinctive,
            [&](const PlayerShellLaunchIntent &intent) {
                distinctiveCallback = nearlyEqual(distinctiveCooldown, 0.37f);
                distinctiveIntent = intent;
            });
    expect(distinctiveOutcome == PlayerFireOutcome::Fired &&
               distinctiveCallback &&
               nearlyEqual(distinctiveCooldown, 0.37f) &&
               samePosition(distinctiveIntent.position, {-7.58f, 2.5f}) &&
               samePosition(distinctiveIntent.velocity, {12.7075f, 0.0f}),
           "transaction ignored explicit reload or spawn-distance scalars");
    PlayerFireParameters distinctiveFull = distinctive;
    distinctiveFull.activeShellCount = 2;
    float distinctiveFullCooldown = 0.0f;
    int distinctiveFullCallbackCount = 0;
    const PlayerFireOutcome distinctiveFullOutcome =
        advancePlayerFireTransaction(
            distinctiveFullCooldown, distinctiveFull,
            [&](const PlayerShellLaunchIntent &) {
                ++distinctiveFullCallbackCount;
            });
    expect(distinctiveFullOutcome ==
                   PlayerFireOutcome::ShellLimitReached &&
               distinctiveFullCallbackCount == 0 &&
               nearlyEqual(distinctiveFullCooldown, 0.37f),
           "full-slot rejection ignored the explicit reload scalar");
    PlayerFireParameters zeroScalars = fireParametersFixture();
    zeroScalars.direction = CardinalDirection::South;
    zeroScalars.reloadInterval = 0.0f;
    zeroScalars.shellSpawnDistance = 0.0f;
    float zeroCooldown = 0.0f;
    PlayerShellLaunchIntent zeroIntent;
    int zeroCallbackCount = 0;
    const PlayerFireOutcome zeroOutcome = advancePlayerFireTransaction(
        zeroCooldown, zeroScalars,
        [&](const PlayerShellLaunchIntent &intent) {
            ++zeroCallbackCount;
            zeroIntent = intent;
        });
    expect(zeroOutcome == PlayerFireOutcome::Fired &&
               zeroCallbackCount == 1 && zeroCooldown == 0.0f &&
               samePosition(zeroIntent.position,
                            zeroScalars.tankPosition) &&
               samePosition(zeroIntent.velocity, {0.0f, 9.775f}),
           "zero reload or spawn-distance boundary stopped being valid");

    reporter.finish();
    return passed ? 0 : 1;
}
