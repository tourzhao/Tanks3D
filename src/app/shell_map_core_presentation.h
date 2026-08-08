#ifndef TANKS3D_APP_SHELL_MAP_CORE_PRESENTATION_H
#define TANKS3D_APP_SHELL_MAP_CORE_PRESENTATION_H

#include "app/presentation_values.h"
#include "audio/audio_cue.h"
#include "core/coordinates.h"
#include "game/game_event.h"

#include <array>
#include <cstddef>
#include <optional>
#include <variant>
#include <vector>

namespace tanks3d::game
{
struct ShellPhysicalImpactResult;
}

namespace tanks3d::app
{
enum class ShellMapCorePresentationStep : unsigned char
{
    PhysicalEventsAppended,
    BrickImpactFxSpawned,
    WallBreachImpactFxSpawned,
    SurfaceImpactFxSpawned,
    CameraShakePassCompleted,
    CoreExplosionFxSpawned,
    AudioRequested,
    ShellImpactCommitted
};

struct AppendMapCoreEventsAction
{
    std::vector<game::GameEvent> events{};
};

struct SpawnMapCoreBrickImpactAction
{
    Float3 position{};
    Float3 normal{};
    bool power = false;
    bool destroyed = false;
};

struct SpawnGovernmentWallBreachImpactAction
{
    Float3 position{};
    Float3 normal{};
    bool heavy = true;
};

struct SpawnMapCoreSurfaceImpactAction
{
    Float3 position{};
    Float3 normal{};
    bool heavy = false;
};

struct ApplyMapCoreRadialCameraShakeAction
{
    core::XZ origin{};
    float maximum = 0.0f;
    float distanceFalloff = 0.0f;
};

struct SpawnGovernmentCoreExplosionAction
{
    Float3 position{};
    Rgba8 color{};
};

struct RequestMapCoreAudioAction
{
    audio::AudioCue cue = audio::AudioCue::Count;
};

struct CommitMapCoreShellImpactAction
{
    core::XZ position{};
};

using ShellMapCorePresentationAction = std::variant<
    AppendMapCoreEventsAction, SpawnMapCoreBrickImpactAction,
    SpawnGovernmentWallBreachImpactAction,
    SpawnMapCoreSurfaceImpactAction,
    ApplyMapCoreRadialCameraShakeAction,
    SpawnGovernmentCoreExplosionAction, RequestMapCoreAudioAction,
    CommitMapCoreShellImpactAction>;

inline constexpr std::size_t kMaximumShellMapCorePresentationActions = 6U;

struct ShellMapCorePresentationCommand
{
    std::array<ShellMapCorePresentationAction,
               kMaximumShellMapCorePresentationActions> actions{};
    std::size_t actionCount = 0U;

    void append(ShellMapCorePresentationAction action);
};

ShellMapCorePresentationStep shellMapCorePresentationStep(
    const ShellMapCorePresentationAction &action);

// Projects one already-committed map/core result into an owned synchronous
// command. Invalid snapshots or event batches are rejected. A dead-core hit is
// valid and produces only the final shell-impact action.
std::optional<ShellMapCorePresentationCommand>
makeShellMapCorePresentationCommand(
    const game::ShellPhysicalImpactResult &physicalImpact,
    std::vector<game::GameEvent> physicalEvents);
} // namespace tanks3d::app

#endif
