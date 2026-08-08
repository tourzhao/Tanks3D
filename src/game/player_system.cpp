#include "game/player_system.h"

#include "core/gameplay_rules.h"

#include <algorithm>
#include <cmath>

namespace tanks3d::game
{
namespace
{
constexpr float kPlayerTrackDustTrailOffset = 0.58f;

const DirectionButtonFrame *buttonForDirection(
    CardinalDirection direction, const PlayerControlFrame &controls)
{
    switch (direction)
    {
    case CardinalDirection::North: return &controls.north;
    case CardinalDirection::South: return &controls.south;
    case CardinalDirection::West: return &controls.west;
    case CardinalDirection::East: return &controls.east;
    case CardinalDirection::None: return nullptr;
    }
    return nullptr;
}

CardinalDirection firstHeldDirection(const PlayerControlFrame &controls)
{
    if (controls.north.held)
        return CardinalDirection::North;
    if (controls.south.held)
        return CardinalDirection::South;
    if (controls.west.held)
        return CardinalDirection::West;
    if (controls.east.held)
        return CardinalDirection::East;
    return CardinalDirection::None;
}
} // namespace

PlayerControlPlan planPlayerControl(
    CardinalDirection currentDrive, const PlayerControlFrame &controls)
{
    CardinalDirection selected = currentDrive;
    if (controls.north.pressed)
        selected = CardinalDirection::North;
    if (controls.south.pressed)
        selected = CardinalDirection::South;
    if (controls.west.pressed)
        selected = CardinalDirection::West;
    if (controls.east.pressed)
        selected = CardinalDirection::East;

    CardinalDirection active = selected;
    const DirectionButtonFrame *selectedButton =
        buttonForDirection(selected, controls);
    if (selectedButton == nullptr || !selectedButton->held)
        active = firstHeldDirection(controls);

    const bool propelling = active != CardinalDirection::None;
    if (propelling)
        selected = active;
    return {selected, propelling, controls.fireHeld};
}

PlayerFrameStart beginPlayerFrame(
    bool active, const PlayerFrameClockState &clocks, float elapsed)
{
    PlayerFrameStart start;
    start.clocks = clocks;
    if (!std::isfinite(elapsed) || elapsed < 0.0f)
        return start;

    start.valid = true;
    start.clocks.dustCooldown = std::max(
        0.0f, start.clocks.dustCooldown - elapsed);
    start.clocks.shieldTimer = std::max(
        0.0f, start.clocks.shieldTimer - elapsed);
    start.clocks.streakPopupTimer = std::max(
        0.0f, start.clocks.streakPopupTimer - elapsed);

    if (!active)
    {
        start.phase = PlayerFramePhase::Inactive;
        return start;
    }
    if (clocks.creationTimer > 0.0f)
    {
        start.phase = PlayerFramePhase::Creating;
        start.clocks.creationTimer = std::max(
            0.0f, start.clocks.creationTimer - elapsed);
        return start;
    }

    start.phase = PlayerFramePhase::Ready;
    start.clocks.fireCooldown = std::max(
        0.0f, start.clocks.fireCooldown - elapsed);
    return start;
}

PlayerDeathTransition advanceInactivePlayerDeath(
    PlayerDeathState &state, float elapsed)
{
    if (!std::isfinite(elapsed) || elapsed < 0.0f)
        return PlayerDeathTransition::Invalid;
    if (state.deathTimer <= 0.0f)
        return PlayerDeathTransition::None;

    PlayerDeathState next = state;
    next.deathTimer = std::max(0.0f, next.deathTimer - elapsed);
    if (next.deathTimer > 0.0f)
    {
        state = next;
        return PlayerDeathTransition::Waiting;
    }

    next.lives = next.lives > 0 ? next.lives - 1 : 0;
    state = next;
    return state.lives > 0 ? PlayerDeathTransition::Respawn
                           : PlayerDeathTransition::Eliminated;
}

PlayerSpawnOutcome preparePlayerSpawnState(
    PlayerSpawnState &state, const PlayerSpawnParameters &parameters)
{
    const bool knownProgression =
        parameters.progression == PlayerSpawnProgression::Preserve ||
        parameters.progression == PlayerSpawnProgression::Reset;
    if (!knownProgression ||
        !std::isfinite(parameters.spawnPosition.x) ||
        !std::isfinite(parameters.spawnPosition.z) ||
        !std::isfinite(parameters.creationDuration) ||
        parameters.creationDuration < 0.0f ||
        !std::isfinite(parameters.initialFireCooldown) ||
        parameters.initialFireCooldown < 0.0f ||
        !std::isfinite(parameters.shieldDuration) ||
        parameters.shieldDuration < 0.0f)
    {
        return PlayerSpawnOutcome::Invalid;
    }

    PlayerSpawnState next = state;
    next.movement.position = parameters.spawnPosition;
    next.movement.yaw = 0.0f;
    next.movement.driveDirection = CardinalDirection::North;
    next.movement.movementDirection = CardinalDirection::North;
    if (parameters.progression == PlayerSpawnProgression::Reset)
    {
        next.level = 0;
        next.directKillStreak = 0;
        next.clocks.streakPopupTimer = 0.0f;
    }
    if (parameters.progression == PlayerSpawnProgression::Reset ||
        next.hitPoints <= 0)
    {
        next.hitPoints = parameters.maximumHitPoints;
    }
    next.active = true;
    next.movement.moving = false;
    next.hasBoat = false;
    next.clocks.creationTimer = parameters.creationDuration;
    next.respawnTimer = 0.0f;
    next.deathTimer = 0.0f;
    next.clocks.fireCooldown = parameters.initialFireCooldown;
    next.clocks.dustCooldown = 0.0f;
    next.movement.iceSlipTimer = 0.0f;
    next.movement.onIce = false;
    next.clocks.shieldTimer = parameters.shieldDuration;
    state = next;
    return PlayerSpawnOutcome::Prepared;
}

PlayerMovementUpdate advanceActivePlayerMovement(
    PlayerMovementState &state,
    const PlayerMovementParameters &parameters,
    const PlayerPositionAvailable &positionAvailable)
{
    PlayerMovementUpdate update;
    if (!std::isfinite(parameters.elapsed) || parameters.elapsed < 0.0f ||
        !std::isfinite(parameters.movementSpeed) ||
        parameters.movementSpeed < 0.0f || !positionAvailable)
    {
        return update;
    }
    const float movementDistance =
        parameters.movementSpeed * parameters.elapsed;
    if (!std::isfinite(movementDistance))
        return update;

    PlayerMovementState next = state;
    const bool hadMomentum = next.moving;
    const CardinalDirection previousDrive = next.driveDirection;
    const CardinalDirection previousTravel = next.movementDirection;

    next.driveDirection = parameters.driveDirection;
    if (parameters.propelling)
        next.yaw = core::cardinalYaw(next.driveDirection);

    next.moving = core::resolveIceTravel(
        parameters.surfaceIsIce, parameters.propelling, false, hadMomentum,
        next.driveDirection, parameters.elapsed, next.movementDirection,
        next.iceSlipTimer, next.onIce);

    if (next.moving &&
        (next.movementDirection != previousTravel ||
         (parameters.propelling &&
          parameters.driveDirection != previousDrive &&
          next.movementDirection == parameters.driveDirection)))
    {
        const core::XZ snapped = core::snappedToCardinalLane(
            next.position, next.movementDirection);
        if (positionAvailable(snapped))
            next.position = snapped;
    }

    update.valid = true;
    if (next.moving)
    {
        const core::XZ direction =
            core::cardinalVector(next.movementDirection);
        const core::XZ candidate =
            next.position + direction * movementDistance;
        if (positionAvailable(candidate))
        {
            next.position = candidate;
            update.movementAccepted = true;
            if (parameters.dustCooldown <= 0.0f)
            {
                update.dust = PlayerTrackDustIntent{
                    next.position - direction * kPlayerTrackDustTrailOffset,
                    direction * parameters.movementSpeed};
            }
        }
        else
        {
            update.blocked = true;
            next.moving = false;
            next.iceSlipTimer = 0.0f;
            next.movementDirection = next.driveDirection;
        }
    }

    state = next;
    return update;
}

PlayerFireOutcome advancePlayerFireTransaction(
    float &fireCooldown, const PlayerFireParameters &parameters,
    const PlayerShellLaunch &launch)
{
    if (!std::isfinite(parameters.reloadInterval) ||
        parameters.reloadInterval < 0.0f ||
        !std::isfinite(parameters.shellSpawnDistance) ||
        parameters.shellSpawnDistance < 0.0f ||
        parameters.activeShellCount < 0 || parameters.playerIndex < 0 ||
        !launch)
    {
        return PlayerFireOutcome::Invalid;
    }
    if (!parameters.requested)
        return PlayerFireOutcome::NotRequested;
    // Preserve the legacy `<= 0` gate exactly: NaN and positive infinity stay
    // inert, while negative infinity is a due attempt.
    if (!(fireCooldown <= 0.0f))
        return PlayerFireOutcome::CoolingDown;

    const core::PlayerLevelStats stats =
        core::playerLevelStats(parameters.playerLevel);
    // The original input loop resets this clock even when the active-shell
    // limit rejects the attempt.
    fireCooldown = parameters.reloadInterval;
    if (parameters.activeShellCount >= stats.maximumShells)
        return PlayerFireOutcome::ShellLimitReached;

    PlayerShellLaunchIntent intent;
    intent.tankPosition = parameters.tankPosition;
    intent.direction = parameters.direction;
    intent.playerIndex = parameters.playerIndex;
    intent.playerLevel = parameters.playerLevel;
    intent.power = stats.powerShell;
    const core::XZ direction = core::cardinalVector(intent.direction);
    intent.position = intent.tankPosition +
        direction * parameters.shellSpawnDistance;
    intent.velocity = direction * stats.shellSpeed;
    launch(intent);
    return PlayerFireOutcome::Fired;
}
} // namespace tanks3d::game
