#ifndef TANKS3D_APP_GAME_SESSION_H
#define TANKS3D_APP_GAME_SESSION_H

#include "app/command_side_effect_dispatch.h"
#include "app/input_adapter.h"
#include "app/shell_cancellation_presentation.h"
#include "app/shell_map_core_presentation.h"
#include "app/shell_tank_presentation.h"
#include "audio/audio_cue.h"
#include "audio/audio_output.h"
#include "core/coordinates.h"
#include "core/gameplay_rules.h"
#include "core/nation.h"
#include "game/bonus_system.h"
#include "game/bonus_identity.h"
#include "game/combat_system.h"
#include "game/enemy_system.h"
#include "game/entities.h"
#include "game/game_event.h"
#include "game/player_system.h"
#include "game/settlement_system.h"
#include "game/stage_map.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <iomanip>
#include <limits>
#include <locale>
#include <memory>
#include <random>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace tanks3d::app
{
namespace fs = std::filesystem;
using tanks3d::game::BonusApplication;
using tanks3d::game::BonusCollectionCallbacks;
using tanks3d::game::BonusCollectionIntent;
using tanks3d::game::BonusEffectCommand;
using tanks3d::game::BonusEffectCommandType;
using tanks3d::game::BonusReleaseIntent;
using tanks3d::game::BonusReleaseRandom;
using tanks3d::game::Enemy;
using tanks3d::game::EnemyFireConfiguration;
using tanks3d::game::EnemyFireOutcome;
using tanks3d::game::EnemyFireRandom;
using tanks3d::game::EnemyFramePhase;
using tanks3d::game::EnemyShellLaunchIntent;
using tanks3d::game::EnemySpawnConfiguration;
using tanks3d::game::EnemySpawnOutcome;
using tanks3d::game::EnemySpawnRandom;
using tanks3d::game::EnemySpawnState;
using tanks3d::game::EnemySteeringOutcome;
using tanks3d::game::EnemySteeringRandom;
using tanks3d::game::BonusType;
using tanks3d::game::CombatOutcome;
using tanks3d::game::CombatTarget;
using tanks3d::game::GameEvent;
using tanks3d::game::GameEventType;
using tanks3d::game::Player;
using tanks3d::game::PlayerControlFrame;
using tanks3d::game::PlayerControlPlan;
using tanks3d::game::PlayerDeathState;
using tanks3d::game::PlayerDeathTransition;
using tanks3d::game::PlayerFireOutcome;
using tanks3d::game::PlayerFireParameters;
using tanks3d::game::PlayerFrameClockState;
using tanks3d::game::PlayerFramePhase;
using tanks3d::game::PlayerInputFrame;
using tanks3d::game::PlayerMovementParameters;
using tanks3d::game::PlayerMovementState;
using tanks3d::game::PlayerSpawnOutcome;
using tanks3d::game::PlayerSpawnParameters;
using tanks3d::game::PlayerSpawnProgression;
using tanks3d::game::PlayerSpawnState;
using tanks3d::game::PlayerShellLaunchIntent;
using tanks3d::game::PlayerHitResult;
using tanks3d::game::Pickup;
using tanks3d::game::Shell;
using tanks3d::game::ShellCancellationOutcome;
using tanks3d::game::ShellFrameSchedule;
using tanks3d::game::ShellPhysicalImpactResult;
using tanks3d::game::ShellOwner;
using tanks3d::game::SettlementBeginKind;
using tanks3d::game::SettlementBeginPlan;
using tanks3d::game::SettlementCompletion;
using tanks3d::game::SettlementState;
using tanks3d::game::SettlementTransitionKind;
using tanks3d::game::SettlementTransitionPlan;
using tanks3d::game::SettlementUpdate;
using tanks3d::game::StageTally;
using tanks3d::game::StageMap;
using tanks3d::game::StageEndReason;
using tanks3d::game::advanceActiveEnemyMovement;
using tanks3d::game::advanceActiveEnemySteering;
using tanks3d::game::advanceBonusPickups;
using tanks3d::game::advanceBonusReleaseTransaction;
using tanks3d::game::advanceEnemyFireTransaction;
using tanks3d::game::advanceEnemySpawnTransaction;
using tanks3d::game::advanceActivePlayerMovement;
using tanks3d::game::advanceInactivePlayerDeath;
using tanks3d::game::advancePlayerFireTransaction;
using tanks3d::game::advanceShellMicrostep;
using tanks3d::game::beginEnemyFrame;
using tanks3d::game::beginPlayerFrame;
using tanks3d::game::beginShellImpact;
using tanks3d::game::bonusOverlapsGovernmentBase;
using tanks3d::game::enemyDestructionComplete;
using tanks3d::game::eventForShellCancellation;
using tanks3d::game::eventsForPhysicalShellImpact;
using tanks3d::game::governmentWallSegment;
using tanks3d::game::kEnemySpawnPoints;
using tanks3d::game::kEnemyTypeCount;
using tanks3d::game::kEnemyTrackDustCooldown;
using tanks3d::game::kEnemyCreationDuration;
using tanks3d::game::kEnemyInitialFireDelay;
using tanks3d::game::kEnemySpawnInterval;
using tanks3d::game::kEnemySpawnRetryInterval;
using tanks3d::game::kGovernmentBaseCenter;
using tanks3d::game::kGovernmentWallCount;
using tanks3d::game::kMapSize;
using tanks3d::game::kPlayerTrackDustCooldown;
using tanks3d::game::kPlayerSpawnPoints;
using tanks3d::game::kShellSpawnDistance;
using tanks3d::game::kStageCount;
using tanks3d::game::kTankRadius;
using tanks3d::game::planSettlementBegin;
using tanks3d::game::planSettlementTransition;
using tanks3d::game::planPlayerControl;
using tanks3d::game::normalizedStage;
using tanks3d::game::prepareShellFrame;
using tanks3d::game::preparePlayerSpawnState;
using tanks3d::game::removeExpiredShells;
using tanks3d::game::resolveShellCancellations;
using tanks3d::game::resolveShellPhysicalImpact;
using tanks3d::game::shellEvent;
using tanks3d::game::bonusName;
using tanks3d::app::CommitMapCoreShellImpactAction;
using tanks3d::app::CommitTankShellImpactAction;
using tanks3d::app::CommandSideEffectDisposition;
using tanks3d::app::CommandSideEffectSink;
using tanks3d::app::Float3;
using tanks3d::app::Rgba8;
using tanks3d::app::CameraPlanarBasis;
using tanks3d::app::cameraPlanarBasis;
using tanks3d::app::ShellCancellationPresentationCommand;
using tanks3d::app::ShellCancellationPresentationStep;
using tanks3d::app::ShellMapCorePresentationAction;
using tanks3d::app::ShellMapCorePresentationCommand;
using tanks3d::app::ShellTankPresentationAction;
using tanks3d::app::ShellTankPresentationCommand;
using tanks3d::app::SpawnTankArmorImpactAction;
using tanks3d::app::makePlayerTankArmorImpactAction;
using tanks3d::app::makeShellCancellationPresentationCommand;
using tanks3d::app::makeShellMapCorePresentationCommand;
using tanks3d::app::makeShellTankPresentationCommand;
using tanks3d::app::dispatchCommandSideEffect;
using tanks3d::app::normalizedCameraYawDegrees;
using tanks3d::audio::AudioCue;
using tanks3d::audio::AudioOutput;
using tanks3d::core::AdvancedGameSettings;
using tanks3d::core::CardinalDirection;
using tanks3d::core::Nation;
using tanks3d::core::XZ;
using tanks3d::core::axisAlignedCentersOverlap;
using tanks3d::core::cardinalYaw;
using tanks3d::core::distanceSquared;
using tanks3d::core::normalizedAdvancedSettings;
using tanks3d::core::normalizedEnemyTuningPercent;
using tanks3d::core::normalizedNation;
using tanks3d::core::playerLevelStats;

constexpr int kEnemiesPerStage = 20;
constexpr float kPi = 3.14159265358979323846f;
constexpr float kClassicBaseEnemySpeed = 5.0f;
constexpr float kEnemyMovementSpeedScale = 0.80f;
constexpr float kBaseEnemySpeed =
    kClassicBaseEnemySpeed * kEnemyMovementSpeedScale;
constexpr float kFastEnemySpeed =
    kClassicBaseEnemySpeed * 1.3f * kEnemyMovementSpeedScale;
// Normal enemy steering remains on its original 100-899 ms clock.  This local
// fallback only engages after a tank has repeatedly failed to advance.
constexpr float kPlayerReloadTime = 0.120f;
// Show surrounding lanes at the same viewing angle. Co-op uses this minimum
// span too, then expands for player separation and the current aspect ratio.
constexpr float kSoloCameraSpan = 18.5f;
// Camera elevation is measured above the ground plane. The supported range
// keeps tank silhouettes readable at the low endpoint while allowing a much
// flatter, near-top-down composition at the high endpoint.
constexpr int kCameraElevationMinimumDegrees = 40;
constexpr int kCameraElevationMaximumDegrees = 70;
inline constexpr int kCameraElevationStepDegrees = 5;
constexpr int kDefaultCameraElevationDegrees = 50;
constexpr float kGameplayCameraOrbitDistance = 21.017376f;
constexpr float kGameplayCameraTargetHeight = 0.35f;
constexpr float kGameplayCameraFollowResponsiveness = 12.0f;

inline int normalizedCameraElevationDegrees(int requestedDegrees)
{
    return std::clamp(requestedDegrees, kCameraElevationMinimumDegrees,
                      kCameraElevationMaximumDegrees);
}

struct GameplayCameraElevationGeometry
{
    float depthOffset = 0.0f;
    float verticalOffset = 0.0f;
    float groundDepthProjection = 0.0f;
};

inline GameplayCameraElevationGeometry gameplayCameraElevationGeometry(
    int requestedDegrees, float orthographicSpan = kSoloCameraSpan)
{
    const float radians =
        static_cast<float>(normalizedCameraElevationDegrees(
            requestedDegrees)) *
        (kPi / 180.0f);
    const float groundDepthProjection = std::sin(radians);
    // A large orthographic image can extend behind a fixed camera's near
    // plane. Retreat along the same axis without changing its image scale.
    // Four units leave clearance for foreground terrain and tank height;
    // the normal 18.5-unit view keeps its original orbit at every elevation.
    const float orbitDistance = std::max(
        kGameplayCameraOrbitDistance,
        orthographicSpan * 0.5f * std::cos(radians) /
            groundDepthProjection + 4.0f);
    return {
        std::cos(radians) * orbitDistance,
        groundDepthProjection * orbitDistance,
        groundDepthProjection};
}

inline float gameplayCameraSpan(XZ playerSeparation, int cameraYawDegrees,
                         int cameraElevationDegrees, float aspectRatio)
{
    const float aspect = std::isfinite(aspectRatio) && aspectRatio > 0.0f
                             ? aspectRatio
                             : (1280.0f / 720.0f);
    const CameraPlanarBasis basis = cameraPlanarBasis(cameraYawDegrees);
    const GameplayCameraElevationGeometry elevation =
        gameplayCameraElevationGeometry(cameraElevationDegrees);
    const float verticalSeparation =
        std::fabs(playerSeparation.x * basis.offsetX +
                  playerSeparation.z * basis.offsetZ) *
        elevation.groundDepthProjection;
    const float horizontalSeparation =
        std::fabs(playerSeparation.x * basis.rightX +
                  playerSeparation.z * basis.rightZ);
    // Preserve both tanks' screen-space margins when a window becomes narrow.
    // A fixed vertical span cap can crop even their centers in portrait views.
    return std::max({kSoloCameraSpan, verticalSeparation + 4.5f,
                     (horizontalSeparation + 6.0f) / aspect});
}

constexpr float kTankPairCollisionExtent = kTankRadius * 2.0f;
constexpr float kTankActivationRetryInterval = 0.05f;
constexpr float kStageIntroDuration = 3.2f;
constexpr float kStageEndDelay = 5.0f;
constexpr float kGameOverReportDelay = 3.1f;
constexpr float kHighScoreDisplayDuration = 5.2f;
// The original ST_CREATE sprite has ten 100 ms frames.  Enemy tanks do not
// exist physically during this warning; only the pulsing star is rendered.
constexpr int kEnemyCreationFrameCount = 10;
constexpr float kEnemyCreationFrameDuration =
    kEnemyCreationDuration / static_cast<float>(kEnemyCreationFrameCount);

constexpr float enemyRateScale(int requestedPercent)
{
    return 1.0f +
           static_cast<float>(normalizedEnemyTuningPercent(requestedPercent)) /
               100.0f;
}

constexpr float intervalForEnemyRate(float baseInterval,
                                     int requestedPercent)
{
    return baseInterval / enemyRateScale(requestedPercent);
}

constexpr float enemyMovementSpeedForType(
    int enemyType, const AdvancedGameSettings &settings)
{
    const float baseSpeed = enemyType == 1 ? kFastEnemySpeed : kBaseEnemySpeed;
    return baseSpeed * enemyRateScale(settings.enemySpeedPercent);
}

inline int enemyCreationFrame(float remainingTime)
{
    if (remainingTime <= 0.0f)
        return -1;
    const float elapsed = std::clamp(kEnemyCreationDuration - remainingTime,
                                     0.0f,
                                     kEnemyCreationDuration - 0.0001f);
    return std::clamp(static_cast<int>(elapsed /
                                      kEnemyCreationFrameDuration),
                      0, kEnemyCreationFrameCount - 1);
}

inline float enemyCreationScale(int frame)
{
    static constexpr std::array<float, kEnemyCreationFrameCount> scales{{
        0.25f, 0.45f, 0.70f, 1.00f, 0.70f,
        0.45f, 0.25f, 0.45f, 0.70f, 1.00f}};
    return frame >= 0 && frame < kEnemyCreationFrameCount
               ? scales[static_cast<std::size_t>(frame)]
               : 0.0f;
}

inline constexpr std::chrono::nanoseconds kInteractiveFrameBudget{8'333'333};

using ShellCancellationPresentationObserver =
    std::function<void(ShellCancellationPresentationStep,
                       const ShellCancellationPresentationCommand &)>;

// This synchronous, default-off seam observes owned actions after their real
// side effects. Normal play retains no command queue or trace.
using ShellMapCorePresentationObserver = std::function<void(
    const ShellMapCorePresentationAction &)>;

// Tank branches intentionally keep their historical asymmetry. Commands own
// their payload and are consumed synchronously for exactly one shell; normal
// play retains no queue or trace. The player armor action needs the separate
// pre-commit hook because it occurs before HP/Boat/lifecycle mutation.
using ShellTankPresentationObserver = std::function<void(
    const ShellTankPresentationAction &,
    const ShellPhysicalImpactResult &, const Shell &)>;
using PlayerTankPreCommitFxObserver =
    std::function<void(const SpawnTankArmorImpactAction &,
                       const Player &, const Shell &)>;

enum class EnemyShellLaunchPresentationStep : unsigned char
{
    ShellInserted,
    EventAppended,
    MuzzleFlashSpawned
};

// Empty during normal play. Transitional production-path tests use this seam
// to observe each real launch side effect after it occurs without moving
// renderer values into EnemySystem or retaining a command queue.
using EnemyShellLaunchPresentationObserver = std::function<void(
    EnemyShellLaunchPresentationStep,
    const EnemyShellLaunchIntent &)>;

enum class PlayerShellLaunchPresentationStep : unsigned char
{
    ShellInserted,
    EventAppended,
    MuzzleFlashSpawned,
    CameraShakeCommitted,
    AudioRequested
};

// Empty during normal play. Transitional production-path tests observe the
// concrete player launch adapter after each real side effect. The first step
// runs inside the synchronous planner callback, after the reload clock reset.
// The observer must be non-throwing, must not re-enter or mutate Game3D, and
// must not retain the intent.
using PlayerShellLaunchPresentationObserver = std::function<void(
    PlayerShellLaunchPresentationStep,
    const PlayerShellLaunchIntent &)>;

enum class BonusReleasePresentationStep : unsigned char
{
    PickupInserted,
    SpawnEventAppended,
    AudioRequested
};

// Empty during normal play. Transitional production-path tests observe the
// concrete release adapter after each real side effect without retaining a
// command queue or moving event/audio values into BonusSystem. The synchronous
// observer must not re-enter or mutate Game3D and must not retain intent
// references.
using BonusReleasePresentationObserver = std::function<void(
    BonusReleasePresentationStep,
    const BonusReleaseIntent &)>;

enum class BonusCollectionPresentationStep : unsigned char
{
    CollectionEventAppended,
    ApplicationCommitted,
    CommandsConsumed,
    MessageCommitted,
    AudioRequested,
    EventPointsCommitted,
    PickupRemoved
};

// Empty during normal play. This synchronous seam observes the real outer
// collection adapter after each phase. It must be non-throwing, must not
// re-enter or mutate Game3D, retain references, or retain the optional
// application pointer.
using BonusCollectionPresentationObserver = std::function<void(
    BonusCollectionPresentationStep,
    const BonusCollectionIntent &, const BonusApplication *)>;

enum class SettlementBeginPresentationStep : unsigned char
{
    PlanReady,
    ReportCommitted,
    StageEndEventAppended,
    AudioStopBoundaryPassed
};

// Empty during normal play. Tests observe report entry after each concrete
// phase without moving GameEvent or audio output into SettlementSystem. The
// final phase means the optional AudioOutput stop was processed; it does not
// claim knowledge of device-level voice state when no output is installed.
// synchronous observer must be non-throwing, must not re-enter or mutate
// Game3D, and must not retain the plan reference.
using SettlementBeginPresentationObserver = std::function<void(
    SettlementBeginPresentationStep, const SettlementBeginPlan &)>;

enum class SettlementTransitionPresentationStep : unsigned char
{
    PlanReady,
    HighScoreCommitted,
    HighScoreAudioRequested,
    HighScoreDisplayCommitted,
    StageCandidateRequested,
    StageCandidatePrepared,
    StageCandidateRejected,
    PlayerProgressionCommitted,
    StageLoadCommitted,
    MenuRequested
};

// Empty during normal play. Tests observe the real two-phase completion
// consumer after each phase without moving map, audio, or navigation into
// SettlementSystem. The synchronous observer must be non-throwing, must not
// re-enter or mutate Game3D, and must not retain the plan reference.
using SettlementTransitionPresentationObserver = std::function<void(
    SettlementTransitionPresentationStep,
    const SettlementTransitionPlan &)>;

// Empty during normal play. Tests attach this synchronous observer to semantic
// audio requests, including requests made while no AudioOutput is installed.
// The observer runs after the real AudioOutput call and retains no runtime trace.
// It must not re-enter or mutate Game3D. Engine-bed updates are deliberately
// outside this one-shot cue seam.
using AudioCueRequestObserver = std::function<void(AudioCue)>;

inline XZ forwardFromYaw(float yaw)
{
    return {std::sin(yaw), -std::cos(yaw)};
}

// These parameters affect only the visible national-base models. Shared
// collision geometry and health rules live with StageMap in game/.


struct SessionDigest
{
    // Canonical, explicit field serialization avoids hashing object padding and
    // makes a deterministic mismatch inspectable in a debugger.
    std::string state;
};

inline bool operator==(const SessionDigest &first, const SessionDigest &second)
{
    return first.state == second.state;
}

struct CameraRig
{
    Float3 position{kGovernmentBaseCenter.x,
                     kGameplayCameraTargetHeight +
                         kGameplayCameraOrbitDistance,
                     kGovernmentBaseCenter.z};
    Float3 target{kGovernmentBaseCenter.x, kGameplayCameraTargetHeight,
                   kGovernmentBaseCenter.z};
    bool initialized = false;
};

inline XZ playerSpawn(int id)
{
    return kPlayerSpawnPoints[static_cast<std::size_t>(id == 0 ? 0 : 1)];
}

inline Rgba8 playerIdentityColor(int id)
{
    return id == 0 ? Rgba8{236, 145, 24, 255} : Rgba8{36, 205, 94, 255};
}
struct SessionTestAccess;

inline void preparePlayerSpawn(Player &player, bool resetLevel)
{
    PlayerSpawnState state;
    state.movement.position = player.position;
    state.movement.yaw = player.yaw;
    state.movement.driveDirection = player.driveDirection;
    state.movement.movementDirection = player.movementDirection;
    state.movement.moving = player.moving;
    state.movement.iceSlipTimer = player.iceSlipTimer;
    state.movement.onIce = player.onIce;
    state.clocks.creationTimer = player.creationTimer;
    state.clocks.fireCooldown = player.fireCooldown;
    state.clocks.dustCooldown = player.dustCooldown;
    state.clocks.shieldTimer = player.shieldTimer;
    state.clocks.streakPopupTimer = player.streakPopupTimer;
    state.hitPoints = player.hitPoints;
    state.level = player.level;
    state.active = player.active;
    state.hasBoat = player.hasBoat;
    state.respawnTimer = player.respawnTimer;
    state.deathTimer = player.deathTimer;
    state.directKillStreak = player.directKillStreak;

    PlayerSpawnParameters parameters;
    parameters.spawnPosition = playerSpawn(player.id);
    parameters.maximumHitPoints = player.maximumHitPoints;
    parameters.progression = resetLevel
        ? PlayerSpawnProgression::Reset
        : PlayerSpawnProgression::Preserve;
    parameters.creationDuration = 1.0f;
    // AliveState's firing clock starts at zero and does not advance during
    // the one-second creation animation.
    parameters.initialFireCooldown = kPlayerReloadTime;
    // CreatingState in the 2D version grants the same ten-second shield on
    // initial creation, stage entry, and every respawn.
    parameters.shieldDuration = 10.0f;

    const PlayerSpawnOutcome outcome =
        preparePlayerSpawnState(state, parameters);
    assert(outcome == PlayerSpawnOutcome::Prepared &&
           "valid player spawn state was rejected");
    if (outcome != PlayerSpawnOutcome::Prepared)
        return;

    player.position = state.movement.position;
    player.yaw = state.movement.yaw;
    player.driveDirection = state.movement.driveDirection;
    player.movementDirection = state.movement.movementDirection;
    player.level = state.level;
    player.directKillStreak = state.directKillStreak;
    player.streakPopupTimer = state.clocks.streakPopupTimer;
    player.hitPoints = state.hitPoints;
    player.active = state.active;
    player.moving = state.movement.moving;
    player.hasBoat = state.hasBoat;
    player.creationTimer = state.clocks.creationTimer;
    player.respawnTimer = state.respawnTimer;
    player.deathTimer = state.deathTimer;
    player.fireCooldown = state.clocks.fireCooldown;
    player.dustCooldown = state.clocks.dustCooldown;
    player.iceSlipTimer = state.movement.iceSlipTimer;
    player.onIce = state.movement.onIce;
    player.shieldTimer = state.clocks.shieldTimer;
}

class RandomSource
{
public:
    virtual ~RandomSource() = default;
    virtual float draw(
        std::uniform_real_distribution<float> &distribution) = 0;
    virtual int draw(
        std::uniform_int_distribution<int> &distribution) = 0;
};

class Mt19937RandomSource final : public RandomSource
{
public:
    explicit Mt19937RandomSource(std::uint32_t seed)
        : engine_(seed)
    {
    }

    float draw(std::uniform_real_distribution<float> &distribution) override
    {
        // Float rounding can produce the excluded upper endpoint on libc++.
        // Correct that value without drawing again or shifting the shared RNG.
        return std::min(distribution(engine_),
                        std::nextafter(distribution.b(), distribution.a()));
    }

    int draw(std::uniform_int_distribution<int> &distribution) override
    {
        return distribution(engine_);
    }

    void appendState(std::ostream &output) const
    {
        output << engine_;
    }

private:
    std::mt19937 engine_;
};

using StageLoadOperation = bool (*)(StageMap &, const fs::path &, int,
                                    std::string &);

inline bool loadGeneratedStageMap(StageMap &map, const fs::path &resourceRoot,
                           int stage, std::string &error)
{
    return map.load(resourceRoot, stage, error);
}

// Optional presentation boundary: simulation owns no engine objects. The legacy
// frontend consumes these callbacks at their original transaction phase; other
// frontends can render the corresponding semantic game events independently.
class SessionPresentation
{
public:
    virtual ~SessionPresentation() = default;
    virtual void updateEffects(float dt) = 0;
    virtual void clearEffects() = 0;
    virtual void trackDust(Float3 position, Float3 velocity) = 0;
    virtual void playerMuzzle(const Player &, const PlayerShellLaunchIntent &) = 0;
    virtual void enemyMuzzle(Nation, const EnemyShellLaunchIntent &) = 0;
    virtual void impact(Float3 position, Float3 normal, bool heavy) = 0;
    virtual void brickImpact(Float3 position, Float3 normal, bool power, bool destroyed) = 0;
    virtual void explosion(Float3 position, Rgba8 color) = 0;
    virtual float cameraAspectRatio() const = 0;
};

struct SessionCamera
{
    Float3 position{};
    Float3 target{};
    Float3 up{0.0f, 1.0f, 0.0f};
    float fovy = 0.0f;
};

class GameSession : private CommandSideEffectSink
{
public:
    explicit GameSession(fs::path resourceRoot, AudioOutput *audio = nullptr)
        : GameSession(std::move(resourceRoot),
                 static_cast<std::uint32_t>(std::random_device{}()), audio)
    {
    }

    GameSession(fs::path resourceRoot, std::uint32_t randomSeed,
           AudioOutput *audio = nullptr)
        : resourceRoot_(std::move(resourceRoot)), audio_(audio),
          randomSeed_(randomSeed), random_(randomSeed)
    {
    }

    bool start(int playerCount, int startingLives, int requestedStage,
               const std::array<Nation, 2> &playerNations,
               AdvancedGameSettings advancedSettings = {},
               int cameraYawDegrees = 0,
               int cameraElevationDegrees =
                   kDefaultCameraElevationDegrees)
    {
        // A rejected candidate must not combine the previous world with a new
        // session configuration or a fresh, unprepared player vector.
        const std::vector<GameEvent> previousEvents = eventsThisUpdate_;
        const int previousPlayerCount = playerCount_;
        const int previousStartingLives = startingLives_;
        const AdvancedGameSettings previousAdvancedSettings = advancedSettings_;
        const int previousCameraYawDegrees = cameraYawDegrees_;
        const int previousCameraElevationDegrees = cameraElevationDegrees_;
        const std::array<Nation, 2> previousStartingNations = startingNations_;
        const int previousStage = stage_;
        const std::vector<Player> previousPlayers = players_;

        eventsThisUpdate_.clear();
        playerCount_ = std::clamp(playerCount, 1, 2);
        startingLives_ = std::clamp(startingLives, 1, 99);
        advancedSettings_ = normalizedAdvancedSettings(advancedSettings);
        cameraYawDegrees_ =
            normalizedCameraYawDegrees(cameraYawDegrees);
        cameraElevationDegrees_ =
            normalizedCameraElevationDegrees(cameraElevationDegrees);
        for (std::size_t index = 0; index < startingNations_.size(); ++index)
            startingNations_[index] = normalizedNation(playerNations[index]);
        stage_ = normalizedStage(requestedStage);
        players_.clear();
        players_.reserve(playerCount_);
        for (int id = 0; id < playerCount_; ++id)
        {
            Player player;
            player.id = id;
            player.nation = startingNations_[static_cast<std::size_t>(id)];
            player.lives = startingLives_;
            player.maximumHitPoints =
                advancedSettings_.playerMaximumHitPoints;
            player.hitPoints = player.maximumHitPoints;
            players_.push_back(player);
        }
        if (loadStage(true))
            return true;

        eventsThisUpdate_ = previousEvents;
        playerCount_ = previousPlayerCount;
        startingLives_ = previousStartingLives;
        advancedSettings_ = previousAdvancedSettings;
        cameraYawDegrees_ = previousCameraYawDegrees;
        cameraElevationDegrees_ = previousCameraElevationDegrees;
        startingNations_ = previousStartingNations;
        stage_ = previousStage;
        players_ = previousPlayers;
        return false;
    }

    bool restart()
    {
        return start(playerCount_, startingLives_, stage_, startingNations_,
                     advancedSettings_, cameraYawDegrees_,
                     cameraElevationDegrees_);
    }

    bool changeStage(int difference)
    {
        eventsThisUpdate_.clear();
        const int previousStage = stage_;
        const std::vector<Player> previousPlayers = players_;
        stage_ = normalizedStage(stage_ + difference % kStageCount);
        for (Player &player : players_)
        {
            if (player.lives <= 0)
            {
                player.lives = std::min(2, startingLives_);
                player.level = 0;
            }
        }
        if (loadStage(false))
            return true;
        stage_ = previousStage;
        players_ = previousPlayers;
        return false;
    }

    void togglePause()
    {
        if (settling() || stageIntro())
            return;
        paused_ = !paused_;
        if (audio_ != nullptr && paused_)
        {
            // PauseState in the 2D game silences every active voice before
            // playing its one pause cue.  Resuming does not play it again.
            audio_->stopAll();
            play(AudioCue::Pause);
        }
    }
    void toggleTargets() { showTargets_ = !showTargets_; }
    void confirmSettlement()
    {
        if (highScoreDisplay_)
        {
            highScoreDisplay_ = false;
            highScoreDisplayTimer_ = 0.0f;
            requestReturnToMenu();
        }
        else
            finishSettlement(settlement_.confirm());
    }
    bool consumeMenuRequest()
    {
        const bool requested = returnToMenuRequested_;
        returnToMenuRequested_ = false;
        return requested;
    }
    bool paused() const { return paused_; }
    bool gameOver() const { return gameOver_; }
    bool stageIntro() const { return stageIntroTimer_ > 0.0f; }
    float stageIntroTimeRemaining() const { return stageIntroTimer_; }
    bool stageTransition() const { return stageTransitionTimer_ > 0.0f; }
    bool settling() const { return settlement_.active(); }
    bool highScoreDisplay() const { return highScoreDisplay_; }
    bool endingSequence() const
    {
        return stageIntro() || gameOver_ || stageTransitionTimer_ > 0.0f ||
               settling() || highScoreDisplay_ || awaitingMenu_;
    }
    bool settlementCounting() const
    {
        return settlement_.counting();
    }
    bool settlementWasGameOver() const { return settlement_.gameOver(); }
    int settlementStage() const { return settlement_.stage(); }
    int settlementScoreCounter() const { return settlement_.scoreCounter(); }
    const StageTally &settlementTally(int playerIndex) const
    {
        return settlement_.tally(playerIndex);
    }
    int settlementDisplayedKills(int playerIndex, int enemyType) const
    {
        return settlement_.displayedKills(playerIndex, enemyType);
    }
    int settlementHighScore() const
    {
        return highScore_;
    }
    bool baseAlive() const { return baseAlive_; }
    Nation baseNation() const { return map_.governmentNation(); }
    Nation enemyNation(int enemyId) const
    {
        return tanks3d::core::opposingNationForPlayers(
            startingNations_, playerCount_, enemyId, randomSeed_, stage_);
    }
    bool showTargets() const { return showTargets_; }
    int stage() const { return stage_; }
    int playerCount() const { return playerCount_; }
    const AdvancedGameSettings &advancedSettings() const
    {
        return advancedSettings_;
    }
    int cameraYawDegrees() const { return cameraYawDegrees_; }
    void setCameraYawDegrees(int requestedDegrees)
    {
        const int normalized =
            normalizedCameraYawDegrees(requestedDegrees);
        if (cameraYawDegrees_ == normalized)
            return;
        cameraYawDegrees_ = normalized;
        resetCameras();
    }
    int cameraElevationDegrees() const
    {
        return cameraElevationDegrees_;
    }
    void setCameraElevationDegrees(int requestedDegrees)
    {
        const int normalized =
            normalizedCameraElevationDegrees(requestedDegrees);
        if (cameraElevationDegrees_ == normalized)
            return;
        cameraElevationDegrees_ = normalized;
        resetCameras();
    }
    int enemiesLeft() const
    {
        return enemySpawnState_.remaining +
               static_cast<int>(enemies_.size());
    }
    const std::string &lastError() const { return lastError_; }
    const StageMap &map() const { return map_; }
    const std::vector<Player> &players() const { return players_; }
    const std::vector<Enemy> &enemies() const { return enemies_; }
    const std::vector<Shell> &shells() const { return shells_; }
    const std::vector<Pickup> &bonuses() const { return bonuses_; }
    const std::array<CameraRig, 2> &cameraRigs() const { return cameraRigs_; }
    const std::string &bonusMessage() const { return bonusMessage_; }
    float bonusMessageTimer() const { return bonusMessageTimer_; }
    std::uint32_t randomSeed() const { return randomSeed_; }
    const std::vector<GameEvent> &eventsThisUpdate() const
    {
        return eventsThisUpdate_;
    }

    SessionDigest sessionDigest() const
    {
        std::ostringstream output;
        output.imbue(std::locale::classic());
        output << "Tanks3D-session-v1|";
        const auto integer = [&](long long value) {
            output << value << ',';
        };
        const auto real = [&](float value) {
            output << std::hexfloat << value << ',';
        };

        integer(stage_);
        integer(playerCount_);
        integer(startingLives_);
        integer(static_cast<int>(startingNations_[0]));
        integer(static_cast<int>(startingNations_[1]));
        integer(advancedSettings_.playerMaximumHitPoints);
        integer(advancedSettings_.enemySpeedPercent);
        integer(advancedSettings_.enemyFireRatePercent);
        integer(advancedSettings_.enemySpawnRatePercent);
        integer(enemySpawnState_.remaining);
        integer(enemySpawnState_.nextSpawnIndex);
        integer(enemySpawnState_.nextEnemyId);
        real(enemySpawnState_.timer);
        real(stageIntroTimer_);
        real(stageTransitionTimer_);
        real(gameOverReportTimer_);
        integer(baseAlive_);
        integer(paused_);
        integer(gameOver_);
        integer(static_cast<int>(settlement_.phase()));
        integer(settlement_.gameOver());
        integer(settlement_.stage());
        integer(settlement_.scoreCounter());
        integer(settlement_.maximumScore());
        integer(settlement_.categoryIndex());
        real(settlement_.countTimer());
        real(settlement_.idleTimer());
        for (std::size_t playerIndex = 0; playerIndex < 2; ++playerIndex)
        {
            const StageTally &tally = settlement_.tally(
                static_cast<int>(playerIndex));
            for (int value : tally.destroyed)
                integer(value);
            for (int value : tally.enemyPoints)
                integer(value);
            integer(tally.bonusPoints);
            integer(tally.scoreAtStageStart);
            for (int enemyType = 0; enemyType < kEnemyTypeCount; ++enemyType)
                integer(settlement_.displayedKills(
                    static_cast<int>(playerIndex), enemyType));
        }
        integer(highScore_);
        integer(highScoreDisplay_);
        real(highScoreDisplayTimer_);
        integer(returnToMenuRequested_);
        integer(awaitingMenu_);
        integer(enemyCreationShowcase_);

        integer(static_cast<int>(map_.governmentNation()));
        real(map_.governmentSteelTimeRemaining());
        for (int index = 0; index < kGovernmentWallCount; ++index)
            integer(map_.governmentWallHealth(index));
        for (int row = 0; row < kMapSize; ++row)
        {
            for (int column = 0; column < kMapSize; ++column)
            {
                integer(static_cast<unsigned char>(map_.tile(row, column)));
                integer(map_.brickMask(row, column));
                integer(map_.brickHitCount(row, column));
                integer(static_cast<int>(
                    map_.brickFirstDirection(row, column)));
            }
        }

        integer(static_cast<long long>(players_.size()));
        for (const Player &player : players_)
        {
            integer(player.id);
            integer(static_cast<int>(player.nation));
            real(player.position.x);
            real(player.position.z);
            real(player.yaw);
            integer(static_cast<int>(player.driveDirection));
            integer(static_cast<int>(player.movementDirection));
            integer(player.lives);
            integer(player.maximumHitPoints);
            integer(player.hitPoints);
            integer(player.level);
            integer(player.active);
            integer(player.moving);
            integer(player.hasBoat);
            real(player.shieldTimer);
            real(player.creationTimer);
            real(player.respawnTimer);
            real(player.deathTimer);
            real(player.fireCooldown);
            real(player.dustCooldown);
            real(player.iceSlipTimer);
            integer(player.onIce);
            integer(player.score);
            integer(player.directKillStreak);
            real(player.streakPopupTimer);
            for (int value : player.stageTally.destroyed)
                integer(value);
            for (int value : player.stageTally.enemyPoints)
                integer(value);
            integer(player.stageTally.bonusPoints);
            integer(player.stageTally.scoreAtStageStart);
        }

        integer(static_cast<long long>(enemies_.size()));
        for (const Enemy &enemy : enemies_)
        {
            integer(enemy.id);
            real(enemy.position.x);
            real(enemy.position.z);
            real(enemy.target.x);
            real(enemy.target.z);
            real(enemy.yaw);
            integer(static_cast<int>(enemy.driveDirection));
            integer(static_cast<int>(enemy.movementDirection));
            integer(enemy.type);
            integer(enemy.armor);
            integer(enemy.carriesBonus);
            integer(enemy.destroyed);
            integer(enemy.moving);
            real(enemy.frozenTimer);
            real(enemy.fireCooldown);
            real(enemy.creationTimer);
            real(enemy.deathTimer);
            real(enemy.blockedTimer);
            real(enemy.dustCooldown);
            real(enemy.directionTimer);
            real(enemy.directionDecisionInterval);
            real(enemy.movementDelay);
            real(enemy.iceSlipTimer);
            integer(enemy.onIce);
        }

        integer(static_cast<long long>(shells_.size()));
        for (const Shell &shell : shells_)
        {
            real(shell.position.x);
            real(shell.position.z);
            real(shell.velocity.x);
            real(shell.velocity.z);
            integer(static_cast<int>(shell.owner));
            integer(shell.ownerIndex);
            integer(shell.power);
            integer(shell.impacting);
            real(shell.life);
        }

        integer(static_cast<long long>(bonuses_.size()));
        for (const Pickup &bonus : bonuses_)
        {
            integer(static_cast<int>(bonus.type));
            real(bonus.position.x);
            // Preserve the v1 transcript field order after replacing the
            // simulation-only Vector3 with an XZ position.
            real(0.0f);
            real(bonus.position.z);
            real(bonus.age);
            real(bonus.life);
        }
        output << "rng:";
        random_.appendState(output);
        return {output.str()};
    }

    void spawnBonusShowcase()
    {
        stageIntroTimer_ = 0.0f;
        bonuses_.clear();
        int typeIndex = 0;
        for (int row = kMapSize - 4; row >= 1 &&
                                          typeIndex < static_cast<int>(BonusType::Count);
             --row)
        {
            for (int column = 1; column < kMapSize - 1 &&
                                 typeIndex < static_cast<int>(BonusType::Count);
                 ++column)
            {
                const XZ candidate{column + 0.5f, row + 0.5f};
                if (map_.isInsideBase(candidate) ||
                    map_.collidesWithTank(candidate, 0.42f))
                    continue;
                const bool tooClose = std::any_of(
                    bonuses_.begin(), bonuses_.end(), [&](const Pickup &pickup) {
                        return distanceSquared(candidate, pickup.position) < 2.25f;
                    });
                if (tooClose)
                    continue;
                Pickup pickup;
                pickup.type = static_cast<BonusType>(typeIndex++);
                pickup.position = candidate;
                bonuses_.push_back(pickup);
            }
        }
    }

    void spawnTankShowcase()
    {
        stageIntroTimer_ = 0.0f;
        map_.prepareShowcaseArena();
        enemies_.clear();
        shells_.clear();
        bonuses_.clear();
        if (!players_.empty())
        {
            players_[0].position = {13.0f, 17.0f};
            players_[0].creationTimer = 0.0f;
            players_[0].shieldTimer = 0.0f;
            players_[0].driveDirection = CardinalDirection::South;
            players_[0].yaw = cardinalYaw(CardinalDirection::South);
            players_[0].moving = true;
        }
        static constexpr std::array<XZ, 4> positions{{
            {8.5f, 12.5f}, {11.5f, 12.5f}, {14.5f, 12.5f}, {17.5f, 12.5f}}};
        static constexpr std::array<CardinalDirection, 4> directions{{
            CardinalDirection::North, CardinalDirection::East,
            CardinalDirection::West, CardinalDirection::South}};
        for (int type = 0; type < 4; ++type)
        {
            Enemy enemy;
            enemy.id = type;
            enemy.type = type;
            enemy.armor = 1;
            enemy.position = positions[static_cast<std::size_t>(type)];
            enemy.driveDirection = directions[static_cast<std::size_t>(type)];
            enemy.yaw = cardinalYaw(enemy.driveDirection);
            enemy.moving = true;
            enemies_.push_back(enemy);
        }
        enemySpawnState_.remaining = 0;
        resetCameras();
    }

    void spawnSettlementShowcase()
    {
        stageIntroTimer_ = 0.0f;
        for (std::size_t index = 0; index < players_.size(); ++index)
        {
            players_[index].score = 2350 + static_cast<int>(index) * 850;
            players_[index].level = index == 0U ? 3 : 2;
            players_[index].lives = std::max(1, players_[index].lives -
                                                   static_cast<int>(index));
            StageTally &tally = players_[index].stageTally;
            if (index == 0U)
            {
                tally.destroyed = {{5, 4, 3, 2}};
                tally.enemyPoints = {{250, 200, 150, 100}};
                tally.bonusPoints = 600;
            }
            else
            {
                tally.destroyed = {{2, 1, 2, 1}};
                tally.enemyPoints = {{100, 50, 100, 50}};
                tally.bonusPoints = 300;
            }
            tally.scoreAtStageStart = players_[index].score -
                                      tally.stagePoints();
        }
        // Showcase setup is not a simulation update and must not leave a
        // synthetic event in the per-update observation buffer.
        beginSettlement(false, false);
    }

    void spawnBaseDamageShowcase()
    {
        stageIntroTimer_ = 0.0f;
        // Developer-only visual QA: one cracked section, one breached section
        // and one lightly damaged section in the normal stage context.
        map_.repairGovernmentWalls();
        for (int hit = 0; hit < 3; ++hit)
            map_.impactShell(governmentWallSegment(0).center, false,
                             CardinalDirection::North);
        for (int hit = 0; hit < 4; ++hit)
            map_.impactShell(governmentWallSegment(1).center, false,
                             CardinalDirection::North);
        for (int hit = 0; hit < 2; ++hit)
            map_.impactShell(governmentWallSegment(2).center, false,
                             CardinalDirection::North);
    }

    void spawnBaseSteelShowcase()
    {
        stageIntroTimer_ = 0.0f;
        // Developer-only visual QA for the shovel's fortified material.
        map_.activateGovernmentSteel();
    }

    void spawnEnemyCreationShowcase()
    {
        stageIntroTimer_ = 0.0f;
        map_.prepareShowcaseArena();
        enemies_.clear();
        shells_.clear();
        bonuses_.clear();
        if (!players_.empty())
        {
            players_[0].position = {13.0f, 17.0f};
            players_[0].creationTimer = 0.0f;
            players_[0].shieldTimer = 0.0f;
        }
        static constexpr std::array<XZ, 3> positions{{
            {10.0f, 13.0f}, {13.0f, 13.0f}, {16.0f, 13.0f}}};
        static constexpr std::array<float, 3> timers{{0.95f, 0.65f, 0.35f}};
        for (int index = 0; index < 3; ++index)
        {
            Enemy enemy;
            enemy.id = index;
            enemy.position = positions[static_cast<std::size_t>(index)];
            enemy.target = kGovernmentBaseCenter;
            enemy.type = index;
            enemy.armor = index + 1;
            enemy.creationTimer = timers[static_cast<std::size_t>(index)];
            enemies_.push_back(enemy);
        }
        enemySpawnState_.remaining = 0;
        enemyCreationShowcase_ = true;
        resetCameras();
    }

    void spawnForestCoverShowcase()
    {
        if (players_.empty())
            return;
        stageIntroTimer_ = 0.0f;
        XZ selected = players_[0].position;
        float bestDistance = std::numeric_limits<float>::max();
        const XZ spawn = playerSpawn(0);
        for (int row = 0; row < kMapSize; ++row)
        {
            for (int column = 0; column < kMapSize; ++column)
            {
                if (map_.tile(row, column) != '%')
                    continue;
                const XZ candidate{column + 0.5f, row + 0.5f};
                const float candidateDistance = distanceSquared(candidate,
                                                                  spawn);
                if (candidateDistance < bestDistance)
                {
                    selected = candidate;
                    bestDistance = candidateDistance;
                }
            }
        }
        players_[0].position = selected;
        players_[0].creationTimer = 0.0f;
        players_[0].shieldTimer = 0.0f;
        players_[0].moving = false;
        enemies_.clear();
        shells_.clear();
        bonuses_.clear();
        enemySpawnState_.remaining = 1;
        enemySpawnState_.timer = 10000.0f;
        resetCameras();
    }

    void update(float dt, const PlayerInputFrame &inputFrame)
    {
        eventsThisUpdate_.clear();
        dt = std::clamp(dt, 0.0f, 1.0f / 20.0f);
        if (awaitingMenu_)
        {
            if (audio_ != nullptr)
                audio_->updateEngine(false, false);
            return;
        }
        if (settling())
        {
            updateSettlement(dt);
            if (audio_ != nullptr)
                audio_->updateEngine(false, false);
            return;
        }
        if (highScoreDisplay_)
        {
            highScoreDisplayTimer_ += dt;
            if (highScoreDisplayTimer_ >= kHighScoreDisplayDuration)
            {
                highScoreDisplay_ = false;
                highScoreDisplayTimer_ = 0.0f;
                requestReturnToMenu();
            }
            if (audio_ != nullptr)
                audio_->updateEngine(false, false);
            return;
        }
        bonusMessageTimer_ = std::max(0.0f, bonusMessageTimer_ - dt);
        if (!paused_ && presentation_ != nullptr)
            presentation_->updateEffects(dt);
        for (float &shake : cameraShake_)
            shake = std::max(0.0f, shake - dt * 2.8f);
        if (paused_)
        {
            if (audio_ != nullptr)
                audio_->updateEngine(false, false);
            updateCameras(dt);
            return;
        }
        if (stageIntroTimer_ > 0.0f)
        {
            // StartingState in the 2D edition owns 3.2 seconds: no player,
            // enemy, projectile, pickup, protection, or engine clock advances
            // while the stage jingle and title are presented.
            stageIntroTimer_ = std::max(0.0f, stageIntroTimer_ - dt);
            if (audio_ != nullptr)
                audio_->updateEngine(false, false);
            updateCameras(dt);
            return;
        }
        if (gameOver_)
        {
            gameOverReportTimer_ += dt;
            if (gameOverReportTimer_ >= kGameOverReportDelay)
                beginSettlement(true);
            if (audio_ != nullptr)
                audio_->updateEngine(false, false);
            updateCameras(dt);
            return;
        }

        // Shovel protection follows live battle time. Pausing, the settlement
        // report, and the game-over hold all freeze the twenty-second clock.
        map_.updateGovernmentProtection(dt);

        if (stageTransitionTimer_ > 0.0f)
        {
            // The original keeps the scene live for five seconds after the
            // last enemy is gone. Players can still collect the final bonus,
            // while surviving rounds can still hit a tank or the eagle.
            updatePlayers(dt, inputFrame);
            updateShells(dt);
            updateBonuses(dt);

            if (!baseAlive_ || allPlayersDefeated())
            {
                play(AudioCue::GameOver);
                gameOver_ = true;
                gameOverReportTimer_ = 0.0f;
                stageTransitionTimer_ = 0.0f;
                if (audio_ != nullptr)
                    audio_->updateEngine(false, false);
                updateCameras(dt);
                return;
            }

            stageTransitionTimer_ -= dt;
            if (stageTransitionTimer_ <= 0.0f)
            {
                stageTransitionTimer_ = 0.0f;
                beginSettlement(false);
            }
            else if (audio_ != nullptr)
            {
                syncEngineAudio(true);
            }
            updateCameras(dt);
            return;
        }

        updatePlayers(dt, inputFrame);
        updateEnemies(dt, random_);
        updateShells(dt);
        updateBonuses(dt);
        spawnEnemyIfNeeded(dt, random_);

        if (!baseAlive_ || allPlayersDefeated())
        {
            if (!gameOver_)
            {
                play(AudioCue::GameOver);
                gameOverReportTimer_ = 0.0f;
            }
            gameOver_ = true;
        }
        else if (enemySpawnState_.remaining == 0 && enemies_.empty())
            stageTransitionTimer_ = kStageEndDelay;

        syncEngineAudio(!gameOver_);

        updateCameras(dt);
    }

    SessionCamera cameraForPlayer(int index, double timeSeconds = 0.0) const
    {
        const CameraRig &rig = cameraRigs_[std::clamp(index, 0, 1)];
        SessionCamera camera{};
        camera.position = rig.position;
        camera.target = rig.target;
        const float shake = cameraShake_[static_cast<std::size_t>(std::clamp(index, 0, 1))];
        if (shake > 0.0f)
        {
            const float phase = static_cast<float>(timeSeconds) * 57.0f + index * 2.1f;
            const CameraPlanarBasis basis =
                cameraPlanarBasis(cameraYawDegrees_);
            // Translate position and target together along screen-right. A
            // planar depth pulse stays on the selected azimuth, so shake never
            // introduces a temporary yaw change.
            const float horizontalShake = std::sin(phase) * shake;
            camera.position.x += horizontalShake * basis.rightX;
            camera.position.z += horizontalShake * basis.rightZ;
            camera.target.x += horizontalShake * basis.rightX;
            camera.target.z += horizontalShake * basis.rightZ;
            camera.position.y += std::cos(phase * 1.37f) * shake * 0.55f;
            const float depthShake =
                std::sin(phase * 0.73f) * shake * 0.40f;
            camera.position.x += depthShake * basis.offsetX;
            camera.position.z += depthShake * basis.offsetZ;
            camera.target.y += std::cos(phase * 1.11f) * shake * 0.22f;
        }
        camera.up = {0.0f, 1.0f, 0.0f};
        camera.fovy = cameraFovy_;
        return camera;
    }

protected:
    friend struct SessionTestAccess;

    void observeSettlementBegin(
        SettlementBeginPresentationStep step,
        const SettlementBeginPlan &plan)
    {
        if (settlementBeginPresentationObserver_)
            settlementBeginPresentationObserver_(step, plan);
    }

    void observeSettlementTransition(
        SettlementTransitionPresentationStep step,
        const SettlementTransitionPlan &plan)
    {
        if (settlementTransitionPresentationObserver_)
            settlementTransitionPresentationObserver_(step, plan);
    }

    void requestReturnToMenu(bool preserveError = false)
    {
        if (!preserveError)
            lastError_.clear();
        awaitingMenu_ = true;
        returnToMenuRequested_ = true;
    }

    void beginSettlement(bool gameOver, bool recordEvent = true)
    {
        const SettlementBeginPlan plan = planSettlementBegin(
            stage_, kStageCount, playerCount_, gameOver, baseAlive_,
            recordEvent, players_);
        const bool planValid = plan.kind != SettlementBeginKind::Invalid;
        assert(planValid && "invalid settlement begin plan");
        if (!planValid)
            return;
        observeSettlementBegin(
            SettlementBeginPresentationStep::PlanReady, plan);

        settlement_.begin(plan.start);
        observeSettlementBegin(
            SettlementBeginPresentationStep::ReportCommitted, plan);
        if (plan.emitStageEnded)
        {
            GameEvent event;
            event.type = GameEventType::StageEnded;
            event.stage = plan.start.stage;
            switch (plan.kind)
            {
            case SettlementBeginKind::Cleared:
                event.stageEndReason = StageEndReason::Cleared;
                break;
            case SettlementBeginKind::BaseDestroyed:
                event.stageEndReason = StageEndReason::BaseDestroyed;
                break;
            case SettlementBeginKind::PlayersDefeated:
                event.stageEndReason = StageEndReason::PlayersDefeated;
                break;
            case SettlementBeginKind::Invalid:
                assert(false && "invalid settlement begin reason");
                return;
            }
            eventsThisUpdate_.push_back(event);
            observeSettlementBegin(
                SettlementBeginPresentationStep::StageEndEventAppended,
                plan);
        }
        if (audio_ != nullptr)
            audio_->stopAll();
        observeSettlementBegin(
            SettlementBeginPresentationStep::AudioStopBoundaryPassed,
            plan);
    }

    void updateSettlement(float dt)
    {
        const SettlementUpdate result = settlement_.update(dt);
        for (int step = 0; step < result.countedSteps; ++step)
            play(AudioCue::ScoreCounted);
        finishSettlement(result.completion);
    }

    void finishSettlement(SettlementCompletion completion)
    {
        if (completion == SettlementCompletion::None)
            return;
        const SettlementTransitionPlan plan = planSettlementTransition(
            completion, stage_, kStageCount, highScore_, players_);
        const bool planValid = plan.kind != SettlementTransitionKind::Invalid;
        assert(planValid && "invalid settlement completion plan");
        if (!planValid)
            return;
        observeSettlementTransition(
            SettlementTransitionPresentationStep::PlanReady, plan);

        if (plan.kind == SettlementTransitionKind::ShowHighScore ||
            plan.kind == SettlementTransitionKind::ReturnToMenu)
        {
            highScore_ = plan.highScoreAfter;
            observeSettlementTransition(
                SettlementTransitionPresentationStep::HighScoreCommitted,
                plan);
            if (plan.kind == SettlementTransitionKind::ShowHighScore)
            {
                play(AudioCue::HighScoreBeaten);
                observeSettlementTransition(
                    SettlementTransitionPresentationStep::
                        HighScoreAudioRequested,
                    plan);
                highScoreDisplay_ = true;
                highScoreDisplayTimer_ = 0.0f;
                observeSettlementTransition(
                    SettlementTransitionPresentationStep::
                        HighScoreDisplayCommitted,
                    plan);
            }
            else
            {
                requestReturnToMenu();
                observeSettlementTransition(
                    SettlementTransitionPresentationStep::MenuRequested,
                    plan);
            }
            return;
        }

        assert(plan.kind == SettlementTransitionKind::AdvanceStage);
        StageMap candidateMap;
        observeSettlementTransition(
            SettlementTransitionPresentationStep::StageCandidateRequested,
            plan);
        if (!prepareStageMap(plan.stageAfter, candidateMap))
        {
            observeSettlementTransition(
                SettlementTransitionPresentationStep::StageCandidateRejected,
                plan);
            requestReturnToMenu(true);
            observeSettlementTransition(
                SettlementTransitionPresentationStep::MenuRequested, plan);
            return;
        }
        observeSettlementTransition(
            SettlementTransitionPresentationStep::StageCandidatePrepared,
            plan);

        stage_ = plan.stageAfter;
        players_ = plan.playersAfter;
        observeSettlementTransition(
            SettlementTransitionPresentationStep::PlayerProgressionCommitted,
            plan);
        commitPreparedStage(std::move(candidateMap), false);
        observeSettlementTransition(
            SettlementTransitionPresentationStep::StageLoadCommitted, plan);
    }

    bool prepareStageMap(int requestedStage, StageMap &candidateMap)
    {
        candidateMap.setGovernmentNation(startingNations_[0]);
        if (!stageLoadOperation_(candidateMap, resourceRoot_, requestedStage,
                                lastError_))
            return false;
        if (candidateMap.stage() != requestedStage)
        {
            lastError_ = "Stage loader returned stage " +
                         std::to_string(candidateMap.stage()) +
                         " for requested stage " +
                         std::to_string(requestedStage);
            return false;
        }
        return true;
    }

    void commitPreparedStage(StageMap &&candidateMap,
                             bool resetPlayerLevels)
    {
        map_ = std::move(candidateMap);
        stage_ = map_.stage();
        enemies_.clear();
        shells_.clear();
        bonuses_.clear();
        if (presentation_ != nullptr)
            presentation_->clearEffects();
        cameraShake_ = {};
        enemySpawnState_.remaining = kEnemiesPerStage;
        enemySpawnState_.timer = intervalForEnemyRate(
            kEnemySpawnInterval, advancedSettings_.enemySpawnRatePercent);
        enemySpawnState_.nextSpawnIndex = 0;
        enemySpawnState_.nextEnemyId = 0;
        baseAlive_ = true;
        gameOver_ = false;
        paused_ = false;
        stageTransitionTimer_ = 0.0f;
        stageIntroTimer_ = kStageIntroDuration;
        gameOverReportTimer_ = 0.0f;
        settlement_.reset(stage_);
        highScoreDisplay_ = false;
        highScoreDisplayTimer_ = 0.0f;
        returnToMenuRequested_ = false;
        awaitingMenu_ = false;
        bonusMessage_.clear();
        bonusMessageTimer_ = 0.0f;
        enemyCreationShowcase_ = false;
        for (Player &player : players_)
        {
            preparePlayerSpawn(player, resetPlayerLevels);
            // Preserve a surviving life across stages, but never replay a
            // popup that was hidden behind the settlement report.
            player.streakPopupTimer = 0.0f;
            player.stageTally.reset(player.score);
        }
        resetCameras();
        play(AudioCue::StageStart);
    }

    bool loadStage(bool resetPlayerLevels)
    {
        StageMap candidateMap;
        if (!prepareStageMap(stage_, candidateMap))
            return false;
        commitPreparedStage(std::move(candidateMap), resetPlayerLevels);
        return true;
    }

    bool allPlayersDefeated() const
    {
        for (std::size_t index = 0; index < players_.size(); ++index)
        {
            const Player &player = players_[index];
            // DestroyedState keeps the final player object alive until every
            // projectile it owns has completed its own impact animation.
            if (player.lives > 0 || player.active ||
                activePlayerShells(static_cast<int>(index)) != 0)
            {
                return false;
            }
        }
        return true;
    }

    bool positionAvailable(XZ position, int ignoredPlayer, int ignoredEnemy,
                           bool allowWater = false,
                           bool reserveCreating = false) const
    {
        if (map_.collidesWithTank(position, kTankRadius, allowWater))
            return false;
        for (int i = 0; i < static_cast<int>(players_.size()); ++i)
        {
            if (i != ignoredPlayer && players_[i].active &&
                (reserveCreating || players_[i].creationTimer <= 0.0f) &&
                axisAlignedCentersOverlap(position, players_[i].position,
                                          kTankPairCollisionExtent))
                return false;
        }
        for (int i = 0; i < static_cast<int>(enemies_.size()); ++i)
        {
            if (i != ignoredEnemy && !enemies_[i].destroyed &&
                (reserveCreating || enemies_[i].creationTimer <= 0.0f) &&
                axisAlignedCentersOverlap(position, enemies_[i].position,
                                          kTankPairCollisionExtent))
                return false;
        }
        return true;
    }

    bool movementPositionAvailable(XZ origin, XZ candidate,
                                   int ignoredPlayer, int ignoredEnemy,
                                   bool allowWater = false) const
    {
        // Invalid existing overlaps must not trap both participants. Permit
        // only separation from every current blocker, while terrain and new
        // tank contacts retain the normal strict collision rule.
        if (map_.collidesWithTank(candidate, kTankRadius, allowWater))
            return false;
        for (int i = 0; i < static_cast<int>(players_.size()); ++i)
        {
            if (i != ignoredPlayer && players_[i].active &&
                players_[i].creationTimer <= 0.0f &&
                !core::axisAlignedMovementAvailable(
                    origin, candidate, players_[i].position,
                    kTankPairCollisionExtent))
                return false;
        }
        for (int i = 0; i < static_cast<int>(enemies_.size()); ++i)
        {
            if (i != ignoredEnemy && !enemies_[i].destroyed &&
                enemies_[i].creationTimer <= 0.0f &&
                !core::axisAlignedMovementAvailable(
                    origin, candidate, enemies_[i].position,
                    kTankPairCollisionExtent))
                return false;
        }
        return true;
    }

    void updatePlayers(float dt, const PlayerInputFrame &inputFrame)
    {
        for (int index = 0; index < static_cast<int>(players_.size()); ++index)
        {
            Player &player = players_[index];
            PlayerFrameClockState clocks;
            clocks.creationTimer = player.creationTimer;
            clocks.fireCooldown = player.fireCooldown;
            clocks.dustCooldown = player.dustCooldown;
            clocks.shieldTimer = player.shieldTimer;
            clocks.streakPopupTimer = player.streakPopupTimer;
            const auto frame = beginPlayerFrame(player.active, clocks, dt);
            assert(frame.valid && "valid Game3D player frame was rejected");
            if (!frame.valid)
                continue;

            player.creationTimer = frame.clocks.creationTimer;
            player.fireCooldown = frame.clocks.fireCooldown;
            player.dustCooldown = frame.clocks.dustCooldown;
            player.shieldTimer = frame.clocks.shieldTimer;
            player.streakPopupTimer = frame.clocks.streakPopupTimer;

            if (frame.phase == PlayerFramePhase::Inactive)
            {
                PlayerDeathState deathState;
                deathState.deathTimer = player.deathTimer;
                deathState.lives = player.lives;
                const PlayerDeathTransition deathTransition =
                    advanceInactivePlayerDeath(deathState, dt);
                assert(deathTransition != PlayerDeathTransition::Invalid &&
                       "valid inactive player death state was rejected");
                if (deathTransition == PlayerDeathTransition::Invalid)
                    continue;

                player.deathTimer = deathState.deathTimer;
                player.lives = deathState.lives;
                if (deathTransition == PlayerDeathTransition::Respawn)
                {
                    // Player::CreatingState discards every projectile owned
                    // by the old life. Final-life rounds remain alive instead
                    // because Game Over waits for them to finish.
                    shells_.erase(
                        std::remove_if(
                            shells_.begin(), shells_.end(),
                            [&](const Shell &shell) {
                                return shell.owner == ShellOwner::Player &&
                                       shell.ownerIndex == index;
                            }),
                        shells_.end());
                    preparePlayerSpawn(player, true);
                    GameEvent event;
                    event.type = GameEventType::PlayerRespawned;
                    event.position = player.position;
                    event.targetPlayerId = player.id;
                    event.valueAfter = player.lives;
                    eventsThisUpdate_.push_back(event);
                    play(AudioCue::PlayerRespawn);
                }
                continue;
            }

            if (frame.phase == PlayerFramePhase::Creating)
            {
                player.moving = false;
                // Warnings remain nonphysical. If a tank entered the spawn
                // footprint, finish creation only after it leaves; do not
                // repeat respawn side effects or spend the protection waiting.
                if (!positionAvailable(player.position, index, -1,
                                       player.hasBoat))
                {
                    if (player.creationTimer <= 0.0f)
                        player.creationTimer = kTankActivationRetryInterval;
                    player.shieldTimer = clocks.shieldTimer;
                }
                continue;
            }
            assert(frame.phase == PlayerFramePhase::Ready &&
                   "valid player frame returned an unknown phase");
            if (frame.phase != PlayerFramePhase::Ready)
                continue;

            const PlayerControlFrame &controls =
                inputFrame.players[static_cast<std::size_t>(index)];
            const PlayerControlPlan control =
                planPlayerControl(player.driveDirection, controls);
            if (!advancePlayerMovement(index, control, dt))
                continue;

            advancePlayerFire(index, control.fireHeld);
        }
    }

    bool advancePlayerMovement(int playerIndex,
                               const PlayerControlPlan &control, float dt)
    {
        Player &player = players_[static_cast<std::size_t>(playerIndex)];
        PlayerMovementState state;
        state.position = player.position;
        state.yaw = player.yaw;
        state.driveDirection = player.driveDirection;
        state.movementDirection = player.movementDirection;
        state.moving = player.moving;
        state.iceSlipTimer = player.iceSlipTimer;
        state.onIce = player.onIce;

        PlayerMovementParameters parameters;
        parameters.driveDirection = control.driveDirection;
        parameters.propelling = control.propelling;
        parameters.elapsed = dt;
        parameters.movementSpeed =
            playerLevelStats(player.level).movementSpeed;
        parameters.surfaceIsIce = map_.isIce(player.position);
        parameters.dustCooldown = player.dustCooldown;
        const bool allowWater = player.hasBoat;
        const auto movement = advanceActivePlayerMovement(
            state, parameters,
            [this, playerIndex, allowWater, &state](XZ candidate) {
                return movementPositionAvailable(
                    state.position, candidate, playerIndex, -1, allowWater);
            });
        assert(movement.valid &&
               "valid Game3D player movement was rejected");
        if (!movement.valid)
            return false;

        player.position = state.position;
        player.yaw = state.yaw;
        player.driveDirection = state.driveDirection;
        player.movementDirection = state.movementDirection;
        player.moving = state.moving;
        player.iceSlipTimer = state.iceSlipTimer;
        player.onIce = state.onIce;

        if (movement.dust.has_value())
        {
            if (presentation_ != nullptr)
                presentation_->trackDust(
                    {movement.dust->position.x, 0.10f,
                     movement.dust->position.z},
                    {movement.dust->velocity.x, 0.0f,
                     movement.dust->velocity.z});
            player.dustCooldown = kPlayerTrackDustCooldown;
        }
        return true;
    }

    int activePlayerShells(int playerIndex) const
    {
        return static_cast<int>(std::count_if(
            shells_.begin(), shells_.end(),
            [playerIndex](const Shell &shell) {
                return shell.owner == ShellOwner::Player &&
                       shell.ownerIndex == playerIndex;
            }));
    }

    void observePlayerShellLaunchPresentation(
        PlayerShellLaunchPresentationStep step,
        const PlayerShellLaunchIntent &intent)
    {
        if (playerShellLaunchPresentationObserver_)
            playerShellLaunchPresentationObserver_(step, intent);
    }

    void launchPlayerShell(const Player &player,
                           const PlayerShellLaunchIntent &intent)
    {
        Shell shell;
        shell.position = intent.position;
        shell.velocity = intent.velocity;
        shell.owner = ShellOwner::Player;
        shell.ownerIndex = intent.playerIndex;
        shell.power = intent.power;
        shells_.push_back(shell);
        observePlayerShellLaunchPresentation(
            PlayerShellLaunchPresentationStep::ShellInserted, intent);

        GameEvent event = shellEvent(GameEventType::ShellFired, shell,
                                     shell.position);
        event.direction = intent.direction;
        eventsThisUpdate_.push_back(event);
        observePlayerShellLaunchPresentation(
            PlayerShellLaunchPresentationStep::EventAppended, intent);

        if (presentation_ != nullptr)
            presentation_->playerMuzzle(player, intent);
        observePlayerShellLaunchPresentation(
            PlayerShellLaunchPresentationStep::MuzzleFlashSpawned, intent);

        cameraShake_[static_cast<std::size_t>(intent.playerIndex)] =
            std::max(
                cameraShake_[static_cast<std::size_t>(intent.playerIndex)],
                0.085f);
        observePlayerShellLaunchPresentation(
            PlayerShellLaunchPresentationStep::CameraShakeCommitted, intent);

        play(AudioCue::PlayerFired);
        observePlayerShellLaunchPresentation(
            PlayerShellLaunchPresentationStep::AudioRequested, intent);
    }

    PlayerFireOutcome advancePlayerFire(int playerIndex, bool requested)
    {
        Player &player = players_[static_cast<std::size_t>(playerIndex)];
        PlayerFireParameters parameters;
        parameters.requested = requested;
        parameters.activeShellCount = activePlayerShells(playerIndex);
        parameters.playerIndex = playerIndex;
        parameters.playerLevel = player.level;
        parameters.tankPosition = player.position;
        parameters.direction = player.driveDirection;
        parameters.reloadInterval = kPlayerReloadTime;
        parameters.shellSpawnDistance = kShellSpawnDistance;
        const PlayerFireOutcome outcome = advancePlayerFireTransaction(
            player.fireCooldown, parameters,
            [this, &player](const PlayerShellLaunchIntent &intent) {
                launchPlayerShell(player, intent);
            });
        assert(outcome != PlayerFireOutcome::Invalid &&
               "valid Game3D player fire was rejected");
        return outcome;
    }

    XZ chooseEnemyTarget(const Enemy &enemy) const
    {
        return tanks3d::game::chooseEnemyTarget(
            enemy.type, enemy.position, kGovernmentBaseCenter, players_);
    }

    void updateEnemies(float dt, RandomSource &random)
    {
        std::uniform_real_distribution<float> random01(0.0f, 1.0f);
        std::uniform_int_distribution<int> randomDirectionIndex(0, 3);
        EnemyFireConfiguration fireConfiguration;
        fireConfiguration.ratePercent =
            advancedSettings_.enemyFireRatePercent;
        fireConfiguration.shellSpawnDistance = kShellSpawnDistance;
        for (int index = 0; index < static_cast<int>(enemies_.size()); ++index)
        {
            Enemy &enemy = enemies_[index];
            const auto frame = beginEnemyFrame(
                enemy, dt, enemyCreationShowcase_);
            if (frame.valid && frame.phase == EnemyFramePhase::Creating &&
                enemy.creationTimer <= 0.0f &&
                !positionAvailable(enemy.position, -1, index))
            {
                enemy.creationTimer = kTankActivationRetryInterval;
            }
            if (!frame.valid || frame.phase != EnemyFramePhase::Active)
                continue;
            enemy.target = chooseEnemyTarget(enemy);

            const EnemySteeringOutcome steering =
                advanceActiveEnemySteering(
                    enemy, frame, dt,
                    [&](XZ candidate) {
                        return movementPositionAvailable(
                            enemy.position, candidate, -1, index);
                    },
                    EnemySteeringRandom{
                        [&]() { return random.draw(random01); },
                        [&]() {
                            return random.draw(randomDirectionIndex);
                        }},
                    !positionAvailable(enemy.position, -1, index));
            if (steering == EnemySteeringOutcome::Invalid)
                continue;

            const bool onIce = map_.isIce(enemy.position);
            const float movementSpeed = enemyMovementSpeedForType(
                enemy.type, advancedSettings_);
            const auto movement = advanceActiveEnemyMovement(
                enemy, frame, dt, movementSpeed, onIce,
                [&](XZ candidate) {
                    return movementPositionAvailable(
                        enemy.position, candidate, -1, index);
                });
            if (!movement.valid)
                continue;
            if (movement.dust)
            {
                if (presentation_ != nullptr)
                    presentation_->trackDust(
                        {movement.dust->position.x, 0.10f,
                         movement.dust->position.z},
                        {movement.dust->velocity.x, 0.0f,
                         movement.dust->velocity.z});
                enemy.dustCooldown = kEnemyTrackDustCooldown;
            }
            const bool blocked = movement.blocked;

            const EnemyFireOutcome fireOutcome =
                advanceEnemyFireTransaction(
                    enemy, blocked, fireConfiguration,
                    EnemyFireRandom{
                        [&]() { return random.draw(random01); }},
                    [this](int enemyId) {
                        return ownedEnemyShellPresent(enemyId);
                    },
                    [this](const EnemyShellLaunchIntent &intent) {
                        launchEnemyShell(intent);
                    });
            if (fireOutcome == EnemyFireOutcome::Invalid)
                assert(false && "valid Game3D enemy fire was rejected");
        }

        enemies_.erase(
            std::remove_if(enemies_.begin(), enemies_.end(),
                           [this](const Enemy &enemy) {
                               return enemyDestructionComplete(enemy,
                                                               shells_);
                           }),
            enemies_.end());
    }

    bool ownedEnemyShellPresent(int enemyId) const
    {
        return std::any_of(
            shells_.begin(), shells_.end(), [enemyId](const Shell &shell) {
                return shell.owner == ShellOwner::Enemy &&
                       shell.ownerIndex == enemyId;
            });
    }

    void observeEnemyShellLaunchPresentation(
        EnemyShellLaunchPresentationStep step,
        const EnemyShellLaunchIntent &intent)
    {
        if (enemyShellLaunchPresentationObserver_)
            enemyShellLaunchPresentationObserver_(step, intent);
    }

    void launchEnemyShell(const EnemyShellLaunchIntent &intent)
    {
        shells_.push_back(intent.shell);
        observeEnemyShellLaunchPresentation(
            EnemyShellLaunchPresentationStep::ShellInserted, intent);

        GameEvent event = shellEvent(GameEventType::ShellFired,
                                     intent.shell, intent.shell.position);
        event.direction = intent.direction;
        eventsThisUpdate_.push_back(event);
        observeEnemyShellLaunchPresentation(
            EnemyShellLaunchPresentationStep::EventAppended, intent);

        if (presentation_ != nullptr)
            presentation_->enemyMuzzle(
                enemyNation(intent.shell.ownerIndex), intent);
        observeEnemyShellLaunchPresentation(
            EnemyShellLaunchPresentationStep::MuzzleFlashSpawned, intent);
    }

    bool resolveFlyingShellImpact(Shell &shell, RandomSource &random)
    {
        const ShellPhysicalImpactResult physicalImpact =
            resolveShellPhysicalImpact(
                map_, baseAlive_, enemies_, players_, shell,
                [this, &random](const Enemy &carrier) {
                    releaseBonus(carrier, random);
                },
                [this, &shell](const Player &player) {
                    const SpawnTankArmorImpactAction action =
                        makePlayerTankArmorImpactAction(
                            shell.position, shell.velocity);
                    consumePlayerTankPreCommitFx(action, player, shell);
                });
        std::vector<GameEvent> physicalEvents =
            eventsForPhysicalShellImpact(physicalImpact);
        const CombatOutcome &outcome = physicalImpact.outcome;
        if (!physicalImpact.resolved)
            return false;
        if (outcome.mapStopsShell() ||
            outcome.target == CombatTarget::GovernmentCore)
        {
            const auto command = makeShellMapCorePresentationCommand(
                physicalImpact, std::move(physicalEvents));
            if (!command)
            {
                assert(false &&
                       "map/core result did not produce presentation");
                // Physical map/core state is already committed. In a release
                // build, fail closed so a malformed snapshot cannot apply the
                // same damage again on the next micro-step.
                beginShellImpact(shell, outcome.position);
                return true;
            }
            consumeShellMapCorePresentationCommand(*command, shell);
            return true;
        }

        if (outcome.target == CombatTarget::EnemyTank ||
            outcome.target == CombatTarget::PlayerTank)
        {
            Rgba8 playerExplosionColor{};
            if (outcome.target == CombatTarget::PlayerTank &&
                physicalImpact.playerCommit.hitResult ==
                    PlayerHitResult::Destroyed)
            {
                playerExplosionColor = playerIdentityColor(outcome.targetPlayerId);
            }
            ShellTankPresentationCommand command =
                makeShellTankPresentationCommand(
                    physicalImpact, std::move(physicalEvents),
                    playerExplosionColor);
            consumeShellTankPresentationCommand(command, physicalImpact,
                                                shell);
            return true;
        }
        return false;
    }

    void observeShellCancellationPresentation(
        ShellCancellationPresentationStep step,
        const ShellCancellationPresentationCommand &command)
    {
        if (shellCancellationPresentationObserver_)
            shellCancellationPresentationObserver_(step, command);
    }

    void observeShellMapCorePresentation(
        const ShellMapCorePresentationAction &action)
    {
        if (shellMapCorePresentationObserver_)
            shellMapCorePresentationObserver_(action);
    }

    void consumeShellMapCorePresentationAction(
        const ShellMapCorePresentationAction &action, Shell &shell)
    {
        const CommandSideEffectDisposition disposition =
            dispatchCommandSideEffect(action, *this);
        if (disposition ==
            CommandSideEffectDisposition::DomainCommitRequired)
        {
            const auto *impact =
                std::get_if<CommitMapCoreShellImpactAction>(&action);
            if (impact == nullptr)
            {
                assert(false &&
                       "map/core side-effect dispatcher requested an "
                       "unknown domain commit");
                return;
            }
            beginShellImpact(shell, impact->position);
        }
        else if (disposition != CommandSideEffectDisposition::Applied)
        {
            assert(false &&
                   "map/core presentation side effect was rejected");
            return;
        }
        observeShellMapCorePresentation(action);
    }

    void consumeShellMapCorePresentationCommand(
        const ShellMapCorePresentationCommand &command, Shell &shell)
    {
        if (command.actionCount > command.actions.size())
        {
            assert(false &&
                   "map/core presentation action count is out of range");
            // Commands are synchronously consumed at the contact point. Keep
            // release builds fail-closed even if an invalid command is ever
            // introduced by a future factory change.
            beginShellImpact(shell, shell.position);
            return;
        }
        for (std::size_t actionIndex = 0;
             actionIndex < command.actionCount; ++actionIndex)
        {
            consumeShellMapCorePresentationAction(
                command.actions[actionIndex], shell);
        }
    }

    void consumePlayerTankPreCommitFx(
        const SpawnTankArmorImpactAction &action,
        const Player &player, const Shell &shell)
    {
        spawnImpact(action.position, action.normal, action.heavy);
        if (playerTankPreCommitFxObserver_)
            playerTankPreCommitFxObserver_(action, player, shell);
    }

    void observeShellTankPresentation(
        const ShellTankPresentationAction &action,
        const ShellPhysicalImpactResult &physicalImpact,
        const Shell &shell)
    {
        if (shellTankPresentationObserver_)
            shellTankPresentationObserver_(action, physicalImpact, shell);
    }

    void consumeShellTankPresentationAction(
        const ShellTankPresentationAction &action,
        const ShellPhysicalImpactResult &physicalImpact,
        Shell &shell)
    {
        const CommandSideEffectDisposition disposition =
            dispatchCommandSideEffect(action, *this);
        if (disposition ==
            CommandSideEffectDisposition::DomainCommitRequired)
        {
            const auto *impact =
                std::get_if<CommitTankShellImpactAction>(&action);
            if (impact == nullptr)
            {
                assert(false &&
                       "tank side-effect dispatcher requested an unknown "
                       "domain commit");
                return;
            }
            beginShellImpact(shell, impact->position);
        }
        else if (disposition != CommandSideEffectDisposition::Applied)
        {
            assert(false && "tank presentation side effect was rejected");
            return;
        }
        observeShellTankPresentation(action, physicalImpact, shell);
    }

    void consumeShellTankPresentationCommand(
        const ShellTankPresentationCommand &command,
        const ShellPhysicalImpactResult &physicalImpact,
        Shell &shell)
    {
        if (command.actionCount > command.actions.size())
        {
            assert(false && "tank presentation action count is out of range");
            return;
        }
        for (std::size_t actionIndex = 0;
             actionIndex < command.actionCount; ++actionIndex)
        {
            consumeShellTankPresentationAction(
                command.actions[actionIndex], physicalImpact, shell);
        }
    }

    void consumeShellCancellationPresentationCommand(
        const ShellCancellationPresentationCommand &command)
    {
        if (dispatchCommandSideEffect(command.appendEvent, *this) !=
            CommandSideEffectDisposition::Applied)
        {
            assert(false && "cancellation event side effect was rejected");
            return;
        }
        observeShellCancellationPresentation(
            ShellCancellationPresentationStep::EventAppended, command);

        if (dispatchCommandSideEffect(command.spawnImpact, *this) !=
            CommandSideEffectDisposition::Applied)
        {
            assert(false && "cancellation impact side effect was rejected");
            return;
        }
        observeShellCancellationPresentation(
            ShellCancellationPresentationStep::ImpactFxSpawned, command);

        if (dispatchCommandSideEffect(command.requestAudio, *this) !=
            CommandSideEffectDisposition::Applied)
        {
            assert(false && "cancellation audio side effect was rejected");
            return;
        }
        observeShellCancellationPresentation(
            ShellCancellationPresentationStep::BulletHitAudioRequested,
            command);
    }

    void consumeShellCancellation(
        const ShellCancellationOutcome &cancellation)
    {
        const auto command = makeShellCancellationPresentationCommand(
            eventForShellCancellation(cancellation));
        if (!command)
        {
            assert(false && "cancellation event did not produce a command");
            return;
        }
        consumeShellCancellationPresentationCommand(*command);
    }

    void updateShells(float dt)
    {
        const ShellFrameSchedule schedule = prepareShellFrame(shells_, dt);
        std::vector<XZ> previousPositions(shells_.size());

        for (int substep = 0; substep < schedule.stepCount; ++substep)
        {
            advanceShellMicrostep(shells_, schedule.stepTime,
                                  previousPositions);

            // The original collision order is terrain/eagle, then tanks,
            // then opposing projectiles.  Resolve all physical hits before
            // testing cancellation within this same swept micro-step.
            for (Shell &shell : shells_)
                resolveFlyingShellImpact(shell, random_);

            // Use the same continuous collision helper exercised by the
            // standalone tests. Physical impacts above keep priority; each
            // surviving pair is swept from the beginning of this micro-step.
            const std::vector<ShellCancellationOutcome> cancellations =
                resolveShellCancellations(
                    shells_, previousPositions, schedule.stepTime, map_);
            for (const ShellCancellationOutcome &cancellation : cancellations)
            {
                consumeShellCancellation(cancellation);
            }
        }

        removeExpiredShells(shells_);
    }

    void releaseBonus(const Enemy &carrier, RandomSource &random)
    {
        // The 2D bonus flag remains on an armored carrier: every direct shell
        // hit releases another random pickup until that tank is destroyed.
        // The original samples the 32 px pickup's top-left corner at one-pixel
        // resolution (0..383). In tile units its center is 1..24.9375, and the
        // eagle is the only excluded overlap; terrain and tanks do not bias it.
        advanceBonusReleaseTransaction(
            players_, carrier.id,
            BonusReleaseRandom{
                [&](int slotCount) {
                    std::uniform_int_distribution<int> distribution(
                        0, slotCount - 1);
                    return random.draw(distribution);
                },
                [&]() {
                    std::uniform_int_distribution<int> distribution(0, 383);
                    return random.draw(distribution);
                }},
            [](XZ position) {
                return bonusOverlapsGovernmentBase(position);
            },
            [this](const BonusReleaseIntent &intent) {
                commitBonusRelease(intent);
            });
    }

    void observeBonusReleasePresentation(
        BonusReleasePresentationStep step,
        const BonusReleaseIntent &intent)
    {
        if (bonusReleasePresentationObserver_)
            bonusReleasePresentationObserver_(step, intent);
    }

    void commitBonusRelease(const BonusReleaseIntent &intent)
    {
        bonuses_.push_back(intent.pickup);
        observeBonusReleasePresentation(
            BonusReleasePresentationStep::PickupInserted, intent);

        GameEvent event;
        event.type = GameEventType::BonusSpawned;
        event.position = intent.pickup.position;
        event.sourceEnemyId = intent.sourceEnemyId;
        event.bonusType = intent.pickup.type;
        eventsThisUpdate_.push_back(event);
        observeBonusReleasePresentation(
            BonusReleasePresentationStep::SpawnEventAppended, intent);

        play(AudioCue::BonusAppeared);
        observeBonusReleasePresentation(
            BonusReleasePresentationStep::AudioRequested, intent);
    }

    void consumeBonusCommand(const BonusEffectCommand &command)
    {
        const CommandSideEffectDisposition disposition =
            dispatchCommandSideEffect(command, *this);
        if (disposition ==
            CommandSideEffectDisposition::DomainCommitRequired)
        {
            if (command.type !=
                BonusEffectCommandType::ActivateGovernmentSteel)
            {
                assert(false &&
                       "bonus side-effect dispatcher requested an unknown "
                       "domain commit");
                return;
            }
            // Match the classic shovel: restore every breached wing, then
            // make shared collision/render geometry invulnerable steel.
            map_.activateGovernmentSteel();
        }
    }

    void observeBonusCollectionPresentation(
        BonusCollectionPresentationStep step,
        const BonusCollectionIntent &intent,
        const BonusApplication *application = nullptr)
    {
        if (bonusCollectionPresentationObserver_)
            bonusCollectionPresentationObserver_(step, intent, application);
    }

    bool pendingBonusCollectionEventMatches(
        std::size_t eventIndex,
        const BonusCollectionIntent &intent) const
    {
        if (eventIndex >= eventsThisUpdate_.size())
            return false;
        const GameEvent &event = eventsThisUpdate_[eventIndex];
        return event.type == GameEventType::BonusCollected &&
               event.position.x == intent.pickup.position.x &&
               event.position.z == intent.pickup.position.z &&
               event.sourcePlayerId == intent.playerId &&
               event.bonusType == intent.pickup.type && event.points == 0;
    }

    void consumeBonusApplication(
        const BonusCollectionIntent &intent,
        const BonusApplication &application)
    {
        assert(application.applied &&
               application.type == intent.pickup.type &&
               application.playerIndex == intent.collectorIndex &&
               application.playerId == intent.playerId);
        observeBonusCollectionPresentation(
            BonusCollectionPresentationStep::ApplicationCommitted,
            intent, &application);

        for (const BonusEffectCommand &command : application.commands)
            consumeBonusCommand(command);
        observeBonusCollectionPresentation(
            BonusCollectionPresentationStep::CommandsConsumed,
            intent, &application);

        const Player &player =
            players_[static_cast<std::size_t>(application.playerIndex)];
        if (application.type == BonusType::Bandage)
        {
            bonusMessage_ = "P" + std::to_string(player.id + 1) +
                            "  BANDAGE  HP " +
                            std::to_string(player.hitPoints) + "/" +
                            std::to_string(player.maximumHitPoints) + "  +" +
                            std::to_string(application.scoreDelta);
        }
        else
        {
            bonusMessage_ = "P" + std::to_string(player.id + 1) + "  " +
                            bonusName(application.type) + "  +" +
                            std::to_string(application.scoreDelta);
        }
        bonusMessageTimer_ = 2.2f;
        observeBonusCollectionPresentation(
            BonusCollectionPresentationStep::MessageCommitted,
            intent, &application);
        play(application.type == BonusType::Tank
                 ? AudioCue::PlayerLifeUp
                 : AudioCue::BonusObtained);
        observeBonusCollectionPresentation(
            BonusCollectionPresentationStep::AudioRequested,
            intent, &application);
    }

    void updateBonuses(float dt)
    {
        std::size_t collectedEventIndex =
            std::numeric_limits<std::size_t>::max();
        (void)advanceBonusPickups(
            bonuses_, dt, players_, enemies_,
            BonusCollectionCallbacks{
                [this, &collectedEventIndex](
                    const BonusCollectionIntent &intent) {
                    collectedEventIndex = eventsThisUpdate_.size();
                    GameEvent event;
                    event.type = GameEventType::BonusCollected;
                    event.position = intent.pickup.position;
                    event.sourcePlayerId = intent.playerId;
                    event.bonusType = intent.pickup.type;
                    eventsThisUpdate_.push_back(event);
                    observeBonusCollectionPresentation(
                        BonusCollectionPresentationStep::
                            CollectionEventAppended,
                        intent);
                },
                [this, &collectedEventIndex](
                    const BonusCollectionIntent &intent,
                    const BonusApplication &application) {
                    const bool pendingEventMatches =
                        pendingBonusCollectionEventMatches(
                            collectedEventIndex, intent);
                    assert(pendingEventMatches &&
                           "pending bonus collection event changed");
                    if (!pendingEventMatches)
                        return;
                    if (!application.applied)
                    {
                        eventsThisUpdate_.erase(
                            eventsThisUpdate_.begin() +
                            static_cast<std::ptrdiff_t>(
                                collectedEventIndex));
                        return;
                    }

                    consumeBonusApplication(intent, application);
                    const bool patchTargetMatches =
                        pendingBonusCollectionEventMatches(
                            collectedEventIndex, intent);
                    assert(patchTargetMatches &&
                           "bonus commands changed the pending event");
                    if (!patchTargetMatches)
                        return;
                    eventsThisUpdate_[collectedEventIndex].points =
                        application.scoreDelta;
                    observeBonusCollectionPresentation(
                        BonusCollectionPresentationStep::
                            EventPointsCommitted,
                        intent, &application);
                }},
            [this](const BonusCollectionIntent &intent,
                   const BonusApplication &application) {
                observeBonusCollectionPresentation(
                    BonusCollectionPresentationStep::PickupRemoved,
                    intent, &application);
            });
    }

    void spawnEnemyIfNeeded(float dt, RandomSource &random)
    {
        EnemySpawnConfiguration configuration;
        configuration.stage = stage_;
        configuration.spawnPoints = kEnemySpawnPoints;
        configuration.target = kGovernmentBaseCenter;
        configuration.initialFireCooldown = intervalForEnemyRate(
            kEnemyInitialFireDelay,
            advancedSettings_.enemyFireRatePercent);
        configuration.normalInterval = intervalForEnemyRate(
            kEnemySpawnInterval,
            advancedSettings_.enemySpawnRatePercent);
        configuration.retryInterval = intervalForEnemyRate(
            kEnemySpawnRetryInterval,
            advancedSettings_.enemySpawnRatePercent);

        std::uniform_real_distribution<float> random01(0.0f, 1.0f);
        std::uniform_int_distribution<int> regularType(0, 2);
        const EnemySpawnOutcome outcome = advanceEnemySpawnTransaction(
            enemySpawnState_, dt, enemies_.size(), configuration,
            [&](XZ candidate) {
                // A warning reserves its spawn slot against another warning,
                // even though ordinary movement can still pass through it.
                return positionAvailable(candidate, -1, -1, false, true);
            },
            EnemySpawnRandom{
                [&]() { return random.draw(random01); },
                [&]() { return random.draw(regularType); }},
            [&](const Enemy &enemy) { enemies_.push_back(enemy); });
        if (outcome == EnemySpawnOutcome::Invalid)
            assert(false && "valid Game3D spawn state was rejected");
    }

    void resetCameras()
    {
        for (int index = 0; index < 2; ++index)
        {
            cameraRigs_[index].initialized = false;
        }
        cameraFovy_ = kSoloCameraSpan;
        updateCameras(1.0f);
    }

    void updateCameras(float dt)
    {
        XZ focus{};
        int trackedPlayers = 0;
        std::array<XZ, 2> tracked{};
        for (const Player &player : players_)
        {
            if (!player.active)
                continue;
            tracked[trackedPlayers++] = player.position;
            focus = focus + player.position;
        }
        if (trackedPlayers == 0)
        {
            tracked[0] = playerSpawn(0);
            focus = tracked[0];
            trackedPlayers = 1;
        }
        focus = focus * (1.0f / static_cast<float>(trackedPlayers));
        const CameraPlanarBasis cameraBasis =
            cameraPlanarBasis(cameraYawDegrees_);
        // Follow every movement instead of waiting for the player group to
        // reach a large screen-space dead zone. Solo play tracks the tank;
        // co-op tracks the midpoint and expands the view for separation.
        const float aspect = presentation_ != nullptr
                                 ? presentation_->cameraAspectRatio()
                                 : (1280.0f / 720.0f);

        float desiredFovy = kSoloCameraSpan;
        if (trackedPlayers > 1)
        {
            desiredFovy = gameplayCameraSpan(
                {tracked[1].x - tracked[0].x,
                 tracked[1].z - tracked[0].z},
                cameraYawDegrees_, cameraElevationDegrees_, aspect);
        }
        const float zoomRate = desiredFovy > cameraFovy_ ? 14.0f : 3.5f;
        const float zoomBlend = 1.0f - std::exp(-zoomRate * dt);
        cameraFovy_ += (desiredFovy - cameraFovy_) * zoomBlend;
        const GameplayCameraElevationGeometry elevationGeometry =
            gameplayCameraElevationGeometry(cameraElevationDegrees_,
                                             cameraFovy_);

        for (int index = 0; index < playerCount_; ++index)
        {
            // Smooth the focus while keeping the orbit synchronized with
            // this frame's span, including rapid portrait-window expansion.
            const Float3 desiredTarget{
                focus.x, kGameplayCameraTargetHeight, focus.z};
            CameraRig &rig = cameraRigs_[index];
            const float blend = rig.initialized
                                    ? 1.0f - std::exp(
                                                 -kGameplayCameraFollowResponsiveness *
                                                 dt)
                                    : 1.0f;
            rig.target.x += (desiredTarget.x - rig.target.x) * blend;
            rig.target.y += (desiredTarget.y - rig.target.y) * blend;
            rig.target.z += (desiredTarget.z - rig.target.z) * blend;
            rig.position = {
                rig.target.x + cameraBasis.offsetX *
                                   elevationGeometry.depthOffset,
                rig.target.y + elevationGeometry.verticalOffset,
                rig.target.z + cameraBasis.offsetZ *
                                   elevationGeometry.depthOffset};
            rig.initialized = true;
        }
    }

    void play(AudioCue cue)
    {
        if (audio_ != nullptr)
            audio_->play(cue);
        if (audioRequestObserver_)
            audioRequestObserver_(cue);
    }

    void emitEvent(const GameEvent &event) override
    {
        eventsThisUpdate_.push_back(event);
    }

    void spawnImpact(Float3 position, Float3 normal, bool heavy) override
    {
        if (presentation_ != nullptr)
            presentation_->impact(position, normal, heavy);
    }

    void spawnBrickImpact(Float3 position, Float3 normal, bool power,
                          bool destroyed) override
    {
        if (presentation_ != nullptr)
            presentation_->brickImpact(position, normal, power, destroyed);
    }

    void spawnExplosion(Float3 position, Rgba8 color) override
    {
        if (presentation_ != nullptr)
            presentation_->explosion(position, color);
    }

    void applyRadialCameraShake(XZ origin, float maximum,
                                float distanceFalloff) override
    {
        for (std::size_t playerIndex = 0;
             playerIndex < players_.size(); ++playerIndex)
        {
            const float distance = std::sqrt(distanceSquared(
                players_[playerIndex].position, origin));
            cameraShake_[playerIndex] = std::max(
                cameraShake_[playerIndex],
                std::max(0.0f,
                         maximum - distance * distanceFalloff));
        }
    }

    bool assignPlayerCameraShake(std::size_t playerIndex,
                                 float value) override
    {
        if (playerIndex >= cameraShake_.size())
            return false;
        cameraShake_[playerIndex] = value;
        return true;
    }

    void requestAudio(AudioCue cue) override
    {
        play(cue);
    }

    void syncEngineAudio(bool allowed)
    {
        if (audio_ == nullptr)
            return;
        const bool anyCreating = std::any_of(
            players_.begin(), players_.end(), [](const Player &player) {
                return player.active && player.creationTimer > 0.0f;
            });
        const bool anyReady = std::any_of(
            players_.begin(), players_.end(), [](const Player &player) {
                return player.active && player.creationTimer <= 0.0f;
            });
        const bool anyMoving = std::any_of(
            players_.begin(), players_.end(), [](const Player &player) {
                return player.active && player.creationTimer <= 0.0f &&
                       player.moving;
            });
        // The original mutes both engine beds whenever either player is in
        // CreatingState, so respawn and stage-start jingles remain exposed.
        audio_->updateEngine(allowed && anyReady && !anyCreating, anyMoving);
    }

    fs::path resourceRoot_;
    // Optional non-owning runtime output; main retains concrete ownership.
    AudioOutput *audio_ = nullptr;
    StageMap map_;
    std::vector<Player> players_;
    std::vector<Enemy> enemies_;
    std::vector<Shell> shells_;
    std::vector<Pickup> bonuses_;
    std::vector<GameEvent> eventsThisUpdate_;
    // Empty in normal play. Transitional same-TU tests attach an observer to
    // verify this one presentation path without retaining a runtime trace.
    ShellCancellationPresentationObserver
        shellCancellationPresentationObserver_{};
    // Empty in normal play. This observes only StageMap/GovernmentCore
    // presentation; enemy/player tank-hit timing remains deliberately separate.
    ShellMapCorePresentationObserver shellMapCorePresentationObserver_{};
    // Empty in normal play. The paired pre/post hooks preserve the player
    // armor effect's pre-commit timing while observing later tank actions.
    ShellTankPresentationObserver shellTankPresentationObserver_{};
    PlayerTankPreCommitFxObserver playerTankPreCommitFxObserver_{};
    // Empty in normal play. This observes the concrete shell/event/muzzle
    // adapter after each real side effect and retains no runtime trace.
    EnemyShellLaunchPresentationObserver
        enemyShellLaunchPresentationObserver_{};
    // Empty in normal play. This observes the accepted scalar player launch
    // through the final audio request without retaining a runtime trace.
    PlayerShellLaunchPresentationObserver
        playerShellLaunchPresentationObserver_{};
    // Empty in normal play. This observes the concrete pickup/event/audio
    // adapter after each real side effect and retains no runtime trace.
    BonusReleasePresentationObserver bonusReleasePresentationObserver_{};
    // Empty in normal play. This observes the concrete collection phases after
    // their real side effects and retains no runtime trace.
    BonusCollectionPresentationObserver
        bonusCollectionPresentationObserver_{};
    // Empty in normal play. This observes the pure report-entry plan followed
    // by concrete report/event/audio-silence phases.
    SettlementBeginPresentationObserver
        settlementBeginPresentationObserver_{};
    // Empty in normal play. This observes only settlement completion planning
    // and its concrete two-phase consumer, never counting/report rendering.
    SettlementTransitionPresentationObserver
        settlementTransitionPresentationObserver_{};
    // Empty in normal play. This observes semantic audio requests after their
    // real side effect so tests can also prove intentionally silent paths.
    AudioCueRequestObserver audioRequestObserver_{};
    // Optional non-owning frontend. Adapters that copy/move must rebind this
    // pointer to the new presentation owner; engine-free sessions leave it null.
    SessionPresentation *presentation_ = nullptr;
    std::array<CameraRig, 2> cameraRigs_{};
    std::array<float, 2> cameraShake_{};
    float cameraFovy_ = kSoloCameraSpan;
    int cameraYawDegrees_ = 0;
    int cameraElevationDegrees_ = kDefaultCameraElevationDegrees;
    std::uint32_t randomSeed_ = 0U;
    Mt19937RandomSource random_;
    StageLoadOperation stageLoadOperation_ = &loadGeneratedStageMap;
    std::string lastError_;
    std::string bonusMessage_;
    float bonusMessageTimer_ = 0.0f;
    int playerCount_ = 1;
    int startingLives_ = 10;
    AdvancedGameSettings advancedSettings_{};
    std::array<Nation, 2> startingNations_{{
        Nation::UnitedStates, Nation::SovietUnion}};
    int stage_ = 1;
    EnemySpawnState enemySpawnState_{kEnemiesPerStage, 0, 0, 0.0f};
    float stageIntroTimer_ = 0.0f;
    float stageTransitionTimer_ = 0.0f;
    float gameOverReportTimer_ = 0.0f;
    SettlementState settlement_{};
    int highScore_ = 2000;
    bool highScoreDisplay_ = false;
    float highScoreDisplayTimer_ = 0.0f;
    bool returnToMenuRequested_ = false;
    bool awaitingMenu_ = false;
    bool enemyCreationShowcase_ = false;
    bool baseAlive_ = true;
    bool paused_ = false;
    bool gameOver_ = false;
    bool showTargets_ = false;
};

} // namespace tanks3d::app

#endif
