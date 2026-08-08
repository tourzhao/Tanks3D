#include "app/command_side_effect_dispatch.h"

#include <cstddef>
#include <type_traits>
#include <variant>

namespace tanks3d::app
{
namespace
{
bool validAudioCue(audio::AudioCue cue)
{
    return static_cast<std::size_t>(cue) <
           static_cast<std::size_t>(audio::AudioCue::Count);
}

template <typename... Visitors>
struct Overloaded : Visitors...
{
    using Visitors::operator()...;
};

template <typename... Visitors>
Overloaded(Visitors...) -> Overloaded<Visitors...>;
} // namespace

CommandSideEffectDisposition dispatchCommandSideEffect(
    const AppendShellCancellationEventAction &action,
    CommandSideEffectSink &sink)
{
    sink.emitEvent(action.event);
    return CommandSideEffectDisposition::Applied;
}

CommandSideEffectDisposition dispatchCommandSideEffect(
    const SpawnShellCancellationImpactAction &action,
    CommandSideEffectSink &sink)
{
    sink.spawnImpact(
        {action.position.x, action.elevation, action.position.z},
        {action.normalXZ.x, action.normalY, action.normalXZ.z},
        action.heavy);
    return CommandSideEffectDisposition::Applied;
}

CommandSideEffectDisposition dispatchCommandSideEffect(
    const RequestShellCancellationAudioAction &action,
    CommandSideEffectSink &sink)
{
    if (!validAudioCue(action.cue))
        return CommandSideEffectDisposition::Rejected;
    sink.requestAudio(action.cue);
    return CommandSideEffectDisposition::Applied;
}

CommandSideEffectDisposition dispatchCommandSideEffect(
    const ShellMapCorePresentationAction &action,
    CommandSideEffectSink &sink)
{
    return std::visit(
        Overloaded{
            [&](const AppendMapCoreEventsAction &append) {
                for (const game::GameEvent &event : append.events)
                    sink.emitEvent(event);
                return CommandSideEffectDisposition::Applied;
            },
            [&](const SpawnMapCoreBrickImpactAction &brick) {
                sink.spawnBrickImpact(brick.position, brick.normal,
                                      brick.power, brick.destroyed);
                return CommandSideEffectDisposition::Applied;
            },
            [&](const SpawnGovernmentWallBreachImpactAction &breach) {
                sink.spawnImpact(breach.position, breach.normal,
                                 breach.heavy);
                return CommandSideEffectDisposition::Applied;
            },
            [&](const SpawnMapCoreSurfaceImpactAction &surface) {
                sink.spawnImpact(surface.position, surface.normal,
                                 surface.heavy);
                return CommandSideEffectDisposition::Applied;
            },
            [&](const ApplyMapCoreRadialCameraShakeAction &radial) {
                sink.applyRadialCameraShake(
                    radial.origin, radial.maximum,
                    radial.distanceFalloff);
                return CommandSideEffectDisposition::Applied;
            },
            [&](const SpawnGovernmentCoreExplosionAction &explosion) {
                sink.spawnExplosion(explosion.position, explosion.color);
                return CommandSideEffectDisposition::Applied;
            },
            [&](const RequestMapCoreAudioAction &request) {
                if (!validAudioCue(request.cue))
                    return CommandSideEffectDisposition::Rejected;
                sink.requestAudio(request.cue);
                return CommandSideEffectDisposition::Applied;
            },
            [&](const CommitMapCoreShellImpactAction &) {
                return CommandSideEffectDisposition::DomainCommitRequired;
            }},
        action);
}

CommandSideEffectDisposition dispatchCommandSideEffect(
    const ShellTankPresentationAction &action,
    CommandSideEffectSink &sink)
{
    return std::visit(
        Overloaded{
            [&](const AppendTankEventsAction &append) {
                for (const game::GameEvent &event : append.events)
                    sink.emitEvent(event);
                return CommandSideEffectDisposition::Applied;
            },
            [&](const SpawnTankArmorImpactAction &armor) {
                sink.spawnImpact(armor.position, armor.normal, armor.heavy);
                return CommandSideEffectDisposition::Applied;
            },
            [&](const SpawnTankExplosionAction &explosion) {
                sink.spawnExplosion(explosion.position, explosion.color);
                return CommandSideEffectDisposition::Applied;
            },
            [&](const ApplyTankRadialCameraShakeAction &radial) {
                sink.applyRadialCameraShake(
                    radial.origin, radial.maximum,
                    radial.distanceFalloff);
                return CommandSideEffectDisposition::Applied;
            },
            [&](const AssignTankTargetCameraShakeAction &target) {
                return sink.assignPlayerCameraShake(
                           target.playerIndex, target.value)
                           ? CommandSideEffectDisposition::Applied
                           : CommandSideEffectDisposition::Rejected;
            },
            [&](const RequestTankAudioAction &request) {
                if (!validAudioCue(request.cue))
                    return CommandSideEffectDisposition::Rejected;
                sink.requestAudio(request.cue);
                return CommandSideEffectDisposition::Applied;
            },
            [&](const CommitTankShellImpactAction &) {
                return CommandSideEffectDisposition::DomainCommitRequired;
            }},
        action);
}

CommandSideEffectDisposition dispatchCommandSideEffect(
    const game::BonusEffectCommand &command,
    CommandSideEffectSink &sink)
{
    switch (command.type)
    {
    case game::BonusEffectCommandType::EnemyArmorHitCue:
        sink.requestAudio(audio::AudioCue::EnemyHit);
        return CommandSideEffectDisposition::Applied;
    case game::BonusEffectCommandType::EnemyDestroyedCue:
        sink.requestAudio(audio::AudioCue::EnemyDestroyed);
        return CommandSideEffectDisposition::Applied;
    case game::BonusEffectCommandType::EnemyExplosion:
        sink.spawnExplosion(
            {command.position.x, 0.42f, command.position.z},
            {255U, 105U, 27U, 255U});
        return CommandSideEffectDisposition::Applied;
    case game::BonusEffectCommandType::EnemyDestroyedEvent:
    {
        game::GameEvent event;
        event.type = game::GameEventType::TankDestroyed;
        event.cause = game::GameEventCause::GrenadeBonus;
        event.position = command.position;
        event.sourcePlayerId = command.playerId;
        event.targetEnemyId = command.enemyId;
        event.enemyType = command.enemyType;
        event.valueBefore = command.valueBefore;
        event.valueAfter = command.valueAfter;
        event.points = command.points;
        sink.emitEvent(event);
        return CommandSideEffectDisposition::Applied;
    }
    case game::BonusEffectCommandType::ActivateGovernmentSteel:
        return CommandSideEffectDisposition::DomainCommitRequired;
    case game::BonusEffectCommandType::AssignPlayerCameraShake:
        if (command.playerIndex < 0 ||
            !sink.assignPlayerCameraShake(
                static_cast<std::size_t>(command.playerIndex),
                command.scalar))
        {
            // Bonus camera assignment historically ignores an invalid player
            // index; tank-hit commands intentionally reject it instead.
            return CommandSideEffectDisposition::Ignored;
        }
        return CommandSideEffectDisposition::Applied;
    case game::BonusEffectCommandType::Count:
        return CommandSideEffectDisposition::Ignored;
    }
    return CommandSideEffectDisposition::Ignored;
}
} // namespace tanks3d::app
