#include <raylib.h>

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-field-initializers"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif
#include <raymath.h>
#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#include <rlgl.h>

#include "app/command_side_effect_dispatch.h"
#include "app/input_adapter.h"
#include "app/release_performance_capabilities.h"
#include "app/release_performance_log.h"
#include "app/release_performance_options.h"
#include "app/release_screenshot_file.h"
#include "app/release_screenshot_options.h"
#include "app/shell_cancellation_presentation.h"
#include "app/shell_map_core_presentation.h"
#include "app/shell_tank_presentation.h"
#include "audio/audio_cue.h"
#include "audio/audio_output.h"
#include "battle_fx.h"
#include "base_model.h"
#include "bonus_assets.h"
#include "core/coordinates.h"
#include "core/gameplay_rules.h"
#include "core/nation.h"
#include "game/bonus_system.h"
#include "game/combat_system.h"
#include "game/enemy_system.h"
#include "environment_assets.h"
#include "game/entities.h"
#include "game/game_event.h"
#include "game/player_system.h"
#include "game/settlement_system.h"
#include "game/stage_map.h"
#include "platform/gamepad_backend.h"
#include "post_process.h"
#include "tank_assets.h"
#include "../tests/test_support.h"
#include "../tests/stage_map_expectations.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <iomanip>
#include <initializer_list>
#include <iostream>
#include <limits>
#include <locale>
#include <memory>
#include <numeric>
#include <random>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <variant>
#include <vector>

#ifndef TANKS3D_RELEASE_SOURCE_COMMIT
#define TANKS3D_RELEASE_SOURCE_COMMIT "0000000000000000000000000000000000000000"
#endif

#ifndef TANKS3D_RELEASE_SOURCE_TAG
#define TANKS3D_RELEASE_SOURCE_TAG "development"
#endif

namespace fs = std::filesystem;

namespace
{
using tanks3d::app::AppendTankEventsAction;
using tanks3d::app::AppendMapCoreEventsAction;
using tanks3d::app::ApplyMapCoreRadialCameraShakeAction;
using tanks3d::app::ApplyTankRadialCameraShakeAction;
using tanks3d::app::AssignTankTargetCameraShakeAction;
using tanks3d::app::CommitMapCoreShellImpactAction;
using tanks3d::app::CommitTankShellImpactAction;
using tanks3d::app::CommandSideEffectDisposition;
using tanks3d::app::CommandSideEffectSink;
using tanks3d::app::Float3;
using tanks3d::app::GamepadActionFrame;
using tanks3d::app::GamepadAssignments;
using tanks3d::app::GamepadInputState;
using tanks3d::app::RequestTankAudioAction;
using tanks3d::app::RequestMapCoreAudioAction;
using tanks3d::app::Rgba8;
using tanks3d::app::ReleasePerformanceOptions;
using tanks3d::app::ReleasePerformanceRecorder;
using tanks3d::app::ReleaseScreenshotOptions;
using tanks3d::app::UiInputFrame;
using tanks3d::app::CameraPlanarBasis;
using tanks3d::app::cameraPlanarBasis;
using tanks3d::app::checkReleasePerformanceCapabilities;
using tanks3d::app::kReleasePerformanceCapabilitiesArgument;
using tanks3d::app::kReleasePerformanceCompleteMarker;
using tanks3d::app::kReleasePerformanceStartMarker;
using tanks3d::app::kReleaseScreenshotHeight;
using tanks3d::app::kReleaseScreenshotWidth;
using tanks3d::app::saveReleaseScreenshotFileNoReplace;
using tanks3d::app::ShellCancellationPresentationCommand;
using tanks3d::app::ShellCancellationPresentationStep;
using tanks3d::app::ShellMapCorePresentationAction;
using tanks3d::app::ShellMapCorePresentationCommand;
using tanks3d::app::ShellMapCorePresentationStep;
using tanks3d::app::ShellTankPresentationAction;
using tanks3d::app::ShellTankPresentationCommand;
using tanks3d::app::ShellTankPresentationStep;
using tanks3d::app::SpawnTankArmorImpactAction;
using tanks3d::app::SpawnTankExplosionAction;
using tanks3d::app::SpawnGovernmentCoreExplosionAction;
using tanks3d::app::SpawnGovernmentWallBreachImpactAction;
using tanks3d::app::SpawnMapCoreBrickImpactAction;
using tanks3d::app::SpawnMapCoreSurfaceImpactAction;
using tanks3d::app::makePlayerTankArmorImpactAction;
using tanks3d::app::makeShellCancellationPresentationCommand;
using tanks3d::app::makeShellMapCorePresentationCommand;
using tanks3d::app::makeShellTankPresentationCommand;
using tanks3d::app::parseReleasePerformanceOptions;
using tanks3d::app::parseReleaseScreenshotOptions;
using tanks3d::app::writeReleasePerformanceCapabilities;
using tanks3d::app::dispatchCommandSideEffect;
using tanks3d::app::kPhysicalGamepadSlotCount;
using tanks3d::app::kCameraYawMaximumDegrees;
using tanks3d::app::kCameraYawMinimumDegrees;
using tanks3d::app::kCameraYawStepDegrees;
using tanks3d::app::mapGamepadInput;
using tanks3d::app::mergePlayerControlFrame;
using tanks3d::app::mergeUiInputFrame;
using tanks3d::app::normalizedCameraYawDegrees;
using tanks3d::app::shellMapCorePresentationStep;
using tanks3d::app::shellTankPresentationStep;
using tanks3d::audio::AudioCue;
using tanks3d::audio::AudioOutput;
using tanks3d::core::AdvancedGameSettings;
using tanks3d::core::CardinalDirection;
using tanks3d::core::Nation;
using tanks3d::core::PlayerLevelStats;
using tanks3d::core::XZ;
using tanks3d::core::axisAlignedCentersOverlap;
using tanks3d::core::cardinalToward;
using tanks3d::core::cardinalVector;
using tanks3d::core::cardinalYaw;
using tanks3d::core::distanceSquared;
using tanks3d::core::kBaseShellSpeed;
using tanks3d::core::kClassicBaseShellSpeed;
using tanks3d::core::kDefaultPlayerMaximumHitPoints;
using tanks3d::core::kEnemyTuningMaximumPercent;
using tanks3d::core::kEnemyTuningMinimumPercent;
using tanks3d::core::kEnemyTuningPercentStep;
using tanks3d::core::kFastShellSpeed;
using tanks3d::core::kIceSlipDuration;
using tanks3d::core::kMaximumPlayerMaximumHitPoints;
using tanks3d::core::kMinimumPlayerMaximumHitPoints;
using tanks3d::core::kSelectableNations;
using tanks3d::core::kShellPacingScale;
using tanks3d::core::lengthSquared;
using tanks3d::core::nationName;
using tanks3d::core::normalizedAdvancedSettings;
using tanks3d::core::normalizedEnemyTuningPercent;
using tanks3d::core::normalizedNation;
using tanks3d::core::playerLevelStats;
using tanks3d::core::resolveIceTravel;
using tanks3d::core::snappedToCardinalLane;
using tanks3d::core::cycleNation;
using tanks3d::core::upgradedPlayerLevel;
using tanks3d::game::BonusApplication;
using tanks3d::game::BonusCollectionCallbacks;
using tanks3d::game::BonusCollectionIntent;
using tanks3d::game::BonusEffectCommand;
using tanks3d::game::BonusEffectCommandType;
using tanks3d::game::BonusReleaseIntent;
using tanks3d::game::BonusReleaseRandom;
using tanks3d::game::Enemy;
using tanks3d::game::EnemyArmorThresholds;
using tanks3d::game::EnemyEscapeChoice;
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
using tanks3d::game::BrickDamage;
using tanks3d::game::CombatOutcome;
using tanks3d::game::CombatTarget;
using tanks3d::game::EnemyTankImpactCommit;
using tanks3d::game::GameEvent;
using tanks3d::game::GameEventCause;
using tanks3d::game::GameEventType;
using tanks3d::game::GovernmentBasePart;
using tanks3d::game::PlayerTankImpactCommit;
using tanks3d::game::Player;
using tanks3d::game::DirectionButtonFrame;
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
using tanks3d::game::SettlementPhase;
using tanks3d::game::SettlementState;
using tanks3d::game::SettlementTransitionKind;
using tanks3d::game::SettlementTransitionPlan;
using tanks3d::game::SettlementUpdate;
using tanks3d::game::StageTally;
using tanks3d::game::StageMap;
using tanks3d::game::StageEndReason;
using tanks3d::game::GovernmentBaseTheme;
using tanks3d::game::GovernmentWallSegment;
using tanks3d::game::ImpactKind;
using tanks3d::game::ShellImpactDetails;
using tanks3d::game::aabbOverlapsRectangle;
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
using tanks3d::game::armorEnemyShouldFire;
using tanks3d::game::beginEnemyFrame;
using tanks3d::game::beginPlayerFrame;
using tanks3d::game::beginShellImpact;
using tanks3d::game::bonusOverlapsGovernmentBase;
using tanks3d::game::enemyDestructionComplete;
using tanks3d::game::enemyArmorForRoll;
using tanks3d::game::enemyArmorTankChanceForStage;
using tanks3d::game::enemyArmorThresholdsForStage;
using tanks3d::game::enemyRollCreatesArmorTank;
using tanks3d::game::enemyRollCreatesBonusCarrier;
using tanks3d::game::eventForShellCancellation;
using tanks3d::game::eventsForPhysicalShellImpact;
using tanks3d::game::chooseEnemyPursuitDirection;
using tanks3d::game::chooseEnemyEscape;
using tanks3d::game::governmentBaseThemeForNation;
using tanks3d::game::isGovernmentWallCell;
using tanks3d::game::governmentWallOverlapsAabb;
using tanks3d::game::governmentWallOverlapsShell;
using tanks3d::game::governmentWallSegment;
using tanks3d::game::isValidBonusType;
using tanks3d::game::kBonusCarrierChance;
using tanks3d::game::kEnemySpawnPoints;
using tanks3d::game::kEnemyTypeCount;
using tanks3d::game::kEnemyBlockedEscapeDelay;
using tanks3d::game::kEnemyTrackDustCooldown;
using tanks3d::game::kEnemyCreationDuration;
using tanks3d::game::kEnemyEscapeCommitTime;
using tanks3d::game::kEnemyEscapeProbeStep;
using tanks3d::game::kEnemyInitialFireDelay;
using tanks3d::game::kEnemySpawnInterval;
using tanks3d::game::kEnemySpawnRetryInterval;
using tanks3d::game::kGovernmentBaseCenter;
using tanks3d::game::kGovernmentCoreHalfSize;
using tanks3d::game::kGovernmentPowerShellDamage;
using tanks3d::game::kGovernmentSteelDuration;
using tanks3d::game::kGovernmentSteelFlashPeriod;
using tanks3d::game::kGovernmentSteelWarningDuration;
using tanks3d::game::kGovernmentWallCount;
using tanks3d::game::kGovernmentWallMaximumHealth;
using tanks3d::game::kGovernmentWallThickness;
using tanks3d::game::kMapSize;
using tanks3d::game::kPlayerTrackDustCooldown;
using tanks3d::game::kPlayerSpawnPoints;
using tanks3d::game::kPickupLifetime;
using tanks3d::game::kPickupTankHitExtent;
using tanks3d::game::kShellImpactDuration;
using tanks3d::game::kShellHalfSize;
using tanks3d::game::kShellSpawnDistance;
using tanks3d::game::kShellSweepStep;
using tanks3d::game::kShellTankHitExtent;
using tanks3d::game::kStageCount;
using tanks3d::game::kStreakPopupDuration;
using tanks3d::game::kTankRadius;
using tanks3d::game::kTankDeathDuration;
using tanks3d::game::kSettlementCountStepTime;
using tanks3d::game::kSettlementIdleTime;
using tanks3d::game::nextSettlementScoreCounter;
using tanks3d::game::planSettlementBegin;
using tanks3d::game::planSettlementTransition;
using tanks3d::game::planPlayerControl;
using tanks3d::game::normalizedStage;
using tanks3d::game::playerMeetsBonusTypeEligibility;
using tanks3d::game::prepareShellFrame;
using tanks3d::game::preparePlayerSpawnState;
using tanks3d::game::removeExpiredShells;
using tanks3d::game::resolvePlayerHit;
using tanks3d::game::resolveShellCancellations;
using tanks3d::game::resolveShellPhysicalImpact;
using tanks3d::game::resolveSweptShellCancellation;
using tanks3d::game::shellCancellationPoint;
using tanks3d::game::shellEvent;
using tanks3d::game::shellSpawnPosition;
using tanks3d::game::shellsCanCancel;
using tanks3d_test::kExpectedStageLayoutSignatures;
using tanks3d_test::stageLayoutSignature;

constexpr int kEnemiesPerStage = 20;
constexpr std::uint32_t kReleaseScreenshotSeed = 0x5c43e3d1U;
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
// Keep the fixed tilted camera local, but show enough surrounding lanes to
// plan interceptions without relying on the minimap for every nearby threat.
constexpr float kSoloCameraSpan = 15.5f;
// Camera elevation is measured above the ground plane. The supported range
// keeps tank silhouettes readable at the low endpoint while allowing a much
// flatter, near-top-down composition at the high endpoint.
constexpr int kCameraElevationMinimumDegrees = 40;
constexpr int kCameraElevationMaximumDegrees = 70;
constexpr int kCameraElevationStepDegrees = 5;
constexpr int kDefaultCameraElevationDegrees = 50;
constexpr float kGameplayCameraOrbitDistance = 21.017376f;
constexpr float kGameplayCameraTargetHeight = 0.35f;
constexpr float kGameplayCameraFollowResponsiveness = 12.0f;

int normalizedCameraElevationDegrees(int requestedDegrees)
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

GameplayCameraElevationGeometry gameplayCameraElevationGeometry(
    int requestedDegrees)
{
    const float radians =
        static_cast<float>(normalizedCameraElevationDegrees(
            requestedDegrees)) *
        (kPi / 180.0f);
    const float groundDepthProjection = std::sin(radians);
    return {
        std::cos(radians) * kGameplayCameraOrbitDistance,
        groundDepthProjection * kGameplayCameraOrbitDistance,
        groundDepthProjection};
}

float gameplayCameraSpan(XZ playerSeparation, int cameraYawDegrees,
                         int cameraElevationDegrees, float aspectRatio)
{
    const float aspect = std::isfinite(aspectRatio) && aspectRatio > 0.0f
                             ? aspectRatio
                             : static_cast<float>(kReleaseScreenshotWidth) /
                                   static_cast<float>(kReleaseScreenshotHeight);
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

float gameplayCameraAspectRatio()
{
    const int width = GetScreenWidth();
    const int height = GetScreenHeight();
    if (width > 0 && height > 0)
        return static_cast<float>(width) / static_cast<float>(height);
    return static_cast<float>(kReleaseScreenshotWidth) /
           static_cast<float>(kReleaseScreenshotHeight);
}

constexpr float kTankPairCollisionExtent = kTankRadius * 2.0f;
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

int enemyCreationFrame(float remainingTime)
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

float enemyCreationScale(int frame)
{
    static constexpr std::array<float, kEnemyCreationFrameCount> scales{{
        0.25f, 0.45f, 0.70f, 1.00f, 0.70f,
        0.45f, 0.25f, 0.45f, 0.70f, 1.00f}};
    return frame >= 0 && frame < kEnemyCreationFrameCount
               ? scales[static_cast<std::size_t>(frame)]
               : 0.0f;
}

constexpr std::chrono::nanoseconds kInteractiveFrameBudget{8'333'333};

class LaptopFramePacer
{
public:
    explicit LaptopFramePacer(std::chrono::steady_clock::time_point start)
        : deadline_(start + kInteractiveFrameBudget)
    {
    }

    ~LaptopFramePacer()
    {
        const auto now = std::chrono::steady_clock::now();
        if (now < deadline_)
            std::this_thread::sleep_until(deadline_);
    }

private:
    std::chrono::steady_clock::time_point deadline_;
};

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

Vector3 toRaylibVector3(Float3 value)
{
    return {value.x, value.y, value.z};
}

Color toRaylibColor(Rgba8 value)
{
    return {value.r, value.g, value.b, value.a};
}

Rgba8 toPresentationColor(Color value)
{
    return {value.r, value.g, value.b, value.a};
}

constexpr std::size_t kAudioCueCount =
    static_cast<std::size_t>(AudioCue::Count);
constexpr std::size_t kAudioVoiceCount = 12U;

// These are the original 2D edition's SoundConfig values.  There is no
// separate music track in that edition: its musical cues are the stage-start,
// game-over, and high-score jingles, while idle/moving form the battle bed.
constexpr std::array<const char *, kAudioCueCount> kAudioCueFiles{{
    "stage_start_up.ogg", "pause.ogg", "game_over.ogg",
    "highscore_beaten.ogg", "menu_item_selected.ogg",
    "bonus_appeared.ogg", "bonus_obtained.ogg",
    "bullet_hit_brick.ogg", "bullet_hit_map_boundaries.ogg",
    "bullet_hit_stone.ogg", "bullet_hit_bullet.ogg",
    "eagle_destroyed.ogg", "enemy_destroyed.ogg", "enemy_hit.ogg",
    "player_destroyed.ogg", "player_fired.ogg", "player_hit.ogg",
    "player_idle.ogg", "player_life_up.ogg", "player_moving.ogg",
    "player_respawn.ogg", "score_point_counted.ogg"}};

constexpr std::array<float, kAudioCueCount> kAudioCueVolumes{{
    1.00f, 1.00f, 1.00f, 1.00f, 0.70f, 0.90f, 0.90f, 0.40f,
    0.40f, 0.40f, 0.40f, 0.70f, 1.00f, 0.70f, 0.60f, 0.60f,
    1.00f, 0.50f, 1.00f, 0.50f, 1.00f, 0.80f}};

constexpr std::array<float, kAudioCueCount> kAudioOverlapFactors{{
    1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 0.90f, 0.90f, 0.70f,
    1.00f, 1.00f, 1.00f, 1.00f, 0.70f, 1.00f, 1.00f, 1.00f,
    1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f}};

constexpr std::array<bool, kAudioCueCount> kAudioMultiInstance{{
    false, false, false, false, true, true, true, true, true, true, true,
    true, true, true, true, true, true, false, true, false, false, true}};

constexpr bool audioCueHasHighestPriority(AudioCue cue)
{
    return cue == AudioCue::StageStart ||
           cue == AudioCue::HighScoreBeaten ||
           cue == AudioCue::PlayerRespawn;
}

class AudioBank final : public AudioOutput
{
public:
    AudioBank() = default;
    AudioBank(const AudioBank &) = delete;
    AudioBank &operator=(const AudioBank &) = delete;

    void load(const fs::path &resourceRoot)
    {
        if (!IsAudioDeviceReady())
            return;

        for (std::size_t index = 0; index < kAudioCueFiles.size(); ++index)
        {
            const fs::path path = resourceRoot / "sounds" /
                                  kAudioCueFiles[index];
            sounds_[index] = LoadSound(path.string().c_str());
            if (IsSoundValid(sounds_[index]))
            {
                SetSoundVolume(sounds_[index], kAudioCueVolumes[index]);
                SetSoundPitch(sounds_[index], 1.0f);
                if (kAudioMultiInstance[index])
                {
                    for (Sound &voice : aliases_[index])
                    {
                        voice = LoadSoundAlias(sounds_[index]);
                        if (IsSoundValid(voice))
                        {
                            SetSoundVolume(voice, kAudioCueVolumes[index]);
                            SetSoundPitch(voice, 1.0f);
                        }
                    }
                }
            }
        }
    }

    void play(AudioCue cue) override
    {
        const std::size_t index = static_cast<std::size_t>(cue);
        Sound &sound = sounds_[index];
        if (!IsSoundValid(sound))
            return;

        if (audioCueHasHighestPriority(cue))
            stopAll();
        else if (highestPriorityPlaying())
            return;

        if (!kAudioMultiInstance[index])
        {
            if (!IsSoundPlaying(sound))
                PlaySound(sound);
            return;
        }

        Sound *available = !IsSoundPlaying(sound) ? &sound : nullptr;
        int activeVoices = IsSoundPlaying(sound) ? 1 : 0;
        for (Sound &voice : aliases_[index])
        {
            if (!IsSoundValid(voice))
                continue;
            if (IsSoundPlaying(voice))
                ++activeVoices;
            else if (available == nullptr)
                available = &voice;
        }
        if (available == nullptr)
            return;

        const float overlapVolume = kAudioCueVolumes[index] *
            std::pow(kAudioOverlapFactors[index],
                     static_cast<float>(activeVoices));
        SetSoundVolume(*available, overlapVolume);
        PlaySound(*available);
    }

    void updateEngine(bool active, bool moving) override
    {
        Sound &idle = sounds_[static_cast<std::size_t>(AudioCue::PlayerIdle)];
        Sound &drive = sounds_[static_cast<std::size_t>(AudioCue::PlayerMoving)];
        if (!active || highestPriorityPlaying())
        {
            if (IsSoundValid(idle))
                StopSound(idle);
            if (IsSoundValid(drive))
                StopSound(drive);
            return;
        }
        Sound &wanted = moving ? drive : idle;
        Sound &other = moving ? idle : drive;
        if (IsSoundValid(other) && IsSoundPlaying(other))
            StopSound(other);
        if (IsSoundValid(wanted) && !IsSoundPlaying(wanted))
            PlaySound(wanted);
    }

    void stopAll() override
    {
        for (std::size_t index = 0; index < sounds_.size(); ++index)
        {
            Sound &sound = sounds_[index];
            if (IsSoundValid(sound))
                StopSound(sound);
            for (Sound &voice : aliases_[index])
                if (IsSoundValid(voice))
                    StopSound(voice);
        }
    }

    void unload()
    {
        for (std::size_t index = 0; index < sounds_.size(); ++index)
        {
            for (Sound &voice : aliases_[index])
            {
                if (IsSoundValid(voice))
                    UnloadSoundAlias(voice);
                voice = {};
            }
            Sound &sound = sounds_[index];
            if (IsSoundValid(sound))
                UnloadSound(sound);
            sound = {};
        }
    }

private:
    bool highestPriorityPlaying() const
    {
        for (AudioCue cue : {AudioCue::StageStart,
                             AudioCue::HighScoreBeaten,
                             AudioCue::PlayerRespawn})
        {
            const Sound &sound = sounds_[static_cast<std::size_t>(cue)];
            if (IsSoundValid(sound) && IsSoundPlaying(sound))
                return true;
        }
        return false;
    }

    std::array<Sound, kAudioCueCount> sounds_{};
    std::array<std::array<Sound, kAudioVoiceCount - 1U>,
               kAudioCueCount> aliases_{};
};

class SceneLighting
{
public:
    SceneLighting() = default;
    SceneLighting(const SceneLighting &) = delete;
    SceneLighting &operator=(const SceneLighting &) = delete;

    void load()
    {
        static constexpr const char *vertexShader = R"GLSL(
#version 330
in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec3 vertexNormal;
in vec4 vertexColor;
uniform mat4 mvp;
uniform mat4 matModel;
uniform mat4 matNormal;
uniform int modelSpaceInput;
out vec3 fragPosition;
out vec2 fragTexCoord;
out vec4 fragColor;
out vec3 fragNormal;
void main()
{
    // rlgl immediate geometry reaches this shader in world space, while GLB
    // meshes retain model-space vertices. Keep the stable immediate path and
    // opt external meshes into raylib's uploaded model/normal matrices.
    fragPosition = modelSpaceInput != 0
                       ? vec3(matModel*vec4(vertexPosition, 1.0))
                       : vertexPosition;
    fragTexCoord = vertexTexCoord;
    fragColor = vertexColor;
    fragNormal = normalize(modelSpaceInput != 0
                               ? vec3(matNormal*vec4(vertexNormal, 0.0))
                               : vertexNormal);
    gl_Position = mvp*vec4(vertexPosition, 1.0);
}
)GLSL";
        static constexpr const char *fragmentShader = R"GLSL(
#version 330
in vec3 fragPosition;
in vec2 fragTexCoord;
in vec4 fragColor;
in vec3 fragNormal;
uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform vec3 viewPos;
uniform vec3 lightDir;
uniform vec3 lightColor;
uniform mat4 lightVP;
uniform sampler2D shadowMap;
uniform int shadowMapResolution;
uniform int shadowsEnabled;
uniform int shadowQuality;
uniform int materialTagOverride;
out vec4 finalColor;

const float PI = 3.14159265358979323846;

float pow5(float value)
{
    float squared = value*value;
    return squared*squared*value;
}

float colorValue(vec3 color)
{
    return dot(color, vec3(0.2126, 0.7152, 0.0722));
}

vec3 fresnelSchlick(float viewHalf, vec3 f0)
{
    return f0 + (1.0 - f0)*pow5(1.0 - viewHalf);
}

float distributionGGX(float normalHalf, float roughness)
{
    float alpha = roughness*roughness;
    float alpha2 = alpha*alpha;
    float denominator = normalHalf*normalHalf*(alpha2 - 1.0) + 1.0;
    return alpha2/max(PI*denominator*denominator, 0.000001);
}

float geometrySchlick(float normalDirection, float k)
{
    return normalDirection/max(normalDirection*(1.0 - k) + k, 0.00001);
}

vec3 analyticSky(vec3 direction)
{
    float upper = smoothstep(-0.12, 0.22, direction.y);
    vec3 sky = mix(vec3(0.28, 0.38, 0.49), vec3(0.08, 0.18, 0.34),
                   smoothstep(0.0, 0.85, max(direction.y, 0.0)));
    vec3 ground = vec3(0.075, 0.065, 0.050)*(0.75 + 0.25*max(-direction.y, 0.0));
    return mix(ground, sky, upper);
}

float shadowVisibility(vec3 normal, vec3 toLight)
{
    if (shadowsEnabled == 0) return 1.0;
    vec4 lightSpace = lightVP*vec4(fragPosition, 1.0);
    vec3 projected = lightSpace.xyz/lightSpace.w;
    projected = projected*0.5 + 0.5;
    if (projected.x <= 0.0 || projected.x >= 1.0 ||
        projected.y <= 0.0 || projected.y >= 1.0 ||
        projected.z <= 0.0 || projected.z >= 1.0)
        return 1.0;

    float normalLight = max(dot(normal, toLight), 0.0);
    float bias = max(0.00065*(1.0 - normalLight), 0.00012);
    vec2 texel = vec2(1.0/float(shadowMapResolution));
    float occlusion = 0.0;
    float samples = 0.0;
    for (int x = -1; x <= 1; ++x)
    {
        for (int y = -1; y <= 1; ++y)
        {
            if (shadowQuality == 0 && abs(x) + abs(y) > 1) continue;
            float closest = texture(shadowMap, projected.xy + vec2(x, y)*texel).r;
            occlusion += projected.z - bias > closest ? 1.0 : 0.0;
            samples += 1.0;
        }
    }
    // Preserve sky/ground bounce in full shadow instead of crushing to black.
    return 1.0 - (occlusion/max(samples, 1.0))*0.82;
}

void main()
{
    vec4 sampled = texture(texture0, fragTexCoord)*colDiffuse;
    vec3 albedo = pow(max(sampled.rgb*fragColor.rgb, vec3(0.0)), vec3(2.2));
    int vertexMaterialTag = int(floor(fragColor.a*255.0 + 0.5));
    int materialTag = materialTagOverride >= 0
                          ? materialTagOverride
                          : vertexMaterialTag;
    // Vehicles use their own painted metal response. Architecture and earth
    // share a softer painted ramp so the whole battlefield belongs together.
    bool vehicleMaterial = materialTag >= 9 && materialTag <= 13;
    bool paintedWorld = materialTag >= 6 && materialTag <= 8;
    bool taggedMaterial = (materialTag >= 1 && materialTag <= 8) ||
                          vehicleMaterial;

    float roughness = 0.70;
    float metallic = 0.0;
    float dielectricF0 = 0.040;
    if (materialTag == 1 || materialTag == 9) roughness = 0.56; // painted armor
    else if (materialTag == 2 || materialTag == 10)
    { roughness = 0.36; metallic = 0.78; }                  // exposed steel
    else if (materialTag == 3 || materialTag == 11)
    { roughness = 0.93; dielectricF0 = 0.025; }             // rubber
    else if (materialTag == 4 || materialTag == 12)
    { roughness = 0.12; dielectricF0 = 0.080; }             // optics
    else if (materialTag == 5 || materialTag == 13) roughness = 0.88; // canvas/stowage
    else if (materialTag == 6) { roughness = 0.92; dielectricF0 = 0.035; } // masonry
    else if (materialTag == 7) roughness = 0.84;            // concrete/plaster
    else if (materialTag == 8) { roughness = 0.88; dielectricF0 = 0.030; } // soil/vegetation

    bool translucentWater = !taggedMaterial && fragColor.a > 0.65 &&
                            fragColor.a < 0.95 &&
                            fragColor.g > fragColor.r*1.20 &&
                            fragColor.b > fragColor.g*0.95;
    if (translucentWater)
    {
        roughness = fragColor.a > 0.90 ? 0.28 : 0.16;
        dielectricF0 = 0.025;
    }

    vec3 normal = normalize(fragNormal);
    vec3 viewDirection = normalize(viewPos - fragPosition);
    vec3 toLight = normalize(-lightDir);
    vec3 halfDirection = normalize(viewDirection + toLight);
    float normalView = max(dot(normal, viewDirection), 0.0001);
    float normalLight = max(dot(normal, toLight), 0.0);
    float normalHalf = max(dot(normal, halfDirection), 0.0);
    float viewHalf = max(dot(viewDirection, halfDirection), 0.0);
    float directNormalLight = normalLight;
    if (vehicleMaterial)
    {
        // Four deliberately broad value blocks preserve readable hand-painted
        // planes at gameplay scale without changing the model silhouettes.
        directNormalLight = normalLight < 0.18 ? 0.10 :
                            normalLight < 0.46 ? 0.36 :
                            normalLight < 0.76 ? 0.70 : 1.03;
    }

    vec3 f0 = mix(vec3(dielectricF0), albedo, metallic);
    vec3 fresnel = fresnelSchlick(viewHalf, f0);
    float k = roughness + 1.0;
    k = k*k/8.0;
    float geometry = geometrySchlick(normalView, k)*geometrySchlick(normalLight, k);
    vec3 specular = distributionGGX(normalHalf, roughness)*geometry*fresnel/
                    max(4.0*normalView*normalLight, 0.0001);
    float graphicHighlightMask = 0.0;
    if (vehicleMaterial)
    {
        graphicHighlightMask = smoothstep(0.70, 0.91, normalHalf);
        float highlightStrength = materialTag == 12 ? 0.46 :
                                  materialTag == 10 ? 0.25 :
                                  materialTag == 9 ? 0.16 :
                                  materialTag == 13 ? 0.075 : 0.025;
        vec3 highlightColor = materialTag == 12
                                  ? vec3(0.56, 0.96, 1.18)
                                  : vec3(1.16, 0.86, 0.48);
        // Use a compact painted glint instead of a broad photographic hotspot.
        specular = specular*0.24 +
                   highlightColor*graphicHighlightMask*highlightStrength;
    }
    vec3 diffuseWeight = (1.0 - fresnel)*(1.0 - metallic);
    float visibility = shadowVisibility(normal, toLight);
    vec3 direct = (diffuseWeight*albedo/PI + specular)*lightColor*
                  2.55*directNormalLight*visibility;

    float hemisphere = normal.y*0.5 + 0.5;
    if (vehicleMaterial)
        hemisphere = hemisphere < 0.34 ? 0.22 :
                     hemisphere < 0.70 ? 0.52 : 0.86;
    vec3 reflected = reflect(-viewDirection, normal);
    vec3 blurredReflection = normalize(mix(reflected, normal, roughness*roughness));
    vec3 environmentFresnel = f0 + (max(vec3(1.0 - roughness), f0) - f0)*
                              pow5(1.0 - normalView);
    vec3 diffuseIrradiance = mix(vec3(0.09, 0.075, 0.055),
                                 vec3(0.28, 0.38, 0.52), hemisphere);
    float ambientOcclusion = mix(0.70, 1.0, hemisphere);
    vec3 indirect = ((1.0 - metallic)*(1.0 - environmentFresnel)*albedo*
                     diffuseIrradiance + analyticSky(blurredReflection)*environmentFresnel)*
                    ambientOcclusion;

    vec3 linearColor = direct + indirect;
    if (paintedWorld)
    {
        // Broad warm highlights, blue-green recesses, and a little bounce
        // retain the authored plaster/brick colors throughout the camera orbit.
        float wrappedLight = clamp(normalLight*0.82 + 0.18, 0.0, 1.0);
        float paintedLight = mix(0.48, 0.79, smoothstep(0.22, 0.40, wrappedLight));
        paintedLight = mix(paintedLight, 1.12, smoothstep(0.70, 0.87, wrappedLight));
        float castShade = mix(0.55, 1.0, visibility);
        vec3 warm = vec3(1.10, 1.01, 0.84);
        vec3 cool = vec3(0.66, 0.81, 0.80);
        vec3 pigment = mix(cool, warm, wrappedLight*castShade);
        linearColor = albedo*paintedLight*castShade*pigment;
        linearColor += albedo*vec3(0.045, 0.050, 0.030);
    }
    float vehicleInkEdge = 0.0;
    float vehicleUnderside = 0.0;
    vec3 vehicleInkColor = vec3(0.006, 0.008, 0.004);
    if (vehicleMaterial)
    {
        // Vehicles deliberately stop using the photographic PBR result here.
        // Three hard painted tones retain the authored base colors while making
        // the rendering read as a 1990s arcade animation at normal game zoom.
        vec3 inkTone = mix(vec3(0.004, 0.006, 0.003), albedo*0.10, 0.28);
        vec3 shadowTone = albedo*vec3(0.40, 0.49, 0.46) +
                          vec3(0.006, 0.010, 0.008);
        vec3 middleTone = albedo*0.88 + vec3(0.012, 0.014, 0.006);
        vec3 lightTone = albedo*1.22 + vec3(0.046, 0.035, 0.016);
        float shadeCoordinate = normalLight*(visibility < 0.55 ? 0.30 : 1.0);
        shadeCoordinate += normal.y > 0.56 ? 0.055 : 0.0;
        linearColor = shadeCoordinate < 0.30 ? shadowTone :
                      shadeCoordinate < 0.72 ? middleTone : lightTone;

        // A fine two-physical-pixel ordered staircase adds hand-painted grain
        // without turning a gameplay-sized roof into a visible checkerboard.
        vec2 arcadeCell = floor(gl_FragCoord.xy/2.0);
        float arcadeOrder = mod(arcadeCell.x + arcadeCell.y*2.0, 4.0);
        float staircase = arcadeOrder < 1.0 ? -0.026 :
                          arcadeOrder < 2.0 ? -0.008 :
                          arcadeOrder < 3.0 ? 0.010 : 0.026;
        linearColor *= 1.0 + staircase*(shadeCoordinate < 0.72 ? 1.0 : 0.45);

        // Material separation is intentionally graphic rather than physical:
        // warm saturated armor, cool flat steel, almost-ink rubber, luminous
        // cyan optics, and muted ochre canvas.
        float separatedValue = colorValue(linearColor);
        if (materialTag == 9)
        {
            linearColor = max(mix(vec3(separatedValue), linearColor, 1.22),
                              vec3(0.0));
        }
        else if (materialTag == 10)
        {
            linearColor = mix(vec3(separatedValue), linearColor, 0.42)*
                          vec3(0.82, 0.92, 1.04);
        }
        else if (materialTag == 11)
        {
            linearColor = mix(inkTone,
                              vec3(separatedValue*0.42,
                                   separatedValue*0.48,
                                   separatedValue*0.34), 0.55);
        }
        else if (materialTag == 12)
        {
            linearColor = max(linearColor, albedo*0.62) +
                          vec3(0.025, 0.145, 0.225);
        }
        else
        {
            linearColor = mix(vec3(separatedValue), linearColor, 0.62)*
                          vec3(1.05, 0.98, 0.73);
        }

        // Highlights are compact opaque paint patches, not metal reflections.
        float hardHighlight = step(0.82, normalHalf)*step(0.58, normalLight)*
                              step(0.42, normal.y*0.5 + 0.5);
        if (materialTag == 9)
            linearColor = mix(linearColor,
                              albedo*1.04 + vec3(0.10, 0.074, 0.031),
                              hardHighlight*0.26);
        else if (materialTag == 10)
            linearColor += vec3(0.045, 0.054, 0.050)*hardHighlight;
        else if (materialTag == 12)
            linearColor = mix(linearColor, vec3(0.66, 0.94, 1.10),
                              hardHighlight*0.70);

        // Save ink masks until after fog so the tank's lower edge and silhouette
        // remain grounded instead of being washed into the environment.
        vehicleInkEdge = 1.0 - smoothstep(0.075, 0.25, normalView);
        float heightInk = 1.0 - smoothstep(0.045, 0.25, fragPosition.y);
        float downwardInk = 1.0 - smoothstep(-0.48, 0.18, normal.y);
        vehicleUnderside = clamp(heightInk*0.42 + downwardInk*0.30, 0.0, 1.0);
        vehicleInkColor = mix(vec3(0.003, 0.005, 0.002), albedo*0.045, 0.20);
    }
    float distanceToCamera = length(viewPos - fragPosition);
    float fogDensity = 0.003*exp(-0.10*max(0.0, 0.5*(fragPosition.y + viewPos.y)));
    float transmittance = exp(-fogDensity*max(distanceToCamera - 6.0, 0.0));
    vec3 fogColor = vec3(0.21, 0.25, 0.22);
    linearColor = mix(fogColor, linearColor,
                      clamp(transmittance, vehicleMaterial ? 0.78 : 0.42, 1.0));
    if (vehicleMaterial)
    {
        float inkAmount = clamp(vehicleInkEdge*0.36 + vehicleUnderside*0.42,
                                0.0, materialTag == 12 ? 0.34 : 0.78);
        linearColor = mix(linearColor, vehicleInkColor, inkAmount);
    }

    // Keep gamma-encoded scene color for the existing filmic post pass. A
    // half-float target preserves values above one for bloom/tonemapping.
    vec3 encoded = pow(max(linearColor, vec3(0.0)), vec3(1.0/2.2));
    if (vehicleMaterial)
    {
        vec2 paletteCell = floor(gl_FragCoord.xy/2.0);
        float paletteDither = mod(paletteCell.x + paletteCell.y*2.0, 4.0) - 1.5;
        encoded = floor(max(encoded + paletteDither*0.006, vec3(0.0))*16.0 + 0.5)/16.0;
    }
    float opacity = taggedMaterial ? 1.0 : sampled.a*fragColor.a;
    finalColor = vec4(encoded, opacity);
}
)GLSL";
        shader_ = LoadShaderFromMemory(vertexShader, fragmentShader);
        if (!IsShaderValid(shader_))
            return;
        shader_.locs[SHADER_LOC_MATRIX_MODEL] =
            GetShaderLocation(shader_, "matModel");
        shader_.locs[SHADER_LOC_MATRIX_NORMAL] =
            GetShaderLocation(shader_, "matNormal");
        shader_.locs[SHADER_LOC_MAP_DIFFUSE] =
            GetShaderLocation(shader_, "texture0");
        shader_.locs[SHADER_LOC_COLOR_DIFFUSE] =
            GetShaderLocation(shader_, "colDiffuse");
        viewLocation_ = GetShaderLocation(shader_, "viewPos");
        modelSpaceLocation_ = GetShaderLocation(shader_, "modelSpaceInput");
        materialTagOverrideLocation_ =
            GetShaderLocation(shader_, "materialTagOverride");
        const int directionLocation = GetShaderLocation(shader_, "lightDir");
        const int colorLocation = GetShaderLocation(shader_, "lightColor");
        lightVpLocation_ = GetShaderLocation(shader_, "lightVP");
        shadowMapLocation_ = GetShaderLocation(shader_, "shadowMap");
        shadowResolutionLocation_ = GetShaderLocation(shader_, "shadowMapResolution");
        shadowsEnabledLocation_ = GetShaderLocation(shader_, "shadowsEnabled");
        shadowQualityLocation_ = GetShaderLocation(shader_, "shadowQuality");
        lightDirection_ = Vector3Normalize({-0.48f, -1.0f, -0.36f});
        const Vector3 color{1.00f, 0.94f, 0.82f};
        SetShaderValue(shader_, directionLocation, &lightDirection_, SHADER_UNIFORM_VEC3);
        SetShaderValue(shader_, colorLocation, &color, SHADER_UNIFORM_VEC3);
        const int immediateSpace = 0;
        const int vertexMaterialTag = -1;
        SetShaderValue(shader_, modelSpaceLocation_, &immediateSpace,
                       SHADER_UNIFORM_INT);
        SetShaderValue(shader_, materialTagOverrideLocation_,
                       &vertexMaterialTag, SHADER_UNIFORM_INT);

        static constexpr const char *depthVertexShader = R"GLSL(
#version 330
in vec3 vertexPosition;
uniform mat4 mvp;
void main() { gl_Position = mvp*vec4(vertexPosition, 1.0); }
)GLSL";
        static constexpr const char *depthFragmentShader = R"GLSL(
#version 330
// The fixed-function depth write is sufficient and preserves early-Z.
void main() { }
)GLSL";
        depthShader_ = LoadShaderFromMemory(depthVertexShader, depthFragmentShader);
        configureShadowMap(true);
    }

    void begin(Vector3 cameraPosition)
    {
        if (!IsShaderValid(shader_))
            return;
        SetShaderValue(shader_, viewLocation_, &cameraPosition, SHADER_UNIFORM_VEC3);
        const int shadowsEnabled = shadowAvailable_ ? 1 : 0;
        const int immediateSpace = 0;
        const int vertexMaterialTag = -1;
        SetShaderValue(shader_, shadowsEnabledLocation_, &shadowsEnabled, SHADER_UNIFORM_INT);
        SetShaderValue(shader_, modelSpaceLocation_, &immediateSpace,
                       SHADER_UNIFORM_INT);
        SetShaderValue(shader_, materialTagOverrideLocation_,
                       &vertexMaterialTag, SHADER_UNIFORM_INT);
        BeginShaderMode(shader_);
        if (shadowAvailable_)
        {
            // BeginShaderMode records the rlgl batch state. Explicitly bind
            // the program before setting the raw sampler uniform, matching
            // raylib's official shadow-map example.
            rlEnableShader(shader_.id);
            rlActiveTextureSlot(kShadowTextureSlot);
            rlEnableTexture(shadowMap_.depth.id);
            rlSetUniform(shadowMapLocation_, &kShadowTextureSlot, SHADER_UNIFORM_INT, 1);
            rlActiveTextureSlot(0);
        }
        active_ = true;
    }

    void end()
    {
        if (active_)
        {
            EndShaderMode();
            if (shadowAvailable_)
            {
                rlActiveTextureSlot(kShadowTextureSlot);
                rlDisableTexture();
                rlActiveTextureSlot(0);
            }
            active_ = false;
        }
    }

    template <typename DrawCasters>
    void updateShadowMap(DrawCasters &&drawCasters)
    {
        if (!shadowAvailable_ || !IsShaderValid(depthShader_))
            return;

        // The main loop targets 120 Hz, but a 60 Hz shadow refresh is already
        // temporally coherent and preserves High's previous visual cadence.
        // Balanced keeps its previous 30 Hz budget instead of paying for the
        // 2048-square depth pass twice as often after the frame-rate increase.
        const int interval = highQuality_ ? 2 : 4;
        const bool shouldUpdate = !hasRenderedShadow_ ||
                                  (shadowFrameCounter_ % static_cast<unsigned int>(interval) == 0U);
        ++shadowFrameCounter_;
        if (!shouldUpdate)
            return;

        const Vector3 target{13.0f, 0.0f, 13.0f};
        Camera3D lightCamera{};
        lightCamera.position = Vector3Subtract(target, Vector3Scale(lightDirection_, 34.0f));
        lightCamera.target = target;
        lightCamera.up = {0.0f, 0.0f, -1.0f};
        lightCamera.fovy = 42.0f;
        lightCamera.projection = CAMERA_ORTHOGRAPHIC;

        const double previousNear = rlGetCullDistanceNear();
        const double previousFar = rlGetCullDistanceFar();
        rlSetClipPlanes(1.0, 78.0);
        BeginTextureMode(shadowMap_);
        ClearBackground(WHITE);
        BeginMode3D(lightCamera);
        const Matrix lightView = rlGetMatrixModelview();
        const Matrix lightProjection = rlGetMatrixProjection();
        BeginShaderMode(depthShader_);
        drawCasters();
        EndShaderMode();
        EndMode3D();
        EndTextureMode();
        rlSetClipPlanes(previousNear, previousFar);

        lightViewProjection_ = MatrixMultiply(lightView, lightProjection);
        SetShaderValueMatrix(shader_, lightVpLocation_, lightViewProjection_);
        hasRenderedShadow_ = true;
    }

    void toggleQuality()
    {
        configureShadowMap(!highQuality_);
    }

    bool highQuality() const { return highQuality_; }
    bool shadowsAvailable() const { return shadowAvailable_; }

    void unload()
    {
        unloadShadowMap();
        if (IsShaderValid(depthShader_))
            UnloadShader(depthShader_);
        if (IsShaderValid(shader_))
            UnloadShader(shader_);
        depthShader_ = {};
        shader_ = {};
    }

    Shader shader() const { return shader_; }
    Shader depthShader() const { return depthShader_; }

private:
    static constexpr int kShadowTextureSlot = 10;

    static RenderTexture2D loadShadowMap(int resolution)
    {
        RenderTexture2D target{};
        target.id = rlLoadFramebuffer();
        target.texture.width = resolution;
        target.texture.height = resolution;
        if (target.id == 0)
            return target;

        rlEnableFramebuffer(target.id);
        target.depth.id = rlLoadTextureDepth(resolution, resolution, false);
        target.depth.width = resolution;
        target.depth.height = resolution;
        // Depth format is selected internally by rlgl; PixelFormat does not
        // expose a portable enum for this attachment.
        target.depth.format = 0;
        target.depth.mipmaps = 1;
        rlFramebufferAttach(target.id, target.depth.id, RL_ATTACHMENT_DEPTH,
                            RL_ATTACHMENT_TEXTURE2D, 0);
        if (!rlFramebufferComplete(target.id))
        {
            rlDisableFramebuffer();
            rlUnloadFramebuffer(target.id);
            return {};
        }
        rlDisableFramebuffer();
        return target;
    }

    void unloadShadowMap()
    {
        if (shadowMap_.id != 0)
            rlUnloadFramebuffer(shadowMap_.id);
        shadowMap_ = {};
        shadowAvailable_ = false;
        hasRenderedShadow_ = false;
    }

    void configureShadowMap(bool highQuality)
    {
        highQuality_ = highQuality;
        unloadShadowMap();
        if (!IsShaderValid(depthShader_) || !IsShaderValid(shader_))
            return;
        const int resolution = highQuality_ ? 2048 : 1024;
        shadowMap_ = loadShadowMap(resolution);
        shadowAvailable_ = shadowMap_.id != 0 && shadowMap_.depth.id != 0;
        if (!shadowAvailable_)
        {
            TraceLog(LOG_WARNING, "TANKS3D: real-time shadow map unavailable");
            return;
        }
        // PCF is performed explicitly in the shader; point depth sampling
        // avoids a second interpolation step that can create light leaks.
        SetTextureFilter(shadowMap_.depth, TEXTURE_FILTER_POINT);
        SetTextureWrap(shadowMap_.depth, TEXTURE_WRAP_CLAMP);
        SetShaderValue(shader_, shadowResolutionLocation_, &resolution, SHADER_UNIFORM_INT);
        const int quality = highQuality_ ? 1 : 0;
        SetShaderValue(shader_, shadowQualityLocation_, &quality, SHADER_UNIFORM_INT);
        TraceLog(LOG_INFO, "TANKS3D: %s shadow map ready (%ix%i)",
                 highQuality_ ? "high-quality" : "balanced", resolution, resolution);
    }

    Shader shader_{};
    Shader depthShader_{};
    RenderTexture2D shadowMap_{};
    Matrix lightViewProjection_{};
    Vector3 lightDirection_{};
    int viewLocation_ = -1;
    int lightVpLocation_ = -1;
    int shadowMapLocation_ = -1;
    int shadowResolutionLocation_ = -1;
    int shadowsEnabledLocation_ = -1;
    int shadowQualityLocation_ = -1;
    int modelSpaceLocation_ = -1;
    int materialTagOverrideLocation_ = -1;
    unsigned int shadowFrameCounter_ = 0;
    bool active_ = false;
    bool highQuality_ = true;
    bool shadowAvailable_ = false;
    bool hasRenderedShadow_ = false;
};

XZ forwardFromYaw(float yaw)
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

bool operator==(const SessionDigest &first, const SessionDigest &second)
{
    return first.state == second.state;
}

struct CameraRig
{
    Vector3 position{kGovernmentBaseCenter.x,
                     kGameplayCameraTargetHeight +
                         kGameplayCameraOrbitDistance,
                     kGovernmentBaseCenter.z};
    Vector3 target{kGovernmentBaseCenter.x, kGameplayCameraTargetHeight,
                   kGovernmentBaseCenter.z};
    bool initialized = false;
};

XZ playerSpawn(int id)
{
    return kPlayerSpawnPoints[static_cast<std::size_t>(id == 0 ? 0 : 1)];
}

Color playerColor(int id);
struct Game3DTestAccess;

void preparePlayerSpawn(Player &player, bool resetLevel)
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
        return distribution(engine_);
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

bool loadGeneratedStageMap(StageMap &map, const fs::path &resourceRoot,
                           int stage, std::string &error)
{
    return map.load(resourceRoot, stage, error);
}

class Game3D : private CommandSideEffectSink
{
public:
    explicit Game3D(fs::path resourceRoot, AudioOutput *audio = nullptr)
        : Game3D(std::move(resourceRoot),
                 static_cast<std::uint32_t>(std::random_device{}()), audio)
    {
    }

    Game3D(fs::path resourceRoot, std::uint32_t randomSeed,
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
    const BattleFx &effects() const { return effects_; }
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
        if (!paused_)
            effects_.update(dt);
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

    Camera3D cameraForPlayer(int index) const
    {
        const CameraRig &rig = cameraRigs_[std::clamp(index, 0, 1)];
        Camera3D camera{};
        camera.position = rig.position;
        camera.target = rig.target;
        const float shake = cameraShake_[static_cast<std::size_t>(std::clamp(index, 0, 1))];
        if (shake > 0.0f)
        {
            const float phase = static_cast<float>(GetTime()) * 57.0f + index * 2.1f;
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
        camera.projection = CAMERA_ORTHOGRAPHIC;
        return camera;
    }

private:
    friend struct Game3DTestAccess;

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
        effects_.clear();
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
                           bool allowWater = false) const
    {
        if (map_.collidesWithTank(position, kTankRadius, allowWater))
            return false;
        for (int i = 0; i < static_cast<int>(players_.size()); ++i)
        {
            if (i != ignoredPlayer && players_[i].active &&
                players_[i].creationTimer <= 0.0f &&
                axisAlignedCentersOverlap(position, players_[i].position,
                                          kTankPairCollisionExtent))
                return false;
        }
        for (int i = 0; i < static_cast<int>(enemies_.size()); ++i)
        {
            if (i != ignoredEnemy && !enemies_[i].destroyed &&
                enemies_[i].creationTimer <= 0.0f &&
                axisAlignedCentersOverlap(position, enemies_[i].position,
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
            [this, playerIndex, allowWater](XZ candidate) {
                return positionAvailable(candidate, playerIndex, -1,
                                         allowWater);
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
            effects_.spawnTrackDust(
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

        const XZ direction = cardinalVector(intent.direction);
        const float muzzleDistance =
            wwii_tank_model::playerMuzzleDistance(player.nation,
                                                  intent.playerLevel);
        const XZ muzzle =
            intent.tankPosition + direction * muzzleDistance;
        effects_.spawnMuzzleFlash(
            {muzzle.x,
             wwii_tank_model::playerMuzzleHeight(player.nation,
                                                 intent.playerLevel),
             muzzle.z},
            {direction.x, 0.0f, direction.z}, playerColor(player.id));
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
            if (!frame.valid || frame.phase != EnemyFramePhase::Active)
                continue;
            enemy.target = chooseEnemyTarget(enemy);

            const EnemySteeringOutcome steering =
                advanceActiveEnemySteering(
                    enemy, frame, dt,
                    [&](XZ candidate) {
                        return positionAvailable(candidate, -1, index);
                    },
                    EnemySteeringRandom{
                        [&]() { return random.draw(random01); },
                        [&]() {
                            return random.draw(randomDirectionIndex);
                        }});
            if (steering == EnemySteeringOutcome::Invalid)
                continue;

            const bool onIce = map_.isIce(enemy.position);
            const float movementSpeed = enemyMovementSpeedForType(
                enemy.type, advancedSettings_);
            const auto movement = advanceActiveEnemyMovement(
                enemy, frame, dt, movementSpeed, onIce,
                [&](XZ candidate) {
                    return positionAvailable(candidate, -1, index);
                });
            if (!movement.valid)
                continue;
            if (movement.dust)
            {
                effects_.spawnTrackDust(
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

        const XZ direction = cardinalVector(intent.direction);
        const float muzzleDistance =
            wwii_tank_model::muzzleDistance(true, intent.enemyType);
        const XZ muzzle =
            intent.tankPosition + direction * muzzleDistance;
        effects_.spawnMuzzleFlash({muzzle.x,
                                   wwii_tank_model::muzzleHeight(
                                       true, intent.enemyType),
                                   muzzle.z},
                                  {direction.x, 0.0f, direction.z},
                                  Color{255, 89, 45, 255});
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
                playerExplosionColor = toPresentationColor(
                    playerColor(outcome.targetPlayerId));
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
                            bonus_assets::name(application.type) + "  +" +
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
                return positionAvailable(candidate, -1, -1);
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
        const GameplayCameraElevationGeometry elevationGeometry =
            gameplayCameraElevationGeometry(cameraElevationDegrees_);
        // Follow every movement instead of waiting for the player group to
        // reach a large screen-space dead zone. Solo play tracks the tank;
        // co-op tracks the midpoint and expands the view for separation.
        const float aspect = gameplayCameraAspectRatio();

        float desiredFovy = kSoloCameraSpan;
        if (trackedPlayers > 1)
        {
            desiredFovy = gameplayCameraSpan(
                {tracked[1].x - tracked[0].x,
                 tracked[1].z - tracked[0].z},
                cameraYawDegrees_, cameraElevationDegrees_, aspect);
        }

        for (int index = 0; index < playerCount_; ++index)
        {
            // Orbit at a fixed distance using the selected azimuth and
            // elevation. Co-op tracks the midpoint and zooms as necessary.
            const Vector3 desiredTarget{
                focus.x, kGameplayCameraTargetHeight, focus.z};
            const Vector3 desiredPosition{
                focus.x + cameraBasis.offsetX *
                              elevationGeometry.depthOffset,
                kGameplayCameraTargetHeight +
                    elevationGeometry.verticalOffset,
                focus.z + cameraBasis.offsetZ *
                              elevationGeometry.depthOffset};
            CameraRig &rig = cameraRigs_[index];
            const float blend = rig.initialized
                                    ? 1.0f - std::exp(
                                                 -kGameplayCameraFollowResponsiveness *
                                                 dt)
                                    : 1.0f;
            rig.position.x += (desiredPosition.x - rig.position.x) * blend;
            rig.position.y += (desiredPosition.y - rig.position.y) * blend;
            rig.position.z += (desiredPosition.z - rig.position.z) * blend;
            rig.target.x += (desiredTarget.x - rig.target.x) * blend;
            rig.target.y += (desiredTarget.y - rig.target.y) * blend;
            rig.target.z += (desiredTarget.z - rig.target.z) * blend;
            rig.initialized = true;
        }
        const float zoomRate = desiredFovy > cameraFovy_ ? 14.0f : 3.5f;
        const float zoomBlend = 1.0f - std::exp(-zoomRate * dt);
        cameraFovy_ += (desiredFovy - cameraFovy_) * zoomBlend;
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
        effects_.spawnImpact(toRaylibVector3(position),
                             toRaylibVector3(normal), heavy);
    }

    void spawnBrickImpact(Float3 position, Float3 normal, bool power,
                          bool destroyed) override
    {
        effects_.spawnBrickImpact(toRaylibVector3(position),
                                  toRaylibVector3(normal), power,
                                  destroyed);
    }

    void spawnExplosion(Float3 position, Rgba8 color) override
    {
        effects_.spawnTankExplosion(toRaylibVector3(position),
                                    toRaylibColor(color));
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
    BattleFx effects_;
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

Color playerColor(int id)
{
    // Match the original atlas: P1 is gold/orange and P2 is bright green.
    return id == 0 ? Color{236, 145, 24, 255} : Color{36, 205, 94, 255};
}

Color materialColor(Color color, unsigned char tag)
{
    color.a = tag;
    return color;
}

Color enemyArmorColor(int armor)
{
    // Enemy armor rows in resources/textures/texture.png: blue, olive,
    // sand, then teal. This palette is part of the original visual language.
    switch (std::clamp(armor, 1, 4))
    {
    case 1: return Color{161, 194, 207, 255};
    case 2: return Color{113, 139, 64, 255};
    case 3: return Color{207, 164, 101, 255};
    default: return Color{39, 151, 112, 255};
    }
}

void drawTankModel(TankAssets &assets, XZ position, float yaw, Color bodyColor,
                   bool enemy, int armor, float shield, int identity, bool moving,
                   Nation nation, bool shadowPass = false)
{
    assets.draw(position.x, position.z, yaw, bodyColor, enemy, armor, shield,
                identity, moving, nation, shadowPass);
}

void drawBrickTile(const StageMap &map, const EnvironmentAssets &environment,
                   int row, int column)
{
    const unsigned char brickMask = map.brickMask(row, column);
    const auto isBuilding = [&map](int neighborRow, int neighborColumn) {
        return map.tile(neighborRow, neighborColumn) == '#' &&
               !isGovernmentWallCell(neighborRow, neighborColumn);
    };
    const std::array<bool, 4> exposedFaces{{
        !isBuilding(row - 1, column),
        !isBuilding(row + 1, column),
        !isBuilding(row, column - 1),
        !isBuilding(row, column + 1)}};
    environment.drawUrbanBuildingCell(map.stage(), row, column, brickMask,
                                      exposedFaces);
}

void drawGroundDetail(int row, int column)
{
    const unsigned int hash = static_cast<unsigned int>(row * 92821 + column * 68917 + 17);
    const float x = column + 0.18f + static_cast<float>((hash >> 3U) & 255U) / 420.0f;
    const float z = row + 0.18f + static_cast<float>((hash >> 11U) & 255U) / 420.0f;
    if (hash % 7U == 0U)
    {
        const std::array<std::array<Vector3, 3>, 2> blades{{
            {{{x - 0.045f, 0.0f, z}, {x + 0.010f, 0.14f, z}, {x + 0.050f, 0.0f, z}}},
            {{{x, 0.0f, z - 0.045f}, {x - 0.016f, 0.11f, z}, {x, 0.0f, z + 0.050f}}}}};
        const auto submitTriangle = [](const std::array<Vector3, 3> &triangle,
                                       Color color, Vector3 normal) {
            rlColor4ub(color.r, color.g, color.b, color.a);
            rlNormal3f(normal.x, normal.y, normal.z);
            for (const Vector3 &point : triangle)
                rlVertex3f(point.x, point.y, point.z);
            for (auto point = triangle.rbegin(); point != triangle.rend(); ++point)
                rlVertex3f(point->x, point->y, point->z);
        };
        rlBegin(RL_TRIANGLES);
        submitTriangle(blades[0], Color{48, 91, 39, 220}, {0.0f, 0.55f, 0.84f});
        submitTriangle(blades[1], Color{91, 135, 54, 215}, {0.84f, 0.55f, 0.0f});
        rlEnd();
    }
    if (hash % 19U == 0U)
    {
        DrawSphereEx({x + 0.16f, 0.035f, z - 0.12f}, 0.055f, 4, 6,
                     Color{105, 103, 91, 6});
    }
}

void drawSteelTile(int row, int column, bool permanent,
                   bool shadowPass = false)
{
    // Cast armored redoubts: broad chamfers, a heavy lid and unmistakable
    // dark embrasures. The one-cell collision footprint remains unchanged.
    using tanks3d::base_model::detail::armoredBlock;
    const float x = column + 0.5f;
    const float z = row + 0.5f;
    const auto paint = [shadowPass](Color color) {
        return shadowPass ? WHITE : materialColor(color, 7);
    };
    const Color dark = paint({39, 53, 51, 255});
    const Color body = paint(permanent ? Color{131, 128, 98, 255}
                                      : Color{104, 139, 130, 255});
    const Color light = paint(permanent ? Color{186, 174, 132, 255}
                                       : Color{162, 182, 151, 255});
    const Color edge = paint({75, 102, 92, 255});
    const Color ochre = paint({224, 167, 66, 255});
    armoredBlock({x, 0.09f, z}, {0.98f, 0.18f, 0.98f}, dark);
    armoredBlock({x, 0.40f, z}, {0.90f, 0.56f, 0.90f}, body);
    armoredBlock({x, 0.70f, z}, {0.98f, 0.15f, 0.98f}, light);
    DrawCylinder({x - 0.06f, 0.774f, z - 0.045f}, 0.23f, 0.25f,
                 0.055f, 12, edge);
    DrawCylinder({x - 0.06f, 0.83f, z - 0.045f}, 0.19f, 0.20f,
                 0.025f, 12, body);
    if (shadowPass)
        return;
    DrawCube({x - 0.06f, 0.873f, z - 0.045f}, 0.13f, 0.04f, 0.035f, dark);
    for (int face = 0; face < 4; ++face)
    {
        rlPushMatrix();
        rlTranslatef(x, 0, z);
        rlRotatef(face*90.0f, 0, 1, 0);
        DrawCube({0, 0.46f, 0.451f}, 0.59f, 0.20f, 0.038f, dark);
        DrawCube({0, 0.55f, 0.472f}, 0.67f, 0.065f, 0.08f, light);
        DrawCube({0, 0.365f, 0.472f}, 0.64f, 0.055f, 0.075f, edge);
        DrawCube({0, 0.45f, 0.478f}, 0.040f, 0.12f, 0.025f, body);
        for (float side : {-1.0f, 1.0f})
        {
            DrawCube({side*0.33f, 0.32f, 0.465f}, 0.09f, 0.24f, 0.07f, edge);
            DrawSphereEx({side*0.33f, 0.36f, 0.51f}, 0.029f, 4, 6, light);
            DrawCube({side*0.21f, 0.20f, 0.458f}, 0.13f, 0.08f, 0.019f, ochre);
        }
        rlPopMatrix();
    }
}

unsigned char forestEdgeMask(const StageMap &map, int row, int column)
{
    unsigned char mask = 0U;
    if (map.tile(row - 1, column) != '%')
        mask |= EnvironmentAssets::kForestEdgeNorth;
    if (map.tile(row, column + 1) != '%')
        mask |= EnvironmentAssets::kForestEdgeEast;
    if (map.tile(row + 1, column) != '%')
        mask |= EnvironmentAssets::kForestEdgeSouth;
    if (map.tile(row, column - 1) != '%')
        mask |= EnvironmentAssets::kForestEdgeWest;
    return mask;
}

// Reject only terrain cells entirely beyond the orthographic image. The
// generous cell bounds include roofs, foliage overhang and facade trim;
// shadows still use the complete arena so offscreen casters remain visible.
class TerrainView
{
public:
    TerrainView() = default;

    TerrainView(const Camera3D &camera, int width, int height)
    {
        const Vector3 forward = Vector3Subtract(camera.target, camera.position);
        const Vector3 cross = Vector3CrossProduct(forward, camera.up);
        if (camera.projection != CAMERA_ORTHOGRAPHIC || width <= 0 ||
            height <= 0 || !std::isfinite(camera.fovy) || camera.fovy <= 0.0f ||
            Vector3LengthSqr(forward) < 0.000001f ||
            Vector3LengthSqr(cross) < 0.000001f)
            return;

        right_ = Vector3Normalize(cross);
        up_ = Vector3CrossProduct(right_, Vector3Normalize(forward));
        center_ = camera.target;
        halfWidth_ = camera.fovy * 0.5f * static_cast<float>(width) / height;
        halfHeight_ = camera.fovy * 0.5f;
        enabled_ = true;
    }

    bool containsCell(int row, int column) const
    {
        if (!enabled_)
            return true;
        // Includes the authored [-0.10, +1.10] forest extent and 1.44 m
        // canopy cap, with additional room for trim and numerical boundaries.
        const Vector3 delta{column + 0.5f - center_.x,
                            0.78f - center_.y,
                            row + 0.5f - center_.z};
        const auto intersects = [&](Vector3 axis, float halfSpan) {
            const float radius = std::fabs(axis.x) * 0.70f +
                                 std::fabs(axis.y) * 0.90f +
                                 std::fabs(axis.z) * 0.70f;
            return std::fabs(Vector3DotProduct(delta, axis)) <=
                   halfSpan + radius;
        };
        return intersects(right_, halfWidth_) && intersects(up_, halfHeight_);
    }

private:
    Vector3 center_{};
    Vector3 right_{};
    Vector3 up_{};
    float halfWidth_ = 0.0f;
    float halfHeight_ = 0.0f;
    bool enabled_ = false;
};

void drawTerrain(const StageMap &map, const EnvironmentAssets &environment,
                 const TerrainView &view = {})
{
    // Continuous player tracking can frame beyond the 26x26 collision arena.
    // A darker textured apron keeps the view grounded without disguising the
    // raised boundary or adding playable terrain.
    environment.drawArenaApron();
    environment.drawArenaGround();

    for (int row = 0; row < kMapSize; ++row)
    {
        for (int column = 0; column < kMapSize; ++column)
        {
            if (!view.containsCell(row, column) ||
                isGovernmentWallCell(row, column))
                continue;
            const char value = map.tile(row, column);
            if (value == '.')
                drawGroundDetail(row, column);
            if (value == '#')
            {
                drawBrickTile(map, environment, row, column);
            }
            else if (value == '@')
            {
                drawSteelTile(row, column, false);
            }
            else if (value == '~')
            {
                const float wave = std::sin(static_cast<float>(GetTime()) * 1.6f + row * 0.7f + column * 0.5f);
                const Color water = materialColor(Color{49, 125, 123, 255}, 7);
                const Color foam = materialColor(Color{155, 195, 159, 255}, 7);
                // Joined water cells form a single pool, edged with a narrow
                // worn bank only where the simulation's water really ends.
                DrawCube({column + 0.5f, -0.015f, row + 0.5f},
                         1.0f, 0.05f, 1.0f, water);
                DrawCube({column + 0.45f + wave*0.05f, 0.018f, row + 0.32f},
                         0.42f, 0.008f, 0.024f, foam);
                DrawCube({column + 0.65f - wave*0.03f, 0.017f, row + 0.68f},
                         0.19f, 0.006f, 0.018f, foam);
                const Color bank = materialColor(Color{123, 116, 78, 255}, 8);
                for (int side = -1; side <= 1; side += 2)
                {
                    if (map.tile(row + side, column) != '~')
                        DrawCube({column + 0.5f, 0.01f, row + 0.5f + side*0.48f},
                                 1.0f, 0.07f, 0.04f, bank);
                    if (map.tile(row, column + side) != '~')
                        DrawCube({column + 0.5f + side*0.48f, 0.01f, row + 0.5f},
                                 0.04f, 0.07f, 1.0f, bank);
                }
            }
            else if (value == '-')
            {
                DrawCube({column + 0.5f, -0.005f, row + 0.5f}, 1.0f, 0.04f, 1.0f, Color{166, 209, 206, 235});
                DrawLine3D({column + 0.18f, 0.025f, row + 0.75f},
                           {column + 0.82f, 0.025f, row + 0.25f}, Color{225, 252, 255, 210});
            }
            else if (value == '%')
            {
                // Opaque trunks and branches receive normal world lighting.
                // Their translucent crowns remain in the foreground pass,
                // preserving the classic second-layer cover rule.
                drawGroundDetail(row, column);
                environment.drawForestStructure(
                    map.stage(), row, column,
                    forestEdgeMask(map, row, column));
            }
        }
    }

    const Color boundary = materialColor(Color{42, 47, 43, 255}, 7);
    DrawCube({-0.08f, 0.19f, 13.0f}, 0.16f, 0.38f, 26.3f, boundary);
    DrawCube({26.08f, 0.19f, 13.0f}, 0.16f, 0.38f, 26.3f, boundary);
    DrawCube({13.0f, 0.19f, -0.08f}, 26.3f, 0.38f, 0.16f, boundary);
    DrawCube({13.0f, 0.19f, 26.08f}, 26.3f, 0.38f, 0.16f, boundary);
}

struct ForestDrawCell
{
    int row = 0;
    int column = 0;
    float depth = 0.0f;
};

std::vector<ForestDrawCell> forestDrawOrder(const StageMap &map,
                                            int cameraYawDegrees)
{
    const CameraPlanarBasis basis =
        cameraPlanarBasis(cameraYawDegrees);
    std::vector<ForestDrawCell> cells;
    cells.reserve(kMapSize * kMapSize);
    for (int row = 0; row < kMapSize; ++row)
    {
        for (int column = 0; column < kMapSize; ++column)
        {
            if (map.tile(row, column) != '%')
                continue;
            cells.push_back({
                row, column,
                (static_cast<float>(column) + 0.5f) * basis.offsetX +
                    (static_cast<float>(row) + 0.5f) * basis.offsetZ});
        }
    }
    std::stable_sort(cells.begin(), cells.end(),
                     [](const ForestDrawCell &left,
                        const ForestDrawCell &right) {
                         return left.depth < right.depth;
                     });
    return cells;
}

void drawForestForeground(const StageMap &map,
                          const EnvironmentAssets &environment,
                          int cameraYawDegrees,
                          const TerrainView &view = {})
{
    BeginBlendMode(BLEND_ALPHA);
    rlDrawRenderBatchActive();
    rlDisableDepthMask();
    // Alpha canopies must be submitted far-to-near along the selected camera
    // azimuth. Stable row-major ties keep the result deterministic.
    for (const ForestDrawCell &cell :
         forestDrawOrder(map, cameraYawDegrees))
    {
        if (!view.containsCell(cell.row, cell.column))
            continue;
        environment.drawForestCanopy(
            map.stage(), cell.row, cell.column,
            forestEdgeMask(map, cell.row, cell.column));
    }
    rlDrawRenderBatchActive();
    rlEnableDepthMask();
    EndBlendMode();
}

void drawBase(const StageMap &map, bool alive, bool shadowPass = false)
{
    tanks3d::base_model::draw(map, alive, shadowPass);
}

void drawTankContactShadow(XZ position, float yaw, bool enemy, int identity,
                           Nation nation = Nation::UnitedStates, int armor = 0)
{
    const auto vehicle = enemy ? wwii_tank_model::enemyVehicle(identity)
                               : wwii_tank_model::playerVehicle(nation, armor);
    const auto spec = wwii_tank_model::detail::arcadeVehicleSpec(vehicle);
    const auto art = wwii_tank_model::detail::arcadeVisualProfile(spec);
    const Color outer{12, 17, 12, 38};
    const Color inner{12, 17, 12, 64};
    rlPushMatrix();
    rlTranslatef(position.x, 0.0f, position.z);
    rlRotatef(-yaw * RAD2DEG, 0.0f, 1.0f, 0.0f);

    const auto ellipse = [](float x, float z, float radiusX, float radiusZ,
                            float y, Color color) {
        rlPushMatrix();
        rlTranslatef(x, 0.0f, z);
        rlScalef(radiusX, 1.0f, radiusZ);
        DrawCylinder({0.0f, y, 0.0f}, 1.0f, 1.0f, 0.012f, 18, color);
        rlPopMatrix();
    };

    if (spec.wheeled)
    {
        const float wheelRadius = art.trackHeight * 0.43f;
        const float axleZ = art.trackLength * 0.5f - wheelRadius - 0.015f;
        for (float side : {-1.0f, 1.0f})
        {
            for (float wheelZ : {-axleZ, 0.0f, axleZ})
            {
                ellipse(side * art.trackHalfWidth, wheelZ,
                        art.trackWidth * 0.5f + 0.035f, wheelRadius + 0.025f,
                        0.004f, outer);
                ellipse(side * art.trackHalfWidth, wheelZ,
                        art.trackWidth * 0.44f, wheelRadius * 0.72f,
                        0.006f, inner);
            }
        }
        ellipse(0.0f, -0.015f, art.hullWidth * 0.44f,
                art.hullLength * 0.44f, 0.005f, outer);
    }
    else
    {
        // The soft edge follows each complete belt; the denser center follows
        // its lower straight run. T95's two belts share this same side envelope.
        const float contactHalfLength =
            (art.trackLength - art.trackHeight) * 0.5f + 0.035f;
        for (float side : {-1.0f, 1.0f})
        {
            ellipse(side * art.trackHalfWidth, 0.0f,
                    art.trackWidth * 0.5f + 0.035f,
                    art.trackLength * 0.5f + 0.025f,
                    0.004f, outer);
            ellipse(side * art.trackHalfWidth, 0.0f,
                    art.trackWidth * 0.46f, contactHalfLength,
                    0.006f, inner);
        }
        ellipse(0.0f, -0.015f, art.hullWidth * 0.44f,
                art.hullLength * 0.46f, 0.004f, outer);
        ellipse(0.0f, -0.015f, art.hullWidth * 0.35f,
                art.hullLength * 0.37f, 0.006f, inner);
    }
    rlPopMatrix();
}

void drawGltfProbeFootprint(XZ position, Color color)
{
    const Color ring{color.r, color.g, color.b, 230};
    DrawCircle3D({position.x, 0.022f, position.z}, kTankRadius,
                 {1.0f, 0.0f, 0.0f}, 90.0f, ring);
    DrawLine3D({position.x - 0.12f, 0.024f, position.z},
               {position.x + 0.12f, 0.024f, position.z}, ring);
    DrawLine3D({position.x, 0.024f, position.z - 0.12f},
               {position.x, 0.024f, position.z + 0.12f}, ring);
}

void drawBoatFloatation(XZ position, float yaw)
{
    const Color pontoon{207, 104, 35, 1};
    const Color rubber{48, 52, 49, 3};
    rlPushMatrix();
    rlTranslatef(position.x, 0.0f, position.z);
    rlRotatef(-yaw * RAD2DEG, 0.0f, 1.0f, 0.0f);
    for (float side : {-0.76f, 0.76f})
    {
        DrawCylinderEx({side, 0.22f, -0.72f}, {side, 0.22f, 0.72f},
                       0.14f, 0.14f, 12, pontoon);
        DrawCylinderEx({side, 0.22f, -0.76f}, {side, 0.22f, -0.66f},
                       0.07f, 0.14f, 12, pontoon);
        DrawCylinderEx({side, 0.22f, 0.66f}, {side, 0.22f, 0.76f},
                       0.14f, 0.07f, 12, pontoon);
    }
    DrawCube({0.0f, 0.19f, -0.48f}, 1.45f, 0.08f, 0.07f, rubber);
    DrawCube({0.0f, 0.19f, 0.48f}, 1.45f, 0.08f, 0.07f, rubber);
    rlPopMatrix();
}

// Buildings retain compact shadow casters, while vehicles reuse the exact
// visible geometry. This keeps model-specific tread, muzzle, attachment, and
// suspension silhouettes synchronized with the sun shadow.
void drawShadowCasters(const Game3D &game, TankAssets &tankAssets)
{
    const StageMap &map = game.map();
    for (int row = 0; row < kMapSize; ++row)
    {
        for (int column = 0; column < kMapSize; ++column)
        {
            if (isGovernmentWallCell(row, column))
                continue;
            const char tile = map.tile(row, column);
            if (tile == '#')
            {
                EnvironmentAssets::drawUrbanBuildingShadow(
                    map.stage(), row, column, map.brickMask(row, column));
            }
            else if (tile == '@')
                drawSteelTile(row, column, false, true);
            else if (tile == '%')
            {
                EnvironmentAssets::drawForestShadow(
                    map.stage(), row, column,
                    forestEdgeMask(map, row, column));
            }
        }
    }

    drawBase(map, game.baseAlive(), true);

    for (const Player &player : game.players())
    {
        if (player.active)
        {
            drawTankModel(tankAssets, player.position, player.yaw,
                          playerColor(player.id), false, player.level, 0.0f,
                          player.id, player.moving, player.nation, true);
            if (player.hasBoat)
                drawBoatFloatation(player.position, player.yaw);
        }
    }
    for (const Enemy &enemy : game.enemies())
    {
        if (enemy.destroyed || enemy.creationTimer > 0.0f)
            continue;
        drawTankModel(tankAssets, enemy.position, enemy.yaw,
                      enemyArmorColor(enemy.armor), true, enemy.armor, 0.0f,
                      enemy.type, enemy.moving, Nation::Germany, true);
    }
    tankAssets.flushQueued(true);
}

void drawWorld(const Game3D &game, TankAssets &tankAssets,
               const EnvironmentAssets &environment,
               const TerrainView &view = {})
{
    environment.drawBackdropCity();
    drawTerrain(game.map(), environment, view);
    drawBase(game.map(), game.baseAlive());

    for (const Player &player : game.players())
    {
        if (player.active)
        {
            drawTankContactShadow(player.position, player.yaw, false,
                                  player.id, player.nation, player.level);
            if (tankAssets.gltfProbeEnabled())
                drawGltfProbeFootprint(player.position,
                                       Color{255, 220, 72, 255});
            drawTankModel(tankAssets, player.position, player.yaw, playerColor(player.id),
                          false, player.level, player.shieldTimer, player.id,
                          player.moving, player.nation);
            if (player.hasBoat)
                drawBoatFloatation(player.position, player.yaw);
        }
        else if (player.lives > 0)
        {
            const XZ spawn = playerSpawn(player.id);
            DrawCylinder({spawn.x, 0.02f, spawn.z}, 0.45f, 0.45f, 0.05f, 20,
                         Fade(playerColor(player.id), 0.55f));
        }
    }

    for (const Enemy &enemy : game.enemies())
    {
        if (enemy.destroyed || enemy.creationTimer > 0.0f)
            continue;
        Color body = enemyArmorColor(enemy.armor);
        if (enemy.carriesBonus)
        {
            const float pulse = 0.62f + 0.18f *
                std::sin(static_cast<float>(GetTime()) * 9.0f + enemy.id);
            body = ColorLerp(body, Color{255, 89, 35, 255}, pulse);
        }
        drawTankContactShadow(enemy.position, enemy.yaw, true, enemy.type,
                              Nation::Germany, enemy.armor);
        if (tankAssets.gltfProbeEnabled())
            drawGltfProbeFootprint(enemy.position,
                                   Color{255, 86, 72, 255});
        drawTankModel(tankAssets, enemy.position, enemy.yaw, body,
                      true, enemy.armor, 0.0f, enemy.type,
                      enemy.moving,
                      Nation::Germany);
        if (enemy.frozenTimer > 0.0f)
        {
            DrawSphereWires({enemy.position.x, 0.58f, enemy.position.z},
                            0.92f, 8, 12, Color{118, 220, 255, 185});
        }
        if (game.showTargets())
        {
            DrawLine3D({enemy.position.x, 0.86f, enemy.position.z},
                       {enemy.target.x, 0.25f, enemy.target.z}, Color{255, 95, 83, 210});
        }
    }
    tankAssets.flushQueued(false);
}

Vector3 creationStarPoint(Vector3 center, Vector3 right, Vector3 up,
                          float angle, float radius)
{
    const float horizontal = std::cos(angle) * radius;
    const float vertical = std::sin(angle) * radius;
    return {center.x + right.x * horizontal + up.x * vertical,
            center.y + right.y * horizontal + up.y * vertical,
            center.z + right.z * horizontal + up.z * vertical};
}

void drawCreationStarPlane(Vector3 center, Vector3 right, float outerRadius,
                           Color color)
{
    constexpr int pointCount = 16;
    constexpr Vector3 up{0.0f, 1.0f, 0.0f};
    for (int point = 0; point < pointCount; ++point)
    {
        const float firstAngle = -kPi * 0.5f +
                                 static_cast<float>(point) * 2.0f * kPi /
                                     static_cast<float>(pointCount);
        const float secondAngle = -kPi * 0.5f +
                                  static_cast<float>(point + 1) * 2.0f * kPi /
                                      static_cast<float>(pointCount);
        const float firstRadius = (point % 2 == 0)
                                      ? outerRadius
                                      : outerRadius * 0.34f;
        const float secondRadius = ((point + 1) % 2 == 0)
                                       ? outerRadius
                                       : outerRadius * 0.34f;
        const Vector3 first = creationStarPoint(center, right, up,
                                                firstAngle, firstRadius);
        const Vector3 second = creationStarPoint(center, right, up,
                                                 secondAngle, secondRadius);
        DrawTriangle3D(center, first, second, color);
        DrawLine3D(first, second, Fade(RAYWHITE, 0.78f));
    }
}

void drawHorizontalWarningRing(Vector3 center, float radius, Color color)
{
    constexpr int segments = 32;
    for (int segment = 0; segment < segments; ++segment)
    {
        const float firstAngle = static_cast<float>(segment) * 2.0f * kPi /
                                 static_cast<float>(segments);
        const float secondAngle = static_cast<float>(segment + 1) * 2.0f * kPi /
                                  static_cast<float>(segments);
        DrawLine3D({center.x + std::cos(firstAngle) * radius, center.y,
                    center.z + std::sin(firstAngle) * radius},
                   {center.x + std::cos(secondAngle) * radius, center.y,
                    center.z + std::sin(secondAngle) * radius},
                   color);
    }
}

void drawEnemyCreationWarnings(const Game3D &game, Camera3D camera)
{
    BeginBlendMode(BLEND_ADDITIVE);
    rlDisableDepthMask();
    for (const Enemy &enemy : game.enemies())
    {
        if (enemy.destroyed)
            continue;
        const int frame = enemyCreationFrame(enemy.creationTimer);
        if (frame < 0)
            continue;
        const float scale = enemyCreationScale(frame);
        const unsigned char alpha = static_cast<unsigned char>(
            frame % 2 == 0 ? 178 : 245);
        const Vector3 center{enemy.position.x, 0.67f, enemy.position.z};
        const Vector3 horizontalView{camera.position.x - center.x, 0.0f,
                                     camera.position.z - center.z};
        const float viewLength = std::max(
            0.0001f, std::sqrt(horizontalView.x * horizontalView.x +
                               horizontalView.z * horizontalView.z));
        const Vector3 screenRight{horizontalView.z / viewLength, 0.0f,
                                  -horizontalView.x / viewLength};
        const float starRadius = 0.20f + scale * 0.38f;
        drawCreationStarPlane(center, screenRight, starRadius,
                              Color{255, 58, 202, alpha});
        drawCreationStarPlane(center, screenRight, starRadius * 0.54f,
                              Color{255, 246, 187, alpha});
        DrawSphere(center, 0.055f + scale * 0.045f,
                   Color{255, 255, 226, alpha});

        const Vector3 ground{enemy.position.x, 0.055f, enemy.position.z};
        const float ringRadius = 0.42f + scale * 0.18f;
        drawHorizontalWarningRing(ground, ringRadius,
                                  Color{255, 82, 49, alpha});
        drawHorizontalWarningRing({ground.x, ground.y + 0.012f, ground.z},
                                  ringRadius * 0.72f,
                                  Color{255, 224, 74,
                                        static_cast<unsigned char>(alpha * 0.78f)});
        DrawCylinderEx({ground.x, 0.08f, ground.z},
                       {center.x, center.y - starRadius * 0.42f, center.z},
                       0.025f, 0.070f, 10,
                       Color{255, 113, 190,
                             static_cast<unsigned char>(alpha * 0.42f)});
        for (int spark = 0; spark < 4; ++spark)
        {
            const float angle = static_cast<float>(spark) * kPi * 0.5f +
                                static_cast<float>(frame) * 0.31f;
            const float distance = 0.23f + scale * 0.16f;
            DrawSphere({center.x + std::cos(angle) * distance,
                        center.y - 0.13f + (spark % 2) * 0.23f,
                        center.z + std::sin(angle) * distance},
                       0.025f + scale * 0.012f,
                       Color{255, 245, 177, alpha});
        }
    }
    rlDrawRenderBatchActive();
    rlEnableDepthMask();
    EndBlendMode();
}

void drawEmissiveBattleFx(const Game3D &game)
{
    // The physical penetrator is deliberately readable without bloom. Its
    // gameplay AABB remains the classic half-tile sprite footprint, while this
    // smaller visible body avoids looking like a glowing ball.
    BeginBlendMode(BLEND_ALPHA);
    for (const Shell &shell : game.shells())
    {
        if (shell.impacting)
            continue;
        const float velocityLength = std::max(0.001f, std::sqrt(lengthSquared(shell.velocity)));
        const XZ direction = shell.velocity * (1.0f / velocityLength);
        const float radius = shell.power ? 0.090f : 0.068f;
        const float bodyLength = shell.power ? 0.30f : 0.23f;
        const Vector3 nose{shell.position.x, 0.67f, shell.position.z};
        const Vector3 base{shell.position.x - direction.x * bodyLength,
                           0.67f,
                           shell.position.z - direction.z * bodyLength};
        DrawCylinderEx(base, nose, radius, radius * 0.54f, 9,
                       shell.owner == ShellOwner::Player
                           ? Color{167, 137, 73, 255}
                           : Color{126, 129, 125, 255});
        const Vector3 bandRear{
            shell.position.x - direction.x * bodyLength * 0.82f, 0.67f,
            shell.position.z - direction.z * bodyLength * 0.82f};
        const Vector3 bandFront{
            shell.position.x - direction.x * bodyLength * 0.66f, 0.67f,
            shell.position.z - direction.z * bodyLength * 0.66f};
        DrawCylinderEx(bandRear, bandFront, radius * 1.06f,
                       radius * 1.06f, 9, Color{82, 73, 53, 245});
        DrawSphere(nose, radius * 0.72f, Color{236, 211, 137, 255});
    }
    EndBlendMode();

    BeginBlendMode(BLEND_ADDITIVE);
    for (const Shell &shell : game.shells())
    {
        if (shell.impacting)
            continue;
        const Color color = shell.owner == ShellOwner::Player
                                ? Color{255, 178, 54, 255}
                                : Color{255, 104, 43, 255};
        const float velocityLength = std::max(0.001f, std::sqrt(lengthSquared(shell.velocity)));
        const XZ direction = shell.velocity * (1.0f / velocityLength);
        const float bodyLength = shell.power ? 0.30f : 0.23f;
        const Vector3 flameBase{shell.position.x - direction.x * bodyLength,
                                0.67f,
                                shell.position.z - direction.z * bodyLength};
        const Vector3 hotTail{flameBase.x - direction.x * (shell.power ? 0.34f : 0.26f),
                              0.67f,
                              flameBase.z - direction.z * (shell.power ? 0.34f : 0.26f)};
        const Vector3 tracerTail{hotTail.x - direction.x * (shell.power ? 0.40f : 0.30f),
                                 0.67f,
                                 hotTail.z - direction.z * (shell.power ? 0.40f : 0.30f)};

        DrawSphere({shell.position.x, 0.67f, shell.position.z},
                   shell.power ? 0.125f : 0.095f, Fade(color, 0.48f));
        DrawCylinderEx(hotTail, flameBase, 0.007f,
                       shell.power ? 0.078f : 0.060f, 9, Fade(color, 0.92f));
        const Vector3 coreTail{hotTail.x + direction.x * 0.07f, 0.67f,
                               hotTail.z + direction.z * 0.07f};
        DrawCylinderEx(coreTail, flameBase, 0.004f,
                       shell.power ? 0.038f : 0.028f, 8,
                       Color{255, 249, 200, 230});
        DrawCylinderEx(tracerTail, hotTail, 0.002f, 0.017f, 7,
                       Fade(color, 0.36f));
    }
    EndBlendMode();
    game.effects().draw();

    BeginBlendMode(BLEND_ADDITIVE);
    const float time = static_cast<float>(GetTime());
    for (const Enemy &enemy : game.enemies())
    {
        if (enemy.destroyed || !enemy.carriesBonus ||
            enemy.creationTimer > 0.0f)
            continue;
        const float pulse = 0.5f + 0.5f * std::sin(time * 8.0f + enemy.position.x);
        const Vector3 beacon{enemy.position.x, 1.24f + pulse * 0.05f,
                             enemy.position.z};
        DrawSphere(beacon, 0.075f + pulse * 0.025f,
                   Color{255, 215, 64, 210});
        DrawCircle3D(beacon, 0.22f + pulse * 0.04f,
                     {1.0f, 0.0f, 0.0f}, 90.0f,
                     Color{255, 112, 36, 125});
    }
    EndBlendMode();
}

Color miniMapTileColor(char value, bool permanent)
{
    switch (value)
    {
    case '#': return Color{149, 63, 45, 255};
    case '@': return permanent ? Color{214, 235, 243, 255} : Color{132, 151, 162, 255};
    case '%': return Color{36, 116, 50, 255};
    case '~': return Color{34, 118, 190, 255};
    case '-': return Color{159, 217, 227, 255};
    default: return Color{55, 61, 54, 255};
    }
}

void drawMiniMapSpawnStar(Vector2 center, float outerRadius, Color color)
{
    constexpr int pointCount = 10;
    for (int point = 0; point < pointCount; ++point)
    {
        const float firstAngle = -kPi * 0.5f +
                                 static_cast<float>(point) * 2.0f * kPi /
                                     static_cast<float>(pointCount);
        const float secondAngle = -kPi * 0.5f +
                                  static_cast<float>(point + 1) * 2.0f * kPi /
                                      static_cast<float>(pointCount);
        const float firstRadius = point % 2 == 0
                                      ? outerRadius
                                      : outerRadius * 0.38f;
        const float secondRadius = (point + 1) % 2 == 0
                                       ? outerRadius
                                       : outerRadius * 0.38f;
        DrawTriangle(center,
                     {center.x + std::cos(firstAngle) * firstRadius,
                      center.y + std::sin(firstAngle) * firstRadius},
                     {center.x + std::cos(secondAngle) * secondRadius,
                      center.y + std::sin(secondAngle) * secondRadius},
                     color);
    }
}

void drawMiniMap(const Game3D &game, int originX, int originY, int cellSize)
{
    const int size = kMapSize * cellSize;
    DrawRectangle(originX - 4, originY - 4, size + 8, size + 8, Color{7, 10, 12, 205});
    for (int row = 0; row < kMapSize; ++row)
    {
        for (int column = 0; column < kMapSize; ++column)
        {
            const int x = originX + column * cellSize;
            const int y = originY + row * cellSize;
            const char tile = game.map().tile(row, column);
            if (tile == '#' && game.map().brickMask(row, column) != 0x0fU)
            {
                DrawRectangle(x, y, cellSize, cellSize,
                              miniMapTileColor('.', false));
                const unsigned char mask = game.map().brickMask(row, column);
                const int half = std::max(1, cellSize / 2);
                for (int quadrant = 0; quadrant < 4; ++quadrant)
                {
                    if ((mask & (1U << quadrant)) == 0U)
                        continue;
                    DrawRectangle(x + ((quadrant & 1) != 0 ? half : 0),
                                  y + ((quadrant & 2) != 0 ? half : 0),
                                  cellSize - ((quadrant & 1) != 0 ? half : cellSize - half),
                                  cellSize - ((quadrant & 2) != 0 ? half : cellSize - half),
                                  miniMapTileColor('#', false));
                }
            }
            else
            {
                DrawRectangle(x, y, cellSize, cellSize,
                              miniMapTileColor(tile, false));
            }
        }
    }
    for (int index = 0; index < kGovernmentWallCount; ++index)
    {
        if (game.map().governmentWallHealth(index) <= 0)
            continue;
        const GovernmentWallSegment segment = governmentWallSegment(index);
        const XZ visibleStart = segment.center -
                                segment.along * segment.halfLength;
        const XZ visibleEnd = segment.center +
                              segment.along * segment.halfLength;
        const Vector2 start{
            originX + visibleStart.x * cellSize,
            originY + visibleStart.z * cellSize};
        const Vector2 end{
            originX + visibleEnd.x * cellSize,
            originY + visibleEnd.z * cellSize};
        const float healthRatio = static_cast<float>(
            game.map().governmentWallHealth(index)) /
            static_cast<float>(kGovernmentWallMaximumHealth);
        const Color wallColor = game.map().governmentWallsSteel()
                                    ? Color{139, 218, 235, 255}
                                    : ColorLerp(Color{174, 88, 54, 255},
                                                Color{228, 219, 184, 255},
                                                healthRatio);
        DrawLineEx(start, end,
                   std::max(2.0f, cellSize * kGovernmentWallThickness),
                   wallColor);
    }
    DrawRectangle(originX + static_cast<int>((kGovernmentBaseCenter.x - kGovernmentCoreHalfSize) * cellSize),
               originY + static_cast<int>((kGovernmentBaseCenter.z - kGovernmentCoreHalfSize) * cellSize),
               static_cast<int>(2.0f * kGovernmentCoreHalfSize * cellSize),
               static_cast<int>(2.0f * kGovernmentCoreHalfSize * cellSize),
               game.baseAlive()
                   ? game.baseNation() == Nation::SovietUnion
                         ? Color{222, 65, 54, 255}
                         : game.baseNation() == Nation::Germany
                               ? Color{180, 164, 135, 255}
                               : GOLD
                   : DARKGRAY);
    for (const Pickup &pickup : game.bonuses())
    {
        if (!bonus_assets::visible(pickup))
            continue;
        const Vector2 center{
            originX + pickup.position.x * cellSize,
            originY + pickup.position.z * cellSize};
        const float pulse = 0.82f + 0.18f * std::sin(pickup.age * 9.0f);
        const float radius = std::max(4.5f, cellSize * 1.08f * pulse);
        const Color markerColor = bonus_assets::accent(pickup.type);
        DrawCircleLines(static_cast<int>(center.x),
                        static_cast<int>(center.y), radius,
                        markerColor);
        drawMiniMapSpawnStar(center, radius,
                             Color{255, 213, 54, 245});
        drawMiniMapSpawnStar(center, radius * 0.58f,
                             Color{255, 252, 211, 255});
    }
    for (const Enemy &enemy : game.enemies())
    {
        if (enemy.destroyed)
            continue;
        if (enemy.creationTimer > 0.0f)
        {
            const int frame = enemyCreationFrame(enemy.creationTimer);
            const float scale = enemyCreationScale(frame);
            const Vector2 center{
                originX + enemy.position.x * cellSize,
                originY + enemy.position.z * cellSize};
            const float radius = std::max(2.5f,
                cellSize * (0.45f + scale * 0.28f));
            DrawCircleLines(static_cast<int>(center.x),
                            static_cast<int>(center.y), radius,
                            frame % 2 == 0 ? Color{255, 85, 205, 255}
                                           : Color{255, 224, 74, 255});
            drawMiniMapSpawnStar(center, radius * 0.72f,
                                 Color{255, 245, 198, 235});
            continue;
        }
        DrawCircle(originX + static_cast<int>(enemy.position.x * cellSize),
                   originY + static_cast<int>(enemy.position.z * cellSize),
                   std::max(2.0f, cellSize * 0.72f),
                   enemy.carriesBonus ? GOLD : RED);
    }
    for (const Player &player : game.players())
    {
        if (!player.active)
            continue;
        DrawCircle(originX + static_cast<int>(player.position.x * cellSize),
                   originY + static_cast<int>(player.position.z * cellSize),
                   std::max(2.0f, cellSize * 0.78f), playerColor(player.id));
    }
    DrawRectangleLines(originX - 1, originY - 1, size + 2, size + 2, Color{220, 228, 230, 255});
}

void drawTextShadow(const std::string &text, int x, int y, int fontSize, Color color)
{
    DrawText(text.c_str(), x + 2, y + 2, fontSize, Color{0, 0, 0, 190});
    DrawText(text.c_str(), x, y, fontSize, color);
}

void drawCenteredText(const std::string &text, int centerX, int y, int fontSize, Color color)
{
    drawTextShadow(text, centerX - MeasureText(text.c_str(), fontSize) / 2, y, fontSize, color);
}

int fittedFontSize(const std::string &text, int maximumWidth, int preferred,
                   int minimum)
{
    int fontSize = preferred;
    while (fontSize > minimum && MeasureText(text.c_str(), fontSize) > maximumWidth)
        --fontSize;
    return fontSize;
}

void drawStreakPopups(const Game3D &game, const Camera3D &camera,
                      int screenWidth, int screenHeight)
{
    for (const Player &player : game.players())
    {
        if (!player.active || player.directKillStreak < 2 ||
            player.streakPopupTimer <= 0.0f)
            continue;

        const float elapsed = std::clamp(
            kStreakPopupDuration - player.streakPopupTimer,
            0.0f, kStreakPopupDuration);
        const float progress = elapsed / kStreakPopupDuration;
        float scale = 1.0f;
        if (elapsed < 0.16f)
        {
            const float entry = elapsed / 0.16f;
            const float offset = entry - 1.0f;
            const float backEase = 1.0f + 2.70158f * offset * offset * offset +
                                   1.70158f * offset * offset;
            scale = 0.55f + 0.65f * backEase;
        }
        else
        {
            const float settle = std::clamp((elapsed - 0.16f) / 0.16f,
                                            0.0f, 1.0f);
            scale = 1.20f - 0.20f * settle;
        }

        const float fadeOut = std::clamp((1.0f - progress) / 0.24f,
                                         0.0f, 1.0f);
        // Two deliberate brightness flashes over the popup lifetime.
        const float flash = 0.5f + 0.5f * std::sin(
            elapsed * (4.0f * kPi / kStreakPopupDuration) + kPi * 0.5f);
        const float alpha = fadeOut * (0.70f + flash * 0.30f);
        const Color accent = playerColor(player.id);
        const Color face = Fade(ColorLerp(accent, WHITE,
                                          0.30f + flash * 0.38f), alpha);
        const Color glow = Fade(accent, alpha * (0.24f + flash * 0.22f));

        const std::string text = "STREAK " +
                                 std::to_string(player.directKillStreak);
        const int baseSize = screenHeight < 700 ? 28 : 34;
        const int fontSize = std::max(18, static_cast<int>(
            std::lround(static_cast<float>(baseSize) * scale)));
        const int textWidth = MeasureText(text.c_str(), fontSize);
        const Vector2 projected = GetWorldToScreenEx(
            {player.position.x, 1.48f, player.position.z}, camera,
            screenWidth, screenHeight);
        const int maximumX = std::max(12, screenWidth - textWidth - 12);
        const int minimumY = std::min(150, std::max(12, screenHeight - 82));
        const int maximumY = std::max(minimumY, screenHeight - 82);
        const int x = std::clamp(
            static_cast<int>(std::lround(projected.x)) - textWidth / 2,
            12, maximumX);
        const int y = std::clamp(
            static_cast<int>(std::lround(projected.y - 18.0f -
                                         progress * 30.0f)),
            minimumY, maximumY);

        for (const Vector2 offset : std::array<Vector2, 8>{{
                 {-3.0f, 0.0f}, {3.0f, 0.0f}, {0.0f, -3.0f}, {0.0f, 3.0f},
                 {-2.0f, -2.0f}, {2.0f, -2.0f}, {-2.0f, 2.0f}, {2.0f, 2.0f}}})
        {
            DrawText(text.c_str(), x + static_cast<int>(offset.x),
                     y + static_cast<int>(offset.y), fontSize, glow);
        }
        DrawText(text.c_str(), x + 3, y + 4, fontSize,
                 Fade(BLACK, alpha * 0.78f));
        DrawText(text.c_str(), x, y, fontSize, face);
    }
}

struct ViewportHudLayout
{
    int panelWidth = 0;
    int panelTextX = 0;
    int mapX = 0;
    int mapCellSize = 0;
    int footerCenterX = 0;
    int footerWidth = 0;
};

ViewportHudLayout viewportHudLayout(Rectangle viewport, int playerCount,
                                     int playerIndex)
{
    const int width = std::max(1, static_cast<int>(viewport.width));
    const int originX = static_cast<int>(viewport.x);
    const int count = std::clamp(playerCount, 1, 2);
    const int index = std::clamp(playerIndex, 0, count - 1);
    ViewportHudLayout layout;
    layout.mapCellSize = width < 700 ? 4 : 5;
    const int mapPixels = kMapSize * layout.mapCellSize;
    layout.mapX = count == 1 ? originX + width - mapPixels - 16
                            : originX + width / 2 - mapPixels / 2;
    // The minimap background extends four pixels beyond its tiles. Keep a
    // twelve-pixel gap to each panel, including its seven-pixel text inset.
    constexpr int mapPadding = 4;
    constexpr int panelGap = 12;
    const int spaceBeforeMap = layout.mapX - mapPadding - panelGap -
                               (originX + 7);
    const int spaceAfterMap = originX + width - 21 -
                              (layout.mapX + mapPixels + mapPadding +
                               panelGap);
    layout.panelWidth = std::max(1, std::min(
        390, count == 1 ? spaceBeforeMap
                       : std::min(spaceBeforeMap, spaceAfterMap)));
    layout.panelTextX = index == 0 ? originX + 14
                                  : originX + width - layout.panelWidth - 14;
    const int footerLeft = originX + width * index / count;
    const int footerRight = originX + width * (index + 1) / count;
    layout.footerCenterX = (footerLeft + footerRight) / 2;
    layout.footerWidth = std::max(1, footerRight - footerLeft - 28);
    return layout;
}

void drawViewportHud(const Game3D &game, int playerIndex, Rectangle viewport,
                     bool showFrameRate)
{
    const Player &player = game.players()[playerIndex];
    const Color accent = playerColor(playerIndex);
    const int width = static_cast<int>(viewport.width);
    const ViewportHudLayout layout = viewportHudLayout(
        viewport, game.playerCount(), playerIndex);
    const int panelWidth = layout.panelWidth;
    const int left = layout.panelTextX;
    const int top = static_cast<int>(viewport.y) + 12;
    const int mapY = top + 4;

    const std::string nationLine = "P" + std::to_string(playerIndex + 1) +
                                   "  " + nationName(player.nation);
    const bool drawFrameRate = showFrameRate && playerIndex == 0;
    const std::string fpsLine = drawFrameRate
                                    ? "FPS " + std::to_string(GetFPS())
                                    : std::string{};
    const std::string statusLine = "STAGE " + std::to_string(game.stage()) +
                                   "   LIVES " + std::to_string(std::max(0, player.lives)) +
                                   "   ENEMY " + std::to_string(game.enemiesLeft());
    const std::string vehicleLine =
        wwii_tank_model::playerVehicleName(player.nation, player.level);
    const std::string tierLine = "TIER " + std::to_string(player.level + 1) + "/4  " +
                                 wwii_tank_model::tierName(player.level) +
                                 "   SCORE " + std::to_string(player.score);
    const bool steelProtected = game.map().governmentWallsSteel();
    const bool streakActive = player.directKillStreak > 0;
    const int panelHeight = 129 + (streakActive ? 24 : 0) +
                            (steelProtected ? 24 : 0);
    DrawRectangle(left - 7, top - 5, panelWidth,
                  panelHeight, Color{5, 8, 11, 185});
    const int fpsFontSize = 15;
    const int fpsWidth = drawFrameRate
                             ? MeasureText(fpsLine.c_str(), fpsFontSize)
                             : 0;
    const int nationWidth = drawFrameRate
                                ? panelWidth - fpsWidth - 32
                                : panelWidth - 14;
    drawTextShadow(nationLine, left, top,
                   fittedFontSize(nationLine, nationWidth, 22, 14), accent);
    if (drawFrameRate)
    {
        drawTextShadow(fpsLine, left + panelWidth - fpsWidth - 14, top + 3,
                       fpsFontSize, Color{139, 218, 235, 255});
    }
    drawTextShadow(statusLine, left, top + 26,
                   fittedFontSize(statusLine, panelWidth - 14, 16, 11), RAYWHITE);
    drawTextShadow(vehicleLine, left, top + 49,
                   fittedFontSize(vehicleLine, panelWidth - 14, 16, 10),
                   player.level >= 3 ? GOLD : ORANGE);
    drawTextShadow(tierLine, left, top + 73,
                   fittedFontSize(tierLine, panelWidth - 14, 15, 10), LIGHTGRAY);
    const std::string hpLine = "HP  " +
                               std::to_string(std::max(0, player.hitPoints)) +
                               " / " + std::to_string(player.maximumHitPoints);
    const Color hpColor = player.hitPoints <= 0
                              ? Color{135, 142, 148, 255}
                          : player.hitPoints >= player.maximumHitPoints
                              ? Color{98, 232, 129, 255}
                          : player.hitPoints * 2 >= player.maximumHitPoints
                              ? Color{255, 207, 83, 255}
                              : Color{255, 99, 75, 255};
    drawTextShadow(hpLine, left, top + 96,
                   fittedFontSize(hpLine, panelWidth - 14, 16, 11), hpColor);
    int nextLineY = top + 120;
    if (streakActive)
    {
        const std::string streakLine = "STREAK  x" +
                                       std::to_string(player.directKillStreak);
        drawTextShadow(streakLine, left, nextLineY,
                       fittedFontSize(streakLine, panelWidth - 14, 16, 11),
                       ColorLerp(accent, WHITE, 0.30f));
        nextLineY += 24;
    }
    if (steelProtected)
    {
        std::ostringstream timer;
        timer << std::fixed << std::setprecision(1)
              << game.map().governmentSteelTimeRemaining();
        const std::string armorLine = "BASE STEEL  " + timer.str() + "s";
        drawTextShadow(armorLine, left, nextLineY,
                       fittedFontSize(armorLine, panelWidth - 14, 15, 10),
                       Color{139, 218, 235, 255});
    }
    if (playerIndex == 0)
        drawMiniMap(game, layout.mapX, mapY, layout.mapCellSize);

    const std::string movement = playerIndex == 0 ? "PAD 1 / ARROWS"
                                                 : "PAD 2 / WASD";
    const std::string fire = "BOTTOM/LEFT FACE OR R1/RT FIRE";
    const std::string controls = movement + "   " + fire;
    const int preferredFont = width < 700 ? 14 : 16;
    const int controlsFont = fittedFontSize(
        controls, layout.footerWidth, preferredFont, 12);
    const int footerY = static_cast<int>(viewport.y + viewport.height) - 28;
    const Color controlsColor{225, 230, 230, 225};
    if (MeasureText(controls.c_str(), controlsFont) <= layout.footerWidth)
    {
        drawCenteredText(controls, layout.footerCenterX, footerY,
                         controlsFont, controlsColor);
    }
    else
    {
        // Preserve every binding without letting either player's hints cross
        // the center line or the viewport edge in a narrow co-op window.
        drawCenteredText(movement, layout.footerCenterX,
                         footerY - preferredFont - 4,
                         fittedFontSize(movement, layout.footerWidth,
                                        preferredFont, 8),
                         controlsColor);
        drawCenteredText(fire, layout.footerCenterX, footerY,
                         fittedFontSize(fire, layout.footerWidth,
                                        preferredFont, 8),
                         controlsColor);
    }
}

void drawGltfProbeHud(const TankAssets &tankAssets, int screenWidth,
                      int screenHeight)
{
    if (!tankAssets.gltfProbeRequested())
        return;
    const int panelWidth = std::min(820, std::max(360, screenWidth - 80));
    const int left = (screenWidth - panelWidth) / 2;
    const int top = screenHeight - 112;
    const Color accent = tankAssets.gltfProbeEnabled()
                             ? Color{102, 236, 255, 255}
                             : Color{255, 98, 82, 255};
    DrawRectangleRounded({static_cast<float>(left), static_cast<float>(top),
                          static_cast<float>(panelWidth), 58.0f},
                         0.16f, 8, Color{4, 9, 12, 220});
    DrawRectangleRoundedLinesEx(
        {static_cast<float>(left), static_cast<float>(top),
         static_cast<float>(panelWidth), 58.0f},
        0.16f, 8, 2.0f, accent);
    const std::string status = tankAssets.gltfProbeStatus();
    drawCenteredText(status, screenWidth / 2, top + 7,
                     fittedFontSize(status, panelWidth - 24, 16, 10), accent);
    drawCenteredText("ISOLATED PIPELINE TEST - COLLISION, MUZZLE AND GAME RULES UNCHANGED",
                     screenWidth / 2, top + 32,
                     fittedFontSize("ISOLATED PIPELINE TEST - COLLISION, MUZZLE AND GAME RULES UNCHANGED",
                                    panelWidth - 24, 13, 9),
                     Color{222, 228, 226, 235});
}

struct ViewTargets
{
    // raylib 6.0 uses this positive metadata sentinel for a native depth
    // renderbuffer in LoadRenderTexture(). IsRenderTextureValid() requires the
    // field even though rlgl selects the actual GPU depth format internally.
    static constexpr int kRaylibDepthAttachmentFormat = 19;

    struct Operations
    {
        std::function<RenderTexture2D(int, int)> loadHdr{};
        std::function<RenderTexture2D(int, int)> loadFallback{};
        std::function<bool(const RenderTexture2D &)> valid{};
        std::function<void(const RenderTexture2D &)> configure{};
        std::function<void(RenderTexture2D)> unload{};
        std::function<void()> reportFallback{};

        bool complete() const
        {
            return static_cast<bool>(loadHdr) &&
                   static_cast<bool>(loadFallback) &&
                   static_cast<bool>(valid) &&
                   static_cast<bool>(configure) &&
                   static_cast<bool>(unload);
        }
    };

    ViewTargets() : ViewTargets(productionOperations()) {}
    explicit ViewTargets(Operations operations)
        : operations_(std::move(operations))
    {
    }
    ViewTargets(const ViewTargets &) = delete;
    ViewTargets &operator=(const ViewTargets &) = delete;

    std::array<RenderTexture2D, 2> targets{};
    std::array<int, 2> widths{};
    int height = 0;
    int count = 0;

    bool ensure(int requestedCount, int screenWidth, int screenHeight)
    {
        if ((requestedCount != 1 && requestedCount != 2) ||
            !operations_.complete())
            return false;
        const int leftWidth = requestedCount == 2 ? screenWidth / 2 : screenWidth;
        const int rightWidth = requestedCount == 2 ? screenWidth - leftWidth : 0;
        const std::array<int, 2> requestedWidths{{
            std::max(1, leftWidth), std::max(0, rightWidth)}};
        const int requestedHeight = std::max(1, screenHeight);
        bool cachedTargetsValid = count == requestedCount &&
                                  widths == requestedWidths &&
                                  height == requestedHeight;
        for (int index = 0; cachedTargetsValid && index < count; ++index)
            cachedTargetsValid = operations_.valid(targets[index]);
        if (cachedTargetsValid)
            return true;

        std::array<RenderTexture2D, 2> replacements{};
        for (int index = 0; index < requestedCount; ++index)
        {
            replacements[index] = operations_.loadHdr(
                requestedWidths[index], requestedHeight);
            if (!operations_.valid(replacements[index]))
            {
                discard(replacements[index]);
                if (operations_.reportFallback)
                    operations_.reportFallback();
                replacements[index] = operations_.loadFallback(
                    requestedWidths[index], requestedHeight);
            }
            if (!operations_.valid(replacements[index]))
            {
                discard(replacements[index]);
                for (RenderTexture2D &replacement : replacements)
                    discard(replacement);
                return false;
            }
        }
        for (int index = 0; index < requestedCount; ++index)
            operations_.configure(replacements[index]);

        release();
        targets = replacements;
        widths = requestedWidths;
        height = requestedHeight;
        count = requestedCount;
        return true;
    }

    void release()
    {
        for (RenderTexture2D &target : targets)
            discard(target);
        widths = {};
        height = 0;
        count = 0;
    }

private:
    Operations operations_{};

    static bool ownsResources(const RenderTexture2D &target)
    {
        // The loaders own attachments through their FBO. A zero FBO must
        // therefore be a fully cleaned zero value; UnloadRenderTexture cannot
        // safely infer whether an orphaned depth id is a texture or renderbuffer.
        assert(target.id != 0 ||
               (target.texture.id == 0 && target.depth.id == 0));
        return target.id != 0;
    }

    void discard(RenderTexture2D &target)
    {
        if (ownsResources(target) && operations_.unload)
            operations_.unload(target);
        target = {};
    }

    static Operations productionOperations()
    {
        Operations operations;
        operations.loadHdr = [](int width, int height) {
            return loadHdrRenderTexture(width, height);
        };
        operations.loadFallback = [](int width, int height) {
            return loadViewRenderTexture(
                width, height, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
        };
        operations.valid = [](const RenderTexture2D &target) {
            return IsRenderTextureValid(target);
        };
        operations.configure = [](const RenderTexture2D &target) {
            SetTextureFilter(target.texture, TEXTURE_FILTER_BILINEAR);
        };
        operations.unload = [](RenderTexture2D target) {
            UnloadRenderTexture(target);
        };
        operations.reportFallback = []() {
            TraceLog(LOG_WARNING,
                     "TANKS3D: RGBA16F view target unavailable; using RGBA8 fallback");
        };
        return operations;
    }

    static RenderTexture2D loadHdrRenderTexture(int width, int height)
    {
        RenderTexture2D target = loadViewRenderTexture(
            width, height, PIXELFORMAT_UNCOMPRESSED_R16G16B16A16);
        if (IsRenderTextureValid(target))
        {
            TraceLog(LOG_INFO,
                     "TANKS3D: RGBA16F HDR view target ready (%ix%i)",
                     width, height);
        }
        return target;
    }

    static RenderTexture2D loadViewRenderTexture(int width, int height,
                                                  int colorFormat)
    {
        RenderTexture2D target{};
        target.id = rlLoadFramebuffer();
        if (target.id == 0)
            return target;

        rlEnableFramebuffer(target.id);
        target.texture.id = rlLoadTexture(nullptr, width, height,
                                          colorFormat, 1);
        target.texture.width = width;
        target.texture.height = height;
        target.texture.mipmaps = 1;
        target.texture.format = colorFormat;

        target.depth.id = rlLoadTextureDepth(width, height, true);
        target.depth.width = width;
        target.depth.height = height;
        target.depth.mipmaps = 1;
        target.depth.format = kRaylibDepthAttachmentFormat;

        rlFramebufferAttach(target.id, target.texture.id,
                            RL_ATTACHMENT_COLOR_CHANNEL0, RL_ATTACHMENT_TEXTURE2D, 0);
        rlFramebufferAttach(target.id, target.depth.id,
                            RL_ATTACHMENT_DEPTH, RL_ATTACHMENT_RENDERBUFFER, 0);

        if (!rlFramebufferComplete(target.id))
        {
            rlDisableFramebuffer();
            // UnloadRenderTexture releases the color texture explicitly and
            // the attached depth renderbuffer through the FBO ownership root.
            UnloadRenderTexture(target);
            return {};
        }

        rlDisableFramebuffer();
        return target;
    }
};

void drawSettlementTankIcon(Vector2 center, int enemyType, Color accent,
                            float scale)
{
    const int type = std::clamp(enemyType, 0, kEnemyTypeCount - 1);
    const float trackHalfWidth = (type == 3 ? 10.0f : 8.7f) * scale;
    const float trackHalfHeight = (type == 1 ? 11.5f : 10.0f) * scale;
    const float trackWidth = (type == 3 ? 4.4f : 3.7f) * scale;
    const Color trackColor{32, 39, 42, 255};
    const Color trackHighlight{88, 96, 95, 255};
    const Color hullColor = ColorLerp(accent, Color{62, 67, 64, 255}, 0.32f);
    const Color edgeColor = ColorLerp(accent, RAYWHITE, 0.28f);

    const Rectangle leftTrack{
        center.x - trackHalfWidth,
        center.y - trackHalfHeight,
        trackWidth,
        trackHalfHeight * 2.0f};
    const Rectangle rightTrack{
        center.x + trackHalfWidth - trackWidth,
        center.y - trackHalfHeight,
        trackWidth,
        trackHalfHeight * 2.0f};
    DrawRectangleRounded(leftTrack, 0.35f, 4, trackColor);
    DrawRectangleRounded(rightTrack, 0.35f, 4, trackColor);
    for (int tread = -1; tread <= 1; ++tread)
    {
        const float y = center.y + tread * trackHalfHeight * 0.62f;
        DrawLineEx({leftTrack.x, y}, {leftTrack.x + trackWidth, y},
                   std::max(1.0f, scale), trackHighlight);
        DrawLineEx({rightTrack.x, y}, {rightTrack.x + trackWidth, y},
                   std::max(1.0f, scale), trackHighlight);
    }

    const float hullWidth = (type == 3 ? 13.2f : type == 1 ? 10.0f : 11.6f) * scale;
    const float hullHeight = (type == 1 ? 17.8f : 15.0f) * scale;
    const Rectangle hull{center.x - hullWidth * 0.5f,
                         center.y - hullHeight * 0.42f,
                         hullWidth, hullHeight};
    DrawRectangleRounded(hull, type == 3 ? 0.18f : 0.34f, 5, hullColor);
    DrawRectangleRoundedLinesEx(hull, type == 3 ? 0.18f : 0.34f, 5,
                                std::max(1.0f, scale), edgeColor);

    const float barrelWidth = (type == 2 ? 3.2f : type == 3 ? 2.8f : 2.1f) * scale;
    const float barrelLength = (type == 1 ? 10.5f : type == 2 ? 8.5f : 7.2f) * scale;
    DrawRectangleRounded(
        {center.x - barrelWidth * 0.5f,
         center.y - hullHeight * 0.40f - barrelLength,
         barrelWidth, barrelLength + 2.0f * scale},
        0.35f, 4, edgeColor);

    if (type == 3)
    {
        const float armorY = center.y + hullHeight * 0.19f;
        DrawRectangleRounded({center.x - hullWidth * 0.48f, armorY,
                              hullWidth * 0.96f, 4.1f * scale},
                             0.22f, 4, edgeColor);
    }

    const float turretRadius = (type == 2 ? 5.4f : type == 3 ? 5.0f :
                                type == 1 ? 3.7f : 4.3f) * scale;
    DrawCircleV({center.x, center.y - hullHeight * 0.15f},
                turretRadius + 1.0f * scale, Color{18, 23, 25, 255});
    DrawCircleV({center.x, center.y - hullHeight * 0.15f},
                turretRadius, accent);
    DrawCircleLines(static_cast<int>(center.x),
                    static_cast<int>(center.y - hullHeight * 0.15f),
                    turretRadius, edgeColor);
    if (type == 2)
    {
        DrawCircleV({center.x, center.y - hullHeight * 0.15f},
                    1.7f * scale, Color{255, 221, 128, 255});
    }
    else if (type == 1)
    {
        DrawLineEx({center.x - 2.5f * scale, center.y + 4.5f * scale},
                   {center.x + 2.5f * scale, center.y + 4.5f * scale},
                   std::max(1.0f, scale), edgeColor);
    }
}

void renderSettlement(const Game3D &game, SceneLighting &lighting,
                      TankAssets &tankAssets)
{
    const int screenWidth = std::max(1, GetScreenWidth());
    const int screenHeight = std::max(1, GetScreenHeight());
    const int playerCount = game.playerCount();
    const float spacing = playerCount == 1 ? 0.0f : 2.65f;
    const auto previewPosition = [playerCount, spacing](int index) {
        const float offset = (static_cast<float>(index) -
                              static_cast<float>(playerCount - 1) * 0.5f) *
                             spacing;
        // Align the models with the oblique report camera's screen-right
        // axis so both players sit on the same visual baseline.
        return XZ{13.0f + offset * 0.77f, 13.0f - offset * 0.64f};
    };

    lighting.updateShadowMap(
        [&game, &tankAssets, &previewPosition]() {
            for (int index = 0; index < game.playerCount(); ++index)
            {
                const Player &player = game.players()[static_cast<std::size_t>(index)];
                drawTankModel(tankAssets, previewPosition(index), kPi,
                              playerColor(index), false, player.level, 0.0f,
                              player.id, true, player.nation, true);
            }
            tankAssets.flushQueued(true);
        });

    BeginDrawing();
    ClearBackground(BLACK);
    DrawRectangleGradientV(0, 0, screenWidth, screenHeight,
                           Color{5, 10, 14, 255}, Color{13, 18, 19, 255});
    const int reportTop = std::max(18, screenHeight / 28);
    drawCenteredText("HI- " + std::to_string(game.settlementHighScore()),
                     screenWidth / 2, reportTop, 22, RAYWHITE);
    drawCenteredText("STAGE " + std::to_string(game.settlementStage()),
                     screenWidth / 2, reportTop + 38, 46,
                     Color{255, 247, 194, 255});
    drawCenteredText(game.settlementWasGameOver()
                         ? "FINAL BATTLE REPORT"
                         : "STAGE BATTLE REPORT",
                     screenWidth / 2, reportTop + 96, 21,
                     game.settlementWasGameOver() ? RED : GOLD);

    const int stageTop = reportTop + 114;
    const int desiredPreviewHeight = screenHeight >= 800 ? 210 : 170;
    const int stageBottom = std::max(
        stageTop + 105,
        std::min(stageTop + desiredPreviewHeight, screenHeight - 330));
    const float previewLeft = screenWidth * 0.08f;
    const float previewWidth = screenWidth * 0.84f;
    DrawRectangleRounded(
        {previewLeft, static_cast<float>(stageTop),
         previewWidth, static_cast<float>(stageBottom - stageTop)},
        0.04f, 8, Color{19, 27, 29, 240});
    DrawRectangleRoundedLinesEx(
        {previewLeft, static_cast<float>(stageTop),
         previewWidth, static_cast<float>(stageBottom - stageTop)},
        0.04f, 8, 2.0f, Color{132, 112, 58, 255});

    Camera3D previewCamera{};
    previewCamera.position = {17.8f, 3.85f, 18.8f};
    previewCamera.target = {13.0f, 0.60f, 13.0f};
    previewCamera.up = {0.0f, 1.0f, 0.0f};
    // The projection still covers the full window while this scene is clipped
    // into the report's shallow panel. Fit a 2.1-unit model envelope there,
    // then translate the camera along its actual up axis to center that panel.
    const float previewHeight = static_cast<float>(stageBottom - stageTop);
    previewCamera.fovy = static_cast<float>(screenHeight) * 2.1f / previewHeight;
    previewCamera.projection = CAMERA_ORTHOGRAPHIC;
    const Vector3 previewForward = Vector3Normalize(
        Vector3Subtract(previewCamera.target, previewCamera.position));
    const Vector3 previewRight = Vector3Normalize(
        Vector3CrossProduct(previewForward, previewCamera.up));
    const Vector3 previewUp = Vector3CrossProduct(previewRight, previewForward);
    const float panelCenter = (stageTop + stageBottom) * 0.5f;
    const float cameraOffset = (panelCenter - screenHeight * 0.5f) *
                               previewCamera.fovy / screenHeight;
    const Vector3 previewShift = Vector3Scale(previewUp, cameraOffset);
    previewCamera.position = Vector3Add(previewCamera.position, previewShift);
    previewCamera.target = Vector3Add(previewCamera.target, previewShift);
    BeginScissorMode(static_cast<int>(previewLeft) + 2, stageTop + 2,
                     std::max(1, static_cast<int>(previewWidth) - 4),
                     std::max(1, stageBottom - stageTop - 4));
    BeginMode3D(previewCamera);
    lighting.begin(previewCamera.position);
    DrawPlane({13.0f, -0.025f, 13.0f},
              {playerCount == 1 ? 5.2f : 8.0f, 4.0f},
              materialColor(Color{39, 46, 43, 255}, 7));
    for (int index = 0; index < playerCount; ++index)
    {
        const Player &player = game.players()[static_cast<std::size_t>(index)];
        const XZ position = previewPosition(index);
        drawTankContactShadow(position, kPi, false, player.id,
                              player.nation, player.level);
        drawTankModel(tankAssets, position, kPi,
                      playerColor(index), false, player.level, 0.0f,
                      player.id, true, player.nation);
    }
    tankAssets.flushQueued(false);
    lighting.end();
    EndMode3D();
    EndScissorMode();

    const int cardTop = stageBottom + 10;
    const int cardHeight = std::max(
        240, std::min(350, screenHeight - cardTop - 54));
    const int outerMargin = std::max(22, screenWidth / 30);
    const int cardGap = playerCount == 1 ? 0 : 20;
    const int cardWidth = playerCount == 1
                              ? std::min(720, screenWidth - outerMargin * 2)
                              : std::min(570, (screenWidth - outerMargin * 2 -
                                               cardGap) / 2);
    const int cardsWidth = cardWidth * playerCount +
                           cardGap * (playerCount - 1);
    const int cardsLeft = (screenWidth - cardsWidth) / 2;
    constexpr std::array<const char *, kEnemyTypeCount> enemyLabels{{
        "BASIC", "FAST", "POWER", "ARMOR"}};
    constexpr std::array<Color, kEnemyTypeCount> enemyColors{{
        Color{236, 213, 120, 255}, Color{102, 211, 232, 255},
        Color{246, 145, 74, 255}, Color{225, 91, 89, 255}}};
    for (int index = 0; index < playerCount; ++index)
    {
        const Player &player = game.players()[static_cast<std::size_t>(index)];
        const StageTally &tally = game.settlementTally(index);
        const int left = cardsLeft + index * (cardWidth + cardGap);
        const int centerX = left + cardWidth / 2;
        DrawRectangleRounded(
            {static_cast<float>(left), static_cast<float>(cardTop),
             static_cast<float>(cardWidth), static_cast<float>(cardHeight)},
            0.08f, 7, Color{8, 12, 14, 235});
        DrawRectangleRoundedLinesEx(
            {static_cast<float>(left), static_cast<float>(cardTop),
             static_cast<float>(cardWidth), static_cast<float>(cardHeight)},
            0.08f, 7, 2.0f, playerColor(index));
        const bool compact = cardHeight < 305;
        drawCenteredText("P" + std::to_string(index + 1) + "  PLAYER",
                         centerX, cardTop + (compact ? 6 : 8),
                         compact ? 18 : 21, playerColor(index));
        const std::string vehicle =
            wwii_tank_model::playerVehicleName(player.nation, player.level);
        drawCenteredText(vehicle, centerX, cardTop + (compact ? 28 : 33),
                         fittedFontSize(vehicle, cardWidth - 24,
                                        compact ? 13 : 15, 10),
                         LIGHTGRAY);

        const int rowTop = cardTop + (compact ? 60 : 73);
        const int rowHeight = compact ? 25 : 31;
        const int iconX = left + (compact ? 23 : 28);
        const int labelX = left + (compact ? 42 : 51);
        const int killsCenterX = left + cardWidth -
                                 (compact ? 132 : 166);
        const int pointsRight = left + cardWidth - (compact ? 12 : 18);
        const int headerY = rowTop - (compact ? 14 : 16);
        DrawLine(left + 12, headerY - 4, left + cardWidth - 12,
                 headerY - 4, Color{86, 96, 96, 170});
        drawTextShadow("TYPE", labelX, headerY,
                       compact ? 10 : 11, Color{151, 161, 160, 255});
        drawCenteredText("K.O.", killsCenterX, headerY,
                         compact ? 10 : 11, Color{151, 161, 160, 255});
        const std::string pointsHeader = "POINTS";
        drawTextShadow(pointsHeader,
                       pointsRight - MeasureText(pointsHeader.c_str(),
                                                 compact ? 10 : 11),
                       headerY, compact ? 10 : 11,
                       Color{151, 161, 160, 255});

        int displayedTotal = 0;
        for (int type = 0; type < kEnemyTypeCount; ++type)
        {
            const int y = rowTop + type * rowHeight;
            const Color rowColor = enemyColors[static_cast<std::size_t>(type)];
            if (type % 2 == 0)
            {
                DrawRectangleRounded(
                    {static_cast<float>(left + 9), static_cast<float>(y - 2),
                     static_cast<float>(cardWidth - 18),
                     static_cast<float>(rowHeight - 1)},
                    0.20f, 4, Color{21, 29, 31, 225});
            }
            drawSettlementTankIcon(
                {static_cast<float>(iconX),
                 static_cast<float>(y + rowHeight / 2 - 1)},
                type, rowColor, compact ? 0.66f : 0.78f);
            drawTextShadow(enemyLabels[static_cast<std::size_t>(type)],
                           labelX, y + (compact ? 4 : 6),
                           compact ? 13 : 15, rowColor);
            const int displayedKills =
                game.settlementDisplayedKills(index, type);
            displayedTotal += displayedKills;
            drawCenteredText("x " + std::to_string(displayedKills),
                             killsCenterX, y + (compact ? 4 : 6),
                             compact ? 13 : 15, RAYWHITE);
            const std::string points = std::to_string(
                tally.enemyPoints[static_cast<std::size_t>(type)]);
            drawTextShadow(points,
                           pointsRight - MeasureText(points.c_str(),
                                                     compact ? 13 : 15),
                           y + (compact ? 4 : 6), compact ? 13 : 15,
                           Color{238, 235, 210, 255});
        }

        const int summaryTop = rowTop + kEnemyTypeCount * rowHeight +
                               (compact ? 5 : 8);
        DrawLine(left + 12, summaryTop - 3, left + cardWidth - 12,
                 summaryTop - 3, Color{113, 103, 61, 220});
        drawTextShadow("TOTAL", labelX, summaryTop + (compact ? 1 : 2),
                       compact ? 15 : 18, GOLD);
        drawCenteredText("x " + std::to_string(displayedTotal),
                         killsCenterX, summaryTop + (compact ? 1 : 2),
                         compact ? 15 : 18, GOLD);
        const std::string totalPoints = std::to_string(tally.totalEnemyPoints());
        drawTextShadow(totalPoints,
                       pointsRight - MeasureText(totalPoints.c_str(),
                                                 compact ? 15 : 18),
                       summaryTop + (compact ? 1 : 2), compact ? 15 : 18,
                       GOLD);

        const int bonusY = summaryTop + (compact ? 24 : 31);
        drawTextShadow("BONUS  " + std::to_string(tally.bonusPoints),
                       left + 16, bonusY, compact ? 13 : 16,
                       Color{142, 214, 154, 255});
        const std::string stageScore =
            "STAGE  +" + std::to_string(tally.stagePoints());
        drawTextShadow(stageScore,
                       left + cardWidth - 16 -
                           MeasureText(stageScore.c_str(), compact ? 13 : 16),
                       bonusY, compact ? 13 : 16,
                       Color{255, 224, 119, 255});

        const int displayedScore = std::min(player.score,
                                            game.settlementScoreCounter());
        const int footerY = bonusY + (compact ? 22 : 28);
        drawTextShadow("SCORE  " + std::to_string(displayedScore),
                       left + 16, footerY, compact ? 14 : 17, RAYWHITE);
        const std::string lives =
            "LIVES  x" + std::to_string(std::max(0, player.lives));
        drawTextShadow(lives,
                       left + cardWidth - 16 -
                           MeasureText(lives.c_str(), compact ? 14 : 17),
                       footerY, compact ? 14 : 17, RAYWHITE);
    }

    const std::string prompt = game.settlementCounting()
                                   ? "BOTTOM FACE / ENTER: COUNT NOW"
                                   : "BOTTOM FACE / ENTER: CONTINUE";
    const int promptY = std::min(screenHeight - 34,
                                 cardTop + cardHeight + 12);
    drawCenteredText(prompt, screenWidth / 2, promptY, 18,
                     Color{205, 210, 205, 255});
    EndDrawing();
}

void renderHighScore(const Game3D &game)
{
    const int screenWidth = std::max(1, GetScreenWidth());
    const int screenHeight = std::max(1, GetScreenHeight());
    static constexpr std::array<Color, 5> flashColors{{
        Color{255, 220, 0, 255}, Color{255, 80, 80, 255},
        Color{255, 255, 255, 255}, Color{80, 255, 80, 255},
        Color{80, 180, 255, 255}}};
    const std::size_t colorIndex = static_cast<std::size_t>(
        static_cast<int>(GetTime() * 50.0) %
        static_cast<int>(flashColors.size()));
    const Color flash = flashColors[colorIndex];

    BeginDrawing();
    ClearBackground(BLACK);
    DrawRectangleGradientV(0, 0, screenWidth, screenHeight,
                           Color{4, 7, 10, 255},
                           Color{14, 19, 18, 255});
    drawCenteredText("NEW RECORD", screenWidth / 2,
                     screenHeight / 2 - 165, 28, GOLD);
    drawCenteredText("HISCORE", screenWidth / 2,
                     screenHeight / 2 - 105, 76, flash);
    drawCenteredText(std::to_string(game.settlementHighScore()),
                     screenWidth / 2, screenHeight / 2 + 5, 58, flash);
    drawCenteredText("BOTTOM FACE / ENTER: CONTINUE", screenWidth / 2,
                     screenHeight - 70, 18,
                     Color{205, 210, 205, 255});
    EndDrawing();
}

bool renderGame(Game3D &game, ViewTargets &viewTargets, SceneLighting &lighting,
                TankAssets &tankAssets, const EnvironmentAssets &environment,
                bonus_assets::Assets &bonusAssets, PostProcess &postProcess,
                bool showFrameRate)
{
    if (game.highScoreDisplay())
    {
        renderHighScore(game);
        return true;
    }
    if (game.settling())
    {
        renderSettlement(game, lighting, tankAssets);
        return true;
    }
    const int screenWidth = std::max(1, GetScreenWidth());
    const int screenHeight = std::max(1, GetScreenHeight());
    // Local co-op shares one full-screen camera that tracks both players.
    if (!viewTargets.ensure(1, screenWidth, screenHeight))
        return false;
    const Camera3D sharedCamera = game.cameraForPlayer(0);
    const TerrainView terrainView(sharedCamera, viewTargets.widths[0],
                                   viewTargets.height);

    // The fixed sun and arena share one stable orthographic shadow map across
    // the shared player camera. High refreshes moving silhouettes at 60 Hz;
    // Balanced updates less often to preserve laptop headroom.
    lighting.updateShadowMap(
        [&game, &tankAssets]() { drawShadowCasters(game, tankAssets); });

    for (int index = 0; index < 1; ++index)
    {
        BeginTextureMode(viewTargets.targets[index]);
        ClearBackground(Color{73, 112, 146, 255});
        DrawRectangleGradientV(0, 0, viewTargets.widths[index], viewTargets.height,
                               Color{63, 104, 142, 255}, Color{169, 193, 202, 255});
        DrawCircleGradient({viewTargets.widths[index] * 0.78f,
                            viewTargets.height * 0.16f},
                           viewTargets.height * 0.13f,
                           Color{255, 236, 188, 85}, Color{255, 239, 205, 0});
        BeginMode3D(sharedCamera);
        lighting.begin(sharedCamera.position);
        drawWorld(game, tankAssets, environment, terrainView);
        lighting.end();
        drawEnemyCreationWarnings(game, sharedCamera);
        for (const Pickup &pickup : game.bonuses())
            bonusAssets.draw(pickup, sharedCamera,
                             static_cast<float>(GetTime()));
        drawEmissiveBattleFx(game);
        drawForestForeground(game.map(), environment,
                             game.cameraYawDegrees(), terrainView);
        EndMode3D();
        EndTextureMode();
    }

    BeginDrawing();
    ClearBackground(BLACK);
    const Rectangle destination{0.0f, 0.0f, static_cast<float>(screenWidth),
                                static_cast<float>(screenHeight)};
    postProcess.draw(viewTargets.targets[0], destination,
                     static_cast<float>(GetTime()));
    drawStreakPopups(game, sharedCamera, screenWidth, screenHeight);
    for (int index = 0; index < game.playerCount(); ++index)
        drawViewportHud(game, index, destination, showFrameRate);
    drawGltfProbeHud(tankAssets, screenWidth, screenHeight);
    DrawRectangleLinesEx(destination, 3.0f, Color{224, 191, 67, 255});
    if (game.bonusMessageTimer() > 0.0f)
        drawCenteredText(game.bonusMessage(), screenWidth / 2, screenHeight - 64,
                         20, Color{255, 224, 94, 255});

    if (game.stageIntro() || game.paused() || game.gameOver() ||
        game.stageTransition())
    {
        DrawRectangle(0, 0, screenWidth, screenHeight, Color{0, 0, 0, 145});
        const std::string headline = game.stageIntro()
                                         ? "STAGE " + std::to_string(game.stage())
                                     : game.gameOver() ? "GAME OVER"
                                     : game.stageTransition() ? "STAGE CLEAR"
                                                              : "PAUSED";
        drawCenteredText(headline, screenWidth / 2, screenHeight / 2 - 42, 52,
                         game.gameOver() ? RED
                         : game.stageIntro() || game.stageTransition() ? GOLD
                                                                       : RAYWHITE);
        if (game.gameOver())
            drawCenteredText("BATTLE REPORT SOON    MINUS / ESC: SETUP",
                             screenWidth / 2, screenHeight / 2 + 24, 21,
                             LIGHTGRAY);
        else if (game.stageIntro())
            drawCenteredText("GET READY", screenWidth / 2,
                             screenHeight / 2 + 24, 21, LIGHTGRAY);
        else if (game.paused())
            drawCenteredText("PLUS / ENTER: RESUME    MINUS / ESC: SETUP",
                             screenWidth / 2, screenHeight / 2 + 24, 21,
                             LIGHTGRAY);
    }
    EndDrawing();
    return true;
}

struct MenuSettings
{
    int playerCount = 1;
    int stage = 1;
    int lives = 10;
    std::array<Nation, 2> nations{{Nation::UnitedStates, Nation::SovietUnion}};
    AdvancedGameSettings advanced{};
    int cameraYawDegrees = 0;
    int cameraElevationDegrees = kDefaultCameraElevationDegrees;
    int selected = 0;
    int advancedSelected = 0;
    bool advancedOpen = false;
    int connectedGamepads = 0;
};

int menuRowCount(const MenuSettings &settings)
{
    return 5 + (settings.playerCount == 2 ? 1 : 0);
}

int advancedMenuRow(const MenuSettings &settings)
{
    return menuRowCount(settings) - 1;
}

bool updateMenu(MenuSettings &settings, const UiInputFrame &input)
{
    bool changed = false;
    int rowCount = menuRowCount(settings);
    const int previousPlayerCount = settings.playerCount;
    const bool advancedWasSelected =
        settings.selected == advancedMenuRow(settings);
    if (input.upPressed)
    {
        settings.selected = (settings.selected + rowCount - 1) % rowCount;
        changed = true;
    }
    if (input.downPressed)
    {
        settings.selected = (settings.selected + 1) % rowCount;
        changed = true;
    }

    int direction = 0;
    if (input.leftPressed)
        direction = -1;
    if (input.rightPressed)
        direction = 1;
    const int step = input.coarseAdjustment ? 10 : 1;
    if (direction != 0)
    {
        if (settings.selected == 0)
        {
            settings.playerCount = settings.playerCount == 1 ? 2 : 1;
            changed = true;
        }
        else if (settings.selected == 1)
        {
            settings.stage = normalizedStage(settings.stage + direction * step);
            changed = true;
        }
        else if (settings.selected == 2)
        {
            settings.lives = std::clamp(settings.lives + direction * step, 1, 99);
            changed = true;
        }
        else if (settings.selected >= 3 &&
                 settings.selected < 3 + settings.playerCount)
        {
            const std::size_t playerIndex = static_cast<std::size_t>(settings.selected - 3);
            settings.nations[playerIndex] = cycleNation(settings.nations[playerIndex], direction);
            changed = true;
        }
    }
    if (input.selectOnePlayerPressed)
    {
        changed = changed || settings.playerCount != 1;
        settings.playerCount = 1;
    }
    if (input.selectTwoPlayerPressed)
    {
        changed = changed || settings.playerCount != 2;
        settings.playerCount = 2;
    }
    rowCount = menuRowCount(settings);
    settings.selected = advancedWasSelected &&
                                settings.playerCount != previousPlayerCount
                            ? advancedMenuRow(settings)
                            : std::clamp(settings.selected, 0, rowCount - 1);
    return changed;
}

constexpr int kAdvancedMenuRowCount = 7;
constexpr int kAdvancedMenuBackRow = kAdvancedMenuRowCount - 1;

bool advancedSettingsAreDefault(const MenuSettings &settings)
{
    return settings.advanced.playerMaximumHitPoints ==
               kDefaultPlayerMaximumHitPoints &&
           settings.advanced.enemySpeedPercent == 0 &&
           settings.advanced.enemyFireRatePercent == 0 &&
           settings.advanced.enemySpawnRatePercent == 0 &&
           settings.cameraYawDegrees == 0 &&
           settings.cameraElevationDegrees ==
               kDefaultCameraElevationDegrees;
}

bool updateAdvancedMenu(MenuSettings &settings, const UiInputFrame &input)
{
    bool changed = false;
    if (input.upPressed)
    {
        settings.advancedSelected =
            (settings.advancedSelected + kAdvancedMenuRowCount - 1) %
            kAdvancedMenuRowCount;
        changed = true;
    }
    if (input.downPressed)
    {
        settings.advancedSelected =
            (settings.advancedSelected + 1) % kAdvancedMenuRowCount;
        changed = true;
    }

    int direction = 0;
    if (input.leftPressed)
        direction = -1;
    if (input.rightPressed)
        direction = 1;
    if (direction != 0)
    {
        if (settings.advancedSelected == 0)
        {
            settings.advanced.playerMaximumHitPoints = std::clamp(
                settings.advanced.playerMaximumHitPoints + direction,
                kMinimumPlayerMaximumHitPoints,
                kMaximumPlayerMaximumHitPoints);
            changed = true;
        }
        else if (settings.advancedSelected >= 1 &&
                 settings.advancedSelected <= 3)
        {
            int *percent = settings.advancedSelected == 1
                               ? &settings.advanced.enemySpeedPercent
                           : settings.advancedSelected == 2
                               ? &settings.advanced.enemyFireRatePercent
                               : &settings.advanced.enemySpawnRatePercent;
            *percent = std::clamp(
                *percent + direction * kEnemyTuningPercentStep,
                kEnemyTuningMinimumPercent, kEnemyTuningMaximumPercent);
            changed = true;
        }
        else if (settings.advancedSelected == 4)
        {
            settings.cameraYawDegrees = std::clamp(
                settings.cameraYawDegrees +
                    direction * kCameraYawStepDegrees,
                kCameraYawMinimumDegrees, kCameraYawMaximumDegrees);
            changed = true;
        }
        else if (settings.advancedSelected == 5)
        {
            settings.cameraElevationDegrees = std::clamp(
                settings.cameraElevationDegrees +
                    direction * kCameraElevationStepDegrees,
                kCameraElevationMinimumDegrees,
                kCameraElevationMaximumDegrees);
            changed = true;
        }
    }

    if (input.resetPressed)
    {
        changed = changed || !advancedSettingsAreDefault(settings);
        settings.advanced = {};
        settings.cameraYawDegrees = 0;
        settings.cameraElevationDegrees =
            kDefaultCameraElevationDegrees;
    }
    return changed;
}

std::string percentageLabel(int requestedPercent)
{
    const int percent = normalizedEnemyTuningPercent(requestedPercent);
    if (percent == 0)
        return "0%  DEFAULT";
    return std::string(percent > 0 ? "+" : "") +
           std::to_string(percent) + "%";
}

std::string cameraYawLabel(int requestedDegrees)
{
    const int degrees = normalizedCameraYawDegrees(requestedDegrees);
    if (degrees == 0)
        return "0 DEG  STRAIGHT";
    return std::string(degrees < 0 ? "LEFT " : "RIGHT ") +
           std::to_string(std::abs(degrees)) + " DEG";
}

std::string cameraElevationLabel(int requestedDegrees)
{
    const int degrees =
        normalizedCameraElevationDegrees(requestedDegrees);
    return std::to_string(degrees) + " DEG" +
           (degrees == kDefaultCameraElevationDegrees
                ? "  DEFAULT"
                : "");
}

std::string technologyTreeLine(Nation nation, int playerIndex)
{
    std::string line = "P" + std::to_string(playerIndex + 1) + " TECH  ";
    for (int level = 0; level < 4; ++level)
    {
        if (level > 0)
            line += "  >  ";
        line += wwii_tank_model::playerVehicleName(nation, level);
    }
    return line;
}

void drawMenu(const MenuSettings &settings, const std::string &error)
{
    const int width = GetScreenWidth();
    const int height = GetScreenHeight();
    const bool compact = height < 700;
    const int rowHeight = compact ? 34 : 46;
    const int treeRowHeight = compact ? 18 : 24;
    BeginDrawing();
    ClearBackground(Color{11, 18, 24, 255});
    DrawRectangleGradientV(0, 0, width, height, Color{28, 52, 63, 255}, Color{7, 11, 15, 255});

    for (int index = 0; index < 16; ++index)
    {
        const int y = height - 40 - index * 25;
        DrawLine(0, y, width, y - width / 5, Fade(Color{82, 125, 133, 255}, 0.12f));
    }

    drawCenteredText("TANKS 3D", width / 2, compact ? 24 : 54,
                     std::clamp(width / 13, 58, compact ? 66 : 92), GOLD);
    drawCenteredText("TILTED TOP-DOWN ARMORED COMBAT", width / 2,
                     compact ? 96 : 139, compact ? 18 : 23,
                     Color{180, 218, 225, 255});

    const int rowCount = menuRowCount(settings);
    const int panelWidth = std::min(900, width - 40);
    const int panelX = (width - panelWidth) / 2;
    const int panelY = compact ? 126 : 178;
    const int panelHeight = 28 + rowCount * rowHeight +
                            settings.playerCount * treeRowHeight +
                            (compact ? 12 : 30);
    DrawRectangleRounded({static_cast<float>(panelX), static_cast<float>(panelY),
                          static_cast<float>(panelWidth), static_cast<float>(panelHeight)},
                         0.08f, 8, Color{4, 8, 11, 220});
    DrawRectangleRoundedLinesEx({static_cast<float>(panelX), static_cast<float>(panelY),
                                 static_cast<float>(panelWidth), static_cast<float>(panelHeight)},
                                0.08f, 8, 2.0f, Color{99, 151, 159, 220});

    std::vector<std::string> labels{{"PLAYERS", "STAGE", "LIVES EACH", "P1 NATION"}};
    std::vector<std::string> values{{std::to_string(settings.playerCount),
                                     std::to_string(settings.stage),
                                     std::to_string(settings.lives),
                                     nationName(settings.nations[0])}};
    if (settings.playerCount == 2)
    {
        labels.push_back("P2 NATION");
        values.push_back(nationName(settings.nations[1]));
    }
    labels.push_back("ADVANCED SETTINGS");
    values.push_back("OPEN");
    const int valueCenter = panelX + panelWidth - 170;
    for (int index = 0; index < rowCount; ++index)
    {
        const int y = panelY + 21 + index * rowHeight;
        const bool selected = index == settings.selected;
        if (selected)
            DrawRectangleRounded({static_cast<float>(panelX + 20), static_cast<float>(y - 7),
                                  static_cast<float>(panelWidth - 40),
                                  static_cast<float>(rowHeight - 6)},
                                 0.16f, 6, Color{42, 72, 76, 235});
        drawTextShadow((selected ? ">  " : "   ") + labels[index],
                       panelX + 38, y, compact ? 18 : 21,
                       selected ? RAYWHITE : Color{170, 185, 188, 255});
        const int valueFont = fittedFontSize(values[index], 230,
                                             compact ? 19 : 23, 15);
        drawTextShadow(values[index], valueCenter - MeasureText(values[index].c_str(), valueFont) / 2,
                       y - 1, valueFont, selected ? GOLD : LIGHTGRAY);
        if (selected && index != advancedMenuRow(settings))
        {
            drawTextShadow("<", valueCenter - 148, y, 21, GOLD);
            drawTextShadow(">", valueCenter + 132, y, 21, GOLD);
        }
    }

    const int treeY = panelY + 28 + rowCount * rowHeight;
    for (int playerIndex = 0; playerIndex < settings.playerCount; ++playerIndex)
    {
        const std::string tree = technologyTreeLine(
            settings.nations[static_cast<std::size_t>(playerIndex)], playerIndex);
        drawCenteredText(tree, width / 2, treeY + playerIndex * treeRowHeight,
                         fittedFontSize(tree, panelWidth - 50, 14, 9),
                         playerIndex == 0 ? Color{240, 180, 65, 255}
                                          : Color{88, 221, 142, 255});
    }

    const int helpY = panelY + panelHeight + (compact ? 10 : 14);
    drawCenteredText("D-PAD / STICK OR KEYS: SELECT / CHANGE    SHOULDER / SHIFT: x10",
                     width / 2, helpY,
                     fittedFontSize(
                         "D-PAD / STICK OR KEYS: SELECT / CHANGE    SHOULDER / SHIFT: x10",
                         width - 40, compact ? 14 : 16, 10), LIGHTGRAY);
    drawCenteredText("BOTTOM FACE / ENTER: START OR OPEN", width / 2,
                     helpY + (compact ? 22 : 31), compact ? 19 : 25,
                     Color{255, 224, 94, 255});
    drawCenteredText("P1  PAD 1 / ARROWS     P2  PAD 2 / WASD", width / 2,
                     helpY + (compact ? 48 : 67), compact ? 14 : 16,
                     Color{189, 210, 214, 255});
    const std::string deviceLine =
        "GAMEPADS " + std::to_string(settings.connectedGamepads) +
        "     1P: PAD 1/2     2P: FIRST=P1 SECOND=P2     "
        "F8 QUALITY     F11 FULLSCREEN     MINUS / ESC QUIT";
    drawCenteredText(deviceLine, width / 2, helpY + (compact ? 70 : 94),
                     fittedFontSize(deviceLine, width - 40,
                                    compact ? 12 : 14, 10),
                     Color{135, 157, 161, 255});
    if (!error.empty())
        drawCenteredText(error, width / 2, height - 42, 17, Color{255, 105, 92, 255});
    EndDrawing();
}

void drawAdvancedMenu(const MenuSettings &settings)
{
    const int width = GetScreenWidth();
    const int height = GetScreenHeight();
    const bool compact = height < 700;
    BeginDrawing();
    ClearBackground(Color{11, 18, 24, 255});
    DrawRectangleGradientV(0, 0, width, height, Color{28, 52, 63, 255},
                           Color{7, 11, 15, 255});
    for (int index = 0; index < 16; ++index)
    {
        const int y = height - 40 - index * 25;
        DrawLine(0, y, width, y - width / 5,
                 Fade(Color{82, 125, 133, 255}, 0.12f));
    }

    drawCenteredText("TANKS 3D", width / 2, compact ? 24 : 54,
                     std::clamp(width / 13, 58, compact ? 66 : 92), GOLD);
    drawCenteredText("ADVANCED SETTINGS", width / 2, compact ? 96 : 139,
                     compact ? 18 : 23,
                     Color{180, 218, 225, 255});

    const int panelWidth = std::min(900, width - 40);
    const int panelX = (width - panelWidth) / 2;
    const int panelY = compact ? 126 : 178;
    const int rowHeight = compact ? 34 : 48;
    const int noteRowHeight = compact ? 18 : 26;
    const int panelHeight = 28 + kAdvancedMenuRowCount * rowHeight +
                            (compact ? 66 : 96);
    DrawRectangleRounded({static_cast<float>(panelX), static_cast<float>(panelY),
                          static_cast<float>(panelWidth), static_cast<float>(panelHeight)},
                         0.08f, 8, Color{4, 8, 11, 220});
    DrawRectangleRoundedLinesEx(
        {static_cast<float>(panelX), static_cast<float>(panelY),
         static_cast<float>(panelWidth), static_cast<float>(panelHeight)},
        0.08f, 8, 2.0f, Color{99, 151, 159, 220});

    const std::array<std::string, kAdvancedMenuRowCount> labels{{
        "PLAYER HP", "ENEMY SPEED", "FIRE FREQUENCY", "SPAWN PACE",
        "VIEW HORIZONTAL", "VIEW ELEVATION", "BACK TO SETUP"}};
    const std::array<std::string, kAdvancedMenuRowCount> values{{
        settings.advanced.playerMaximumHitPoints == 1
            ? "1  BANDAGE OFF"
            : std::to_string(settings.advanced.playerMaximumHitPoints),
        percentageLabel(settings.advanced.enemySpeedPercent),
        percentageLabel(settings.advanced.enemyFireRatePercent),
        percentageLabel(settings.advanced.enemySpawnRatePercent),
        cameraYawLabel(settings.cameraYawDegrees),
        cameraElevationLabel(settings.cameraElevationDegrees),
        "RETURN"}};
    const int valueCenter = panelX + panelWidth - 190;
    for (int index = 0; index < kAdvancedMenuRowCount; ++index)
    {
        const int y = panelY + 21 + index * rowHeight;
        const bool selected = index == settings.advancedSelected;
        if (selected)
            DrawRectangleRounded(
                {static_cast<float>(panelX + 20), static_cast<float>(y - 7),
                 static_cast<float>(panelWidth - 40),
                 static_cast<float>(rowHeight - 4)},
                0.16f, 6, Color{42, 72, 76, 235});
        drawTextShadow((selected ? ">  " : "   ") + labels[index],
                       panelX + 38, y, compact ? 18 : 21,
                       selected ? RAYWHITE : Color{170, 185, 188, 255});
        const int valueFont = fittedFontSize(values[index], 270,
                                             compact ? 19 : 22, 13);
        drawTextShadow(values[index],
                       valueCenter -
                           MeasureText(values[index].c_str(), valueFont) / 2,
                       y - 1, valueFont, selected ? GOLD : LIGHTGRAY);
        if (selected && index != kAdvancedMenuBackRow)
        {
            drawTextShadow("<", valueCenter - 170, y, 21, GOLD);
            drawTextShadow(">", valueCenter + 152, y, 21, GOLD);
        }
    }

    const int noteY = panelY + 30 + kAdvancedMenuRowCount * rowHeight;
    drawCenteredText("HP 1-6 (STEP 1)    RATES -30% TO +30% (STEP 5%)",
                     width / 2, noteY, compact ? 13 : 15,
                     Color{170, 198, 203, 255});
    drawCenteredText("HORIZONTAL LEFT 45 TO RIGHT 45 (STEP 5 DEG)",
                     width / 2, noteY + noteRowHeight, compact ? 13 : 15,
                     Color{170, 198, 203, 255});
    drawCenteredText("ELEVATION 40 TO 70 (STEP 5 DEG)    HIGHER = MORE TOP-DOWN",
                     width / 2, noteY + noteRowHeight * 2, compact ? 13 : 15,
                     Color{142, 178, 184, 255});

    const int helpY = panelY + panelHeight + (compact ? 10 : 14);
    drawCenteredText("D-PAD / STICK OR KEYS: SELECT / CHANGE", width / 2,
                     helpY, compact ? 14 : 17, LIGHTGRAY);
    drawCenteredText("BOTTOM FACE / ENTER: SELECT    MINUS / ESC: RETURN    TOP FACE / R: RESET",
                     width / 2, helpY + (compact ? 24 : 32),
                     fittedFontSize(
                         "BOTTOM FACE / ENTER: SELECT    MINUS / ESC: RETURN    TOP FACE / R: RESET",
                         width - 40, compact ? 14 : 17, 11),
                     Color{255, 224, 94, 255});
    EndDrawing();
}

template <typename HeldQuery, typename PressedQuery>
PlayerInputFrame playerInputFrameFromKeyState(HeldQuery held,
                                               PressedQuery pressed)
{
    PlayerInputFrame frame;
    const auto readPlayer = [&](PlayerControlFrame &controls,
                                KeyboardKey northKey,
                                KeyboardKey southKey,
                                KeyboardKey westKey,
                                KeyboardKey eastKey) {
        controls.north.held = held(northKey);
        controls.south.held = held(southKey);
        controls.west.held = held(westKey);
        controls.east.held = held(eastKey);
        controls.north.pressed = pressed(northKey);
        controls.south.pressed = pressed(southKey);
        controls.west.pressed = pressed(westKey);
        controls.east.pressed = pressed(eastKey);
    };

    readPlayer(frame.players[0], KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT);
    frame.players[0].fireHeld =
        held(KEY_RIGHT_ALT) || held(KEY_RIGHT_CONTROL) || held(KEY_SPACE);
    readPlayer(frame.players[1], KEY_W, KEY_S, KEY_A, KEY_D);
    frame.players[1].fireHeld =
        held(KEY_LEFT_ALT) || held(KEY_LEFT_CONTROL) || held(KEY_F);
    return frame;
}

PlayerInputFrame readRaylibPlayerInputFrame()
{
    return playerInputFrameFromKeyState(
        [](KeyboardKey key) { return IsKeyDown(key); },
        [](KeyboardKey key) { return IsKeyPressed(key); });
}

UiInputFrame readRaylibUiInputFrame()
{
    UiInputFrame frame;
    frame.upPressed = IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W);
    frame.downPressed = IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S);
    frame.leftPressed = IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A);
    frame.rightPressed = IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D);
    frame.confirmPressed =
        IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE);
    frame.cancelPressed = IsKeyPressed(KEY_ESCAPE);
    frame.quitPressed = frame.cancelPressed || IsKeyPressed(KEY_Q);
    frame.pausePressed = IsKeyPressed(KEY_ENTER);
    frame.restartPressed = IsKeyPressed(KEY_R);
    frame.resetPressed = frame.restartPressed;
    frame.coarseAdjustment =
        IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    frame.selectOnePlayerPressed = IsKeyPressed(KEY_ONE);
    frame.selectTwoPlayerPressed = IsKeyPressed(KEY_TWO);
    return frame;
}

struct GamepadFrame
{
    PlayerInputFrame players;
    UiInputFrame ui;
    int connectedCount = 0;
};

class GamepadInput
{
  public:
    GamepadFrame read(bool gameplayActive, bool enabled,
                      int cameraYawDegrees = 0)
    {
        GamepadFrame frame;
        if (hasRead_ && previousGameplayActive_ && !gameplayActive)
        {
            // A held stick that was steering a rotated battle must not emit a
            // fresh menu-navigation edge when menu input returns to 0 degrees.
            for (GamepadInputState &state : states_)
            {
                state.previousStickDirection = CardinalDirection::None;
                state.suppressStickUntilRelease = true;
            }
        }
        previousGameplayActive_ = gameplayActive;
        hasRead_ = true;
        if (!enabled)
            return frame;

        const tanks3d::platform::GamepadBackendFrame backendFrame =
            backend_.poll();
        const auto &snapshots = backendFrame.snapshots;
        std::array<bool, kPhysicalGamepadSlotCount> available{};
        for (std::size_t slot = 0; slot < snapshots.size(); ++slot)
        {
            available[slot] = snapshots[slot].available;
            if (available[slot])
                ++frame.connectedCount;
        }

        assignments_.update(available, gameplayActive);
        reportConnectionChanges(available, backendFrame.names);
        for (std::size_t slot = 0; slot < snapshots.size(); ++slot)
        {
            const GamepadActionFrame actions =
                mapGamepadInput(snapshots[slot], states_[slot],
                                gameplayActive ? cameraYawDegrees : 0);
            const int playerIndex =
                assignments_.playerIndexForPhysicalSlot(slot);
            if (!gameplayActive)
            {
                if (playerIndex >= 0)
                    mergeUiInputFrame(frame.ui, actions.ui);
                continue;
            }
            if (playerIndex < 0 ||
                playerIndex >= static_cast<int>(frame.players.players.size()))
            {
                continue;
            }
            mergePlayerControlFrame(
                frame.players.players[static_cast<std::size_t>(playerIndex)],
                actions.player);
            mergeUiInputFrame(frame.ui, actions.ui);
        }
        return frame;
    }

  private:
    void reportConnectionChanges(
        const std::array<bool, kPhysicalGamepadSlotCount> &available,
        const std::array<std::string, kPhysicalGamepadSlotCount>
            &reportedNames)
    {
        for (std::size_t slot = 0; slot < available.size(); ++slot)
        {
            if (available[slot] == previousAvailable_[slot])
                continue;

            if (available[slot])
            {
                names_[slot] = reportedNames[slot].empty()
                                   ? "Unknown controller"
                                   : reportedNames[slot];
                std::cout << "Gamepad connected in slot " << slot + 1
                          << ": " << names_[slot];
                const int playerIndex =
                    assignments_.playerIndexForPhysicalSlot(slot);
                if (playerIndex >= 0)
                    std::cout << " (P" << playerIndex + 1 << ')';
                else
                    std::cout << " (unassigned; two player slots are in use)";
                std::cout << '\n';
            }
            else
            {
                std::cout << "Gamepad disconnected from slot " << slot + 1;
                if (!names_[slot].empty())
                    std::cout << ": " << names_[slot];
                std::cout << '\n';
            }
            previousAvailable_[slot] = available[slot];
        }
    }

    tanks3d::platform::GamepadBackend backend_;
    std::array<GamepadInputState, kPhysicalGamepadSlotCount> states_{};
    GamepadAssignments assignments_;
    std::array<bool, kPhysicalGamepadSlotCount> previousAvailable_{};
    std::array<std::string, kPhysicalGamepadSlotCount> names_{};
    bool previousGameplayActive_ = false;
    bool hasRead_ = false;
};

fs::path locateResourceRoot(const char *programPath)
{
    std::error_code error;
    const fs::path current = fs::current_path(error);
    fs::path executable = fs::absolute(programPath, error);
    if (error)
        executable = programPath;
    const fs::path executableDirectory = executable.parent_path();
    const std::array<fs::path, 6> candidates{{
        // Prefer paths anchored to the executable so launching from another
        // working directory cannot select an unrelated resources folder.
        executableDirectory / "../Resources",
        executableDirectory / "../resources",
        current / "resources",
        current / "../resources",
        current / "../../resources",
        executableDirectory / "../../resources"}};

    for (const fs::path &candidate : candidates)
    {
        std::error_code probeError;
        const bool hasReleaseResources =
            fs::is_directory(candidate, probeError) && !probeError &&
            (fs::is_regular_file(
                 candidate / "textures" / "battlefield_grass.png",
                 probeError) ||
             fs::is_regular_file(candidate / "models" / "tank_basic.glb",
                                 probeError) ||
             fs::is_regular_file(
                 candidate / "sounds" / "stage_start_up.ogg", probeError));
        if (!hasReleaseResources)
            continue;
        probeError.clear();
        const fs::path canonical = fs::weakly_canonical(candidate, probeError);
        return probeError ? candidate : canonical;
    }
    return {};
}

#include "../tests/self_tests.inl"

} // namespace

int main(int argc, char **argv)
{
    const std::string firstArgument = argc > 1 ? argv[1] : "";
    bool capabilityArgumentSeen = false;
    for (int argument = 1; argument < argc; ++argument)
    {
        if (std::string{argv[argument]} ==
            kReleasePerformanceCapabilitiesArgument)
        {
            capabilityArgumentSeen = true;
        }
    }
    if (capabilityArgumentSeen)
    {
        if (argc != 2 ||
            firstArgument != kReleasePerformanceCapabilitiesArgument)
        {
            std::cerr << kReleasePerformanceCapabilitiesArgument
                      << " must be the only argument\n";
            return 2;
        }
        const auto capabilityCheck = checkReleasePerformanceCapabilities(
            TANKS3D_RELEASE_SOURCE_COMMIT, TANKS3D_RELEASE_SOURCE_TAG);
        if (!capabilityCheck.success)
        {
            std::cerr << "Release performance capability self-check failed: "
                      << capabilityCheck.error << '\n';
            return 1;
        }
        writeReleasePerformanceCapabilities(
            std::cout, TANKS3D_RELEASE_SOURCE_COMMIT,
            TANKS3D_RELEASE_SOURCE_TAG);
        return std::cout ? 0 : 1;
    }
    if (firstArgument == "--self-test=unit")
        return runSelfTests({}, SelfTestSelection::Unit);
    if (firstArgument == "--self-test=session")
        return runSelfTests({}, SelfTestSelection::Session);
    if (firstArgument.rfind("--self-test=", 0) == 0 &&
        firstArgument != "--self-test=assets")
    {
        std::cerr << "Unknown self-test category: "
                  << firstArgument.substr(std::string("--self-test=").size())
                  << "\nValid categories: unit, session, assets, "
                     "release-performance-capabilities. "
                     "Use --self-test to run all suites.\n";
        return 2;
    }

    const fs::path resourceRoot = locateResourceRoot(argc > 0 ? argv[0] : "Tanks3D");
    if (resourceRoot.empty())
    {
        std::cerr << "Unable to locate game resources. Run from the repository root or build directory.\n";
        return 1;
    }
    if (firstArgument == "--self-test")
        return runSelfTests(resourceRoot);
    if (firstArgument == "--self-test=assets")
        return runSelfTests(resourceRoot, SelfTestSelection::Assets);
    if (firstArgument == "--dump-stage-signatures")
        return dumpStageSignatures(resourceRoot);

    std::vector<std::string> commandLineArguments;
    commandLineArguments.reserve(
        static_cast<std::size_t>(std::max(0, argc - 1)));
    for (int argument = 1; argument < argc; ++argument)
        commandLineArguments.emplace_back(argv[argument]);
    const auto releaseScreenshotParse =
        parseReleaseScreenshotOptions(commandLineArguments);
    if (!releaseScreenshotParse.valid())
    {
        std::cerr << "Invalid release screenshot options: "
                  << releaseScreenshotParse.error << '\n';
        return 2;
    }
    const ReleaseScreenshotOptions releaseScreenshot =
        releaseScreenshotParse.options;
    const auto releasePerformanceParse =
        parseReleasePerformanceOptions(commandLineArguments);
    if (!releasePerformanceParse.valid())
    {
        std::cerr << "Invalid release performance options: "
                  << releasePerformanceParse.error << '\n';
        return 2;
    }
    const ReleasePerformanceOptions releasePerformance =
        releasePerformanceParse.options;
    if (releaseScreenshot.requested())
    {
        const fs::path outputPath = releaseScreenshot.outputPath;
        std::error_code pathError;
        const bool outputExists = fs::exists(outputPath, pathError);
        if (pathError)
        {
            std::cerr << "Unable to inspect release screenshot output: "
                      << pathError.message() << '\n';
            return 2;
        }
        const bool outputIsSymlink = fs::is_symlink(outputPath, pathError);
        if (pathError == std::errc::no_such_file_or_directory)
            pathError.clear();
        if (pathError)
        {
            std::cerr << "Unable to inspect release screenshot output: "
                      << pathError.message() << '\n';
            return 2;
        }
        if (outputExists || outputIsSymlink)
        {
            std::cerr << "Release screenshot output already exists: "
                      << outputPath << '\n';
            return 2;
        }
        const fs::path parent = outputPath.has_parent_path()
                                    ? outputPath.parent_path()
                                    : fs::path{"."};
        if (!fs::is_directory(parent, pathError) || pathError)
        {
            std::cerr << "Release screenshot parent directory is not "
                         "accessible: "
                      << parent << '\n';
            return 2;
        }
    }
    if (releasePerformance.requested())
    {
        const fs::path outputPath = releasePerformance.outputPath;
        std::error_code pathError;
        const bool outputExists = fs::exists(outputPath, pathError);
        if (pathError)
        {
            std::cerr << "Unable to inspect release performance output: "
                      << pathError.message() << '\n';
            return 2;
        }
        const bool outputIsSymlink = fs::is_symlink(outputPath, pathError);
        if (pathError == std::errc::no_such_file_or_directory)
            pathError.clear();
        if (pathError)
        {
            std::cerr << "Unable to inspect release performance output: "
                      << pathError.message() << '\n';
            return 2;
        }
        if (outputExists || outputIsSymlink)
        {
            std::cerr << "Release performance output already exists: "
                      << outputPath << '\n';
            return 2;
        }
        const fs::path parent = outputPath.has_parent_path()
                                    ? outputPath.parent_path()
                                    : fs::path{"."};
        if (!fs::is_directory(parent, pathError) || pathError)
        {
            std::cerr << "Release performance parent directory is not "
                         "accessible: "
                      << parent << '\n';
            return 2;
        }
    }

    bool gltfTankQaRequested = false;
    fs::path gltfTankQaPath;
    for (int argument = 1; argument < argc; ++argument)
    {
        const std::string value = argv[argument];
        if (value == "--gltf-tank-qa")
        {
            gltfTankQaRequested = true;
        }
        else if (value.rfind("--gltf-tank-qa=", 0) == 0)
        {
            gltfTankQaRequested = true;
            gltfTankQaPath = value.substr(15);
        }
    }

    // Pace explicitly at 120 Hz for responsive controller input on ProMotion
    // displays. Combining a separate 60 Hz limiter with the 120 Hz swap
    // interval made GLFW/raylib wait twice and added avoidable input latency.
    // The 3D view is already resolved into its HDR render target before the
    // full-screen post-process pass. Multisampling the Retina back buffer then
    // shades that resolved image four more times without improving geometry
    // edges, so keep the presentation buffer single-sampled.
    unsigned int windowFlags = FLAG_WINDOW_HIGHDPI;
    // Screenshot exports must reach their requested frame even if macOS
    // minimizes the capture window while other applications are in use.
    if (releaseScreenshot.requested())
        windowFlags |= FLAG_WINDOW_ALWAYS_RUN;
    else
        windowFlags |= FLAG_WINDOW_RESIZABLE;
    SetConfigFlags(windowFlags);
    InitWindow(kReleaseScreenshotWidth, kReleaseScreenshotHeight,
               "TANKS 3D - TILTED TOP-DOWN ARMORED COMBAT");
    if (!IsWindowReady())
    {
        std::cerr << "Unable to create the game window or graphics context\n";
        return 1;
    }
    SetExitKey(KEY_NULL);
    InitAudioDevice();

    MenuSettings settings;
    GamepadInput gamepadInput;
    AudioBank audio;
    audio.load(resourceRoot);
    SceneLighting lighting;
    lighting.load();
    PostProcess postProcess;
    postProcess.load();
    EnvironmentAssets environment;
    environment.load(resourceRoot);
    bonus_assets::Assets bonusAssets;
    bonusAssets.load(resourceRoot);
    TankAssets tankAssets;
    if (gltfTankQaRequested)
        tankAssets.configureGltfProbe(gltfTankQaPath);
    tankAssets.load(resourceRoot, lighting.shader(), lighting.depthShader());
    const std::uint32_t gameSeed =
        (releaseScreenshot.requested() || releasePerformance.requested())
                                       ? kReleaseScreenshotSeed
                                       : static_cast<std::uint32_t>(
                                             std::random_device{}());
    Game3D game(resourceRoot, gameSeed, &audio);
    ViewTargets viewTargets;
    bool inGame = false;
    bool bonusShowcase = false;
    bool tankShowcase = false;
    bool settlementShowcase = false;
    bool baseDamageShowcase = false;
    bool baseSteelShowcase = false;
    bool enemyCreationShowcase = false;
    bool forestCoverShowcase = false;
    bool exitRequested = false;
    bool releaseScreenshotSaved = false;
    bool releasePerformanceSaved = false;
    bool releasePerformanceFramePending = false;
    int releasePerformancePendingStage = 1;
    int releasePerformancePendingPlayerCount = 1;
    std::string releasePerformancePendingState = "gameplay";
    bool releasePerformancePendingFocus = true;
    std::uint64_t releasePerformanceCompletedStages = 0U;
    std::uint64_t releasePerformancePendingCompletedStages = 0U;
    std::unique_ptr<ReleasePerformanceRecorder> releasePerformanceRecorder;
    int renderedGameFrames = 0;
    int processResult = 0;
    std::string menuError;

    for (int argument = 1; argument < argc; ++argument)
    {
        const std::string value = argv[argument];
        if (value.rfind("--stage=", 0) == 0)
        {
            int requestedStage = settings.stage;
            std::istringstream(value.substr(8)) >> requestedStage;
            settings.stage = normalizedStage(requestedStage);
        }
        else if (value.rfind("--camera-yaw=", 0) == 0)
        {
            int requestedYaw = settings.cameraYawDegrees;
            std::istringstream(value.substr(13)) >> requestedYaw;
            settings.cameraYawDegrees =
                normalizedCameraYawDegrees(requestedYaw);
            game.setCameraYawDegrees(settings.cameraYawDegrees);
        }
        else if (value.rfind("--camera-elevation=", 0) == 0 ||
                 value.rfind("--camera-pitch=", 0) == 0)
        {
            int requestedElevation = settings.cameraElevationDegrees;
            const std::size_t separator = value.find('=');
            std::istringstream(value.substr(separator + 1)) >>
                requestedElevation;
            settings.cameraElevationDegrees =
                normalizedCameraElevationDegrees(requestedElevation);
            game.setCameraElevationDegrees(
                settings.cameraElevationDegrees);
        }
        else if (value == "--quick-start")
        {
            inGame = game.start(1, settings.lives, settings.stage,
                                settings.nations, settings.advanced,
                                settings.cameraYawDegrees,
                                settings.cameraElevationDegrees);
        }
        else if (value == "--quick-start-2p")
        {
            settings.playerCount = 2;
            inGame = game.start(2, settings.lives, settings.stage,
                                settings.nations, settings.advanced,
                                settings.cameraYawDegrees,
                                settings.cameraElevationDegrees);
        }
        else if (value == "--quick-start-ussr")
        {
            settings.nations[0] = Nation::SovietUnion;
            inGame = game.start(1, settings.lives, settings.stage,
                                settings.nations, settings.advanced,
                                settings.cameraYawDegrees,
                                settings.cameraElevationDegrees);
        }
        else if (value == "--quick-start-germany")
        {
            settings.nations[0] = Nation::Germany;
            inGame = game.start(1, settings.lives, settings.stage,
                                settings.nations, settings.advanced,
                                settings.cameraYawDegrees,
                                settings.cameraElevationDegrees);
        }
        else if (value == "--bonus-showcase")
        {
            bonusShowcase = true;
        }
        else if (value == "--tank-showcase")
        {
            tankShowcase = true;
        }
        else if (value == "--settlement-showcase")
        {
            settlementShowcase = true;
        }
        else if (value == "--base-damage-showcase")
        {
            baseDamageShowcase = true;
        }
        else if (value == "--base-steel-showcase")
        {
            baseSteelShowcase = true;
        }
        else if (value == "--enemy-creation-showcase")
        {
            enemyCreationShowcase = true;
        }
        else if (value == "--forest-cover-showcase")
        {
            forestCoverShowcase = true;
        }
        else if (value == "--advanced-settings-showcase")
        {
            settings.advancedOpen = true;
        }
        else if (value == "--advanced-settings-showcase-hp1")
        {
            settings.advancedOpen = true;
            settings.advanced.playerMaximumHitPoints = 1;
        }
    }
    if (gltfTankQaRequested)
    {
        settings.nations[0] = Nation::UnitedStates;
        if (!inGame)
            inGame = game.start(1, settings.lives, settings.stage,
                                settings.nations, settings.advanced,
                                settings.cameraYawDegrees,
                                settings.cameraElevationDegrees);
        tankShowcase = true;
    }
    if (inGame && bonusShowcase)
        game.spawnBonusShowcase();
    if (inGame && tankShowcase)
        game.spawnTankShowcase();
    if (inGame && settlementShowcase)
        game.spawnSettlementShowcase();
    if (inGame && baseDamageShowcase)
        game.spawnBaseDamageShowcase();
    if (inGame && baseSteelShowcase)
        game.spawnBaseSteelShowcase();
    if (inGame && enemyCreationShowcase)
        game.spawnEnemyCreationShowcase();
    if (inGame && forestCoverShowcase)
        game.spawnForestCoverShowcase();
    if (releaseScreenshot.requested() && !inGame)
    {
        std::cerr << "--release-screenshot requires a quick-start mode\n";
        processResult = 2;
        exitRequested = true;
    }
    if (releasePerformance.requested())
    {
        if (!inGame)
        {
            std::cerr << "Release performance telemetry could not start its "
                         "quick-start game\n";
            processResult = 2;
            exitRequested = true;
        }
        else
        {
            releasePerformanceRecorder =
                std::make_unique<ReleasePerformanceRecorder>(
                    releasePerformance, TANKS3D_RELEASE_SOURCE_COMMIT,
                    TANKS3D_RELEASE_SOURCE_TAG,
                    std::chrono::system_clock::now());
            if (!releasePerformanceRecorder->valid())
            {
                std::cerr << "Unable to initialize release performance "
                             "telemetry: "
                          << releasePerformanceRecorder->error() << '\n';
                processResult = 2;
                exitRequested = true;
            }
            else
            {
                std::cout << kReleasePerformanceStartMarker << ' '
                          << releasePerformance.sessionNonce << std::endl;
            }
        }
    }

    auto previousFrame =
        std::chrono::steady_clock::now() - kInteractiveFrameBudget;
    while (!exitRequested && !WindowShouldClose())
    {
        const auto frameStart = std::chrono::steady_clock::now();
        LaptopFramePacer framePacer(frameStart);
        const double actualFrameDuration =
            std::chrono::duration<double>(frameStart - previousFrame).count();
        const float dt = static_cast<float>(actualFrameDuration);
        previousFrame = frameStart;
        if (releasePerformanceRecorder && releasePerformanceFramePending)
        {
            releasePerformanceFramePending = false;
            if (!releasePerformanceRecorder->recordFrame(
                    actualFrameDuration, releasePerformancePendingStage,
                    releasePerformancePendingPlayerCount,
                    releasePerformancePendingState,
                    releasePerformancePendingFocus,
                    releasePerformancePendingCompletedStages))
            {
                std::cerr << "Release performance telemetry failed: "
                          << releasePerformanceRecorder->error() << '\n';
                processResult = 1;
                exitRequested = true;
                break;
            }
            if (releasePerformanceRecorder->targetDurationReached())
            {
                const auto finalized = releasePerformanceRecorder->finalize(
                    std::chrono::system_clock::now(), true);
                const auto saved = finalized.succeeded()
                                       ? releasePerformanceRecorder
                                             ->saveNoReplace()
                                       : finalized;
                if (!saved.succeeded())
                {
                    std::cerr << "Unable to save release performance "
                                 "telemetry: "
                              << saved.message << '\n';
                    processResult = 1;
                }
                else
                {
                    releasePerformanceSaved = true;
                    std::cout << kReleasePerformanceCompleteMarker << ' '
                              << releasePerformance.sessionNonce
                              << std::endl;
                }
                exitRequested = true;
                break;
            }
        }

        UiInputFrame uiInput = readRaylibUiInputFrame();
        const GamepadFrame gamepadFrame = gamepadInput.read(
            inGame, !releaseScreenshot.requested() &&
                        !releasePerformance.requested(),
            game.cameraYawDegrees());
        mergeUiInputFrame(uiInput, gamepadFrame.ui);
        settings.connectedGamepads = gamepadFrame.connectedCount;

        if (!releaseScreenshot.requested() &&
            !releasePerformance.requested() && IsKeyPressed(KEY_F11))
            ToggleBorderlessWindowed();
        if (!releasePerformance.requested() && IsKeyPressed(KEY_F8))
            lighting.toggleQuality();

        if (releasePerformanceRecorder && !inGame)
        {
            std::cerr << "Release performance telemetry left the active "
                         "game before reaching its duration\n";
            processResult = 1;
            exitRequested = true;
            break;
        }

        if (!inGame)
        {
            if (settings.advancedOpen)
            {
                if (updateAdvancedMenu(settings, uiInput))
                    audio.play(AudioCue::MenuSelect);
                const bool returnToSetup = uiInput.cancelPressed ||
                    (uiInput.confirmPressed &&
                     settings.advancedSelected == kAdvancedMenuBackRow);
                if (returnToSetup)
                {
                    audio.play(AudioCue::MenuSelect);
                    settings.advancedOpen = false;
                    drawMenu(settings, menuError);
                    continue;
                }
                drawAdvancedMenu(settings);
                continue;
            }

            if (updateMenu(settings, uiInput))
                audio.play(AudioCue::MenuSelect);
            if (uiInput.quitPressed)
            {
                exitRequested = true;
            }
            else if (uiInput.confirmPressed)
            {
                audio.play(AudioCue::MenuSelect);
                if (settings.selected == advancedMenuRow(settings))
                {
                    settings.advancedOpen = true;
                    settings.advancedSelected = 0;
                    drawAdvancedMenu(settings);
                    continue;
                }
                if (game.start(settings.playerCount, settings.lives,
                               settings.stage, settings.nations,
                               settings.advanced,
                               settings.cameraYawDegrees,
                               settings.cameraElevationDegrees))
                {
                    inGame = true;
                    menuError.clear();
                }
                else
                {
                    menuError = game.lastError();
                }
            }
            drawMenu(settings, menuError);
            continue;
        }

        if (uiInput.cancelPressed)
        {
            inGame = false;
            audio.stopAll();
            viewTargets.release();
            // EndDrawing() also advances raylib's input state.  Draw the menu
            // before continuing so this Escape press cannot be observed again
            // by the menu on the next frame and mistaken for an exit request.
            drawMenu(settings, menuError);
            continue;
        }
        if ((game.settling() || game.highScoreDisplay()) &&
            uiInput.confirmPressed)
            game.confirmSettlement();
        else if (!releasePerformance.requested() &&
                 !game.endingSequence() && uiInput.pausePressed)
            game.togglePause();
        if (!releasePerformance.requested())
        {
            if (!game.endingSequence() && IsKeyPressed(KEY_T))
                game.toggleTargets();
            if (!game.endingSequence() && IsKeyPressed(KEY_N) &&
                !game.changeStage(1))
            {
                menuError = game.lastError();
            }
            if (!game.endingSequence() && IsKeyPressed(KEY_B) &&
                !game.changeStage(-1))
            {
                menuError = game.lastError();
            }
            if (!game.endingSequence() && uiInput.restartPressed &&
                !game.restart())
            {
                menuError = game.lastError();
            }
        }

        if (game.consumeMenuRequest())
        {
            menuError = game.lastError();
            inGame = false;
            audio.updateEngine(false, false);
            viewTargets.release();
            // Advance raylib's input frame before the menu can observe the
            // same confirm edge that requested this transition.
            drawMenu(settings, menuError);
            continue;
        }

        if (!bonusShowcase && !tankShowcase)
        {
            PlayerInputFrame inputFrame = readRaylibPlayerInputFrame();
            for (std::size_t playerIndex = 0;
                 playerIndex < inputFrame.players.size(); ++playerIndex)
            {
                mergePlayerControlFrame(
                    inputFrame.players[playerIndex],
                    gamepadFrame.players.players[playerIndex]);
            }
            if (game.playerCount() == 1)
            {
                // Either of the two menu-capable pads may start and control a
                // solo battle; two-player games retain strict P1/P2 isolation.
                mergePlayerControlFrame(
                    inputFrame.players[0],
                    gamepadFrame.players.players[1]);
            }
            game.update(dt, inputFrame);
            if (releasePerformanceRecorder)
            {
                for (const GameEvent &event : game.eventsThisUpdate())
                {
                    if (event.type != GameEventType::StageEnded ||
                        event.stageEndReason != StageEndReason::Cleared)
                    {
                        continue;
                    }
                    if (releasePerformanceCompletedStages ==
                        std::numeric_limits<std::uint64_t>::max())
                    {
                        std::cerr << "Release performance completed-stage "
                                     "counter overflowed\n";
                        processResult = 1;
                        exitRequested = true;
                        break;
                    }
                    ++releasePerformanceCompletedStages;
                }
                if (exitRequested)
                    break;
            }
        }
        if (game.consumeMenuRequest())
        {
            menuError = game.lastError();
            inGame = false;
            audio.updateEngine(false, false);
            viewTargets.release();
            drawMenu(settings, menuError);
            continue;
        }
        tankAssets.setAnimationClock(GetTime());
        if (!renderGame(game, viewTargets, lighting, tankAssets, environment,
                        bonusAssets, postProcess,
                        !releaseScreenshot.requested()))
        {
            menuError = "Unable to allocate the gameplay render target";
            std::cerr << menuError << '\n';
            viewTargets.release();
            audio.updateEngine(false, false);
            if (releaseScreenshot.requested() ||
                releasePerformance.requested())
            {
                processResult = 1;
                exitRequested = true;
                break;
            }
            inGame = false;
            drawMenu(settings, menuError);
            continue;
        }
        ++renderedGameFrames;
        if (releasePerformanceRecorder)
        {
            releasePerformancePendingStage = game.stage();
            releasePerformancePendingPlayerCount = game.playerCount();
            releasePerformancePendingState =
                game.highScoreDisplay()
                    ? "high_score"
                    : (game.settling() ? "settlement" : "gameplay");
            releasePerformancePendingFocus = IsWindowFocused();
            releasePerformancePendingCompletedStages =
                releasePerformanceCompletedStages;
            releasePerformanceFramePending = true;
        }
        if (releaseScreenshot.due(renderedGameFrames))
        {
            Image image = LoadImageFromScreen();
            bool exported = false;
            std::string exportMessage;
            if (image.data != nullptr)
            {
                if (image.width != kReleaseScreenshotWidth ||
                    image.height != kReleaseScreenshotHeight)
                {
                    ImageResize(&image, kReleaseScreenshotWidth,
                                kReleaseScreenshotHeight);
                }
                int encodedSize = 0;
                unsigned char *encoded =
                    ExportImageToMemory(image, ".png", &encodedSize);
                if (encoded != nullptr && encodedSize > 0)
                {
                    const auto fileResult =
                        saveReleaseScreenshotFileNoReplace(
                            releaseScreenshot.outputPath, encoded,
                            static_cast<std::size_t>(encodedSize));
                    exported = fileResult.saved();
                    exportMessage = fileResult.message;
                }
                else
                {
                    exportMessage = "PNG encoding failed";
                }
                if (encoded != nullptr)
                    MemFree(encoded);
                UnloadImage(image);
            }
            else
            {
                exportMessage = "framebuffer read failed";
            }
            if (exported)
            {
                releaseScreenshotSaved = true;
                std::cout << "Saved release screenshot: "
                          << releaseScreenshot.outputPath << '\n';
                if (!exportMessage.empty())
                    std::cerr << "Release screenshot warning: "
                              << exportMessage << '\n';
            }
            else
            {
                std::cerr << "Unable to save release screenshot: "
                          << releaseScreenshot.outputPath;
                if (!exportMessage.empty())
                    std::cerr << ": " << exportMessage;
                std::cerr << '\n';
                processResult = 1;
            }
            exitRequested = true;
        }
    }

    if (releaseScreenshot.requested() && !releaseScreenshotSaved &&
        processResult == 0)
    {
        std::cerr << "Release screenshot capture ended before its requested "
                     "frame\n";
        processResult = 1;
    }
    if (releasePerformance.requested() && !releasePerformanceSaved &&
        processResult == 0)
    {
        std::cerr << "Release performance capture ended before its requested "
                     "duration\n";
        processResult = 1;
    }

    viewTargets.release();
    audio.updateEngine(false, false);
    audio.unload();
    tankAssets.unload();
    bonusAssets.unload();
    environment.unload();
    postProcess.unload();
    lighting.unload();
    if (IsAudioDeviceReady())
        CloseAudioDevice();
#if defined(__APPLE__)
    // raylib 6.0 can enter rlglClose() after GLFW has already discarded the
    // macOS OpenGL context (for example when the window is closed by the OS),
    // then dereference a null GL unload entry point.  Process teardown already
    // releases the native window and context; avoid that known double-cleanup
    // path while retaining explicit CloseWindow() on the other backends.
#else
    CloseWindow();
#endif
    return processResult;
}
