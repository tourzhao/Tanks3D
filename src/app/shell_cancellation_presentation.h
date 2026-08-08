#ifndef TANKS3D_APP_SHELL_CANCELLATION_PRESENTATION_H
#define TANKS3D_APP_SHELL_CANCELLATION_PRESENTATION_H

#include "audio/audio_cue.h"
#include "core/coordinates.h"
#include "game/game_event.h"

#include <optional>

namespace tanks3d::app
{
enum class ShellCancellationPresentationStep : unsigned char
{
    EventAppended,
    ImpactFxSpawned,
    BulletHitAudioRequested
};

struct AppendShellCancellationEventAction
{
    game::GameEvent event{};
};

struct SpawnShellCancellationImpactAction
{
    core::XZ position{};
    float elevation = 0.0f;
    core::XZ normalXZ{};
    float normalY = 0.0f;
    bool heavy = false;
};

struct RequestShellCancellationAudioAction
{
    audio::AudioCue cue = audio::AudioCue::Count;
};

struct ShellCancellationPresentationCommand
{
    AppendShellCancellationEventAction appendEvent{};
    SpawnShellCancellationImpactAction spawnImpact{};
    RequestShellCancellationAudioAction requestAudio{};
};

// Builds the three historical presentation actions from the canonical,
// detached cancellation event. Other event types are rejected, and no action
// retains live shell state.
std::optional<ShellCancellationPresentationCommand>
makeShellCancellationPresentationCommand(game::GameEvent event);
} // namespace tanks3d::app

#endif
