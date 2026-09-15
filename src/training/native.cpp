// Transitional bridge to the production Game3D in main.cpp, matching the LAN
// integration-test seam. Built only by the optional AI target; no second ruleset.
#define main tanks3dApplicationMain
#include "../main.cpp"
#undef main

#include <memory>
#include <stdexcept>

namespace tanks3d::training
{
constexpr int kChannels = 14;
constexpr int kStateSize = 256;
constexpr int kObservationSize = kChannels * 26 * 26 + kStateSize;
constexpr float kStep = 1.0f / 60.0f;
constexpr int kRepeat = 3;
thread_local std::string lastError;

struct Environment
{
    fs::path resources;
    std::unique_ptr<Game3D> game;
    int ticks = 0, maxTicks = 7200, previousDirection = 0;
    int kills = 0, deaths = 0, shots = 0, baseHits = 0, pickups = 0;
    bool done = false, won = false, truncated = false;
    float reward = 0;
    explicit Environment(fs::path root) : resources(std::move(root)) {}

    void reset(std::uint32_t seed, int stage, int lives, int limit)
    {
        if (stage < 1 || stage > 35 || lives < 1 || lives > 99 || limit < 3 || limit > 216000)
            throw std::invalid_argument("Invalid episode settings");
        game = std::make_unique<Game3D>(resources, seed);
        if (!game->start(1, lives, stage, {{Nation::UnitedStates, Nation::SovietUnion}}))
            throw std::runtime_error(game->lastError());
        // Advance the real opening timer, with neutral input. Never mutate it.
        for (int i = 0; game->stageIntro() && i < 1800; ++i)
            game->update(kStep, {});
        if (game->stageIntro())
            throw std::runtime_error("Opening timer did not finish");
        ticks = previousDirection = kills = deaths = shots = baseHits = pickups = 0;
        maxTicks = limit;
        done = won = truncated = false;
        reward = 0;
    }

    void step(int action)
    {
        if (!game || done)
            throw std::logic_error("Reset is required before stepping");
        if (action < 0 || action >= 10)
            throw std::invalid_argument("Action outside [0,9]");
        reward = 0;
        const int direction = action / 2;
        for (int frame = 0; frame < kRepeat && !done; ++frame)
        {
            PlayerInputFrame input;
            auto &player = input.players[0];
            DirectionButtonFrame *buttons[]{nullptr, &player.north, &player.south, &player.west,
                                            &player.east};
            if (direction)
                *buttons[direction] = {true, frame == 0 && direction != previousDirection};
            player.fireHeld = (action % 2) != 0;
            game->update(kStep, input);
            ++ticks;
            for (const auto &event : game->eventsThisUpdate())
            {
                if (event.type == GameEventType::ShellFired && event.sourcePlayerId == 0)
                    ++shots;
                if (event.type == GameEventType::TankDestroyed)
                {
                    if (event.targetEnemyId >= 0)
                    {
                        ++kills;
                        reward += 3.0f;
                    }
                    if (event.targetPlayerId == 0)
                    {
                        ++deaths;
                        reward -= 3.0f;
                    }
                }
                if (event.type == GameEventType::TankDamaged)
                    reward += event.targetPlayerId == 0 ? -1.0f : 0.25f;
                if (event.type == GameEventType::BonusCollected && event.sourcePlayerId == 0)
                {
                    ++pickups;
                    reward += 1.0f;
                }
                if (event.type == GameEventType::BaseDamaged && event.sourcePlayerId == 0)
                {
                    ++baseHits;
                    reward -= 2.0f;
                }
            }
            won = game->settling() && !game->settlementWasGameOver();
            const bool lost = game->gameOver();
            truncated = !won && !lost && ticks >= maxTicks;
            done = won || lost || truncated;
            if (won)
                reward += 50.0f;
            if (lost)
                reward -= game->baseAlive() ? 30.0f : 50.0f;
        }
        previousDirection = direction;
    }

    void observe(float *out, int playerIndex = 0) const
    {
        if (!game || !out)
            throw std::logic_error("Missing episode or output buffer");
        std::fill(out, out + kObservationSize, 0.0f);
        const auto cell = [&](int channel, int row, int col, float value)
        {
            if (row >= 0 && row < 26 && col >= 0 && col < 26)
                out[channel * 676 + row * 26 + col] = value;
        };
        const auto mark = [&](int channel, XZ position, float value)
        { cell(channel, static_cast<int>(position.z), static_cast<int>(position.x), value); };
        const auto &map = game->map();
        for (int row = 0; row < 26; ++row)
            for (int col = 0; col < 26; ++col)
            {
                const auto mask = map.brickMask(row, col);
                for (int bit = 0; bit < 4; ++bit)
                    cell(bit, row, col, (mask & (1 << bit)) ? 1.0f : 0.0f);
                const char tile = map.tile(row, col);
                cell(4, row, col, tile == '@');
                cell(5, row, col, tile == '~');
                cell(6, row, col, tile == '%');
                cell(7, row, col, tile == '-');
                const int wall = tanks3d::game::governmentWallIndexForCell(row, col);
                if (wall >= 0)
                {
                    cell(8, row, col, map.governmentWallHealth(wall) / 4.0f);
                    if (map.governmentWallsSteel() && map.governmentWallHealth(wall) > 0)
                        cell(4, row, col, 1.0f);
                }
                cell(9, row, col, row >= 24 && col >= 12 && col <= 13);
            }
        float *state = out + kChannels * 676;
        const auto &p = game->players().at(static_cast<std::size_t>(playerIndex));
        const XZ facing = tanks3d::core::cardinalVector(p.driveDirection);
        const float own[]{p.position.x / 26,
                          p.position.z / 26,
                          facing.x,
                          facing.z,
                          p.hitPoints / 6.0f,
                          p.lives / 10.0f,
                          p.level / 3.0f,
                          p.fireCooldown / .12f,
                          p.active ? 1.0f : 0.0f,
                          p.hasBoat ? 1.0f : 0.0f,
                          std::min(1.0f, p.shieldTimer / 10),
                          p.iceSlipTimer / .38f,
                          p.creationTimer > 0 ? 1.0f : 0.0f,
                          p.respawnTimer > 0 ? 1.0f : 0.0f,
                          p.onIce ? 1.0f : 0.0f,
                          p.moving ? 1.0f : 0.0f};
        std::copy(std::begin(own), std::end(own), state);
        mark(10, p.position, 1);
        int index = 0;
        for (const auto &enemy : game->enemies())
        {
            if (enemy.destroyed)
                continue;
            mark(11, enemy.position, enemy.armor / 4.0f);
            if (index >= 4)
                continue;
            const XZ drive = tanks3d::core::cardinalVector(enemy.driveDirection);
            const float values[]{1,
                                 enemy.position.x / 26,
                                 enemy.position.z / 26,
                                 drive.x,
                                 drive.z,
                                 enemy.armor / 4.0f,
                                 enemy.type / 3.0f,
                                 enemy.moving ? 1.0f : 0.0f,
                                 enemy.carriesBonus ? 1.0f : 0.0f,
                                 enemy.creationTimer > 0 ? 1.0f : 0.0f,
                                 enemy.frozenTimer > 0 ? 1.0f : 0.0f,
                                 enemy.onIce ? 1.0f : 0.0f};
            std::copy(std::begin(values), std::end(values), state + 16 + index++ * 12);
        }
        std::vector<const Shell *> shells;
        for (const auto &shell : game->shells())
            if (!shell.impacting)
            {
                shells.push_back(&shell);
                if (shell.owner == ShellOwner::Enemy)
                    mark(12, shell.position, 1);
            }
        std::stable_sort(shells.begin(), shells.end(),
                         [&](const Shell *a, const Shell *b)
                         {
                             return distanceSquared(a->position, p.position) <
                                    distanceSquared(b->position, p.position);
                         });
        for (std::size_t i = 0; i < std::min<std::size_t>(24, shells.size()); ++i)
        {
            const auto &shell = *shells[i];
            const float values[]{shell.owner == ShellOwner::Enemy ? 1.0f : -1.0f,
                                 shell.position.x / 26,
                                 shell.position.z / 26,
                                 shell.velocity.x / 30,
                                 shell.velocity.z / 30,
                                 shell.power ? 1.0f : 0.0f};
            std::copy(std::begin(values), std::end(values), state + 64 + i * 6);
        }
        index = 0;
        for (const auto &bonus : game->bonuses())
        {
            mark(13, bonus.position, (static_cast<int>(bonus.type) + 1) / 9.0f);
            if (index >= 9)
                continue;
            const float values[]{1, bonus.position.x / 26, bonus.position.z / 26,
                                 static_cast<int>(bonus.type) / 8.0f};
            std::copy(std::begin(values), std::end(values), state + 208 + index++ * 4);
        }
        state[244] = ticks / static_cast<float>(maxTicks);
        state[245] = game->enemiesLeft() / 20.0f;
        state[246] = game->baseAlive() ? 1 : 0;
        state[247] = game->stageTransition() ? 1 : 0;
        state[248] = map.governmentSteelTimeRemaining() / 20;
        state[249] = shells.size() / 24.0f;
        state[250] = game->bonuses().size() / 9.0f;
        // Remaining entries reserved; seed/RNG, enemy targets and future
        // spawning decisions are deliberately absent from policy observations.
    }
    void info(double *out) const
    {
        if (!out || !game)
            throw std::logic_error("Missing episode or info buffer");
        const double values[]{reward,
                              done && !truncated ? 1.0 : 0.0,
                              truncated ? 1.0 : 0.0,
                              static_cast<double>(ticks),
                              static_cast<double>(kills),
                              static_cast<double>(deaths),
                              game->baseAlive() ? 1.0 : 0.0,
                              won ? 1.0 : 0.0,
                              static_cast<double>(shots),
                              static_cast<double>(baseHits),
                              static_cast<double>(game->players()[0].score),
                              static_cast<double>(game->stage()),
                              ticks / 60.0,
                              static_cast<double>(pickups),
                              0,
                              0};
        std::copy(std::begin(values), std::end(values), out);
    }
};

struct Viewer
{
    SceneLighting lighting;
    PostProcess post;
    EnvironmentAssets environment;
    bonus_assets::Assets bonuses;
    TankAssets tanks;
    ViewTargets targets;
    Viewer(const fs::path &resources, int width, int height, bool hidden)
    {
        SetTraceLogLevel(LOG_WARNING);
        SetConfigFlags(FLAG_WINDOW_ALWAYS_RUN | (hidden ? FLAG_WINDOW_HIDDEN : 0));
        InitWindow(width, height, "Tanks 3D - trained AI player");
        if (!IsWindowReady())
            throw std::runtime_error("Graphics context unavailable");
        SetExitKey(KEY_NULL);
        lighting.load();
        post.load();
        environment.load(resources);
        bonuses.load(resources);
        tanks.load(resources, lighting.shader(), lighting.depthShader());
    }
    ~Viewer()
    {
        targets.release();
        tanks.unload();
        bonuses.unload();
        environment.unload();
        post.unload();
        lighting.unload();
        CloseWindow();
    }
};
std::unique_ptr<Viewer> viewer;

template <typename F> int guarded(F &&operation)
{
    try
    {
        operation();
        lastError.clear();
        return 0;
    }
    catch (const std::exception &e)
    {
        lastError = e.what();
        return -1;
    }
    catch (...)
    {
        lastError = "Unknown native training error";
        return -1;
    }
}
Environment &get(void *handle)
{
    if (!handle)
        throw std::invalid_argument("Null environment handle");
    return *static_cast<Environment *>(handle);
}
} // namespace tanks3d::training

extern "C"
{
    const char *t3ai_error()
    {
        return tanks3d::training::lastError.c_str();
    }
    int t3ai_version()
    {
        return 1;
    }
    int t3ai_observation_size()
    {
        return tanks3d::training::kObservationSize;
    }
    void *t3ai_create(const char *resources)
    {
        void *handle = nullptr;
        tanks3d::training::guarded(
            [&]()
            {
                if (!resources || !fs::is_directory(resources))
                    throw std::invalid_argument("Missing resources");
                handle = new tanks3d::training::Environment(resources);
            });
        return handle;
    }
    void t3ai_destroy(void *handle)
    {
        delete static_cast<tanks3d::training::Environment *>(handle);
    }
    int t3ai_reset(void *handle, std::uint32_t seed, int stage, int lives, int maxTicks,
                   float *observation, double *info)
    {
        return tanks3d::training::guarded(
            [&]()
            {
                auto &e = tanks3d::training::get(handle);
                e.reset(seed, stage, lives, maxTicks);
                e.observe(observation);
                e.info(info);
            });
    }
    int t3ai_step(void *handle, int action, float *observation, double *info)
    {
        return tanks3d::training::guarded(
            [&]()
            {
                auto &e = tanks3d::training::get(handle);
                e.step(action);
                e.observe(observation);
                e.info(info);
            });
    }
    std::uint64_t t3ai_digest(void *handle)
    {
        std::uint64_t result = 0;
        tanks3d::training::guarded(
            [&]()
            {
                const auto &environment = tanks3d::training::get(handle);
                if (!environment.game)
                    throw std::logic_error("Reset is required before reading a digest");
                result = tanks3d::net::stateHash(environment.game->sessionDigest().state);
            });
        return result;
    }
    int t3ai_render(void *handle, unsigned char *rgba, int width, int height, int hidden)
    {
        return tanks3d::training::guarded(
            [&]()
            {
                auto &e = tanks3d::training::get(handle);
                if (!e.game || !rgba || width < 640 || height < 480 || width > 1920 ||
                    height > 1080)
                    throw std::invalid_argument("Invalid render request");
                if (!tanks3d::training::viewer)
                    tanks3d::training::viewer = std::make_unique<tanks3d::training::Viewer>(
                        e.resources, width, height, hidden != 0);
                auto &v = *tanks3d::training::viewer;
                if (GetScreenWidth() != width || GetScreenHeight() != height)
                    throw std::invalid_argument("Close the viewer before changing dimensions");
                if (WindowShouldClose())
                    throw std::runtime_error("Viewer closed");
                v.tanks.setAnimationClock(e.ticks / 60.0);
                if (!renderGame(*e.game, v.targets, v.lighting, v.tanks, v.environment, v.bonuses,
                                v.post, false))
                    throw std::runtime_error("Render target failed");
                Image frame = LoadImageFromScreen();
                ImageFormat(&frame, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
                if (frame.width != width || frame.height != height)
                {
                    UnloadImage(frame);
                    throw std::runtime_error("Unexpected framebuffer size");
                }
                std::memcpy(rgba, frame.data, static_cast<std::size_t>(width) * height * 4);
                UnloadImage(frame);
            });
    }
    void t3ai_close_viewer()
    {
        tanks3d::training::viewer.reset();
    }
}

#include "coop.inl"
