#include "app/shell_map_core_presentation.h"

#include "game/combat_system.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <utility>

namespace tanks3d::app
{
namespace
{
bool finite(core::XZ value)
{
    return std::isfinite(value.x) && std::isfinite(value.z);
}

bool validOwner(game::ShellOwner owner)
{
    return owner == game::ShellOwner::Player ||
           owner == game::ShellOwner::Enemy;
}

bool emptyGovernmentWallDetails(const game::ShellImpactDetails &details)
{
    return details.governmentWallIndex == -1 &&
           details.governmentWallHealthBefore == 0 &&
           details.governmentWallHealthAfter == 0;
}

bool validBrickDetails(const game::ShellImpactDetails &details,
                       bool power)
{
    if (details.brickCount < 0 ||
        details.brickCount > static_cast<int>(details.bricks.size()))
    {
        return false;
    }
    int previousRow = -1;
    int previousColumn = -1;
    for (int index = 0; index < details.brickCount; ++index)
    {
        const game::BrickDamage &damage =
            details.bricks[static_cast<std::size_t>(index)];
        if (damage.row < 0 || damage.row >= game::kMapSize ||
            damage.column < 0 || damage.column >= game::kMapSize ||
            damage.beforeMask == 0U || damage.beforeMask > 0x0fU ||
            damage.afterMask > 0x0fU ||
            damage.afterMask == damage.beforeMask ||
            (damage.afterMask | damage.beforeMask) !=
                damage.beforeMask ||
            (power && damage.afterMask != 0U))
        {
            return false;
        }
        if (damage.row < previousRow ||
            (damage.row == previousRow &&
             damage.column <= previousColumn))
        {
            return false;
        }
        previousRow = damage.row;
        previousColumn = damage.column;
    }
    return true;
}

bool destroyedBrick(const game::ShellImpactDetails &details)
{
    for (int index = 0; index < details.brickCount; ++index)
    {
        const game::BrickDamage &damage =
            details.bricks[static_cast<std::size_t>(index)];
        if (damage.beforeMask != 0U && damage.afterMask == 0U)
            return true;
    }
    return false;
}

game::GameEvent brickEvent(const game::CombatOutcome &outcome,
                           const game::BrickDamage &damage)
{
    game::GameEvent event = game::shellEvent(
        game::GameEventType::BrickHit, outcome.owner,
        outcome.ownerIndex, outcome.position, outcome.power);
    event.impactKind = game::ImpactKind::Brick;
    event.row = damage.row;
    event.column = damage.column;
    event.valueBefore = damage.beforeMask;
    event.valueAfter = damage.afterMask;
    return event;
}

game::GameEvent governmentWallEvent(
    const game::CombatOutcome &outcome)
{
    const game::ShellImpactDetails &details = outcome.impactDetails;
    game::GameEvent event = game::shellEvent(
        game::GameEventType::BaseDamaged, outcome.owner,
        outcome.ownerIndex, outcome.position, outcome.power);
    event.impactKind = game::ImpactKind::GovernmentWall;
    event.basePart = game::GovernmentBasePart::Wall;
    event.baseSegmentIndex = details.governmentWallIndex;
    event.valueBefore = details.governmentWallHealthBefore;
    event.valueAfter = details.governmentWallHealthAfter;
    return event;
}

game::GameEvent governmentCoreEvent(
    const game::CombatOutcome &outcome)
{
    game::GameEvent event = game::shellEvent(
        game::GameEventType::BaseDamaged, outcome.owner,
        outcome.ownerIndex, outcome.position, outcome.power);
    event.basePart = game::GovernmentBasePart::Core;
    event.valueBefore = outcome.governmentCoreHealthBefore;
    event.valueAfter = outcome.governmentCoreHealthAfter;
    return event;
}

bool mapEventsMatch(const game::CombatOutcome &outcome,
                    const std::vector<game::GameEvent> &events)
{
    const game::ShellImpactDetails &details = outcome.impactDetails;
    std::vector<game::GameEvent> expected;
    expected.reserve(details.bricks.size() + 1U);
    for (int index = 0; index < details.brickCount; ++index)
    {
        expected.push_back(brickEvent(
            outcome, details.bricks[static_cast<std::size_t>(index)]));
    }
    if (details.governmentWallIndex >= 0 &&
        details.governmentWallHealthAfter <
            details.governmentWallHealthBefore)
    {
        expected.push_back(governmentWallEvent(outcome));
    }
    return events == expected;
}

bool validStageMapResult(
    const game::CombatOutcome &outcome,
    const std::vector<game::GameEvent> &events)
{
    const game::ShellImpactDetails &details = outcome.impactDetails;
    if (!validBrickDetails(details, outcome.power) ||
        outcome.governmentCoreHealthBefore != 0 ||
        outcome.governmentCoreHealthAfter != 0)
    {
        return false;
    }

    switch (outcome.impactKind)
    {
    case game::ImpactKind::Brick:
        if (details.brickCount == 0 ||
            !emptyGovernmentWallDetails(details))
            return false;
        break;
    case game::ImpactKind::GovernmentWall:
    {
        if (details.brickCount != 0 ||
            details.governmentWallIndex < 0 ||
            details.governmentWallIndex >= game::kGovernmentWallCount ||
            details.governmentWallHealthBefore <= 0 ||
            details.governmentWallHealthBefore >
                game::kGovernmentWallMaximumHealth)
        {
            return false;
        }
        const int expectedDamage = outcome.power
            ? game::kGovernmentPowerShellDamage
            : 1;
        const int expectedAfter = std::max(
            0, details.governmentWallHealthBefore - expectedDamage);
        if (details.governmentWallHealthAfter != expectedAfter)
        {
            return false;
        }
        break;
    }
    case game::ImpactKind::Steel:
        if (details.governmentWallIndex >= 0)
        {
            if (details.brickCount != 0 ||
                details.governmentWallIndex >=
                    game::kGovernmentWallCount ||
                details.governmentWallHealthBefore <= 0 ||
                details.governmentWallHealthBefore >
                    game::kGovernmentWallMaximumHealth ||
                details.governmentWallHealthAfter !=
                    details.governmentWallHealthBefore)
            {
                return false;
            }
        }
        else if (!emptyGovernmentWallDetails(details))
        {
            return false;
        }
        break;
    case game::ImpactKind::Boundary:
        if (details.brickCount != 0 ||
            !emptyGovernmentWallDetails(details))
            return false;
        break;
    case game::ImpactKind::None:
    default:
        return false;
    }
    return mapEventsMatch(outcome, events);
}

bool validGovernmentCoreResult(
    const game::CombatOutcome &outcome,
    const std::vector<game::GameEvent> &events)
{
    const game::ShellImpactDetails &details = outcome.impactDetails;
    if (outcome.impactKind != game::ImpactKind::None ||
        details.brickCount != 0 ||
        !emptyGovernmentWallDetails(details))
    {
        return false;
    }
    const bool liveHit = outcome.governmentCoreHealthBefore == 1 &&
                         outcome.governmentCoreHealthAfter == 0;
    const bool deadHit = outcome.governmentCoreHealthBefore == 0 &&
                         outcome.governmentCoreHealthAfter == 0;
    if (!liveHit && !deadHit)
        return false;
    if (liveHit)
        return events == std::vector<game::GameEvent>{
                             governmentCoreEvent(outcome)};
    return events.empty();
}
} // namespace

void ShellMapCorePresentationCommand::append(
    ShellMapCorePresentationAction action)
{
    assert(actionCount < actions.size());
    actions.at(actionCount) = std::move(action);
    ++actionCount;
}

ShellMapCorePresentationStep shellMapCorePresentationStep(
    const ShellMapCorePresentationAction &action)
{
    if (std::holds_alternative<AppendMapCoreEventsAction>(action))
        return ShellMapCorePresentationStep::PhysicalEventsAppended;
    if (std::holds_alternative<SpawnMapCoreBrickImpactAction>(action))
        return ShellMapCorePresentationStep::BrickImpactFxSpawned;
    if (std::holds_alternative<
            SpawnGovernmentWallBreachImpactAction>(action))
        return ShellMapCorePresentationStep::WallBreachImpactFxSpawned;
    if (std::holds_alternative<SpawnMapCoreSurfaceImpactAction>(action))
        return ShellMapCorePresentationStep::SurfaceImpactFxSpawned;
    if (std::holds_alternative<
            ApplyMapCoreRadialCameraShakeAction>(action))
        return ShellMapCorePresentationStep::CameraShakePassCompleted;
    if (std::holds_alternative<
            SpawnGovernmentCoreExplosionAction>(action))
        return ShellMapCorePresentationStep::CoreExplosionFxSpawned;
    if (std::holds_alternative<RequestMapCoreAudioAction>(action))
        return ShellMapCorePresentationStep::AudioRequested;
    if (std::holds_alternative<CommitMapCoreShellImpactAction>(action))
        return ShellMapCorePresentationStep::ShellImpactCommitted;
    assert(false && "unhandled shell-map/core presentation action");
    return ShellMapCorePresentationStep::ShellImpactCommitted;
}

std::optional<ShellMapCorePresentationCommand>
makeShellMapCorePresentationCommand(
    const game::ShellPhysicalImpactResult &physicalImpact,
    std::vector<game::GameEvent> physicalEvents)
{
    if (!physicalImpact.resolved)
        return std::nullopt;
    if (physicalImpact.enemyCommit.applied ||
        physicalImpact.playerCommit.applied)
    {
        return std::nullopt;
    }

    const game::CombatOutcome &outcome = physicalImpact.outcome;
    if (!finite(outcome.position) || !finite(outcome.incomingVelocity) ||
        !validOwner(outcome.owner) || outcome.ownerIndex < 0)
    {
        return std::nullopt;
    }

    ShellMapCorePresentationCommand command;
    if (outcome.target == game::CombatTarget::StageMap)
    {
        if (!validStageMapResult(outcome, physicalEvents))
            return std::nullopt;
        if (!physicalEvents.empty())
        {
            command.append(
                AppendMapCoreEventsAction{std::move(physicalEvents)});
        }

        const float impactSpeed = std::max(
            0.001f,
            std::sqrt(core::lengthSquared(outcome.incomingVelocity)));
        const core::XZ travel =
            outcome.incomingVelocity * (1.0f / impactSpeed);
        if (outcome.impactKind == game::ImpactKind::Brick ||
            outcome.impactKind == game::ImpactKind::GovernmentWall)
        {
            const bool wall = outcome.impactKind ==
                              game::ImpactKind::GovernmentWall;
            const bool destroyed = wall
                ? outcome.impactDetails.governmentWallHealthAfter == 0
                : destroyedBrick(outcome.impactDetails);
            command.append(SpawnMapCoreBrickImpactAction{
                {outcome.position.x, wall ? 0.54f : 0.47f,
                 outcome.position.z},
                {-travel.x, 0.28f, -travel.z}, outcome.power,
                destroyed});
            if (wall && destroyed)
            {
                command.append(SpawnGovernmentWallBreachImpactAction{
                    {outcome.position.x, 0.48f, outcome.position.z},
                    {-travel.x, 0.42f, -travel.z}, true});
                command.append(ApplyMapCoreRadialCameraShakeAction{
                    outcome.position, 0.22f, 0.012f});
            }
        }
        else
        {
            command.append(SpawnMapCoreSurfaceImpactAction{
                {outcome.position.x, 0.43f, outcome.position.z},
                {0.0f, 1.0f, 0.0f},
                outcome.power ||
                    outcome.impactKind == game::ImpactKind::Steel});
        }

        if (outcome.owner == game::ShellOwner::Player)
        {
            audio::AudioCue cue = audio::AudioCue::BoundaryHit;
            if (outcome.impactKind == game::ImpactKind::Brick ||
                outcome.impactKind == game::ImpactKind::GovernmentWall)
            {
                cue = audio::AudioCue::BrickHit;
            }
            else if (outcome.impactKind == game::ImpactKind::Steel)
            {
                cue = audio::AudioCue::SteelHit;
            }
            command.append(RequestMapCoreAudioAction{cue});
        }
    }
    else if (outcome.target == game::CombatTarget::GovernmentCore)
    {
        if (!validGovernmentCoreResult(outcome, physicalEvents))
            return std::nullopt;
        if (outcome.governmentCoreHealthBefore == 1)
        {
            command.append(
                AppendMapCoreEventsAction{std::move(physicalEvents)});
            command.append(SpawnGovernmentCoreExplosionAction{
                {game::kGovernmentBaseCenter.x, 0.42f,
                 game::kGovernmentBaseCenter.z},
                {255U, 195U, 55U, 255U}});
            command.append(RequestMapCoreAudioAction{
                audio::AudioCue::EagleDestroyed});
        }
    }
    else
    {
        return std::nullopt;
    }

    command.append(CommitMapCoreShellImpactAction{outcome.position});
    return command;
}
} // namespace tanks3d::app
