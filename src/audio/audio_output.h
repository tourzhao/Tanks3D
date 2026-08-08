#ifndef TANKS3D_AUDIO_AUDIO_OUTPUT_H
#define TANKS3D_AUDIO_AUDIO_OUTPUT_H

#include "audio/audio_cue.h"

namespace tanks3d::audio
{
// Runtime boundary held through a non-owning pointer by orchestration. Resource
// loading, device lifetime, and concrete voice policy remain platform work.
class AudioOutput
{
public:
    virtual ~AudioOutput() = default;

    virtual void play(AudioCue cue) = 0;
    virtual void updateEngine(bool active, bool moving) = 0;
    virtual void stopAll() = 0;
};
} // namespace tanks3d::audio

#endif
