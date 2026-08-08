#include "app/shell_tank_presentation.h"

#include "game/combat_system.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <utility>

namespace tanks3d::app
{
void ShellTankPresentationCommand::append(
    ShellTankPresentationAction action)
{
    assert(actionCount < actions.size());
    actions.at(actionCount) = std::move(action);
    ++actionCount;
}

ShellTankPresentationStep shellTankPresentationStep(
    const ShellTankPresentationAction &action)
{
    if (std::holds_alternative<AppendTankEventsAction>(action))
        return ShellTankPresentationStep::PhysicalEventsAppended;
    if (std::holds_alternative<SpawnTankArmorImpactAction>(action))
        return ShellTankPresentationStep::ArmorImpactFxSpawned;
    if (std::holds_alternative<SpawnTankExplosionAction>(action))
        return ShellTankPresentationStep::TankExplosionFxSpawned;
    if (std::holds_alternative<ApplyTankRadialCameraShakeAction>(action))
        return ShellTankPresentationStep::CameraShakePassCompleted;
    if (std::holds_alternative<AssignTankTargetCameraShakeAction>(action))
        return ShellTankPresentationStep::TargetCameraShakeAssigned;
    if (std::holds_alternative<RequestTankAudioAction>(action))
        return ShellTankPresentationStep::AudioRequested;
    if (std::holds_alternative<CommitTankShellImpactAction>(action))
        return ShellTankPresentationStep::ShellImpactCommitted;
    assert(false && "unhandled shell-tank presentation action");
    return ShellTankPresentationStep::ShellImpactCommitted;
}

SpawnTankArmorImpactAction makePlayerTankArmorImpactAction(
    core::XZ contactPosition, core::XZ incomingVelocity)
{
    const float impactSpeed = std::max(
        0.001f, std::sqrt(core::lengthSquared(incomingVelocity)));
    return {{contactPosition.x, 0.52f, contactPosition.z},
            {-incomingVelocity.x / impactSpeed, 0.28f,
             -incomingVelocity.z / impactSpeed},
            true};
}

ShellTankPresentationCommand makeShellTankPresentationCommand(
    const game::ShellPhysicalImpactResult &physicalImpact,
    std::vector<game::GameEvent> physicalEvents,
    Rgba8 playerExplosionColor)
{
    ShellTankPresentationCommand command;
    if (!physicalImpact.resolved)
        return command;

    const game::CombatOutcome &outcome = physicalImpact.outcome;
    if (outcome.target == game::CombatTarget::EnemyTank)
    {
        if (!physicalImpact.enemyCommit.applied)
            return command;
        if (physicalImpact.enemyCommit.destroyedNow)
        {
            command.append(SpawnTankExplosionAction{
                {outcome.targetEnemyPosition.x, 0.42f,
                 outcome.targetEnemyPosition.z},
                {255U, 105U, 27U, 255U}});
            command.append(ApplyTankRadialCameraShakeAction{
                outcome.targetEnemyPosition, 0.24f, 0.012f});
            command.append(RequestTankAudioAction{
                audio::AudioCue::EnemyDestroyed});
            command.append(
                AppendTankEventsAction{std::move(physicalEvents)});
        }
        else
        {
            command.append(
                AppendTankEventsAction{std::move(physicalEvents)});
            const float impactSpeed = std::max(
                0.001f,
                std::sqrt(core::lengthSquared(outcome.incomingVelocity)));
            command.append(SpawnTankArmorImpactAction{
                {outcome.position.x, 0.52f, outcome.position.z},
                {-outcome.incomingVelocity.x / impactSpeed, 0.28f,
                 -outcome.incomingVelocity.z / impactSpeed},
                true});
            command.append(ApplyTankRadialCameraShakeAction{
                outcome.targetEnemyPosition, 0.13f, 0.008f});
            command.append(
                RequestTankAudioAction{audio::AudioCue::EnemyHit});
        }
    }
    else if (outcome.target == game::CombatTarget::PlayerTank)
    {
        if (!physicalImpact.playerCommit.applied ||
            outcome.targetPlayerIndex < 0)
        {
            return command;
        }
        const std::size_t playerIndex = static_cast<std::size_t>(
            outcome.targetPlayerIndex);
        switch (physicalImpact.playerCommit.hitResult)
        {
        case game::PlayerHitResult::Shielded:
            break;
        case game::PlayerHitResult::BoatAbsorbed:
            command.append(AssignTankTargetCameraShakeAction{
                playerIndex, 0.16f});
            break;
        case game::PlayerHitResult::Damaged:
            command.append(
                AppendTankEventsAction{std::move(physicalEvents)});
            command.append(AssignTankTargetCameraShakeAction{
                playerIndex, 0.22f});
            command.append(RequestTankAudioAction{
                audio::AudioCue::PlayerHit});
            break;
        case game::PlayerHitResult::Destroyed:
            command.append(
                AppendTankEventsAction{std::move(physicalEvents)});
            command.append(SpawnTankExplosionAction{
                {outcome.targetPlayerPosition.x, 0.42f,
                 outcome.targetPlayerPosition.z},
                playerExplosionColor});
            command.append(AssignTankTargetCameraShakeAction{
                playerIndex, 0.42f});
            command.append(RequestTankAudioAction{
                audio::AudioCue::PlayerDestroyed});
            break;
        }
    }
    else
    {
        return command;
    }

    command.append(CommitTankShellImpactAction{outcome.position});
    return command;
}
} // namespace tanks3d::app
