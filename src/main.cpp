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
#include "app/ai_player.h"
#include "app/input_adapter.h"
#include "app/release_performance_capabilities.h"
#include "app/release_performance_log.h"
#include "app/release_performance_options.h"
#include "app/release_screenshot_file.h"
#include "app/release_screenshot_options.h"
#include "app/shell_cancellation_presentation.h"
#include "app/shell_flight_presentation.h"
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
#include "app/game_session.h"
#include "platform/gamepad_backend.h"
#include "post_process.h"
#include "app/lan_game_bridge.h"
#include "app/lan_session.h"
#include "lan_menu.h"
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

using namespace tanks3d::game;
using namespace tanks3d::app;
constexpr std::uint32_t kReleaseScreenshotSeed = 0x5c43e3d1U;

float gameplayCameraAspectRatio()
{
    const int width = GetScreenWidth();
    const int height = GetScreenHeight();
    if (width > 0 && height > 0)
        return static_cast<float>(width) / static_cast<float>(height);
    return static_cast<float>(kReleaseScreenshotWidth) /
           static_cast<float>(kReleaseScreenshotHeight);
}

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

Vector3 toRaylibVector3(Float3 value)
{
    return {value.x, value.y, value.z};
}

Color toRaylibColor(Rgba8 value)
{
    return {value.r, value.g, value.b, value.a};
}



constexpr std::size_t kAudioCueCount =
    static_cast<std::size_t>(AudioCue::Count);
constexpr std::size_t kAudioVoiceCount = 12U;

// Cue mapping and playback settings follow krystiankaluzny/Tanks. Stage-start
// and game-over recordings come from JustoSenka/BattleCity; see ASSET_LICENSES.md.
// Music consists of one-shot jingles, while idle/moving form the battle bed.
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
uniform vec4 tankWidthNormalCorrection;
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
    vec3 normal = modelSpaceInput != 0
                      ? vec3(matNormal*vec4(vertexNormal, 0.0))
                      : vertexNormal;
    if (modelSpaceInput == 0)
        normal += tankWidthNormalCorrection.xyz *
                  dot(tankWidthNormalCorrection.xyz, normal) *
                  tankWidthNormalCorrection.w;
    fragNormal = normalize(normal);
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

float shadowVisibility(vec3 normal, vec3 toLight, float biasScale)
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
    float bias = max(0.00065*(1.0 - normalLight), 0.00012)*biasScale;
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
    // Authored arcade castings use clean paint panels. Give their materials
    // (14..17) the established vehicle responses without the
    // legacy screen-space grain; Pixel Style remains in the world post pass.
    bool sampleMaterial = materialTag >= 14 && materialTag <= 17;
    if (sampleMaterial) materialTag -= 5;
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
    float visibility = shadowVisibility(normal, toLight, sampleMaterial ? 2.0 : 1.0);
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
        if (!sampleMaterial)
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
        if (materialTag == 9 && !sampleMaterial)
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
    if (vehicleMaterial && !sampleMaterial)
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
        tankWidthNormalLocation_ =
            GetShaderLocation(shader_, "tankWidthNormalCorrection");
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
        const Vector4 noWidthCorrection{};
        SetShaderValue(shader_, tankWidthNormalLocation_, &noWidthCorrection,
                       SHADER_UNIFORM_VEC4);
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
        wwii_tank_model::presentationLighting = {
            this, [](void *context, float widthScale)
            {
                static_cast<SceneLighting *>(context)->setTankWidthNormalCorrection(widthScale);
            }};
        active_ = true;
    }

    void end()
    {
        if (active_)
        {
            if (wwii_tank_model::presentationLighting.context == this)
                wwii_tank_model::presentationLighting = {};
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

    void setTankWidthNormalCorrection(float widthScale)
    {
        // rlgl applies its forward matrix to immediate normals, including
        // normals generated inside DrawCylinderEx. Undo the extra width twice
        // along the posed world-space X axis to obtain inverse-transpose
        // normals. Flush on entry/exit so the uniform cannot affect neighbors,
        // shields or terrain. Depth rendering never installs this callback.
        rlDrawRenderBatchActive();
        const Matrix pose = rlGetMatrixTransform();
        const Vector3 axis = Vector3Normalize({pose.m0, pose.m1, pose.m2});
        const Vector4 correction{axis.x, axis.y, axis.z,
                                 1.0f / (widthScale * widthScale) - 1.0f};
        SetShaderValue(shader_, tankWidthNormalLocation_, &correction,
                       SHADER_UNIFORM_VEC4);
    }

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
    int tankWidthNormalLocation_ = -1;
    int materialTagOverrideLocation_ = -1;
    unsigned int shadowFrameCounter_ = 0;
    bool active_ = false;
    bool highQuality_ = true;
    bool shadowAvailable_ = false;
    bool hasRenderedShadow_ = false;
};

struct Game3DTestAccess;
Color playerColor(int id);

// Raylib presentation adapter. All session rules are shared with the Godot
// frontend; only effect geometry and the engine camera conversion live here.
class Game3D : public tanks3d::app::GameSession,
               private tanks3d::app::SessionPresentation
{
public:
    explicit Game3D(fs::path root, AudioOutput *audio = nullptr)
        : GameSession(std::move(root), audio)
    {
        presentation_ = this;
    }
    Game3D(fs::path root, std::uint32_t seed, AudioOutput *audio = nullptr)
        : GameSession(std::move(root), seed, audio)
    {
        presentation_ = this;
    }
    Game3D(const Game3D &other) : GameSession(other), effects_(other.effects_)
    {
        presentation_ = this;
    }
    Game3D(Game3D &&other) noexcept
        : GameSession(std::move(other)), effects_(std::move(other.effects_))
    {
        presentation_ = this;
    }
    Game3D &operator=(const Game3D &other)
    {
        GameSession::operator=(other);
        effects_ = other.effects_;
        presentation_ = this;
        return *this;
    }
    Game3D &operator=(Game3D &&other) noexcept
    {
        GameSession::operator=(std::move(other));
        effects_ = std::move(other.effects_);
        presentation_ = this;
        return *this;
    }
    explicit Game3D(GameSession &&session) : GameSession(std::move(session))
    {
        presentation_ = this;
    }
    const BattleFx &effects() const { return effects_; }
    Camera3D cameraForPlayer(int index) const
    {
        const auto camera = GameSession::cameraForPlayer(index, GetTime());
        return {toRaylibVector3(camera.position), toRaylibVector3(camera.target),
                toRaylibVector3(camera.up), camera.fovy, CAMERA_ORTHOGRAPHIC};
    }

private:
    friend struct Game3DTestAccess;
    void updateEffects(float dt) override { effects_.update(dt); }
    void clearEffects() override { effects_.clear(); }
    void trackDust(Float3 position, Float3 velocity) override
    {
        effects_.spawnTrackDust(toRaylibVector3(position), toRaylibVector3(velocity));
    }
    void playerMuzzle(const Player &player, const PlayerShellLaunchIntent &intent) override
    {
        const XZ direction = cardinalVector(intent.direction);
        const Vector2 mount = wwii_tank_model::renderedPlayerMuzzle(
            player.nation, intent.playerLevel);
        const XZ muzzle = intent.tankPosition + direction * mount.x;
        effects_.spawnMuzzleFlash({muzzle.x, mount.y, muzzle.z},
                                  {direction.x, 0.0f, direction.z}, playerColor(player.id));
    }
    void enemyMuzzle(Nation nation, const EnemyShellLaunchIntent &intent) override
    {
        const XZ direction = cardinalVector(intent.direction);
        const Vector2 mount = wwii_tank_model::renderedEnemyMuzzle(nation, intent.enemyType);
        const XZ muzzle = intent.tankPosition + direction * mount.x;
        effects_.spawnMuzzleFlash({muzzle.x, mount.y, muzzle.z},
                                  {direction.x, 0.0f, direction.z}, Color{255, 89, 45, 255});
    }
    void impact(Float3 position, Float3 normal, bool heavy) override
    {
        effects_.spawnImpact(toRaylibVector3(position), toRaylibVector3(normal), heavy);
    }
    void brickImpact(Float3 position, Float3 normal, bool power, bool destroyed) override
    {
        effects_.spawnBrickImpact(toRaylibVector3(position), toRaylibVector3(normal),
                                  power, destroyed);
    }
    void explosion(Float3 position, Rgba8 color) override
    {
        effects_.spawnTankExplosion(toRaylibVector3(position), toRaylibColor(color));
    }
    float cameraAspectRatio() const override { return gameplayCameraAspectRatio(); }
    BattleFx effects_;
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

struct SteelTileTriangle
{
    std::array<Vector3, 3> points;
    Vector3 normal;
    Color color;
};

struct SteelTileGeometry
{
    std::vector<SteelTileTriangle> triangles;
    std::size_t shadowTriangleCount = 0;
};

// Record the existing immediate-mode shapes once, including their world-space
// rounding and flat normals. Replaying does not switch shaders or textures.
class SteelTileGeometryBuilder
{
public:
    explicit SteelTileGeometryBuilder(SteelTileGeometry &geometry)
        : geometry_(geometry)
    {
    }

    void pushMatrix()
    {
        assert(matrixDepth_ < matrixStack_.size());
        matrixStack_[matrixDepth_++] = transform_;
    }

    void popMatrix()
    {
        assert(matrixDepth_ > 0);
        transform_ = matrixStack_[--matrixDepth_];
    }

    void translate(float x, float y, float z)
    {
        transform_ = MatrixMultiply(MatrixTranslate(x, y, z), transform_);
    }

    void rotate(float degrees, float x, float y, float z)
    {
        transform_ = MatrixMultiply(MatrixRotate({x, y, z}, DEG2RAD*degrees),
                                    transform_);
    }

    void armoredBlock(Vector3 center, Vector3 size, Color color)
    {
        // Keep the contour and triangle order of base_model::armoredBlock.
        const float x = size.x*0.5f, z = size.z*0.5f;
        const float bevel = std::min(x, z)*0.26f;
        const float shoulder = std::min(size.y*0.22f, 0.13f);
        const std::array<Vector2, 8> contour{{
            {-x + bevel, -z}, {x - bevel, -z},
            {x, -z + bevel}, {x, z - bevel},
            {x - bevel, z}, {-x + bevel, z},
            {-x, z - bevel}, {-x, -z + bevel}}};
        const float bottom = center.y - size.y*0.5f;
        const float top = center.y + size.y*0.5f;
        for (std::size_t index = 0; index < contour.size(); ++index)
        {
            const Vector2 a = contour[index];
            const Vector2 b = contour[(index + 1) % contour.size()];
            const Vector3 lowA{center.x + a.x, bottom, center.z + a.y};
            const Vector3 lowB{center.x + b.x, bottom, center.z + b.y};
            const Vector3 midA{lowA.x, top - shoulder, lowA.z};
            const Vector3 midB{lowB.x, top - shoulder, lowB.z};
            const Vector3 topA{center.x + a.x*0.90f, top, center.z + a.y*0.90f};
            const Vector3 topB{center.x + b.x*0.90f, top, center.z + b.y*0.90f};
            triangle(lowB, lowA, midA, color);
            triangle(lowB, midA, midB, color);
            triangle(midB, midA, topA, color);
            triangle(midB, topA, topB, color);
            triangle({center.x, top, center.z}, topB, topA, color);
            triangle({center.x, bottom, center.z}, lowA, lowB, color);
        }
    }

    void cube(Vector3 center, float width, float height, float length, Color color)
    {
        // Six complete faces, in DrawCube's front/back/top/bottom/right/left
        // order. Like DrawCube, the current texture coordinate is untouched.
        const float x = width/2, y = height/2, z = length/2;
        const std::array<Vector3, 8> corners{{
            {-x, -y, -z}, {x, -y, -z}, {-x, y, -z}, {x, y, -z},
            {-x, -y, z}, {x, -y, z}, {-x, y, z}, {x, y, z}}};
        static constexpr std::array<std::array<int, 6>, 6> faces{{
            {{4, 5, 6, 7, 6, 5}}, {{0, 2, 1, 3, 1, 2}},
            {{2, 6, 7, 3, 2, 7}}, {{0, 5, 4, 1, 5, 0}},
            {{1, 3, 7, 5, 1, 7}}, {{0, 6, 2, 4, 6, 0}}}};
        static constexpr std::array<Vector3, 6> normals{{
            {0, 0, 1}, {0, 0, -1}, {0, 1, 0},
            {0, -1, 0}, {1, 0, 0}, {-1, 0, 0}}};
        pushMatrix();
        translate(center.x, center.y, center.z);
        for (std::size_t face = 0; face < faces.size(); ++face)
            for (int triangle = 0; triangle < 2; ++triangle)
            {
                const auto &indices = faces[face];
                append(corners[indices[triangle*3]],
                       corners[indices[triangle*3 + 1]],
                       corners[indices[triangle*3 + 2]], normals[face], color);
            }
        popMatrix();
    }

private:
    void triangle(Vector3 a, Vector3 b, Vector3 c, Color color)
    {
        const Vector3 u{b.x - a.x, b.y - a.y, b.z - a.z};
        const Vector3 v{c.x - a.x, c.y - a.y, c.z - a.z};
        Vector3 normal{u.y*v.z - u.z*v.y, u.z*v.x - u.x*v.z,
                       u.x*v.y - u.y*v.x};
        const float length = std::sqrt(normal.x*normal.x + normal.y*normal.y +
                                       normal.z*normal.z);
        if (length > 0.00001f)
        {
            normal.x /= length;
            normal.y /= length;
            normal.z /= length;
        }
        append(a, b, c, normal, color);
    }

    void append(Vector3 a, Vector3 b, Vector3 c, Vector3 normal, Color color)
    {
        if (matrixDepth_ > 0)
        {
            a = Vector3Transform(a, transform_);
            b = Vector3Transform(b, transform_);
            c = Vector3Transform(c, transform_);
            // rlNormal3f rotates without renormalizing. Preserve that behavior.
            normal = {transform_.m0*normal.x + transform_.m4*normal.y + transform_.m8*normal.z,
                      transform_.m1*normal.x + transform_.m5*normal.y + transform_.m9*normal.z,
                      transform_.m2*normal.x + transform_.m6*normal.y + transform_.m10*normal.z};
        }
        geometry_.triangles.push_back({{{a, b, c}}, normal, color});
    }

    SteelTileGeometry &geometry_;
    Matrix transform_ = MatrixIdentity();
    std::array<Matrix, 4> matrixStack_{};
    std::size_t matrixDepth_ = 0;
};

SteelTileGeometry buildSteelTileGeometry(int row, int column, bool permanent)
{
    // Cold plated barriers share a clear armor mark. No windows, tiled roof,
    // or warm masonry color competes with the destructible brick buildings.
    SteelTileGeometry geometry;
    geometry.triangles.reserve(permanent ? 1080 : 936);
    SteelTileGeometryBuilder builder(geometry);
    const float x = column + 0.5f;
    const float z = row + 0.5f;
    const auto paint = [](Color color) {
        // The graphic steel response remains readable at the gameplay scale.
        return materialColor(color, 10);
    };
    const Color dark = paint({46, 65, 80, 255});
    const Color body = paint(permanent ? Color{112, 145, 166, 255}
                                      : Color{107, 151, 174, 255});
    const Color light = paint({173, 203, 213, 255});
    const Color edge = paint({76, 110, 134, 255});
    const Color permanentMark = paint({220, 199, 111, 255});
    builder.armoredBlock({x, 0.075f, z}, {0.96f, 0.15f, 0.96f}, dark);
    builder.armoredBlock({x, 0.405f, z}, {0.90f, 0.60f, 0.90f}, body);
    builder.armoredBlock({x, 0.730f, z}, {0.96f, 0.12f, 0.96f}, light);
    for (float sideX : {-1.0f, 1.0f})
        for (float sideZ : {-1.0f, 1.0f})
            builder.armoredBlock({x + sideX*0.401f, 0.40f, z + sideZ*0.401f},
                         {0.10f, 0.60f, 0.10f}, edge);
    builder.armoredBlock({x, 0.801f, z}, {0.72f, 0.045f, 0.72f}, body);
    for (float angle : {-45.0f, 45.0f})
    {
        builder.pushMatrix();
        builder.translate(x, 0.827f, z);
        builder.rotate(angle, 0, 1, 0);
        builder.cube({0, 0, 0}, 0.055f, 0.022f, 0.53f, light);
        builder.popMatrix();
    }
    geometry.shadowTriangleCount = geometry.triangles.size();
    // Crossed structural ribs identify reinforced metal without a letter or
    // pickup-like emblem. Every plate and bolt remains inside its own tile.
    if (permanent)
        for (float side : {-1.0f, 1.0f})
        {
            builder.cube({x + side*0.32f, 0.827f, z}, 0.028f, 0.008f, 0.67f, permanentMark);
            builder.cube({x, 0.827f, z + side*0.32f}, 0.67f, 0.008f, 0.028f, permanentMark);
        }
    for (int face = 0; face < 4; ++face)
    {
        builder.pushMatrix();
        builder.translate(x, 0, z);
        builder.rotate(face*90.0f, 0, 1, 0);
        builder.cube({0, 0.42f, 0.454f}, 0.70f, 0.43f, 0.016f, dark);
        builder.armoredBlock({0, 0.435f, 0.465f}, {0.63f, 0.365f, 0.024f}, body);
        for (float side : {-1.0f, 1.0f})
        {
            for (float y : {0.29f, 0.575f})
                builder.cube({side*0.265f, y, 0.482f},
                         0.036f, 0.036f, 0.018f, light);
        }
        for (float angle : {-52.0f, 52.0f})
        {
            builder.pushMatrix();
            builder.translate(0, 0.435f, 0.484f);
            builder.rotate(angle, 0, 0, 1);
            builder.cube({0, 0, 0}, 0.043f, 0.345f, 0.018f, light);
            builder.popMatrix();
        }
        if (permanent)
            for (float side : {-1.0f, 1.0f})
                builder.cube({side*0.327f, 0.435f, 0.483f},
                         0.025f, 0.30f, 0.006f, permanentMark);
        builder.popMatrix();
    }
    return geometry;
}

class SteelTileGeometryCache
{
    friend struct SteelTileGeometryCacheTestAccess;

public:
    const SteelTileGeometry &get(int row, int column, bool permanent)
    {
        if (row < 0 || row >= kMapSize || column < 0 || column >= kMapSize)
        {
            // Keep diagnostic/gallery coordinates valid without unbounded keys.
            fallback_ = buildSteelTileGeometry(row, column, permanent);
            return fallback_;
        }
        const int key = (permanent ? kMapSize*kMapSize : 0) + row*kMapSize + column;
        const int slot = slots_[key] - 1;
        if (slot >= 0)
        {
            entries_[slot].lastUse = ++clock_;
            return entries_[slot].geometry;
        }
        auto oldest = std::min_element(entries_.begin(), entries_.end(),
            [](const Entry &a, const Entry &b) { return a.lastUse < b.lastUse; });
        if (oldest->key >= 0)
            slots_[oldest->key] = 0;
        oldest->geometry = buildSteelTileGeometry(row, column, permanent);
        oldest->key = key;
        oldest->lastUse = ++clock_;
        slots_[key] = static_cast<int>(oldest - entries_.begin()) + 1;
        return oldest->geometry;
    }

    void clear()
    {
        for (auto &entry : entries_)
            entry = Entry{};
        slots_.fill(0);
        fallback_ = SteelTileGeometry{};
        clock_ = 0;
    }

private:
    struct Entry
    {
        SteelTileGeometry geometry;
        std::uint64_t lastUse = 0;
        int key = -1;
    };
    // The 35 original maps contain at most 176 steel cells. 256 entries keep
    // both whole-map shadow and visible passes warm, with a 14 MiB upper bound.
    // Cells are stage-independent; only cold misses scan this fixed array.
    std::array<Entry, 256> entries_{};
    std::array<int, 2*kMapSize*kMapSize> slots_{};
    SteelTileGeometry fallback_;
    std::uint64_t clock_ = 0;
};

SteelTileGeometryCache &steelTileGeometryCache()
{
    static SteelTileGeometryCache cache;
    return cache;
}

void clearSteelTileGeometryCache()
{
    steelTileGeometryCache().clear();
}

void drawSteelTile(int row, int column, bool permanent,
                   bool shadowPass = false)
{
    const SteelTileGeometry &geometry = steelTileGeometryCache().get(row, column, permanent);
    const std::size_t count = shadowPass ? geometry.shadowTriangleCount :
                                          geometry.triangles.size();
    Color previousColor{};
    Vector3 previousNormal{};
    rlBegin(RL_TRIANGLES);
    for (std::size_t index = 0; index < count; ++index)
    {
        const SteelTileTriangle &triangle = geometry.triangles[index];
        const Color color = shadowPass ? WHITE : triangle.color;
        if (index == 0 || color.r != previousColor.r || color.g != previousColor.g ||
            color.b != previousColor.b || color.a != previousColor.a)
        {
            rlColor4ub(color.r, color.g, color.b, color.a);
            previousColor = color;
        }
        const Vector3 normal = triangle.normal;
        if (index == 0 || normal.x != previousNormal.x || normal.y != previousNormal.y ||
            normal.z != previousNormal.z)
        {
            rlNormal3f(normal.x, normal.y, normal.z);
            previousNormal = normal;
        }
        for (const Vector3 &point : triangle.points)
            rlVertex3f(point.x, point.y, point.z);
    }
    rlEnd();
}

void drawWaterTile(const StageMap &map, int row, int column)
{
    const std::uint32_t seed = static_cast<std::uint32_t>(row + 1) * 92821U ^
                               static_cast<std::uint32_t>(column + 1) * 68917U;
    // Use the matte painted-world response for water: broad blue-green value
    // regions and stepped reflections remain legible after pixel sampling.
    const Color deep = materialColor({31, 87, 128, 255}, 7);
    const Color shallow = materialColor({48, 141, 156, 255}, 7);
    const Color bank = materialColor({118, 143, 118, 255}, 8);
    const Color ripple = materialColor({93, 172, 185, 255}, 7);
    const Color glint = materialColor({163, 211, 204, 255}, 7);
    DrawCube({column + 0.5f, -0.015f, row + 0.5f}, 1.0f, 0.05f, 1.0f, deep);
    const auto patch = [row, column](float x, float z, float width,
                                    float depth, float y, Color color) {
        const float left = column + x, top = row + z;
        rlBegin(RL_QUADS);
        rlColor4ub(color.r, color.g, color.b, color.a);
        rlNormal3f(0, 1, 0);
        rlVertex3f(left, y, top);
        rlVertex3f(left, y, top + depth);
        rlVertex3f(left + width, y, top + depth);
        rlVertex3f(left + width, y, top);
        rlEnd();
    };
    for (int edge = 0; edge < 4; ++edge)
    {
        static constexpr std::array<std::array<int, 2>, 4> neighbor{{
            {{-1, 0}}, {{0, 1}}, {{1, 0}}, {{0, -1}}}};
        if (map.tile(row + neighbor[edge][0], column + neighbor[edge][1]) == '~')
            continue;
        for (int step = 0; step < 3; ++step)
        {
            const float along = step / 3.0f;
            const float depth = 0.09f + 0.025f *
                static_cast<float>((seed >> (edge * 3 + step)) & 3U);
            if (edge == 0 || edge == 2)
                patch(along, edge == 0 ? 0.0f : 1.0f - depth,
                      1.0f/3.0f, depth, 0.014f, shallow);
            else
                patch(edge == 3 ? 0.0f : 1.0f - depth, along,
                      depth, 1.0f/3.0f, 0.014f, shallow);
        }
        if (edge == 0 || edge == 2)
            patch(0, edge == 0 ? 0.0f : 0.974f, 1, 0.026f, 0.018f, bank);
        else
            patch(edge == 3 ? 0.0f : 0.974f, 0, 0.026f, 1, 0.018f, bank);
    }
    const float drift = std::round(std::sin(static_cast<float>(GetTime()) * 0.9f +
                                           (seed & 15U)) * 2.0f) * 0.016f;
    const auto unit = [seed](int shift) {
        return static_cast<float>((seed >> shift) & 7U) / 7.0f;
    };
    const float firstX = 0.21f + unit(2)*0.13f + drift;
    const float firstZ = 0.22f + unit(6)*0.21f;
    const float firstLength = 0.17f + unit(10)*0.13f;
    const float stair = (seed & 32U) != 0U ? 0.027f : -0.026f;
    patch(firstX, firstZ, firstLength, 0.027f, 0.020f, ripple);
    patch(firstX + firstLength*0.68f, firstZ + stair,
          0.07f + unit(13)*0.05f, 0.026f, 0.020f, glint);
    const float secondX = 0.40f + unit(16)*0.13f - drift;
    const float secondZ = 0.57f + unit(19)*0.15f;
    patch(secondX, secondZ, 0.11f + unit(22)*0.11f, 0.024f, 0.020f, ripple);
    patch(secondX - 0.045f, secondZ + 0.024f,
          0.075f + unit(25)*0.03f, 0.024f, 0.020f, glint);
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
                drawWaterTile(map, row, column);
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
    const auto vehicle = enemy ? wwii_tank_model::enemyVehicle(nation, identity)
                               : wwii_tank_model::playerVehicle(nation, armor);
    const auto spec = wwii_tank_model::detail::arcadeVehicleSpec(vehicle);
    const auto art = wwii_tank_model::detail::visualProfileForVehicle(vehicle);
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
                      enemy.type, enemy.moving, game.enemyNation(enemy.id), true);
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
                          false, player.level, 0.0f, player.id,
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
                              game.enemyNation(enemy.id), enemy.armor);
        if (tankAssets.gltfProbeEnabled())
            drawGltfProbeFootprint(enemy.position,
                                   Color{255, 86, 72, 255});
        drawTankModel(tankAssets, enemy.position, enemy.yaw, body,
                      true, enemy.armor, 0.0f, enemy.type,
                      enemy.moving,
                      game.enemyNation(enemy.id));
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

void drawTankProtection(const Game3D &game)
{
    // Protection is an unlit overlay, not a lit part of the tank's armor.
    rlDrawRenderBatchActive();
    rlDisableDepthMask();
    BeginBlendMode(BLEND_ADDITIVE);
    for (const Player &player : game.players())
    {
        if (!player.active || player.shieldTimer <= 0.0f)
            continue;
        rlPushMatrix();
        rlTranslatef(player.position.x, 0.0f, player.position.z);
        wwii_tank_model::detail::drawShield(player.shieldTimer, player.id);
        rlPopMatrix();
    }
    EndBlendMode();
    rlDrawRenderBatchActive();
    rlEnableDepthMask();
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

tanks3d::app::ShellFlightPresentation renderedShellFlight(const Game3D &game,
                                                         const Shell &shell)
{
    // Owners can turn or be destroyed after firing. Only their vehicle identity
    // selects the neutral mount; flight distance comes from the shell itself.
    Vector2 mount{kShellSpawnDistance, 0.67f};
    if (shell.owner == ShellOwner::Player)
    {
        for (const Player &player : game.players())
            if (player.id == shell.ownerIndex)
            {
                mount = wwii_tank_model::renderedPlayerMuzzle(player.nation, player.level);
                break;
            }
    }
    else
    {
        for (const Enemy &enemy : game.enemies())
            if (enemy.id == shell.ownerIndex)
            {
                mount = wwii_tank_model::renderedEnemyMuzzle(game.enemyNation(enemy.id), enemy.type);
                break;
            }
    }
    return tanks3d::app::shellFlightPresentation(
        shell.life, std::sqrt(lengthSquared(shell.velocity)), shell.impacting,
        kShellSpawnDistance, mount.x, mount.y, 0.67f);
}

void drawEmissiveBattleFx(const Game3D &game, const Camera3D &camera)
{
    // The physical penetrator is deliberately readable without bloom. Its
    // gameplay AABB remains the classic half-tile sprite footprint, while this
    // smaller visible body avoids looking like a glowing ball.
    BeginBlendMode(BLEND_ALPHA);
    for (const Shell &shell : game.shells())
    {
        const auto flight = renderedShellFlight(game, shell);
        if (!flight.visible)
            continue;
        const float velocityLength = std::max(0.001f, std::sqrt(lengthSquared(shell.velocity)));
        const XZ direction = shell.velocity * (1.0f / velocityLength);
        const float radius = shell.power ? 0.095f : 0.074f;
        const float bodyLength = shell.power ? 0.30f : 0.23f;
        const Vector3 nose{shell.position.x, flight.height, shell.position.z};
        const Vector3 base{shell.position.x - direction.x * bodyLength,
                           flight.height,
                           shell.position.z - direction.z * bodyLength};
        const Vector3 shoulder{shell.position.x - direction.x * bodyLength * 0.28f,
                                flight.height,
                                shell.position.z - direction.z * bodyLength * 0.28f};
        const Color metal = shell.owner == ShellOwner::Player
                                ? Color{183, 147, 77, 255}
                                : Color{151, 154, 141, 255};
        DrawCylinderEx(base, shoulder, radius, radius, 7, metal);
        DrawCylinderEx(shoulder, nose, radius, radius * 0.12f, 7,
                       Color{235, 212, 149, 255});
        const Vector3 bandRear{
            shell.position.x - direction.x * bodyLength * 0.82f, flight.height,
            shell.position.z - direction.z * bodyLength * 0.82f};
        const Vector3 bandFront{
            shell.position.x - direction.x * bodyLength * 0.66f, flight.height,
            shell.position.z - direction.z * bodyLength * 0.66f};
        DrawCylinderEx(bandRear, bandFront, radius * 1.06f,
                       radius * 1.06f, 7, Color{76, 70, 51, 255});
    }
    EndBlendMode();

    rlDrawRenderBatchActive();
    rlDisableDepthMask();
    BeginBlendMode(BLEND_ADDITIVE);
    for (const Shell &shell : game.shells())
    {
        const auto flight = renderedShellFlight(game, shell);
        if (!flight.visible)
            continue;
        const Color color = shell.owner == ShellOwner::Player
                                ? Color{255, 178, 54, 255}
                                : Color{255, 104, 43, 255};
        const float velocityLength = std::max(0.001f, std::sqrt(lengthSquared(shell.velocity)));
        const XZ direction = shell.velocity * (1.0f / velocityLength);
        const float bodyLength = shell.power ? 0.30f : 0.23f;
        const Vector3 flameBase{shell.position.x - direction.x * bodyLength,
                                flight.height,
                                shell.position.z - direction.z * bodyLength};
        const Vector3 hotTail{flameBase.x - direction.x * (shell.power ? 0.19f : 0.14f),
                              flight.height,
                              flameBase.z - direction.z * (shell.power ? 0.19f : 0.14f)};
        const Vector3 tracerTail{hotTail.x - direction.x * (shell.power ? 0.15f : 0.11f),
                                 flight.height,
                                 hotTail.z - direction.z * (shell.power ? 0.15f : 0.11f)};

        DrawCylinderEx(hotTail, flameBase, 0.007f,
                       shell.power ? 0.050f : 0.038f, 7, Fade(color, 0.78f));
        const Vector3 coreTail{hotTail.x + direction.x * 0.07f, flight.height,
                               hotTail.z + direction.z * 0.07f};
        DrawCylinderEx(coreTail, flameBase, 0.004f,
                       shell.power ? 0.023f : 0.017f, 6,
                       Color{255, 236, 164, 180});
        DrawCylinderEx(tracerTail, hotTail, 0.002f, 0.011f, 5,
                       Fade(color, 0.26f));
    }
    EndBlendMode();
    rlDrawRenderBatchActive();
    rlEnableDepthMask();
    game.effects().draw(camera);

    rlDrawRenderBatchActive();
    rlDisableDepthMask();
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
    rlDrawRenderBatchActive();
    rlEnableDepthMask();
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
    int mapY = 0;
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
    // Match the Godot radar's existing 168 x 196 lower-right footprint.
    // This is screen-space presentation only, never camera framing input.
    layout.mapX = originX + width - 184 + (168 - mapPixels) / 2;
    layout.mapY = static_cast<int>(viewport.y + viewport.height) - 158;
    layout.panelWidth = std::max(1, std::min(320, (width - 28) / count));
    layout.panelTextX = index == 0 ? originX + 14
                                  : originX + width - layout.panelWidth;
    const int footerSpace = std::max(1, width - 200);
    const int footerLeft = originX + footerSpace * index / count;
    const int footerRight = originX + footerSpace * (index + 1) / count;
    layout.footerCenterX = (footerLeft + footerRight) / 2;
    layout.footerWidth = std::max(1, footerRight - footerLeft - 28);
    return layout;
}

void drawViewportHud(const Game3D &game, int playerIndex, Rectangle viewport,
                     bool showFrameRate, bool showControlHints = true,
                     bool aiControlled = false)
{
    const Player &player = game.players()[playerIndex];
    const Color accent = playerColor(playerIndex);
    const int width = static_cast<int>(viewport.width);
    const ViewportHudLayout layout = viewportHudLayout(
        viewport, game.playerCount(), playerIndex);
    const int panelWidth = layout.panelWidth;
    const int contentWidth = panelWidth - 14;
    const int left = layout.panelTextX;
    const int top = static_cast<int>(viewport.y) + 12;
    const bool streakActive = player.directKillStreak > 0;
    std::string stateLine;
    if (!player.active)
        stateLine = player.lives > 0 ? "RESPAWNING" : "OUT";
    else
    {
        if (player.shieldTimer > 0.0f)
            stateLine = "SHIELD";
        if (player.hasBoat)
            stateLine += stateLine.empty() ? "BOAT" : "  BOAT";
    }
    const int panelHeight = streakActive || !stateLine.empty() ? 102 : 80;
    DrawRectangle(left - 7, top - 5, panelWidth,
                  panelHeight, Color{5, 8, 11, 185});
    const auto column = [&](const std::string &text, int row, int start,
                            int available, int preferred, int minimum,
                            Color color, bool rightAligned = false)
    {
        const int fontSize = fittedFontSize(text, available, preferred, minimum);
        const int x = left + start + (rightAligned
            ? available - MeasureText(text.c_str(), fontSize) : 0);
        drawTextShadow(text, x, top + row, fontSize, color);
    };
    const std::string nationLine = "P" + std::to_string(playerIndex + 1) +
                                   (aiControlled ? " AI  " : "  ") + nationName(player.nation);
    column(nationLine, 0, 0, contentWidth - 106, 18, 16, accent);
    column("LIVES " + std::to_string(std::max(0, player.lives)),
           2, contentWidth - 98, 98, 14, 14, RAYWHITE, true);
    column(wwii_tank_model::playerVehicleName(player.nation, player.level),
           26, 0, contentWidth - 120, 14, 14,
           player.level >= 3 ? GOLD : ORANGE);
    const Color hpColor = player.hitPoints <= 0
                              ? Color{135, 142, 148, 255}
                          : player.hitPoints >= player.maximumHitPoints
                              ? Color{98, 232, 129, 255}
                          : player.hitPoints * 2 >= player.maximumHitPoints
                              ? Color{255, 207, 83, 255}
                              : Color{255, 99, 75, 255};
    column("HP  " + std::to_string(std::max(0, player.hitPoints)) + " / " +
               std::to_string(player.maximumHitPoints),
           24, contentWidth - 112, 112, 16, 16, hpColor, true);
    const int tierWidth = contentWidth * 168 / 306;
    column("TIER " + std::to_string(player.level + 1) + "/4  " +
               wwii_tank_model::tierName(player.level),
           51, 0, tierWidth, 13, 12, LIGHTGRAY);
    column("SCORE " + std::to_string(player.score),
           51, tierWidth + 6, contentWidth - tierWidth - 6,
           13, 12, RAYWHITE, true);
    if (streakActive)
        column("STREAK  x" + std::to_string(player.directKillStreak),
               75, 0, contentWidth - 132, 14, 14,
               ColorLerp(accent, WHITE, 0.30f));
    if (!stateLine.empty())
        column(stateLine, 75, contentWidth - 124, 124, 14, 14, LIGHTGRAY, true);
    if (playerIndex == 0)
    {
        const int radarX = static_cast<int>(viewport.x) + width - 184;
        const int radarY = static_cast<int>(viewport.y + viewport.height) - 208;
        DrawRectangle(radarX, radarY, 168, 196, Color{14, 22, 23, 235});
        DrawRectangleLines(radarX, radarY, 168, 196, Color{83, 99, 103, 255});
        std::ostringstream mission;
        mission << "STAGE " << std::setfill('0') << std::setw(2) << game.stage()
                << "  ENEMY " << std::setw(2) << game.enemiesLeft();
        drawTextShadow(mission.str(), radarX + 14, radarY + 10, 12,
                       Color{225, 230, 230, 255});
        std::string baseLine = game.baseAlive() ? "HEADQUARTERS" : "BASE LOST";
        if (game.map().governmentWallsSteel())
        {
            std::ostringstream timer;
            timer << "BASE STEEL " << std::fixed << std::setprecision(1)
                  << game.map().governmentSteelTimeRemaining() << "s";
            baseLine = timer.str();
        }
        drawTextShadow(baseLine, radarX + 14, radarY + 22, 12,
                       Color{225, 230, 230, 255});
        drawMiniMap(game, layout.mapX, layout.mapY, layout.mapCellSize);
        if (showFrameRate)
            drawTextShadow("FPS " + std::to_string(GetFPS()),
                           static_cast<int>(viewport.x) + 14,
                           static_cast<int>(viewport.y + viewport.height) - 76,
                           12, Color{139, 218, 235, 255});
    }

    if (!showControlHints)
        return;
    if (aiControlled)
    {
        drawCenteredText("AI TEAMMATE", layout.footerCenterX,
                         static_cast<int>(viewport.y + viewport.height) - 28,
                         16, accent);
        return;
    }
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
        // the center line or the lower-right radar in a narrow co-op window.
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
                bool showFrameRate, const std::string &lanStatus = "",
                bool aiPlayer2 = false)
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
        drawTankProtection(game);
        drawEnemyCreationWarnings(game, sharedCamera);
        for (const Pickup &pickup : game.bonuses())
            bonusAssets.draw(pickup, sharedCamera,
                             static_cast<float>(GetTime()));
        drawEmissiveBattleFx(game, sharedCamera);
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
        drawViewportHud(game, index, destination, showFrameRate, lanStatus.empty(),
                        aiPlayer2 && index == 1);
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
        const std::string exitHint = lanStatus.empty()
                                         ? "MINUS / ESC: SETUP"
                                         : "MINUS / ESC: LEAVE ROOM";
        if (game.gameOver())
            drawCenteredText("BATTLE REPORT SOON    " + exitHint,
                             screenWidth / 2, screenHeight / 2 + 24, 21,
                             LIGHTGRAY);
        else if (game.stageIntro())
            drawCenteredText("GET READY", screenWidth / 2,
                             screenHeight / 2 + 24, 21, LIGHTGRAY);
        else if (game.paused())
            drawCenteredText("PLUS / ENTER: RESUME    " + exitHint,
                             screenWidth / 2, screenHeight / 2 + 24, 21,
                             LIGHTGRAY);
    }
    tanks3d::drawLanStatus(lanStatus);
    EndDrawing();
    return true;
}

struct MenuSettings
{
    int playerCount = 1;
    bool aiPlayer2 = false;
    int stage = 1;
    int lives = 10;
    std::array<Nation, 2> nations{{Nation::UnitedStates, Nation::SovietUnion}};
    AdvancedGameSettings advanced{};
    int cameraYawDegrees = 0;
    int cameraElevationDegrees = kDefaultCameraElevationDegrees;
    bool pixelStyleEnabled = false;
    int selected = 0;
    int advancedSelected = 0;
    bool advancedOpen = false;
    int connectedGamepads = 0;
};

std::string menuPlayerModeLabel(const MenuSettings &settings)
{
    return settings.aiPlayer2 ? "AI AS P2"
                             : (settings.playerCount == 1 ? "1 PLAYER" : "2 PLAYERS");
}

int menuRowCount(const MenuSettings &settings)
{
    return 6 + (settings.playerCount == 2 ? 1 : 0);
}

int advancedMenuRow(const MenuSettings &settings)
{
    return menuRowCount(settings) - 1;
}

int lanMenuRow(const MenuSettings &settings)
{
    return menuRowCount(settings) - 2;
}

bool updateMenu(MenuSettings &settings, const UiInputFrame &input)
{
    bool changed = false;
    int rowCount = menuRowCount(settings);
    const int previousPlayerCount = settings.playerCount;
    const bool advancedWasSelected =
        settings.selected == advancedMenuRow(settings);
    const bool lanWasSelected = settings.selected == lanMenuRow(settings);
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
            const int mode = settings.aiPlayer2 ? 2 : settings.playerCount - 1;
            const int next = (mode + direction + 3) % 3;
            settings.playerCount = next == 0 ? 1 : 2;
            settings.aiPlayer2 = next == 2;
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
        changed = changed || settings.playerCount != 1 || settings.aiPlayer2;
        settings.playerCount = 1;
        settings.aiPlayer2 = false;
    }
    if (input.selectTwoPlayerPressed)
    {
        changed = changed || settings.playerCount != 2 || settings.aiPlayer2;
        settings.playerCount = 2;
        settings.aiPlayer2 = false;
    }
    rowCount = menuRowCount(settings);
    settings.selected = advancedWasSelected &&
                                settings.playerCount != previousPlayerCount
                            ? advancedMenuRow(settings)
                        : lanWasSelected && settings.playerCount != previousPlayerCount
                            ? lanMenuRow(settings)
                            : std::clamp(settings.selected, 0, rowCount - 1);
    return changed;
}

constexpr int kAdvancedMenuPixelStyleRow = 6;
constexpr int kAdvancedMenuRowCount = 8;
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
               kDefaultCameraElevationDegrees &&
           !settings.pixelStyleEnabled;
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

    if (settings.advancedSelected == kAdvancedMenuPixelStyleRow &&
        (direction != 0 || input.confirmPressed))
    {
        settings.pixelStyleEnabled = !settings.pixelStyleEnabled;
        changed = true;
    }

    if (input.resetPressed)
    {
        changed = changed || !advancedSettingsAreDefault(settings);
        settings.advanced = {};
        settings.cameraYawDegrees = 0;
        settings.cameraElevationDegrees =
            kDefaultCameraElevationDegrees;
        settings.pixelStyleEnabled = false;
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
    const int rowHeight = compact ? 34 :
        (settings.playerCount == 2 && height < 760 ? 42 : 46);
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
    std::vector<std::string> values{{menuPlayerModeLabel(settings),
                                     std::to_string(settings.stage),
                                     std::to_string(settings.lives),
                                     nationName(settings.nations[0])}};
    if (settings.playerCount == 2)
    {
        labels.push_back("P2 NATION");
        values.push_back(nationName(settings.nations[1]));
    }
    labels.push_back("LOCAL NETWORK");
    values.push_back("OPEN");
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
        if (selected && index != advancedMenuRow(settings) && index != lanMenuRow(settings))
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
    drawCenteredText(settings.aiPlayer2
                         ? "P1  YOU: PAD / ARROWS     P2  AI TEAMMATE"
                         : "PLAYERS: 1 PLAYER / 2 PLAYERS / AI AS P2", width / 2,
                     helpY + (compact ? 48 : 67), compact ? 14 : 16,
                     Color{189, 210, 214, 255});
    const std::string deviceLine =
        "GAMEPADS " + std::to_string(settings.connectedGamepads) +
        (settings.aiPlayer2 ? "     AI MODE: PAD 1/2 CONTROLS P1     "
                           : "     1P: PAD 1/2     2P: FIRST=P1 SECOND=P2     ") +
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
    const int rowHeight = compact ? 28 : 40;
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
        "VIEW HORIZONTAL", "VIEW ELEVATION", "PIXEL STYLE", "BACK TO SETUP"}};
    const std::array<std::string, kAdvancedMenuRowCount> values{{
        settings.advanced.playerMaximumHitPoints == 1
            ? "1  BANDAGE OFF"
            : std::to_string(settings.advanced.playerMaximumHitPoints),
        percentageLabel(settings.advanced.enemySpeedPercent),
        percentageLabel(settings.advanced.enemyFireRatePercent),
        percentageLabel(settings.advanced.enemySpawnRatePercent),
        cameraYawLabel(settings.cameraYawDegrees),
        cameraElevationLabel(settings.cameraElevationDegrees),
        settings.pixelStyleEnabled ? "ON" : "OFF",
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
        const std::string label =
            (selected ? ">  " : "   ") + labels[index];
        const int labelFont = fittedFontSize(
            label, valueCenter - 170 - (panelX + 38) - 12,
            compact ? 18 : 21, 13);
        drawTextShadow(label, panelX + 38, y, labelFont,
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
    drawCenteredText("BOTTOM FACE / ENTER: TOGGLE / SELECT    MINUS / ESC: RETURN    TOP FACE / R: RESET",
                     width / 2, helpY + (compact ? 24 : 32),
                     fittedFontSize(
                         "BOTTOM FACE / ENTER: TOGGLE / SELECT    MINUS / ESC: RETURN    TOP FACE / R: RESET",
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
    tanks3d::app::AiPlayerController aiPlayer;
    ViewTargets viewTargets;
    tanks3d::net::TcpChannel lanChannel;
    tanks3d::app::LanSession lanSession(lanChannel, tanks3d::net::executableFingerprint());
    tanks3d::LanMenu lanMenu;
    bool lanOpen = false;
    bool lanAutoHost = false;
    std::string lanAutoJoin;
    std::uint16_t lanPort = tanks3d::net::kLanPort;
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
        if (value == "--lan-host" || value.rfind("--lan-host=", 0) == 0)
        {
            lanAutoHost = true;
            if (value != "--lan-host")
            {
                tanks3d::net::Endpoint endpoint;
                if (!tanks3d::net::parseEndpoint("127.0.0.1:" + value.substr(11),
                                                 endpoint))
                {
                    std::cerr << "Invalid LAN port\n";
                    return 2;
                }
                lanPort = endpoint.port;
            }
        }
        else if (value.rfind("--lan-join=", 0) == 0)
        {
            tanks3d::net::Endpoint endpoint;
            lanAutoJoin = value.substr(11);
            if (!tanks3d::net::parseEndpoint(lanAutoJoin, endpoint))
            {
                std::cerr << "--lan-join requires an IPv4 address[:port]\n";
                return 2;
            }
        }
        else if (value.rfind("--lan-", 0) == 0)
        {
            std::cerr << "Use --lan-host[=port] or --lan-join=IPv4[:port]\n";
            return 2;
        }
        else if (value.rfind("--stage=", 0) == 0)
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
            settings.playerCount = 1;
            settings.aiPlayer2 = false;
            inGame = game.start(1, settings.lives, settings.stage,
                                settings.nations, settings.advanced,
                                settings.cameraYawDegrees,
                                settings.cameraElevationDegrees);
        }
        else if (value == "--quick-start-2p" || value == "--quick-start-ai")
        {
            settings.playerCount = 2;
            settings.aiPlayer2 = value == "--quick-start-ai";
            inGame = game.start(2, settings.lives, settings.stage,
                                settings.nations, settings.advanced,
                                settings.cameraYawDegrees,
                                settings.cameraElevationDegrees);
        }
        else if (value == "--quick-start-ussr")
        {
            settings.playerCount = 1;
            settings.aiPlayer2 = false;
            settings.nations[0] = Nation::SovietUnion;
            inGame = game.start(1, settings.lives, settings.stage,
                                settings.nations, settings.advanced,
                                settings.cameraYawDegrees,
                                settings.cameraElevationDegrees);
        }
        else if (value == "--quick-start-germany")
        {
            settings.playerCount = 1;
            settings.aiPlayer2 = false;
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

    const auto hostLanRoom = [&]()
    {
        tanks3d::net::RoomSettings room;
        room.seed = static_cast<std::uint32_t>(std::random_device{}());
        room.stage = settings.stage;
        room.lives = settings.lives;
        room.nations = settings.nations;
        room.advanced = settings.advanced;
        lanMenu.hostAddresses = tanks3d::net::localIpv4Addresses();
        lanMenu.error.clear();
        lanSession.host(room, GetTime(), lanPort);
    };
    if (lanAutoHost || !lanAutoJoin.empty())
    {
        if ((lanAutoHost && !lanAutoJoin.empty()) || inGame || bonusShowcase ||
            tankShowcase || settlementShowcase || baseDamageShowcase ||
            baseSteelShowcase || enemyCreationShowcase || forestCoverShowcase ||
            settings.advancedOpen || releaseScreenshot.requested() ||
            releasePerformance.requested())
        {
            std::cerr << "Use one LAN mode, without quick-start or preview modes\n";
            return 2;
        }
        lanOpen = true;
        SetWindowState(FLAG_WINDOW_ALWAYS_RUN);
        if (lanAutoHost)
            hostLanRoom();
        else
        {
            lanMenu.address = lanAutoJoin;
            tanks3d::net::Endpoint endpoint;
            tanks3d::net::parseEndpoint(lanAutoJoin, endpoint);
            lanSession.join(endpoint, settings.nations[0], GetTime());
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

        if (lanOpen)
        {
            using tanks3d::LanMenuAction;
            using tanks3d::app::LanPhase;
            const bool focused = IsWindowFocused();
            const UiInputFrame lanUi = focused ? uiInput : UiInputFrame{};
            const auto phase = lanSession.phase();
            if (phase == LanPhase::Idle || phase == LanPhase::Failed)
            {
                inGame = false;
                const auto action = lanMenu.update(lanUi);
                if (action == LanMenuAction::Back)
                {
                    lanSession.stop();
                    lanOpen = false;
                    ClearWindowState(FLAG_WINDOW_ALWAYS_RUN);
                    drawMenu(settings, menuError);
                    continue;
                }
                if (action == LanMenuAction::Host)
                    hostLanRoom();
                if (action == LanMenuAction::Join)
                {
                    tanks3d::net::Endpoint endpoint;
                    if (tanks3d::net::parseEndpoint(lanMenu.address, endpoint))
                    {
                        lanMenu.error.clear();
                        lanSession.join(endpoint, settings.nations[0], GetTime());
                    }
                    else
                        lanMenu.error = "Enter the host IPv4 address, optionally "
                                        "followed by :port.";
                }
            }
            else if (lanUi.cancelPressed)
            {
                lanSession.stop();
                lanOpen = inGame = false;
                audio.stopAll();
                viewTargets.release();
                ClearWindowState(FLAG_WINDOW_ALWAYS_RUN);
                drawMenu(settings, menuError);
                continue;
            }
            PlayerInputFrame localFrame = readRaylibPlayerInputFrame();
            mergePlayerControlFrame(localFrame.players[0], localFrame.players[1]);
            for (const auto &controls : gamepadFrame.players.players)
                mergePlayerControlFrame(localFrame.players[0], controls);
            if (!focused)
                localFrame = {};
            std::uint8_t controls = 0;
            if (lanUi.pausePressed)
                controls |= tanks3d::net::Control::Pause;
            if (lanUi.confirmPressed)
                controls |= tanks3d::net::Control::Confirm;
            if (lanSession.isHost() && lanUi.restartPressed)
                controls |= tanks3d::net::Control::Restart;
            lanSession.update(GetTime(), localFrame.players[0], controls);
            if (const auto room = lanSession.takeStart())
            {
                audio.stopAll();
                viewTargets.release();
                game = Game3D(resourceRoot, room->seed, &audio);
                if (game.start(2, room->lives, room->stage, room->nations,
                               room->advanced, settings.cameraYawDegrees,
                               settings.cameraElevationDegrees))
                {
                    inGame = true;
                    lanSession.ready();
                }
                else
                    lanSession.abort(tanks3d::net::LeaveReason::CannotStart);
            }
            tanks3d::net::Packet tick;
            while (lanSession.nextTick(tick))
            {
                const auto result = tanks3d::app::applyLanTick(game, tick);
                if (result != tanks3d::app::LanTickResult::Continue)
                {
                    if (result == tanks3d::app::LanTickResult::CannotLoad)
                        lanSession.abort(tanks3d::net::LeaveReason::CannotStart);
                    else
                        lanSession.stop();
                    inGame = false;
                    audio.stopAll();
                    viewTargets.release();
                    break;
                }
                if (tick.tick % tanks3d::net::kDigestInterval == 0)
                    lanSession.recordDigest(
                        tanks3d::net::stateHash(game.sessionDigest().state));
            }
            lanSession.flush();
            if (lanSession.phase() == LanPhase::Playing)
            {
                tankAssets.setAnimationClock(GetTime());
                postProcess.setPixelStyleEnabled(settings.pixelStyleEnabled);
                if (lanSession.waitingForPeer())
                    audio.updateEngine(false, false);
                const std::string status =
                    lanSession.waitingForPeer()
                        ? "LAN: WAITING FOR THE OTHER COMPUTER..."
                        : (lanSession.isHost() ? "LAN HOST - YOU ARE P1 (GOLD)"
                                               : "LAN GUEST - YOU ARE P2 (GREEN)");
                if (!renderGame(game, viewTargets, lighting, tankAssets, environment,
                                bonusAssets, postProcess, true, status))
                    lanSession.abort(tanks3d::net::LeaveReason::CannotStart);
            }
            else
            {
                if (lanSession.phase() == LanPhase::Failed)
                {
                    inGame = false;
                    audio.stopAll();
                    viewTargets.release();
                }
                lanMenu.draw(lanSession, settings.stage, settings.lives,
                             nationName(settings.nations[0]), lanPort);
            }
            continue;
        }

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
                if (settings.selected == lanMenuRow(settings))
                {
                    lanOpen = true;
                    lanMenu.error.clear();
                    SetWindowState(FLAG_WINDOW_ALWAYS_RUN);
                    lanMenu.draw(lanSession, settings.stage, settings.lives,
                                 nationName(settings.nations[0]), lanPort);
                    continue;
                }
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
                    aiPlayer.reset();
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
            if (!game.endingSequence() && uiInput.restartPressed)
            {
                if (game.restart())
                    aiPlayer.reset();
                else
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
            if (game.playerCount() == 1 || settings.aiPlayer2)
            {
                // Either menu-capable pad controls the sole human in solo/AI
                // mode; human two-player games retain P1/P2 isolation.
                mergePlayerControlFrame(
                    inputFrame.players[0],
                    gamepadFrame.players.players[1]);
            }
            if (settings.aiPlayer2)
            {
                const bool battleRunning = !game.paused() && !game.stageIntro() &&
                    !game.gameOver() && !game.settling() && !game.highScoreDisplay();
                aiPlayer.update(dt, battleRunning, game.stage(), game.map(),
                                game.players(), game.enemies(), inputFrame);
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
        postProcess.setPixelStyleEnabled(settings.pixelStyleEnabled);
        if (!renderGame(game, viewTargets, lighting, tankAssets, environment,
                        bonusAssets, postProcess,
                        !releaseScreenshot.requested(), "", settings.aiPlayer2))
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

    lanSession.stop();
    viewTargets.release();
    audio.updateEngine(false, false);
    audio.unload();
    tankAssets.unload();
    bonusAssets.unload();
    environment.unload();
    clearSteelTileGeometryCache();
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
