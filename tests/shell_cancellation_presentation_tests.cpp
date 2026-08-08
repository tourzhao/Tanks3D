#include "app/shell_cancellation_presentation.h"

#include "test_support.h"

#include <array>
#include <cmath>
#include <string>

namespace
{
using tanks3d::app::makeShellCancellationPresentationCommand;
using tanks3d::audio::AudioCue;
using tanks3d::core::CardinalDirection;
using tanks3d::core::XZ;
using tanks3d::game::GameEvent;
using tanks3d::game::GameEventCause;
using tanks3d::game::GameEventType;

bool nearlyEqual(float first, float second, float tolerance = 0.00001f)
{
    return std::fabs(first - second) <= tolerance;
}

bool sameXZ(XZ first, XZ second)
{
    return nearlyEqual(first.x, second.x) &&
           nearlyEqual(first.z, second.z);
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

    reporter.beginSuite("shell-cancellation-presentation-command");
    GameEvent event;
    event.type = GameEventType::ShellCancelled;
    event.cause = GameEventCause::None;
    event.position = {12.25f, 19.75f};
    event.direction = CardinalDirection::West;
    event.sourcePlayerId = 1;
    event.sourceEnemyId = 73;
    event.valueBefore = 4;
    event.valueAfter = 2;
    event.points = 125;
    event.power = true;
    const auto command = makeShellCancellationPresentationCommand(event);
    expect(command.has_value() && command->appendEvent.event == event &&
               sameXZ(command->spawnImpact.position, event.position) &&
               nearlyEqual(command->spawnImpact.elevation, 0.67f) &&
               sameXZ(command->spawnImpact.normalXZ, {}) &&
               nearlyEqual(command->spawnImpact.normalY, 1.0f) &&
               !command->spawnImpact.heavy &&
               command->requestAudio.cue == AudioCue::BulletHit,
           "a valid cancellation event changed its owned event, FX, or "
           "audio payload");

    reporter.beginSuite("shell-cancellation-presentation-boundaries");
    constexpr std::array<GameEventType, 9> kOtherEventTypes{{
        GameEventType::ShellFired,
        GameEventType::BrickHit,
        GameEventType::TankDamaged,
        GameEventType::TankDestroyed,
        GameEventType::PlayerRespawned,
        GameEventType::BonusSpawned,
        GameEventType::BonusCollected,
        GameEventType::BaseDamaged,
        GameEventType::StageEnded,
    }};
    bool allRejected = true;
    for (GameEventType type : kOtherEventTypes)
    {
        event.type = type;
        allRejected = allRejected &&
                      !makeShellCancellationPresentationCommand(event)
                           .has_value();
    }
    expect(allRejected,
           "a non-cancellation event produced cancellation presentation");

    reporter.finish();
    return passed ? 0 : 1;
}
