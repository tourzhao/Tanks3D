#include "app/shell_map_core_presentation.h"
#include "game/combat_system.h"

#include "test_support.h"

#include <cmath>
#include <limits>
#include <string>
#include <variant>
#include <vector>

namespace
{
using tanks3d::app::AppendMapCoreEventsAction;
using tanks3d::app::ApplyMapCoreRadialCameraShakeAction;
using tanks3d::app::CommitMapCoreShellImpactAction;
using tanks3d::app::Float3;
using tanks3d::app::RequestMapCoreAudioAction;
using tanks3d::app::Rgba8;
using tanks3d::app::ShellMapCorePresentationAction;
using tanks3d::app::ShellMapCorePresentationCommand;
using tanks3d::app::ShellMapCorePresentationStep;
using tanks3d::app::SpawnGovernmentCoreExplosionAction;
using tanks3d::app::SpawnGovernmentWallBreachImpactAction;
using tanks3d::app::SpawnMapCoreBrickImpactAction;
using tanks3d::app::SpawnMapCoreSurfaceImpactAction;
using tanks3d::app::kMaximumShellMapCorePresentationActions;
using tanks3d::app::makeShellMapCorePresentationCommand;
using tanks3d::app::shellMapCorePresentationStep;
using tanks3d::audio::AudioCue;
using tanks3d::core::XZ;
using tanks3d::game::BrickDamage;
using tanks3d::game::CombatTarget;
using tanks3d::game::GameEvent;
using tanks3d::game::GameEventType;
using tanks3d::game::GovernmentBasePart;
using tanks3d::game::ImpactKind;
using tanks3d::game::ShellOwner;
using tanks3d::game::ShellPhysicalImpactResult;
using tanks3d::game::kGovernmentBaseCenter;

static_assert(std::variant_size_v<ShellMapCorePresentationAction> == 8U,
              "every map/core action needs an explicit step");
static_assert(kMaximumShellMapCorePresentationActions == 6U,
              "the fixed map/core command capacity changed");

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
const Action *actionAt(const ShellMapCorePresentationCommand &command,
                       std::size_t index)
{
    if (index >= command.actionCount || index >= command.actions.size())
        return nullptr;
    return std::get_if<Action>(&command.actions[index]);
}

template <typename... Actions>
bool actionTypesAre(const ShellMapCorePresentationCommand &command)
{
    if (command.actionCount != sizeof...(Actions))
        return false;
    std::size_t index = 0U;
    return (... && (actionAt<Actions>(command, index++) != nullptr));
}

ShellPhysicalImpactResult mapResult(ImpactKind impact,
                                    ShellOwner owner = ShellOwner::Player,
                                    bool power = false)
{
    ShellPhysicalImpactResult result;
    result.resolved = true;
    result.outcome.target = CombatTarget::StageMap;
    result.outcome.impactKind = impact;
    result.outcome.position = {7.25f, 8.75f};
    result.outcome.incomingVelocity = {3.0f, 4.0f};
    result.outcome.owner = owner;
    result.outcome.ownerIndex = owner == ShellOwner::Player ? 1 : 73;
    result.outcome.power = power;
    return result;
}

GameEvent brickEvent(const ShellPhysicalImpactResult &result,
                     const BrickDamage &damage)
{
    GameEvent event = tanks3d::game::shellEvent(
        GameEventType::BrickHit, result.outcome.owner,
        result.outcome.ownerIndex, result.outcome.position,
        result.outcome.power);
    event.impactKind = ImpactKind::Brick;
    event.row = damage.row;
    event.column = damage.column;
    event.valueBefore = damage.beforeMask;
    event.valueAfter = damage.afterMask;
    return event;
}

GameEvent wallEvent(const ShellPhysicalImpactResult &result)
{
    const auto &details = result.outcome.impactDetails;
    GameEvent event = tanks3d::game::shellEvent(
        GameEventType::BaseDamaged, result.outcome.owner,
        result.outcome.ownerIndex, result.outcome.position,
        result.outcome.power);
    event.impactKind = ImpactKind::GovernmentWall;
    event.basePart = GovernmentBasePart::Wall;
    event.baseSegmentIndex = details.governmentWallIndex;
    event.valueBefore = details.governmentWallHealthBefore;
    event.valueAfter = details.governmentWallHealthAfter;
    return event;
}

ShellPhysicalImpactResult coreResult(bool live,
                                     ShellOwner owner = ShellOwner::Enemy)
{
    ShellPhysicalImpactResult result;
    result.resolved = true;
    result.outcome.target = CombatTarget::GovernmentCore;
    result.outcome.position = {13.55f, 23.35f};
    result.outcome.incomingVelocity = {-2.0f, 0.0f};
    result.outcome.owner = owner;
    result.outcome.ownerIndex = owner == ShellOwner::Player ? 0 : 81;
    result.outcome.governmentCoreHealthBefore = live ? 1 : 0;
    result.outcome.governmentCoreHealthAfter = 0;
    return result;
}

GameEvent coreEvent(const ShellPhysicalImpactResult &result)
{
    GameEvent event = tanks3d::game::shellEvent(
        GameEventType::BaseDamaged, result.outcome.owner,
        result.outcome.ownerIndex, result.outcome.position,
        result.outcome.power);
    event.basePart = GovernmentBasePart::Core;
    event.valueBefore = result.outcome.governmentCoreHealthBefore;
    event.valueAfter = result.outcome.governmentCoreHealthAfter;
    return event;
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

    reporter.beginSuite("shell-map-core-presentation-command-branches");

    ShellPhysicalImpactResult brick = mapResult(ImpactKind::Brick);
    brick.outcome.impactDetails.brickCount = 1;
    brick.outcome.impactDetails.bricks[0] = {5, 6, 0x0fU, 0x00U};
    const GameEvent brickHit =
        brickEvent(brick, brick.outcome.impactDetails.bricks[0]);
    const auto brickCommand = makeShellMapCorePresentationCommand(
        brick, std::vector<GameEvent>{brickHit});
    const auto *brickEvents = brickCommand
        ? actionAt<AppendMapCoreEventsAction>(*brickCommand, 0U)
        : nullptr;
    const auto *brickFx = brickCommand
        ? actionAt<SpawnMapCoreBrickImpactAction>(*brickCommand, 1U)
        : nullptr;
    const auto *brickAudio = brickCommand
        ? actionAt<RequestMapCoreAudioAction>(*brickCommand, 2U)
        : nullptr;
    const auto *brickCommit = brickCommand
        ? actionAt<CommitMapCoreShellImpactAction>(*brickCommand, 3U)
        : nullptr;
    expect(brickCommand &&
               actionTypesAre<AppendMapCoreEventsAction,
                              SpawnMapCoreBrickImpactAction,
                              RequestMapCoreAudioAction,
                              CommitMapCoreShellImpactAction>(*brickCommand) &&
               brickEvents != nullptr &&
               brickEvents->events == std::vector<GameEvent>{brickHit} &&
               brickFx != nullptr &&
               sameFloat3(brickFx->position, {7.25f, 0.47f, 8.75f}) &&
               sameFloat3(brickFx->normal, {-0.6f, 0.28f, -0.8f}) &&
               !brickFx->power && brickFx->destroyed &&
               brickAudio != nullptr &&
               brickAudio->cue == AudioCue::BrickHit &&
               brickCommit != nullptr &&
               sameXZ(brickCommit->position, brick.outcome.position),
           "player brick command changed its event, FX, audio, or commit");

    ShellPhysicalImpactResult wall = mapResult(
        ImpactKind::GovernmentWall, ShellOwner::Enemy);
    wall.outcome.impactDetails.governmentWallIndex = 2;
    wall.outcome.impactDetails.governmentWallHealthBefore = 4;
    wall.outcome.impactDetails.governmentWallHealthAfter = 3;
    const GameEvent wallHit = wallEvent(wall);
    const auto wallCommand = makeShellMapCorePresentationCommand(
        wall, std::vector<GameEvent>{wallHit});
    const auto *wallFx = wallCommand
        ? actionAt<SpawnMapCoreBrickImpactAction>(*wallCommand, 1U)
        : nullptr;
    expect(wallCommand &&
               actionTypesAre<AppendMapCoreEventsAction,
                              SpawnMapCoreBrickImpactAction,
                              CommitMapCoreShellImpactAction>(*wallCommand) &&
               wallFx != nullptr &&
               sameFloat3(wallFx->position, {7.25f, 0.54f, 8.75f}) &&
               sameFloat3(wallFx->normal, {-0.6f, 0.28f, -0.8f}) &&
               !wallFx->power && !wallFx->destroyed,
           "enemy nonlethal wall command gained audio or wrong FX");

    ShellPhysicalImpactResult breach = mapResult(
        ImpactKind::GovernmentWall, ShellOwner::Player, true);
    breach.outcome.impactDetails.governmentWallIndex = 1;
    breach.outcome.impactDetails.governmentWallHealthBefore = 2;
    breach.outcome.impactDetails.governmentWallHealthAfter = 0;
    const GameEvent breachedWall = wallEvent(breach);
    const auto breachCommand = makeShellMapCorePresentationCommand(
        breach, std::vector<GameEvent>{breachedWall});
    const auto *breachBrick = breachCommand
        ? actionAt<SpawnMapCoreBrickImpactAction>(*breachCommand, 1U)
        : nullptr;
    const auto *breachFx = breachCommand
        ? actionAt<SpawnGovernmentWallBreachImpactAction>(
              *breachCommand, 2U)
        : nullptr;
    const auto *breachCamera = breachCommand
        ? actionAt<ApplyMapCoreRadialCameraShakeAction>(
              *breachCommand, 3U)
        : nullptr;
    const auto *breachAudio = breachCommand
        ? actionAt<RequestMapCoreAudioAction>(*breachCommand, 4U)
        : nullptr;
    expect(breachCommand &&
               actionTypesAre<AppendMapCoreEventsAction,
                              SpawnMapCoreBrickImpactAction,
                              SpawnGovernmentWallBreachImpactAction,
                              ApplyMapCoreRadialCameraShakeAction,
                              RequestMapCoreAudioAction,
                              CommitMapCoreShellImpactAction>(
                   *breachCommand) &&
               breachBrick != nullptr && breachBrick->power &&
               breachBrick->destroyed && breachFx != nullptr &&
               sameFloat3(breachFx->position,
                          {7.25f, 0.48f, 8.75f}) &&
               sameFloat3(breachFx->normal,
                          {-0.6f, 0.42f, -0.8f}) &&
               breachFx->heavy && breachCamera != nullptr &&
               sameXZ(breachCamera->origin, breach.outcome.position) &&
               nearlyEqual(breachCamera->maximum, 0.22f) &&
               nearlyEqual(breachCamera->distanceFalloff, 0.012f) &&
               breachAudio != nullptr &&
               breachAudio->cue == AudioCue::BrickHit,
           "lethal wall command changed breach FX, camera, or audio");

    ShellPhysicalImpactResult protectedWall = mapResult(
        ImpactKind::Steel, ShellOwner::Enemy);
    protectedWall.outcome.impactDetails.governmentWallIndex = 3;
    protectedWall.outcome.impactDetails.governmentWallHealthBefore = 4;
    protectedWall.outcome.impactDetails.governmentWallHealthAfter = 4;
    const auto protectedCommand = makeShellMapCorePresentationCommand(
        protectedWall, {});
    const auto *protectedFx = protectedCommand
        ? actionAt<SpawnMapCoreSurfaceImpactAction>(
              *protectedCommand, 0U)
        : nullptr;
    expect(protectedCommand &&
               actionTypesAre<SpawnMapCoreSurfaceImpactAction,
                              CommitMapCoreShellImpactAction>(
                   *protectedCommand) &&
               protectedFx != nullptr && protectedFx->heavy &&
               sameFloat3(protectedFx->position,
                          {7.25f, 0.43f, 8.75f}) &&
               sameFloat3(protectedFx->normal, {0.0f, 1.0f, 0.0f}),
           "protected enemy wall command gained events/audio or wrong FX");

    ShellPhysicalImpactResult steel = mapResult(
        ImpactKind::Steel, ShellOwner::Player, true);
    steel.outcome.impactDetails.brickCount = 2;
    steel.outcome.impactDetails.bricks[0] = {4, 5, 0x0fU, 0x00U};
    steel.outcome.impactDetails.bricks[1] = {4, 6, 0x0fU, 0x00U};
    const std::vector<GameEvent> steelEvents{
        brickEvent(steel, steel.outcome.impactDetails.bricks[0]),
        brickEvent(steel, steel.outcome.impactDetails.bricks[1])};
    const auto steelCommand = makeShellMapCorePresentationCommand(
        steel, steelEvents);
    const auto *steelAppend = steelCommand
        ? actionAt<AppendMapCoreEventsAction>(*steelCommand, 0U)
        : nullptr;
    const auto *steelSurface = steelCommand
        ? actionAt<SpawnMapCoreSurfaceImpactAction>(*steelCommand, 1U)
        : nullptr;
    const auto *steelAudio = steelCommand
        ? actionAt<RequestMapCoreAudioAction>(*steelCommand, 2U)
        : nullptr;
    expect(steelCommand &&
               actionTypesAre<AppendMapCoreEventsAction,
                              SpawnMapCoreSurfaceImpactAction,
                              RequestMapCoreAudioAction,
                              CommitMapCoreShellImpactAction>(*steelCommand) &&
               steelAppend != nullptr &&
               steelAppend->events == steelEvents &&
               steelSurface != nullptr && steelSurface->heavy &&
               steelAudio != nullptr &&
               steelAudio->cue == AudioCue::SteelHit,
           "steel-plus-bricks command lost its event batch or steel cue");

    ShellPhysicalImpactResult boundary = mapResult(
        ImpactKind::Boundary, ShellOwner::Player, false);
    const auto boundaryCommand = makeShellMapCorePresentationCommand(
        boundary, {});
    const auto *boundaryFx = boundaryCommand
        ? actionAt<SpawnMapCoreSurfaceImpactAction>(
              *boundaryCommand, 0U)
        : nullptr;
    const auto *boundaryAudio = boundaryCommand
        ? actionAt<RequestMapCoreAudioAction>(*boundaryCommand, 1U)
        : nullptr;
    expect(boundaryCommand &&
               actionTypesAre<SpawnMapCoreSurfaceImpactAction,
                              RequestMapCoreAudioAction,
                              CommitMapCoreShellImpactAction>(
                   *boundaryCommand) &&
               boundaryFx != nullptr && !boundaryFx->heavy &&
               boundaryAudio != nullptr &&
               boundaryAudio->cue == AudioCue::BoundaryHit,
           "non-power boundary command changed heavy or audio semantics");

    const ShellPhysicalImpactResult liveCore = coreResult(true);
    const GameEvent destroyedCore = coreEvent(liveCore);
    const auto coreCommand = makeShellMapCorePresentationCommand(
        liveCore, std::vector<GameEvent>{destroyedCore});
    const auto *coreAppend = coreCommand
        ? actionAt<AppendMapCoreEventsAction>(*coreCommand, 0U)
        : nullptr;
    const auto *coreExplosion = coreCommand
        ? actionAt<SpawnGovernmentCoreExplosionAction>(*coreCommand, 1U)
        : nullptr;
    const auto *coreAudio = coreCommand
        ? actionAt<RequestMapCoreAudioAction>(*coreCommand, 2U)
        : nullptr;
    const auto *coreCommit = coreCommand
        ? actionAt<CommitMapCoreShellImpactAction>(*coreCommand, 3U)
        : nullptr;
    expect(coreCommand &&
               actionTypesAre<AppendMapCoreEventsAction,
                              SpawnGovernmentCoreExplosionAction,
                              RequestMapCoreAudioAction,
                              CommitMapCoreShellImpactAction>(*coreCommand) &&
               coreAppend != nullptr &&
               coreAppend->events == std::vector<GameEvent>{destroyedCore} &&
               coreExplosion != nullptr &&
               sameFloat3(coreExplosion->position,
                          {kGovernmentBaseCenter.x, 0.42f,
                           kGovernmentBaseCenter.z}) &&
               sameColor(coreExplosion->color,
                         {255U, 195U, 55U, 255U}) &&
               coreAudio != nullptr &&
               coreAudio->cue == AudioCue::EagleDestroyed &&
               coreCommit != nullptr &&
               sameXZ(coreCommit->position, liveCore.outcome.position) &&
               !sameXZ(coreCommit->position, kGovernmentBaseCenter),
           "live-core command lost fixed explosion or contact commit");

    const ShellPhysicalImpactResult deadCore = coreResult(false);
    const auto deadCoreCommand = makeShellMapCorePresentationCommand(
        deadCore, {});
    const auto *deadCoreCommit = deadCoreCommand
        ? actionAt<CommitMapCoreShellImpactAction>(*deadCoreCommand, 0U)
        : nullptr;
    expect(deadCoreCommand &&
               actionTypesAre<CommitMapCoreShellImpactAction>(
                   *deadCoreCommand) &&
               deadCoreCommit != nullptr &&
               sameXZ(deadCoreCommit->position,
                      deadCore.outcome.position),
           "dead-core command stopped being a silent shell commit");

    reporter.beginSuite("shell-map-core-presentation-boundaries");
    ShellPhysicalImpactResult unresolved = brick;
    unresolved.resolved = false;
    bool nonMapCoreTargetsRejected = true;
    for (CombatTarget target : {CombatTarget::None,
                                CombatTarget::EnemyTank,
                                CombatTarget::PlayerTank})
    {
        ShellPhysicalImpactResult result = brick;
        result.outcome.target = target;
        nonMapCoreTargetsRejected = nonMapCoreTargetsRejected &&
            !makeShellMapCorePresentationCommand(
                 result, std::vector<GameEvent>{brickHit})
                 .has_value();
    }
    expect(!makeShellMapCorePresentationCommand(
                unresolved, std::vector<GameEvent>{brickHit})
                .has_value() &&
               nonMapCoreTargetsRejected,
           "unresolved or non-map/core result produced a command");

    ShellPhysicalImpactResult invalidOwner = brick;
    invalidOwner.outcome.owner = static_cast<ShellOwner>(99);
    ShellPhysicalImpactResult invalidPosition = brick;
    invalidPosition.outcome.position.x =
        std::numeric_limits<float>::infinity();
    ShellPhysicalImpactResult invalidVelocity = brick;
    invalidVelocity.outcome.incomingVelocity.z =
        std::numeric_limits<float>::quiet_NaN();
    ShellPhysicalImpactResult invalidOwnerIndex = brick;
    invalidOwnerIndex.outcome.ownerIndex = -1;
    const GameEvent invalidOwnerEvent = brickEvent(
        invalidOwner, invalidOwner.outcome.impactDetails.bricks[0]);
    const GameEvent invalidPositionEvent = brickEvent(
        invalidPosition, invalidPosition.outcome.impactDetails.bricks[0]);
    const GameEvent invalidOwnerIndexEvent = brickEvent(
        invalidOwnerIndex,
        invalidOwnerIndex.outcome.impactDetails.bricks[0]);
    ShellPhysicalImpactResult mapWithEnemyCommit = brick;
    mapWithEnemyCommit.enemyCommit.applied = true;
    ShellPhysicalImpactResult coreWithPlayerCommit = liveCore;
    coreWithPlayerCommit.playerCommit.applied = true;
    expect(!makeShellMapCorePresentationCommand(
                invalidOwner, std::vector<GameEvent>{invalidOwnerEvent})
                .has_value() &&
               !makeShellMapCorePresentationCommand(
                    invalidPosition,
                    std::vector<GameEvent>{invalidPositionEvent})
                    .has_value() &&
               !makeShellMapCorePresentationCommand(
                    invalidVelocity, std::vector<GameEvent>{brickHit})
                    .has_value() &&
               !makeShellMapCorePresentationCommand(
                    invalidOwnerIndex,
                    std::vector<GameEvent>{invalidOwnerIndexEvent})
                    .has_value() &&
               !makeShellMapCorePresentationCommand(
                    mapWithEnemyCommit,
                    std::vector<GameEvent>{brickHit})
                    .has_value() &&
               !makeShellMapCorePresentationCommand(
                    coreWithPlayerCommit,
                    std::vector<GameEvent>{destroyedCore})
                    .has_value(),
           "invalid owner/coordinate or contradictory commit produced a "
           "command");

    ShellPhysicalImpactResult noImpact = brick;
    noImpact.outcome.impactKind = ImpactKind::None;
    ShellPhysicalImpactResult tooManyBricks = brick;
    tooManyBricks.outcome.impactDetails.brickCount = 3;
    ShellPhysicalImpactResult reversedBricks = steel;
    reversedBricks.outcome.impactDetails.bricks[0] =
        {4, 6, 0x0fU, 0x00U};
    reversedBricks.outcome.impactDetails.bricks[1] =
        {4, 5, 0x0fU, 0x00U};
    const std::vector<GameEvent> reversedBrickEvents{
        brickEvent(reversedBricks,
                   reversedBricks.outcome.impactDetails.bricks[0]),
        brickEvent(reversedBricks,
                   reversedBricks.outcome.impactDetails.bricks[1])};
    ShellPhysicalImpactResult negativeBrickCount = brick;
    negativeBrickCount.outcome.impactDetails.brickCount = -1;
    ShellPhysicalImpactResult additiveBrickMask = brick;
    additiveBrickMask.outcome.impactDetails.bricks[0] =
        {5, 6, 0x01U, 0x08U};
    const GameEvent additiveBrickEvent = brickEvent(
        additiveBrickMask,
        additiveBrickMask.outcome.impactDetails.bricks[0]);
    ShellPhysicalImpactResult partialPowerBrick = brick;
    partialPowerBrick.outcome.power = true;
    partialPowerBrick.outcome.impactDetails.bricks[0] =
        {5, 6, 0x0fU, 0x03U};
    const GameEvent partialPowerBrickEvent = brickEvent(
        partialPowerBrick,
        partialPowerBrick.outcome.impactDetails.bricks[0]);
    ShellPhysicalImpactResult mapWithCoreHealth = brick;
    mapWithCoreHealth.outcome.governmentCoreHealthBefore = 1;
    ShellPhysicalImpactResult mapWithCoreHealthAfter = brick;
    mapWithCoreHealthAfter.outcome.governmentCoreHealthAfter = 1;
    bool invalidBrickFieldsRejected = true;
    for (const BrickDamage &damage : {
             BrickDamage{-1, 6, 0x0fU, 0x00U},
             BrickDamage{tanks3d::game::kMapSize, 6, 0x0fU, 0x00U},
             BrickDamage{5, -1, 0x0fU, 0x00U},
             BrickDamage{5, tanks3d::game::kMapSize, 0x0fU, 0x00U},
             BrickDamage{5, 6, 0x00U, 0x00U},
             BrickDamage{5, 6, 0x10U, 0x00U},
             BrickDamage{5, 6, 0x0fU, 0x10U},
             BrickDamage{5, 6, 0x0fU, 0x0fU}})
    {
        ShellPhysicalImpactResult malformedBrick = brick;
        malformedBrick.outcome.impactDetails.bricks[0] = damage;
        invalidBrickFieldsRejected = invalidBrickFieldsRejected &&
            !makeShellMapCorePresentationCommand(
                 malformedBrick,
                 std::vector<GameEvent>{brickEvent(malformedBrick, damage)})
                 .has_value();
    }
    ShellPhysicalImpactResult brickWithoutDamage = brick;
    brickWithoutDamage.outcome.impactDetails.brickCount = 0;
    ShellPhysicalImpactResult brickWithWall = brick;
    brickWithWall.outcome.impactDetails.governmentWallIndex = 0;
    brickWithWall.outcome.impactDetails.governmentWallHealthBefore = 4;
    brickWithWall.outcome.impactDetails.governmentWallHealthAfter = 3;
    const std::vector<GameEvent> brickWithWallEvents{
        brickEvent(brickWithWall,
                   brickWithWall.outcome.impactDetails.bricks[0]),
        wallEvent(brickWithWall)};
    GameEvent wrongBrickEvent = brickHit;
    wrongBrickEvent.row = 9;
    expect(!makeShellMapCorePresentationCommand(
                noImpact, std::vector<GameEvent>{brickHit})
                .has_value() &&
               !makeShellMapCorePresentationCommand(
                    tooManyBricks, std::vector<GameEvent>{brickHit})
                    .has_value() &&
               !makeShellMapCorePresentationCommand(
                    reversedBricks, reversedBrickEvents)
                    .has_value() &&
               !makeShellMapCorePresentationCommand(
                    negativeBrickCount, {})
                    .has_value() &&
               !makeShellMapCorePresentationCommand(
                    additiveBrickMask,
                    std::vector<GameEvent>{additiveBrickEvent})
                    .has_value() &&
               !makeShellMapCorePresentationCommand(
                    partialPowerBrick,
                    std::vector<GameEvent>{partialPowerBrickEvent})
                    .has_value() &&
               !makeShellMapCorePresentationCommand(
                    mapWithCoreHealth,
                    std::vector<GameEvent>{brickHit})
                    .has_value() &&
               !makeShellMapCorePresentationCommand(
                    mapWithCoreHealthAfter,
                    std::vector<GameEvent>{brickHit})
                    .has_value() &&
               invalidBrickFieldsRejected &&
               !makeShellMapCorePresentationCommand(
                    brickWithoutDamage, {})
                    .has_value() &&
               !makeShellMapCorePresentationCommand(
                    brickWithWall, brickWithWallEvents)
                    .has_value() &&
               !makeShellMapCorePresentationCommand(
                    brick, std::vector<GameEvent>{wrongBrickEvent})
                    .has_value(),
           "malformed map details or event batch produced a command");

    ShellPhysicalImpactResult invalidWall = wall;
    invalidWall.outcome.impactDetails.governmentWallIndex = 5;
    ShellPhysicalImpactResult healingWall = wall;
    healingWall.outcome.impactDetails.governmentWallHealthAfter = 5;
    ShellPhysicalImpactResult mixedWall = wall;
    mixedWall.outcome.impactDetails.brickCount = 1;
    mixedWall.outcome.impactDetails.bricks[0] = {1, 1, 0x0fU, 0x00U};
    const std::vector<GameEvent> mixedWallEvents{
        brickEvent(mixedWall,
                   mixedWall.outcome.impactDetails.bricks[0]),
        wallEvent(mixedWall)};
    ShellPhysicalImpactResult normalWallOverdamage = wall;
    normalWallOverdamage.outcome.impactDetails.governmentWallHealthAfter = 0;
    ShellPhysicalImpactResult powerWallUnderdamage = breach;
    powerWallUnderdamage.outcome.impactDetails.governmentWallHealthBefore = 4;
    powerWallUnderdamage.outcome.impactDetails.governmentWallHealthAfter = 1;
    ShellPhysicalImpactResult oversizedWall = wall;
    oversizedWall.outcome.impactDetails.governmentWallHealthBefore = 999;
    oversizedWall.outcome.impactDetails.governmentWallHealthAfter = 998;
    ShellPhysicalImpactResult minimumWall = wall;
    minimumWall.outcome.impactDetails.governmentWallHealthBefore =
        std::numeric_limits<int>::min();
    minimumWall.outcome.impactDetails.governmentWallHealthAfter = 0;
    ShellPhysicalImpactResult oversizedProtectedWall = protectedWall;
    oversizedProtectedWall.outcome.impactDetails.governmentWallHealthBefore =
        999;
    oversizedProtectedWall.outcome.impactDetails.governmentWallHealthAfter =
        999;
    ShellPhysicalImpactResult invalidProtectedIndex = protectedWall;
    invalidProtectedIndex.outcome.impactDetails.governmentWallIndex =
        tanks3d::game::kGovernmentWallCount;
    ShellPhysicalImpactResult protectedWallWithBrick = protectedWall;
    protectedWallWithBrick.outcome.impactDetails.brickCount = 1;
    protectedWallWithBrick.outcome.impactDetails.bricks[0] =
        {1, 1, 0x0fU, 0x00U};
    ShellPhysicalImpactResult protectedZeroHealth = protectedWall;
    protectedZeroHealth.outcome.impactDetails.governmentWallHealthBefore = 0;
    protectedZeroHealth.outcome.impactDetails.governmentWallHealthAfter = 0;
    ShellPhysicalImpactResult changedProtectedHealth = protectedWall;
    changedProtectedHealth.outcome.impactDetails.governmentWallHealthAfter = 3;
    ShellPhysicalImpactResult residualSteelHealth = mapResult(
        ImpactKind::Steel, ShellOwner::Enemy);
    residualSteelHealth.outcome.impactDetails.governmentWallHealthBefore = 4;
    residualSteelHealth.outcome.impactDetails.governmentWallHealthAfter = 4;
    ShellPhysicalImpactResult boundaryWithBrick = boundary;
    boundaryWithBrick.outcome.impactDetails.brickCount = 1;
    boundaryWithBrick.outcome.impactDetails.bricks[0] =
        {1, 1, 0x0fU, 0x00U};
    ShellPhysicalImpactResult boundaryWithWall = boundary;
    boundaryWithWall.outcome.impactDetails.governmentWallIndex = 0;
    boundaryWithWall.outcome.impactDetails.governmentWallHealthBefore = 4;
    boundaryWithWall.outcome.impactDetails.governmentWallHealthAfter = 3;
    expect(!makeShellMapCorePresentationCommand(
                invalidWall,
                std::vector<GameEvent>{wallEvent(invalidWall)})
                .has_value() &&
               !makeShellMapCorePresentationCommand(
                    healingWall,
                    std::vector<GameEvent>{wallEvent(healingWall)})
                    .has_value() &&
               !makeShellMapCorePresentationCommand(
                    mixedWall, mixedWallEvents)
                    .has_value() &&
               !makeShellMapCorePresentationCommand(
                    normalWallOverdamage,
                    std::vector<GameEvent>{wallEvent(normalWallOverdamage)})
                    .has_value() &&
               !makeShellMapCorePresentationCommand(
                    powerWallUnderdamage,
                    std::vector<GameEvent>{wallEvent(powerWallUnderdamage)})
                    .has_value() &&
               !makeShellMapCorePresentationCommand(
                    oversizedWall,
                    std::vector<GameEvent>{wallEvent(oversizedWall)})
                    .has_value() &&
               !makeShellMapCorePresentationCommand(
                    minimumWall,
                    std::vector<GameEvent>{wallEvent(minimumWall)})
                    .has_value() &&
               !makeShellMapCorePresentationCommand(
                    oversizedProtectedWall, {})
                    .has_value() &&
               !makeShellMapCorePresentationCommand(
                    invalidProtectedIndex, {})
                    .has_value() &&
               !makeShellMapCorePresentationCommand(
                    protectedWallWithBrick,
                    std::vector<GameEvent>{brickEvent(
                        protectedWallWithBrick,
                        protectedWallWithBrick.outcome.impactDetails
                            .bricks[0])})
                    .has_value() &&
               !makeShellMapCorePresentationCommand(
                    protectedZeroHealth, {})
                    .has_value() &&
               !makeShellMapCorePresentationCommand(
                    changedProtectedHealth,
                    std::vector<GameEvent>{wallEvent(
                        changedProtectedHealth)})
                    .has_value() &&
               !makeShellMapCorePresentationCommand(
                    residualSteelHealth, {})
                    .has_value() &&
               !makeShellMapCorePresentationCommand(
                    boundaryWithBrick,
                    std::vector<GameEvent>{brickEvent(
                        boundaryWithBrick,
                        boundaryWithBrick.outcome.impactDetails.bricks[0])})
                    .has_value() &&
               !makeShellMapCorePresentationCommand(
                    boundaryWithWall,
                    std::vector<GameEvent>{wallEvent(boundaryWithWall)})
                    .has_value(),
           "invalid government-wall snapshot produced a command");

    ShellPhysicalImpactResult coreWithMapImpact = liveCore;
    coreWithMapImpact.outcome.impactKind = ImpactKind::Brick;
    ShellPhysicalImpactResult healingCore = liveCore;
    healingCore.outcome.governmentCoreHealthBefore = 0;
    healingCore.outcome.governmentCoreHealthAfter = 1;
    const GameEvent healingCoreEvent = coreEvent(healingCore);
    ShellPhysicalImpactResult coreWithBrick = liveCore;
    coreWithBrick.outcome.impactDetails.brickCount = 1;
    coreWithBrick.outcome.impactDetails.bricks[0] =
        {1, 1, 0x0fU, 0x00U};
    ShellPhysicalImpactResult coreWithWall = liveCore;
    coreWithWall.outcome.impactDetails.governmentWallIndex = 0;
    coreWithWall.outcome.impactDetails.governmentWallHealthBefore = 4;
    coreWithWall.outcome.impactDetails.governmentWallHealthAfter = 4;
    GameEvent wrongCoreEvent = destroyedCore;
    wrongCoreEvent.basePart = GovernmentBasePart::Wall;
    expect(!makeShellMapCorePresentationCommand(
                coreWithMapImpact,
                std::vector<GameEvent>{destroyedCore})
                .has_value() &&
               !makeShellMapCorePresentationCommand(
                    healingCore, std::vector<GameEvent>{healingCoreEvent})
                    .has_value() &&
               !makeShellMapCorePresentationCommand(
                    coreWithBrick,
                    std::vector<GameEvent>{coreEvent(coreWithBrick)})
                    .has_value() &&
               !makeShellMapCorePresentationCommand(
                    coreWithWall,
                    std::vector<GameEvent>{coreEvent(coreWithWall)})
                    .has_value() &&
               !makeShellMapCorePresentationCommand(
                    liveCore, std::vector<GameEvent>{wrongCoreEvent})
                    .has_value() &&
               !makeShellMapCorePresentationCommand(
                    deadCore, std::vector<GameEvent>{destroyedCore})
                    .has_value(),
           "malformed core transition or event batch produced a command");

    ShellPhysicalImpactResult stationaryBrick = brick;
    stationaryBrick.outcome.incomingVelocity = {};
    const GameEvent stationaryEvent = brickEvent(
        stationaryBrick,
        stationaryBrick.outcome.impactDetails.bricks[0]);
    const auto stationaryCommand = makeShellMapCorePresentationCommand(
        stationaryBrick, std::vector<GameEvent>{stationaryEvent});
    const auto *stationaryFx = stationaryCommand
        ? actionAt<SpawnMapCoreBrickImpactAction>(
              *stationaryCommand, 1U)
        : nullptr;
    expect(stationaryFx != nullptr &&
               sameFloat3(stationaryFx->normal,
                          {0.0f, 0.28f, 0.0f}),
           "stationary impact produced a non-finite normal");

    reporter.beginSuite("shell-map-core-presentation-step-mapping");
    expect(shellMapCorePresentationStep(AppendMapCoreEventsAction{}) ==
               ShellMapCorePresentationStep::PhysicalEventsAppended,
           "AppendMapCoreEventsAction mapped to the wrong step");
    expect(shellMapCorePresentationStep(SpawnMapCoreBrickImpactAction{}) ==
               ShellMapCorePresentationStep::BrickImpactFxSpawned,
           "SpawnMapCoreBrickImpactAction mapped to the wrong step");
    expect(shellMapCorePresentationStep(
               SpawnGovernmentWallBreachImpactAction{}) ==
               ShellMapCorePresentationStep::WallBreachImpactFxSpawned,
           "SpawnGovernmentWallBreachImpactAction mapped to the wrong step");
    expect(shellMapCorePresentationStep(
               SpawnMapCoreSurfaceImpactAction{}) ==
               ShellMapCorePresentationStep::SurfaceImpactFxSpawned,
           "SpawnMapCoreSurfaceImpactAction mapped to the wrong step");
    expect(shellMapCorePresentationStep(
               ApplyMapCoreRadialCameraShakeAction{}) ==
               ShellMapCorePresentationStep::CameraShakePassCompleted,
           "ApplyMapCoreRadialCameraShakeAction mapped to the wrong step");
    expect(shellMapCorePresentationStep(
               SpawnGovernmentCoreExplosionAction{}) ==
               ShellMapCorePresentationStep::CoreExplosionFxSpawned,
           "SpawnGovernmentCoreExplosionAction mapped to the wrong step");
    expect(shellMapCorePresentationStep(RequestMapCoreAudioAction{}) ==
               ShellMapCorePresentationStep::AudioRequested,
           "RequestMapCoreAudioAction mapped to the wrong step");
    expect(shellMapCorePresentationStep(
               CommitMapCoreShellImpactAction{}) ==
               ShellMapCorePresentationStep::ShellImpactCommitted,
           "CommitMapCoreShellImpactAction mapped to the wrong step");

    reporter.finish();
    return passed ? 0 : 1;
}
