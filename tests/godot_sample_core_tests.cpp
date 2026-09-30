// Exercise the production-world seam without a window, audio or Godot install.
#include "../src/godot/sample_core.cpp"
#include "app/shell_flight_presentation.h"

#include <iostream>
#include <thread>

// Narrow white-box fixture access; no renderer or legacy application is linked.
namespace tanks3d::app
{
struct SessionTestAccess
{
    static bool prepareGameEventScenario(GameSession &game,
                                         bool openArena = true)
    {
        if (openArena)
            game.map_.prepareShowcaseArena();
        game.stageIntroTimer_ = 0.0f;
        game.stageTransitionTimer_ = 0.0f;
        game.gameOverReportTimer_ = 0.0f;
        game.gameOver_ = false;
        game.paused_ = false;
        game.baseAlive_ = true;
        game.settlement_.reset(game.stage_);
        game.highScoreDisplay_ = false;
        game.highScoreDisplayTimer_ = 0.0f;
        game.returnToMenuRequested_ = false;
        game.awaitingMenu_ = false;
        game.enemies_.clear();
        game.shells_.clear();
        game.bonuses_.clear();
        game.eventsThisUpdate_.clear();
        game.cameraShake_ = {};
        game.enemySpawnState_.remaining = 1;
        game.enemySpawnState_.timer = 10000.0f;

        static constexpr std::array<XZ, 2> positions{{
            {7.0f, 13.0f}, {19.0f, 13.0f}}};
        for (std::size_t index = 0; index < game.players_.size(); ++index)
        {
            Player &player = game.players_[index];
            player.position = positions[index];
            player.yaw = cardinalYaw(CardinalDirection::North);
            player.driveDirection = CardinalDirection::North;
            player.movementDirection = CardinalDirection::North;
            player.lives = 3;
            player.maximumHitPoints = 3;
            player.hitPoints = 3;
            player.level = 0;
            player.active = true;
            player.moving = false;
            player.hasBoat = false;
            player.shieldTimer = 0.0f;
            player.creationTimer = 0.0f;
            player.respawnTimer = 0.0f;
            player.deathTimer = 0.0f;
            player.fireCooldown = 0.0f;
            player.dustCooldown = 0.0f;
            player.iceSlipTimer = 0.0f;
            player.onIce = false;
            player.score = 0;
            player.resetDirectKillStreak();
            player.stageTally.reset(0);
        }
        return !game.players_.empty();
    }

    static void addEventShell(GameSession &game, ShellOwner owner, int ownerId,
                              XZ position, XZ velocity,
                              bool power = false,
                              bool impacting = false,
                              float life = 4.0f)
    {
        Shell shell;
        shell.owner = owner;
        shell.ownerIndex = ownerId;
        shell.position = position;
        shell.velocity = velocity;
        shell.power = power;
        shell.impacting = impacting;
        shell.life = life;
        game.shells_.push_back(shell);
    }

    static void rejectStageLoads(GameSession &game)
    {
        game.stageLoadOperation_ =
            [](StageMap &, const fs::path &, int, std::string &error) {
                error = "Injected next-stage validation failure";
                return false;
            };
    }

    static void setPlayerDirectKillTally(GameSession &game, int playerIndex,
                                         int enemyType, int points)
    {
        if (playerIndex < 0 ||
            playerIndex >= static_cast<int>(game.players_.size()))
            return;
        Player &player = game.players_[static_cast<std::size_t>(playerIndex)];
        player.score = points;
        player.stageTally.reset(0);
        player.stageTally.creditEnemy(enemyType, points, true);
    }

    static void beginSettlement(GameSession &game, bool gameOver,
                                bool baseAlive, bool recordEvent = true)
    {
        game.baseAlive_ = baseAlive;
        game.beginSettlement(gameOver, recordEvent);
    }

    static Player &player(GameSession &game, int index)
    {
        return game.players_.at(static_cast<std::size_t>(index));
    }

    static Enemy &addEnemy(GameSession &game, XZ position,
                           CardinalDirection direction = CardinalDirection::East)
    {
        Enemy enemy;
        enemy.id = 100 + static_cast<int>(game.enemies_.size());
        enemy.position = position;
        enemy.driveDirection = direction;
        enemy.movementDirection = direction;
        enemy.directionDecisionInterval = 10000.0f;
        enemy.fireCooldown = 10000.0f;
        enemy.movementDelay = 0.0f;
        game.enemies_.push_back(enemy);
        return game.enemies_.back();
    }

    static Enemy &enemy(GameSession &game, int index)
    {
        return game.enemies_.at(static_cast<std::size_t>(index));
    }

    static void armEnemySpawn(GameSession &game)
    {
        game.enemySpawnState_ = {1, 0, 103, 0.0f};
    }

    static void spawnEnemy(GameSession &game, RandomSource &random)
    {
        game.spawnEnemyIfNeeded(0.0f, random);
    }

    static bool positionAvailable(const GameSession &game, XZ candidate,
                                  int playerIndex = -1, int enemyIndex = -1)
    {
        return game.positionAvailable(candidate, playerIndex, enemyIndex);
    }
};
}

namespace
{
using namespace tanks3d::godot_sample;
using Game3D = tanks3d::app::GameSession;
using Game3DTestAccess = tanks3d::app::SessionTestAccess;
using tanks3d::godot_sample::Sample;
using Handle = std::unique_ptr<void, decltype(&tanks_sample_destroy)>;
constexpr float kStep = 1.0f / 60.0f;

class SampleMemoryChannel final : public tanks3d::net::Channel
{
public:
    SampleMemoryChannel *peer = nullptr;
    tanks3d::net::ChannelState current = tanks3d::net::ChannelState::Idle;
    std::vector<std::uint8_t> incoming;
    std::string message;
    bool listen(std::uint16_t, const std::string &) override
    {
        current = tanks3d::net::ChannelState::Listening;
        return true;
    }
    bool connect(const tanks3d::net::Endpoint &) override
    {
        current = peer->current = tanks3d::net::ChannelState::Connected;
        return true;
    }
    void poll() override {}
    bool send(const tanks3d::net::Packet &packet) override
    {
        const auto bytes = tanks3d::net::encodePacket(packet);
        peer->incoming.insert(peer->incoming.end(), bytes.begin(), bytes.end());
        return !bytes.empty();
    }
    bool receive(tanks3d::net::Packet &packet) override
    {
        bool available = false;
        if (!tanks3d::net::decodePacket(incoming, packet, available))
        {
            current = tanks3d::net::ChannelState::Failed;
            message = "Invalid memory transport packet";
        }
        return available;
    }
    void close() override
    {
        current = tanks3d::net::ChannelState::Idle;
        incoming.clear();
    }
    tanks3d::net::ChannelState state() const override { return current; }
    const std::string &error() const override { return message; }
    std::uint16_t port() const override { return 41987; }
};

void require(bool condition, const char *message)
{
    if (!condition)
        throw std::runtime_error(message);
}

Handle create()
{
    Handle result(tanks_sample_create("resources"), tanks_sample_destroy);
    require(result != nullptr, "sample creation failed");
    return result;
}

Sample &sample(const Handle &handle)
{
    return *static_cast<Sample *>(handle.get());
}

// Kept independent of the bridge's mask conversion to catch edge/order errors.
PlayerControlFrame expectedInput(unsigned bits, unsigned previous)
{
    PlayerControlFrame input;
    input.north = {(bits & 1U) != 0, (bits & 1U) != 0 && (previous & 1U) == 0};
    input.south = {(bits & 2U) != 0, (bits & 2U) != 0 && (previous & 2U) == 0};
    input.west = {(bits & 4U) != 0, (bits & 4U) != 0 && (previous & 4U) == 0};
    input.east = {(bits & 8U) != 0, (bits & 8U) != 0 && (previous & 8U) == 0};
    input.fireHeld = (bits & 16U) != 0;
    return input;
}

void testTankOverlapRecovery()
{
    constexpr float tinyStep = 1.0f / 240.0f;
    const std::array<Nation, 2> nations{{Nation::UnitedStates, Nation::SovietUnion}};
    const auto arena = [&](int playerCount)
    {
        Game3D game("resources", 0x0a11ceU);
        require(game.start(playerCount, 3, 1, nations), "overlap arena start failed");
        require(Game3DTestAccess::prepareGameEventScenario(game), "overlap arena preparation failed");
        return game;
    };
    const auto east = [](int slot)
    {
        PlayerInputFrame input;
        input.players[static_cast<std::size_t>(slot)].east = {true, true};
        return input;
    };
    const auto overlapping = [](XZ first, XZ second)
    {
        return std::fabs(first.x - second.x) < 1.75f &&
               std::fabs(first.z - second.z) < 1.75f;
    };

    // The old absolute occupancy query rejected every small step, even away
    // from an existing overlap. Cover both centers coincident and offset.
    for (bool enemyBlocker : {false, true})
    {
        for (float offset : {0.0f, 0.25f})
        {
            Game3D game = arena(enemyBlocker ? 1 : 2);
            const XZ blocker{7.0f, 13.0f};
            Game3DTestAccess::player(game, 0).position = {7.0f + offset, 13.0f};
            if (enemyBlocker)
                Game3DTestAccess::addEnemy(game, blocker).frozenTimer = 10000.0f;
            else
                Game3DTestAccess::player(game, 1).position = blocker;
            Game3D repeated = game;
            const float startX = game.players()[0].position.x;
            game.update(tinyStep, east(0));
            repeated.update(tinyStep, east(0));
            require(game.players()[0].position.x > startX,
                    "overlapping player cannot make a tiny outward step");
            for (int tick = 0; tick < 120; ++tick)
            {
                game.update(tinyStep, east(0));
                repeated.update(tinyStep, east(0));
                require(game.sessionDigest() == repeated.sessionDigest(),
                        "overlap recovery changed deterministic replay");
            }
            require(!overlapping(game.players()[0].position, blocker),
                    "overlapping player failed to become fully separated");
        }
    }

    // Enemy movement includes a forward probe and steering. Exercise both its
    // player blocker and enemy blocker through the full production update.
    for (bool enemyBlocker : {false, true})
    {
        Game3D game = arena(1);
        const XZ blocker{7.0f, 13.0f};
        if (!enemyBlocker)
            Game3DTestAccess::player(game, 0).position = blocker;
        else
            Game3DTestAccess::player(game, 0).position = {19.0f, 13.0f};
        Game3DTestAccess::addEnemy(game, blocker);
        if (enemyBlocker)
            Game3DTestAccess::addEnemy(game, blocker).frozenTimer = 10000.0f;
        game.update(tinyStep, {});
        require(game.enemies()[0].position.x > blocker.x,
                "overlapping enemy cannot make a tiny outward step");
        for (int tick = 0; tick < 480; ++tick)
            game.update(tinyStep, {});
        require(!overlapping(game.enemies()[0].position, blocker),
                "overlapping enemy failed to become fully separated");
    }

    Game3D inward = arena(2);
    Game3DTestAccess::player(inward, 0).position = {7.25f, 13.0f};
    Game3DTestAccess::player(inward, 1).position = {7.0f, 13.0f};
    PlayerInputFrame west;
    west.players[0].west = {true, true};
    inward.update(tinyStep, west);
    require(inward.players()[0].position.x == 7.25f,
            "overlap recovery accepted a step deeper into the blocker");

    Game3D thirdTank = arena(2);
    Game3DTestAccess::player(thirdTank, 1).position = thirdTank.players()[0].position;
    Game3DTestAccess::addEnemy(thirdTank, {8.75f, 13.0f}).frozenTimer = 10000.0f;
    thirdTank.update(tinyStep, east(0));
    require(thirdTank.players()[0].position.x == 7.0f,
            "escaping one overlap entered a different live tank");

    Game3D wall = arena(2);
    Game3DTestAccess::player(wall, 0).position = {0.875f, 13.0f};
    Game3DTestAccess::player(wall, 1).position = {0.875f, 13.0f};
    wall.update(tinyStep, west);
    require(wall.players()[0].position.x == 0.875f,
            "overlap recovery escaped through the map boundary");

    Game3D contact = arena(2);
    Game3DTestAccess::player(contact, 1).position = {8.75f, 13.0f};
    require(Game3DTestAccess::positionAvailable(contact, {7.0f, 13.0f}, 0) &&
                !Game3DTestAccess::positionAvailable(contact, {7.001f, 13.0f}, 0),
            "strict 1.75-tile edge-contact rule changed");
    contact.update(tinyStep, east(0));
    require(contact.players()[0].position.x == 7.0f,
            "ordinary movement entered a previously nonoverlapping tank");

    // Forced AI escape first queries its stationary lane-aligned center.
    // An outward-only predicate must not accidentally reject this anchor.
    Game3D escape = arena(1);
    Game3DTestAccess::player(escape, 0).position = {19.0f, 13.0f};
    auto &trapped = Game3DTestAccess::addEnemy(escape, {7.0f, 13.0f});
    trapped.blockedTimer = 0.5f;
    Game3DTestAccess::addEnemy(escape, {7.0f, 13.0f}).frozenTimer = 10000.0f;
    escape.update(tinyStep, {});
    require(distanceSquared(escape.enemies()[0].position, {7.0f, 13.0f}) > 0.0f,
            "enemy escape cannot plan from an overlapping aligned center");

    // A lane snap can deepen the overlap even though the unsnapped side step
    // is legal. Here north deepens it, south hits the bottom boundary, and
    // snapping either horizontal route to z=25 moves toward the blocker.
    Game3D edgeEscape = arena(1);
    Game3DTestAccess::player(edgeEscape, 0).position = {19.0f, 13.0f};
    const XZ edgeStart{7.2f, 25.1f};
    const XZ edgeBlocker{7.2f, 24.7f};
    auto &edgeTrapped = Game3DTestAccess::addEnemy(
        edgeEscape, edgeStart, CardinalDirection::North);
    edgeTrapped.blockedTimer = 0.5f;
    Game3DTestAccess::addEnemy(edgeEscape, edgeBlocker).frozenTimer = 10000.0f;
    XZ previous = edgeStart;
    for (int tick = 0; tick < 30; ++tick)
    {
        edgeEscape.update(tinyStep, {});
        const XZ current = edgeEscape.enemies()[0].position;
        require(current.x != edgeStart.x && current.z == edgeStart.z &&
                    std::fabs(current.x - edgeBlocker.x) >=
                        std::fabs(previous.x - edgeBlocker.x) &&
                    std::fabs(current.z - edgeBlocker.z) >=
                        std::fabs(previous.z - edgeBlocker.z) &&
                    !edgeEscape.map().collidesWithTank(current, 0.875f),
                "off-lane enemy overlap cannot escape sideways without deepening overlap or crossing terrain");
        previous = current;
    }
}

void testTankCreationOccupancy()
{
    const std::array<Nation, 2> nations{{Nation::UnitedStates, Nation::SovietUnion}};
    const auto arena = [&](int playerCount)
    {
        Game3D game("resources", 0xb17b17U);
        require(game.start(playerCount, 3, 1, nations), "creation arena start failed");
        require(Game3DTestAccess::prepareGameEventScenario(game), "creation arena preparation failed");
        return game;
    };
    Game3D birth = arena(1);
    auto &warning = Game3DTestAccess::addEnemy(birth, {7.0f, 13.0f});
    warning.creationTimer = 0.01f;
    warning.frozenTimer = 10000.0f;
    for (int tick = 0; tick < 40; ++tick)
    {
        birth.update(0.05f, {});
        require(birth.enemies()[0].creationTimer > 0.0f &&
                    !birth.enemies()[0].moving && birth.shells().empty(),
                "enemy warning became a physical tank on an occupied footprint");
    }
    PlayerInputFrame leave;
    leave.players[0].east = {true, true};
    birth.update(0.05f, leave);
    require(birth.players()[0].position.x > 7.0f,
            "creation warning became a physical movement blocker");
    for (int tick = 0; tick < 8; ++tick)
        birth.update(0.05f, leave);
    require(birth.enemies()[0].creationTimer == 0.0f,
            "enemy did not finish creation after its footprint cleared");

    for (bool enemyBlocker : {false, true})
    {
        Game3D respawn = arena(enemyBlocker ? 1 : 2);
        auto &dying = Game3DTestAccess::player(respawn, 0);
        dying.active = false;
        dying.hitPoints = 0;
        dying.lives = 2;
        dying.deathTimer = 0.01f;
        if (enemyBlocker)
            Game3DTestAccess::addEnemy(respawn, {9.0f, 25.0f}).frozenTimer = 10000.0f;
        else
            Game3DTestAccess::player(respawn, 1).position = {9.0f, 25.0f};
        int respawnEvents = 0;
        const auto step = [&](const PlayerInputFrame &input = PlayerInputFrame{})
        {
            respawn.update(0.05f, input);
            for (const auto &event : respawn.eventsThisUpdate())
                respawnEvents += event.type == GameEventType::PlayerRespawned;
        };
        for (int tick = 0; tick < 25; ++tick)
            step();
        require(respawn.players()[0].active && respawn.players()[0].creationTimer > 0.0f &&
                    respawn.players()[0].lives == 1 && respawnEvents == 1,
                "occupied player respawn overlapped, consumed extra lives, or repeated its event");
        const float shield = respawn.players()[0].shieldTimer;
        for (int tick = 0; tick < 60; ++tick)
            step();
        require(respawn.players()[0].shieldTimer == shield && shield > 0.0f &&
                    respawn.players()[0].creationTimer > 0.0f &&
                    respawn.players()[0].lives == 1 && respawnEvents == 1,
                "blocked birth consumed shield protection or repeated respawn side effects");
        if (enemyBlocker)
        {
            // Use real movement to clear the blocker without restarting P1.
            auto &movingEnemy = Game3DTestAccess::enemy(respawn, 0);
            movingEnemy.frozenTimer = 0.0f;
            movingEnemy.driveDirection = CardinalDirection::North;
            movingEnemy.movementDirection = CardinalDirection::North;
            for (int tick = 0; tick < 60; ++tick)
                step();
        }
        else
        {
            PlayerInputFrame north;
            north.players[1].north = {true, true};
            for (int tick = 0; tick < 10; ++tick)
                step(north);
        }
        require(respawn.players()[0].creationTimer == 0.0f &&
                    respawn.players()[0].position.x == 9.0f &&
                    respawn.players()[0].position.z == 25.0f &&
                    respawn.players()[0].lives == 1 && respawnEvents == 1,
                "player failed to finish a single respawn at its normal home position");
    }

    class SpawnRandom final : public tanks3d::app::RandomSource
    {
    public:
        int draws = 0;
        float draw(std::uniform_real_distribution<float> &) override
        {
            ++draws;
            return 0.5f;
        }
        int draw(std::uniform_int_distribution<int> &) override
        {
            ++draws;
            return 0;
        }
    } random;
    Game3D reservations = arena(1);
    for (XZ point : tanks3d::game::kEnemySpawnPoints)
        Game3DTestAccess::addEnemy(reservations, point).creationTimer = 0.8f;
    Game3DTestAccess::armEnemySpawn(reservations);
    Game3DTestAccess::spawnEnemy(reservations, random);
    require(reservations.enemies().size() == 3U && random.draws == 0,
            "spawn allocation reused a warning footprint or consumed blocked-spawn RNG");
}

void testShellFlightPresentation()
{
    using tanks3d::app::shellFlightPresentation;
    constexpr float spawn = tanks3d::game::kShellSpawnDistance;
    const auto inside = shellFlightPresentation(4.0f, 8.0f, false, spawn, 1.4f, .45f, .67f);
    require(!inside.visible && inside.height == .45f, "long visual gun must hide its internal flight");
    const auto emerging = shellFlightPresentation(3.9f, 8.0f, false, spawn, 1.4f, .45f, .67f);
    const auto midway = shellFlightPresentation(3.8f, 8.0f, false, spawn, 1.4f, .45f, .67f);
    const auto distant = shellFlightPresentation(3.6f, 8.0f, false, spawn, 1.4f, .45f, .67f);
    require(emerging.visible && emerging.height >= .45f && emerging.height < .451f &&
                midway.height > emerging.height && midway.height < .67f &&
                std::fabs(distant.height - .67f) < .000001f,
            "shell must emerge at gun height and converge smoothly to normal flight");
    require(!shellFlightPresentation(.2f, 0.0f, true, spawn, 1.4f, .45f, .67f).visible,
            "near-wall native impact must never be extended or redrawn as flight");
    require(shellFlightPresentation(4.0f, 8.0f, false, spawn, spawn, .67f, .67f).visible,
            "missing owners must retain normal flight instead of hiding shells");

    auto handle = create();
    require(tanks_sample_reset(handle.get(), 12, 1, 2, 1) == 0, "shell snapshot reset failed");
    auto &game = *sample(handle).game;
    Game3DTestAccess::addEventShell(game, ShellOwner::Player, 0, {7.0f, 11.0f}, {0.0f, -8.0f}, false, false, 3.875f);
    const auto digest = game.sessionDigest();
    const std::string snapshot = tanks_sample_snapshot(handle.get());
    require(snapshot.find("\"life\":3.875") != std::string::npos &&
                snapshot == tanks_sample_snapshot(handle.get()) && game.sessionDigest() == digest &&
                game.shells().back().life == 3.875f && game.shells().back().position.z == 11.0f,
            "presentation lifetime must serialize read-only without changing native flight/RNG");
}

void testInvalidInputAndReadOnlySnapshot()
{
    require(tanks_sample_create(nullptr) == nullptr &&
                std::string(tanks_sample_error(nullptr)).size() > 0,
            "missing resource root must report an error");
    require(tanks_sample_step(nullptr, kStep, 0, 0) == -1,
            "null handle must fail cleanly");
    tanks_sample_destroy(nullptr);
    auto handle = create();
    require(tanks_sample_snapshot(handle.get()) == nullptr &&
                tanks_sample_step(handle.get(), kStep, 0, 0) == -1,
            "unstarted sample must reject reads/steps");
    require(tanks_sample_reset(handle.get(), 12, 1, 2, 1) == 0, "reset failed");
    const auto original = sample(handle).game->sessionDigest().state;
    const std::string first = tanks_sample_snapshot(handle.get());
    require(first == tanks_sample_snapshot(handle.get()) &&
                original == sample(handle).game->sessionDigest().state,
            "snapshot must not mutate world or randomness");
    require(first.find("\"camera\":") != std::string::npos &&
                first.find("\"span\":18.5") != std::string::npos &&
                first.find("\"brick_masks\":[") != std::string::npos,
            "snapshot must include production camera and terrain damage");
    for (double bad : {-1.0, std::numeric_limits<double>::quiet_NaN(),
                       std::numeric_limits<double>::infinity()})
        require(tanks_sample_step(handle.get(), bad, 1, 0) == -1,
                "bad dt must reject atomically");
    require(tanks_sample_step(handle.get(), kStep, 128, 0) == -1 &&
                tanks_sample_reset(handle.get(), 0, 36, 2, 1) == -1 &&
                tanks_sample_reset(handle.get(), 0, 1, 1, 1) == -1 &&
                tanks_sample_reset(handle.get(), 0, 1, 2, 2) == -1,
            "unknown bits/stage/player settings must reject");
    require(original == sample(handle).game->sessionDigest().state &&
                sample(handle).tick == 0 && sample(handle).ai.decisions() == 0,
            "rejection must preserve world, inputs and AI");
    require(first == tanks_sample_snapshot(handle.get()), "rejection changed snapshot");
    require(tanks_sample_step(handle.get(), 100000.0, 0, 0) == 0,
            "large finite dt should clamp");
    Game3D reference("resources", 12U);
    require(reference.start(2, 3, 1, {{Nation::UnitedStates, Nation::SovietUnion}}),
            "reference start failed");
    reference.update(0.05f, {});
    require(reference.sessionDigest() == sample(handle).game->sessionDigest(),
            "clamping must retain production 0.05 second rule");
}

void testSeededProductionParity()
{
    auto handle = create();
    for (int stage = 1; stage <= 35; ++stage)
    {
        require(tanks_sample_reset(handle.get(), 731, stage, 1, 0) == 0,
                "stage reset failed");
        Game3D reference("resources", 731U);
        require(reference.start(1, 3, stage, {{Nation::UnitedStates, Nation::SovietUnion}}),
                "reference stage failed");
        unsigned previous = 0;
        for (int tick = 0; tick < 320; ++tick)
        {
            // Include multi-direction presses, held fallback and stop/fire.
            static constexpr unsigned commands[]{17U, 25U, 24U, 16U, 20U, 0U, 18U};
            const unsigned bits = commands[(tick / 19) % 7];
            PlayerInputFrame input;
            input.players[0] = expectedInput(bits, previous);
            const float elapsed = tick % 3 == 0 ? 0.024f : kStep;
            reference.update(elapsed, input);
            require(tanks_sample_step(handle.get(), elapsed, bits, 0) == 0,
                    "production parity step failed");
            require(reference.sessionDigest() == sample(handle).game->sessionDigest(),
                    "sample changed production map/rules/RNG");
            require(reference.eventsThisUpdate() == sample(handle).game->eventsThisUpdate(),
                    "sample changed production events");
            previous = bits;
        }
    }
}

void testEnemyNationsForSoloHumanAndAiCoop()
{
    auto handle = create();
    std::array<std::array<Nation, 32>, 9> humanCoopChoices{};
    // All nine menu pairs in solo (P2 ignored), human co-op, and AI co-op.
    for (int mode = 0; mode < 3; ++mode)
    {
        for (int first = 0; first < 3; ++first)
        {
            for (int second = 0; second < 3; ++second)
            {
                TanksSampleConfig config{};
                tanks_sample_default_config(&config);
                config.players = mode == 0 ? 1 : 2;
                config.ai_p2 = mode == 2 ? 1 : 0;
                config.lives = 99;
                config.max_hp = 6;
                config.nation_p1 = first;
                config.nation_p2 = second;
                require(tanks_sample_reset_config(handle.get(), 731, &config) == 0,
                        "nationality setup failed");
                auto &game = *sample(handle).game;
                const auto beforeLookups = game.sessionDigest();
                for (int id = 0; id < 32; ++id)
                {
                    const Nation opponent = game.enemyNation(id);
                    require(static_cast<int>(opponent) != first &&
                                (mode == 0 || static_cast<int>(opponent) != second),
                            "native enemy used a participating player's nation");
                    auto &humanChoice = humanCoopChoices[static_cast<std::size_t>(first * 3 + second)]
                                                       [static_cast<std::size_t>(id)];
                    if (mode == 1)
                        humanChoice = opponent;
                    if (mode == 2)
                        require(humanChoice == opponent,
                                "AI P2 changed its nation's participation in the opponent pool");
                }
                require(game.sessionDigest() == beforeLookups,
                        "native nationality queries consumed gameplay RNG");
                for (int tick = 0; tick < 420; ++tick)
                    require(tanks_sample_step(handle.get(), kStep, 16U, 0U) == 0,
                            "nationality spawn step failed");
                require(!game.enemies().empty(), "nationality check never reached a real spawn");
                const auto beforeSnapshot = game.sessionDigest();
                const std::string snapshot = tanks_sample_snapshot(handle.get());
                const auto enemySection = snapshot.find("\"enemies\":[");
                require(enemySection != std::string::npos, "snapshot omitted enemies");
                for (const auto &enemy : game.enemies())
                {
                    const Nation opponent = game.enemyNation(enemy.id);
                    require(static_cast<int>(opponent) != first &&
                                (mode == 0 || static_cast<int>(opponent) != second),
                            "spawned native enemy used a participating player's nation");
                    const auto begin = snapshot.find("{\"id\":" + std::to_string(enemy.id) + ",", enemySection);
                    require(begin != std::string::npos, "snapshot omitted a spawned enemy");
                    const auto end = snapshot.find('}', begin);
                    const std::string fields = snapshot.substr(begin, end - begin);
                    require(fields.find("\"nation\":" + std::to_string(static_cast<int>(opponent)) + ",") !=
                                std::string::npos &&
                                fields.find("\"vehicle_id\":" + std::to_string(static_cast<int>(
                                    wwii_tank_model::enemyVehicle(opponent, enemy.type))) + ",") != std::string::npos,
                            "Godot snapshot nationality and national vehicle disagree");
                }
                require(game.sessionDigest() == beforeSnapshot,
                        "native nationality snapshot mutated world/RNG");
            }
        }
    }
}

void testAiPauseResetAndManualControlIsolation()
{
    auto handle = create();
    auto alternate = create();
    require(tanks_sample_reset(handle.get(), 3000000, 1, 2, 1) == 0 &&
                tanks_sample_reset(alternate.get(), 3000000, 1, 2, 1) == 0,
            "AI setup failed");
    Game3D reference("resources", 3000000U);
    require(reference.start(2, 3, 1, {{Nation::UnitedStates, Nation::SovietUnion}}),
            "AI reference start failed");
    tanks3d::app::AiPlayerController controller;
    unsigned previous = 0;
    int shots = 0;
    for (int tick = 0; tick < 1500; ++tick)
    {
        const unsigned bits = tick % 240 < 120 ? 17U : 16U;
        PlayerInputFrame input;
        input.players[0] = expectedInput(bits, previous);
        const bool running = !reference.paused() && !reference.stageIntro() &&
            !reference.gameOver() && !reference.settling() && !reference.highScoreDisplay();
        controller.update(kStep, running, reference.stage(), reference.map(),
                          reference.players(), reference.enemies(), input);
        reference.update(kStep, input);
        reference.consumeMenuRequest();
        require(tanks_sample_step(handle.get(), kStep, bits, 0) == 0 &&
                    tanks_sample_step(alternate.get(), kStep, bits, 127U) == 0,
                "AI step failed");
        require(reference.sessionDigest() == sample(handle).game->sessionDigest(),
                "native AI sample must match current app path");
        require(sample(handle).game->sessionDigest() == sample(alternate).game->sessionDigest(),
                "manual P2 commands must not affect AI P2");
        for (const auto &event : reference.eventsThisUpdate())
            shots += event.type == GameEventType::ShellFired && event.sourcePlayerId == 1;
        previous = bits;
    }
    require(shots > 0 && sample(handle).ai.decisions() > 0, "AI must play, not stay neutral");
    // A fresh live stage isolates pause behavior from possible battle outcomes.
    require(tanks_sample_reset(handle.get(), 42, 1, 2, 1) == 0, "AI reset failed");
    require(sample(handle).ai.decisions() == 0 && sample(handle).tick == 0,
            "reset must discard old AI routes/clocks");
    for (int tick = 0; tick < 300; ++tick)
        require(tanks_sample_step(handle.get(), kStep, 0, 0) == 0, "intro step failed");
    require(tanks_sample_step(handle.get(), kStep, 64, 0) == 0 && sample(handle).game->paused(),
            "pause edge must pause");
    const auto decisions = sample(handle).ai.decisions();
    for (int tick = 0; tick < 30; ++tick)
        require(tanks_sample_step(handle.get(), kStep, 64, 0) == 0, "paused step failed");
    require(sample(handle).game->paused() && sample(handle).ai.decisions() == decisions,
            "held pause must not retoggle and paused AI must not run");
    require(tanks_sample_step(handle.get(), kStep, 0, 0) == 0 &&
                tanks_sample_step(handle.get(), kStep, 64, 0) == 0 &&
                !sample(handle).game->paused(), "second pause edge must resume");
}

void testOrderedAudioOutput()
{
    auto handle = create();
    require(tanks_sample_drain_audio(nullptr) == nullptr, "null audio drain must fail");
    require(std::string(tanks_sample_drain_audio(handle.get())) == "[]", "unstarted audio queue must be empty");
    require(tanks_sample_reset(handle.get(), 77, 1, 1, 0) == 0, "audio fixture could not start");
    auto &game = *sample(handle).game;
    const auto before = game.sessionDigest();
    const std::string initial = tanks_sample_drain_audio(handle.get());
    require(initial == "[{\"op\":\"play\",\"cue\":0}]",
            "stage start must emit exactly the native priority jingle command");
    require(game.sessionDigest() == before && std::string(tanks_sample_drain_audio(handle.get())) == "[]",
            "audio drain must clear only output, without changing world/RNG");
    require(Game3DTestAccess::prepareGameEventScenario(game), "running audio fixture failed");
    require(tanks_sample_step(handle.get(), kStep, 64, 0) == 0, "pause audio step failed");
    const std::string paused = tanks_sample_drain_audio(handle.get());
    require(paused.find("\"op\":\"stop\"") < paused.find("\"cue\":1") &&
            paused.find("\"cue\":1") != std::string::npos &&
            paused.find("\"active\":false") != std::string::npos,
            "native pause must retain both cue and engine silence");
    require(Game3DTestAccess::prepareGameEventScenario(game), "audio event fixture failed");
    Game3DTestAccess::addEventShell(game, ShellOwner::Player, 0, {0.04f, 5.0f}, {-10.0f, 0.0f});
    require(tanks_sample_step(handle.get(), kStep, 0, 0) == 0, "boundary audio step failed");
    const std::string boundary = tanks_sample_drain_audio(handle.get());
    require(boundary.find("\"cue\":8") != std::string::npos,
            "BoundaryHit audio must not be lost through a visual-event approximation");
    Game3DTestAccess::setPlayerDirectKillTally(game, 0, 0, 5000);
    Game3DTestAccess::beginSettlement(game, true, false);
    tanks_sample_drain_audio(handle.get());
    require(tanks_sample_step(handle.get(), 0.05, 0, 0) == 0 &&
            tanks_sample_step(handle.get(), 0.05, 0, 0) == 0, "score audio step failed");
    require(std::string(tanks_sample_drain_audio(handle.get())).find("\"cue\":21") != std::string::npos,
            "report score counter must retain its native audio request");
    require(tanks_sample_step(handle.get(), 0, 32, 0) == 0, "report skip failed");
    require(tanks_sample_step(handle.get(), 0, 0, 0) == 0, "report release failed");
    require(tanks_sample_step(handle.get(), 0, 32, 0) == 0, "highscore transition failed");
    require(std::string(tanks_sample_drain_audio(handle.get())).find("\"cue\":3") != std::string::npos,
            "highscore transition must retain its native priority audio request");

    SampleAudio queue;
    queue.updateEngine(true, false);
    queue.updateEngine(true, false);
    queue.play(tanks3d::audio::AudioCue::PlayerFired);
    queue.updateEngine(true, false);
    queue.stopAll();
    queue.updateEngine(true, false);
    require(queue.commands.size() == 5, "engine coalescing crossed a play/stop barrier");
}

void testReportConfirmation()
{
    auto handle = create();
    require(tanks_sample_reset(handle.get(), 9, 1, 2, 1) == 0, "report reset failed");
    auto &game = *sample(handle).game;
    Game3DTestAccess::setPlayerDirectKillTally(game, 0, 0, 100);
    Game3DTestAccess::beginSettlement(game, false, true);
    require(tanks_sample_step(handle.get(), kStep, 32, 0) == 0 && game.settling(),
            "first confirm should finish report counting");
    require(tanks_sample_step(handle.get(), kStep, 32, 0) == 0 && game.stage() == 1,
            "held confirm must not skip report");
    require(tanks_sample_step(handle.get(), kStep, 0, 0) == 0 &&
                tanks_sample_step(handle.get(), kStep, 32, 0) == 0 && game.stage() == 2 &&
                game.stageIntro() && sample(handle).ai.decisions() == 0,
            "fresh confirm must load real next stage and reset AI");
    Game3DTestAccess::beginSettlement(game, true, false);
    require(tanks_sample_step(handle.get(), kStep, 0, 0) == 0, "report edge release failed");
    for (int press = 0; press < 4 && !sample(handle).menuRequested; ++press)
    {
        require(tanks_sample_step(handle.get(), kStep, 32, 0) == 0 &&
                    tanks_sample_step(handle.get(), kStep, 0, 0) == 0,
                "game-over report confirmation failed");
    }
    require(sample(handle).menuRequested, "game-over exit must be visible to Godot UI");
}

void testEventMetadata()
{
    auto handle = create();
    require(tanks_sample_reset(handle.get(), 9, 1, 1, 0) == 0, "event reset failed");
    auto &game = *sample(handle).game;
    require(Game3DTestAccess::prepareGameEventScenario(game), "event fixture failed");
    const auto wall = governmentWallSegment(0);
    Game3DTestAccess::addEventShell(game, ShellOwner::Enemy, 63,
                                  wall.center, {0.0f, 1.0f});
    require(tanks_sample_step(handle.get(), 0.0, 0, 0) == 0, "wall impact failed");
    const std::string wallSnapshot = tanks_sample_snapshot(handle.get());
    require(wallSnapshot.find("\"type\":\"BaseDamaged\"") != std::string::npos &&
                wallSnapshot.find("\"base_part\":1,\"impact_kind\":2") != std::string::npos,
            "wall event metadata must distinguish damage from a destroyed core");
    require(Game3DTestAccess::prepareGameEventScenario(game), "core fixture failed");
    Game3DTestAccess::addEventShell(game, ShellOwner::Enemy, 63,
                                  kGovernmentBaseCenter, {0.0f, 1.0f});
    require(tanks_sample_step(handle.get(), 0.0, 0, 0) == 0, "core impact failed");
    const std::string coreSnapshot = tanks_sample_snapshot(handle.get());
    require(coreSnapshot.find("\"base_part\":2,\"impact_kind\":0") != std::string::npos &&
                coreSnapshot.find("\"base_alive\":false") != std::string::npos,
            "core event must retain independent destruction identity");
}

void testFullConfiguration()
{
    auto handle = create();
    TanksSampleConfig config{};
    tanks_sample_default_config(&config);
    require(config.lives == 3 && config.max_hp == 3 && config.camera_elevation == 50,
            "legacy setup defaults changed");
    config.players = 2;
    config.stage = 17;
    config.lives = 99;
    config.nation_p1 = 2;
    config.nation_p2 = 0;
    config.max_hp = 6;
    config.enemy_speed = -30;
    config.enemy_fire = 25;
    config.enemy_spawn = 30;
    config.camera_yaw = -45;
    config.camera_elevation = 70;
    require(tanks_sample_reset_config(handle.get(), 731, &config) == 0, "full setup rejected");
    Game3D reference("resources", 731U);
    require(reference.start(2, 99, 17, {{Nation::Germany, Nation::UnitedStates}},
                            {6, -30, 25, 30}, -45, 70), "reference setup failed");
    unsigned previous = 0;
    for (int tick = 0; tick < 420; ++tick)
    {
        const unsigned bits = tick % 200 < 100 ? 17U : 24U;
        PlayerInputFrame input;
        input.players[0] = expectedInput(bits, previous);
        reference.update(kStep, input);
        require(tanks_sample_step(handle.get(), kStep, bits, 0) == 0 &&
                    reference.sessionDigest() == sample(handle).game->sessionDigest(),
                "custom setup diverged from production rules or random stream");
        previous = bits;
    }
    require(reference.restart() && tanks_sample_restart(handle.get()) == 0 &&
                reference.sessionDigest() == sample(handle).game->sessionDigest(),
            "manual restart must preserve production current-stage and RNG behavior");
    const std::string good = tanks_sample_snapshot(handle.get());
    require(good.find("\"nation_name\":\"GERMANY\"") != std::string::npos &&
                good.find("\"max_hp\":6") != std::string::npos &&
                good.find("\"camera_yaw\":-45") != std::string::npos &&
                good.find("\"player_count\":2") != std::string::npos,
            "settings/HUD snapshot must preserve selected setup");
    for (auto field : {&TanksSampleConfig::stage, &TanksSampleConfig::players,
                       &TanksSampleConfig::ai_p2, &TanksSampleConfig::lives,
                       &TanksSampleConfig::nation_p1, &TanksSampleConfig::nation_p2,
                       &TanksSampleConfig::max_hp, &TanksSampleConfig::enemy_speed,
                       &TanksSampleConfig::enemy_fire, &TanksSampleConfig::enemy_spawn,
                       &TanksSampleConfig::camera_yaw, &TanksSampleConfig::camera_elevation})
    {
        auto invalid = config;
        invalid.*field = 1000;
        require(tanks_sample_reset_config(handle.get(), 2, &invalid) == -1 &&
                    good == tanks_sample_snapshot(handle.get()),
                "invalid config must preserve whole running sample atomically");
    }
    for (auto field : {&TanksSampleConfig::enemy_speed, &TanksSampleConfig::enemy_fire,
                       &TanksSampleConfig::enemy_spawn, &TanksSampleConfig::camera_yaw})
    {
        auto invalid = config;
        invalid.*field = 1;
        require(tanks_sample_reset_config(handle.get(), 2, &invalid) == -1,
                "non-menu setting increment must reject");
    }
    require(tanks_sample_reset_config(handle.get(), 2, nullptr) == -1 &&
                good == tanks_sample_snapshot(handle.get()), "null config must reject atomically");
    Game3DTestAccess::setPlayerDirectKillTally(*sample(handle).game, 0, 2, 300);
    Game3DTestAccess::beginSettlement(*sample(handle).game, false, true);
    const std::string report = tanks_sample_snapshot(handle.get());
    require(report.find("\"kills\":[0,0,1,0]") != std::string::npos &&
                report.find("\"points_by_type\":[0,0,300,0]") != std::string::npos,
            "report must expose native categorized tally, not infer from total score");
}

void testLanBridge(Nation guestNation)
{
    using tanks3d::app::LanPhase;
    using tanks3d::godot_sample::SampleLan;
    auto host = create();
    auto guest = create();
    auto first = std::make_unique<SampleMemoryChannel>();
    auto second = std::make_unique<SampleMemoryChannel>();
    first->peer = second.get();
    second->peer = first.get();
    auto settings = tanks3d::godot_sample::defaultConfig();
    settings.players = 2;
    settings.stage = 9;
    settings.lives = 4;
    settings.max_hp = 6;
    settings.enemy_speed = -15;
    sample(host).lan = std::make_unique<SampleLan>(std::move(first), "same-extension", settings, 0);
    auto guestSettings = settings;
    guestSettings.camera_yaw = 45;
    sample(guest).lan = std::make_unique<SampleLan>(std::move(second), "same-extension", guestSettings, 0);
    tanks3d::net::RoomSettings room;
    room.seed = guestNation == Nation::UnitedStates ? 0x5eed1234U : 731U;
    room.stage = settings.stage;
    room.lives = settings.lives;
    room.advanced = {6, -15, 0, 0};
    require(sample(host).lan->session.host(room, 0) &&
                sample(guest).lan->session.join({"127.0.0.1", 41987}, guestNation, 0),
            "LAN memory connection failed");
    double now = 0;
    int matches = 0, hostShots = 0, guestShots = 0;
    std::array<Nation, kEnemiesPerStage> spawnedNations{};
    std::array<bool, kEnemiesPerStage> observedSpawns{};
    bool checkedNationSequence = false;
    for (int frame = 0; frame < 900; ++frame)
    {
        now += 1.0 / 60.0;
        require(tanks_sample_lan_poll(host.get(), now, 17U) == 0 &&
                    tanks_sample_lan_poll(guest.get(), now, 24U) == 0,
                "LAN wrapper handshake/tick failed");
        if (!sample(host).game || !sample(guest).game)
            continue;
        for (const auto &event : sample(host).events)
        {
            hostShots += event.type == GameEventType::ShellFired && event.sourcePlayerId == 0;
            guestShots += event.type == GameEventType::ShellFired && event.sourcePlayerId == 1;
        }
        if (sample(host).tick == sample(guest).tick)
        {
            auto &hostGame = *sample(host).game;
            auto &guestGame = *sample(guest).game;
            require(hostGame.sessionDigest() == guestGame.sessionDigest(),
                    "LAN bridge peers changed production state/RNG despite ordered identical packets");
            if (!checkedNationSequence)
            {
                require(hostGame.randomSeed() == room.seed && guestGame.randomSeed() == room.seed &&
                            hostGame.stage() == room.stage && guestGame.stage() == room.stage,
                        "LAN peers did not retain the agreed national-sequence seed/stage");
                const auto beforeHost = hostGame.sessionDigest();
                const auto beforeGuest = guestGame.sessionDigest();
                Nation previous = Nation::Count;
                bool repeated = false;
                bool soviet = false;
                bool german = false;
                // Check both lookup orders; no render call may roll a new
                // identity or advance either peer's gameplay random state.
                for (int id = 63; id >= 0; --id)
                {
                    const Nation nation = hostGame.enemyNation(id);
                    require(nation == guestGame.enemyNation(id) &&
                                nation != Nation::UnitedStates && nation != guestNation,
                            "LAN peers disagreed on opposing nations or included a player nation");
                    repeated |= nation == previous;
                    soviet |= nation == Nation::SovietUnion;
                    german |= nation == Nation::Germany;
                    previous = nation;
                }
                for (int id = 0; id < 64; ++id)
                    require(guestGame.enemyNation(id) == hostGame.enemyNation(id),
                            "reverse-order LAN nation lookups changed an identity");
                require(guestNation != Nation::UnitedStates || (soviet && german && repeated),
                        "same-nation LAN still alternates enemies or excludes an eligible nation");
                require(hostGame.sessionDigest() == beforeHost && guestGame.sessionDigest() == beforeGuest,
                        "LAN nationality reads consumed either peer's gameplay RNG");
                checkedNationSequence = true;
            }
            for (const auto &enemy : hostGame.enemies())
            {
                require(enemy.id >= 0 && enemy.id < kEnemiesPerStage,
                        "LAN nationality fixture unexpectedly advanced beyond its first stage");
                const auto id = static_cast<std::size_t>(enemy.id);
                const Nation nation = hostGame.enemyNation(enemy.id);
                require(nation == guestGame.enemyNation(enemy.id) &&
                            (!observedSpawns[id] || nation == spawnedNations[id]),
                        "a real LAN enemy changed nation across peers or frames");
                observedSpawns[id] = true;
                spawnedNations[id] = nation;
            }
            ++matches;
        }
    }
    require(matches > 700 && hostShots > 0 && guestShots > 0,
            "LAN peers must actually simulate, including both assigned player inputs");
    if (guestNation == Nation::UnitedStates)
    {
        int observed = 0;
        bool soviet = false;
        bool german = false;
        bool repeated = false;
        for (std::size_t id = 0; id < observedSpawns.size(); ++id)
        {
            if (!observedSpawns[id]) continue;
            ++observed;
            soviet |= spawnedNations[id] == Nation::SovietUnion;
            german |= spawnedNations[id] == Nation::Germany;
            repeated |= id > 0 && observedSpawns[id - 1] && spawnedNations[id] == spawnedNations[id - 1];
        }
        require(observed >= 3 && soviet && german && repeated,
                "same-nation LAN never actually spawned both opponents and a repeated nationality");
    }
    require(checkedNationSequence &&
                sample(host).config.nation_p2 == static_cast<int>(guestNation) &&
                sample(guest).config.nation_p2 == static_cast<int>(guestNation) &&
                sample(host).config.max_hp == 6 && sample(guest).config.enemy_speed == -15 &&
                sample(host).game->cameraYawDegrees() == 0 && sample(guest).game->cameraYawDegrees() == 45,
            "host rules/guest nation must agree while each camera remains local");
    const std::string before = tanks_sample_snapshot(host.get());
    require(tanks_sample_step(host.get(), kStep, 0, 0) == -1 &&
                tanks_sample_lan_poll(host.get(), now - 1, 0) == -1 &&
                before == tanks_sample_snapshot(host.get()),
            "offline stepping/backwards LAN time must reject without mutating network game");
    require(tanks_sample_lan_poll(host.get(), now, 0) == 0 && sample(host).events.empty(),
            "zero-tick LAN poll must not replay prior tick effects");
    require(tanks_sample_lan_status(host.get()) &&
                std::string(tanks_sample_lan_status(host.get())).find("\"role\":\"host\"") != std::string::npos,
            "LAN UI must expose role/status");
    require(tanks_sample_lan_stop(host.get()) == 0 && sample(host).lan->session.phase() == LanPhase::Idle,
            "LAN stop failed");
    require(tanks_sample_lan_poll(guest.get(), now + kStep, 0) == -1 &&
                sample(guest).lan->session.phase() == LanPhase::Failed &&
                tanks_sample_step(host.get(), kStep, 0, 0) == -1,
            "disconnect must end network game and never fall back silently to offline");
    require(tanks_sample_reset(host.get(), 1, 1, 1, 0) == 0 && !sample(host).lan &&
                tanks_sample_step(host.get(), kStep, 0, 0) == 0,
            "explicit fresh reset must release LAN and restore offline play");
    settings.ai_p2 = 1;
    const std::string offline = tanks_sample_snapshot(host.get());
    require(tanks_sample_lan_host(host.get(), 1, &settings, 41987, now) == -1 &&
                tanks_sample_lan_join(host.get(), "bad:address", 0, 0, 50, now) == -1 &&
                offline == tanks_sample_snapshot(host.get()),
            "invalid LAN requests must preserve previous offline game");
    require(tanks3d::godot_sample::lanFingerprint().find("tanks3d-godot-lan-v1/") == 0,
            "LAN compatibility must identify native image, not the Godot engine");
}

void testNativePad()
{
    auto handle = create();
    const auto pad = [&](float x, float y, unsigned bits = 0U)
    {
        return tanks_sample_map_pad(handle.get(), 0, x, y, bits, 0);
    };
    require(pad(.25f, 0) == 8 && pad(.15f, 0) == 8 && pad(.10f, 0) == 0 &&
                pad(.19f, 0) == 0, "native controller hysteresis must survive bridge samples");
    require(pad(1, 0, 1) == 1, "D-pad must take priority over a conflicting stick");
    require(pad(0, 0, 32) == 48 && pad(0, 0, 32) == 16,
            "bottom face must hold fire but confirm only once");
    require(pad(0, 0, 64) == 96 && pad(0, 0, 64) == 0,
            "Start must pulse native confirm and pause without repeats");
    require(pad(0, 0, 128) == 128 && pad(0, 0, 128) == 0,
            "Back must be a cancel edge, never a face button");
    require(tanks_sample_reset_pad(handle.get(), 0, 1) == 0 && pad(.8f, 0) == 0 &&
                pad(0, 0) == 0 && pad(.8f, 0) == 8,
            "returning to menu must suppress held stick until centered");
    require(tanks_sample_map_pad(handle.get(), 4, 0, 0, 0, 0) == -1 &&
                tanks_sample_map_pad(handle.get(), 0, std::numeric_limits<float>::quiet_NaN(), 0, 0, 0) == -1,
            "invalid controller samples must reject");
    for (int yaw : {-45, -20, 0, 25, 45})
    {
        tanks3d::app::GamepadInputState native;
        require(tanks_sample_reset_pad(handle.get(), 1, 0) == 0, "controller reset failed");
        for (const auto axis : {XZ{1, 0}, XZ{.8f, .9f}, XZ{0, -1}, XZ{-1, 0}, XZ{0, .1f}})
        {
            tanks3d::app::GamepadSnapshot input;
            input.available = true;
            input.leftStickX = axis.x;
            input.leftStickY = axis.z;
            const auto expected = tanks3d::app::mapGamepadInput(input, native, yaw).player;
            const int bits = (expected.north.held ? 1 : 0) | (expected.south.held ? 2 : 0) |
                             (expected.west.held ? 4 : 0) | (expected.east.held ? 8 : 0);
            require(tanks_sample_map_pad(handle.get(), 1, axis.x, axis.z, 0, yaw) == bits,
                    "camera-relative controller directions diverged from native mapper");
        }
    }
}

// Production stage 1 geometry plus the actual Godot controller mapper. These
// are input-driven movement checks, not a visual or physical latency claim.
void testPrecisePlayerTurns()
{
    constexpr float tolerance = 0.00001f;
    for (int inputSource : {0, 1, 2}) // keyboard commands, D-pad, stick
    {
        for (int playerSlot : {0, 1})
        {
            auto handle = create();
            const auto reset = [&](XZ position) -> Game3D &
            {
                require(tanks_sample_reset(handle.get(), 731, 1, 2, 0) == 0,
                        "precise turn reset failed");
                Game3D &game = *sample(handle).game;
                require(Game3DTestAccess::prepareGameEventScenario(game, false),
                        "precise turn fixture preparation failed");
                Game3DTestAccess::player(game, playerSlot).position = position;
                Game3DTestAccess::player(game, 1 - playerSlot).position = {9.0f, 25.0f};
                require(!game.map().collidesWithTank(position, 0.875f),
                        "precise turn fixture starts inside terrain");
                return game;
            };
            const auto drive = [&](unsigned direction, bool fire = false)
            {
                int command = static_cast<int>(direction | (fire ? 16U : 0U));
                if (inputSource != 0)
                {
                    const float x = direction == 8 ? 1.0f : direction == 4 ? -1.0f : 0.0f;
                    const float y = direction == 2 ? 1.0f : direction == 1 ? -1.0f : 0.0f;
                    command = tanks_sample_map_pad(
                        handle.get(), playerSlot,
                        inputSource == 2 ? x : 0.0f, inputSource == 2 ? y : 0.0f,
                        (inputSource == 1 ? direction : 0U) | (fire ? 16U : 0U), 0);
                }
                require(command >= 0 && tanks_sample_step(
                            handle.get(), kStep, playerSlot == 0 ? command : 0,
                            playerSlot == 1 ? command : 0) == 0,
                        "precise turn input step failed");
            };
            {
                Game3D &game = reset({1.0f, 25.0f});
                drive(8);
                const XZ east = game.players()[playerSlot].position;
                drive(1, true);
                const Player &player = game.players()[playerSlot];
                require(std::fabs(east.x - (1.0f + 5.0f * kStep)) < tolerance &&
                            std::fabs(player.position.x - east.x) < tolerance &&
                            std::fabs(player.position.z - (25.0f - 5.0f * kStep)) < tolerance &&
                            player.driveDirection == CardinalDirection::North && player.moving,
                        "wall-corner micro-turn erased a short step or delayed movement");
                require(!game.shells().empty() && game.shells()[0].ownerIndex == playerSlot &&
                            game.shells()[0].velocity.x == 0.0f && game.shells()[0].velocity.z < 0.0f,
                        "micro-turn/fire did not use the new direction and player slot");
                require(game.players()[1 - playerSlot].position.x == 9.0f &&
                            game.players()[1 - playerSlot].position.z == 25.0f,
                        "precise movement leaked into the other player");
            }
            {
                Game3D &game = reset({1.2f, 25.0f});
                drive(1);
                require(game.players()[playerSlot].position.x == 1.2f,
                        "clear entry rounded position before assistance was needed");
                for (int tick = 0; tick < 3; ++tick)
                {
                    const float previousZ = game.players()[playerSlot].position.z;
                    drive(1);
                    const Player &player = game.players()[playerSlot];
                    require(player.position.z < previousZ &&
                                !game.map().collidesWithTank(player.position, 0.875f),
                            "held direction stalled or crossed terrain at the narrow lane entry");
                }
                require(game.players()[playerSlot].position.x == 1.0f &&
                            std::fabs(game.players()[playerSlot].position.z -
                                      (25.0f - 4.0f * 5.0f * kStep)) < tolerance,
                        "lane assistance changed forward movement speed");
            }
            {
                Game3D &game = reset({1.35f, 25.0f});
                drive(1);
                const XZ before = game.players()[playerSlot].position;
                drive(1);
                require(game.players()[playerSlot].position.x == before.x &&
                            game.players()[playerSlot].position.z == before.z &&
                            !game.players()[playerSlot].moving,
                        "lane assistance pulled the tank beyond the 5/16-tile range");
            }
            {
                Game3D &game = reset({1.125f, 23.0f});
                drive(8, true);
                const Player &player = game.players()[playerSlot];
                const bool firedEast = std::any_of(
                    game.eventsThisUpdate().begin(), game.eventsThisUpdate().end(),
                    [playerSlot](const auto &event) {
                        return event.type == GameEventType::ShellFired &&
                            event.sourcePlayerId == playerSlot &&
                            event.direction == CardinalDirection::East;
                    });
                require(player.position.x == 1.125f && player.position.z == 23.0f &&
                            player.driveDirection == CardinalDirection::East && !player.moving && firedEast,
                        "turning against a solid wall moved the tank or prevented immediate fire");
            }
            {
                Game3D &game = reset({1.2f, 25.0f});
                Game3DTestAccess::player(game, 1 - playerSlot).position = {1.0f, 23.25f};
                drive(1);
                const Player &player = game.players()[playerSlot];
                require(player.position.x == 1.2f && player.position.z == 25.0f && !player.moving,
                        "failed lane assistance moved sideways or overlapped the other player");
            }
        }
    }
}

void testMenuSessionContinuation()
{
    auto handle = create();
    auto config = tanks3d::godot_sample::defaultConfig();
    require(tanks_sample_start_config(handle.get(), &config) == -1,
            "menu continuation needs an initialized seeded session");
    require(tanks_sample_reset_config(handle.get(), 731, &config) == 0, "menu session reset failed");
    Game3D reference("resources", 731U);
    require(reference.start(1, 3, 1, {{Nation::UnitedStates, Nation::SovietUnion}}),
            "menu reference start failed");
    for (int tick = 0; tick < 450; ++tick)
    {
        reference.update(kStep, {});
        require(tanks_sample_step(handle.get(), kStep, 0, 0) == 0, "menu prelude failed");
    }
    for (Game3D *game : {&reference, sample(handle).game.get()})
    {
        Game3DTestAccess::setPlayerDirectKillTally(*game, 0, 0, 5000);
        Game3DTestAccess::beginSettlement(*game, true, false);
        game->confirmSettlement();
        game->confirmSettlement();
    }
    require(reference.settlementHighScore() == 5000 && sample(handle).game->settlementHighScore() == 5000,
            "fixture must establish a real completed highscore transition");
    config.stage = 3;
    config.players = 2;
    config.lives = 10;
    config.max_hp = 4;
    config.nation_p1 = 1;
    config.nation_p2 = 2;
    require(reference.start(2, 10, 3, {{Nation::SovietUnion, Nation::Germany}}, {4, 0, 0, 0}) &&
                tanks_sample_start_config(handle.get(), &config) == 0 &&
                reference.sessionDigest() == sample(handle).game->sessionDigest() &&
                sample(handle).game->settlementHighScore() == 5000,
            "menu deployment must preserve session highscore/RNG through real production start");
    for (int tick = 0; tick < 360; ++tick)
    {
        reference.update(kStep, {});
        require(tanks_sample_step(handle.get(), kStep, 0, 0) == 0 &&
                    reference.sessionDigest() == sample(handle).game->sessionDigest(),
                "next-run enemy random sequence diverged from original session");
    }
    Game3DTestAccess::rejectStageLoads(*sample(handle).game);
    const std::string before = tanks_sample_snapshot(handle.get());
    require(tanks_sample_start_config(handle.get(), &config) == -1 &&
                before == tanks_sample_snapshot(handle.get()),
            "failed next-run load must preserve complete prior app session");
}

void testLanSocketBridge()
{
    auto host = create();
    auto guest = create();
    auto config = tanks3d::godot_sample::defaultConfig();
    config.players = 2;
    config.max_hp = 6;
    config.nation_p1 = 2;
    const auto started = std::chrono::steady_clock::now();
    const auto now = [&]()
    {
        return std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
    };
    const int hosted = tanks_sample_lan_host(host.get(), 731, &config, 0, now());
    require(hosted == 0, tanks_sample_error(host.get()));
    const auto address = "127.0.0.1:" + std::to_string(sample(host).lan->channel->port());
    const int joined = tanks_sample_lan_join(guest.get(), address.c_str(), 1, 0, 50, now());
    require(joined == 0, tanks_sample_error(guest.get()));
    int matches = 0;
    while (now() < 12 && sample(guest).tick < 360)
    {
        const int hostResult = tanks_sample_lan_poll(host.get(), now(), 17);
        require(hostResult == 0, tanks_sample_error(host.get()));
        const int guestResult = tanks_sample_lan_poll(guest.get(), now(), 24);
        require(guestResult == 0, tanks_sample_error(guest.get()));
        if (sample(host).game && sample(guest).game && sample(host).tick == sample(guest).tick)
        {
            require(sample(host).game->sessionDigest() == sample(guest).game->sessionDigest(),
                    "real TCP bridge peers diverged");
            ++matches;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    require(sample(guest).tick >= 360 && matches > 100 && sample(host).config.nation_p2 == 1,
            "real TCP wrapper did not finish handshake, both player inputs and digest intervals");
    tanks_sample_lan_stop(host.get());
    bool failed = false;
    for (int attempt = 0; attempt < 200 && !failed; ++attempt)
    {
        failed = tanks_sample_lan_poll(guest.get(), now(), 0) < 0;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    require(failed && tanks_sample_step(guest.get(), kStep, 0, 0) < 0,
            "real socket disconnect must end battle and prohibit silent offline advancement");
}
} // namespace

int main(int argc, char **argv)
{
    try
    {
        if (argc == 2 && std::string(argv[1]) == "--tank-overlap")
        {
            testTankOverlapRecovery();
            testTankCreationOccupancy();
            std::cout << "PASS canonical tank occupancy: overlap recovery, creation gates, respawn, spawn reservations\n";
            return 0;
        }
        if (argc == 2 && std::string(argv[1]) == "--precise-turns")
        {
            testPrecisePlayerTurns();
            std::cout << "PASS precise turns: real stage 1 walls, micro-steps, held lane entry, collision, turn/fire, both player slots, keyboard/D-pad/stick\n";
            return 0;
        }
        if (argc == 2 && std::string(argv[1]) == "--lan-sockets")
        {
            testLanSocketBridge();
            std::cout << "PASS Godot C bridge: real TCP host/join, 360 ticks, matching digests and disconnect\n";
            return 0;
        }
        testShellFlightPresentation();
        testInvalidInputAndReadOnlySnapshot();
        testTankOverlapRecovery();
        testTankCreationOccupancy();
        testSeededProductionParity();
        testEnemyNationsForSoloHumanAndAiCoop();
        testAiPauseResetAndManualControlIsolation();
        testOrderedAudioOutput();
        testReportConfirmation();
        testEventMetadata();
        testFullConfiguration();
        testLanBridge(Nation::Germany);
        testLanBridge(Nation::UnitedStates);
        testNativePad();
        testPrecisePlayerTurns();
        testMenuSessionContinuation();
        std::cout << "PASS Godot C bridge: 35-stage production parity, read-only snapshots, "
                     "overlap recovery/creation occupancy, full setup/restart, solo/human/AI opponent nations, input validation, native AI/P2 isolation, pause, reports, "
                     "same/different-nation LAN handshake/state/RNG/events native controller mapping and precise wall-corner turns\n";
    }
    catch (const std::exception &exception)
    {
        std::cerr << "FAIL Godot C bridge: " << exception.what() << '\n';
        return 1;
    }
}
