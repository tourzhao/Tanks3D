#include "app/shell_tank_presentation.h"
#include "game/combat_system.h"

#include "test_support.h"

#include <cmath>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace
{
using tanks3d::app::AppendTankEventsAction;
using tanks3d::app::ApplyTankRadialCameraShakeAction;
using tanks3d::app::AssignTankTargetCameraShakeAction;
using tanks3d::app::CommitTankShellImpactAction;
using tanks3d::app::Float3;
using tanks3d::app::RequestTankAudioAction;
using tanks3d::app::Rgba8;
using tanks3d::app::ShellTankPresentationAction;
using tanks3d::app::ShellTankPresentationCommand;
using tanks3d::app::ShellTankPresentationStep;
using tanks3d::app::SpawnTankArmorImpactAction;
using tanks3d::app::SpawnTankExplosionAction;
using tanks3d::app::kMaximumShellTankPresentationActions;
using tanks3d::app::makePlayerTankArmorImpactAction;
using tanks3d::app::makeShellTankPresentationCommand;
using tanks3d::app::shellTankPresentationStep;
using tanks3d::audio::AudioCue;
using tanks3d::core::XZ;
using tanks3d::game::CombatTarget;
using tanks3d::game::GameEvent;
using tanks3d::game::GameEventCause;
using tanks3d::game::GameEventType;
using tanks3d::game::PlayerHitResult;
using tanks3d::game::ShellPhysicalImpactResult;

static_assert(std::variant_size_v<ShellTankPresentationAction> == 7U,
              "every tank presentation action needs an explicit step");
static_assert(kMaximumShellTankPresentationActions == 6U,
              "the fixed command capacity changed");
static_assert(static_cast<std::size_t>(AudioCue::Count) == 22U,
              "audio cue ordinals no longer match the audio metadata");

bool nearlyEqual(float first, float second, float tolerance = 0.00001f)
{
    return std::fabs(first - second) <= tolerance;
}

bool sameXZ(XZ first, XZ second)
{
    return nearlyEqual(first.x, second.x) &&
           nearlyEqual(first.z, second.z);
}

bool sameFloat3(Float3 first, Float3 second)
{
    return nearlyEqual(first.x, second.x) &&
           nearlyEqual(first.y, second.y) &&
           nearlyEqual(first.z, second.z);
}

bool sameColor(Rgba8 first, Rgba8 second)
{
    return first.r == second.r && first.g == second.g &&
           first.b == second.b && first.a == second.a;
}

template <typename Action>
const Action *actionAt(const ShellTankPresentationCommand &command,
                       std::size_t index)
{
    if (index >= command.actionCount || index >= command.actions.size())
        return nullptr;
    return std::get_if<Action>(&command.actions[index]);
}

template <typename... Actions>
bool actionTypesAre(const ShellTankPresentationCommand &command)
{
    if (command.actionCount != sizeof...(Actions))
        return false;
    std::size_t index = 0U;
    return (... && (actionAt<Actions>(command, index++) != nullptr));
}

GameEvent testEvent()
{
    GameEvent event;
    event.type = GameEventType::TankDamaged;
    event.cause = GameEventCause::PlayerShell;
    event.position = {17.25f, 18.75f};
    event.sourcePlayerId = 1;
    event.targetEnemyId = 73;
    event.enemyType = 2;
    event.valueBefore = 2;
    event.valueAfter = 1;
    event.points = 50;
    return event;
}

ShellPhysicalImpactResult enemyResult(bool destroyed)
{
    ShellPhysicalImpactResult result;
    result.resolved = true;
    result.outcome.target = CombatTarget::EnemyTank;
    result.outcome.position = {7.25f, 8.75f};
    result.outcome.incomingVelocity = {3.0f, 4.0f};
    result.outcome.targetEnemyPosition = {11.0f, 12.0f};
    result.enemyCommit.applied = true;
    result.enemyCommit.destroyedNow = destroyed;
    return result;
}

ShellPhysicalImpactResult playerResult(PlayerHitResult hitResult)
{
    ShellPhysicalImpactResult result;
    result.resolved = true;
    result.outcome.target = CombatTarget::PlayerTank;
    result.outcome.position = {7.25f, 8.75f};
    result.outcome.incomingVelocity = {3.0f, 4.0f};
    result.outcome.targetPlayerIndex = 1;
    result.outcome.targetPlayerId = 9;
    result.outcome.targetPlayerPosition = {13.0f, 14.0f};
    result.playerCommit.applied = true;
    result.playerCommit.hitResult = hitResult;
    return result;
}
} // namespace

int main()
{
    tanks3d_test::Reporter reporter;
    reporter.reset();
    bool passed = true;
    const auto expect = [&](bool condition, const std::string &message) {
        passed = reporter.check(condition, message) && passed;
    };

    const GameEvent event = testEvent();
    constexpr Rgba8 kPlayerColor{12U, 34U, 56U, 78U};
    constexpr XZ kContact{7.25f, 8.75f};

    reporter.beginSuite("shell-tank-presentation-command-branches");
    const ShellTankPresentationCommand enemyHit =
        makeShellTankPresentationCommand(
            enemyResult(false), std::vector<GameEvent>{event},
            kPlayerColor);
    const auto *enemyHitEvents =
        actionAt<AppendTankEventsAction>(enemyHit, 0U);
    const auto *enemyHitArmor =
        actionAt<SpawnTankArmorImpactAction>(enemyHit, 1U);
    const auto *enemyHitCamera =
        actionAt<ApplyTankRadialCameraShakeAction>(enemyHit, 2U);
    const auto *enemyHitAudio =
        actionAt<RequestTankAudioAction>(enemyHit, 3U);
    const auto *enemyHitCommit =
        actionAt<CommitTankShellImpactAction>(enemyHit, 4U);
    expect(
        actionTypesAre<AppendTankEventsAction,
                       SpawnTankArmorImpactAction,
                       ApplyTankRadialCameraShakeAction,
                       RequestTankAudioAction,
                       CommitTankShellImpactAction>(enemyHit) &&
            enemyHitEvents != nullptr &&
            enemyHitEvents->events == std::vector<GameEvent>{event} &&
            enemyHitArmor != nullptr && enemyHitArmor->heavy &&
            sameFloat3(enemyHitArmor->position,
                       {kContact.x, 0.52f, kContact.z}) &&
            sameFloat3(enemyHitArmor->normal, {-0.6f, 0.28f, -0.8f}) &&
            enemyHitCamera != nullptr &&
            sameXZ(enemyHitCamera->origin, {11.0f, 12.0f}) &&
            nearlyEqual(enemyHitCamera->maximum, 0.13f) &&
            nearlyEqual(enemyHitCamera->distanceFalloff, 0.008f) &&
            enemyHitAudio != nullptr &&
            enemyHitAudio->cue == AudioCue::EnemyHit &&
            enemyHitCommit != nullptr &&
            sameXZ(enemyHitCommit->position, kContact),
        "nonfatal enemy mapping changed its order or payload");

    const ShellTankPresentationCommand enemyDeath =
        makeShellTankPresentationCommand(
            enemyResult(true), std::vector<GameEvent>{event},
            kPlayerColor);
    const auto *enemyDeathExplosion =
        actionAt<SpawnTankExplosionAction>(enemyDeath, 0U);
    const auto *enemyDeathCamera =
        actionAt<ApplyTankRadialCameraShakeAction>(enemyDeath, 1U);
    const auto *enemyDeathAudio =
        actionAt<RequestTankAudioAction>(enemyDeath, 2U);
    const auto *enemyDeathEvents =
        actionAt<AppendTankEventsAction>(enemyDeath, 3U);
    const auto *enemyDeathCommit =
        actionAt<CommitTankShellImpactAction>(enemyDeath, 4U);
    expect(
        actionTypesAre<SpawnTankExplosionAction,
                       ApplyTankRadialCameraShakeAction,
                       RequestTankAudioAction, AppendTankEventsAction,
                       CommitTankShellImpactAction>(enemyDeath) &&
            enemyDeathExplosion != nullptr &&
            sameFloat3(enemyDeathExplosion->position,
                       {11.0f, 0.42f, 12.0f}) &&
            sameColor(enemyDeathExplosion->color,
                      {255U, 105U, 27U, 255U}) &&
            enemyDeathCamera != nullptr &&
            sameXZ(enemyDeathCamera->origin, {11.0f, 12.0f}) &&
            nearlyEqual(enemyDeathCamera->maximum, 0.24f) &&
            nearlyEqual(enemyDeathCamera->distanceFalloff, 0.012f) &&
            enemyDeathAudio != nullptr &&
            enemyDeathAudio->cue == AudioCue::EnemyDestroyed &&
            enemyDeathEvents != nullptr &&
            enemyDeathEvents->events == std::vector<GameEvent>{event} &&
            enemyDeathCommit != nullptr &&
            sameXZ(enemyDeathCommit->position, kContact),
        "fatal enemy mapping lost explosion-audio-event asymmetry");

    const ShellTankPresentationCommand shield =
        makeShellTankPresentationCommand(
            playerResult(PlayerHitResult::Shielded),
            std::vector<GameEvent>{event}, kPlayerColor);
    const auto *shieldCommit =
        actionAt<CommitTankShellImpactAction>(shield, 0U);
    expect(actionTypesAre<CommitTankShellImpactAction>(shield) &&
               shieldCommit != nullptr &&
               sameXZ(shieldCommit->position, kContact),
           "Shield mapping is no longer a silent final impact");

    const ShellTankPresentationCommand boat =
        makeShellTankPresentationCommand(
            playerResult(PlayerHitResult::BoatAbsorbed),
            std::vector<GameEvent>{event}, kPlayerColor);
    const auto *boatCamera =
        actionAt<AssignTankTargetCameraShakeAction>(boat, 0U);
    const auto *boatCommit =
        actionAt<CommitTankShellImpactAction>(boat, 1U);
    expect(actionTypesAre<AssignTankTargetCameraShakeAction,
                          CommitTankShellImpactAction>(boat) &&
               boatCamera != nullptr && boatCamera->playerIndex == 1U &&
               nearlyEqual(boatCamera->value, 0.16f) &&
               boatCommit != nullptr &&
               sameXZ(boatCommit->position, kContact),
           "Boat mapping changed its target index, shake, or impact");

    const ShellTankPresentationCommand playerDamage =
        makeShellTankPresentationCommand(
            playerResult(PlayerHitResult::Damaged),
            std::vector<GameEvent>{event}, kPlayerColor);
    const auto *playerDamageEvents =
        actionAt<AppendTankEventsAction>(playerDamage, 0U);
    const auto *playerDamageCamera =
        actionAt<AssignTankTargetCameraShakeAction>(playerDamage, 1U);
    const auto *playerDamageAudio =
        actionAt<RequestTankAudioAction>(playerDamage, 2U);
    const auto *playerDamageCommit =
        actionAt<CommitTankShellImpactAction>(playerDamage, 3U);
    expect(
        actionTypesAre<AppendTankEventsAction,
                       AssignTankTargetCameraShakeAction,
                       RequestTankAudioAction,
                       CommitTankShellImpactAction>(playerDamage) &&
            playerDamageEvents != nullptr &&
            playerDamageEvents->events == std::vector<GameEvent>{event} &&
            playerDamageCamera != nullptr &&
            playerDamageCamera->playerIndex == 1U &&
            nearlyEqual(playerDamageCamera->value, 0.22f) &&
            playerDamageAudio != nullptr &&
            playerDamageAudio->cue == AudioCue::PlayerHit &&
            playerDamageCommit != nullptr &&
            sameXZ(playerDamageCommit->position, kContact),
        "nonfatal player mapping changed its event-camera-audio order");

    const ShellTankPresentationCommand playerDeath =
        makeShellTankPresentationCommand(
            playerResult(PlayerHitResult::Destroyed),
            std::vector<GameEvent>{event}, kPlayerColor);
    const auto *playerDeathEvents =
        actionAt<AppendTankEventsAction>(playerDeath, 0U);
    const auto *playerDeathExplosion =
        actionAt<SpawnTankExplosionAction>(playerDeath, 1U);
    const auto *playerDeathCamera =
        actionAt<AssignTankTargetCameraShakeAction>(playerDeath, 2U);
    const auto *playerDeathAudio =
        actionAt<RequestTankAudioAction>(playerDeath, 3U);
    const auto *playerDeathCommit =
        actionAt<CommitTankShellImpactAction>(playerDeath, 4U);
    expect(
        actionTypesAre<AppendTankEventsAction,
                       SpawnTankExplosionAction,
                       AssignTankTargetCameraShakeAction,
                       RequestTankAudioAction,
                       CommitTankShellImpactAction>(playerDeath) &&
            playerDeathEvents != nullptr &&
            playerDeathEvents->events == std::vector<GameEvent>{event} &&
            playerDeathExplosion != nullptr &&
            sameFloat3(playerDeathExplosion->position,
                       {13.0f, 0.42f, 14.0f}) &&
            sameColor(playerDeathExplosion->color, kPlayerColor) &&
            playerDeathCamera != nullptr &&
            playerDeathCamera->playerIndex == 1U &&
            nearlyEqual(playerDeathCamera->value, 0.42f) &&
            playerDeathAudio != nullptr &&
            playerDeathAudio->cue == AudioCue::PlayerDestroyed &&
            playerDeathCommit != nullptr &&
            sameXZ(playerDeathCommit->position, kContact),
        "fatal player mapping changed its injected color or action order");

    reporter.beginSuite("shell-tank-presentation-command-boundaries");
    bool nonTankTargetsStayEmpty = true;
    for (CombatTarget target : {CombatTarget::None,
                                CombatTarget::StageMap,
                                CombatTarget::GovernmentCore})
    {
        ShellPhysicalImpactResult result;
        result.resolved = true;
        result.outcome.target = target;
        nonTankTargetsStayEmpty = nonTankTargetsStayEmpty &&
            makeShellTankPresentationCommand(
                result, std::vector<GameEvent>{event}, kPlayerColor)
                    .actionCount == 0U;
    }
    expect(nonTankTargetsStayEmpty,
           "a non-tank result produced a tank presentation command");

    ShellPhysicalImpactResult unresolvedEnemy = enemyResult(false);
    unresolvedEnemy.resolved = false;
    ShellPhysicalImpactResult unresolvedPlayer =
        playerResult(PlayerHitResult::Damaged);
    unresolvedPlayer.resolved = false;
    expect(makeShellTankPresentationCommand(
               unresolvedEnemy, std::vector<GameEvent>{event}, kPlayerColor)
                   .actionCount == 0U &&
               makeShellTankPresentationCommand(
                   unresolvedPlayer, std::vector<GameEvent>{event},
                   kPlayerColor)
                       .actionCount == 0U,
           "an unresolved tank snapshot produced a command");

    ShellPhysicalImpactResult uncommittedEnemy = enemyResult(false);
    uncommittedEnemy.enemyCommit.applied = false;
    ShellPhysicalImpactResult uncommittedPlayer =
        playerResult(PlayerHitResult::Damaged);
    uncommittedPlayer.playerCommit.applied = false;
    expect(makeShellTankPresentationCommand(
               uncommittedEnemy, std::vector<GameEvent>{event}, kPlayerColor)
                   .actionCount == 0U &&
               makeShellTankPresentationCommand(
                   uncommittedPlayer, std::vector<GameEvent>{event},
                   kPlayerColor)
                       .actionCount == 0U,
           "an unapplied tank commit produced a command");

    ShellPhysicalImpactResult negativePlayerIndex =
        playerResult(PlayerHitResult::Destroyed);
    negativePlayerIndex.outcome.targetPlayerIndex = -1;
    expect(makeShellTankPresentationCommand(
               negativePlayerIndex, std::vector<GameEvent>{event},
               kPlayerColor)
                   .actionCount == 0U,
           "a negative player target index produced a command");

    const SpawnTankArmorImpactAction playerArmor =
        makePlayerTankArmorImpactAction({2.25f, 3.50f},
                                        {-3.0f, 4.0f});
    expect(playerArmor.heavy &&
               sameFloat3(playerArmor.position,
                          {2.25f, 0.52f, 3.50f}) &&
               sameFloat3(playerArmor.normal,
                          {0.6f, 0.28f, -0.8f}),
           "player pre-commit armor projection changed");

    reporter.beginSuite("shell-tank-presentation-step-mapping");
    expect(shellTankPresentationStep(AppendTankEventsAction{}) ==
               ShellTankPresentationStep::PhysicalEventsAppended,
           "AppendTankEventsAction mapped to the wrong step");
    expect(shellTankPresentationStep(SpawnTankArmorImpactAction{}) ==
               ShellTankPresentationStep::ArmorImpactFxSpawned,
           "SpawnTankArmorImpactAction mapped to the wrong step");
    expect(shellTankPresentationStep(SpawnTankExplosionAction{}) ==
               ShellTankPresentationStep::TankExplosionFxSpawned,
           "SpawnTankExplosionAction mapped to the wrong step");
    expect(shellTankPresentationStep(
               ApplyTankRadialCameraShakeAction{}) ==
               ShellTankPresentationStep::CameraShakePassCompleted,
           "ApplyTankRadialCameraShakeAction mapped to the wrong step");
    expect(shellTankPresentationStep(
               AssignTankTargetCameraShakeAction{}) ==
               ShellTankPresentationStep::TargetCameraShakeAssigned,
           "AssignTankTargetCameraShakeAction mapped to the wrong step");
    expect(shellTankPresentationStep(RequestTankAudioAction{}) ==
               ShellTankPresentationStep::AudioRequested,
           "RequestTankAudioAction mapped to the wrong step");
    expect(shellTankPresentationStep(CommitTankShellImpactAction{}) ==
               ShellTankPresentationStep::ShellImpactCommitted,
           "CommitTankShellImpactAction mapped to the wrong step");

    reporter.finish();
    return passed ? 0 : 1;
}
