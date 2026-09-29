// Engine-independent adapter over the canonical production session.
#include "app/game_session.h"
#include "game/vehicle_identity.h"
#include "app/ai_player.h"
#include "app/lan_game_bridge.h"
#include "app/lan_session.h"
#include "sample_core.h"

#include <iomanip>
#include <memory>
#include <stdexcept>
#include <dlfcn.h>
#include <fstream>

namespace tanks3d::godot_sample
{
namespace fs = std::filesystem;
using namespace tanks3d::app;
using namespace tanks3d::core;
using namespace tanks3d::game;
constexpr unsigned kInputBits = 127U;
constexpr unsigned kConfirm = 32U;
constexpr unsigned kPause = 64U;
thread_local std::string createError;

TanksSampleConfig defaultConfig()
{
    return {1, 1, 0, 3, 0, 1, 3, 0, 0, 0, 0, kDefaultCameraElevationDegrees};
}

void validateConfig(const TanksSampleConfig &config)
{
    const auto tuning = [](int value)
    {
        return value >= tanks3d::core::kEnemyTuningMinimumPercent &&
               value <= tanks3d::core::kEnemyTuningMaximumPercent &&
               value % tanks3d::core::kEnemyTuningPercentStep == 0;
    };
    if (config.stage < 1 || config.stage > kStageCount || config.players < 1 || config.players > 2 ||
        (config.ai_p2 != 0 && config.ai_p2 != 1) || (config.ai_p2 && config.players != 2) ||
        config.lives < 1 || config.lives > 99 || config.nation_p1 < 0 || config.nation_p1 >= 3 ||
        config.nation_p2 < 0 || config.nation_p2 >= 3 ||
        config.max_hp < kMinimumPlayerMaximumHitPoints || config.max_hp > kMaximumPlayerMaximumHitPoints ||
        !tuning(config.enemy_speed) || !tuning(config.enemy_fire) || !tuning(config.enemy_spawn) ||
        config.camera_yaw < kCameraYawMinimumDegrees || config.camera_yaw > kCameraYawMaximumDegrees ||
        config.camera_yaw % kCameraYawStepDegrees != 0 ||
        config.camera_elevation < kCameraElevationMinimumDegrees ||
        config.camera_elevation > kCameraElevationMaximumDegrees ||
        config.camera_elevation % kCameraElevationStepDegrees != 0)
        throw std::invalid_argument("Invalid sample settings or unsupported setting increment");
}

struct SampleLan
{
    std::unique_ptr<tanks3d::net::Channel> channel;
    tanks3d::app::LanSession session;
    TanksSampleConfig config;
    unsigned previous = 0;
    double lastNow = 0;
    std::vector<std::string> addresses = tanks3d::net::localIpv4Addresses();

    SampleLan(std::unique_ptr<tanks3d::net::Channel> transport, std::string build,
              const TanksSampleConfig &settings, double now)
        : channel(std::move(transport)), session(*channel, std::move(build)), config(settings), lastNow(now) {}
};

struct AudioCommand
{
    enum class Operation { Play, Engine, Stop };
    Operation operation;
    tanks3d::audio::AudioCue cue = tanks3d::audio::AudioCue::StageStart;
    bool active = false;
    bool moving = false;
};

// An ordered output queue, deliberately outside the deterministic world. The
// engine frontend drains this after stepping (including report/menu phases).
// Identical adjacent engine updates can be coalesced; play/stop are barriers.
struct SampleAudio final : tanks3d::audio::AudioOutput
{
    std::vector<AudioCommand> commands;

    void play(tanks3d::audio::AudioCue cue) override
    {
        commands.push_back({AudioCommand::Operation::Play, cue});
    }
    void updateEngine(bool active, bool moving) override
    {
        if (!commands.empty())
        {
            const auto &last = commands.back();
            if (last.operation == AudioCommand::Operation::Engine &&
                last.active == active && last.moving == moving)
                return;
        }
        commands.push_back({AudioCommand::Operation::Engine,
                            tanks3d::audio::AudioCue::StageStart, active, moving});
    }
    void stopAll() override
    {
        commands.push_back({AudioCommand::Operation::Stop});
    }
};

struct Sample
{
    fs::path resources;
    std::unique_ptr<SampleAudio> audio; // Must outlive the session holding its pointer.
    std::unique_ptr<GameSession> game;
    std::string audioSnapshot;
    tanks3d::app::AiPlayerController ai;
    std::array<unsigned, 2> previous{};
    std::uint64_t tick = 0;
    bool aiP2 = false;
    bool menuRequested = false;
    std::string error;
    std::string snapshot;
    TanksSampleConfig config = defaultConfig();
    std::unique_ptr<SampleLan> lan;
    std::vector<GameEvent> events;
    std::string lanStatus;
    std::array<tanks3d::app::GamepadInputState, 4> padStates{};
    std::array<unsigned, 4> padButtons{};

    explicit Sample(fs::path root) : resources(std::move(root)) {}
};

void commitGame(Sample &sample, std::unique_ptr<GameSession> game, const TanksSampleConfig &config)
{
    sample.game = std::move(game);
    sample.config = config;
    sample.ai.reset();
    sample.previous = {};
    sample.tick = 0;
    sample.aiP2 = config.ai_p2 != 0;
    sample.menuRequested = false;
    sample.snapshot.clear();
    sample.events.clear();
    sample.padStates = {};
    sample.padButtons = {};
}

void configureGame(GameSession &game, const TanksSampleConfig &config)
{
    const AdvancedGameSettings advanced{config.max_hp, config.enemy_speed,
                                         config.enemy_fire, config.enemy_spawn};
    if (!game.start(config.players, config.lives, config.stage,
                    {{static_cast<Nation>(config.nation_p1), static_cast<Nation>(config.nation_p2)}},
                    advanced, config.camera_yaw, config.camera_elevation))
        throw std::runtime_error(game.lastError());
}

void reset(Sample &sample, int seed, const TanksSampleConfig &config)
{
    validateConfig(config);
    auto audio = std::make_unique<SampleAudio>();
    auto game = std::make_unique<GameSession>(sample.resources, static_cast<std::uint32_t>(seed), audio.get());
    configureGame(*game, config);
    commitGame(sample, std::move(game), config);
    sample.audio = std::move(audio);
    sample.audioSnapshot.clear();
}

std::string jsonString(const std::string &value)
{
    std::ostringstream out;
    out << '"';
    for (unsigned char byte : value)
    {
        if (byte == '"' || byte == '\\')
            out << '\\' << static_cast<char>(byte);
        else if (byte < 32)
            out << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<unsigned>(byte) << std::dec;
        else
            out << static_cast<char>(byte);
    }
    out << '"';
    return out.str();
}

std::string lanFingerprint()
{
    // In Godot, the executable identifies the engine, not the gameplay build.
    // Fingerprint the loaded native image so differing extensions fail closed.
    static int imageAnchor;
    Dl_info image{};
    if (!dladdr(&imageAnchor, &image) || !image.dli_fname)
        throw std::runtime_error("Cannot locate native gameplay image for LAN compatibility");
    std::ifstream input(image.dli_fname, std::ios::binary);
    if (!input)
        throw std::runtime_error("Cannot read native gameplay image for LAN compatibility");
    std::uint64_t hash = 14695981039346656037ULL, length = 0;
    std::array<char, 16384> buffer{};
    while (input.read(buffer.data(), buffer.size()) || input.gcount() > 0)
    {
        for (std::streamsize index = 0; index < input.gcount(); ++index)
        {
            hash ^= static_cast<unsigned char>(buffer[static_cast<std::size_t>(index)]);
            hash *= 1099511628211ULL;
        }
        length += static_cast<std::uint64_t>(input.gcount());
    }
    if (!input.eof() || !length)
        throw std::runtime_error("Cannot fingerprint native gameplay image");
    std::ostringstream out;
    out << "tanks3d-godot-lan-v1/" << std::hex << hash << '/' << length;
    return out.str();
}

const char *lanPhaseName(tanks3d::app::LanPhase phase)
{
    using tanks3d::app::LanPhase;
    switch (phase)
    {
    case LanPhase::Idle: return "idle";
    case LanPhase::Waiting: return "waiting";
    case LanPhase::Connecting: return "connecting";
    case LanPhase::Handshake: return "handshake";
    case LanPhase::Starting: return "starting";
    case LanPhase::Playing: return "playing";
    case LanPhase::Failed: return "failed";
    }
    return "failed";
}

std::string lanStatus(const Sample &sample)
{
    if (!sample.lan)
        return "{\"phase\":\"idle\",\"active\":false,\"host\":false,\"local_player\":0,\"waiting\":false,\"tick\":0,\"port\":41987,\"error\":\"\",\"addresses\":[]}";
    const auto &lan = *sample.lan;
    const auto phase = lan.session.phase();
    const bool active = phase != tanks3d::app::LanPhase::Idle && phase != tanks3d::app::LanPhase::Failed;
    std::ostringstream out;
    out << std::boolalpha << "{\"phase\":" << jsonString(lanPhaseName(phase))
        << ",\"active\":" << active << ",\"host\":" << lan.session.isHost()
        << ",\"connected\":" << (lan.channel->state() == tanks3d::net::ChannelState::Connected)
        << ",\"role\":\"" << (lan.session.isHost() ? "host" : "guest") << '"'
        << ",\"local_player\":" << lan.session.localPlayer()
        << ",\"waiting\":" << lan.session.waitingForPeer() << ",\"tick\":" << lan.session.tick()
        << ",\"port\":" << lan.channel->port() << ",\"error\":" << jsonString(lan.session.error())
        << ",\"addresses\":[";
    for (std::size_t index = 0; index < lan.addresses.size(); ++index)
        out << (index ? "," : "") << jsonString(lan.addresses[index]);
    const auto &settings = lan.config;
    out << "],\"settings\":{\"stage\":" << settings.stage << ",\"players\":2,\"ai_p2\":false"
        << ",\"lives\":" << settings.lives << ",\"nation_p1\":" << settings.nation_p1
        << ",\"nation_p2\":" << settings.nation_p2 << ",\"max_hp\":" << settings.max_hp
        << ",\"enemy_speed\":" << settings.enemy_speed << ",\"enemy_fire\":" << settings.enemy_fire
        << ",\"enemy_spawn\":" << settings.enemy_spawn << ",\"camera_yaw\":" << settings.camera_yaw
        << ",\"camera_elevation\":" << settings.camera_elevation << "}}";
    return out.str();
}

Sample &get(void *handle)
{
    if (!handle)
        throw std::invalid_argument("Null sample handle");
    return *static_cast<Sample *>(handle);
}

template <typename Operation>
int guarded(void *handle, Operation &&operation)
{
    try
    {
        auto &sample = get(handle);
        operation(sample);
        sample.error.clear();
        return 0;
    }
    catch (const std::exception &exception)
    {
        (handle ? static_cast<Sample *>(handle)->error : createError) = exception.what();
    }
    catch (...)
    {
        (handle ? static_cast<Sample *>(handle)->error : createError) = "Unknown sample error";
    }
    return -1;
}

PlayerControlFrame control(unsigned held, unsigned previous)
{
    PlayerControlFrame result;
    DirectionButtonFrame *directions[]{&result.north, &result.south,
                                       &result.west, &result.east};
    for (unsigned index = 0; index < 4; ++index)
    {
        const unsigned bit = 1U << index;
        *directions[index] = {(held & bit) != 0, (held & bit) && !(previous & bit)};
    }
    result.fireHeld = (held & 16U) != 0;
    return result;
}

const char *eventName(GameEventType type)
{
    switch (type)
    {
    case GameEventType::ShellFired: return "ShellFired";
    case GameEventType::ShellCancelled: return "ShellCancelled";
    case GameEventType::BrickHit: return "BrickHit";
    case GameEventType::TankDamaged: return "TankDamaged";
    case GameEventType::TankDestroyed: return "TankDestroyed";
    case GameEventType::PlayerRespawned: return "PlayerRespawned";
    case GameEventType::BonusSpawned: return "BonusSpawned";
    case GameEventType::BonusCollected: return "BonusCollected";
    case GameEventType::BaseDamaged: return "BaseDamaged";
    case GameEventType::StageEnded: return "StageEnded";
    }
    return "Unknown";
}

std::string digest(const GameSession &game)
{
    std::uint64_t hash = 14695981039346656037ULL;
    for (unsigned char byte : game.sessionDigest().state)
    {
        hash ^= byte;
        hash *= 1099511628211ULL;
    }
    std::ostringstream out;
    out << std::hex << std::setw(16) << std::setfill('0') << hash;
    return out.str();
}

std::string snapshot(const Sample &sample)
{
    if (!sample.game)
        throw std::logic_error("Reset is required before reading the sample");
    const auto &game = *sample.game;
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << std::setprecision(std::numeric_limits<float>::max_digits10)
        << std::boolalpha << "{\"schema\":1,\"stage\":" << game.stage()
        << ",\"player_count\":" << game.playerCount()
        << ",\"tick\":" << sample.tick << ",\"digest\":\"" << digest(game)
        << "\",\"game_over\":" << game.gameOver() << ",\"settling\":" << game.settling()
        << ",\"settlement_counting\":" << game.settlementCounting()
        << ",\"high_score\":" << game.highScoreDisplay()
        << ",\"intro\":" << game.stageIntro()
        << ",\"intro_remaining\":" << game.stageIntroTimeRemaining()
        << ",\"stage_transition\":" << game.stageTransition()
        << ",\"paused\":" << game.paused() << ",\"ai_p2\":" << sample.aiP2
        << ",\"ai_decisions\":" << sample.ai.decisions()
        << ",\"menu_requested\":" << sample.menuRequested
        << ",\"base_alive\":" << game.baseAlive()
        << ",\"base_nation\":" << static_cast<int>(game.baseNation())
        << ",\"base_steel_visible\":" << game.map().governmentSteelVisible()
        << ",\"base_steel_remaining\":" << game.map().governmentSteelTimeRemaining()
        << ",\"bonus_message\":" << jsonString(game.bonusMessage())
        << ",\"bonus_message_remaining\":" << game.bonusMessageTimer()
        << ",\"enemies_left\":" << game.enemiesLeft() << ",\"map\":[";
    for (int row = 0; row < kMapSize; ++row)
    {
        if (row)
            out << ',';
        out << '"';
        for (int column = 0; column < kMapSize; ++column)
            out << game.map().tile(row, column);
        out << '"';
    }
    out << "],\"brick_masks\":[";
    for (int index = 0; index < kMapSize * kMapSize; ++index)
    {
        if (index)
            out << ',';
        out << static_cast<int>(game.map().brickMask(index / kMapSize, index % kMapSize));
    }
    out << "],\"base_walls\":[";
    for (int index = 0; index < tanks3d::game::kGovernmentWallCount; ++index)
    {
        if (index)
            out << ',';
        out << game.map().governmentWallHealth(index);
    }
    out << "],\"base_steel\":" << game.map().governmentWallsSteel()
        << ",\"players\":[";
    bool separator = false;
    for (const auto &player : game.players())
    {
        if (separator)
            out << ',';
        separator = true;
        out << "{\"id\":" << player.id << ",\"x\":" << player.position.x
            << ",\"z\":" << player.position.z << ",\"yaw\":" << player.yaw
            << ",\"active\":" << player.active << ",\"moving\":" << player.moving
            << ",\"hp\":" << player.hitPoints << ",\"lives\":" << player.lives
            << ",\"max_hp\":" << player.maximumHitPoints
            << ",\"level\":" << player.level << ",\"nation\":" << static_cast<int>(player.nation)
            << ",\"nation_name\":\"" << nationName(player.nation)
            << "\",\"vehicle_name\":\"" << wwii_tank_model::playerVehicleName(player.nation, player.level)
            << "\",\"vehicle_id\":" << static_cast<int>(wwii_tank_model::playerVehicle(player.nation, player.level))
            << ",\"shield\":" << player.shieldTimer << ",\"creating\":" << player.creationTimer
            << ",\"respawning\":" << player.respawnTimer << ",\"death\":" << player.deathTimer
            << ",\"streak\":" << player.directKillStreak << ",\"streak_popup\":" << player.streakPopupTimer
            << ",\"on_ice\":" << player.onIce << ",\"fire_cooldown\":" << player.fireCooldown
            << ",\"score\":" << player.score << ",\"boat\":" << player.hasBoat << '}';
    }
    out << "],\"enemies\":[";
    separator = false;
    for (const auto &enemy : game.enemies())
    {
        if (separator)
            out << ',';
        separator = true;
        out << "{\"id\":" << enemy.id << ",\"x\":" << enemy.position.x
            << ",\"z\":" << enemy.position.z << ",\"yaw\":" << enemy.yaw
            << ",\"type\":" << enemy.type << ",\"nation\":" << static_cast<int>(game.enemyNation(enemy.id))
            << ",\"nation_name\":\"" << nationName(game.enemyNation(enemy.id))
            << "\",\"vehicle_name\":\"" << wwii_tank_model::nameForVehicle(
                   wwii_tank_model::enemyVehicle(game.enemyNation(enemy.id), enemy.type))
            << "\",\"vehicle_id\":" << static_cast<int>(wwii_tank_model::enemyVehicle(game.enemyNation(enemy.id), enemy.type))
            << ",\"armor\":" << enemy.armor << ",\"creating\":" << enemy.creationTimer
            << ",\"frozen\":" << enemy.frozenTimer << ",\"death\":" << enemy.deathTimer
            << ",\"carries_bonus\":" << enemy.carriesBonus << ",\"on_ice\":" << enemy.onIce
            << ",\"moving\":" << enemy.moving << ",\"destroyed\":" << enemy.destroyed << '}';
    }
    out << "],\"shells\":[";
    separator = false;
    for (const auto &shell : game.shells())
    {
        if (separator)
            out << ',';
        separator = true;
        out << "{\"x\":" << shell.position.x << ",\"z\":" << shell.position.z
            << ",\"vx\":" << shell.velocity.x << ",\"vz\":" << shell.velocity.z
            << ",\"owner\":" << static_cast<int>(shell.owner)
            << ",\"owner_index\":" << shell.ownerIndex << ",\"power\":" << shell.power
            << ",\"impacting\":" << shell.impacting << ",\"life\":" << shell.life << '}';
    }
    out << "],\"pickups\":[";
    separator = false;
    for (const auto &pickup : game.bonuses())
    {
        if (separator)
            out << ',';
        separator = true;
        out << "{\"type\":" << static_cast<int>(pickup.type)
            << ",\"x\":" << pickup.position.x << ",\"z\":" << pickup.position.z
            << ",\"age\":" << pickup.age << ",\"life\":" << pickup.life << '}';
    }
    out << "],\"events\":[";
    separator = false;
    for (const auto &event : sample.events)
    {
        if (separator)
            out << ',';
        separator = true;
        out << "{\"type\":\"" << eventName(event.type) << "\",\"x\":" << event.position.x
            << ",\"z\":" << event.position.z << ",\"source_player\":" << event.sourcePlayerId
            << ",\"source_enemy\":" << event.sourceEnemyId << ",\"target_player\":" << event.targetPlayerId
            << ",\"target_enemy\":" << event.targetEnemyId << ",\"points\":" << event.points
            << ",\"direction\":" << static_cast<int>(event.direction)
            << ",\"bonus_type\":" << static_cast<int>(event.bonusType)
            << ",\"base_part\":" << static_cast<int>(event.basePart)
            << ",\"impact_kind\":" << static_cast<int>(event.impactKind)
            << ",\"power\":" << event.power << '}';
    }
    const auto &settings = sample.config;
    out << "],\"settings\":{\"stage\":" << settings.stage << ",\"players\":" << settings.players
        << ",\"ai_p2\":" << (settings.ai_p2 != 0) << ",\"lives\":" << settings.lives
        << ",\"nation_p1\":" << settings.nation_p1 << ",\"nation_p2\":" << settings.nation_p2
        << ",\"max_hp\":" << settings.max_hp << ",\"enemy_speed\":" << settings.enemy_speed
        << ",\"enemy_fire\":" << settings.enemy_fire << ",\"enemy_spawn\":" << settings.enemy_spawn
        << ",\"camera_yaw\":" << settings.camera_yaw << ",\"camera_elevation\":" << settings.camera_elevation
        << "},\"report\":{\"active\":" << game.settling() << ",\"stage\":" << game.settlementStage()
        << ",\"game_over\":" << game.settlementWasGameOver() << ",\"counting\":" << game.settlementCounting()
        << ",\"score_counter\":" << game.settlementScoreCounter()
        << ",\"high_score\":" << game.settlementHighScore() << ",\"players\":[";
    for (int slot = 0; slot < game.playerCount(); ++slot)
    {
        if (slot)
            out << ',';
        const auto &tally = game.settlementTally(slot);
        out << "{\"id\":" << slot << ",\"score_at_start\":" << tally.scoreAtStageStart
            << ",\"score\":" << game.players()[static_cast<std::size_t>(slot)].score
            << ",\"total_destroyed\":" << tally.totalDestroyed()
            << ",\"enemy_points\":" << tally.totalEnemyPoints() << ",\"bonus_points\":" << tally.bonusPoints
            << ",\"stage_points\":" << tally.stagePoints() << ",\"kills\":[";
        for (int type = 0; type < 4; ++type)
            out << (type ? "," : "") << tally.destroyed[static_cast<std::size_t>(type)];
        out << "],\"displayed_kills\":[";
        for (int type = 0; type < 4; ++type)
            out << (type ? "," : "") << game.settlementDisplayedKills(slot, type);
        out << "],\"points_by_type\":[";
        for (int type = 0; type < 4; ++type)
            out << (type ? "," : "") << tally.enemyPoints[static_cast<std::size_t>(type)];
        out << "]}";
    }
    // cameraForPlayer only reads the game. Its wall-clock shake is deliberately
    // excluded; rig/orthographic span retain the production follow and framing.
    const auto &camera = game.cameraRigs()[0];
    out << "]},\"camera\":{\"position\":[" << camera.position.x << ',' << camera.position.y
        << ',' << camera.position.z << "],\"target\":[" << camera.target.x << ','
        << camera.target.y << ',' << camera.target.z << "],\"span\":" << game.cameraForPlayer(0).fovy
        << ",\"elevation\":" << game.cameraElevationDegrees()
        << ",\"yaw\":" << game.cameraYawDegrees() << "},\"lan\":" << lanStatus(sample) << '}';
    return out.str();
}
} // namespace tanks3d::godot_sample

extern "C"
{
void *tanks_sample_create(const char *resource_root)
{
    try
    {
        if (!resource_root || !*resource_root)
            throw std::invalid_argument("A resource root is required");
        auto *sample = new tanks3d::godot_sample::Sample(resource_root);
        tanks3d::godot_sample::createError.clear();
        return sample;
    }
    catch (const std::exception &exception)
    {
        tanks3d::godot_sample::createError = exception.what();
    }
    catch (...)
    {
        tanks3d::godot_sample::createError = "Unable to create sample";
    }
    return nullptr;
}

void tanks_sample_destroy(void *handle)
{
    delete static_cast<tanks3d::godot_sample::Sample *>(handle);
}

int tanks_sample_reset(void *handle, int seed, int stage, int players, int ai_p2)
{
    using namespace tanks3d::godot_sample;
    auto config = defaultConfig();
    config.stage = stage;
    config.players = players;
    config.ai_p2 = ai_p2;
    return tanks_sample_reset_config(handle, seed, &config);
}

void tanks_sample_default_config(TanksSampleConfig *config)
{
    if (config)
        *config = tanks3d::godot_sample::defaultConfig();
}

int tanks_sample_reset_config(void *handle, int seed, const TanksSampleConfig *config)
{
    using namespace tanks3d::godot_sample;
    return guarded(handle, [&](Sample &sample)
    {
        if (!config)
            throw std::invalid_argument("Missing sample configuration");
        reset(sample, seed, *config);
        if (sample.lan)
            sample.lan->session.stop();
        sample.lan.reset();
    });
}

int tanks_sample_restart(void *handle)
{
    using namespace tanks3d::godot_sample;
    return guarded(handle, [](Sample &sample)
    {
        if (!sample.game || sample.lan || sample.game->endingSequence())
            throw std::logic_error("Restart requires a running offline battle");
        if (!sample.game->restart())
            throw std::runtime_error(sample.game->lastError());
        sample.ai.reset();
        sample.previous = {};
        sample.events.clear();
        sample.menuRequested = false;
        sample.config.stage = sample.game->stage();
    });
}

int tanks_sample_start_config(void *handle, const TanksSampleConfig *config)
{
    using namespace tanks3d::godot_sample;
    return guarded(handle, [&](Sample &sample)
    {
        if (!config || !sample.game)
            throw std::invalid_argument("A valid configuration and initialized app session are required");
        validateConfig(*config);
        if (sample.lan && sample.lan->session.phase() != tanks3d::app::LanPhase::Idle &&
            sample.lan->session.phase() != tanks3d::app::LanPhase::Failed)
            throw std::logic_error("Disconnect the LAN session before starting offline");
        // The original menu calls start() on the current game. A candidate copy
        // preserves its RNG/highscore while keeping allocation/load failure atomic.
        auto candidate = std::make_unique<GameSession>(*sample.game);
        configureGame(*candidate, *config);
        commitGame(sample, std::move(candidate), *config);
        sample.lan.reset();
    });
}

int tanks_sample_step(void *handle, double dt, unsigned p1_bits, unsigned p2_bits)
{
    using namespace tanks3d::godot_sample;
    return guarded(handle, [&](Sample &sample)
    {
        if (sample.lan)
            throw std::logic_error("LAN worlds must advance only through lan_poll; reset to start offline");
        if (!sample.game)
            throw std::logic_error("Reset is required before stepping the sample");
        if (!std::isfinite(dt) || dt < 0 || ((p1_bits | p2_bits) & ~kInputBits))
            throw std::invalid_argument("Invalid elapsed time or input bits");
        const float elapsed = static_cast<float>(std::min(dt, 0.05));
        auto &game = *sample.game;
        const unsigned pressed = (p1_bits & ~sample.previous[0]) |
                                 (sample.aiP2 ? 0U : (p2_bits & ~sample.previous[1]));
        if ((pressed & kConfirm) && (game.settling() || game.highScoreDisplay()))
            game.confirmSettlement();
        else if ((pressed & kPause) && !game.endingSequence())
            game.togglePause();
        PlayerInputFrame input;
        input.players[0] = control(p1_bits, sample.previous[0]);
        input.players[1] = control(p2_bits, sample.previous[1]);
        if (sample.aiP2)
        {
            const bool battleRunning = !game.paused() && !game.stageIntro() &&
                !game.gameOver() && !game.settling() && !game.highScoreDisplay();
            sample.ai.update(elapsed, battleRunning, game.stage(), game.map(),
                             game.players(), game.enemies(), input);
        }
        game.update(elapsed, input);
        sample.events = game.eventsThisUpdate();
        if (game.consumeMenuRequest())
            sample.menuRequested = true;
        sample.previous = {{p1_bits, p2_bits}};
        ++sample.tick;
    });
}

const char *tanks_sample_snapshot(void *handle)
{
    using namespace tanks3d::godot_sample;
    if (guarded(handle, [](Sample &sample) { sample.snapshot = snapshot(sample); }) < 0)
        return nullptr;
    return get(handle).snapshot.c_str();
}

const char *tanks_sample_drain_audio(void *handle)
{
    using namespace tanks3d::godot_sample;
    if (guarded(handle, [](Sample &sample)
        {
            std::ostringstream out;
            out << '[';
            bool first = true;
            if (sample.audio)
            {
                for (const auto &command : sample.audio->commands)
                {
                    if (!first) out << ',';
                    first = false;
                    switch (command.operation)
                    {
                    case AudioCommand::Operation::Play:
                        out << "{\"op\":\"play\",\"cue\":" << static_cast<unsigned>(command.cue) << '}';
                        break;
                    case AudioCommand::Operation::Engine:
                        out << "{\"op\":\"engine\",\"active\":" << (command.active ? "true" : "false")
                            << ",\"moving\":" << (command.moving ? "true" : "false") << '}';
                        break;
                    case AudioCommand::Operation::Stop:
                        out << "{\"op\":\"stop\"}";
                        break;
                    }
                }
            }
            out << ']';
            sample.audioSnapshot = out.str();
            if (sample.audio) sample.audio->commands.clear();
        }) < 0)
        return nullptr;
    return get(handle).audioSnapshot.c_str();
}

const char *tanks_sample_error(void *handle)
{
    return handle ? static_cast<tanks3d::godot_sample::Sample *>(handle)->error.c_str()
                  : tanks3d::godot_sample::createError.c_str();
}

int tanks_sample_lan_host(void *handle, int seed, const TanksSampleConfig *config, int port, double now_seconds)
{
    using namespace tanks3d::godot_sample;
    return guarded(handle, [&](Sample &sample)
    {
        if (!config || !std::isfinite(now_seconds) || now_seconds < 0 || port < 0 || port > 65535)
            throw std::invalid_argument("Invalid LAN host settings");
        validateConfig(*config);
        if (config->players != 2 || config->ai_p2)
            throw std::invalid_argument("LAN requires two human players");
        auto candidate = std::make_unique<SampleLan>(std::make_unique<tanks3d::net::TcpChannel>(),
                                                     lanFingerprint(), *config, now_seconds);
        tanks3d::net::RoomSettings room;
        room.seed = static_cast<std::uint32_t>(seed);
        room.stage = config->stage;
        room.lives = config->lives;
        room.nations = {{static_cast<Nation>(config->nation_p1), static_cast<Nation>(config->nation_p2)}};
        room.advanced = {config->max_hp, config->enemy_speed, config->enemy_fire, config->enemy_spawn};
        if (!candidate->session.host(room, now_seconds, static_cast<std::uint16_t>(port)))
            throw std::runtime_error(candidate->session.error());
        if (sample.lan)
            sample.lan->session.stop();
        sample.lan = std::move(candidate);
        sample.events.clear();
    });
}

int tanks_sample_lan_join(void *handle, const char *address, int nation,
                          int camera_yaw, int camera_elevation, double now_seconds)
{
    using namespace tanks3d::godot_sample;
    return guarded(handle, [&](Sample &sample)
    {
        tanks3d::net::Endpoint endpoint;
        if (!address || !tanks3d::net::parseEndpoint(address, endpoint) ||
            !std::isfinite(now_seconds) || now_seconds < 0)
            throw std::invalid_argument("Enter an IPv4 address, optionally followed by :port");
        auto config = defaultConfig();
        config.players = 2;
        config.nation_p2 = nation;
        config.camera_yaw = camera_yaw;
        config.camera_elevation = camera_elevation;
        validateConfig(config);
        auto candidate = std::make_unique<SampleLan>(std::make_unique<tanks3d::net::TcpChannel>(),
                                                     lanFingerprint(), config, now_seconds);
        if (!candidate->session.join(endpoint, static_cast<Nation>(nation), now_seconds))
            throw std::runtime_error(candidate->session.error());
        if (sample.lan)
            sample.lan->session.stop();
        sample.lan = std::move(candidate);
        sample.events.clear();
    });
}

int tanks_sample_lan_poll(void *handle, double now_seconds, unsigned local_bits)
{
    using namespace tanks3d::godot_sample;
    return guarded(handle, [&](Sample &sample)
    {
        if (!sample.lan)
            throw std::logic_error("No LAN session");
        auto &lan = *sample.lan;
        if (!std::isfinite(now_seconds) || now_seconds < lan.lastNow || (local_bits & ~255U))
            throw std::invalid_argument("LAN time must be monotonic and input bits valid");
        const unsigned pressed = local_bits & ~lan.previous;
        std::uint8_t controls = 0;
        if (pressed & kConfirm)
            controls |= tanks3d::net::Control::Confirm;
        if (pressed & kPause)
            controls |= tanks3d::net::Control::Pause;
        if ((pressed & 128U) && lan.session.isHost())
            controls |= tanks3d::net::Control::Restart;
        lan.session.update(now_seconds, control(local_bits, lan.previous), controls);
        lan.lastNow = now_seconds;
        lan.previous = local_bits;
        sample.events.clear();
        if (const auto room = lan.session.takeStart())
        {
            auto config = lan.config;
            config.players = 2;
            config.ai_p2 = 0;
            config.stage = room->stage;
            config.lives = room->lives;
            config.nation_p1 = static_cast<int>(room->nations[0]);
            config.nation_p2 = static_cast<int>(room->nations[1]);
            config.max_hp = room->advanced.playerMaximumHitPoints;
            config.enemy_speed = room->advanced.enemySpeedPercent;
            config.enemy_fire = room->advanced.enemyFireRatePercent;
            config.enemy_spawn = room->advanced.enemySpawnRatePercent;
            try
            {
                reset(sample, static_cast<int>(room->seed), config);
                lan.config = config;
                lan.session.ready();
            }
            catch (...)
            {
                lan.session.abort(tanks3d::net::LeaveReason::CannotStart);
                throw;
            }
        }
        tanks3d::net::Packet packet;
        while (lan.session.nextTick(packet))
        {
            if (!sample.game)
            {
                lan.session.abort(tanks3d::net::LeaveReason::CannotStart);
                break;
            }
            const auto result = tanks3d::app::applyLanTick(*sample.game, packet);
            ++sample.tick;
            if (result != tanks3d::app::LanTickResult::Continue)
            {
                if (result == tanks3d::app::LanTickResult::CannotLoad)
                    lan.session.abort(tanks3d::net::LeaveReason::CannotStart);
                else
                {
                    sample.menuRequested = true;
                    lan.session.stop();
                }
                break;
            }
            sample.events.insert(sample.events.end(), sample.game->eventsThisUpdate().begin(),
                                 sample.game->eventsThisUpdate().end());
            if (packet.tick % tanks3d::net::kDigestInterval == 0)
                lan.session.recordDigest(tanks3d::net::stateHash(sample.game->sessionDigest().state));
        }
        lan.session.flush();
        if (lan.session.phase() == tanks3d::app::LanPhase::Failed)
            throw std::runtime_error(lan.session.error());
    });
}

int tanks_sample_lan_stop(void *handle)
{
    using namespace tanks3d::godot_sample;
    return guarded(handle, [](Sample &sample)
    {
        if (sample.lan)
            sample.lan->session.stop();
        sample.events.clear();
    });
}

const char *tanks_sample_lan_status(void *handle)
{
    using namespace tanks3d::godot_sample;
    if (guarded(handle, [](Sample &sample) { sample.lanStatus = lanStatus(sample); }) < 0)
        return nullptr;
    return get(handle).lanStatus.c_str();
}

int tanks_sample_map_pad(void *handle, int slot, float x, float y, unsigned buttons, int yaw)
{
    using namespace tanks3d::godot_sample;
    int result = -1;
    if (guarded(handle, [&](Sample &sample)
    {
        if (slot < 0 || slot >= 4 || !std::isfinite(x) || !std::isfinite(y) ||
            (buttons & ~255U) || yaw < kCameraYawMinimumDegrees || yaw > kCameraYawMaximumDegrees)
            throw std::invalid_argument("Invalid controller slot, axes or buttons");
        const auto index = static_cast<std::size_t>(slot);
        const unsigned previous = sample.padButtons[index];
        const auto button = [&](unsigned bit)
        {
            return tanks3d::app::ButtonSnapshot{(buttons & bit) != 0, (buttons & bit) && !(previous & bit)};
        };
        tanks3d::app::GamepadSnapshot input;
        input.available = true;
        input.leftStickX = x;
        input.leftStickY = y;
        input.dpadUp = button(1);
        input.dpadDown = button(2);
        input.dpadLeft = button(4);
        input.dpadRight = button(8);
        input.rightShoulder = button(16);
        input.faceDown = button(32);
        input.middleRight = button(64);
        input.middleLeft = button(128);
        const auto actions = tanks3d::app::mapGamepadInput(input, sample.padStates[index], yaw);
        result = (actions.player.north.held ? 1 : 0) | (actions.player.south.held ? 2 : 0) |
                 (actions.player.west.held ? 4 : 0) | (actions.player.east.held ? 8 : 0) |
                 (actions.player.fireHeld ? 16 : 0) | (actions.ui.confirmPressed ? 32 : 0) |
                 (actions.ui.pausePressed ? 64 : 0) | (actions.ui.cancelPressed ? 128 : 0);
        sample.padButtons[index] = buttons;
    }) < 0)
        return -1;
    return result;
}

int tanks_sample_reset_pad(void *handle, int slot, int suppress_stick)
{
    using namespace tanks3d::godot_sample;
    return guarded(handle, [&](Sample &sample)
    {
        if (slot < 0 || slot >= 4 || (suppress_stick != 0 && suppress_stick != 1))
            throw std::invalid_argument("Invalid controller reset");
        const auto index = static_cast<std::size_t>(slot);
        sample.padStates[index] = {};
        sample.padStates[index].suppressStickUntilRelease = suppress_stick != 0;
        // Menu transitions preserve held button history; a disconnect resets it.
        if (!suppress_stick)
            sample.padButtons[index] = 0;
    });
}
}
