#ifndef TANKS3D_APP_SHELL_TANK_PRESENTATION_H
#define TANKS3D_APP_SHELL_TANK_PRESENTATION_H

#include "app/presentation_values.h"
#include "audio/audio_cue.h"
#include "core/coordinates.h"
#include "game/game_event.h"

#include <array>
#include <cstddef>
#include <variant>
#include <vector>

namespace tanks3d::game
{
struct ShellPhysicalImpactResult;
}

namespace tanks3d::app
{
enum class ShellTankPresentationStep : unsigned char
{
    PhysicalEventsAppended,
    ArmorImpactFxSpawned,
    TankExplosionFxSpawned,
    CameraShakePassCompleted,
    TargetCameraShakeAssigned,
    AudioRequested,
    ShellImpactCommitted
};

struct AppendTankEventsAction
{
    std::vector<game::GameEvent> events{};
};

struct SpawnTankArmorImpactAction
{
    Float3 position{};
    Float3 normal{};
    bool heavy = true;
};

struct SpawnTankExplosionAction
{
    Float3 position{};
    Rgba8 color{};
};

struct ApplyTankRadialCameraShakeAction
{
    core::XZ origin{};
    float maximum = 0.0f;
    float distanceFalloff = 0.0f;
};

struct AssignTankTargetCameraShakeAction
{
    std::size_t playerIndex = 0U;
    float value = 0.0f;
};

struct RequestTankAudioAction
{
    audio::AudioCue cue = audio::AudioCue::Count;
};

struct CommitTankShellImpactAction
{
    core::XZ position{};
};

using ShellTankPresentationAction = std::variant<
    AppendTankEventsAction, SpawnTankArmorImpactAction,
    SpawnTankExplosionAction, ApplyTankRadialCameraShakeAction,
    AssignTankTargetCameraShakeAction, RequestTankAudioAction,
    CommitTankShellImpactAction>;

inline constexpr std::size_t kMaximumShellTankPresentationActions = 6U;

struct ShellTankPresentationCommand
{
    std::array<ShellTankPresentationAction,
               kMaximumShellTankPresentationActions> actions{};
    std::size_t actionCount = 0U;

    void append(ShellTankPresentationAction action);
};

ShellTankPresentationStep shellTankPresentationStep(
    const ShellTankPresentationAction &action);

SpawnTankArmorImpactAction makePlayerTankArmorImpactAction(
    core::XZ contactPosition, core::XZ incomingVelocity);

// Projects one already-committed physical result into an owned, synchronous
// command. Unresolved, unapplied, and invalid-player results are rejected with
// an empty command. The player color is injected because nation/player
// rendering stays outside this raylib-free module.
ShellTankPresentationCommand makeShellTankPresentationCommand(
    const game::ShellPhysicalImpactResult &physicalImpact,
    std::vector<game::GameEvent> physicalEvents,
    Rgba8 playerExplosionColor);
} // namespace tanks3d::app

#endif
