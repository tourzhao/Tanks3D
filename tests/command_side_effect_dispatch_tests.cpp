#include "app/command_side_effect_dispatch.h"
#include "test_support.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <string>
#include <vector>

namespace
{
using tanks3d::app::AppendMapCoreEventsAction;
using tanks3d::app::AppendShellCancellationEventAction;
using tanks3d::app::AppendTankEventsAction;
using tanks3d::app::ApplyMapCoreRadialCameraShakeAction;
using tanks3d::app::ApplyTankRadialCameraShakeAction;
using tanks3d::app::AssignTankTargetCameraShakeAction;
using tanks3d::app::CommandSideEffectDisposition;
using tanks3d::app::CommandSideEffectSink;
using tanks3d::app::CommitMapCoreShellImpactAction;
using tanks3d::app::CommitTankShellImpactAction;
using tanks3d::app::Float3;
using tanks3d::app::RequestMapCoreAudioAction;
using tanks3d::app::RequestShellCancellationAudioAction;
using tanks3d::app::RequestTankAudioAction;
using tanks3d::app::Rgba8;
using tanks3d::app::ShellMapCorePresentationAction;
using tanks3d::app::ShellTankPresentationAction;
using tanks3d::app::SpawnGovernmentCoreExplosionAction;
using tanks3d::app::SpawnGovernmentWallBreachImpactAction;
using tanks3d::app::SpawnMapCoreBrickImpactAction;
using tanks3d::app::SpawnMapCoreSurfaceImpactAction;
using tanks3d::app::SpawnShellCancellationImpactAction;
using tanks3d::app::SpawnTankArmorImpactAction;
using tanks3d::app::SpawnTankExplosionAction;
using tanks3d::app::dispatchCommandSideEffect;
using tanks3d::audio::AudioCue;
using tanks3d::core::CardinalDirection;
using tanks3d::core::XZ;
using tanks3d::game::BonusEffectCommand;
using tanks3d::game::BonusEffectCommandType;
using tanks3d::game::BonusType;
using tanks3d::game::GameEvent;
using tanks3d::game::GameEventCause;
using tanks3d::game::GameEventType;
using tanks3d::game::GovernmentBasePart;
using tanks3d::game::ImpactKind;

bool nearlyEqual(float first, float second, float tolerance = 0.00001f)
{
    return std::fabs(first - second) <= tolerance;
}

bool sameFloat3(Float3 first, Float3 second)
{
    return nearlyEqual(first.x, second.x) &&
           nearlyEqual(first.y, second.y) &&
           nearlyEqual(first.z, second.z);
}

bool samePosition(XZ first, XZ second)
{
    return nearlyEqual(first.x, second.x) &&
           nearlyEqual(first.z, second.z);
}

bool sameColor(Rgba8 first, Rgba8 second)
{
    return first.r == second.r && first.g == second.g &&
           first.b == second.b && first.a == second.a;
}

enum class CallKind
{
    Event,
    Impact,
    BrickImpact,
    Explosion,
    RadialCameraShake,
    PlayerCameraShake,
    Audio
};

struct RecordedCall
{
    CallKind kind = CallKind::Event;
    GameEvent event{};
    Float3 position{};
    Float3 normal{};
    Rgba8 color{};
    XZ origin{};
    float maximum = 0.0f;
    float distanceFalloff = 0.0f;
    float value = 0.0f;
    std::size_t playerIndex = 0U;
    AudioCue cue = AudioCue::Count;
    bool heavy = false;
    bool power = false;
    bool destroyed = false;
};

class RecordingSink final : public CommandSideEffectSink
{
public:
    void emitEvent(const GameEvent &event) override
    {
        RecordedCall call;
        call.kind = CallKind::Event;
        call.event = event;
        calls.push_back(call);
    }

    void spawnImpact(Float3 position, Float3 normal, bool heavy) override
    {
        RecordedCall call;
        call.kind = CallKind::Impact;
        call.position = position;
        call.normal = normal;
        call.heavy = heavy;
        calls.push_back(call);
    }

    void spawnBrickImpact(Float3 position, Float3 normal, bool power,
                          bool destroyed) override
    {
        RecordedCall call;
        call.kind = CallKind::BrickImpact;
        call.position = position;
        call.normal = normal;
        call.power = power;
        call.destroyed = destroyed;
        calls.push_back(call);
    }

    void spawnExplosion(Float3 position, Rgba8 color) override
    {
        RecordedCall call;
        call.kind = CallKind::Explosion;
        call.position = position;
        call.color = color;
        calls.push_back(call);
    }

    void applyRadialCameraShake(XZ origin, float maximum,
                                float distanceFalloff) override
    {
        RecordedCall call;
        call.kind = CallKind::RadialCameraShake;
        call.origin = origin;
        call.maximum = maximum;
        call.distanceFalloff = distanceFalloff;
        calls.push_back(call);
    }

    bool assignPlayerCameraShake(std::size_t playerIndex,
                                 float value) override
    {
        RecordedCall call;
        call.kind = CallKind::PlayerCameraShake;
        call.playerIndex = playerIndex;
        call.value = value;
        calls.push_back(call);
        return playerIndex < acceptedPlayerCount;
    }

    void requestAudio(AudioCue cue) override
    {
        RecordedCall call;
        call.kind = CallKind::Audio;
        call.cue = cue;
        calls.push_back(call);
    }

    void reset()
    {
        calls.clear();
    }

    std::size_t acceptedPlayerCount = 2U;
    std::vector<RecordedCall> calls{};
};

GameEvent testEvent(int discriminator)
{
    GameEvent event;
    event.type = discriminator == 1 ? GameEventType::ShellCancelled
                                    : GameEventType::TankDestroyed;
    event.cause = discriminator == 1 ? GameEventCause::EnemyShell
                                     : GameEventCause::PlayerShell;
    event.position = {1.25f * discriminator, -2.5f * discriminator};
    event.direction = discriminator == 1 ? CardinalDirection::West
                                         : CardinalDirection::East;
    event.sourcePlayerId = 10 + discriminator;
    event.sourceEnemyId = 20 + discriminator;
    event.targetPlayerId = 30 + discriminator;
    event.targetEnemyId = 40 + discriminator;
    event.enemyType = discriminator;
    event.bonusType = BonusType::Star;
    event.impactKind = ImpactKind::Brick;
    event.basePart = GovernmentBasePart::Wall;
    event.stage = 5 + discriminator;
    event.row = 7 + discriminator;
    event.column = 9 + discriminator;
    event.baseSegmentIndex = discriminator;
    event.valueBefore = 4 + discriminator;
    event.valueAfter = 3 + discriminator;
    event.points = 50 * discriminator;
    event.power = discriminator == 2;
    return event;
}

bool kindsAre(const RecordingSink &sink,
              const std::vector<CallKind> &expected)
{
    if (sink.calls.size() != expected.size())
        return false;
    for (std::size_t index = 0; index < expected.size(); ++index)
    {
        if (sink.calls[index].kind != expected[index])
            return false;
    }
    return true;
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
    RecordingSink sink;

    reporter.beginSuite("dispatch-cancellation-event-leaf");
    const GameEvent cancellationEvent = testEvent(1);
    AppendShellCancellationEventAction appendCancellation;
    appendCancellation.event = cancellationEvent;
    const CommandSideEffectDisposition cancellationEventDisposition =
        dispatchCommandSideEffect(appendCancellation, sink);
    expect(cancellationEventDisposition ==
               CommandSideEffectDisposition::Applied,
           "cancellation event was not applied");
    expect(sink.calls.size() == 1U &&
               sink.calls[0].kind == CallKind::Event,
           "cancellation event emitted the wrong sink call count or kind");
    expect(sink.calls[0].event == cancellationEvent,
           "cancellation event payload changed during dispatch");

    reporter.beginSuite("dispatch-cancellation-impact-leaf");
    sink.reset();
    SpawnShellCancellationImpactAction cancellationImpact;
    cancellationImpact.position = {4.25f, -8.5f};
    cancellationImpact.elevation = 0.37f;
    cancellationImpact.normalXZ = {-0.75f, 0.50f};
    cancellationImpact.normalY = 0.125f;
    cancellationImpact.heavy = true;
    const CommandSideEffectDisposition cancellationImpactDisposition =
        dispatchCommandSideEffect(cancellationImpact, sink);
    expect(cancellationImpactDisposition ==
               CommandSideEffectDisposition::Applied,
           "cancellation impact was not applied");
    expect(sink.calls.size() == 1U &&
               sink.calls[0].kind == CallKind::Impact,
           "cancellation impact emitted the wrong sink call");
    expect(sameFloat3(sink.calls[0].position,
                      {4.25f, 0.37f, -8.5f}) &&
               sameFloat3(sink.calls[0].normal,
                          {-0.75f, 0.125f, 0.50f}) &&
               sink.calls[0].heavy,
           "cancellation XZ/elevation or normal payload changed");

    reporter.beginSuite("dispatch-cancellation-audio-validation");
    sink.reset();
    RequestShellCancellationAudioAction validCancellationAudio;
    validCancellationAudio.cue = AudioCue::ScoreCounted;
    expect(dispatchCommandSideEffect(validCancellationAudio, sink) ==
                   CommandSideEffectDisposition::Applied &&
               sink.calls.size() == 1U &&
               sink.calls[0].kind == CallKind::Audio &&
               sink.calls[0].cue == AudioCue::ScoreCounted,
           "last valid cancellation audio cue was not forwarded");
    sink.reset();
    RequestShellCancellationAudioAction sentinelCancellationAudio;
    sentinelCancellationAudio.cue = AudioCue::Count;
    expect(dispatchCommandSideEffect(sentinelCancellationAudio, sink) ==
                   CommandSideEffectDisposition::Rejected &&
               sink.calls.empty(),
           "cancellation audio sentinel reached the sink");
    RequestShellCancellationAudioAction outOfRangeCancellationAudio;
    outOfRangeCancellationAudio.cue = static_cast<AudioCue>(
        std::numeric_limits<std::size_t>::max());
    expect(dispatchCommandSideEffect(outOfRangeCancellationAudio, sink) ==
                   CommandSideEffectDisposition::Rejected &&
               sink.calls.empty(),
           "out-of-range cancellation audio reached the sink");

    reporter.beginSuite("dispatch-cancellation-three-leaf-order");
    sink.reset();
    RequestShellCancellationAudioAction orderedCancellationAudio;
    orderedCancellationAudio.cue = AudioCue::BulletHit;
    expect(dispatchCommandSideEffect(appendCancellation, sink) ==
                   CommandSideEffectDisposition::Applied,
           "ordered cancellation event failed");
    expect(dispatchCommandSideEffect(cancellationImpact, sink) ==
                   CommandSideEffectDisposition::Applied,
           "ordered cancellation impact failed");
    expect(dispatchCommandSideEffect(orderedCancellationAudio, sink) ==
                   CommandSideEffectDisposition::Applied,
           "ordered cancellation audio failed");
    expect(kindsAre(sink, {CallKind::Event, CallKind::Impact,
                           CallKind::Audio}),
           "cancellation leaf calls lost event-impact-audio order");
    expect(sink.calls[0].event == cancellationEvent &&
               sameFloat3(sink.calls[1].position,
                          {4.25f, 0.37f, -8.5f}) &&
               sink.calls[2].cue == AudioCue::BulletHit,
           "ordered cancellation calls lost their payloads");

    reporter.beginSuite("dispatch-map-core-event-and-impact-variants");
    sink.reset();
    const GameEvent firstMapEvent = testEvent(1);
    const GameEvent secondMapEvent = testEvent(2);
    AppendMapCoreEventsAction appendMap;
    appendMap.events = {firstMapEvent, secondMapEvent};
    ShellMapCorePresentationAction mapAction = appendMap;
    expect(dispatchCommandSideEffect(mapAction, sink) ==
                   CommandSideEffectDisposition::Applied &&
               kindsAre(sink, {CallKind::Event, CallKind::Event}) &&
               sink.calls[0].event == firstMapEvent &&
               sink.calls[1].event == secondMapEvent,
           "map/core event batch lost vector order or payload");
    sink.reset();
    AppendMapCoreEventsAction emptyMapEvents;
    mapAction = emptyMapEvents;
    expect(dispatchCommandSideEffect(mapAction, sink) ==
                   CommandSideEffectDisposition::Applied &&
               sink.calls.empty(),
           "empty map/core event batch emitted a sink call");

    SpawnMapCoreBrickImpactAction brick;
    brick.position = {1.0f, 2.0f, 3.0f};
    brick.normal = {-1.0f, -2.0f, -3.0f};
    brick.power = true;
    brick.destroyed = true;
    mapAction = brick;
    expect(dispatchCommandSideEffect(mapAction, sink) ==
                   CommandSideEffectDisposition::Applied &&
               sink.calls.size() == 1U &&
               sink.calls[0].kind == CallKind::BrickImpact,
           "map/core brick action selected the wrong sink leaf");
    expect(sameFloat3(sink.calls[0].position, brick.position) &&
               sameFloat3(sink.calls[0].normal, brick.normal) &&
               sink.calls[0].power && sink.calls[0].destroyed,
           "map/core brick payload changed");

    sink.reset();
    SpawnGovernmentWallBreachImpactAction breach;
    breach.position = {4.0f, 5.0f, 6.0f};
    breach.normal = {0.25f, 0.50f, 0.75f};
    breach.heavy = false;
    mapAction = breach;
    expect(dispatchCommandSideEffect(mapAction, sink) ==
                   CommandSideEffectDisposition::Applied &&
               sink.calls.size() == 1U &&
               sink.calls[0].kind == CallKind::Impact &&
               sameFloat3(sink.calls[0].position, breach.position) &&
               sameFloat3(sink.calls[0].normal, breach.normal) &&
               !sink.calls[0].heavy,
           "government-wall breach payload changed");

    sink.reset();
    SpawnMapCoreSurfaceImpactAction surface;
    surface.position = {-4.0f, -5.0f, -6.0f};
    surface.normal = {-0.25f, -0.50f, -0.75f};
    surface.heavy = true;
    mapAction = surface;
    expect(dispatchCommandSideEffect(mapAction, sink) ==
                   CommandSideEffectDisposition::Applied &&
               sink.calls.size() == 1U &&
               sink.calls[0].kind == CallKind::Impact &&
               sameFloat3(sink.calls[0].position, surface.position) &&
               sameFloat3(sink.calls[0].normal, surface.normal) &&
               sink.calls[0].heavy,
           "map/core surface payload changed");

    reporter.beginSuite("dispatch-map-core-camera-explosion-and-audio");
    sink.reset();
    ApplyMapCoreRadialCameraShakeAction mapRadial;
    mapRadial.origin = {7.25f, 8.50f};
    mapRadial.maximum = 0.45f;
    mapRadial.distanceFalloff = 0.08f;
    mapAction = mapRadial;
    expect(dispatchCommandSideEffect(mapAction, sink) ==
                   CommandSideEffectDisposition::Applied &&
               sink.calls.size() == 1U &&
               sink.calls[0].kind == CallKind::RadialCameraShake,
           "map/core radial camera action selected the wrong sink leaf");
    expect(samePosition(sink.calls[0].origin, mapRadial.origin) &&
               nearlyEqual(sink.calls[0].maximum, mapRadial.maximum) &&
               nearlyEqual(sink.calls[0].distanceFalloff,
                           mapRadial.distanceFalloff),
           "map/core radial camera payload changed");

    sink.reset();
    SpawnGovernmentCoreExplosionAction coreExplosion;
    coreExplosion.position = {9.0f, 10.0f, 11.0f};
    coreExplosion.color = {12U, 34U, 56U, 78U};
    mapAction = coreExplosion;
    expect(dispatchCommandSideEffect(mapAction, sink) ==
                   CommandSideEffectDisposition::Applied &&
               sink.calls.size() == 1U &&
               sink.calls[0].kind == CallKind::Explosion &&
               sameFloat3(sink.calls[0].position,
                          coreExplosion.position) &&
               sameColor(sink.calls[0].color, coreExplosion.color),
           "government-core explosion payload changed");

    sink.reset();
    RequestMapCoreAudioAction validMapAudio;
    validMapAudio.cue = AudioCue::EagleDestroyed;
    mapAction = validMapAudio;
    expect(dispatchCommandSideEffect(mapAction, sink) ==
                   CommandSideEffectDisposition::Applied &&
               sink.calls.size() == 1U &&
               sink.calls[0].kind == CallKind::Audio &&
               sink.calls[0].cue == AudioCue::EagleDestroyed,
           "valid map/core audio was not forwarded");
    sink.reset();
    RequestMapCoreAudioAction invalidMapAudio;
    invalidMapAudio.cue = AudioCue::Count;
    mapAction = invalidMapAudio;
    expect(dispatchCommandSideEffect(mapAction, sink) ==
                   CommandSideEffectDisposition::Rejected &&
               sink.calls.empty(),
           "invalid map/core audio reached the sink");
    invalidMapAudio.cue = static_cast<AudioCue>(
        std::numeric_limits<std::size_t>::max());
    mapAction = invalidMapAudio;
    expect(dispatchCommandSideEffect(mapAction, sink) ==
                   CommandSideEffectDisposition::Rejected &&
               sink.calls.empty(),
           "out-of-range map/core audio reached the sink");

    reporter.beginSuite("dispatch-map-core-all-variant-order");
    sink.reset();
    RequestMapCoreAudioAction orderedMapAudio;
    orderedMapAudio.cue = AudioCue::SteelHit;
    CommitMapCoreShellImpactAction mapCommit;
    mapCommit.position = {99.0f, 98.0f};
    const std::array<ShellMapCorePresentationAction, 8> orderedMapActions{{
        appendMap, brick, breach, surface, mapRadial, coreExplosion,
        orderedMapAudio, mapCommit}};
    for (std::size_t actionIndex = 0;
         actionIndex < orderedMapActions.size(); ++actionIndex)
    {
        const CommandSideEffectDisposition disposition =
            dispatchCommandSideEffect(orderedMapActions[actionIndex], sink);
        const CommandSideEffectDisposition expectedDisposition =
            actionIndex + 1U == orderedMapActions.size()
                ? CommandSideEffectDisposition::DomainCommitRequired
                : CommandSideEffectDisposition::Applied;
        expect(disposition == expectedDisposition,
               "map/core variant returned wrong disposition at index " +
                   std::to_string(actionIndex));
    }
    expect(kindsAre(sink,
                    {CallKind::Event, CallKind::Event,
                     CallKind::BrickImpact, CallKind::Impact,
                     CallKind::Impact, CallKind::RadialCameraShake,
                     CallKind::Explosion, CallKind::Audio}),
           "map/core all-variant sink order changed");
    expect(sink.calls.back().cue == AudioCue::SteelHit &&
               sink.calls[0].event == firstMapEvent &&
               sink.calls[1].event == secondMapEvent,
           "map/core ordered sequence lost boundary payloads");

    reporter.beginSuite("dispatch-tank-event-and-fx-variants");
    sink.reset();
    const GameEvent firstTankEvent = testEvent(2);
    const GameEvent secondTankEvent = testEvent(1);
    AppendTankEventsAction appendTank;
    appendTank.events = {firstTankEvent, secondTankEvent};
    ShellTankPresentationAction tankAction = appendTank;
    expect(dispatchCommandSideEffect(tankAction, sink) ==
                   CommandSideEffectDisposition::Applied &&
               kindsAre(sink, {CallKind::Event, CallKind::Event}) &&
               sink.calls[0].event == firstTankEvent &&
               sink.calls[1].event == secondTankEvent,
           "tank event batch lost vector order or payload");
    sink.reset();
    AppendTankEventsAction emptyTankEvents;
    tankAction = emptyTankEvents;
    expect(dispatchCommandSideEffect(tankAction, sink) ==
                   CommandSideEffectDisposition::Applied &&
               sink.calls.empty(),
           "empty tank event batch emitted a sink call");

    SpawnTankArmorImpactAction tankArmor;
    tankArmor.position = {1.5f, 2.5f, 3.5f};
    tankArmor.normal = {-1.5f, -2.5f, -3.5f};
    tankArmor.heavy = false;
    tankAction = tankArmor;
    expect(dispatchCommandSideEffect(tankAction, sink) ==
                   CommandSideEffectDisposition::Applied &&
               sink.calls.size() == 1U &&
               sink.calls[0].kind == CallKind::Impact &&
               sameFloat3(sink.calls[0].position, tankArmor.position) &&
               sameFloat3(sink.calls[0].normal, tankArmor.normal) &&
               !sink.calls[0].heavy,
           "tank armor-impact payload changed");

    sink.reset();
    SpawnTankExplosionAction tankExplosion;
    tankExplosion.position = {6.5f, 7.5f, 8.5f};
    tankExplosion.color = {210U, 180U, 150U, 120U};
    tankAction = tankExplosion;
    expect(dispatchCommandSideEffect(tankAction, sink) ==
                   CommandSideEffectDisposition::Applied &&
               sink.calls.size() == 1U &&
               sink.calls[0].kind == CallKind::Explosion &&
               sameFloat3(sink.calls[0].position,
                          tankExplosion.position) &&
               sameColor(sink.calls[0].color, tankExplosion.color),
           "tank explosion payload changed");

    sink.reset();
    ApplyTankRadialCameraShakeAction tankRadial;
    tankRadial.origin = {-3.25f, 4.75f};
    tankRadial.maximum = 0.62f;
    tankRadial.distanceFalloff = 0.17f;
    tankAction = tankRadial;
    expect(dispatchCommandSideEffect(tankAction, sink) ==
                   CommandSideEffectDisposition::Applied &&
               sink.calls.size() == 1U &&
               sink.calls[0].kind == CallKind::RadialCameraShake &&
               samePosition(sink.calls[0].origin, tankRadial.origin) &&
               nearlyEqual(sink.calls[0].maximum, tankRadial.maximum) &&
               nearlyEqual(sink.calls[0].distanceFalloff,
                           tankRadial.distanceFalloff),
           "tank radial-camera payload changed");

    reporter.beginSuite("dispatch-tank-target-camera-validation");
    sink.reset();
    sink.acceptedPlayerCount = 2U;
    AssignTankTargetCameraShakeAction validTankCamera;
    validTankCamera.playerIndex = 1U;
    validTankCamera.value = 0.73f;
    tankAction = validTankCamera;
    expect(dispatchCommandSideEffect(tankAction, sink) ==
                   CommandSideEffectDisposition::Applied &&
               sink.calls.size() == 1U &&
               sink.calls[0].kind == CallKind::PlayerCameraShake &&
               sink.calls[0].playerIndex == 1U &&
               nearlyEqual(sink.calls[0].value, 0.73f),
           "valid tank target camera assignment changed");
    sink.reset();
    AssignTankTargetCameraShakeAction invalidTankCamera;
    invalidTankCamera.playerIndex = 2U;
    invalidTankCamera.value = 0.81f;
    tankAction = invalidTankCamera;
    expect(dispatchCommandSideEffect(tankAction, sink) ==
                   CommandSideEffectDisposition::Rejected &&
               sink.calls.size() == 1U &&
               sink.calls[0].kind == CallKind::PlayerCameraShake &&
               sink.calls[0].playerIndex == 2U &&
               nearlyEqual(sink.calls[0].value, 0.81f),
           "invalid tank target camera was not explicitly rejected");
    sink.reset();
    invalidTankCamera.playerIndex =
        std::numeric_limits<std::size_t>::max();
    tankAction = invalidTankCamera;
    expect(dispatchCommandSideEffect(tankAction, sink) ==
                   CommandSideEffectDisposition::Rejected &&
               sink.calls.size() == 1U &&
               sink.calls[0].playerIndex ==
                   std::numeric_limits<std::size_t>::max(),
           "extreme tank target camera index was not rejected");

    reporter.beginSuite("dispatch-tank-audio-validation");
    sink.reset();
    RequestTankAudioAction validTankAudio;
    validTankAudio.cue = AudioCue::PlayerDestroyed;
    tankAction = validTankAudio;
    expect(dispatchCommandSideEffect(tankAction, sink) ==
                   CommandSideEffectDisposition::Applied &&
               sink.calls.size() == 1U &&
               sink.calls[0].kind == CallKind::Audio &&
               sink.calls[0].cue == AudioCue::PlayerDestroyed,
           "valid tank audio was not forwarded");
    sink.reset();
    RequestTankAudioAction invalidTankAudio;
    invalidTankAudio.cue = AudioCue::Count;
    tankAction = invalidTankAudio;
    expect(dispatchCommandSideEffect(tankAction, sink) ==
                   CommandSideEffectDisposition::Rejected &&
               sink.calls.empty(),
           "tank audio sentinel reached the sink");
    invalidTankAudio.cue = static_cast<AudioCue>(
        std::numeric_limits<std::size_t>::max());
    tankAction = invalidTankAudio;
    expect(dispatchCommandSideEffect(tankAction, sink) ==
                   CommandSideEffectDisposition::Rejected &&
               sink.calls.empty(),
           "out-of-range tank audio reached the sink");

    reporter.beginSuite("dispatch-tank-all-variant-order");
    sink.reset();
    sink.acceptedPlayerCount = 2U;
    RequestTankAudioAction orderedTankAudio;
    orderedTankAudio.cue = AudioCue::EnemyDestroyed;
    CommitTankShellImpactAction tankCommit;
    tankCommit.position = {-90.0f, -91.0f};
    const std::array<ShellTankPresentationAction, 7> orderedTankActions{{
        appendTank, tankArmor, tankExplosion, tankRadial,
        validTankCamera, orderedTankAudio, tankCommit}};
    for (std::size_t actionIndex = 0;
         actionIndex < orderedTankActions.size(); ++actionIndex)
    {
        const CommandSideEffectDisposition disposition =
            dispatchCommandSideEffect(orderedTankActions[actionIndex], sink);
        const CommandSideEffectDisposition expectedDisposition =
            actionIndex + 1U == orderedTankActions.size()
                ? CommandSideEffectDisposition::DomainCommitRequired
                : CommandSideEffectDisposition::Applied;
        expect(disposition == expectedDisposition,
               "tank variant returned wrong disposition at index " +
                   std::to_string(actionIndex));
    }
    expect(kindsAre(sink,
                    {CallKind::Event, CallKind::Event, CallKind::Impact,
                     CallKind::Explosion, CallKind::RadialCameraShake,
                     CallKind::PlayerCameraShake, CallKind::Audio}),
           "tank all-variant sink order changed");
    expect(sink.calls[0].event == firstTankEvent &&
               sink.calls[1].event == secondTankEvent &&
               sink.calls[5].playerIndex == 1U &&
               sink.calls[6].cue == AudioCue::EnemyDestroyed,
           "tank ordered sequence lost boundary payloads");

    reporter.beginSuite("dispatch-bonus-audio-explosion-and-event");
    sink.reset();
    BonusEffectCommand armorCue;
    armorCue.type = BonusEffectCommandType::EnemyArmorHitCue;
    expect(dispatchCommandSideEffect(armorCue, sink) ==
                   CommandSideEffectDisposition::Applied &&
               sink.calls.size() == 1U &&
               sink.calls[0].kind == CallKind::Audio &&
               sink.calls[0].cue == AudioCue::EnemyHit,
           "bonus armor cue did not request EnemyHit");
    BonusEffectCommand destroyedCue;
    destroyedCue.type = BonusEffectCommandType::EnemyDestroyedCue;
    expect(dispatchCommandSideEffect(destroyedCue, sink) ==
                   CommandSideEffectDisposition::Applied &&
               sink.calls.size() == 2U &&
               sink.calls[1].kind == CallKind::Audio &&
               sink.calls[1].cue == AudioCue::EnemyDestroyed,
           "bonus destruction cue did not request EnemyDestroyed");

    BonusEffectCommand bonusExplosion;
    bonusExplosion.type = BonusEffectCommandType::EnemyExplosion;
    bonusExplosion.position = {12.25f, -15.75f};
    expect(dispatchCommandSideEffect(bonusExplosion, sink) ==
                   CommandSideEffectDisposition::Applied &&
               sink.calls.size() == 3U &&
               sink.calls[2].kind == CallKind::Explosion,
           "bonus explosion selected the wrong sink leaf");
    expect(sameFloat3(sink.calls[2].position,
                      {12.25f, 0.42f, -15.75f}) &&
               sameColor(sink.calls[2].color,
                         {255U, 105U, 27U, 255U}),
           "bonus explosion lost fixed elevation or orange color");

    BonusEffectCommand bonusEvent;
    bonusEvent.type = BonusEffectCommandType::EnemyDestroyedEvent;
    bonusEvent.playerId = 4;
    bonusEvent.enemyId = 91;
    bonusEvent.enemyType = 3;
    bonusEvent.position = {-2.25f, 18.50f};
    bonusEvent.valueBefore = 4;
    bonusEvent.valueAfter = 0;
    bonusEvent.points = 200;
    expect(dispatchCommandSideEffect(bonusEvent, sink) ==
                   CommandSideEffectDisposition::Applied &&
               sink.calls.size() == 4U &&
               sink.calls[3].kind == CallKind::Event,
           "bonus destruction event selected the wrong sink leaf");
    GameEvent expectedBonusEvent;
    expectedBonusEvent.type = GameEventType::TankDestroyed;
    expectedBonusEvent.cause = GameEventCause::GrenadeBonus;
    expectedBonusEvent.position = bonusEvent.position;
    expectedBonusEvent.sourcePlayerId = bonusEvent.playerId;
    expectedBonusEvent.targetEnemyId = bonusEvent.enemyId;
    expectedBonusEvent.enemyType = bonusEvent.enemyType;
    expectedBonusEvent.valueBefore = bonusEvent.valueBefore;
    expectedBonusEvent.valueAfter = bonusEvent.valueAfter;
    expectedBonusEvent.points = bonusEvent.points;
    expect(sink.calls[3].event == expectedBonusEvent,
           "bonus destruction event payload or unrelated sentinels changed");
    expect(kindsAre(sink,
                    {CallKind::Audio, CallKind::Audio,
                     CallKind::Explosion, CallKind::Event}),
           "bonus armor-destroy-explosion-event order changed");

    reporter.beginSuite("dispatch-bonus-camera-domain-and-sentinels");
    sink.reset();
    sink.acceptedPlayerCount = 2U;
    BonusEffectCommand validBonusCamera;
    validBonusCamera.type =
        BonusEffectCommandType::AssignPlayerCameraShake;
    validBonusCamera.playerIndex = 1;
    validBonusCamera.scalar = 0.34f;
    expect(dispatchCommandSideEffect(validBonusCamera, sink) ==
                   CommandSideEffectDisposition::Applied &&
               sink.calls.size() == 1U &&
               sink.calls[0].kind == CallKind::PlayerCameraShake &&
               sink.calls[0].playerIndex == 1U &&
               nearlyEqual(sink.calls[0].value, 0.34f),
           "valid bonus camera assignment changed");

    sink.reset();
    BonusEffectCommand negativeBonusCamera = validBonusCamera;
    negativeBonusCamera.playerIndex = -1;
    expect(dispatchCommandSideEffect(negativeBonusCamera, sink) ==
                   CommandSideEffectDisposition::Ignored &&
               sink.calls.empty(),
           "negative bonus camera index was not silently ignored");

    BonusEffectCommand rejectedBonusCamera = validBonusCamera;
    rejectedBonusCamera.playerIndex = 2;
    rejectedBonusCamera.scalar = 0.55f;
    expect(dispatchCommandSideEffect(rejectedBonusCamera, sink) ==
                   CommandSideEffectDisposition::Ignored &&
               sink.calls.size() == 1U &&
               sink.calls[0].kind == CallKind::PlayerCameraShake &&
               sink.calls[0].playerIndex == 2U &&
               nearlyEqual(sink.calls[0].value, 0.55f),
           "sink-rejected bonus camera was not silently ignored");

    sink.reset();
    BonusEffectCommand steel;
    steel.type = BonusEffectCommandType::ActivateGovernmentSteel;
    expect(dispatchCommandSideEffect(steel, sink) ==
                   CommandSideEffectDisposition::DomainCommitRequired &&
               sink.calls.empty(),
           "Shovel steel domain commit triggered a sink call");
    BonusEffectCommand sentinel;
    sentinel.type = BonusEffectCommandType::Count;
    expect(dispatchCommandSideEffect(sentinel, sink) ==
                   CommandSideEffectDisposition::Ignored &&
               sink.calls.empty(),
           "bonus command sentinel triggered a sink call");
    BonusEffectCommand outOfRangeBonus;
    outOfRangeBonus.type = static_cast<BonusEffectCommandType>(255);
    expect(dispatchCommandSideEffect(outOfRangeBonus, sink) ==
                   CommandSideEffectDisposition::Ignored &&
               sink.calls.empty(),
           "out-of-range bonus command triggered a sink call");

    reporter.beginSuite("dispatch-all-domain-commits-are-sink-free");
    sink.reset();
    mapAction = mapCommit;
    tankAction = tankCommit;
    const CommandSideEffectDisposition mapCommitDisposition =
        dispatchCommandSideEffect(mapAction, sink);
    const CommandSideEffectDisposition tankCommitDisposition =
        dispatchCommandSideEffect(tankAction, sink);
    const CommandSideEffectDisposition steelCommitDisposition =
        dispatchCommandSideEffect(steel, sink);
    expect(mapCommitDisposition ==
               CommandSideEffectDisposition::DomainCommitRequired,
           "map/core shell commit did not remain a domain operation");
    expect(tankCommitDisposition ==
               CommandSideEffectDisposition::DomainCommitRequired,
           "tank shell commit did not remain a domain operation");
    expect(steelCommitDisposition ==
               CommandSideEffectDisposition::DomainCommitRequired,
           "Shovel steel did not remain a domain operation");
    expect(sink.calls.empty(),
           "one or more domain commits invoked a side-effect sink method");

    reporter.finish();
    return passed ? 0 : 1;
}
