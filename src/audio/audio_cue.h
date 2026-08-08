#ifndef TANKS3D_AUDIO_AUDIO_CUE_H
#define TANKS3D_AUDIO_AUDIO_CUE_H

#include <cstddef>

namespace tanks3d::audio
{
enum class AudioCue : std::size_t
{
    StageStart,
    Pause,
    GameOver,
    HighScoreBeaten,
    MenuSelect,
    BonusAppeared,
    BonusObtained,
    BrickHit,
    BoundaryHit,
    SteelHit,
    BulletHit,
    EagleDestroyed,
    EnemyDestroyed,
    EnemyHit,
    PlayerDestroyed,
    PlayerFired,
    PlayerHit,
    PlayerIdle,
    PlayerLifeUp,
    PlayerMoving,
    PlayerRespawn,
    ScoreCounted,
    Count
};
} // namespace tanks3d::audio

#endif
