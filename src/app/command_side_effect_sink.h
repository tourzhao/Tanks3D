#ifndef TANKS3D_APP_COMMAND_SIDE_EFFECT_SINK_H
#define TANKS3D_APP_COMMAND_SIDE_EFFECT_SINK_H

#include "app/presentation_values.h"
#include "audio/audio_cue.h"
#include "core/coordinates.h"
#include "game/game_event.h"

#include <cstddef>

namespace tanks3d::app
{
// Narrow synchronous boundary shared by detached gameplay/presentation
// commands. Implementations perform each leaf operation before returning;
// commands and observers therefore retain their established ordering without
// introducing a queue or storing references to live game state.
class CommandSideEffectSink
{
public:
    virtual ~CommandSideEffectSink() = default;

    virtual void emitEvent(const game::GameEvent &event) = 0;
    virtual void spawnImpact(Float3 position, Float3 normal,
                             bool heavy) = 0;
    virtual void spawnBrickImpact(Float3 position, Float3 normal,
                                  bool power, bool destroyed) = 0;
    virtual void spawnExplosion(Float3 position, Rgba8 color) = 0;
    virtual void applyRadialCameraShake(core::XZ origin, float maximum,
                                        float distanceFalloff) = 0;
    virtual bool assignPlayerCameraShake(std::size_t playerIndex,
                                         float value) = 0;
    virtual void requestAudio(audio::AudioCue cue) = 0;
};
} // namespace tanks3d::app

#endif
