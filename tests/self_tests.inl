#ifndef TANKS3D_SELF_TESTS_INL
#define TANKS3D_SELF_TESTS_INL

tanks3d_test::Reporter gSelfTestReporter;
constexpr int kTerrainFixtureStage = 2;

enum class AudioOutputCallType : unsigned char
{
    Play,
    UpdateEngine,
    StopAll
};

enum class AudioBoundaryTraceStep : unsigned char
{
    StopAllOutput,
    PlayOutput,
    SemanticObserver,
    EngineOutput
};

struct AudioOutputCall
{
    AudioOutputCallType type = AudioOutputCallType::StopAll;
    AudioCue cue = AudioCue::Count;
    bool active = false;
    bool moving = false;

    bool operator==(const AudioOutputCall &other) const
    {
        return type == other.type && cue == other.cue &&
               active == other.active && moving == other.moving;
    }
};

AudioOutputCall playOutputCall(AudioCue cue)
{
    AudioOutputCall call;
    call.type = AudioOutputCallType::Play;
    call.cue = cue;
    return call;
}

AudioOutputCall engineOutputCall(bool active, bool moving)
{
    AudioOutputCall call;
    call.type = AudioOutputCallType::UpdateEngine;
    call.active = active;
    call.moving = moving;
    return call;
}

AudioOutputCall stopOutputCall()
{
    return AudioOutputCall{};
}

class RecordingAudioOutput final : public AudioOutput
{
public:
    void play(AudioCue cue) override
    {
        record(playOutputCall(cue));
    }

    void updateEngine(bool active, bool moving) override
    {
        record(engineOutputCall(active, moving));
    }

    void stopAll() override
    {
        record(stopOutputCall());
    }

    void clear() { calls.clear(); }

    std::vector<AudioOutputCall> calls{};
    // Synchronous test-only seam. Callers must not retain the call reference.
    std::function<void(const AudioOutputCall &)> onCall{};

private:
    void record(AudioOutputCall call)
    {
        calls.push_back(std::move(call));
        if (onCall)
            onCall(calls.back());
    }
};

struct ShellCancellationPresentationSnapshot
{
    ShellCancellationPresentationStep step =
        ShellCancellationPresentationStep::EventAppended;
    ShellCancellationPresentationCommand command{};
    std::size_t eventCount = 0U;
    std::size_t effectCount = 0U;
    std::size_t impactingShellCount = 0U;
};

struct ShellMapCorePresentationSnapshot
{
    ShellMapCorePresentationStep step =
        ShellMapCorePresentationStep::PhysicalEventsAppended;
    ShellMapCorePresentationAction action{};
    AudioCue audioCue = AudioCue::Count;
    std::size_t eventCount = 0U;
    std::size_t effectCount = 0U;
    std::vector<GameEvent> events{};
    std::vector<Shell> shells{};
    std::array<float, 2> cameraShake{};
};

struct ShellTankPresentationSnapshot
{
    ShellTankPresentationStep step =
        ShellTankPresentationStep::PhysicalEventsAppended;
    ShellTankPresentationAction action{};
    bool preCommitFx = false;
    ShellPhysicalImpactResult physicalImpact{};
    Shell shell{};
    AudioCue audioCue = AudioCue::Count;
    std::size_t eventCount = 0U;
    std::size_t effectCount = 0U;
    std::vector<Player> players{};
    std::vector<Enemy> enemies{};
    std::array<float, 2> cameraShake{};
};

struct EnemyShellLaunchPresentationSnapshot
{
    EnemyShellLaunchPresentationStep step =
        EnemyShellLaunchPresentationStep::ShellInserted;
    EnemyShellLaunchIntent intent{};
    std::vector<GameEvent> events{};
    std::vector<Shell> shells{};
    std::size_t effectCount = 0U;
    float enemyFireCooldown = 0.0f;
    std::array<float, 2> cameraShake{};
};

enum class PlayerShellLaunchTraceStep : unsigned char
{
    ShellInserted,
    EventAppended,
    MuzzleFlashSpawned,
    CameraShakeCommitted,
    AudioOutput,
    SemanticAudio,
    AudioRequested
};

struct PlayerShellLaunchPresentationSnapshot
{
    PlayerShellLaunchPresentationStep step =
        PlayerShellLaunchPresentationStep::ShellInserted;
    PlayerShellLaunchIntent intent{};
    std::vector<GameEvent> events{};
    std::vector<Shell> shells{};
    std::vector<AudioCue> audioRequests{};
    std::size_t effectCount = 0U;
    float playerFireCooldown = 0.0f;
    std::array<float, 2> cameraShake{};
};

struct BonusReleasePresentationSnapshot
{
    BonusReleasePresentationStep step =
        BonusReleasePresentationStep::PickupInserted;
    BonusReleaseIntent intent{};
    std::vector<Pickup> bonuses{};
    std::vector<GameEvent> events{};
    std::vector<AudioCue> audioRequests{};
    std::vector<Player> players{};
    std::vector<Enemy> enemies{};
};

struct BonusCollectionPresentationSnapshot
{
    BonusCollectionPresentationStep step =
        BonusCollectionPresentationStep::CollectionEventAppended;
    BonusCollectionIntent intent{};
    bool hasApplication = false;
    BonusApplication application{};
    std::vector<Pickup> bonuses{};
    std::vector<GameEvent> events{};
    std::vector<AudioCue> audioRequests{};
    std::vector<Player> players{};
    std::vector<Enemy> enemies{};
    std::size_t effectCount = 0U;
    std::array<float, 2> cameraShake{};
    std::string message{};
    float messageTimer = 0.0f;
};

struct SettlementBeginPresentationSnapshot
{
    SettlementBeginPresentationStep step =
        SettlementBeginPresentationStep::PlanReady;
    SettlementBeginPlan plan{};
    SettlementPhase phase = SettlementPhase::None;
    int settlementStage = 1;
    int scoreCounter = 0;
    int maximumScore = 0;
    int categoryIndex = 0;
    bool gameOver = false;
    std::size_t audioOutputCallCount = 0U;
    std::vector<GameEvent> events{};
};

struct SettlementTransitionPresentationSnapshot
{
    SettlementTransitionPresentationStep step =
        SettlementTransitionPresentationStep::PlanReady;
    SettlementTransitionPlan plan{};
    int stage = 1;
    int mapStage = 1;
    int highScore = 0;
    bool settling = false;
    bool highScoreDisplay = false;
    float highScoreDisplayTimer = 0.0f;
    bool awaitingMenu = false;
    bool menuRequested = false;
    float stageIntroTimer = 0.0f;
    std::string lastError{};
    std::vector<Player> players{};
    std::vector<AudioCue> audioRequests{};
};

struct SettlementAudioRequestSnapshot
{
    AudioCue cue = AudioCue::Count;
    int stage = 1;
    int mapStage = 1;
    SettlementPhase settlementPhase = SettlementPhase::None;
    int settlementStage = 1;
    int settlementScoreCounter = 0;
    int settlementCategoryIndex = 0;
    std::array<std::array<int, kEnemyTypeCount>, 2>
        settlementDisplayedKills{};
    bool baseAlive = false;
    bool gameOver = false;
    bool paused = false;
    bool highScoreDisplay = false;
    bool awaitingMenu = false;
    bool menuRequested = false;
    float stageIntroTimer = 0.0f;
    float stageTransitionTimer = 0.0f;
    float gameOverReportTimer = 0.0f;
    int enemiesRemaining = 0;
    int nextSpawnIndex = 0;
    int nextEnemyId = 0;
    std::vector<Player> players{};
    std::size_t enemyCount = 0U;
    std::size_t shellCount = 0U;
    std::size_t bonusCount = 0U;
    std::size_t effectCount = 0U;
    std::array<float, 2> cameraShake{};
    std::string bonusMessage{};
    float bonusMessageTimer = 0.0f;
};

bool settlementPlayerStatesMatch(const Player &first, const Player &second)
{
    return first.id == second.id && first.nation == second.nation &&
           first.position.x == second.position.x &&
           first.position.z == second.position.z && first.yaw == second.yaw &&
           first.driveDirection == second.driveDirection &&
           first.movementDirection == second.movementDirection &&
           first.lives == second.lives &&
           first.maximumHitPoints == second.maximumHitPoints &&
           first.hitPoints == second.hitPoints && first.level == second.level &&
           first.active == second.active && first.moving == second.moving &&
           first.hasBoat == second.hasBoat &&
           first.shieldTimer == second.shieldTimer &&
           first.creationTimer == second.creationTimer &&
           first.respawnTimer == second.respawnTimer &&
           first.deathTimer == second.deathTimer &&
           first.fireCooldown == second.fireCooldown &&
           first.dustCooldown == second.dustCooldown &&
           first.iceSlipTimer == second.iceSlipTimer &&
           first.onIce == second.onIce && first.score == second.score &&
           first.directKillStreak == second.directKillStreak &&
           first.streakPopupTimer == second.streakPopupTimer &&
           first.stageTally.destroyed == second.stageTally.destroyed &&
           first.stageTally.enemyPoints == second.stageTally.enemyPoints &&
           first.stageTally.bonusPoints == second.stageTally.bonusPoints &&
           first.stageTally.scoreAtStageStart ==
               second.stageTally.scoreAtStageStart;
}

bool settlementPlayerVectorsMatch(const std::vector<Player> &first,
                                  const std::vector<Player> &second)
{
    if (first.size() != second.size())
        return false;
    for (std::size_t index = 0; index < first.size(); ++index)
    {
        if (!settlementPlayerStatesMatch(first[index], second[index]))
            return false;
    }
    return true;
}

std::vector<Player> expectedSettlementProgression(
    std::vector<Player> players)
{
    for (Player &player : players)
    {
        if (player.lives <= 0)
        {
            player.lives = 2;
            player.level = 0;
        }
        else
            player.lives = player.lives >= 99 ? 99 : player.lives + 1;
    }
    return players;
}

std::vector<Player> expectedSettlementStageEntry(
    std::vector<Player> players)
{
    players = expectedSettlementProgression(std::move(players));
    for (Player &player : players)
    {
        player.position = playerSpawn(player.id);
        player.yaw = 0.0f;
        player.driveDirection = CardinalDirection::North;
        player.movementDirection = CardinalDirection::North;
        if (player.hitPoints <= 0)
            player.hitPoints = player.maximumHitPoints;
        player.active = true;
        player.moving = false;
        player.hasBoat = false;
        player.shieldTimer = 10.0f;
        player.creationTimer = 1.0f;
        player.respawnTimer = 0.0f;
        player.deathTimer = 0.0f;
        player.fireCooldown = kPlayerReloadTime;
        player.dustCooldown = 0.0f;
        player.iceSlipTimer = 0.0f;
        player.onIce = false;
        player.streakPopupTimer = 0.0f;
        player.stageTally.destroyed = {};
        player.stageTally.enemyPoints = {};
        player.stageTally.bonusPoints = 0;
        player.stageTally.scoreAtStageStart = player.score;
    }
    return players;
}

// Transitional same-translation-unit probe. It invokes the production spawn
// and bonus paths directly until input commands and rule modules are extracted.
struct Game3DTestAccess
{
    static void resetPlayerInputRuntime(Game3D &game)
    {
        game.stageIntroTimer_ = 0.0f;
        game.stageTransitionTimer_ = 0.0f;
        game.gameOver_ = false;
        game.paused_ = false;
        game.baseAlive_ = true;
        game.enemies_.clear();
        game.shells_.clear();
        game.bonuses_.clear();
        game.effects_.clear();
        game.cameraShake_ = {};
        game.enemySpawnState_.remaining = 1;
        game.enemySpawnState_.timer = 10000.0f;

        static constexpr std::array<XZ, 2> positions{{
            {7.0f, 13.0f}, {19.0f, 13.0f}}};
        for (std::size_t index = 0; index < game.players_.size(); ++index)
        {
            Player &player = game.players_[index];
            player.position = positions[index];
            player.driveDirection = CardinalDirection::North;
            player.movementDirection = CardinalDirection::North;
            player.yaw = cardinalYaw(CardinalDirection::North);
            player.level = 0;
            player.active = true;
            player.moving = false;
            player.hasBoat = false;
            player.creationTimer = 0.0f;
            player.respawnTimer = 0.0f;
            player.deathTimer = 0.0f;
            player.fireCooldown = 0.0f;
            player.dustCooldown = 0.0f;
            player.iceSlipTimer = 0.0f;
            player.onIce = false;
        }
    }

    static bool preparePlayerInputScenario(Game3D &game)
    {
        game.map_.prepareShowcaseArena();
        resetPlayerInputRuntime(game);
        return !game.players_.empty();
    }

    static bool preparePlayerIceInputScenario(Game3D &game, XZ &anchor)
    {
        resetPlayerInputRuntime(game);
        if (game.players_.empty())
            return false;
        for (int row = 1; row < kMapSize - 2; ++row)
        {
            for (int column = 1; column < kMapSize - 2; ++column)
            {
                bool iceBlock = true;
                for (int rowOffset = 0; rowOffset < 2; ++rowOffset)
                    for (int columnOffset = 0; columnOffset < 2;
                         ++columnOffset)
                        iceBlock = iceBlock &&
                            game.map_.tile(row + rowOffset,
                                           column + columnOffset) == '-';
                if (!iceBlock)
                    continue;
                const XZ candidate{static_cast<float>(column + 1),
                                   static_cast<float>(row + 1)};
                if (!game.positionAvailable(candidate, 0, -1) ||
                    !game.positionAvailable(
                        candidate + cardinalVector(
                                        CardinalDirection::North) * 0.25f,
                        0, -1) ||
                    !game.positionAvailable(
                        candidate + cardinalVector(
                                        CardinalDirection::East) * 0.25f,
                        0, -1))
                    continue;
                anchor = candidate;
                game.players_[0].position = candidate;
                return true;
            }
        }
        return false;
    }

    static bool preparePlayerBoatInputScenario(Game3D &game, XZ &anchor)
    {
        resetPlayerInputRuntime(game);
        if (game.players_.empty())
            return false;
        for (int row = 0; row < kMapSize - 1; ++row)
        {
            for (int column = 0; column < kMapSize - 1; ++column)
            {
                if (game.map_.tile(row, column) != '~' ||
                    game.map_.tile(row, column + 1) != '~' ||
                    game.map_.tile(row + 1, column) != '~' ||
                    game.map_.tile(row + 1, column + 1) != '~')
                {
                    continue;
                }
                const XZ candidate{
                    static_cast<float>(column) + 0.5f,
                    static_cast<float>(row) + 0.5f};
                const XZ forward{
                    candidate.x + 0.25f, candidate.z};
                if (game.positionAvailable(candidate, 0, -1, false) ||
                    !game.positionAvailable(candidate, 0, -1, true) ||
                    !game.positionAvailable(forward, 0, -1, true))
                {
                    continue;
                }
                anchor = candidate;
                game.players_[0].position = candidate;
                return true;
            }
        }
        return false;
    }

    static void setPlayerHasBoat(Game3D &game, int playerIndex,
                                 bool hasBoat)
    {
        if (playerIndex < 0 ||
            playerIndex >= static_cast<int>(game.players_.size()))
            return;
        game.players_[static_cast<std::size_t>(playerIndex)].hasBoat =
            hasBoat;
    }

    static void recenterPlayer(Game3D &game, int playerIndex, XZ position)
    {
        if (playerIndex < 0 ||
            playerIndex >= static_cast<int>(game.players_.size()))
            return;
        game.players_[static_cast<std::size_t>(playerIndex)].position =
            position;
    }

    static void refreshCameras(Game3D &game)
    {
        game.resetCameras();
    }

    static void advanceCameras(Game3D &game, float dt)
    {
        game.updateCameras(dt);
    }

    static void setPlayerCreationTimer(Game3D &game, int playerIndex,
                                       float timer)
    {
        if (playerIndex < 0 ||
            playerIndex >= static_cast<int>(game.players_.size()))
            return;
        game.players_[static_cast<std::size_t>(playerIndex)].creationTimer =
            timer;
    }

    static void setPlayerFrameEntryState(
        Game3D &game, int playerIndex, float creationTimer,
        float fireCooldown, float dustCooldown, float shieldTimer,
        float streakPopupTimer, bool moving)
    {
        if (playerIndex < 0 ||
            playerIndex >= static_cast<int>(game.players_.size()))
            return;
        Player &player =
            game.players_[static_cast<std::size_t>(playerIndex)];
        player.creationTimer = creationTimer;
        player.fireCooldown = fireCooldown;
        player.dustCooldown = dustCooldown;
        player.shieldTimer = shieldTimer;
        player.streakPopupTimer = streakPopupTimer;
        player.moving = moving;
    }

    static void setPlayerWaitingForDeath(Game3D &game, int playerIndex)
    {
        if (playerIndex < 0 ||
            playerIndex >= static_cast<int>(game.players_.size()))
            return;
        Player &player = game.players_[static_cast<std::size_t>(playerIndex)];
        player.active = false;
        player.moving = false;
        player.lives = std::max(1, player.lives);
        player.deathTimer = kTankDeathDuration;
        player.respawnTimer = 0.0f;
    }

    static void setPlayerDeathEntryState(
        Game3D &game, int playerIndex, int entityId, bool active,
        int lives, float deathTimer)
    {
        if (playerIndex < 0 ||
            playerIndex >= static_cast<int>(game.players_.size()))
            return;
        Player &player = game.players_[static_cast<std::size_t>(playerIndex)];
        player.id = entityId;
        player.active = active;
        player.moving = false;
        player.lives = lives;
        player.deathTimer = deathTimer;
        player.respawnTimer = 0.0f;
    }

    static bool markPlayerShellImpactingAndReadyToFire(
        Game3D &game, int playerIndex)
    {
        if (playerIndex < 0 ||
            playerIndex >= static_cast<int>(game.players_.size()))
            return false;
        for (Shell &shell : game.shells_)
        {
            if (shell.owner != ShellOwner::Player ||
                shell.ownerIndex != playerIndex)
                continue;
            shell.impacting = true;
            shell.life = kShellImpactDuration;
            game.players_[static_cast<std::size_t>(playerIndex)]
                .fireCooldown = 0.0f;
            return true;
        }
        return false;
    }

    static bool prepareGameEventScenario(Game3D &game,
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
        game.effects_.clear();
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

    static void addEventEnemy(Game3D &game, int id, int type, int armor,
                              XZ position, bool carriesBonus = false,
                              bool destroyed = false)
    {
        Enemy enemy;
        enemy.id = id;
        enemy.type = type;
        enemy.armor = armor;
        enemy.position = position;
        enemy.target = kGovernmentBaseCenter;
        enemy.carriesBonus = carriesBonus;
        enemy.destroyed = destroyed;
        enemy.frozenTimer = destroyed ? 0.0f : 10000.0f;
        enemy.deathTimer = destroyed ? kTankDeathDuration : 0.0f;
        game.enemies_.push_back(enemy);
    }

    static void addEventShell(Game3D &game, ShellOwner owner, int ownerId,
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

    static bool resolveEventShellImpactWithRandom(
        Game3D &game, std::size_t shellIndex, RandomSource &random)
    {
        if (shellIndex >= game.shells_.size())
            return false;
        return game.resolveFlyingShellImpact(
            game.shells_[shellIndex], random);
    }

    static void captureShellCancellationPresentation(
        Game3D &game,
        std::vector<ShellCancellationPresentationSnapshot> &snapshots)
    {
        game.shellCancellationPresentationObserver_ =
            [&game, &snapshots](
                ShellCancellationPresentationStep step,
                const ShellCancellationPresentationCommand &command) {
                ShellCancellationPresentationSnapshot snapshot;
                snapshot.step = step;
                snapshot.command = command;
                snapshot.eventCount = game.eventsThisUpdate_.size();
                snapshot.effectCount = game.effects_.activeCount();
                snapshot.impactingShellCount = static_cast<std::size_t>(
                    std::count_if(
                        game.shells_.begin(), game.shells_.end(),
                        [](const Shell &shell) { return shell.impacting; }));
                snapshots.push_back(std::move(snapshot));
            };
    }

    static void captureShellMapCorePresentation(
        Game3D &game,
        std::vector<ShellMapCorePresentationSnapshot> &snapshots)
    {
        game.shellMapCorePresentationObserver_ =
            [&game, &snapshots](
                const ShellMapCorePresentationAction &action) {
                ShellMapCorePresentationSnapshot snapshot;
                snapshot.step = shellMapCorePresentationStep(action);
                snapshot.action = action;
                if (const auto *audio =
                        std::get_if<RequestMapCoreAudioAction>(&action))
                    snapshot.audioCue = audio->cue;
                snapshot.eventCount = game.eventsThisUpdate_.size();
                snapshot.effectCount = game.effects_.activeCount();
                snapshot.events = game.eventsThisUpdate_;
                snapshot.shells = game.shells_;
                snapshot.cameraShake = game.cameraShake_;
                snapshots.push_back(std::move(snapshot));
            };
    }

    static void captureShellTankPresentation(
        Game3D &game,
        std::vector<ShellTankPresentationSnapshot> &snapshots)
    {
        const auto appendSnapshot =
            [&game, &snapshots](ShellTankPresentationSnapshot snapshot) {
                snapshot.eventCount = game.eventsThisUpdate_.size();
                snapshot.effectCount = game.effects_.activeCount();
                snapshot.players = game.players_;
                snapshot.enemies = game.enemies_;
                snapshot.cameraShake = game.cameraShake_;
                snapshots.push_back(std::move(snapshot));
            };
        game.playerTankPreCommitFxObserver_ =
            [appendSnapshot](const SpawnTankArmorImpactAction &action,
                             const Player &, const Shell &shell) {
                ShellTankPresentationSnapshot snapshot;
                snapshot.step =
                    ShellTankPresentationStep::ArmorImpactFxSpawned;
                snapshot.action = action;
                snapshot.preCommitFx = true;
                snapshot.shell = shell;
                appendSnapshot(std::move(snapshot));
            };
        game.shellTankPresentationObserver_ =
            [appendSnapshot](
                const ShellTankPresentationAction &action,
                const ShellPhysicalImpactResult &physicalImpact,
                const Shell &shell) {
                ShellTankPresentationSnapshot snapshot;
                snapshot.step = shellTankPresentationStep(action);
                snapshot.action = action;
                snapshot.physicalImpact = physicalImpact;
                snapshot.shell = shell;
                if (const auto *audio =
                        std::get_if<RequestTankAudioAction>(&action))
                    snapshot.audioCue = audio->cue;
                appendSnapshot(std::move(snapshot));
            };
    }

    static bool hasTankPresentationObservers(const Game3D &game)
    {
        return static_cast<bool>(game.shellTankPresentationObserver_) ||
               static_cast<bool>(game.playerTankPreCommitFxObserver_);
    }

    static void captureEnemyShellLaunchPresentation(
        Game3D &game,
        std::vector<EnemyShellLaunchPresentationSnapshot> &snapshots)
    {
        game.enemyShellLaunchPresentationObserver_ =
            [&game, &snapshots](
                EnemyShellLaunchPresentationStep step,
                const EnemyShellLaunchIntent &intent) {
                EnemyShellLaunchPresentationSnapshot snapshot;
                snapshot.step = step;
                snapshot.intent = intent;
                snapshot.events = game.eventsThisUpdate_;
                snapshot.shells = game.shells_;
                snapshot.effectCount = game.effects_.activeCount();
                snapshot.cameraShake = game.cameraShake_;
                const auto enemy = std::find_if(
                    game.enemies_.begin(), game.enemies_.end(),
                    [&](const Enemy &candidate) {
                        return candidate.id == intent.shell.ownerIndex;
                    });
                snapshot.enemyFireCooldown =
                    enemy == game.enemies_.end()
                        ? std::numeric_limits<float>::quiet_NaN()
                        : enemy->fireCooldown;
                snapshots.push_back(std::move(snapshot));
            };
    }

    static bool hasEnemyShellLaunchPresentationObserver(
        const Game3D &game)
    {
        return static_cast<bool>(
            game.enemyShellLaunchPresentationObserver_);
    }

    static void capturePlayerShellLaunchPresentation(
        Game3D &game, const std::vector<AudioCue> &audioRequests,
        std::vector<PlayerShellLaunchPresentationSnapshot> &snapshots,
        std::function<void(PlayerShellLaunchPresentationStep)> onStep = {})
    {
        game.playerShellLaunchPresentationObserver_ =
            [&game, &audioRequests, &snapshots,
             onStep = std::move(onStep)](
                PlayerShellLaunchPresentationStep step,
                const PlayerShellLaunchIntent &intent) {
                PlayerShellLaunchPresentationSnapshot snapshot;
                snapshot.step = step;
                snapshot.intent = intent;
                snapshot.events = game.eventsThisUpdate_;
                snapshot.shells = game.shells_;
                snapshot.audioRequests = audioRequests;
                snapshot.effectCount = game.effects_.activeCount();
                snapshot.cameraShake = game.cameraShake_;
                if (intent.playerIndex >= 0 &&
                    intent.playerIndex <
                        static_cast<int>(game.players_.size()))
                {
                    snapshot.playerFireCooldown =
                        game.players_[static_cast<std::size_t>(
                            intent.playerIndex)].fireCooldown;
                }
                else
                {
                    snapshot.playerFireCooldown =
                        std::numeric_limits<float>::quiet_NaN();
                }
                snapshots.push_back(std::move(snapshot));
                if (onStep)
                    onStep(step);
            };
    }

    static bool hasPlayerShellLaunchPresentationObserver(
        const Game3D &game)
    {
        return static_cast<bool>(
            game.playerShellLaunchPresentationObserver_);
    }

    static void captureBonusReleasePresentation(
        Game3D &game, const std::vector<AudioCue> &audioRequests,
        std::vector<BonusReleasePresentationSnapshot> &snapshots)
    {
        game.bonusReleasePresentationObserver_ =
            [&game, &audioRequests, &snapshots](
                BonusReleasePresentationStep step,
                const BonusReleaseIntent &intent) {
                BonusReleasePresentationSnapshot snapshot;
                snapshot.step = step;
                snapshot.intent = intent;
                snapshot.bonuses = game.bonuses_;
                snapshot.events = game.eventsThisUpdate_;
                snapshot.audioRequests = audioRequests;
                snapshot.players = game.players_;
                snapshot.enemies = game.enemies_;
                snapshots.push_back(std::move(snapshot));
            };
    }

    static bool hasBonusReleasePresentationObserver(
        const Game3D &game)
    {
        return static_cast<bool>(game.bonusReleasePresentationObserver_);
    }

    static void captureBonusCollectionPresentation(
        Game3D &game, const std::vector<AudioCue> &audioRequests,
        std::vector<BonusCollectionPresentationSnapshot> &snapshots)
    {
        game.bonusCollectionPresentationObserver_ =
            [&game, &audioRequests, &snapshots](
                BonusCollectionPresentationStep step,
                const BonusCollectionIntent &intent,
                const BonusApplication *application) {
                BonusCollectionPresentationSnapshot snapshot;
                snapshot.step = step;
                snapshot.intent = intent;
                snapshot.hasApplication = application != nullptr;
                if (application != nullptr)
                    snapshot.application = *application;
                snapshot.bonuses = game.bonuses_;
                snapshot.events = game.eventsThisUpdate_;
                snapshot.audioRequests = audioRequests;
                snapshot.players = game.players_;
                snapshot.enemies = game.enemies_;
                snapshot.effectCount = game.effects_.activeCount();
                snapshot.cameraShake = game.cameraShake_;
                snapshot.message = game.bonusMessage_;
                snapshot.messageTimer = game.bonusMessageTimer_;
                snapshots.push_back(std::move(snapshot));
            };
    }

    static bool hasBonusCollectionPresentationObserver(
        const Game3D &game)
    {
        return static_cast<bool>(
            game.bonusCollectionPresentationObserver_);
    }

    static void captureSettlementBeginPresentation(
        Game3D &game,
        std::vector<SettlementBeginPresentationSnapshot> &snapshots,
        const std::vector<AudioOutputCall> *audioOutputCalls = nullptr)
    {
        game.settlementBeginPresentationObserver_ =
            [&game, &snapshots, audioOutputCalls](
                SettlementBeginPresentationStep step,
                const SettlementBeginPlan &plan) {
                SettlementBeginPresentationSnapshot snapshot;
                snapshot.step = step;
                snapshot.plan = plan;
                snapshot.phase = game.settlement_.phase();
                snapshot.settlementStage = game.settlement_.stage();
                snapshot.scoreCounter = game.settlement_.scoreCounter();
                snapshot.maximumScore = game.settlement_.maximumScore();
                snapshot.categoryIndex = game.settlement_.categoryIndex();
                snapshot.gameOver = game.settlement_.gameOver();
                if (audioOutputCalls != nullptr)
                    snapshot.audioOutputCallCount =
                        audioOutputCalls->size();
                snapshot.events = game.eventsThisUpdate_;
                snapshots.push_back(std::move(snapshot));
            };
    }

    static bool hasSettlementBeginPresentationObserver(
        const Game3D &game)
    {
        return static_cast<bool>(
            game.settlementBeginPresentationObserver_);
    }

    static void captureSettlementTransitionPresentation(
        Game3D &game, const std::vector<AudioCue> &audioRequests,
        std::vector<SettlementTransitionPresentationSnapshot> &snapshots)
    {
        game.settlementTransitionPresentationObserver_ =
            [&game, &audioRequests, &snapshots](
                SettlementTransitionPresentationStep step,
                const SettlementTransitionPlan &plan) {
                SettlementTransitionPresentationSnapshot snapshot;
                snapshot.step = step;
                snapshot.plan = plan;
                snapshot.stage = game.stage_;
                snapshot.mapStage = game.map_.stage();
                snapshot.highScore = game.highScore_;
                snapshot.settling = game.settlement_.active();
                snapshot.highScoreDisplay = game.highScoreDisplay_;
                snapshot.highScoreDisplayTimer =
                    game.highScoreDisplayTimer_;
                snapshot.awaitingMenu = game.awaitingMenu_;
                snapshot.menuRequested = game.returnToMenuRequested_;
                snapshot.stageIntroTimer = game.stageIntroTimer_;
                snapshot.lastError = game.lastError_;
                snapshot.players = game.players_;
                snapshot.audioRequests = audioRequests;
                snapshots.push_back(std::move(snapshot));
            };
    }

    static bool hasSettlementTransitionPresentationObserver(
        const Game3D &game)
    {
        return static_cast<bool>(
            game.settlementTransitionPresentationObserver_);
    }

    static void captureAudioRequests(
        Game3D &game, std::vector<AudioCue> &requests,
        std::function<void(AudioCue)> onRequest = {})
    {
        game.audioRequestObserver_ =
            [&requests, onRequest = std::move(onRequest)](AudioCue cue) {
                requests.push_back(cue);
                if (onRequest)
                    onRequest(cue);
            };
    }

    static void captureSettlementAudioRequests(
        Game3D &game, std::vector<AudioCue> &requests,
        std::vector<SettlementAudioRequestSnapshot> &snapshots)
    {
        game.audioRequestObserver_ =
            [&game, &requests, &snapshots](AudioCue cue) {
                requests.push_back(cue);
                SettlementAudioRequestSnapshot snapshot;
                snapshot.cue = cue;
                snapshot.stage = game.stage_;
                snapshot.mapStage = game.map_.stage();
                snapshot.settlementPhase = game.settlement_.phase();
                snapshot.settlementStage = game.settlement_.stage();
                snapshot.settlementScoreCounter =
                    game.settlement_.scoreCounter();
                snapshot.settlementCategoryIndex =
                    game.settlement_.categoryIndex();
                for (int playerIndex = 0; playerIndex < 2; ++playerIndex)
                {
                    for (int enemyType = 0;
                         enemyType < kEnemyTypeCount; ++enemyType)
                    {
                        snapshot.settlementDisplayedKills[
                            static_cast<std::size_t>(playerIndex)][
                            static_cast<std::size_t>(enemyType)] =
                            game.settlement_.displayedKills(
                                playerIndex, enemyType);
                    }
                }
                snapshot.baseAlive = game.baseAlive_;
                snapshot.gameOver = game.gameOver_;
                snapshot.paused = game.paused_;
                snapshot.highScoreDisplay = game.highScoreDisplay_;
                snapshot.awaitingMenu = game.awaitingMenu_;
                snapshot.menuRequested = game.returnToMenuRequested_;
                snapshot.stageIntroTimer = game.stageIntroTimer_;
                snapshot.stageTransitionTimer = game.stageTransitionTimer_;
                snapshot.gameOverReportTimer = game.gameOverReportTimer_;
                snapshot.enemiesRemaining =
                    game.enemySpawnState_.remaining;
                snapshot.nextSpawnIndex =
                    game.enemySpawnState_.nextSpawnIndex;
                snapshot.nextEnemyId = game.enemySpawnState_.nextEnemyId;
                snapshot.players = game.players_;
                snapshot.enemyCount = game.enemies_.size();
                snapshot.shellCount = game.shells_.size();
                snapshot.bonusCount = game.bonuses_.size();
                snapshot.effectCount = game.effects_.activeCount();
                snapshot.cameraShake = game.cameraShake_;
                snapshot.bonusMessage = game.bonusMessage_;
                snapshot.bonusMessageTimer = game.bonusMessageTimer_;
                snapshots.push_back(std::move(snapshot));
            };
    }

    static bool hasAudioRequestObserver(const Game3D &game)
    {
        return static_cast<bool>(game.audioRequestObserver_);
    }

    static bool hasSessionConfiguration(
        const Game3D &game, int playerCount, int startingLives,
        const std::array<Nation, 2> &startingNations,
        const AdvancedGameSettings &settings)
    {
        return game.playerCount_ == playerCount &&
               game.startingLives_ == startingLives &&
               game.startingNations_ == startingNations &&
               game.advancedSettings_.playerMaximumHitPoints ==
                   settings.playerMaximumHitPoints &&
               game.advancedSettings_.enemySpeedPercent ==
                   settings.enemySpeedPercent &&
               game.advancedSettings_.enemyFireRatePercent ==
                   settings.enemyFireRatePercent &&
               game.advancedSettings_.enemySpawnRatePercent ==
                   settings.enemySpawnRatePercent;
    }

    static void requestAudioCueForTest(Game3D &game, AudioCue cue)
    {
        game.play(cue);
    }

    static void addEventPickup(Game3D &game, BonusType type,
                               XZ position)
    {
        bonus_assets::Pickup pickup;
        pickup.type = type;
        pickup.position = position;
        game.bonuses_.push_back(pickup);
    }

    static void appendRollbackEvent(Game3D &game)
    {
        GameEvent event;
        event.type = GameEventType::BonusSpawned;
        event.bonusType = BonusType::Clock;
        event.position = {9.25f, 7.75f};
        event.stage = game.stage_;
        game.eventsThisUpdate_.push_back(event);
    }

    static void setPlayerCombatState(Game3D &game, int hitPoints,
                                     int directKillStreak = 0)
    {
        if (game.players_.empty())
            return;
        Player &player = game.players_.front();
        player.maximumHitPoints = 3;
        player.hitPoints = std::clamp(hitPoints, 0, 3);
        player.shieldTimer = 0.0f;
        player.hasBoat = false;
        player.directKillStreak = directKillStreak;
    }

    static void setPlayerProtection(Game3D &game, int playerIndex,
                                    float shieldTimer, bool hasBoat)
    {
        if (playerIndex < 0 ||
            playerIndex >= static_cast<int>(game.players_.size()))
            return;
        Player &player = game.players_[static_cast<std::size_t>(playerIndex)];
        player.shieldTimer = shieldTimer;
        player.hasBoat = hasBoat;
    }

    static void setPlayerLives(Game3D &game, int playerIndex, int lives)
    {
        if (playerIndex < 0 ||
            playerIndex >= static_cast<int>(game.players_.size()))
            return;
        game.players_[static_cast<std::size_t>(playerIndex)].lives = lives;
    }

    static void setPlayerHitPoints(Game3D &game, int playerIndex,
                                   int hitPoints)
    {
        if (playerIndex < 0 ||
            playerIndex >= static_cast<int>(game.players_.size()))
            return;
        Player &player = game.players_[static_cast<std::size_t>(playerIndex)];
        player.hitPoints = std::clamp(
            hitPoints, 0, player.maximumHitPoints);
    }

    static void setEnemyFrozenTimer(Game3D &game, int enemyIndex,
                                    float frozenTimer)
    {
        if (enemyIndex < 0 ||
            enemyIndex >= static_cast<int>(game.enemies_.size()))
            return;
        game.enemies_[static_cast<std::size_t>(enemyIndex)].frozenTimer =
            frozenTimer;
    }

    static void setPlayerSettlementState(Game3D &game, int playerIndex,
                                         int lives, int level, int score)
    {
        if (playerIndex < 0 ||
            playerIndex >= static_cast<int>(game.players_.size()))
            return;
        Player &player = game.players_[static_cast<std::size_t>(playerIndex)];
        player.lives = lives;
        player.level = level;
        player.score = score;
    }

    static bool prepareSettlementAdvancePlayerFixture(Game3D &game)
    {
        if (game.players_.size() != 2U)
            return false;

        Player &first = game.players_[0];
        first.position = {4.25f, 8.75f};
        first.yaw = 123.0f;
        first.driveDirection = CardinalDirection::East;
        first.movementDirection = CardinalDirection::South;
        first.maximumHitPoints = 6;
        first.hitPoints = 2;
        first.active = false;
        first.moving = true;
        first.hasBoat = true;
        first.shieldTimer = 2.25f;
        first.creationTimer = 0.37f;
        first.respawnTimer = 0.41f;
        first.deathTimer = 0.29f;
        first.fireCooldown = 0.87f;
        first.dustCooldown = 0.18f;
        first.iceSlipTimer = 0.33f;
        first.onIce = true;
        first.directKillStreak = 7;
        first.streakPopupTimer = 0.75f;

        Player &second = game.players_[1];
        second.position = {20.5f, 9.5f};
        second.yaw = 271.0f;
        second.driveDirection = CardinalDirection::West;
        second.movementDirection = CardinalDirection::East;
        second.maximumHitPoints = 5;
        second.hitPoints = 1;
        second.active = true;
        second.moving = true;
        second.hasBoat = true;
        second.shieldTimer = 4.5f;
        second.creationTimer = 0.62f;
        second.respawnTimer = 0.24f;
        second.deathTimer = 0.48f;
        second.fireCooldown = 0.73f;
        second.dustCooldown = 0.27f;
        second.iceSlipTimer = 0.51f;
        second.onIce = true;
        second.directKillStreak = 4;
        second.streakPopupTimer = 0.55f;
        return true;
    }

    static void rejectStageLoads(Game3D &game)
    {
        game.stageLoadOperation_ =
            [](StageMap &, const fs::path &, int, std::string &error) {
                error = "Injected next-stage validation failure";
                return false;
            };
    }

    static void acceptWrongStageLoads(Game3D &game)
    {
        game.stageLoadOperation_ =
            [](StageMap &map, const fs::path &resourceRoot, int,
               std::string &error) {
                return map.load(resourceRoot, 1, error);
            };
    }

    static void setPlayerDirectKillTally(Game3D &game, int playerIndex,
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

    static void prepareGameOverReport(
        Game3D &game, bool baseAlive, int highScore,
        const std::array<int, 2> &playerScores)
    {
        game.stageIntroTimer_ = 0.0f;
        game.stageTransitionTimer_ = 0.0f;
        game.gameOver_ = true;
        game.gameOverReportTimer_ = kGameOverReportDelay;
        game.baseAlive_ = baseAlive;
        game.highScore_ = highScore;
        for (std::size_t index = 0; index < game.players_.size(); ++index)
        {
            game.players_[index].score = playerScores[index];
            if (baseAlive)
            {
                game.players_[index].active = false;
                game.players_[index].moving = false;
                game.players_[index].lives = 0;
                game.players_[index].hitPoints = 0;
                game.players_[index].deathTimer = 0.0f;
            }
        }
    }

    static void beginSettlement(Game3D &game, bool gameOver,
                                bool baseAlive, bool recordEvent = true)
    {
        game.baseAlive_ = baseAlive;
        game.beginSettlement(gameOver, recordEvent);
    }

    static bool settlementRuntimeReset(const Game3D &game)
    {
        return game.settlement_.phase() == SettlementPhase::None &&
               !game.settlement_.gameOver() &&
               std::fabs(game.settlement_.countTimer()) < 0.0001f &&
               std::fabs(game.settlement_.idleTimer()) < 0.0001f;
    }

    static float cameraShake(const Game3D &game, int playerIndex)
    {
        if (playerIndex < 0 ||
            playerIndex >= static_cast<int>(game.cameraShake_.size()))
            return -1.0f;
        return game.cameraShake_[static_cast<std::size_t>(playerIndex)];
    }

    static void setCameraShake(Game3D &game, int playerIndex, float value)
    {
        if (playerIndex < 0 ||
            playerIndex >= static_cast<int>(game.cameraShake_.size()))
            return;
        game.cameraShake_[static_cast<std::size_t>(playerIndex)] = value;
    }

    static void setPlayerLevel(Game3D &game, int level,
                               int playerIndex = 0)
    {
        if (playerIndex >= 0 &&
            playerIndex < static_cast<int>(game.players_.size()))
        {
            game.players_[static_cast<std::size_t>(playerIndex)].level =
                std::clamp(level, 0, 3);
        }
    }

    static void preparePlayerRespawnEvent(Game3D &game)
    {
        if (game.players_.empty())
            return;
        Player &player = game.players_.front();
        player.active = false;
        player.moving = false;
        player.hitPoints = 0;
        player.lives = 2;
        player.level = 3;
        player.deathTimer = 0.02f;
        player.creationTimer = 0.73f;
        player.fireCooldown = 0.64f;
        player.dustCooldown = 0.43f;
        player.shieldTimer = 0.52f;
        player.directKillStreak = 4;
        player.streakPopupTimer = 0.31f;
        player.score = 2375;
        player.stageTally.destroyed = {{2, 3, 5, 7}};
        player.stageTally.enemyPoints = {{100, 150, 250, 350}};
        player.stageTally.bonusPoints = 600;
        player.stageTally.scoreAtStageStart = 925;
    }

    static void prepareClearedStageEvent(Game3D &game)
    {
        game.enemies_.clear();
        game.shells_.clear();
        game.enemySpawnState_.remaining = 0;
    }

    static void fireEnemyForEvent(Game3D &game, int enemyIndex)
    {
        if (enemyIndex < 0 ||
            enemyIndex >= static_cast<int>(game.enemies_.size()))
            return;
        Enemy &enemy =
            game.enemies_[static_cast<std::size_t>(enemyIndex)];
        enemy.fireCooldown = 0.0f;
        EnemyFireConfiguration configuration;
        configuration.ratePercent =
            game.advancedSettings_.enemyFireRatePercent;
        configuration.shellSpawnDistance = kShellSpawnDistance;
        const EnemyFireOutcome outcome = advanceEnemyFireTransaction(
            enemy, false, configuration,
            EnemyFireRandom{[]() { return 0.5f; }},
            [&](int enemyId) {
                return game.ownedEnemyShellPresent(enemyId);
            },
            [&](const EnemyShellLaunchIntent &intent) {
                game.launchEnemyShell(intent);
            });
        assert(outcome == EnemyFireOutcome::Fired);
    }

    static void firePlayerForEvent(Game3D &game, int playerIndex)
    {
        if (playerIndex < 0 ||
            playerIndex >= static_cast<int>(game.players_.size()))
            return;
        game.players_[static_cast<std::size_t>(playerIndex)].fireCooldown =
            0.0f;
        const PlayerFireOutcome outcome =
            game.advancePlayerFire(playerIndex, true);
        assert(outcome == PlayerFireOutcome::Fired ||
               outcome == PlayerFireOutcome::ShellLimitReached);
    }

    static void setEnemyTargetPlayerState(Game3D &game, int playerIndex,
                                          bool active, XZ position)
    {
        if (playerIndex < 0 ||
            playerIndex >= static_cast<int>(game.players_.size()))
            return;
        Player &player = game.players_[static_cast<std::size_t>(playerIndex)];
        player.active = active;
        player.position = position;
    }

    static XZ chooseEnemyTarget(const Game3D &game, const Enemy &enemy)
    {
        return game.chooseEnemyTarget(enemy);
    }

    static void installEnemyUpdateScenario(Game3D &game,
                                           const Enemy &enemy)
    {
        game.enemies_.clear();
        game.shells_.clear();
        game.eventsThisUpdate_.clear();
        game.effects_.clear();
        game.enemies_.push_back(enemy);
        game.enemySpawnState_.remaining = 0;
        game.enemySpawnState_.timer = 10000.0f;
    }

    static void appendEnemyUpdateScenario(Game3D &game,
                                          const Enemy &enemy)
    {
        game.enemies_.push_back(enemy);
    }

    static void updateEnemies(Game3D &game, float dt, RandomSource &random)
    {
        game.updateEnemies(dt, random);
    }

    static void setEnemyCreationShowcase(Game3D &game, bool enabled)
    {
        game.enemyCreationShowcase_ = enabled;
    }

    static void armEnemySpawn(Game3D &game, int remaining, float timer,
                              int nextSpawn, int nextEnemyId = -1)
    {
        game.enemySpawnState_.remaining = remaining;
        game.enemySpawnState_.timer = timer;
        game.enemySpawnState_.nextSpawnIndex = nextSpawn;
        if (nextEnemyId >= 0)
            game.enemySpawnState_.nextEnemyId = nextEnemyId;
    }

    static void spawnEnemyIfNeeded(Game3D &game, float dt,
                                   RandomSource &random)
    {
        game.spawnEnemyIfNeeded(dt, random);
    }

    static int enemiesRemainingToSpawn(const Game3D &game)
    {
        return game.enemySpawnState_.remaining;
    }

    static float enemySpawnTimer(const Game3D &game)
    {
        return game.enemySpawnState_.timer;
    }

    static int nextEnemySpawn(const Game3D &game)
    {
        return game.enemySpawnState_.nextSpawnIndex;
    }

    static int nextEnemyId(const Game3D &game)
    {
        return game.enemySpawnState_.nextEnemyId;
    }

    static bool sampleRandomEnemy(Game3D &game, Enemy &sample)
    {
        return sampleRandomEnemy(game, game.random_, sample);
    }

    static bool sampleRandomEnemy(Game3D &game, RandomSource &random,
                                  Enemy &sample)
    {
        game.enemies_.clear();
        if (game.enemySpawnState_.remaining <= 0)
            game.enemySpawnState_.remaining = kEnemiesPerStage;
        const int remainingBefore = game.enemySpawnState_.remaining;
        game.enemySpawnState_.timer = 0.0f;
        game.spawnEnemyIfNeeded(0.0f, random);
        if (game.enemies_.size() != 1U ||
            game.enemySpawnState_.remaining != remainingBefore - 1)
            return false;
        sample = game.enemies_.front();
        return true;
    }

    static void makeBandageEligible(Game3D &game)
    {
        if (!game.players_.empty())
        {
            Player &player = game.players_.front();
            player.hitPoints = std::max(1, player.maximumHitPoints - 1);
        }
    }

    static bool sampleRandomBonus(Game3D &game,
                                  bonus_assets::Pickup &sample)
    {
        return sampleRandomBonus(game, game.random_, sample);
    }

    static bool sampleRandomBonus(Game3D &game, RandomSource &random,
                                  bonus_assets::Pickup &sample)
    {
        const std::size_t countBefore = game.bonuses_.size();
        Enemy carrier;
        carrier.carriesBonus = true;
        game.releaseBonus(carrier, random);
        if (game.bonuses_.size() != countBefore + 1U)
            return false;
        sample = game.bonuses_.back();
        return true;
    }
};

class ScriptedRandomSource final : public RandomSource
{
public:
    enum class Kind
    {
        Real,
        Integer
    };

    struct Step
    {
        Kind kind = Kind::Real;
        float realMinimum = 0.0f;
        float realMaximum = 1.0f;
        float realValue = 0.0f;
        int integerMinimum = 0;
        int integerMaximum = 0;
        int integerValue = 0;
    };

    static Step real(float value, float minimum = 0.0f,
                     float maximum = 1.0f)
    {
        Step step;
        step.kind = Kind::Real;
        step.realMinimum = minimum;
        step.realMaximum = maximum;
        step.realValue = value;
        return step;
    }

    static Step integer(int minimum, int maximum, int value)
    {
        Step step;
        step.kind = Kind::Integer;
        step.integerMinimum = minimum;
        step.integerMaximum = maximum;
        step.integerValue = value;
        return step;
    }

    explicit ScriptedRandomSource(
        std::vector<Step> steps,
        std::function<bool()> drawPrecondition = {})
        : steps_(std::move(steps)),
          drawPrecondition_(std::move(drawPrecondition))
    {
    }

    float draw(std::uniform_real_distribution<float> &distribution) override
    {
        observeDrawPrecondition();
        const Step *step = next(Kind::Real);
        if (step == nullptr)
            return distribution.a();
        if (distribution.a() != step->realMinimum ||
            distribution.b() != step->realMaximum)
        {
            fail("real distribution range mismatch at draw " +
                 std::to_string(cursor_ - 1U));
        }
        if (step->realValue < distribution.a() ||
            step->realValue >= distribution.b())
        {
            fail("scripted real value is outside [a,b) at draw " +
                 std::to_string(cursor_ - 1U));
            return distribution.a();
        }
        return step->realValue;
    }

    int draw(std::uniform_int_distribution<int> &distribution) override
    {
        observeDrawPrecondition();
        const Step *step = next(Kind::Integer);
        if (step == nullptr)
            return distribution.a();
        if (distribution.a() != step->integerMinimum ||
            distribution.b() != step->integerMaximum)
        {
            fail("integer distribution range mismatch at draw " +
                 std::to_string(cursor_ - 1U));
        }
        if (step->integerValue < distribution.a() ||
            step->integerValue > distribution.b())
        {
            fail("scripted integer value is outside [a,b] at draw " +
                 std::to_string(cursor_ - 1U));
            return distribution.a();
        }
        return step->integerValue;
    }

    bool complete() const
    {
        return failure_.empty() && cursor_ == steps_.size();
    }

    std::string diagnostic() const
    {
        if (!failure_.empty())
            return failure_;
        if (cursor_ != steps_.size())
        {
            return std::to_string(steps_.size() - cursor_) +
                   " scripted draws were not consumed";
        }
        return {};
    }

private:
    void observeDrawPrecondition()
    {
        if (drawPrecondition_ && !drawPrecondition_())
        {
            fail("random draw precondition failed at draw " +
                 std::to_string(cursor_));
        }
    }

    const Step *next(Kind expected)
    {
        if (cursor_ >= steps_.size())
        {
            fail("random script exhausted at draw " +
                 std::to_string(cursor_));
            return nullptr;
        }
        const Step &step = steps_[cursor_++];
        if (step.kind != expected)
        {
            fail(std::string("random draw kind mismatch at draw ") +
                 std::to_string(cursor_ - 1U) + ": production requested " +
                 (expected == Kind::Real ? "real" : "integer") +
                 ", script supplied " +
                 (step.kind == Kind::Real ? "real" : "integer"));
            return nullptr;
        }
        return &step;
    }

    void fail(std::string message)
    {
        if (failure_.empty())
            failure_ = std::move(message);
    }

    std::vector<Step> steps_;
    std::function<bool()> drawPrecondition_;
    std::size_t cursor_ = 0U;
    std::string failure_;
};

class InvalidBonusTypeRandomSource final : public RandomSource
{
public:
    float draw(std::uniform_real_distribution<float> &distribution) override
    {
        return distribution.a();
    }

    int draw(std::uniform_int_distribution<int> &distribution) override
    {
        ++integerDrawCount_;
        if (integerDrawCount_ == 1)
            return distribution.b() + 1;
        return distribution.a();
    }

    int integerDrawCount() const
    {
        return integerDrawCount_;
    }

private:
    int integerDrawCount_ = 0;
};

template <typename Runner>
int runSelfTestSuite(const char *name, Runner runner)
{
    return tanks3d_test::runSuite(gSelfTestReporter, name, runner);
}

bool checkTest(bool condition, const std::string &message)
{
    return gSelfTestReporter.check(condition, message);
}

int dumpStageSignatures(const fs::path &resourceRoot)
{
    std::string error;
    const std::ios::fmtflags originalFlags = std::cout.flags();
    const char originalFill = std::cout.fill();
    std::cout << "Stage layout signatures:\n";
    for (int stage = 1; stage <= kStageCount; ++stage)
    {
        StageMap map;
        if (!map.load(resourceRoot, stage, error))
        {
            std::cerr << "Unable to generate stage " << stage << ": "
                      << error << '\n';
            return 1;
        }
        if ((stage - 1) % 4 == 0)
            std::cout << "    ";
        std::cout << "0x" << std::hex << std::setw(16)
                  << std::setfill('0') << stageLayoutSignature(map) << "ULL"
                  << std::dec;
        if (stage != kStageCount)
            std::cout << ',';
        if (stage == kStageCount || stage % 4 == 0)
            std::cout << '\n';
        else
            std::cout << ' ';
    }
    std::cout.flags(originalFlags);
    std::cout.fill(originalFill);
    return 0;
}

int runAssetsAndAudioSelfTests(const fs::path &resourceRoot)
{
    const fs::path probeModel =
        TankAssets::defaultProbeModelPath(resourceRoot);
    std::error_code probeError;
    const std::uintmax_t probeBytes = probeModel.empty()
                                              ? 0U
                                              : fs::file_size(probeModel,
                                                              probeError);
    if (!checkTest(!probeModel.empty() && !probeError &&
                       probeModel.extension() == ".glb" &&
                       probeBytes > 500000U,
                   "isolated GLB QA asset is missing or truncated"))
        return 1;
    bool completeOriginalAudioBank = true;
    for (const char *file : kAudioCueFiles)
    {
        std::error_code audioError;
        const fs::path path = resourceRoot / "sounds" / file;
        completeOriginalAudioBank = completeOriginalAudioBank &&
            fs::is_regular_file(path, audioError) && !audioError &&
            fs::file_size(path, audioError) > 100U && !audioError;
    }
    if (!checkTest(completeOriginalAudioBank &&
                       kAudioCueCount == 22U &&
                       kAudioCueFiles[static_cast<std::size_t>(
                           AudioCue::HighScoreBeaten)] ==
                           std::string("highscore_beaten.ogg") &&
                       audioCueHasHighestPriority(AudioCue::StageStart) &&
                       audioCueHasHighestPriority(AudioCue::PlayerRespawn) &&
                       audioCueHasHighestPriority(
                           AudioCue::HighScoreBeaten) &&
                       !audioCueHasHighestPriority(AudioCue::GameOver) &&
                       !kAudioMultiInstance[static_cast<std::size_t>(
                           AudioCue::PlayerIdle)] &&
                       kAudioMultiInstance[static_cast<std::size_t>(
                           AudioCue::PlayerFired)] &&
                       std::fabs(kAudioOverlapFactors[
                           static_cast<std::size_t>(AudioCue::BrickHit)] -
                                 0.70f) < 0.0001f,
                   "2D audio bank or playback metadata drifted"))
        return 1;
    return 0;
}

int runAudioOutputBoundarySelfTests(const fs::path &resourceRoot)
{
    RecordingAudioOutput audio;
    Game3D game(resourceRoot, 0xa0d10b00U, &audio);
    const std::array<Nation, 2> nations{{
        Nation::UnitedStates, Nation::SovietUnion}};
    const bool silentBeforeStart = audio.calls.empty();
    std::vector<bool> stageStartCommitted;
    audio.onCall = [&game, &stageStartCommitted](
                       const AudioOutputCall &call) {
        if (call.type != AudioOutputCallType::Play ||
            call.cue != AudioCue::StageStart)
        {
            return;
        }
        stageStartCommitted.push_back(
            game.stage() == 1 && game.map().stage() == 1 &&
            game.stageIntro() && game.players().size() == 2U &&
            game.players()[0].creationTimer > 0.0f &&
            game.players()[1].creationTimer > 0.0f);
    };
    if (!game.start(2, 3, 1, nations))
        return 1;
    if (!checkTest(
            silentBeforeStart &&
                stageStartCommitted == std::vector<bool>({true}) &&
                audio.calls == std::vector<AudioOutputCall>({
                    playOutputCall(AudioCue::StageStart)}) &&
                game.stage() == 1 && game.map().stage() == 1 &&
                game.stageIntro() && game.players().size() == 2U &&
                game.players()[0].creationTimer > 0.0f &&
                game.players()[1].creationTimer > 0.0f,
            "stage entry did not delegate one post-commit StageStart cue"))
        return 1;

    if (!Game3DTestAccess::preparePlayerInputScenario(game))
        return 1;
    audio.clear();
    std::vector<float> creationTimersAtEngineRequest;
    audio.onCall = [&game, &creationTimersAtEngineRequest](
                       const AudioOutputCall &call) {
        if (call.type == AudioOutputCallType::UpdateEngine)
            creationTimersAtEngineRequest.push_back(
                game.players()[1].creationTimer);
    };
    Game3DTestAccess::setPlayerCreationTimer(game, 1, 0.45f);
    PlayerInputFrame movingInput;
    movingInput.players[0].north.held = true;
    game.update(0.05f, movingInput);
    Game3DTestAccess::setPlayerCreationTimer(game, 1, 0.0f);
    game.update(0.05f, movingInput);
    if (!checkTest(
            audio.calls == std::vector<AudioOutputCall>({
                engineOutputCall(false, true),
                engineOutputCall(true, true)}) &&
                creationTimersAtEngineRequest.size() == 2U &&
                creationTimersAtEngineRequest[0] > 0.0f &&
                creationTimersAtEngineRequest[1] <= 0.0f &&
                game.players()[0].moving &&
                game.players()[1].creationTimer <= 0.0f,
            "engine output did not preserve creating-player mute and ready movement"))
        return 1;

    audio.clear();
    std::vector<bool> pausedAtOutputCall;
    std::vector<AudioBoundaryTraceStep> pauseAudioTrace;
    audio.onCall = [&game, &pausedAtOutputCall, &pauseAudioTrace](
                       const AudioOutputCall &call) {
        pausedAtOutputCall.push_back(game.paused());
        switch (call.type)
        {
        case AudioOutputCallType::StopAll:
            pauseAudioTrace.push_back(
                AudioBoundaryTraceStep::StopAllOutput);
            break;
        case AudioOutputCallType::Play:
            pauseAudioTrace.push_back(AudioBoundaryTraceStep::PlayOutput);
            break;
        case AudioOutputCallType::UpdateEngine:
            pauseAudioTrace.push_back(AudioBoundaryTraceStep::EngineOutput);
            break;
        }
    };
    std::vector<AudioCue> semanticRequests;
    Game3DTestAccess::captureAudioRequests(
        game, semanticRequests,
        [&pauseAudioTrace](AudioCue) {
            pauseAudioTrace.push_back(
                AudioBoundaryTraceStep::SemanticObserver);
        });
    game.togglePause();
    game.update(0.05f, {});
    game.togglePause();
    game.update(0.0f, {});
    if (!checkTest(
            !game.paused() &&
                pausedAtOutputCall ==
                    std::vector<bool>({true, true, true, false}) &&
                semanticRequests ==
                    std::vector<AudioCue>({AudioCue::Pause}) &&
                pauseAudioTrace ==
                    std::vector<AudioBoundaryTraceStep>({
                        AudioBoundaryTraceStep::StopAllOutput,
                        AudioBoundaryTraceStep::PlayOutput,
                        AudioBoundaryTraceStep::SemanticObserver,
                        AudioBoundaryTraceStep::EngineOutput,
                        AudioBoundaryTraceStep::EngineOutput}) &&
                audio.calls == std::vector<AudioOutputCall>({
                    stopOutputCall(),
                    playOutputCall(AudioCue::Pause),
                    engineOutputCall(false, false),
                    engineOutputCall(true, false)}),
            "pause/resume audio output order or engine state drifted"))
        return 1;
    return 0;
}

int runNationalVisualContractSelfTests(const fs::path &resourceRoot)
{
    std::string error;
    if (!checkTest(
            governmentBaseThemeForNation(Nation::UnitedStates) ==
                    GovernmentBaseTheme::UnitedStatesPentagon &&
                governmentBaseThemeForNation(Nation::SovietUnion) ==
                    GovernmentBaseTheme::SovietRingCastle &&
                governmentBaseThemeForNation(Nation::Germany) ==
                    GovernmentBaseTheme::GermanParliament &&
                governmentBaseThemeForNation(Nation::Count) ==
                    GovernmentBaseTheme::UnitedStatesPentagon,
            "one of the three national base themes is mapped incorrectly"))
        return 1;
    const auto americanHeavy = wwii_tank_model::detail::arcadeVehicleSpec(
        wwii_tank_model::Vehicle::M26Pershing);
    const auto sovietHeavy = wwii_tank_model::detail::arcadeVehicleSpec(
        wwii_tank_model::Vehicle::IS2);
    const auto germanHeavy = wwii_tank_model::detail::arcadeVehicleSpec(
        wwii_tank_model::Vehicle::TigerIE);
    const auto americanSuper = wwii_tank_model::detail::arcadeVehicleSpec(
        wwii_tank_model::Vehicle::T28T95);
    const auto sovietSuper = wwii_tank_model::detail::arcadeVehicleSpec(
        wwii_tank_model::Vehicle::KV5Project);
    const auto germanSuper = wwii_tank_model::detail::arcadeVehicleSpec(
        wwii_tank_model::Vehicle::Maus);
    const Color identityPaint{255, 219, 78, 255};
    const Color americanPaint =
        wwii_tank_model::detail::nationalPlayerPaint(
            identityPaint, Nation::UnitedStates);
    const Color sovietPaint =
        wwii_tank_model::detail::nationalPlayerPaint(
            identityPaint, Nation::SovietUnion);
    const Color germanPaint =
        wwii_tank_model::detail::nationalPlayerPaint(
            identityPaint, Nation::Germany);
    const auto differentRgb = [](Color first, Color second) {
        return first.r != second.r || first.g != second.g ||
               first.b != second.b;
    };
    if (!checkTest(
            americanHeavy.nationStyle ==
                    wwii_tank_model::detail::ArcadeNationStyle::American &&
                sovietHeavy.nationStyle ==
                    wwii_tank_model::detail::ArcadeNationStyle::Soviet &&
                germanHeavy.nationStyle ==
                    wwii_tank_model::detail::ArcadeNationStyle::German &&
                americanHeavy.headShape ==
                    wwii_tank_model::detail::ArcadeHeadShape::Cast &&
                sovietHeavy.headShape ==
                    wwii_tank_model::detail::ArcadeHeadShape::Teardrop &&
                germanHeavy.headShape ==
                    wwii_tank_model::detail::ArcadeHeadShape::Angular &&
                americanSuper.headShape ==
                    wwii_tank_model::detail::ArcadeHeadShape::Casemate &&
                sovietSuper.auxiliaryTurret &&
                !germanSuper.auxiliaryTurret && germanSuper.skirts &&
                differentRgb(americanPaint, sovietPaint) &&
                differentRgb(americanPaint, germanPaint) &&
                differentRgb(sovietPaint, germanPaint),
            "national tank silhouettes or field paints collapsed together"))
        return 1;
    StageMap nationThemeMap;
    nationThemeMap.setGovernmentNation(Nation::SovietUnion);
    if (!checkTest(nationThemeMap.load(resourceRoot, 1, error) &&
                       nationThemeMap.governmentNation() ==
                           Nation::SovietUnion &&
                       nationThemeMap.governmentBaseTheme() ==
                           GovernmentBaseTheme::SovietRingCastle,
                   "stage loading discarded the selected USSR base"))
        return 1;
    nationThemeMap.setGovernmentNation(Nation::Germany);
    if (!checkTest(nationThemeMap.governmentBaseTheme() ==
                           GovernmentBaseTheme::GermanParliament,
                   "Germany did not select the parliament base"))
        return 1;
    nationThemeMap.setGovernmentNation(Nation::Count);
    if (!checkTest(nationThemeMap.governmentNation() ==
                           Nation::UnitedStates,
                   "invalid national base selection did not normalize to USA"))
        return 1;
    return 0;
}

int runStageAndEnvironmentSelfTests(const fs::path &resourceRoot)
{
    std::string error;
    std::array<int, 3> urbanKindCounts{};
    std::array<std::array<bool, 3>, 3> urbanHeightTiers{};
    int urbanBrickCells = 0;
    int forestCells = 0;
    int forestSaplingCells = 0;
    std::array<std::uint64_t, kStageCount> generatedStageSignatures{};
    const auto sameVector = [](Vector3 first, Vector3 second) {
        return std::fabs(first.x - second.x) < 0.00001f &&
               std::fabs(first.y - second.y) < 0.00001f &&
               std::fabs(first.z - second.z) < 0.00001f;
    };
    for (int stage = 1; stage <= kStageCount; ++stage)
    {
        StageMap map;
        if (!checkTest(map.load(resourceRoot, stage, error), error))
            return 1;
        if (!checkTest(map.stage() == stage, "wrong normalized stage"))
            return 1;
        StageMap repeatedGeneratedMap;
        if (!checkTest(repeatedGeneratedMap.load(resourceRoot, stage, error),
                       error))
            return 1;
        bool deterministicLayout = true;
        for (int row = 0; row < kMapSize; ++row)
        {
            for (int column = 0; column < kMapSize; ++column)
            {
                const char value = map.tile(row, column);
                deterministicLayout = deterministicLayout &&
                    value == repeatedGeneratedMap.tile(row, column) &&
                    map.brickMask(row, column) ==
                        repeatedGeneratedMap.brickMask(row, column);
            }
        }
        const std::uint64_t signature = stageLayoutSignature(map);
        const std::uint64_t expectedSignature =
            kExpectedStageLayoutSignatures[
                static_cast<std::size_t>(stage - 1)];
        std::ostringstream signatureMessage;
        signatureMessage << "stage " << stage
                         << " layout signature drifted: expected 0x"
                         << std::hex << expectedSignature << ", got 0x"
                         << signature;
        if (!checkTest(signature == expectedSignature,
                       signatureMessage.str()))
            return 1;
        bool uniqueLayout = true;
        for (int earlierStage = 0; earlierStage < stage - 1; ++earlierStage)
            uniqueLayout = uniqueLayout &&
                generatedStageSignatures[
                    static_cast<std::size_t>(earlierStage)] != signature;
        generatedStageSignatures[static_cast<std::size_t>(stage - 1)] =
            signature;

        bool spawnRoutesOpen =
            map.hasTankRoute(kPlayerSpawnPoints[0], kPlayerSpawnPoints[1]);
        if (stage != 1)
            spawnRoutesOpen = spawnRoutesOpen &&
                map.hasTankRoute(kEnemySpawnPoints[1], {13.0f, 20.0f});
        for (XZ enemySpawn : kEnemySpawnPoints)
            for (XZ playerStart : kPlayerSpawnPoints)
                spawnRoutesOpen = spawnRoutesOpen &&
                    map.hasTankRoute(enemySpawn, playerStart);
        if (!checkTest(deterministicLayout && uniqueLayout && spawnRoutesOpen,
                       "stage generation is unstable, duplicated, or disconnected"))
            return 1;
        for (int row = 21; row <= 25; ++row)
        {
            for (int column = 10; column <= 15; ++column)
            {
                if (!checkTest(map.tile(row, column) == '.' &&
                                   map.brickMask(row, column) == 0U,
                               "expanded base footprint retained a legacy defensive tile"))
                    return 1;
            }
        }
        for (int index = 0; index < kGovernmentWallCount; ++index)
            if (!checkTest(map.governmentWallHealth(index) ==
                               kGovernmentWallMaximumHealth,
                           "Pentagon wall did not start at full health"))
                return 1;

        for (int row = 0; row < kMapSize; ++row)
        {
            for (int column = 0; column < kMapSize; ++column)
            {
                if (map.tile(row, column) == '%')
                {
                    ++forestCells;
                    const unsigned char edges = forestEdgeMask(map, row, column);
                    const auto plan = EnvironmentAssets::forestPlan(
                        stage, row, column, edges);
                    const auto repeated = EnvironmentAssets::forestPlan(
                        stage, row, column, edges);
                    const auto enclosed = EnvironmentAssets::forestPlan(
                        stage, row, column, 0U);
                    bool stable = plan.seed == repeated.seed &&
                                  plan.treeCount == repeated.treeCount &&
                                  plan.edgeMask == edges &&
                                  plan.treeCount >= 2 && plan.treeCount <= 3 &&
                                  plan.topHeight <=
                                      EnvironmentAssets::kForestCanopyHeightCap + 0.0001f;
                    for (int index = 0; index < plan.treeCount; ++index)
                    {
                        const auto &tree = plan.trees[static_cast<std::size_t>(index)];
                        const auto &again = repeated.trees[static_cast<std::size_t>(index)];
                        const float extentX = tree.crownRadius *
                                              tree.crownScaleX * 1.04f;
                        const float extentZ = tree.crownRadius *
                                              tree.crownScaleZ * 1.04f;
                        stable = stable && tree.seed == again.seed &&
                                 sameVector(tree.trunkBase, again.trunkBase) &&
                                 sameVector(tree.trunkTop, again.trunkTop) &&
                                 tree.crownRadius <=
                                     EnvironmentAssets::kForestCanopyRadiusCap + 0.0001f &&
                                 tree.crownTopHeight <=
                                     EnvironmentAssets::kForestCanopyHeightCap + 0.0001f &&
                                 tree.trunkTop.x - extentX >=
                                     column - EnvironmentAssets::kForestTileOverhangCap - 0.0001f &&
                                 tree.trunkTop.x + extentX <=
                                     column + 1.0f + EnvironmentAssets::kForestTileOverhangCap + 0.0001f &&
                                 tree.trunkTop.z - extentZ >=
                                     row - EnvironmentAssets::kForestTileOverhangCap - 0.0001f &&
                                 tree.trunkTop.z + extentZ <=
                                     row + 1.0f + EnvironmentAssets::kForestTileOverhangCap + 0.0001f;
                        if (index < 2)
                        {
                            stable = stable &&
                                tree.crownRadius >= (index == 0 ? 0.336f
                                                                : 0.303f) &&
                                tree.crownBaseHeight <= 0.50f;
                        }
                    }
                    for (int index = 0; index < 2; ++index)
                    {
                        const auto &edgeTree = plan.trees[static_cast<std::size_t>(index)];
                        const auto &coreTree = enclosed.trees[static_cast<std::size_t>(index)];
                        stable = stable && edgeTree.seed == coreTree.seed &&
                                 sameVector(edgeTree.trunkBase, coreTree.trunkBase) &&
                                 sameVector(edgeTree.trunkTop, coreTree.trunkTop);
                    }
                    if (!checkTest(stable && map.tile(row, column) == '%',
                                   "forest plan is unstable, over-sized, or mutates gameplay"))
                        return 1;
                    if (plan.treeCount == 3)
                        ++forestSaplingCells;
                    continue;
                }
                if (map.tile(row, column) != '#')
                    continue;
                ++urbanBrickCells;
                const char tileBefore = map.tile(row, column);
                const unsigned char maskBefore = map.brickMask(row, column);
                const auto profile = EnvironmentAssets::urbanProfile(
                    stage, row, column);
                const auto repeated = EnvironmentAssets::urbanProfile(
                    stage, row, column);
                const int kind = static_cast<int>(profile.kind);
                if (!checkTest(kind >= 0 && kind < 3 &&
                                   profile.heightTier < 3U &&
                                   profile.palette < 3U &&
                                   profile.totalHeight <=
                                       EnvironmentAssets::kUrbanTotalHeightCap + 0.0001f &&
                                   profile.totalHeight > profile.coreHeight &&
                                   repeated.kind == profile.kind &&
                                   repeated.heightTier == profile.heightTier &&
                                   repeated.palette == profile.palette &&
                                   repeated.roofVariant == profile.roofVariant &&
                                   repeated.floors == profile.floors &&
                                   repeated.seed == profile.seed &&
                                   std::fabs(repeated.coreHeight - profile.coreHeight) < 0.0001f &&
                                   std::fabs(repeated.totalHeight - profile.totalHeight) < 0.0001f,
                               "urban building profile is unstable or exceeds its height cap"))
                    return 1;

                const auto fullPlan = EnvironmentAssets::urbanMassPlan(
                    profile, row, column, 0x0fU);
                const auto &bodyMass = fullPlan.masses[0];
                const auto &capMass = fullPlan.masses[1];
                const auto &roofMass = fullPlan.masses[2];
                const float bodyTop = bodyMass.center.y + bodyMass.size.y * 0.5f;
                const float capBottom = capMass.center.y - capMass.size.y * 0.5f;
                const float capTop = capMass.center.y + capMass.size.y * 0.5f;
                const float roofBottom = roofMass.center.y - roofMass.size.y * 0.5f;
                const float roofTop = roofMass.center.y + roofMass.size.y * 0.5f;
                if (!checkTest(fullPlan.count == 3 &&
                                   std::fabs(fullPlan.topHeight -
                                             profile.totalHeight) < 0.0001f &&
                                   bodyMass.size.x <= 1.0f &&
                                   bodyMass.size.z <= 1.0f &&
                                   std::fabs(bodyTop - capBottom) < 0.0001f &&
                                   roofBottom - capTop >= 0.0079f &&
                                   std::fabs(roofTop - profile.totalHeight) < 0.0001f &&
                                   map.tile(row, column) == tileBefore &&
                                   map.brickMask(row, column) == maskBefore,
                               "urban roof masses overlap, exceed their tile, or mutate gameplay"))
                    return 1;
                ++urbanKindCounts[static_cast<std::size_t>(kind)];
                urbanHeightTiers[static_cast<std::size_t>(kind)]
                                [profile.heightTier] = true;
            }
        }
    }

    bool completeUrbanVariation = urbanBrickCells > 1000;
    for (int kind = 0; kind < 3; ++kind)
    {
        completeUrbanVariation = completeUrbanVariation &&
                                 urbanKindCounts[static_cast<std::size_t>(kind)] > 0;
        for (bool tierPresent : urbanHeightTiers[static_cast<std::size_t>(kind)])
            completeUrbanVariation = completeUrbanVariation && tierPresent;
    }
    if (!checkTest(completeUrbanVariation,
                   "urban generator did not produce all building kinds and height tiers"))
        return 1;
    if (!checkTest(forestCells > 1000 && forestSaplingCells > 0 &&
                       EnvironmentAssets::kForestCanopyAlphaMinimum == 104U &&
                       EnvironmentAssets::kForestCanopyAlphaMaximum == 132U &&
                       EnvironmentAssets::kForestCanopyAlphaMinimum >= 96U &&
                       EnvironmentAssets::kForestCanopyAlphaMaximum <= 144U,
                   "forest generator lacks coverage, edge saplings, or readable alpha bounds"))
        return 1;

    const auto maskProfile = EnvironmentAssets::urbanProfile(1, 2, 2);
    for (unsigned int mask = 0U; mask <= 0x0fU; ++mask)
    {
        const auto plan = EnvironmentAssets::urbanMassPlan(
            maskProfile, 2, 2, static_cast<unsigned char>(mask));
        int survivingQuadrants = 0;
        for (int quadrant = 0; quadrant < 4; ++quadrant)
            survivingQuadrants += (mask & (1U << quadrant)) != 0U ? 1 : 0;
        const int expectedMasses = mask == 0x0fU ? 3 : survivingQuadrants;
        bool bounded = plan.count == expectedMasses &&
                       plan.topHeight <= EnvironmentAssets::kUrbanTotalHeightCap + 0.0001f;
        if (mask != 0x0fU)
        {
            for (int index = 0; index < plan.count; ++index)
            {
                const auto &mass = plan.masses[static_cast<std::size_t>(index)];
                bounded = bounded && mass.size.x <= 0.44f + 0.0001f &&
                          mass.size.z <= 0.44f + 0.0001f &&
                          mass.center.x >= 2.0f && mass.center.x <= 3.0f &&
                          mass.center.z >= 2.0f && mass.center.z <= 3.0f;
            }
        }
        if (!checkTest(bounded,
                       "urban damage mask produced floating or out-of-tile geometry"))
            return 1;
    }

    return 0;
}

int runTerrainBaseAndBrickSelfTests(const fs::path &resourceRoot)
{
    std::string error;
    StageMap forestRuleMap;
    if (!forestRuleMap.load(resourceRoot, 10, error))
        return 1;
    std::array<int, 2> forestCell{{-1, -1}};
    for (int row = 0; row < kMapSize && forestCell[0] < 0; ++row)
        for (int column = 0; column < kMapSize; ++column)
            if (forestRuleMap.tile(row, column) == '%')
            {
                forestCell = {{row, column}};
                break;
            }
    if (!checkTest(forestCell[0] >= 0, "stage 10 lacks a forest test tile"))
        return 1;
    int forestTileCount = 0;
    for (int row = 0; row < kMapSize; ++row)
        for (int column = 0; column < kMapSize; ++column)
            forestTileCount += forestRuleMap.tile(row, column) == '%' ? 1 : 0;
    bool forestOrdersAreStable = forestTileCount > 0;
    for (const int angle : std::array<int, 3>{{-45, 0, 45}})
    {
        const std::vector<ForestDrawCell> order =
            forestDrawOrder(forestRuleMap, angle);
        forestOrdersAreStable = forestOrdersAreStable &&
            static_cast<int>(order.size()) == forestTileCount;
        for (std::size_t index = 1; index < order.size(); ++index)
        {
            const ForestDrawCell &previous = order[index - 1];
            const ForestDrawCell &current = order[index];
            const bool monotonic = previous.depth <= current.depth + 0.00001f;
            const bool stableTie =
                previous.depth != current.depth ||
                previous.row * kMapSize + previous.column <
                    current.row * kMapSize + current.column;
            forestOrdersAreStable = forestOrdersAreStable && monotonic &&
                                    stableTie;
        }
    }
    if (!checkTest(forestOrdersAreStable,
                   "forest alpha draw order is not stable far-to-near at "
                   "the selectable camera endpoints"))
        return 1;
    const XZ forestCenter{forestCell[1] + 0.5f, forestCell[0] + 0.5f};
    const ImpactKind normalForestImpact = forestRuleMap.impactShell(
        forestCenter, false, CardinalDirection::North);
    const bool normalForestSurvived =
        forestRuleMap.tile(forestCell[0], forestCell[1]) == '%';
    const ImpactKind powerForestImpact = forestRuleMap.impactShell(
        forestCenter, true, CardinalDirection::North);
    if (!checkTest(normalForestImpact == ImpactKind::None &&
                       normalForestSurvived &&
                       powerForestImpact == ImpactKind::None &&
                       forestRuleMap.tile(forestCell[0], forestCell[1]) == '.',
                   "forest no longer follows the 2D normal/power-shell cover rule"))
        return 1;

    StageMap governmentMap;
    if (!governmentMap.load(resourceRoot, 1, error))
        return 1;
    const GovernmentWallSegment testedWall = governmentWallSegment(0);
    bool governmentGeometryInsideMap = true;
    for (int index = 0; index < kGovernmentWallCount; ++index)
    {
        const GovernmentWallSegment segment = governmentWallSegment(index);
        const float roofHalfThickness = segment.halfThickness + 0.035f;
        for (float alongSign : {-1.0f, 1.0f})
            for (float acrossSign : {-1.0f, 1.0f})
            {
                const XZ corner = segment.center +
                    segment.along * (alongSign * segment.halfLength) +
                    segment.outward * (acrossSign * roofHalfThickness);
                governmentGeometryInsideMap = governmentGeometryInsideMap &&
                    corner.x >= 0.0f && corner.x <= kMapSize &&
                    corner.z >= 0.0f && corner.z <= kMapSize;
            }
        const float angle = kGovernmentPentagonYaw +
                            static_cast<float>(index) * 2.0f * kPi /
                                static_cast<float>(kGovernmentWallCount);
        const XZ foundationCorner{
            kGovernmentBaseCenter.x + std::sin(angle) *
                kGovernmentFoundationRadius,
            kGovernmentBaseCenter.z + std::cos(angle) *
                kGovernmentFoundationRadius};
        governmentGeometryInsideMap = governmentGeometryInsideMap &&
            foundationCorner.x >= 0.0f && foundationCorner.x <= kMapSize &&
            foundationCorner.z >= 0.0f && foundationCorner.z <= kMapSize;
    }
    if (!checkTest(std::fabs(kGovernmentBaseCenter.z - 23.60f) < 0.0001f &&
                       std::fabs(kGovernmentWallRadius - 1.95f) < 0.0001f &&
                       std::fabs(kGovernmentWallThickness - 1.16f) < 0.0001f &&
                       std::fabs(kGovernmentFoundationRadius - 2.62f) < 0.0001f &&
                       std::fabs(kGovernmentCourtyardRadius - 1.02f) < 0.0001f &&
                       tanks3d::base_model::kCommandCoreFootprint <=
                           kGovernmentCoreRadius * 2.0f &&
                       std::fabs(kGovernmentCoreRadius - 0.92f) < 0.0001f &&
                       std::fabs(testedWall.halfLength -
                                 (testedWall.length * 0.5f +
                                  kGovernmentWallEndOverlap)) < 0.0001f &&
                       governmentGeometryInsideMap &&
                       governmentMap.wallOccupies(testedWall.center) &&
                       governmentMap.collidesWithTank(testedWall.center, 0.05f) &&
                       !governmentMap.collidesWithTank(playerSpawn(0), kTankRadius) &&
                       !governmentMap.collidesWithTank(playerSpawn(1), kTankRadius),
                   "expanded Pentagon proportions or shared collision geometry drifted"))
        return 1;

    for (int hit = 0; hit < kGovernmentWallMaximumHealth; ++hit)
    {
        ShellImpactDetails details;
        const ImpactKind impact = governmentMap.impactShell(
            testedWall.center, false, CardinalDirection::North, &details);
        const int expectedBefore = kGovernmentWallMaximumHealth - hit;
        const int expectedAfter = expectedBefore - 1;
        if (!checkTest(impact == ImpactKind::GovernmentWall &&
                           details.governmentWallIndex == 0 &&
                           details.governmentWallHealthBefore == expectedBefore &&
                           details.governmentWallHealthAfter == expectedAfter &&
                           details.destroyedGovernmentWall() ==
                               (expectedAfter == 0),
                       "normal shells did not reduce one Pentagon wall by one HP"))
            return 1;
    }
    if (!checkTest(governmentMap.governmentWallHealth(0) == 0 &&
                       !governmentMap.wallOccupies(testedWall.center) &&
                       !governmentMap.collidesWithTank(testedWall.center, 0.05f),
                   "destroyed Pentagon wall did not open a real breach"))
        return 1;

    governmentMap.repairGovernmentWalls();
    governmentMap.repairGovernmentWalls();
    bool repairedGovernment = true;
    for (int index = 0; index < kGovernmentWallCount; ++index)
        repairedGovernment = repairedGovernment &&
                             governmentMap.governmentWallHealth(index) ==
                                 kGovernmentWallMaximumHealth;
    if (!checkTest(repairedGovernment,
                   "shovel-style Pentagon wall repair is not idempotent"))
        return 1;

    StageMap steelGovernmentMap;
    if (!steelGovernmentMap.load(resourceRoot, 1, error))
        return 1;
    for (int hit = 0; hit < kGovernmentWallMaximumHealth; ++hit)
        steelGovernmentMap.impactShell(testedWall.center, false,
                                       CardinalDirection::North);
    if (!checkTest(steelGovernmentMap.governmentWallHealth(0) == 0,
                   "steel protection setup did not create a Pentagon breach"))
        return 1;
    steelGovernmentMap.activateGovernmentSteel();
    bool steelRepairedEveryWing = true;
    for (int index = 0; index < kGovernmentWallCount; ++index)
        steelRepairedEveryWing = steelRepairedEveryWing &&
            steelGovernmentMap.governmentWallHealth(index) ==
                kGovernmentWallMaximumHealth;
    ShellImpactDetails protectedNormalDetails;
    ShellImpactDetails protectedPowerDetails;
    const ImpactKind protectedNormalImpact = steelGovernmentMap.impactShell(
        testedWall.center, false, CardinalDirection::North,
        &protectedNormalDetails);
    const ImpactKind protectedPowerImpact = steelGovernmentMap.impactShell(
        testedWall.center, true, CardinalDirection::South,
        &protectedPowerDetails);
    if (!checkTest(steelRepairedEveryWing &&
                       steelGovernmentMap.governmentWallsSteel() &&
                       steelGovernmentMap.governmentSteelVisible() &&
                       std::fabs(
                           steelGovernmentMap.governmentSteelTimeRemaining() -
                           kGovernmentSteelDuration) < 0.0001f &&
                       protectedNormalImpact == ImpactKind::Steel &&
                       protectedPowerImpact == ImpactKind::Steel &&
                       protectedNormalDetails.governmentWallHealthBefore == 4 &&
                       protectedNormalDetails.governmentWallHealthAfter == 4 &&
                       protectedPowerDetails.governmentWallHealthBefore == 4 &&
                       protectedPowerDetails.governmentWallHealthAfter == 4 &&
                       steelGovernmentMap.governmentWallHealth(0) == 4,
                   "shovel did not repair and protect every Pentagon wing"))
        return 1;

    steelGovernmentMap.updateGovernmentProtection(17.10f);
    const bool firstWarningFrame =
        steelGovernmentMap.governmentSteelVisible();
    const ImpactKind warningImpact = steelGovernmentMap.impactShell(
        testedWall.center, true, CardinalDirection::North);
    steelGovernmentMap.updateGovernmentProtection(kGovernmentSteelFlashPeriod);
    const bool secondWarningFrame =
        steelGovernmentMap.governmentSteelVisible();
    if (!checkTest(firstWarningFrame != secondWarningFrame &&
                       warningImpact == ImpactKind::Steel &&
                       steelGovernmentMap.governmentWallHealth(0) == 4,
                   "final shovel warning did not flash while remaining protective"))
        return 1;

    steelGovernmentMap.activateGovernmentSteel();
    steelGovernmentMap.updateGovernmentProtection(15.0f);
    steelGovernmentMap.activateGovernmentSteel();
    if (!checkTest(std::fabs(
                           steelGovernmentMap.governmentSteelTimeRemaining() -
                           kGovernmentSteelDuration) < 0.0001f,
                   "repeated shovel pickup did not reset the steel timer"))
        return 1;
    steelGovernmentMap.updateGovernmentProtection(kGovernmentSteelDuration +
                                                    0.01f);
    const ImpactKind expiredImpact = steelGovernmentMap.impactShell(
        testedWall.center, false, CardinalDirection::North);
    if (!checkTest(!steelGovernmentMap.governmentWallsSteel() &&
                       !steelGovernmentMap.governmentSteelVisible() &&
                       steelGovernmentMap.governmentSteelTimeRemaining() == 0.0f &&
                       expiredImpact == ImpactKind::GovernmentWall &&
                       steelGovernmentMap.governmentWallHealth(0) == 3,
                   "expired shovel protection did not restore ordinary wall damage"))
        return 1;
    steelGovernmentMap.activateGovernmentSteel();
    if (!steelGovernmentMap.load(resourceRoot, 1, error))
        return 1;
    if (!checkTest(!steelGovernmentMap.governmentWallsSteel() &&
                       steelGovernmentMap.governmentWallHealth(0) ==
                           kGovernmentWallMaximumHealth,
                   "loading a new stage carried shovel steel across stages"))
        return 1;

    StageMap powerGovernmentMap;
    if (!powerGovernmentMap.load(resourceRoot, 1, error))
        return 1;
    ShellImpactDetails firstPowerDetails;
    ShellImpactDetails secondPowerDetails;
    const ImpactKind firstPowerImpact = powerGovernmentMap.impactShell(
        testedWall.center, true, CardinalDirection::South,
        &firstPowerDetails);
    const ImpactKind secondPowerImpact = powerGovernmentMap.impactShell(
        testedWall.center, true, CardinalDirection::South,
        &secondPowerDetails);
    if (!checkTest(firstPowerImpact == ImpactKind::GovernmentWall &&
                       secondPowerImpact == ImpactKind::GovernmentWall &&
                       firstPowerDetails.governmentWallHealthBefore == 4 &&
                       firstPowerDetails.governmentWallHealthAfter == 2 &&
                       secondPowerDetails.governmentWallHealthBefore == 2 &&
                       secondPowerDetails.governmentWallHealthAfter == 0 &&
                       secondPowerDetails.destroyedGovernmentWall(),
                   "power shells did not reduce Pentagon walls by two HP"))
        return 1;

    const float shellProjection = kShellHalfSize *
        (std::fabs(testedWall.outward.x) +
         std::fabs(testedWall.outward.z));
    const float shellContactDistance = testedWall.halfThickness +
                                       shellProjection;
    const float tankProjection = 0.10f *
        (std::fabs(testedWall.outward.x) +
         std::fabs(testedWall.outward.z));
    if (!checkTest(governmentWallOverlapsShell(
                           testedWall,
                           testedWall.center + testedWall.outward *
                               (shellContactDistance - 0.002f)) &&
                       !governmentWallOverlapsShell(
                           testedWall,
                           testedWall.center + testedWall.outward *
                               (shellContactDistance + 0.002f)) &&
                       governmentWallOverlapsAabb(
                           testedWall,
                           testedWall.center + testedWall.outward *
                               (testedWall.halfThickness + tankProjection -
                                0.002f),
                           0.10f) &&
                       !governmentWallOverlapsAabb(
                           testedWall,
                           testedWall.center + testedWall.outward *
                               (testedWall.halfThickness + tankProjection +
                                0.002f),
                           0.10f),
                   "Pentagon OBB edge contact rules drifted"))
        return 1;

    StageMap cornerGovernmentMap;
    if (!cornerGovernmentMap.load(resourceRoot, 1, error))
        return 1;
    const ImpactKind cornerImpact = cornerGovernmentMap.impactShell(
        governmentPentagonCorner(0), false, CardinalDirection::North);
    int remainingGovernmentHealth = 0;
    for (int index = 0; index < kGovernmentWallCount; ++index)
        remainingGovernmentHealth +=
            cornerGovernmentMap.governmentWallHealth(index);
    if (!checkTest(cornerImpact == ImpactKind::GovernmentWall &&
                       remainingGovernmentHealth ==
                           kGovernmentWallCount *
                               kGovernmentWallMaximumHealth - 1,
                   "one corner shell damaged more than one Pentagon wall"))
        return 1;

    StageMap breachGovernmentMap;
    if (!breachGovernmentMap.load(resourceRoot, 1, error))
        return 1;
    // Probe just beyond the wall faces while remaining outside the eagle's
    // reinforced circular core hit area.
    const float breachProbeDistance = testedWall.halfThickness + 0.02f;
    const XZ outsideShell = testedWall.center +
                            testedWall.outward * breachProbeDistance;
    const XZ insideShell = testedWall.center -
                           testedWall.outward * breachProbeDistance;
    if (!checkTest(breachGovernmentMap.solidSeparatesShells(
                           outsideShell, insideShell) &&
                       breachGovernmentMap.isInsideBase(kGovernmentBaseCenter) &&
                       breachGovernmentMap.shellHitsGovernmentCore(
                           kGovernmentBaseCenter) &&
                       !breachGovernmentMap.shellHitsGovernmentCore(
                           testedWall.center),
                   "intact Pentagon wall or eagle collision is porous"))
        return 1;
    for (int hit = 0; hit < kGovernmentWallMaximumHealth; ++hit)
        breachGovernmentMap.impactShell(testedWall.center, false,
                                        CardinalDirection::North);
    if (!checkTest(!breachGovernmentMap.solidSeparatesShells(
                           outsideShell, insideShell) &&
                       breachGovernmentMap.impactShell(
                           kGovernmentBaseCenter, false,
                           CardinalDirection::North) == ImpactKind::None &&
                       breachGovernmentMap.shellHitsGovernmentCore(
                           kGovernmentBaseCenter),
                   "destroyed wall did not expose the central eagle"))
        return 1;

    StageMap map;
    if (!map.load(resourceRoot, 1, error))
        return 1;

    bool testedOrdinarySteel = false;
    bool testedBrick = false;
    for (int row = 0; row < kMapSize; ++row)
    {
        for (int column = 0; column < kMapSize; ++column)
        {
            if (!testedOrdinarySteel && map.tile(row, column) == '@')
            {
                map.impactShell({column + 0.5f, row + 0.5f}, true,
                                CardinalDirection::North);
                testedOrdinarySteel = checkTest(map.tile(row, column) == '.', "power shell did not destroy ordinary steel");
            }
            if (!testedBrick && map.tile(row, column) == '#')
            {
                map.impactShell({column + 0.5f, row + 0.5f}, false,
                                CardinalDirection::North);
                const bool halfRemains = map.tile(row, column) == '#' &&
                                         map.brickMask(row, column) == 0x03U;
                map.impactShell({column + 0.25f, row + 0.25f}, false,
                                CardinalDirection::East);
                const bool quarterRemains = map.tile(row, column) == '#' &&
                                            map.brickMask(row, column) == 0x02U;
                map.impactShell({column + 0.75f, row + 0.25f}, false,
                                CardinalDirection::South);
                testedBrick = checkTest(halfRemains && quarterRemains &&
                                            map.tile(row, column) == '.',
                                        "directional half/quarter brick damage is incorrect");
            }
        }
    }
    if (!checkTest(testedOrdinarySteel && testedBrick, "stage 1 lacks expected test tiles"))
        return 1;

    const auto findBrick = [](const StageMap &candidate) {
        std::array<int, 2> cell{{-1, -1}};
        for (int row = 0; row < kMapSize && cell[0] < 0; ++row)
            for (int column = 0; column < kMapSize; ++column)
                if (candidate.tile(row, column) == '#')
                {
                    cell = {{row, column}};
                    break;
                }
        return cell;
    };

    const std::array<CardinalDirection, 4> firstHitDirections{{
        CardinalDirection::North, CardinalDirection::East,
        CardinalDirection::South, CardinalDirection::West}};
    const std::array<unsigned char, 4> firstHitMasks{{0x03U, 0x0aU, 0x0cU, 0x05U}};
    for (std::size_t index = 0; index < firstHitDirections.size(); ++index)
    {
        StageMap directionalMap;
        if (!directionalMap.load(resourceRoot, 1, error))
            return 1;
        const auto cell = findBrick(directionalMap);
        const auto buildingBefore = EnvironmentAssets::urbanProfile(
            directionalMap.stage(), cell[0], cell[1]);
        directionalMap.impactShell({cell[1] + 0.5f, cell[0] + 0.5f}, false,
                                   firstHitDirections[index]);
        const auto buildingAfter = EnvironmentAssets::urbanProfile(
            directionalMap.stage(), cell[0], cell[1]);
        if (!checkTest(cell[0] >= 0 &&
                           directionalMap.brickMask(cell[0], cell[1]) ==
                               firstHitMasks[index] &&
                           buildingBefore.kind == buildingAfter.kind &&
                           buildingBefore.seed == buildingAfter.seed &&
                           std::fabs(buildingBefore.totalHeight -
                                     buildingAfter.totalHeight) < 0.0001f,
                       "brick damage changed its building identity or wrong half"))
            return 1;
    }

    // A tank may enter the half of a brick tile removed by the first shot.
    // The next round must begin at the tank collision edge and hit the far
    // half, even when the barrel is slightly off the brick centerline. The
    // former fixed 1.02-tile spawn skipped beyond this remaining half.
    // +/-0.74 leaves only 0.01 tile of lateral AABB overlap, covering a
    // visually near-grazing barrel without turning zero-area contact into a hit.
    const std::array<float, 5> nearCollinearOffsets{{
        -0.74f, -0.24f, 0.0f, 0.24f, 0.74f}};
    for (CardinalDirection direction : firstHitDirections)
    {
        for (float lateralOffset : nearCollinearOffsets)
        {
            StageMap closeWallMap;
            if (!closeWallMap.load(resourceRoot, 1, error))
                return 1;
            const auto cell = findBrick(closeWallMap);
            if (!checkTest(cell[0] >= 0, "stage 1 lacks a close-wall brick"))
                return 1;

            const XZ axis = cardinalVector(direction);
            const XZ lateral{-axis.z, axis.x};
            const XZ brickCenter{cell[1] + 0.5f, cell[0] + 0.5f};
            ShellImpactDetails partialDetails;
            const ImpactKind partialImpact = closeWallMap.impactShell(
                brickCenter + lateral * lateralOffset, false, direction,
                &partialDetails);
            const XZ tankPosition =
                brickCenter - axis * kTankRadius + lateral * lateralOffset;
            const XZ firstSweepPosition =
                shellSpawnPosition(tankPosition, direction) +
                axis * kShellSweepStep;
            ShellImpactDetails breachDetails;
            const ImpactKind breachImpact = closeWallMap.impactShell(
                firstSweepPosition, false, direction, &breachDetails);
            if (!checkTest(partialImpact == ImpactKind::Brick &&
                               partialDetails.brickCount >= 1 &&
                               !partialDetails.destroyedBrick() &&
                               breachImpact == ImpactKind::Brick &&
                               breachDetails.destroyedBrick() &&
                               closeWallMap.tile(cell[0], cell[1]) == '.',
                           "close or near-collinear barrel skipped the remaining brick half"))
                return 1;
        }
    }

    StageMap powerBrickMap;
    if (!powerBrickMap.load(resourceRoot, 1, error))
        return 1;
    const auto powerBrickCell = findBrick(powerBrickMap);
    ShellImpactDetails powerBrickDetails;
    powerBrickMap.impactShell({powerBrickCell[1] + 0.5f,
                               powerBrickCell[0] + 0.5f},
                              true, CardinalDirection::North,
                              &powerBrickDetails);
    if (!checkTest(powerBrickMap.tile(powerBrickCell[0], powerBrickCell[1]) == '.' &&
                       powerBrickDetails.destroyedBrick(),
                   "maximum-level shell did not report clearing a full brick"))
        return 1;

    BattleFx partialBrickFx;
    partialBrickFx.spawnBrickImpact({2.5f, 0.47f, 2.5f},
                                    {0.0f, 0.28f, 1.0f}, false, false);
    BattleFx destroyedBrickFx;
    destroyedBrickFx.spawnBrickImpact({2.5f, 0.47f, 2.5f},
                                      {0.0f, 0.28f, 1.0f}, false, true);
    BattleFx powerBrickFx;
    powerBrickFx.spawnBrickImpact({2.5f, 0.47f, 2.5f},
                                  {0.0f, 0.28f, 1.0f}, true, true);
    if (!checkTest(destroyedBrickFx.activeCount() >
                           partialBrickFx.activeCount() + 20U &&
                       powerBrickFx.activeCount() >= destroyedBrickFx.activeCount(),
                   "complete brick breach lacks a distinct collapse animation"))
        return 1;
    for (int frame = 0; frame < 45; ++frame)
        destroyedBrickFx.update(0.05f);
    if (!checkTest(destroyedBrickFx.activeCount() == 0U,
                   "brick collapse particles did not expire"))
        return 1;

    const std::array<CardinalDirection, 4> crossingSecondDirections{{
        CardinalDirection::East, CardinalDirection::South,
        CardinalDirection::West, CardinalDirection::North}};
    const std::array<unsigned char, 4> crossingMasks{{0x02U, 0x08U, 0x04U, 0x01U}};
    for (std::size_t index = 0; index < firstHitDirections.size(); ++index)
    {
        StageMap crossingMap;
        if (!crossingMap.load(resourceRoot, 1, error))
            return 1;
        const auto cell = findBrick(crossingMap);
        const XZ center{cell[1] + 0.5f, cell[0] + 0.5f};
        crossingMap.impactShell(center, false, firstHitDirections[index]);
        crossingMap.impactShell(center, false, crossingSecondDirections[index]);
        if (!checkTest(crossingMap.brickMask(cell[0], cell[1]) ==
                           crossingMasks[index],
                       "perpendicular second hit did not leave the correct quarter"))
            return 1;
    }

    StageMap sameAxisMap;
    if (!sameAxisMap.load(resourceRoot, 1, error))
        return 1;
    const auto sameAxisCell = findBrick(sameAxisMap);
    const XZ sameAxisCenter{sameAxisCell[1] + 0.5f, sameAxisCell[0] + 0.5f};
    sameAxisMap.impactShell(sameAxisCenter, false, CardinalDirection::North);
    sameAxisMap.impactShell(sameAxisCenter, false, CardinalDirection::South);
    if (!checkTest(sameAxisMap.tile(sameAxisCell[0], sameAxisCell[1]) == '.',
                   "opposite-axis second hit did not clear a brick"))
        return 1;

    StageMap dualBrickMap;
    if (!dualBrickMap.load(resourceRoot, 1, error))
        return 1;
    std::array<int, 2> dualCell{{-1, -1}};
    for (int row = 0; row < kMapSize && dualCell[0] < 0; ++row)
        for (int column = 0; column + 1 < kMapSize; ++column)
            if (dualBrickMap.tile(row, column) == '#' &&
                dualBrickMap.tile(row, column + 1) == '#')
            {
                dualCell = {{row, column}};
                break;
            }
    if (!checkTest(dualCell[0] >= 0, "stage 1 lacks adjacent brick test tiles"))
        return 1;
    dualBrickMap.impactShell({dualCell[1] + 1.0f, dualCell[0] + 0.5f}, false,
                             CardinalDirection::North);
    if (!checkTest(dualBrickMap.brickMask(dualCell[0], dualCell[1]) == 0x03U &&
                       dualBrickMap.brickMask(dualCell[0], dualCell[1] + 1) == 0x03U,
                   "half-tile shell did not damage two bricks across a seam"))
        return 1;

    StageMap powerDualBrickMap;
    if (!powerDualBrickMap.load(resourceRoot, 1, error))
        return 1;
    powerDualBrickMap.impactShell(
        {dualCell[1] + 1.0f, dualCell[0] + 0.5f}, true,
        CardinalDirection::North);
    if (!checkTest(powerDualBrickMap.tile(dualCell[0], dualCell[1]) == '.' &&
                       powerDualBrickMap.tile(dualCell[0], dualCell[1] + 1) == '.',
                   "maximum-level half-tile shell did not clear both seam bricks"))
        return 1;

    StageMap occupancyMap;
    if (!occupancyMap.load(resourceRoot, 1, error))
        return 1;
    const auto occupancyCell = findBrick(occupancyMap);
    occupancyMap.impactShell({occupancyCell[1] + 0.5f,
                              occupancyCell[0] + 0.5f},
                             false, CardinalDirection::North);
    if (!checkTest(occupancyMap.wallOccupies({occupancyCell[1] + 0.25f,
                                              occupancyCell[0] + 0.25f}) &&
                       !occupancyMap.wallOccupies({occupancyCell[1] + 0.25f,
                                                   occupancyCell[0] + 0.75f}),
                   "partial brick solid/empty quadrants are incorrect"))
        return 1;
    if (!checkTest(occupancyMap.impactShell({-0.30f, 12.5f}, false,
                                             CardinalDirection::North) ==
                       ImpactKind::Boundary,
                   "full shell AABB did not detect a tangential map boundary"))
        return 1;

    StageMap exactTouchMap;
    if (!exactTouchMap.load(resourceRoot, 1, error))
        return 1;
    std::array<int, 2> exactTouchCell{{-1, -1}};
    for (int row = 0; row < kMapSize && exactTouchCell[0] < 0; ++row)
        for (int column = 1; column < kMapSize; ++column)
            if (exactTouchMap.tile(row, column) == '#' &&
                exactTouchMap.tile(row, column - 1) == '.')
            {
                exactTouchCell = {{row, column}};
                break;
            }
    const ImpactKind exactTouchImpact = exactTouchMap.impactShell(
        {exactTouchCell[1] - 0.25f, exactTouchCell[0] + 0.5f}, false,
        CardinalDirection::North);
    if (!checkTest(exactTouchCell[0] >= 0 &&
                       exactTouchImpact == ImpactKind::None &&
                       exactTouchMap.brickMask(exactTouchCell[0],
                                               exactTouchCell[1]) == 0x0fU,
                   "zero-area edge contact incorrectly damaged a brick"))
        return 1;

    StageMap wrapped;
    if (!checkTest(wrapped.load(resourceRoot, 36, error) && wrapped.stage() == 1, "stage 36 did not wrap to 1"))
        return 1;
    if (!checkTest(wrapped.load(resourceRoot, 0, error) && wrapped.stage() == 35, "stage 0 did not wrap to 35"))
        return 1;

    return 0;
}

int runPlayerLifecycleAndBonusSelfTests()
{
    const auto noisySpawnPlayer = [](int id) {
        Player player;
        player.id = id;
        player.nation = Nation::Germany;
        player.position = {-9.25f, 31.75f};
        player.yaw = 2.75f;
        player.driveDirection = CardinalDirection::West;
        player.movementDirection = CardinalDirection::South;
        player.lives = 7;
        player.maximumHitPoints = 6;
        player.hitPoints = 2;
        player.level = 2;
        player.active = false;
        player.moving = true;
        player.hasBoat = true;
        player.shieldTimer = 0.52f;
        player.creationTimer = 0.73f;
        player.respawnTimer = 0.61f;
        player.deathTimer = 0.47f;
        player.fireCooldown = 0.64f;
        player.dustCooldown = 0.43f;
        player.iceSlipTimer = 0.29f;
        player.onIce = true;
        player.score = 2375;
        player.directKillStreak = 3;
        player.streakPopupTimer = 0.40f;
        player.stageTally.destroyed = {{2, 3, 5, 7}};
        player.stageTally.enemyPoints = {{100, 150, 250, 350}};
        player.stageTally.bonusPoints = 600;
        player.stageTally.scoreAtStageStart = 925;
        return player;
    };
    const auto applyExpectedSpawnWrites = [](Player &player, XZ position) {
        player.position = position;
        player.yaw = 0.0f;
        player.driveDirection = CardinalDirection::North;
        player.movementDirection = CardinalDirection::North;
        player.active = true;
        player.moving = false;
        player.hasBoat = false;
        player.creationTimer = 1.0f;
        player.respawnTimer = 0.0f;
        player.deathTimer = 0.0f;
        player.fireCooldown = kPlayerReloadTime;
        player.dustCooldown = 0.0f;
        player.iceSlipTimer = 0.0f;
        player.onIce = false;
        player.shieldTimer = 10.0f;
    };
    const auto unownedSpawnFieldsMatch = [](const Player &player,
                                             const Player &before) {
        return player.id == before.id && player.nation == before.nation &&
               player.lives == before.lives &&
               player.maximumHitPoints == before.maximumHitPoints &&
               player.score == before.score &&
               player.stageTally.destroyed == before.stageTally.destroyed &&
               player.stageTally.enemyPoints ==
                   before.stageTally.enemyPoints &&
               player.stageTally.bonusPoints ==
                   before.stageTally.bonusPoints &&
               player.stageTally.scoreAtStageStart ==
                   before.stageTally.scoreAtStageStart;
    };

    struct SpawnPositionCase
    {
        int id;
        XZ expected;
    };
    const std::array<SpawnPositionCase, 4> spawnPositionCases{{
        {0, {9.0f, 25.0f}},
        {1, {17.0f, 25.0f}},
        {-8, {17.0f, 25.0f}},
        {37, {17.0f, 25.0f}}}};
    for (const SpawnPositionCase &spawnCase : spawnPositionCases)
    {
        Player mappedPlayer = noisySpawnPlayer(spawnCase.id);
        preparePlayerSpawn(mappedPlayer, false);
        if (!checkTest(
                mappedPlayer.position.x == spawnCase.expected.x &&
                    mappedPlayer.position.z == spawnCase.expected.z,
                "preparePlayerSpawn changed the literal spawn mapping for id " +
                    std::to_string(spawnCase.id)))
        {
            return 1;
        }
    }

    Player preservedPlayer = noisySpawnPlayer(1);
    const Player beforePreserve = preservedPlayer;
    Player expectedPreserve = beforePreserve;
    applyExpectedSpawnWrites(expectedPreserve, {17.0f, 25.0f});
    preparePlayerSpawn(preservedPlayer, false);
    if (!checkTest(
            settlementPlayerStatesMatch(preservedPlayer, expectedPreserve) &&
                unownedSpawnFieldsMatch(preservedPlayer, beforePreserve),
            "preserve-mode spawn changed its full Player write mask"))
    {
        return 1;
    }

    Player resetPlayer = noisySpawnPlayer(37);
    const Player beforeReset = resetPlayer;
    Player expectedReset = beforeReset;
    applyExpectedSpawnWrites(expectedReset, {17.0f, 25.0f});
    expectedReset.level = 0;
    expectedReset.hitPoints = expectedReset.maximumHitPoints;
    expectedReset.directKillStreak = 0;
    expectedReset.streakPopupTimer = 0.0f;
    preparePlayerSpawn(resetPlayer, true);
    if (!checkTest(
            settlementPlayerStatesMatch(resetPlayer, expectedReset) &&
                unownedSpawnFieldsMatch(resetPlayer, beforeReset),
            "reset-mode spawn changed its full Player write mask"))
    {
        return 1;
    }

    Player deadStagePlayer = noisySpawnPlayer(-8);
    deadStagePlayer.hitPoints = 0;
    const Player beforeDeadStage = deadStagePlayer;
    Player expectedDeadStage = beforeDeadStage;
    applyExpectedSpawnWrites(expectedDeadStage, {17.0f, 25.0f});
    expectedDeadStage.hitPoints = expectedDeadStage.maximumHitPoints;
    preparePlayerSpawn(deadStagePlayer, false);
    if (!checkTest(
            settlementPlayerStatesMatch(deadStagePlayer,
                                        expectedDeadStage) &&
                unownedSpawnFieldsMatch(deadStagePlayer, beforeDeadStage),
            "preserve-mode dead stage spawn changed its full Player write mask"))
    {
        return 1;
    }

    Player hitPointPlayer;
    hitPointPlayer.level = 3;
    hitPointPlayer.hitPoints = kDefaultPlayerMaximumHitPoints;
    hitPointPlayer.directKillStreak = 4;
    hitPointPlayer.shieldTimer = 1.0f;
    hitPointPlayer.hasBoat = true;
    const PlayerHitResult shieldedHit = resolvePlayerHit(hitPointPlayer);
    hitPointPlayer.shieldTimer = 0.0f;
    const PlayerHitResult boatHit = resolvePlayerHit(hitPointPlayer);
    const PlayerHitResult firstDamage = resolvePlayerHit(hitPointPlayer);
    const PlayerHitResult secondDamage = resolvePlayerHit(hitPointPlayer);
    const PlayerHitResult fatalDamage = resolvePlayerHit(hitPointPlayer);
    if (!checkTest(shieldedHit == PlayerHitResult::Shielded &&
                       boatHit == PlayerHitResult::BoatAbsorbed &&
                       !hitPointPlayer.hasBoat &&
                       firstDamage == PlayerHitResult::Damaged &&
                       secondDamage == PlayerHitResult::Damaged &&
                       fatalDamage == PlayerHitResult::Destroyed &&
                       hitPointPlayer.hitPoints == 0 && hitPointPlayer.level == 3 &&
                       hitPointPlayer.directKillStreak == 4,
                   "shield, Boat, or three-hit player HP resolution is incorrect"))
        return 1;

    hitPointPlayer.hitPoints = 1;
    const bool firstHeal = hitPointPlayer.healOneHitPoint();
    const bool secondHeal = hitPointPlayer.healOneHitPoint();
    const bool fullHealRejected = !hitPointPlayer.healOneHitPoint();
    const bool fullBandageRejected =
        !playerMeetsBonusTypeEligibility(
            hitPointPlayer, bonus_assets::Type::Bandage);
    hitPointPlayer.hitPoints = 2;
    const bool hurtBandageAccepted =
        playerMeetsBonusTypeEligibility(
            hitPointPlayer, bonus_assets::Type::Bandage);
    const bool classicBonusStillAccepted =
        playerMeetsBonusTypeEligibility(
            hitPointPlayer, bonus_assets::Type::Star);
    hitPointPlayer.active = false;
    const bool inactiveBandageRejected =
        !playerMeetsBonusTypeEligibility(
            hitPointPlayer, bonus_assets::Type::Bandage);
    if (!checkTest(firstHeal && secondHeal && fullHealRejected &&
                       fullBandageRejected && hurtBandageAccepted &&
                       classicBonusStillAccepted && inactiveBandageRejected,
                   "Bandage healing or collection eligibility is incorrect"))
        return 1;

    Player oneHitPointPlayer;
    oneHitPointPlayer.maximumHitPoints = 1;
    oneHitPointPlayer.hitPoints = 1;
    const bool oneHpBandageDisabled =
        !oneHitPointPlayer.needsHealing() &&
        !oneHitPointPlayer.healOneHitPoint() &&
        !playerMeetsBonusTypeEligibility(
            oneHitPointPlayer, bonus_assets::Type::Bandage);
    const PlayerHitResult oneHpFatalHit =
        resolvePlayerHit(oneHitPointPlayer);
    Player sixHitPointPlayer;
    sixHitPointPlayer.maximumHitPoints = 6;
    sixHitPointPlayer.hitPoints = 5;
    const bool sixHpHealed = sixHitPointPlayer.healOneHitPoint() &&
                             sixHitPointPlayer.hitPoints == 6 &&
                             !sixHitPointPlayer.healOneHitPoint();
    if (!checkTest(oneHpBandageDisabled &&
                       oneHpFatalHit == PlayerHitResult::Destroyed &&
                       oneHitPointPlayer.hitPoints == 0 && sixHpHealed,
                   "configured 1-6 HP range or 1-HP Bandage disable is incorrect"))
        return 1;

    std::array<int, static_cast<std::size_t>(bonus_assets::Type::Count)>
        healthyBonusWeights{};
    std::array<int, static_cast<std::size_t>(bonus_assets::Type::Count)>
        injuredBonusWeights{};
    for (int slot = 0; slot < bonus_assets::weightedTypeSlotCount(false); ++slot)
        ++healthyBonusWeights[static_cast<std::size_t>(
            bonus_assets::typeForWeightedSlot(slot, false))];
    for (int slot = 0; slot < bonus_assets::weightedTypeSlotCount(true); ++slot)
        ++injuredBonusWeights[static_cast<std::size_t>(
            bonus_assets::typeForWeightedSlot(slot, true))];
    bool classicWeightsExact = true;
    for (int typeIndex = 0;
         typeIndex < bonus_assets::kClassicPickupTypeCount; ++typeIndex)
    {
        classicWeightsExact = classicWeightsExact &&
                              healthyBonusWeights[static_cast<std::size_t>(typeIndex)] == 1 &&
                              injuredBonusWeights[static_cast<std::size_t>(typeIndex)] == 1;
    }
    const std::size_t bandageIndex =
        static_cast<std::size_t>(bonus_assets::Type::Bandage);
    if (!checkTest(bonus_assets::weightedTypeSlotCount(false) == 8 &&
                       bonus_assets::weightedTypeSlotCount(true) == 10 &&
                       classicWeightsExact && healthyBonusWeights[bandageIndex] == 0 &&
                       injuredBonusWeights[bandageIndex] == 2,
                   "healthy or injured bonus weights are incorrect"))
        return 1;

    Player streakPlayer;
    streakPlayer.stageTally.reset(0);
    streakPlayer.creditDirectEnemyHit(3, 50, false);
    streakPlayer.creditDirectEnemyHit(0, 50, true);
    streakPlayer.creditDirectEnemyHit(1, 50, true);
    if (!checkTest(streakPlayer.directKillStreak == 2 &&
                       streakPlayer.score == 150 &&
                       streakPlayer.stageTally.totalDestroyed() == 2 &&
                       std::fabs(streakPlayer.streakPopupTimer -
                                 kStreakPopupDuration) < 0.0001f,
                   "direct tank destructions did not build the streak"))
        return 1;
    streakPlayer.active = false;
    streakPlayer.creditDirectEnemyHit(2, 50, true);
    const bool deadShellKeptScoreWithoutStreak =
        streakPlayer.directKillStreak == 2 && streakPlayer.score == 200 &&
        streakPlayer.stageTally.totalDestroyed() == 3;
    streakPlayer.resetDirectKillStreak();
    if (!checkTest(deadShellKeptScoreWithoutStreak &&
                       streakPlayer.directKillStreak == 0 &&
                       streakPlayer.streakPopupTimer == 0.0f,
                   "death reset or posthumous-shell streak rule is incorrect"))
        return 1;

    PlayerDeathState deathPlayer;
    deathPlayer.lives = 2;
    deathPlayer.deathTimer = kTankDeathDuration;
    const PlayerDeathTransition waitingDeath =
        advanceInactivePlayerDeath(deathPlayer, 0.48f);
    const PlayerDeathTransition finishedDeath =
        advanceInactivePlayerDeath(deathPlayer, 0.02f);
    PlayerDeathState finalLifePlayer;
    finalLifePlayer.lives = 1;
    finalLifePlayer.deathTimer = kTankDeathDuration;
    const PlayerDeathTransition finalDeath =
        advanceInactivePlayerDeath(finalLifePlayer, 0.50f);
    if (!checkTest(waitingDeath == PlayerDeathTransition::Waiting &&
                       deathPlayer.lives == 1 &&
                       finishedDeath == PlayerDeathTransition::Respawn &&
                       finalDeath == PlayerDeathTransition::Eliminated &&
                       finalLifePlayer.lives == 0,
                   "player life changed before the 490 ms explosion ended"))
        return 1;

    Enemy destroyedEnemy;
    destroyedEnemy.id = 42;
    destroyedEnemy.destroyed = true;
    destroyedEnemy.deathTimer = 0.0f;
    Shell survivingEnemyRound;
    survivingEnemyRound.owner = ShellOwner::Enemy;
    survivingEnemyRound.ownerIndex = destroyedEnemy.id;
    std::vector<Shell> destructionShells{survivingEnemyRound};
    const bool removedBeforeRound =
        enemyDestructionComplete(destroyedEnemy, destructionShells);
    destructionShells.clear();
    if (!checkTest(!removedBeforeRound &&
                       enemyDestructionComplete(destroyedEnemy,
                                                destructionShells),
                   "enemy vacated its slot before its final shell ended"))
        return 1;

    if (!checkTest(aabbOverlapsRectangle(
                           {0.0f, 0.0f}, kTankRadius,
                           0.874f, 0.874f, 1.5f, 1.5f) &&
                       !aabbOverlapsRectangle(
                           {0.0f, 0.0f}, kTankRadius,
                           0.875f, 0.875f, 1.5f, 1.5f),
                   "tank obstacle collision is not a strict 28x28 AABB"))
        return 1;
    if (!checkTest(upgradedPlayerLevel(0) == 1 &&
                       upgradedPlayerLevel(1) == 2 &&
                       upgradedPlayerLevel(2) == 3 &&
                       upgradedPlayerLevel(3) == 3,
                   "star progression is not 0 -> 1 -> 2 -> 3"))
        return 1;
    return 0;
}

int runSettingsProgressionAndSettlementSelfTests(
    const fs::path &resourceRoot)
{
    AdvancedGameSettings invalidAdvanced;
    invalidAdvanced.playerMaximumHitPoints = 99;
    invalidAdvanced.enemySpeedPercent = -99;
    invalidAdvanced.enemyFireRatePercent = 2;
    invalidAdvanced.enemySpawnRatePercent = 29;
    const AdvancedGameSettings normalizedAdvanced =
        normalizedAdvancedSettings(invalidAdvanced);
    bool tuningStepsExact = true;
    for (int percent = kEnemyTuningMinimumPercent;
         percent <= kEnemyTuningMaximumPercent;
         percent += kEnemyTuningPercentStep)
    {
        tuningStepsExact = tuningStepsExact &&
                           normalizedEnemyTuningPercent(percent) == percent;
    }
    AdvancedGameSettings slowEnemySettings;
    slowEnemySettings.enemySpeedPercent = -30;
    AdvancedGameSettings fastEnemySettings;
    fastEnemySettings.enemySpeedPercent = 30;
    if (!checkTest(
            normalizedAdvanced.playerMaximumHitPoints == 6 &&
                normalizedAdvanced.enemySpeedPercent == -30 &&
                normalizedAdvanced.enemyFireRatePercent == 0 &&
                normalizedAdvanced.enemySpawnRatePercent == 30 &&
                tuningStepsExact &&
                std::fabs(enemyRateScale(-30) - 0.70f) < 0.0001f &&
                std::fabs(enemyRateScale(0) - 1.0f) < 0.0001f &&
                std::fabs(enemyRateScale(30) - 1.30f) < 0.0001f &&
                std::fabs(enemyMovementSpeedForType(0, slowEnemySettings) -
                          2.8f) < 0.0001f &&
                std::fabs(enemyMovementSpeedForType(1, slowEnemySettings) -
                          3.64f) < 0.0001f &&
                std::fabs(enemyMovementSpeedForType(0, fastEnemySettings) -
                          5.2f) < 0.0001f &&
                std::fabs(enemyMovementSpeedForType(1, fastEnemySettings) -
                          6.76f) < 0.0001f &&
                std::fabs(intervalForEnemyRate(0.5f, -30) -
                          (0.5f / 0.7f)) < 0.0001f &&
                std::fabs(intervalForEnemyRate(0.5f, 30) -
                          (0.5f / 1.3f)) < 0.0001f &&
                std::fabs(intervalForEnemyRate(0.8f, 30) -
                          (0.8f / 1.3f)) < 0.0001f,
            "advanced percentage normalization, movement, or rate intervals are incorrect"))
        return 1;
    MenuSettings menuDefaults;
    const bool onePlayerAdvancedRow =
        menuRowCount(menuDefaults) == 5 &&
        advancedMenuRow(menuDefaults) == 4;
    menuDefaults.playerCount = 2;
    if (!checkTest(onePlayerAdvancedRow &&
                       kAdvancedMenuRowCount == 7 &&
                       kAdvancedMenuBackRow == 6 &&
                       menuRowCount(menuDefaults) == 6 &&
                       advancedMenuRow(menuDefaults) == 5 &&
                       advancedSettingsAreDefault(menuDefaults) &&
                       percentageLabel(-30) == "-30%" &&
                       percentageLabel(0) == "0%  DEFAULT" &&
                       percentageLabel(30) == "+30%" &&
                       cameraYawLabel(-45) == "LEFT 45 DEG" &&
                       cameraYawLabel(0) == "0 DEG  STRAIGHT" &&
                       cameraYawLabel(45) == "RIGHT 45 DEG" &&
                       cameraElevationLabel(40) == "40 DEG" &&
                       cameraElevationLabel(50) == "50 DEG  DEFAULT" &&
                       cameraElevationLabel(70) == "70 DEG",
                   "advanced menu rows, defaults, or percentage labels are incorrect"))
        return 1;
    MenuSettings cameraMenu;
    cameraMenu.advancedSelected = 4;
    UiInputFrame adjustCameraRight;
    adjustCameraRight.rightPressed = true;
    const bool cameraStepChanged =
        updateAdvancedMenu(cameraMenu, adjustCameraRight);
    cameraMenu.cameraYawDegrees = kCameraYawMaximumDegrees;
    updateAdvancedMenu(cameraMenu, adjustCameraRight);
    const bool cameraMaximumClamped =
        cameraMenu.cameraYawDegrees == kCameraYawMaximumDegrees;
    UiInputFrame adjustCameraLeft;
    adjustCameraLeft.leftPressed = true;
    updateAdvancedMenu(cameraMenu, adjustCameraLeft);
    const bool cameraLeftStepExact =
        cameraMenu.cameraYawDegrees ==
        kCameraYawMaximumDegrees - kCameraYawStepDegrees;
    cameraMenu.cameraYawDegrees = kCameraYawMinimumDegrees;
    updateAdvancedMenu(cameraMenu, adjustCameraLeft);
    if (!checkTest(cameraStepChanged && cameraMaximumClamped &&
                       cameraLeftStepExact &&
                       cameraMenu.cameraYawDegrees ==
                           kCameraYawMinimumDegrees,
                   "camera menu step or endpoint clamping is incorrect"))
        return 1;
    MenuSettings elevationMenu;
    elevationMenu.advancedSelected = 5;
    const bool elevationStepChanged =
        updateAdvancedMenu(elevationMenu, adjustCameraRight);
    elevationMenu.cameraElevationDegrees =
        kCameraElevationMaximumDegrees;
    updateAdvancedMenu(elevationMenu, adjustCameraRight);
    const bool elevationMaximumClamped =
        elevationMenu.cameraElevationDegrees ==
        kCameraElevationMaximumDegrees;
    updateAdvancedMenu(elevationMenu, adjustCameraLeft);
    const bool elevationLeftStepExact =
        elevationMenu.cameraElevationDegrees ==
        kCameraElevationMaximumDegrees - kCameraElevationStepDegrees;
    elevationMenu.cameraElevationDegrees =
        kCameraElevationMinimumDegrees;
    updateAdvancedMenu(elevationMenu, adjustCameraLeft);
    if (!checkTest(
            elevationStepChanged && elevationMaximumClamped &&
                elevationLeftStepExact &&
                elevationMenu.cameraElevationDegrees ==
                    kCameraElevationMinimumDegrees,
            "camera elevation menu step or endpoint clamping is incorrect"))
        return 1;
    menuDefaults.advanced.enemySpeedPercent = 5;
    menuDefaults.cameraYawDegrees = 35;
    menuDefaults.cameraElevationDegrees = 70;
    UiInputFrame resetAdvanced;
    resetAdvanced.resetPressed = true;
    const bool advancedResetChanged =
        updateAdvancedMenu(menuDefaults, resetAdvanced);
    if (!checkTest(advancedResetChanged &&
                       advancedSettingsAreDefault(menuDefaults),
                   "advanced settings reset is incorrect"))
        return 1;
    bool elevationGeometryValid = true;
    for (const int degrees : std::array<int, 3>{{
             kCameraElevationMinimumDegrees,
             kDefaultCameraElevationDegrees,
             kCameraElevationMaximumDegrees}})
    {
        const GameplayCameraElevationGeometry geometry =
            gameplayCameraElevationGeometry(degrees);
        elevationGeometryValid = elevationGeometryValid &&
            std::fabs(std::hypot(geometry.depthOffset,
                                 geometry.verticalOffset) -
                      kGameplayCameraOrbitDistance) < 0.0001f &&
            std::fabs(std::atan2(geometry.verticalOffset,
                                 geometry.depthOffset) *
                          (180.0f / kPi) -
                      static_cast<float>(degrees)) < 0.001f &&
            std::fabs(geometry.verticalOffset /
                          kGameplayCameraOrbitDistance -
                      geometry.groundDepthProjection) < 0.0001f;
    }
    const GameplayCameraElevationGeometry defaultElevationGeometry =
        gameplayCameraElevationGeometry(kDefaultCameraElevationDegrees);
    if (!checkTest(
            elevationGeometryValid &&
                std::fabs(defaultElevationGeometry.depthOffset - 13.51f) <
                    0.001f &&
                std::fabs(defaultElevationGeometry.verticalOffset - 16.10f) <
                    0.001f &&
                std::fabs(defaultElevationGeometry.groundDepthProjection -
                          0.76603282f) < 0.0001f,
            "camera elevation geometry changed orbit distance or default framing"))
        return 1;
    const std::array<float, 4> expectedMovement{{5.0f, 6.5f, 6.5f, 6.5f}};
    const std::array<float, 4> expectedShellSpeed{{9.775f, 12.7075f,
                                                   12.7075f, 12.7075f}};
    const std::array<int, 4> expectedShellCount{{2, 2, 3, 4}};
    for (int level = 0; level < 4; ++level)
    {
        const PlayerLevelStats stats = playerLevelStats(level);
        if (!checkTest(std::fabs(stats.movementSpeed - expectedMovement[level]) < 0.0001f &&
                           std::fabs(stats.shellSpeed - expectedShellSpeed[level]) < 0.0001f &&
                           stats.maximumShells == expectedShellCount[level] &&
                           stats.powerShell == (level == 3),
                       "player Star level stats drifted from the 2D rules"))
            return 1;
    }
    if (!checkTest(std::fabs(kPlayerReloadTime - 0.120f) < 0.0001f &&
                       std::fabs(kClassicBaseEnemySpeed - 5.0f) < 0.0001f &&
                       std::fabs(kEnemyMovementSpeedScale - 0.80f) < 0.0001f &&
                       std::fabs(kBaseEnemySpeed - 4.0f) < 0.0001f &&
                       std::fabs(kFastEnemySpeed - 5.2f) < 0.0001f &&
                       std::fabs(kIceSlipDuration - 0.380f) < 0.0001f &&
                       std::fabs(kTankRadius - 0.875f) < 0.0001f &&
                       std::fabs(kTankPairCollisionExtent - 1.75f) <
                           0.0001f &&
                       std::fabs(kShellHalfSize - 0.25f) < 0.0001f &&
                       std::fabs(kShellTankHitExtent - 1.125f) < 0.0001f &&
                       std::fabs(kPickupTankHitExtent - 1.875f) <
                           0.0001f &&
                       std::fabs(kShellSpawnDistance - 0.625f) < 0.0001f &&
                       std::fabs(kShellSweepStep - 0.12f) < 0.0001f &&
                       std::fabs(kShellImpactDuration - 0.200f) < 0.0001f &&
                       std::fabs(kTankDeathDuration - 0.490f) < 0.0001f &&
                       kDefaultPlayerMaximumHitPoints == 3 &&
                       kMinimumPlayerMaximumHitPoints == 1 &&
                       kMaximumPlayerMaximumHitPoints == 6 &&
                       kEnemyTuningMinimumPercent == -30 &&
                       kEnemyTuningMaximumPercent == 30 &&
                       kEnemyTuningPercentStep == 5 &&
                       kCameraYawMinimumDegrees == -45 &&
                       kCameraYawMaximumDegrees == 45 &&
                       kCameraYawStepDegrees == 5 &&
                       normalizedCameraYawDegrees(-90) == -45 &&
                       normalizedCameraYawDegrees(90) == 45 &&
                       kCameraElevationMinimumDegrees == 40 &&
                       kCameraElevationMaximumDegrees == 70 &&
                       kCameraElevationStepDegrees == 5 &&
                       kDefaultCameraElevationDegrees == 50 &&
                       normalizedCameraElevationDegrees(0) == 40 &&
                       normalizedCameraElevationDegrees(50) == 50 &&
                       normalizedCameraElevationDegrees(90) == 70 &&
                       std::fabs(kEnemySpawnInterval - 0.5f) < 0.0001f &&
                       std::fabs(kEnemySpawnRetryInterval - 0.15f) < 0.0001f &&
                       std::fabs(kEnemyInitialFireDelay - 0.1f) < 0.0001f &&
                       std::fabs(kClassicBaseShellSpeed - 14.375f) < 0.0001f &&
                       std::fabs(kShellPacingScale - 0.68f) < 0.0001f &&
                       std::fabs(kBaseShellSpeed - 9.775f) < 0.0001f &&
                       std::fabs(kFastShellSpeed - 12.7075f) < 0.0001f &&
                       std::fabs(tanks3d::game::kEnemyCollisionProbePadding -
                                 0.0625f) <
                           0.0001f &&
                       std::fabs(kEnemyBlockedEscapeDelay - 0.30f) <
                           0.0001f &&
                       std::fabs(kSoloCameraSpan - 15.5f) < 0.0001f &&
                       std::fabs(kGameplayCameraOrbitDistance -
                                 21.017376f) < 0.0001f &&
                       std::fabs(kGameplayCameraTargetHeight - 0.35f) <
                           0.0001f &&
                       std::fabs(kGameplayCameraFollowResponsiveness - 12.0f) <
                           0.0001f &&
                       std::fabs(kBonusCarrierChance - 0.12f) < 0.0001f &&
                       bonus_assets::kClassicPickupTypeCount == 8 &&
                       bonus_assets::kBandageWeight == 2 &&
                       std::fabs(bonus_assets::kPickupLifetime - 12.5f) <
                           0.0001f &&
                       std::fabs(bonus_assets::kPickupFastBlinkStart - 9.375f) <
                           0.0001f &&
                       std::fabs(bonus_assets::Pickup{}.life - 12.5f) <
                           0.0001f &&
                       std::fabs(bonus_assets::kPickupModelScale - 1.53f) <
                           0.0001f &&
                       std::fabs(bonus_assets::kPickupIconSize - 0.90f) <
                           0.0001f &&
                       std::fabs(kEnemyCreationDuration - 1.0f) < 0.0001f &&
                       kEnemyCreationFrameCount == 10 &&
                       std::fabs(kEnemyCreationFrameDuration - 0.1f) <
                           0.0001f &&
                       std::fabs(kStageIntroDuration - 3.2f) < 0.0001f &&
                       enemyCreationFrame(1.0f) == 0 &&
                       enemyCreationFrame(0.95f) == 0 &&
                       enemyCreationFrame(0.85f) == 1 &&
                       enemyCreationFrame(0.05f) == 9 &&
                       enemyCreationFrame(0.0f) == -1 &&
                       std::fabs(enemyCreationScale(3) - 1.0f) < 0.0001f &&
                       std::fabs(enemyCreationScale(6) - 0.25f) < 0.0001f,
                   "player timing, projectile geometry, pickups, or 10-frame enemy creation timing drifted"))
        return 1;

    if (!checkTest(axisAlignedCentersOverlap({0.0f, 0.0f},
                                              {1.749f, 1.749f},
                                              kTankPairCollisionExtent) &&
                       !axisAlignedCentersOverlap({0.0f, 0.0f},
                                                  {1.75f, 0.0f},
                                                  kTankPairCollisionExtent) &&
                       axisAlignedCentersOverlap({0.0f, 0.0f},
                                                 {1.124f, 1.124f},
                                                 kShellTankHitExtent) &&
                       !axisAlignedCentersOverlap({0.0f, 0.0f},
                                                  {1.125f, 0.0f},
                                                  kShellTankHitExtent) &&
                       axisAlignedCentersOverlap({0.0f, 0.0f},
                                                 {1.874f, 1.874f},
                                                 kPickupTankHitExtent) &&
                       !axisAlignedCentersOverlap({0.0f, 0.0f},
                                                  {1.875f, 0.0f},
                                                  kPickupTankHitExtent),
                   "classic tank, shell, or pickup AABB edges drifted"))
        return 1;

    CardinalDirection iceTravel = CardinalDirection::North;
    float iceTimer = 0.0f;
    bool wasOnIce = false;
    const bool enteredIce = resolveIceTravel(
        true, true, false, true, CardinalDirection::North, 0.05f,
        iceTravel, iceTimer, wasOnIce);
    const bool keptOldDirection = resolveIceTravel(
        true, true, false, true, CardinalDirection::East, 0.19f,
        iceTravel, iceTimer, wasOnIce);
    const bool acceptedTurn = resolveIceTravel(
        true, true, false, true, CardinalDirection::East, 0.20f,
        iceTravel, iceTimer, wasOnIce);
    if (!checkTest(enteredIce && keptOldDirection && acceptedTurn &&
                       iceTravel == CardinalDirection::East &&
                       iceTimer <= 0.0001f,
                   "ice did not preserve the old direction for 380 ms"))
        return 1;

    iceTravel = CardinalDirection::North;
    iceTimer = kIceSlipDuration;
    wasOnIce = true;
    const bool releaseStillSlides = resolveIceTravel(
        true, false, false, true, CardinalDirection::North, 0.19f,
        iceTravel, iceTimer, wasOnIce);
    const bool releaseStopped = resolveIceTravel(
        true, false, false, true, CardinalDirection::North, 0.20f,
        iceTravel, iceTimer, wasOnIce);
    iceTravel = CardinalDirection::North;
    iceTimer = kIceSlipDuration;
    wasOnIce = true;
    const bool blockedSlide = resolveIceTravel(
        true, false, true, true, CardinalDirection::East, 0.01f,
        iceTravel, iceTimer, wasOnIce);
    if (!checkTest(releaseStillSlides && !releaseStopped && !blockedSlide &&
                       iceTravel == CardinalDirection::East &&
                       iceTimer <= 0.0001f,
                   "ice release or collision did not end the classic slip"))
        return 1;
    if (!checkTest(nextSettlementScoreCounter(0) == 1 &&
                       nextSettlementScoreCounter(9) == 10 &&
                       nextSettlementScoreCounter(10) == 20 &&
                       nextSettlementScoreCounter(90) == 100 &&
                       nextSettlementScoreCounter(900) == 1000 &&
                       nextSettlementScoreCounter(9000) == 10000 &&
                       nextSettlementScoreCounter(90000) == 100000 &&
                       std::fabs(kStageEndDelay - 5.0f) < 0.0001f &&
                       std::fabs(kGameOverReportDelay - 3.1f) < 0.0001f &&
                       std::fabs(kSettlementCountStepTime - 0.1f) < 0.0001f &&
                       std::fabs(kSettlementIdleTime - 5.0f) < 0.0001f &&
                       std::fabs(kHighScoreDisplayDuration - 5.2f) <
                           0.0001f &&
                       std::fabs(kGovernmentSteelDuration - 20.0f) < 0.0001f &&
                       std::fabs(kGovernmentSteelWarningDuration - 3.0f) <
                           0.0001f,
                   "2D score settlement cadence or transition timing drifted"))
        return 1;

    StageTally armorTally;
    armorTally.reset(1200);
    armorTally.creditEnemy(3, 50, false);
    armorTally.creditEnemy(3, 50, false);
    armorTally.creditEnemy(3, 50, false);
    armorTally.creditEnemy(3, 50, true);
    armorTally.creditEnemy(0, 50, true);
    armorTally.creditBonus(300);
    StageTally grenadeTally;
    grenadeTally.reset(0);
    grenadeTally.creditBonus(300);
    grenadeTally.creditBonus(kEnemyTypeCount * 200);
    if (!checkTest(armorTally.destroyed[3] == 1 &&
                       armorTally.destroyed[0] == 1 &&
                       armorTally.enemyPoints[3] == 200 &&
                       armorTally.totalDestroyed() == 2 &&
                       armorTally.totalEnemyPoints() == 250 &&
                       armorTally.bonusPoints == 300 &&
                       armorTally.stagePoints() == 550 &&
                       armorTally.scoreAtStageStart == 1200 &&
                       grenadeTally.totalDestroyed() == 0 &&
                       grenadeTally.totalEnemyPoints() == 0 &&
                       grenadeTally.bonusPoints == 1100 &&
                       grenadeTally.stagePoints() == 1100,
                   "classified tally counted grenade clears as tank K.O.s"))
        return 1;

    Game3D settlementGame(resourceRoot, 0x5e771e00U);
    const std::array<Nation, 2> settlementNations{{
        Nation::UnitedStates, Nation::SovietUnion}};
    if (!settlementGame.start(2, 3, 1, settlementNations))
        return 1;
    const GameplayCameraElevationGeometry defaultCameraGeometry =
        gameplayCameraElevationGeometry(kDefaultCameraElevationDegrees);
    bool cardinalCameraReady =
        settlementGame.cameraYawDegrees() == 0 &&
        settlementGame.cameraElevationDegrees() ==
            kDefaultCameraElevationDegrees;
    for (const CameraRig &camera : settlementGame.cameraRigs())
    {
        cardinalCameraReady = cardinalCameraReady && camera.initialized &&
            std::fabs(camera.position.x - camera.target.x) < 0.0001f &&
            std::fabs((camera.position.y - camera.target.y) -
                      defaultCameraGeometry.verticalOffset) < 0.0001f &&
            std::fabs((camera.position.z - camera.target.z) -
                      defaultCameraGeometry.depthOffset) < 0.0001f;
    }
    if (!checkTest(cardinalCameraReady,
                   "default gameplay camera is not cardinal-aligned south "
                   "of its target"))
        return 1;
    const auto cameraMatchesAngles = [](const Game3D &game, int yawDegrees,
                                        int elevationDegrees) {
        const CameraPlanarBasis basis = cameraPlanarBasis(yawDegrees);
        const GameplayCameraElevationGeometry geometry =
            gameplayCameraElevationGeometry(elevationDegrees);
        bool matches = game.cameraYawDegrees() ==
                           normalizedCameraYawDegrees(yawDegrees) &&
                       game.cameraElevationDegrees() ==
                           normalizedCameraElevationDegrees(
                               elevationDegrees);
        for (int index = 0; index < game.playerCount(); ++index)
        {
            const CameraRig &camera =
                game.cameraRigs()[static_cast<std::size_t>(index)];
            const float dx = camera.position.x - camera.target.x;
            const float dz = camera.position.z - camera.target.z;
            matches = matches && camera.initialized &&
                std::fabs(dx - basis.offsetX *
                                   geometry.depthOffset) < 0.0001f &&
                std::fabs(dz - basis.offsetZ *
                                   geometry.depthOffset) < 0.0001f &&
                std::fabs(std::sqrt(dx * dx + dz * dz) -
                          geometry.depthOffset) < 0.0001f &&
                std::fabs((camera.position.y - camera.target.y) -
                          geometry.verticalOffset) < 0.0001f;
        }
        return matches;
    };
    Game3D leftCameraGame(resourceRoot, 0xcab1e045U);
    Game3D straightDigestGame(resourceRoot, 0xcab1e046U);
    Game3D rotatedDigestGame(resourceRoot, 0xcab1e046U);
    const bool cameraGamesReady =
        leftCameraGame.start(1, 3, 1, settlementNations,
                             AdvancedGameSettings{}, -45,
                             kCameraElevationMinimumDegrees) &&
        cameraMatchesAngles(leftCameraGame, -45,
                            kCameraElevationMinimumDegrees) &&
        leftCameraGame.restart() &&
        cameraMatchesAngles(leftCameraGame, -45,
                            kCameraElevationMinimumDegrees) &&
        straightDigestGame.start(1, 3, 1, settlementNations,
                                 AdvancedGameSettings{}, 0, 50) &&
        rotatedDigestGame.start(1, 3, 1, settlementNations,
                                AdvancedGameSettings{}, 45, 70);
    if (!checkTest(cameraGamesReady,
                   "selectable camera games failed to start, restart, or "
                   "match their requested azimuth"))
        return 1;
    leftCameraGame.setCameraYawDegrees(99);
    leftCameraGame.setCameraElevationDegrees(99);
    if (!checkTest(
            cameraMatchesAngles(leftCameraGame, 45, 70) &&
                straightDigestGame.sessionDigest() ==
                    rotatedDigestGame.sessionDigest(),
            "camera angle did not clamp/persist or leaked into deterministic "
            "gameplay state"))
        return 1;
    Game3D centeredCameraGame(resourceRoot, 0xcab1e047U);
    if (!centeredCameraGame.start(1, 3, 1, settlementNations))
        return 1;
    const std::array<XZ, 6> soloTrackingPositions{{
        {13.0f, 13.0f}, {0.875f, 0.875f}, {25.125f, 0.875f},
        {0.875f, 25.125f}, {25.125f, 25.125f}, {8.25f, 16.75f}}};
    bool soloCameraCentered = true;
    for (const int yawDegrees : std::array<int, 3>{{-45, 0, 45}})
    {
        centeredCameraGame.setCameraYawDegrees(yawDegrees);
        const CameraPlanarBasis basis = cameraPlanarBasis(yawDegrees);
        for (const int elevationDegrees : std::array<int, 3>{{
                 kCameraElevationMinimumDegrees,
                 kDefaultCameraElevationDegrees,
                 kCameraElevationMaximumDegrees}})
        {
            centeredCameraGame.setCameraElevationDegrees(
                elevationDegrees);
            const GameplayCameraElevationGeometry geometry =
                gameplayCameraElevationGeometry(elevationDegrees);
            for (const XZ position : soloTrackingPositions)
            {
                Game3DTestAccess::recenterPlayer(
                    centeredCameraGame, 0, position);
                Game3DTestAccess::refreshCameras(centeredCameraGame);
                const CameraRig &camera =
                    centeredCameraGame.cameraRigs()[0];
                soloCameraCentered = soloCameraCentered &&
                    std::fabs(camera.target.x - position.x) < 0.0001f &&
                    std::fabs(camera.target.z - position.z) < 0.0001f &&
                    std::fabs(camera.position.x - camera.target.x -
                              basis.offsetX * geometry.depthOffset) <
                        0.0001f &&
                    std::fabs(camera.position.z - camera.target.z -
                              basis.offsetZ * geometry.depthOffset) <
                        0.0001f &&
                    std::fabs(camera.position.y - camera.target.y -
                              geometry.verticalOffset) < 0.0001f;
            }
        }
    }
    if (!checkTest(soloCameraCentered,
                   "solo camera did not center its active tank across yaw, "
                   "elevation, and playable positions"))
        return 1;

    bool smoothCameraFollow = true;
    constexpr XZ smoothStart{13.0f, 13.0f};
    constexpr XZ smoothEnd{13.25f, 12.80f};
    for (const int yawDegrees : std::array<int, 3>{{-45, 0, 45}})
    {
        centeredCameraGame.setCameraYawDegrees(yawDegrees);
        const CameraPlanarBasis basis = cameraPlanarBasis(yawDegrees);
        for (const int elevationDegrees : std::array<int, 3>{{
                 kCameraElevationMinimumDegrees,
                 kDefaultCameraElevationDegrees,
                 kCameraElevationMaximumDegrees}})
        {
            centeredCameraGame.setCameraElevationDegrees(
                elevationDegrees);
            const GameplayCameraElevationGeometry geometry =
                gameplayCameraElevationGeometry(elevationDegrees);
            Game3DTestAccess::recenterPlayer(
                centeredCameraGame, 0, smoothStart);
            Game3DTestAccess::refreshCameras(centeredCameraGame);
            Game3DTestAccess::recenterPlayer(
                centeredCameraGame, 0, smoothEnd);
            Game3DTestAccess::advanceCameras(
                centeredCameraGame, 1.0f / 60.0f);
            const CameraRig firstStep = centeredCameraGame.cameraRigs()[0];
            const float firstError = std::hypot(
                smoothEnd.x - firstStep.target.x,
                smoothEnd.z - firstStep.target.z);
            const float initialError = std::hypot(
                smoothEnd.x - smoothStart.x,
                smoothEnd.z - smoothStart.z);
            const float movedX = firstStep.target.x - smoothStart.x;
            const float movedZ = firstStep.target.z - smoothStart.z;
            const float movementCross =
                movedX * (smoothEnd.z - smoothStart.z) -
                movedZ * (smoothEnd.x - smoothStart.x);
            Game3DTestAccess::advanceCameras(
                centeredCameraGame, 1.0f / 60.0f);
            const CameraRig secondStep =
                centeredCameraGame.cameraRigs()[0];
            const float secondError = std::hypot(
                smoothEnd.x - secondStep.target.x,
                smoothEnd.z - secondStep.target.z);
            smoothCameraFollow = smoothCameraFollow &&
                movedX > 0.0f && movedX < smoothEnd.x - smoothStart.x &&
                movedZ < 0.0f && movedZ > smoothEnd.z - smoothStart.z &&
                std::fabs(movementCross) < 0.0001f &&
                firstError < initialError && secondError < firstError &&
                std::fabs(secondStep.position.x - secondStep.target.x -
                          basis.offsetX * geometry.depthOffset) < 0.0001f &&
                std::fabs(secondStep.position.z - secondStep.target.z -
                          basis.offsetZ * geometry.depthOffset) < 0.0001f &&
                std::fabs(secondStep.position.y - secondStep.target.y -
                          geometry.verticalOffset) < 0.0001f;
        }
    }
    if (!checkTest(smoothCameraFollow,
                   "camera did not begin converging on a small tank movement "
                   "during its first frame"))
        return 1;

    Game3D coopCameraGame(resourceRoot, 0xcab1e048U);
    if (!coopCameraGame.start(2, 3, 1, settlementNations))
        return 1;
    struct CoopCameraCase
    {
        int yawDegrees;
        int elevationDegrees;
        XZ first;
        XZ second;
    };
    const std::array<CoopCameraCase, 5> coopCases{{
        {45, kCameraElevationMinimumDegrees,
         {17.05f, 8.95f}, {25.125f, 0.875f}},
        {45, kCameraElevationMaximumDegrees,
         {17.05f, 17.05f}, {25.125f, 25.125f}},
        {0, kDefaultCameraElevationDegrees,
         {5.0f, 7.0f}, {19.0f, 16.0f}},
        {-45, kCameraElevationMaximumDegrees,
         {17.05f, 17.05f}, {25.125f, 25.125f}},
        {-45, kCameraElevationMinimumDegrees,
         {8.95f, 17.05f}, {0.875f, 25.125f}}}};
    bool coopCameraCentered = true;
    for (const CoopCameraCase &testCase : coopCases)
    {
        coopCameraGame.setCameraYawDegrees(testCase.yawDegrees);
        coopCameraGame.setCameraElevationDegrees(
            testCase.elevationDegrees);
        Game3DTestAccess::recenterPlayer(
            coopCameraGame, 0, testCase.first);
        Game3DTestAccess::recenterPlayer(
            coopCameraGame, 1, testCase.second);
        Game3DTestAccess::refreshCameras(coopCameraGame);
        const XZ midpoint{
            (testCase.first.x + testCase.second.x) * 0.5f,
            (testCase.first.z + testCase.second.z) * 0.5f};
        const CameraPlanarBasis basis =
            cameraPlanarBasis(testCase.yawDegrees);
        const GameplayCameraElevationGeometry geometry =
            gameplayCameraElevationGeometry(testCase.elevationDegrees);
        for (const CameraRig &camera : coopCameraGame.cameraRigs())
        {
            coopCameraCentered = coopCameraCentered &&
                std::fabs(camera.target.x - midpoint.x) < 0.0001f &&
                std::fabs(camera.target.z - midpoint.z) < 0.0001f &&
                std::fabs(camera.position.x - camera.target.x -
                          basis.offsetX * geometry.depthOffset) <
                    0.0001f &&
                std::fabs(camera.position.z - camera.target.z -
                          basis.offsetZ * geometry.depthOffset) <
                    0.0001f &&
                std::fabs(camera.position.y - camera.target.y -
                          geometry.verticalOffset) < 0.0001f;
        }
    }
    if (!checkTest(coopCameraCentered,
                   "co-op camera did not center the active player midpoint"))
        return 1;

    const float cameraTestAspect = gameplayCameraAspectRatio();
    struct OppositeCoopCameraCase
    {
        int yawDegrees;
        int elevationDegrees;
        XZ first;
        XZ second;
    };
    const std::array<OppositeCoopCameraCase, 6> oppositeCoopCases{{
        {45, kCameraElevationMaximumDegrees,
         {0.875f, 0.875f}, {25.125f, 25.125f}},
        {45, kCameraElevationMinimumDegrees,
         {25.125f, 0.875f}, {0.875f, 25.125f}},
        {0, kCameraElevationMaximumDegrees,
         {13.0f, 0.875f}, {13.0f, 25.125f}},
        {0, kDefaultCameraElevationDegrees,
         {0.875f, 13.0f}, {25.125f, 13.0f}},
        {-45, kCameraElevationMinimumDegrees,
         {25.125f, 0.875f}, {0.875f, 25.125f}},
        {-45, kCameraElevationMaximumDegrees,
         {0.875f, 0.875f}, {25.125f, 25.125f}}}};
    bool oppositeCoopFramingSafe = true;
    for (const OppositeCoopCameraCase &testCase : oppositeCoopCases)
    {
        coopCameraGame.setCameraYawDegrees(testCase.yawDegrees);
        coopCameraGame.setCameraElevationDegrees(
            testCase.elevationDegrees);
        Game3DTestAccess::recenterPlayer(
            coopCameraGame, 0, testCase.first);
        Game3DTestAccess::recenterPlayer(
            coopCameraGame, 1, testCase.second);
        Game3DTestAccess::refreshCameras(coopCameraGame);
        const CameraPlanarBasis basis =
            cameraPlanarBasis(testCase.yawDegrees);
        const GameplayCameraElevationGeometry geometry =
            gameplayCameraElevationGeometry(testCase.elevationDegrees);
        const Camera3D camera = coopCameraGame.cameraForPlayer(0);
        const float horizontalHalfSpan =
            camera.fovy * cameraTestAspect * 0.5f;
        const float verticalHalfSpan = camera.fovy * 0.5f;
        for (const XZ playerPosition :
             std::array<XZ, 2>{{testCase.first, testCase.second}})
        {
            const float remainingRight = std::fabs(
                (playerPosition.x - camera.target.x) * basis.rightX +
                (playerPosition.z - camera.target.z) * basis.rightZ);
            const float projectedDepth = std::fabs(
                (playerPosition.x - camera.target.x) * basis.offsetX +
                (playerPosition.z - camera.target.z) * basis.offsetZ) *
                geometry.groundDepthProjection;
            oppositeCoopFramingSafe = oppositeCoopFramingSafe &&
                remainingRight + 3.0f <= horizontalHalfSpan + 0.01f &&
                projectedDepth + 2.25f <= verticalHalfSpan + 0.01f;
        }
    }
    if (!checkTest(oppositeCoopFramingSafe,
                   "elevated camera span can crop opposite-corner co-op "
                   "tanks at a supported camera yaw and elevation"))
        return 1;

    const std::array<float, 6> cameraAspects{{
        4.0f / 3.0f, 16.0f / 9.0f, 21.0f / 9.0f,
        1.0f, 8.0f / 9.0f, 9.0f / 16.0f}};
    const std::array<XZ, 2> diagonalSeparations{{
        {24.25f, 24.25f}, {24.25f, -24.25f}}};
    bool resizedCoopFramingSafe = true;
    bool landscapeFramingUnchanged = true;
    for (int yawDegrees = kCameraYawMinimumDegrees;
         yawDegrees <= kCameraYawMaximumDegrees;
         yawDegrees += kCameraYawStepDegrees)
    {
        coopCameraGame.setCameraYawDegrees(yawDegrees);
        for (int elevationDegrees = kCameraElevationMinimumDegrees;
             elevationDegrees <= kCameraElevationMaximumDegrees;
             elevationDegrees += kCameraElevationStepDegrees)
        {
            coopCameraGame.setCameraElevationDegrees(elevationDegrees);
            const Camera3D camera = coopCameraGame.cameraForPlayer(0);
            // Derive projection axes from the actual camera pose rather than
            // repeating the span helper's planar yaw/elevation calculation.
            const Vector3 forward = Vector3Normalize(
                Vector3Subtract(camera.target, camera.position));
            const Vector3 right = Vector3Normalize(
                Vector3CrossProduct(forward, camera.up));
            const Vector3 up = Vector3CrossProduct(right, forward);
            for (float aspect : cameraAspects)
            {
                for (const XZ separation : diagonalSeparations)
                {
                    const float span = gameplayCameraSpan(
                        separation, yawDegrees, elevationDegrees, aspect);
                    const Vector3 playerOffset{
                        separation.x * 0.5f, 0.0f,
                        separation.z * 0.5f};
                    const float projectedRight = std::fabs(
                        Vector3DotProduct(playerOffset, right));
                    const float projectedUp = std::fabs(
                        Vector3DotProduct(playerOffset, up));
                    resizedCoopFramingSafe = resizedCoopFramingSafe &&
                        std::isfinite(span) && span >= kSoloCameraSpan &&
                        projectedRight + 3.0f <= span * aspect * 0.5f +
                                                       0.0001f &&
                        projectedUp + 2.25f <= span * 0.5f + 0.0001f;
                    if (aspect == 16.0f / 9.0f)
                    {
                        const float previousSpan = std::clamp(
                            std::max({kSoloCameraSpan,
                                      projectedUp * 2.0f + 4.5f,
                                      (projectedRight * 2.0f + 6.0f) /
                                          aspect}),
                            kSoloCameraSpan, 38.0f);
                        landscapeFramingUnchanged =
                            landscapeFramingUnchanged &&
                            std::fabs(span - previousSpan) < 0.0001f;
                    }
                }
            }
        }
    }
    if (!checkTest(resizedCoopFramingSafe && landscapeFramingUnchanged,
                   "resized co-op view crops a tank or its framing margin, "
                   "or changes the existing 16:9 composition"))
        return 1;
    const float fallbackCameraSpan = gameplayCameraSpan(
        diagonalSeparations[0], 45, 70, 16.0f / 9.0f);
    bool invalidCameraAspectsUseFallback = true;
    for (float aspect : std::array<float, 4>{{
             0.0f, -1.0f, std::numeric_limits<float>::infinity(),
             std::numeric_limits<float>::quiet_NaN()}})
    {
        invalidCameraAspectsUseFallback = invalidCameraAspectsUseFallback &&
            gameplayCameraSpan(diagonalSeparations[0], 45, 70, aspect) ==
                fallbackCameraSpan;
    }
    if (!checkTest(invalidCameraAspectsUseFallback &&
                       gameplayCameraSpan({}, 0, 50, 16.0f / 9.0f) ==
                           kSoloCameraSpan,
                   "camera aspect fallback or minimum local span changed"))
        return 1;

    bool hudRegionsStaySeparate = true;
    for (const Rectangle viewport : std::array<Rectangle, 9>{{
             {0.0f, 0.0f, 1280.0f, 720.0f},
             {0.0f, 0.0f, 800.0f, 900.0f},
             {0.0f, 0.0f, 720.0f, 1280.0f},
             {0.0f, 0.0f, 640.0f, 480.0f},
             {0.0f, 0.0f, 699.0f, 900.0f},
             {0.0f, 0.0f, 700.0f, 900.0f},
             {0.0f, 0.0f, 1024.0f, 768.0f},
             {0.0f, 0.0f, 2560.0f, 1080.0f},
             {31.0f, 23.0f, 800.0f, 900.0f}}})
    {
        for (const int playerCount : {1, 2})
        {
            float previousFooterRight = viewport.x;
            for (int playerIndex = 0; playerIndex < playerCount;
                 ++playerIndex)
            {
                const ViewportHudLayout layout = viewportHudLayout(
                    viewport, playerCount, playerIndex);
                const int panelLeft = layout.panelTextX - 7;
                const int panelRight = panelLeft + layout.panelWidth;
                const int mapLeft = layout.mapX - 4;
                const int mapRight = layout.mapX +
                                     kMapSize * layout.mapCellSize + 4;
                const float footerLeft = layout.footerCenterX -
                                          layout.footerWidth * 0.5f;
                const float footerRight = layout.footerCenterX +
                                           layout.footerWidth * 0.5f;
                hudRegionsStaySeparate = hudRegionsStaySeparate &&
                    layout.panelWidth > 0 && layout.footerWidth > 0 &&
                    panelLeft >= viewport.x &&
                    panelRight <= viewport.x + viewport.width &&
                    mapLeft >= viewport.x &&
                    mapRight <= viewport.x + viewport.width &&
                    (panelRight + 12 <= mapLeft ||
                     mapRight + 12 <= panelLeft) &&
                    footerLeft >= previousFooterRight &&
                    footerRight <= viewport.x + viewport.width;
                previousFooterRight = footerRight;
            }
        }
    }
    const Rectangle defaultHudViewport{0.0f, 0.0f, 1280.0f, 720.0f};
    const ViewportHudLayout defaultSoloHud =
        viewportHudLayout(defaultHudViewport, 1, 0);
    const ViewportHudLayout defaultFirstHud =
        viewportHudLayout(defaultHudViewport, 2, 0);
    const ViewportHudLayout defaultSecondHud =
        viewportHudLayout(defaultHudViewport, 2, 1);
    if (!checkTest(hudRegionsStaySeparate &&
                       defaultSoloHud.panelWidth == 390 &&
                       defaultSoloHud.panelTextX == 14 &&
                       defaultSoloHud.mapX == 1134 &&
                       defaultFirstHud.panelWidth == 390 &&
                       defaultSecondHud.panelWidth == 390 &&
                       defaultFirstHud.panelTextX == 14 &&
                       defaultSecondHud.panelTextX == 876 &&
                       defaultFirstHud.mapX == 575 &&
                       defaultSecondHud.mapX == 575 &&
                       defaultFirstHud.footerCenterX == 320 &&
                       defaultSecondHud.footerCenterX == 960,
                   "HUD panels, minimap, or player hints overlap after resize, "
                   "or the default HUD placement changed"))
        return 1;
    const auto initialSpawnMatches = [](const Player &player, int id,
                                        Nation nation, XZ position) {
        return player.id == id && player.nation == nation &&
               player.position.x == position.x &&
               player.position.z == position.z && player.yaw == 0.0f &&
               player.driveDirection == CardinalDirection::North &&
               player.movementDirection == CardinalDirection::North &&
               player.lives == 3 && player.maximumHitPoints == 3 &&
               player.hitPoints == 3 && player.level == 0 && player.active &&
               !player.moving && !player.hasBoat &&
               player.shieldTimer == 10.0f &&
               player.creationTimer == 1.0f &&
               player.respawnTimer == 0.0f &&
               player.deathTimer == 0.0f &&
               player.fireCooldown == kPlayerReloadTime &&
               player.dustCooldown == 0.0f &&
               player.iceSlipTimer == 0.0f && !player.onIce &&
               player.score == 0 && player.directKillStreak == 0 &&
               player.streakPopupTimer == 0.0f &&
               player.stageTally.totalDestroyed() == 0 &&
               player.stageTally.totalEnemyPoints() == 0 &&
               player.stageTally.bonusPoints == 0 &&
               player.stageTally.scoreAtStageStart == 0;
    };
    if (!checkTest(
            settlementGame.players().size() == 2U &&
                initialSpawnMatches(
                    settlementGame.players()[0], 0,
                    Nation::UnitedStates, {9.0f, 25.0f}) &&
                initialSpawnMatches(
                    settlementGame.players()[1], 1,
                    Nation::SovietUnion, {17.0f, 25.0f}),
            "initial start did not apply the complete reset spawn contract"))
    {
        return 1;
    }
    const bool settlementBeginObserverWasDefaultOff =
        !Game3DTestAccess::hasSettlementBeginPresentationObserver(
            settlementGame);
    std::vector<SettlementBeginPresentationSnapshot>
        settlementShowcaseBeginSnapshots;
    std::vector<AudioCue> settlementShowcaseAudio;
    std::vector<SettlementAudioRequestSnapshot>
        settlementShowcaseAudioSnapshots;
    Game3DTestAccess::captureSettlementBeginPresentation(
        settlementGame, settlementShowcaseBeginSnapshots);
    Game3DTestAccess::captureSettlementAudioRequests(
        settlementGame, settlementShowcaseAudio,
        settlementShowcaseAudioSnapshots);
    settlementGame.spawnSettlementShowcase();
    const bool settlementShowcaseBeginTrace =
        settlementBeginObserverWasDefaultOff &&
        settlementShowcaseBeginSnapshots.size() == 3U &&
        settlementShowcaseBeginSnapshots[0].step ==
            SettlementBeginPresentationStep::PlanReady &&
        settlementShowcaseBeginSnapshots[1].step ==
            SettlementBeginPresentationStep::ReportCommitted &&
        settlementShowcaseBeginSnapshots[2].step ==
            SettlementBeginPresentationStep::AudioStopBoundaryPassed &&
        settlementShowcaseBeginSnapshots[0].plan.kind ==
            SettlementBeginKind::Cleared &&
        !settlementShowcaseBeginSnapshots[0].plan.emitStageEnded &&
        settlementShowcaseBeginSnapshots[0].plan.start.stage == 1 &&
        settlementShowcaseBeginSnapshots[0].plan.start.playerCount == 2 &&
        !settlementShowcaseBeginSnapshots[0].plan.start.gameOver &&
        settlementShowcaseBeginSnapshots[0].plan.start.scores ==
            std::array<int, 2>{{2350, 3200}} &&
        settlementShowcaseBeginSnapshots[0]
                .plan.start.tallies[0].totalDestroyed() == 14 &&
        settlementShowcaseBeginSnapshots[0]
                .plan.start.tallies[1].totalDestroyed() == 6 &&
        settlementShowcaseBeginSnapshots[0].phase ==
            SettlementPhase::None &&
        settlementShowcaseBeginSnapshots[1].phase ==
            SettlementPhase::Counting &&
        settlementShowcaseBeginSnapshots[2].phase ==
            SettlementPhase::Counting &&
        settlementShowcaseBeginSnapshots[0].maximumScore == 0 &&
        settlementShowcaseBeginSnapshots[1].maximumScore == 3200 &&
        settlementShowcaseBeginSnapshots[2].maximumScore == 3200 &&
        settlementShowcaseBeginSnapshots[0].events.empty() &&
        settlementShowcaseBeginSnapshots[1].events.empty() &&
        settlementShowcaseBeginSnapshots[2].events.empty();
    if (!checkTest(settlementShowcaseBeginTrace &&
                       settlementGame.settling() &&
                       settlementGame.settlementCounting() &&
                       settlementGame.eventsThisUpdate().empty() &&
                       settlementGame.settlementTally(0).totalDestroyed() == 14 &&
                       settlementGame.settlementTally(1).totalDestroyed() == 6 &&
                       settlementGame.settlementDisplayedKills(0, 0) == 0 &&
                       settlementGame.settlementDisplayedKills(1, 0) == 0,
                   "battle report did not snapshot separate player/type tallies"))
        return 1;
    settlementGame.update(0.05f, {});
    settlementGame.update(0.05f, {});
    if (!checkTest(
            settlementGame.settlementDisplayedKills(0, 0) == 1 &&
                       settlementGame.settlementDisplayedKills(1, 0) == 1,
                   "battle report did not animate classified kill counts"))
        return 1;
    if (!checkTest(
            settlementShowcaseAudio ==
                    std::vector<AudioCue>({AudioCue::ScoreCounted}) &&
                settlementShowcaseAudioSnapshots.size() == 1U &&
                settlementShowcaseAudioSnapshots[0].cue ==
                    AudioCue::ScoreCounted &&
                settlementShowcaseAudioSnapshots[0].settlementPhase ==
                    SettlementPhase::Counting &&
                settlementShowcaseAudioSnapshots[0]
                        .settlementScoreCounter == 1 &&
                settlementShowcaseAudioSnapshots[0]
                        .settlementCategoryIndex == 0 &&
                settlementShowcaseAudioSnapshots[0]
                        .settlementDisplayedKills[0][0] == 1 &&
                settlementShowcaseAudioSnapshots[0]
                        .settlementDisplayedKills[1][0] == 1,
            "settlement count audio was requested before count state committed"))
        return 1;
    const std::size_t settlementAudioBeforeConfirm =
        settlementShowcaseAudio.size();
    settlementGame.confirmSettlement();
    if (!checkTest(
            settlementShowcaseAudio.size() == settlementAudioBeforeConfirm,
            "skipping classified settlement synthesized count audio"))
        return 1;
    if (!checkTest(!settlementGame.settlementCounting() &&
                       settlementGame.settlementDisplayedKills(0, 0) == 5 &&
                       settlementGame.settlementDisplayedKills(0, 3) == 2 &&
                       settlementGame.settlementDisplayedKills(1, 2) == 2,
                   "battle report skip did not reveal every classified tally"))
        return 1;

    RecordingAudioOutput settlementEventAppendAudio;
    Game3D settlementEventAppendGame(
        resourceRoot, 0x5e771e05U, &settlementEventAppendAudio);
    if (!settlementEventAppendGame.start(1, 3, 1,
                                         settlementNations) ||
        !Game3DTestAccess::prepareGameEventScenario(
            settlementEventAppendGame))
    {
        return 1;
    }
    settlementEventAppendAudio.clear();
    Game3DTestAccess::appendRollbackEvent(settlementEventAppendGame);
    std::vector<SettlementBeginPresentationSnapshot>
        settlementEventAppendSnapshots;
    Game3DTestAccess::captureSettlementBeginPresentation(
        settlementEventAppendGame, settlementEventAppendSnapshots,
        &settlementEventAppendAudio.calls);
    Game3DTestAccess::beginSettlement(
        settlementEventAppendGame, false, true);
    if (!checkTest(
            settlementEventAppendSnapshots.size() == 4U &&
                settlementEventAppendSnapshots[0].events.size() == 1U &&
                settlementEventAppendSnapshots[1].events.size() == 1U &&
                settlementEventAppendSnapshots[2].events.size() == 2U &&
                settlementEventAppendSnapshots[3].events.size() == 2U &&
                settlementEventAppendSnapshots[0]
                        .audioOutputCallCount == 0U &&
                settlementEventAppendSnapshots[1]
                        .audioOutputCallCount == 0U &&
                settlementEventAppendSnapshots[2]
                        .audioOutputCallCount == 0U &&
                settlementEventAppendSnapshots[3]
                        .audioOutputCallCount == 1U &&
                settlementEventAppendAudio.calls ==
                    std::vector<AudioOutputCall>({stopOutputCall()}) &&
                settlementEventAppendSnapshots[2].events[0].type ==
                    GameEventType::BonusSpawned &&
                settlementEventAppendSnapshots[2].events[0].bonusType ==
                    BonusType::Clock &&
                settlementEventAppendSnapshots[2].events[1].type ==
                    GameEventType::StageEnded &&
                settlementEventAppendSnapshots[2]
                        .events[1].stageEndReason ==
                    StageEndReason::Cleared &&
                settlementEventAppendGame.eventsThisUpdate() ==
                    settlementEventAppendSnapshots[3].events,
            "settlement begin cleared or reordered existing update events"))
        return 1;

    if (!Game3DTestAccess::prepareSettlementAdvancePlayerFixture(
            settlementGame))
        return 1;
    const std::vector<Player> playersBeforeSettlementAdvance =
        settlementGame.players();
    const std::vector<Player> playersAfterSettlementProgression =
        expectedSettlementProgression(playersBeforeSettlementAdvance);
    const std::vector<Player> playersAfterSettlementStageEntry =
        expectedSettlementStageEntry(playersBeforeSettlementAdvance);

    const bool settlementObserverWasDefaultOff =
        !Game3DTestAccess::hasSettlementTransitionPresentationObserver(
            settlementGame);
    std::vector<AudioCue> settlementAdvanceAudio;
    std::vector<SettlementAudioRequestSnapshot>
        settlementAdvanceAudioSnapshots;
    std::vector<SettlementTransitionPresentationSnapshot>
        settlementAdvanceSnapshots;
    Game3DTestAccess::captureSettlementAudioRequests(
        settlementGame, settlementAdvanceAudio,
        settlementAdvanceAudioSnapshots);
    Game3DTestAccess::captureSettlementTransitionPresentation(
        settlementGame, settlementAdvanceAudio,
        settlementAdvanceSnapshots);

    for (int frame = 0; frame < 99; ++frame)
        settlementGame.update(0.05f, {});
    settlementGame.update(0.04f, {});
    if (!checkTest(settlementGame.settling() &&
                       settlementGame.stage() == 1,
                   "successful battle report ended before its five-second hold"))
        return 1;
    settlementGame.update(0.02f, {});
    if (!checkTest(!settlementGame.settling() &&
                       Game3DTestAccess::settlementRuntimeReset(
                           settlementGame) &&
                       settlementGame.stage() == 2 &&
                       settlementGame.stageIntro() &&
                       settlementGame.baseAlive() &&
                       settlementPlayerVectorsMatch(
                           settlementGame.players(),
                           playersAfterSettlementStageEntry) &&
                       !settlementGame.consumeMenuRequest(),
                   "successful report did not commit the complete stage-entry player state"))
        return 1;
    if (!checkTest(
            settlementGame.players().size() == 2U &&
                settlementGame.players()[0].directKillStreak == 7 &&
                settlementGame.players()[1].directKillStreak == 4 &&
                settlementGame.players()[0].streakPopupTimer == 0.0f &&
                settlementGame.players()[1].streakPopupTimer == 0.0f &&
                settlementGame.players()[0].stageTally.totalDestroyed() == 0 &&
                settlementGame.players()[1].stageTally.totalDestroyed() == 0 &&
                settlementGame.players()[0].stageTally.totalEnemyPoints() == 0 &&
                settlementGame.players()[1].stageTally.totalEnemyPoints() == 0 &&
                settlementGame.players()[0].stageTally.bonusPoints == 0 &&
                settlementGame.players()[1].stageTally.bonusPoints == 0 &&
                settlementGame.players()[0].stageTally.scoreAtStageStart ==
                    settlementGame.players()[0].score &&
                settlementGame.players()[1].stageTally.scoreAtStageStart ==
                    settlementGame.players()[1].score,
            "stage-preserve spawn did not keep streak while its caller cleared popup and tally"))
    {
        return 1;
    }
    if (!checkTest(
            settlementObserverWasDefaultOff &&
                settlementAdvanceSnapshots.size() == 5U &&
                settlementAdvanceSnapshots[0].step ==
                    SettlementTransitionPresentationStep::PlanReady &&
                settlementAdvanceSnapshots[1].step ==
                    SettlementTransitionPresentationStep::
                        StageCandidateRequested &&
                settlementAdvanceSnapshots[2].step ==
                    SettlementTransitionPresentationStep::
                        StageCandidatePrepared &&
                settlementAdvanceSnapshots[3].step ==
                    SettlementTransitionPresentationStep::
                        PlayerProgressionCommitted &&
                settlementAdvanceSnapshots[4].step ==
                    SettlementTransitionPresentationStep::StageLoadCommitted &&
                settlementAdvanceSnapshots[0].plan.kind ==
                    SettlementTransitionKind::AdvanceStage &&
                settlementAdvanceSnapshots[0].plan.stageBefore == 1 &&
                settlementAdvanceSnapshots[0].plan.stageAfter == 2 &&
                !settlementAdvanceSnapshots[0].settling &&
                settlementAdvanceSnapshots[0].stage == 1 &&
                settlementAdvanceSnapshots[0].mapStage == 1 &&
                settlementPlayerVectorsMatch(
                    settlementAdvanceSnapshots[0].players,
                    playersBeforeSettlementAdvance) &&
                settlementPlayerVectorsMatch(
                    settlementAdvanceSnapshots[0].plan.playersAfter,
                    playersAfterSettlementProgression) &&
                settlementPlayerVectorsMatch(
                    settlementAdvanceSnapshots[1].players,
                    playersBeforeSettlementAdvance) &&
                settlementPlayerVectorsMatch(
                    settlementAdvanceSnapshots[2].players,
                    playersBeforeSettlementAdvance) &&
                settlementAdvanceSnapshots[3].stage == 2 &&
                settlementAdvanceSnapshots[3].mapStage == 1 &&
                settlementPlayerVectorsMatch(
                    settlementAdvanceSnapshots[3].players,
                    playersAfterSettlementProgression) &&
                settlementAdvanceSnapshots[3].audioRequests.empty() &&
                settlementAdvanceSnapshots[4].stage == 2 &&
                settlementAdvanceSnapshots[4].mapStage == 2 &&
                std::fabs(settlementAdvanceSnapshots[4].stageIntroTimer -
                          kStageIntroDuration) < 0.0001f &&
                settlementPlayerVectorsMatch(
                    settlementAdvanceSnapshots[4].players,
                    playersAfterSettlementStageEntry) &&
                settlementAdvanceSnapshots[4].audioRequests ==
                    std::vector<AudioCue>({AudioCue::StageStart}),
            "two-phase settlement advance order or payload drifted"))
        return 1;
    if (!checkTest(
            settlementAdvanceAudioSnapshots.size() == 1U &&
                settlementAdvanceAudioSnapshots[0].cue ==
                    AudioCue::StageStart &&
                settlementAdvanceAudioSnapshots[0].stage == 2 &&
                settlementAdvanceAudioSnapshots[0].mapStage == 2 &&
                settlementAdvanceAudioSnapshots[0].settlementPhase ==
                    SettlementPhase::None &&
                settlementAdvanceAudioSnapshots[0].settlementStage == 2 &&
                settlementAdvanceAudioSnapshots[0].baseAlive &&
                !settlementAdvanceAudioSnapshots[0].gameOver &&
                !settlementAdvanceAudioSnapshots[0].paused &&
                !settlementAdvanceAudioSnapshots[0].highScoreDisplay &&
                !settlementAdvanceAudioSnapshots[0].awaitingMenu &&
                !settlementAdvanceAudioSnapshots[0].menuRequested &&
                std::fabs(settlementAdvanceAudioSnapshots[0]
                              .stageIntroTimer -
                          kStageIntroDuration) < 0.0001f &&
                std::fabs(settlementAdvanceAudioSnapshots[0]
                              .stageTransitionTimer) < 0.0001f &&
                std::fabs(settlementAdvanceAudioSnapshots[0]
                              .gameOverReportTimer) < 0.0001f &&
                settlementAdvanceAudioSnapshots[0].enemiesRemaining ==
                    kEnemiesPerStage &&
                settlementAdvanceAudioSnapshots[0].nextSpawnIndex == 0 &&
                settlementAdvanceAudioSnapshots[0].nextEnemyId == 0 &&
                settlementPlayerVectorsMatch(
                    settlementAdvanceAudioSnapshots[0].players,
                    playersAfterSettlementStageEntry) &&
                settlementAdvanceAudioSnapshots[0].enemyCount == 0U &&
                settlementAdvanceAudioSnapshots[0].shellCount == 0U &&
                settlementAdvanceAudioSnapshots[0].bonusCount == 0U &&
                settlementAdvanceAudioSnapshots[0].effectCount == 0U &&
                settlementAdvanceAudioSnapshots[0].cameraShake ==
                    std::array<float, 2>{} &&
                settlementAdvanceAudioSnapshots[0].bonusMessage.empty() &&
                std::fabs(settlementAdvanceAudioSnapshots[0]
                              .bonusMessageTimer) < 0.0001f,
            "stage-start audio was requested before the new stage was fully committed"))
        return 1;

    Game3D settlementLifeGame(resourceRoot, 0x5e771e01U);
    if (!settlementLifeGame.start(2, 3, kStageCount,
                                  settlementNations))
        return 1;
    settlementLifeGame.spawnSettlementShowcase();
    Game3DTestAccess::setPlayerSettlementState(
        settlementLifeGame, 0, 0, 3, 2350);
    Game3DTestAccess::setPlayerSettlementState(
        settlementLifeGame, 1, 99, 2, 3200);
    Game3DTestAccess::setPlayerHitPoints(settlementLifeGame, 0, 0);
    settlementLifeGame.confirmSettlement();
    settlementLifeGame.confirmSettlement();
    if (!checkTest(
            !settlementLifeGame.settling() &&
                Game3DTestAccess::settlementRuntimeReset(
                    settlementLifeGame) &&
                settlementLifeGame.stage() == 1 &&
                settlementLifeGame.players().size() == 2U &&
                settlementLifeGame.players()[0].lives == 2 &&
                settlementLifeGame.players()[0].level == 0 &&
                settlementLifeGame.players()[0].hitPoints == 3 &&
                settlementLifeGame.players()[1].lives == 99 &&
                settlementLifeGame.players()[1].level == 2 &&
                settlementLifeGame.players()[0].active &&
                settlementLifeGame.players()[1].active &&
                settlementLifeGame.players()[0].position.x == 9.0f &&
                settlementLifeGame.players()[0].position.z == 25.0f &&
                settlementLifeGame.players()[1].position.x == 17.0f &&
                settlementLifeGame.players()[1].position.z == 25.0f &&
                settlementLifeGame.players()[0]
                        .stageTally.totalDestroyed() == 0 &&
                settlementLifeGame.players()[1]
                        .stageTally.totalDestroyed() == 0 &&
                settlementLifeGame.players()[0]
                        .stageTally.totalEnemyPoints() == 0 &&
                settlementLifeGame.players()[1]
                        .stageTally.totalEnemyPoints() == 0 &&
                settlementLifeGame.players()[0]
                        .stageTally.scoreAtStageStart == 2350 &&
                settlementLifeGame.players()[1]
                        .stageTally.scoreAtStageStart == 3200 &&
                !settlementLifeGame.consumeMenuRequest(),
            "stage wrap, dead-player spawn recovery, caller tally reset, or life cap changed"))
        return 1;

    Game3D settlementLoadFailureGame(resourceRoot, 0x5e771e05U);
    if (!settlementLoadFailureGame.start(2, 3, 5, settlementNations))
        return 1;
    settlementLoadFailureGame.spawnSettlementShowcase();
    Game3DTestAccess::setPlayerSettlementState(
        settlementLoadFailureGame, 0, 3, 3, 2350);
    Game3DTestAccess::setPlayerSettlementState(
        settlementLoadFailureGame, 1, 0, 2, 3200);
    if (!Game3DTestAccess::prepareSettlementAdvancePlayerFixture(
            settlementLoadFailureGame))
        return 1;
    const int stageBeforeRejectedLoad = settlementLoadFailureGame.stage();
    const int mapStageBeforeRejectedLoad =
        settlementLoadFailureGame.map().stage();
    const std::vector<Player> playersBeforeRejectedLoad =
        settlementLoadFailureGame.players();
    const std::vector<Player> playersPlannedAfterRejectedLoad =
        expectedSettlementProgression(playersBeforeRejectedLoad);
    Game3DTestAccess::rejectStageLoads(settlementLoadFailureGame);
    std::vector<AudioCue> rejectedSettlementAudio;
    std::vector<SettlementTransitionPresentationSnapshot>
        rejectedSettlementSnapshots;
    Game3DTestAccess::captureAudioRequests(
        settlementLoadFailureGame, rejectedSettlementAudio);
    Game3DTestAccess::captureSettlementTransitionPresentation(
        settlementLoadFailureGame, rejectedSettlementAudio,
        rejectedSettlementSnapshots);
    settlementLoadFailureGame.confirmSettlement();
    settlementLoadFailureGame.confirmSettlement();
    const bool rejectedLoadMenuRequest =
        settlementLoadFailureGame.consumeMenuRequest();
    settlementLoadFailureGame.update(0.05f, {});
    if (!checkTest(
            !settlementLoadFailureGame.settling() &&
                rejectedLoadMenuRequest &&
                !settlementLoadFailureGame.consumeMenuRequest() &&
                settlementLoadFailureGame.eventsThisUpdate().empty() &&
                settlementLoadFailureGame.stage() == stageBeforeRejectedLoad &&
                settlementLoadFailureGame.map().stage() ==
                    mapStageBeforeRejectedLoad &&
                settlementPlayerVectorsMatch(
                    settlementLoadFailureGame.players(),
                    playersBeforeRejectedLoad) &&
                settlementLoadFailureGame.lastError() ==
                    "Injected next-stage validation failure",
            "rejected next stage partially committed complete player state"))
        return 1;
    if (!checkTest(
            rejectedSettlementSnapshots.size() == 4U &&
                rejectedSettlementSnapshots[0].step ==
                    SettlementTransitionPresentationStep::PlanReady &&
                rejectedSettlementSnapshots[1].step ==
                    SettlementTransitionPresentationStep::
                        StageCandidateRequested &&
                rejectedSettlementSnapshots[2].step ==
                    SettlementTransitionPresentationStep::
                        StageCandidateRejected &&
                rejectedSettlementSnapshots[3].step ==
                    SettlementTransitionPresentationStep::MenuRequested &&
                rejectedSettlementSnapshots[0].plan.kind ==
                    SettlementTransitionKind::AdvanceStage &&
                rejectedSettlementSnapshots[0].plan.stageBefore == 5 &&
                rejectedSettlementSnapshots[0].plan.stageAfter == 6 &&
                settlementPlayerVectorsMatch(
                    rejectedSettlementSnapshots[0].players,
                    playersBeforeRejectedLoad) &&
                settlementPlayerVectorsMatch(
                    rejectedSettlementSnapshots[0].plan.playersAfter,
                    playersPlannedAfterRejectedLoad) &&
                settlementPlayerVectorsMatch(
                    rejectedSettlementSnapshots[1].players,
                    playersBeforeRejectedLoad) &&
                rejectedSettlementSnapshots[2].stage == 5 &&
                rejectedSettlementSnapshots[2].mapStage == 5 &&
                settlementPlayerVectorsMatch(
                    rejectedSettlementSnapshots[2].players,
                    playersBeforeRejectedLoad) &&
                rejectedSettlementSnapshots[2].lastError ==
                    "Injected next-stage validation failure" &&
                !rejectedSettlementSnapshots[2].awaitingMenu &&
                !rejectedSettlementSnapshots[2].menuRequested &&
                rejectedSettlementSnapshots[3].stage == 5 &&
                rejectedSettlementSnapshots[3].mapStage == 5 &&
                rejectedSettlementSnapshots[3].awaitingMenu &&
                rejectedSettlementSnapshots[3].menuRequested &&
                settlementPlayerVectorsMatch(
                    rejectedSettlementSnapshots[3].players,
                    playersBeforeRejectedLoad) &&
                rejectedSettlementAudio.empty(),
            "rejected settlement stage mutated live state before menu commit"))
        return 1;

    Game3D wrongStageSettlementGame(resourceRoot, 0x5e771e06U);
    if (!wrongStageSettlementGame.start(2, 3, 5,
                                        settlementNations))
        return 1;
    wrongStageSettlementGame.spawnSettlementShowcase();
    if (!Game3DTestAccess::prepareSettlementAdvancePlayerFixture(
            wrongStageSettlementGame))
        return 1;
    const std::vector<Player> playersBeforeWrongStageLoad =
        wrongStageSettlementGame.players();
    const std::vector<Player> playersPlannedAfterWrongStageLoad =
        expectedSettlementProgression(playersBeforeWrongStageLoad);
    Game3DTestAccess::acceptWrongStageLoads(wrongStageSettlementGame);
    std::vector<AudioCue> wrongStageSettlementAudio;
    std::vector<SettlementTransitionPresentationSnapshot>
        wrongStageSettlementSnapshots;
    Game3DTestAccess::captureAudioRequests(
        wrongStageSettlementGame, wrongStageSettlementAudio);
    Game3DTestAccess::captureSettlementTransitionPresentation(
        wrongStageSettlementGame, wrongStageSettlementAudio,
        wrongStageSettlementSnapshots);
    wrongStageSettlementGame.confirmSettlement();
    wrongStageSettlementGame.confirmSettlement();
    const bool wrongStageMenuRequest =
        wrongStageSettlementGame.consumeMenuRequest();
    if (!checkTest(
            !wrongStageSettlementGame.settling() &&
                wrongStageMenuRequest &&
                !wrongStageSettlementGame.consumeMenuRequest() &&
                wrongStageSettlementGame.stage() == 5 &&
                wrongStageSettlementGame.map().stage() == 5 &&
                settlementPlayerVectorsMatch(
                    wrongStageSettlementGame.players(),
                    playersBeforeWrongStageLoad) &&
                wrongStageSettlementGame.lastError() ==
                    "Stage loader returned stage 1 for requested stage 6" &&
                wrongStageSettlementAudio.empty(),
            "wrong-stage loader result committed or reported the wrong error"))
        return 1;
    if (!checkTest(
            wrongStageSettlementSnapshots.size() == 4U &&
                wrongStageSettlementSnapshots[0].step ==
                    SettlementTransitionPresentationStep::PlanReady &&
                wrongStageSettlementSnapshots[1].step ==
                    SettlementTransitionPresentationStep::
                        StageCandidateRequested &&
                wrongStageSettlementSnapshots[2].step ==
                    SettlementTransitionPresentationStep::
                        StageCandidateRejected &&
                wrongStageSettlementSnapshots[3].step ==
                    SettlementTransitionPresentationStep::MenuRequested &&
                wrongStageSettlementSnapshots[0].plan.stageBefore == 5 &&
                wrongStageSettlementSnapshots[0].plan.stageAfter == 6 &&
                settlementPlayerVectorsMatch(
                    wrongStageSettlementSnapshots[0].plan.playersAfter,
                    playersPlannedAfterWrongStageLoad) &&
                settlementPlayerVectorsMatch(
                    wrongStageSettlementSnapshots[2].players,
                    playersBeforeWrongStageLoad) &&
                wrongStageSettlementSnapshots[2].lastError ==
                    "Stage loader returned stage 1 for requested stage 6" &&
                !wrongStageSettlementSnapshots[2].awaitingMenu &&
                !wrongStageSettlementSnapshots[2].menuRequested &&
                wrongStageSettlementSnapshots[3].awaitingMenu &&
                wrongStageSettlementSnapshots[3].menuRequested &&
                settlementPlayerVectorsMatch(
                    wrongStageSettlementSnapshots[3].players,
                    playersBeforeWrongStageLoad),
            "wrong-stage settlement rejection order or rollback drifted"))
        return 1;

    Game3D restartLoadFailureGame(resourceRoot, 0x5e771e07U);
    if (!restartLoadFailureGame.start(2, 4, 9, settlementNations) ||
        !Game3DTestAccess::prepareGameEventScenario(
            restartLoadFailureGame) ||
        !Game3DTestAccess::prepareSettlementAdvancePlayerFixture(
            restartLoadFailureGame))
        return 1;
    Game3DTestAccess::addEventEnemy(
        restartLoadFailureGame, 91, 2, 3, {8.5f, 6.5f}, true);
    Game3DTestAccess::addEventShell(
        restartLoadFailureGame, ShellOwner::Player, 0, {7.5f, 8.0f},
        {0.0f, -3.0f}, true);
    Game3DTestAccess::addEventPickup(
        restartLoadFailureGame, BonusType::Clock, {12.5f, 10.5f});
    Game3DTestAccess::appendRollbackEvent(restartLoadFailureGame);
    Game3DTestAccess::setCameraShake(restartLoadFailureGame, 0, 0.42f);
    Game3DTestAccess::rejectStageLoads(restartLoadFailureGame);
    std::vector<AudioCue> restartFailureAudio;
    Game3DTestAccess::captureAudioRequests(
        restartLoadFailureGame, restartFailureAudio);
    const SessionDigest digestBeforeRejectedRestart =
        restartLoadFailureGame.sessionDigest();
    const std::vector<GameEvent> eventsBeforeRejectedRestart =
        restartLoadFailureGame.eventsThisUpdate();
    const std::vector<Player> playersBeforeRejectedRestart =
        restartLoadFailureGame.players();
    const int stageBeforeRejectedRestart = restartLoadFailureGame.stage();
    const int mapStageBeforeRejectedRestart =
        restartLoadFailureGame.map().stage();
    const std::size_t enemiesBeforeRejectedRestart =
        restartLoadFailureGame.enemies().size();
    const std::size_t shellsBeforeRejectedRestart =
        restartLoadFailureGame.shells().size();
    const std::size_t bonusesBeforeRejectedRestart =
        restartLoadFailureGame.bonuses().size();
    const std::size_t effectsBeforeRejectedRestart =
        restartLoadFailureGame.effects().activeCount();
    const std::string errorBeforeRejectedStart =
        restartLoadFailureGame.lastError();
    const std::size_t audioBeforeRejectedStart =
        restartFailureAudio.size();
    const std::array<Nation, 2> alternateStartNations{{
        Nation::Germany, Nation::Germany}};
    const AdvancedGameSettings alternateStartSettings{
        6, -30, 30, -25};
    const bool rejectedDifferentStart = !restartLoadFailureGame.start(
        1, 87, 24, alternateStartNations, alternateStartSettings,
        45, kCameraElevationMaximumDegrees);
    if (!checkTest(
            errorBeforeRejectedStart.empty() && rejectedDifferentStart &&
                restartLoadFailureGame.sessionDigest() ==
                    digestBeforeRejectedRestart &&
                restartLoadFailureGame.eventsThisUpdate() ==
                    eventsBeforeRejectedRestart &&
                settlementPlayerVectorsMatch(
                    restartLoadFailureGame.players(),
                    playersBeforeRejectedRestart) &&
                restartLoadFailureGame.stage() ==
                    stageBeforeRejectedRestart &&
                restartLoadFailureGame.map().stage() ==
                    mapStageBeforeRejectedRestart &&
                restartLoadFailureGame.enemies().size() ==
                    enemiesBeforeRejectedRestart &&
                restartLoadFailureGame.shells().size() ==
                    shellsBeforeRejectedRestart &&
                restartLoadFailureGame.bonuses().size() ==
                    bonusesBeforeRejectedRestart &&
                restartLoadFailureGame.effects().activeCount() ==
                    effectsBeforeRejectedRestart &&
                std::fabs(Game3DTestAccess::cameraShake(
                              restartLoadFailureGame, 0) -
                          0.42f) < 0.0001f &&
                Game3DTestAccess::hasSessionConfiguration(
                    restartLoadFailureGame, 2, 4, settlementNations,
                    AdvancedGameSettings{}) &&
                restartLoadFailureGame.cameraYawDegrees() == 0 &&
                restartLoadFailureGame.cameraElevationDegrees() ==
                    kDefaultCameraElevationDegrees &&
                restartLoadFailureGame.lastError() ==
                    "Injected next-stage validation failure" &&
                restartFailureAudio.size() == audioBeforeRejectedStart,
            "rejected start did not restore the prior configuration and complete session"))
        return 1;
    const bool rejectedRestart = !restartLoadFailureGame.restart();
    if (!checkTest(
            rejectedRestart &&
                restartLoadFailureGame.sessionDigest() ==
                    digestBeforeRejectedRestart &&
                restartLoadFailureGame.eventsThisUpdate() ==
                    eventsBeforeRejectedRestart &&
                settlementPlayerVectorsMatch(
                    restartLoadFailureGame.players(),
                    playersBeforeRejectedRestart) &&
                restartLoadFailureGame.stage() ==
                    stageBeforeRejectedRestart &&
                restartLoadFailureGame.map().stage() ==
                    mapStageBeforeRejectedRestart &&
                restartLoadFailureGame.enemies().size() ==
                    enemiesBeforeRejectedRestart &&
                restartLoadFailureGame.shells().size() ==
                    shellsBeforeRejectedRestart &&
                restartLoadFailureGame.bonuses().size() ==
                    bonusesBeforeRejectedRestart &&
                restartLoadFailureGame.effects().activeCount() ==
                    effectsBeforeRejectedRestart &&
                std::fabs(Game3DTestAccess::cameraShake(
                              restartLoadFailureGame, 0) -
                          0.42f) < 0.0001f &&
                restartLoadFailureGame.lastError() ==
                    "Injected next-stage validation failure" &&
                restartFailureAudio.empty(),
            "rejected restart did not atomically restore session, events, players, and world"))
        return 1;

    const SessionDigest digestBeforeRejectedStageChange =
        restartLoadFailureGame.sessionDigest();
    const std::vector<Player> playersBeforeRejectedStageChange =
        restartLoadFailureGame.players();
    const int stageBeforeRejectedStageChange =
        restartLoadFailureGame.stage();
    const int mapStageBeforeRejectedStageChange =
        restartLoadFailureGame.map().stage();
    const std::size_t audioBeforeRejectedStageChange =
        restartFailureAudio.size();
    const bool rejectedStageChange =
        !restartLoadFailureGame.changeStage(1);
    if (!checkTest(
            rejectedStageChange &&
                restartLoadFailureGame.sessionDigest() ==
                    digestBeforeRejectedStageChange &&
                restartLoadFailureGame.eventsThisUpdate().empty() &&
                settlementPlayerVectorsMatch(
                    restartLoadFailureGame.players(),
                    playersBeforeRejectedStageChange) &&
                restartLoadFailureGame.stage() ==
                    stageBeforeRejectedStageChange &&
                restartLoadFailureGame.map().stage() ==
                    mapStageBeforeRejectedStageChange &&
                restartLoadFailureGame.lastError() ==
                    "Injected next-stage validation failure" &&
                restartFailureAudio.size() ==
                    audioBeforeRejectedStageChange,
            "rejected stage change did not preserve state or clear only events"))
        return 1;

    Game3D naturalCountGame(resourceRoot, 0x5e771e04U);
    if (!naturalCountGame.start(1, 3, 1, settlementNations) ||
        !Game3DTestAccess::prepareGameEventScenario(naturalCountGame))
        return 1;
    Game3DTestAccess::setPlayerDirectKillTally(
        naturalCountGame, 0, 2, 50);
    Game3DTestAccess::prepareClearedStageEvent(naturalCountGame);
    std::vector<SettlementBeginPresentationSnapshot>
        naturalCountBeginSnapshots;
    std::vector<AudioCue> naturalCountAudio;
    std::vector<SettlementAudioRequestSnapshot>
        naturalCountAudioSnapshots;
    Game3DTestAccess::captureSettlementBeginPresentation(
        naturalCountGame, naturalCountBeginSnapshots);
    Game3DTestAccess::captureSettlementAudioRequests(
        naturalCountGame, naturalCountAudio,
        naturalCountAudioSnapshots);
    naturalCountGame.update(0.0f, {});
    for (int frame = 0; frame < 99; ++frame)
        naturalCountGame.update(0.05f, {});
    naturalCountGame.update(0.04f, {});
    if (!checkTest(!naturalCountGame.settling(),
                   "cleared-stage activity window ended before five seconds"))
        return 1;
    naturalCountGame.update(0.02f, {});
    const bool naturalCountBeginTrace =
        naturalCountBeginSnapshots.size() == 4U &&
        naturalCountBeginSnapshots[0].step ==
            SettlementBeginPresentationStep::PlanReady &&
        naturalCountBeginSnapshots[1].step ==
            SettlementBeginPresentationStep::ReportCommitted &&
        naturalCountBeginSnapshots[2].step ==
            SettlementBeginPresentationStep::StageEndEventAppended &&
        naturalCountBeginSnapshots[3].step ==
            SettlementBeginPresentationStep::AudioStopBoundaryPassed &&
        naturalCountBeginSnapshots[0].plan.kind ==
            SettlementBeginKind::Cleared &&
        naturalCountBeginSnapshots[0].plan.emitStageEnded &&
        naturalCountBeginSnapshots[0].plan.start.stage == 1 &&
        naturalCountBeginSnapshots[0].plan.start.playerCount == 1 &&
        !naturalCountBeginSnapshots[0].plan.start.gameOver &&
        naturalCountBeginSnapshots[0].plan.start.scores ==
            std::array<int, 2>{{50, 0}} &&
        naturalCountBeginSnapshots[0].phase == SettlementPhase::None &&
        naturalCountBeginSnapshots[1].phase ==
            SettlementPhase::Counting &&
        naturalCountBeginSnapshots[2].phase ==
            SettlementPhase::Counting &&
        naturalCountBeginSnapshots[3].phase ==
            SettlementPhase::Counting &&
        naturalCountBeginSnapshots[1].maximumScore == 50 &&
        naturalCountBeginSnapshots[1].categoryIndex == 0 &&
        naturalCountBeginSnapshots[0].events.empty() &&
        naturalCountBeginSnapshots[1].events.empty() &&
        naturalCountBeginSnapshots[2].events.size() == 1U &&
        naturalCountBeginSnapshots[3].events.size() == 1U &&
        naturalCountBeginSnapshots[2].events[0].type ==
            GameEventType::StageEnded &&
        naturalCountBeginSnapshots[2].events[0].stage == 1 &&
        naturalCountBeginSnapshots[2].events[0].stageEndReason ==
            StageEndReason::Cleared;
    if (!checkTest(
            naturalCountBeginTrace &&
                naturalCountGame.settlementCounting() &&
                naturalCountGame.eventsThisUpdate().size() == 1U &&
                naturalCountGame.eventsThisUpdate()[0].type ==
                    GameEventType::StageEnded &&
                naturalCountGame.eventsThisUpdate()[0].stageEndReason ==
                    StageEndReason::Cleared,
            "cleared stage did not enter classified score counting"))
        return 1;
    for (int frame = 0; frame < 27; ++frame)
        naturalCountGame.update(0.05f, {});
    naturalCountGame.update(0.04f, {});
    if (!checkTest(naturalCountGame.settlementCounting(),
                   "score counting completed before the 1.4-second boundary"))
        return 1;
    naturalCountGame.update(0.02f, {});
    if (!checkTest(
            naturalCountAudio.size() == 14U &&
                naturalCountAudioSnapshots.size() == 14U &&
                std::all_of(
                    naturalCountAudio.begin(), naturalCountAudio.end(),
                    [](AudioCue cue) {
                        return cue == AudioCue::ScoreCounted;
                    }) &&
                naturalCountAudioSnapshots.front().settlementPhase ==
                    SettlementPhase::Counting &&
                naturalCountAudioSnapshots.front()
                        .settlementScoreCounter == 1 &&
                naturalCountAudioSnapshots.front()
                        .settlementCategoryIndex == 3 &&
                naturalCountAudioSnapshots.front()
                        .settlementDisplayedKills[0][2] == 1 &&
                naturalCountAudioSnapshots.back().settlementPhase ==
                    SettlementPhase::Idle &&
                naturalCountAudioSnapshots.back()
                        .settlementScoreCounter == 50 &&
                naturalCountAudioSnapshots.back()
                        .settlementCategoryIndex == kEnemyTypeCount &&
                naturalCountAudioSnapshots.back()
                        .settlementDisplayedKills[0][2] == 1,
            "natural score-count requests did not observe committed report state"))
        return 1;
    if (!checkTest(
            naturalCountGame.settling() &&
                !naturalCountGame.settlementCounting() &&
                naturalCountGame.settlementScoreCounter() == 50 &&
                naturalCountGame.settlementDisplayedKills(0, 0) == 0 &&
                naturalCountGame.settlementDisplayedKills(0, 1) == 0 &&
                naturalCountGame.settlementDisplayedKills(0, 2) == 1 &&
                naturalCountGame.settlementDisplayedKills(0, 3) == 0,
            "natural score counting skipped or misclassified the K.O. row"))
        return 1;
    naturalCountGame.confirmSettlement();
    if (!checkTest(!naturalCountGame.settling() &&
                       naturalCountGame.stage() == 2 &&
                       naturalCountGame.stageIntro(),
                   "naturally completed report did not enter the next stage"))
        return 1;

    Game3D noRecordGame(resourceRoot, 0x5e771e02U);
    if (!noRecordGame.start(1, 3, 1, settlementNations) ||
        !Game3DTestAccess::prepareGameEventScenario(noRecordGame))
        return 1;
    Game3DTestAccess::prepareGameOverReport(
        noRecordGame, true, 2000, {{1999, 0}});
    std::vector<SettlementBeginPresentationSnapshot>
        noRecordBeginSnapshots;
    Game3DTestAccess::captureSettlementBeginPresentation(
        noRecordGame, noRecordBeginSnapshots);
    noRecordGame.update(0.0f, {});
    const bool noRecordBeginTrace =
        noRecordBeginSnapshots.size() == 4U &&
        noRecordBeginSnapshots[0].step ==
            SettlementBeginPresentationStep::PlanReady &&
        noRecordBeginSnapshots[1].step ==
            SettlementBeginPresentationStep::ReportCommitted &&
        noRecordBeginSnapshots[2].step ==
            SettlementBeginPresentationStep::StageEndEventAppended &&
        noRecordBeginSnapshots[3].step ==
            SettlementBeginPresentationStep::AudioStopBoundaryPassed &&
        noRecordBeginSnapshots[0].plan.kind ==
            SettlementBeginKind::PlayersDefeated &&
        noRecordBeginSnapshots[0].plan.emitStageEnded &&
        noRecordBeginSnapshots[0].plan.start.stage == 1 &&
        noRecordBeginSnapshots[0].plan.start.playerCount == 1 &&
        noRecordBeginSnapshots[0].plan.start.gameOver &&
        noRecordBeginSnapshots[0].plan.start.scores ==
            std::array<int, 2>{{1999, 0}} &&
        noRecordBeginSnapshots[0].phase == SettlementPhase::None &&
        !noRecordBeginSnapshots[0].gameOver &&
        noRecordBeginSnapshots[1].phase == SettlementPhase::Counting &&
        noRecordBeginSnapshots[1].gameOver &&
        noRecordBeginSnapshots[1].maximumScore == 1999 &&
        noRecordBeginSnapshots[0].events.empty() &&
        noRecordBeginSnapshots[1].events.empty() &&
        noRecordBeginSnapshots[2].events.size() == 1U &&
        noRecordBeginSnapshots[3].events.size() == 1U &&
        noRecordBeginSnapshots[2].events[0].stage == 1 &&
        noRecordBeginSnapshots[2].events[0].stageEndReason ==
            StageEndReason::PlayersDefeated;
    if (!checkTest(
            noRecordBeginTrace && noRecordGame.settling() &&
                noRecordGame.settlementWasGameOver() &&
                noRecordGame.eventsThisUpdate().size() == 1U &&
                noRecordGame.eventsThisUpdate()[0].type ==
                    GameEventType::StageEnded &&
                noRecordGame.eventsThisUpdate()[0].stageEndReason ==
                    StageEndReason::PlayersDefeated,
            "defeat did not enter the game-over report with its reason"))
        return 1;
    std::vector<AudioCue> noRecordSettlementAudio;
    std::vector<SettlementTransitionPresentationSnapshot>
        noRecordSettlementSnapshots;
    Game3DTestAccess::captureAudioRequests(
        noRecordGame, noRecordSettlementAudio);
    Game3DTestAccess::captureSettlementTransitionPresentation(
        noRecordGame, noRecordSettlementAudio,
        noRecordSettlementSnapshots);
    noRecordGame.confirmSettlement();
    for (int frame = 0; frame < 99; ++frame)
        noRecordGame.update(0.05f, {});
    noRecordGame.update(0.04f, {});
    if (!checkTest(noRecordGame.settling(),
                   "game-over report ended before its five-second hold"))
        return 1;
    noRecordGame.update(0.02f, {});
    noRecordGame.update(0.05f, {});
    const bool frozeBeforeMenuConsumption =
        !noRecordGame.settling() &&
        noRecordGame.eventsThisUpdate().empty();
    const bool noRecordMenuRequest = noRecordGame.consumeMenuRequest();
    noRecordGame.update(0.05f, {});
    const bool frozeAfterMenuConsumption =
        !noRecordGame.settling() &&
        noRecordGame.eventsThisUpdate().empty();
    if (!checkTest(
            frozeBeforeMenuConsumption && frozeAfterMenuConsumption &&
                Game3DTestAccess::settlementRuntimeReset(noRecordGame) &&
                noRecordGame.settlementHighScore() == 2000 &&
                !noRecordGame.highScoreDisplay() && noRecordMenuRequest &&
                !noRecordGame.consumeMenuRequest() &&
                noRecordGame.stage() == 1,
            "non-record defeat did not return once to the menu"))
        return 1;
    if (!checkTest(
            noRecordSettlementSnapshots.size() == 3U &&
                noRecordSettlementSnapshots[0].step ==
                    SettlementTransitionPresentationStep::PlanReady &&
                noRecordSettlementSnapshots[1].step ==
                    SettlementTransitionPresentationStep::HighScoreCommitted &&
                noRecordSettlementSnapshots[2].step ==
                    SettlementTransitionPresentationStep::MenuRequested &&
                noRecordSettlementSnapshots[0].plan.kind ==
                    SettlementTransitionKind::ReturnToMenu &&
                noRecordSettlementSnapshots[0].plan.highScoreBefore == 2000 &&
                noRecordSettlementSnapshots[0].plan.highScoreAfter == 2000 &&
                !noRecordSettlementSnapshots[0].settling &&
                !noRecordSettlementSnapshots[1].highScoreDisplay &&
                !noRecordSettlementSnapshots[1].awaitingMenu &&
                noRecordSettlementSnapshots[2].awaitingMenu &&
                noRecordSettlementSnapshots[2].menuRequested &&
                noRecordSettlementAudio.empty(),
            "non-record settlement high-score/menu order drifted"))
        return 1;

    Game3D recordGame(resourceRoot, 0x5e771e03U);
    if (!recordGame.start(2, 3, 1, settlementNations) ||
        !Game3DTestAccess::prepareGameEventScenario(recordGame))
        return 1;
    Game3DTestAccess::prepareGameOverReport(
        recordGame, false, 2000, {{2100, 2600}});
    std::vector<SettlementBeginPresentationSnapshot>
        recordBeginSnapshots;
    Game3DTestAccess::captureSettlementBeginPresentation(
        recordGame, recordBeginSnapshots);
    recordGame.update(0.0f, {});
    const bool baseLossReport =
        recordBeginSnapshots.size() == 4U &&
        recordBeginSnapshots[0].step ==
            SettlementBeginPresentationStep::PlanReady &&
        recordBeginSnapshots[1].step ==
            SettlementBeginPresentationStep::ReportCommitted &&
        recordBeginSnapshots[2].step ==
            SettlementBeginPresentationStep::StageEndEventAppended &&
        recordBeginSnapshots[3].step ==
            SettlementBeginPresentationStep::AudioStopBoundaryPassed &&
        recordBeginSnapshots[0].plan.kind ==
            SettlementBeginKind::BaseDestroyed &&
        recordBeginSnapshots[0].plan.emitStageEnded &&
        recordBeginSnapshots[0].plan.start.stage == 1 &&
        recordBeginSnapshots[0].plan.start.playerCount == 2 &&
        recordBeginSnapshots[0].plan.start.gameOver &&
        recordBeginSnapshots[0].plan.start.scores ==
            std::array<int, 2>{{2100, 2600}} &&
        recordBeginSnapshots[0].phase == SettlementPhase::None &&
        !recordBeginSnapshots[0].gameOver &&
        recordBeginSnapshots[1].phase == SettlementPhase::Counting &&
        recordBeginSnapshots[1].gameOver &&
        recordBeginSnapshots[1].maximumScore == 2600 &&
        recordBeginSnapshots[0].events.empty() &&
        recordBeginSnapshots[1].events.empty() &&
        recordBeginSnapshots[2].events.size() == 1U &&
        recordBeginSnapshots[3].events.size() == 1U &&
        recordBeginSnapshots[2].events[0].stage == 1 &&
        recordBeginSnapshots[2].events[0].stageEndReason ==
            StageEndReason::BaseDestroyed &&
        recordGame.eventsThisUpdate().size() == 1U &&
        recordGame.eventsThisUpdate()[0].type ==
            GameEventType::StageEnded &&
        recordGame.eventsThisUpdate()[0].stageEndReason ==
            StageEndReason::BaseDestroyed;
    std::vector<AudioCue> recordSettlementAudio;
    std::vector<SettlementTransitionPresentationSnapshot>
        recordSettlementSnapshots;
    Game3DTestAccess::captureAudioRequests(
        recordGame, recordSettlementAudio);
    Game3DTestAccess::captureSettlementTransitionPresentation(
        recordGame, recordSettlementAudio, recordSettlementSnapshots);
    recordGame.confirmSettlement();
    recordGame.confirmSettlement();
    const bool prematureRecordMenuRequest = recordGame.consumeMenuRequest();
    if (!checkTest(
            baseLossReport && !recordGame.settling() &&
                Game3DTestAccess::settlementRuntimeReset(recordGame) &&
                recordGame.settlementHighScore() == 2600 &&
                recordGame.highScoreDisplay() &&
                !prematureRecordMenuRequest,
            "record defeat did not select the best player score"))
        return 1;
    if (!checkTest(
            recordSettlementSnapshots.size() == 4U &&
                recordSettlementSnapshots[0].step ==
                    SettlementTransitionPresentationStep::PlanReady &&
                recordSettlementSnapshots[1].step ==
                    SettlementTransitionPresentationStep::HighScoreCommitted &&
                recordSettlementSnapshots[2].step ==
                    SettlementTransitionPresentationStep::
                        HighScoreAudioRequested &&
                recordSettlementSnapshots[3].step ==
                    SettlementTransitionPresentationStep::
                        HighScoreDisplayCommitted &&
                recordSettlementSnapshots[0].plan.kind ==
                    SettlementTransitionKind::ShowHighScore &&
                recordSettlementSnapshots[0].plan.highScoreBefore == 2000 &&
                recordSettlementSnapshots[0].plan.highScoreAfter == 2600 &&
                recordSettlementSnapshots[0].highScore == 2000 &&
                !recordSettlementSnapshots[0].settling &&
                recordSettlementSnapshots[1].highScore == 2600 &&
                recordSettlementSnapshots[1].audioRequests.empty() &&
                !recordSettlementSnapshots[1].highScoreDisplay &&
                recordSettlementSnapshots[2].audioRequests ==
                    std::vector<AudioCue>({AudioCue::HighScoreBeaten}) &&
                !recordSettlementSnapshots[2].highScoreDisplay &&
                recordSettlementSnapshots[3].highScoreDisplay &&
                std::fabs(recordSettlementSnapshots[3]
                              .highScoreDisplayTimer) < 0.0001f &&
                !recordSettlementSnapshots[3].awaitingMenu &&
                !recordSettlementSnapshots[3].menuRequested,
            "record settlement high-score/audio/display order drifted"))
        return 1;
    recordGame.confirmSettlement();
    if (!checkTest(!recordGame.highScoreDisplay() &&
                       recordGame.consumeMenuRequest() &&
                       !recordGame.consumeMenuRequest(),
                   "record confirmation did not return once to the menu"))
        return 1;

    if (!recordGame.start(2, 3, 1, settlementNations) ||
        !Game3DTestAccess::prepareGameEventScenario(recordGame))
        return 1;
    Game3DTestAccess::prepareGameOverReport(
        recordGame, true, 2600, {{2700, 2900}});
    recordGame.update(0.0f, {});
    recordGame.confirmSettlement();
    recordGame.confirmSettlement();
    for (int frame = 0; frame < 103; ++frame)
        recordGame.update(0.05f, {});
    recordGame.update(0.04f, {});
    const bool recordStillVisible = recordGame.highScoreDisplay();
    const bool earlyTimeoutMenuRequest = recordGame.consumeMenuRequest();
    recordGame.update(0.02f, {});
    if (!checkTest(
            recordStillVisible && !earlyTimeoutMenuRequest &&
                !recordGame.highScoreDisplay() &&
                recordGame.settlementHighScore() == 2900 &&
                recordGame.consumeMenuRequest() &&
                !recordGame.consumeMenuRequest(),
            "record screen timeout drifted from 5.2 seconds"))
        return 1;

    return 0;
}

int runMovementAndEnemyEscapeSelfTests(const fs::path &resourceRoot)
{
    std::string error;
    for (CardinalDirection direction : {CardinalDirection::North,
                                        CardinalDirection::South,
                                        CardinalDirection::West,
                                        CardinalDirection::East})
    {
        const XZ axis = cardinalVector(direction);
        const XZ facing = forwardFromYaw(cardinalYaw(direction));
        const bool singleAxis = (std::fabs(axis.x) == 1.0f && axis.z == 0.0f) ||
                                (std::fabs(axis.z) == 1.0f && axis.x == 0.0f);
        if (!checkTest(singleAxis && distanceSquared(axis, facing) < 0.00001f,
                       "cardinal movement produced a diagonal or mismatched facing"))
            return 1;
    }
    if (!checkTest(cardinalToward({0.0f, 0.0f}, {4.0f, 1.0f}) ==
                           CardinalDirection::East &&
                       cardinalToward({0.0f, 0.0f}, {1.0f, -4.0f}) ==
                           CardinalDirection::North,
                   "cardinal target selection is incorrect"))
        return 1;
    if (!checkTest(std::fabs(snappedToCardinalLane({4.18f, 7.73f},
                                                   CardinalDirection::North).x - 4.0f) <
                           0.001f &&
                       std::fabs(snappedToCardinalLane({4.18f, 7.73f},
                                                       CardinalDirection::East).z - 8.0f) <
                           0.001f,
                   "cardinal lane snapping is incorrect"))
        return 1;

    StageMap enemyEscapeMap;
    if (!enemyEscapeMap.load(resourceRoot, 2, error))
        return 1;
    const auto terrainAvailable = [&](XZ candidate) {
        return !enemyEscapeMap.collidesWithTank(candidate, kTankRadius);
    };
    const XZ deadEndPosition{21.0f, 3.0f};
    const EnemyEscapeChoice deadEndChoice = chooseEnemyEscape(
        deadEndPosition, CardinalDirection::South, kGovernmentBaseCenter, 0,
        terrainAvailable);
    const XZ cornerPosition{13.0f, 4.0f};
    const EnemyEscapeChoice cornerChoice = chooseEnemyEscape(
        cornerPosition, CardinalDirection::South, kGovernmentBaseCenter, 1,
        terrainAvailable);
    const EnemyEscapeChoice sealedChoice = chooseEnemyEscape(
        {8.0f, 8.0f}, CardinalDirection::South, kGovernmentBaseCenter, 0,
        [](XZ) { return false; });
    const auto directionOpenFrom = [&](XZ position,
                                       CardinalDirection direction) {
        return terrainAvailable(position + cardinalVector(direction) *
                                 kEnemyEscapeProbeStep);
    };
    if (!checkTest(
            terrainAvailable(deadEndPosition) &&
                !directionOpenFrom(deadEndPosition, CardinalDirection::South) &&
                !directionOpenFrom(deadEndPosition, CardinalDirection::East) &&
                !directionOpenFrom(deadEndPosition, CardinalDirection::West) &&
                directionOpenFrom(deadEndPosition, CardinalDirection::North) &&
                deadEndChoice.direction == CardinalDirection::North &&
                deadEndChoice.clearProbeCount > 0 &&
                terrainAvailable(deadEndChoice.alignedPosition) &&
                terrainAvailable(cornerPosition) &&
                !directionOpenFrom(cornerPosition, CardinalDirection::South) &&
                !directionOpenFrom(cornerPosition, CardinalDirection::East) &&
                directionOpenFrom(cornerPosition, CardinalDirection::North) &&
                directionOpenFrom(cornerPosition, CardinalDirection::West) &&
                (cornerChoice.direction == CardinalDirection::North ||
                 cornerChoice.direction == CardinalDirection::West) &&
                !sealedChoice.available(),
            "enemy local escape chose blocked terrain or failed a dead end"))
        return 1;

    const std::array<Nation, 2> nations{{Nation::UnitedStates,
                                         Nation::SovietUnion}};
    Game3D escapeGame(resourceRoot, 0xe2000001U);
    if (!escapeGame.start(1, 3, 2, nations))
        return 1;
    Game3DTestAccess::setEnemyTargetPlayerState(
        escapeGame, 0, false, {13.0f, 10.0f});
    Enemy escapingEnemy;
    escapingEnemy.id = 601;
    escapingEnemy.position = deadEndPosition;
    escapingEnemy.target = kGovernmentBaseCenter;
    escapingEnemy.driveDirection = CardinalDirection::South;
    escapingEnemy.movementDirection = CardinalDirection::South;
    escapingEnemy.yaw = cardinalYaw(CardinalDirection::South);
    escapingEnemy.armor = 2;
    escapingEnemy.blockedTimer = 0.45f;
    escapingEnemy.directionDecisionInterval = 0.83f;
    escapingEnemy.movementDelay = 1.0f;
    escapingEnemy.fireCooldown = 1.0f;
    escapingEnemy.iceSlipTimer = 0.37f;
    Game3DTestAccess::installEnemyUpdateScenario(escapeGame, escapingEnemy);
    constexpr float kEscapeIntervalRoll = 0.25f;
    ScriptedRandomSource escapeRandom(
        {ScriptedRandomSource::real(kEscapeIntervalRoll)},
        [&]() {
            return escapeGame.enemies().size() == 1U &&
                   distanceSquared(escapeGame.enemies()[0].position,
                                   deadEndPosition) == 0.0f &&
                   escapeGame.enemies()[0].target.x ==
                       kGovernmentBaseCenter.x &&
                   escapeGame.enemies()[0].target.z ==
                       kGovernmentBaseCenter.z &&
                   escapeGame.enemies()[0].driveDirection ==
                       CardinalDirection::North &&
                   escapeGame.enemies()[0].movementDirection ==
                       CardinalDirection::North &&
                   escapeGame.enemies()[0].yaw ==
                       cardinalYaw(CardinalDirection::North) &&
                   escapeGame.enemies()[0].iceSlipTimer == 0.0f &&
                   escapeGame.enemies()[0].directionTimer == 0.0f &&
                   escapeGame.enemies()[0].blockedTimer ==
                       escapingEnemy.blockedTimer &&
                   escapeGame.enemies()[0].directionDecisionInterval ==
                       escapingEnemy.directionDecisionInterval;
        });
    Game3DTestAccess::updateEnemies(escapeGame, 0.0f, escapeRandom);
    const Enemy escapedEnemy = escapeGame.enemies().front();
    const float expectedEscapeInterval = kEnemyEscapeCommitTime +
        kEscapeIntervalRoll *
            tanks3d::game::kEnemyEscapeDecisionIntervalRange;

    Enemy sealedEnemy = escapingEnemy;
    sealedEnemy.id = 602;
    sealedEnemy.position = {-10.0f, -10.0f};
    sealedEnemy.driveDirection = CardinalDirection::East;
    sealedEnemy.movementDirection = CardinalDirection::East;
    sealedEnemy.yaw = cardinalYaw(CardinalDirection::East);
    sealedEnemy.directionTimer = 0.07f;
    sealedEnemy.directionDecisionInterval = 0.83f;
    sealedEnemy.iceSlipTimer = 0.0f;
    Game3DTestAccess::installEnemyUpdateScenario(escapeGame, sealedEnemy);
    const Enemy sealedBefore = escapeGame.enemies().front();
    ScriptedRandomSource sealedRandom({});
    Game3DTestAccess::updateEnemies(escapeGame, 0.0f, sealedRandom);
    const Enemy &sealedAfter = escapeGame.enemies().front();
    const bool sealedFailureCommitted =
        sealedAfter.id == sealedBefore.id &&
        distanceSquared(sealedAfter.position, sealedBefore.position) == 0.0f &&
        distanceSquared(sealedAfter.target, sealedBefore.target) == 0.0f &&
        sealedAfter.yaw == sealedBefore.yaw &&
        sealedAfter.driveDirection == sealedBefore.driveDirection &&
        sealedAfter.movementDirection == sealedBefore.movementDirection &&
        sealedAfter.type == sealedBefore.type &&
        sealedAfter.armor == sealedBefore.armor &&
        sealedAfter.carriesBonus == sealedBefore.carriesBonus &&
        sealedAfter.destroyed == sealedBefore.destroyed &&
        sealedAfter.moving == sealedBefore.moving &&
        sealedAfter.frozenTimer == sealedBefore.frozenTimer &&
        sealedAfter.fireCooldown == sealedBefore.fireCooldown &&
        sealedAfter.creationTimer == sealedBefore.creationTimer &&
        sealedAfter.deathTimer == sealedBefore.deathTimer &&
        sealedAfter.blockedTimer == sealedBefore.blockedTimer &&
        sealedAfter.dustCooldown == sealedBefore.dustCooldown &&
        sealedAfter.directionTimer == 0.0f &&
        sealedAfter.directionDecisionInterval ==
            tanks3d::game::kEnemyDirectionDecisionMinimumInterval &&
        sealedAfter.movementDelay == sealedBefore.movementDelay &&
        sealedAfter.iceSlipTimer == sealedBefore.iceSlipTimer &&
        sealedAfter.onIce == sealedBefore.onIce;
    if (!checkTest(
            escapeRandom.complete() && sealedRandom.complete() &&
                escapedEnemy.position.x == deadEndPosition.x &&
                escapedEnemy.position.z == deadEndPosition.z &&
                escapedEnemy.driveDirection == CardinalDirection::North &&
                escapedEnemy.movementDirection == CardinalDirection::North &&
                escapedEnemy.yaw == cardinalYaw(CardinalDirection::North) &&
                escapedEnemy.iceSlipTimer == 0.0f &&
                escapedEnemy.blockedTimer == 0.0f &&
                std::fabs(escapedEnemy.directionDecisionInterval -
                          expectedEscapeInterval) < 0.00001f &&
                escapedEnemy.armor == escapingEnemy.armor &&
                sealedFailureCommitted,
            "Game3D update path did not commit one complete escape before its "
            "single interval draw or mishandled a sealed retry: " +
                escapeRandom.diagnostic() + sealedRandom.diagnostic()))
        return 1;

    return 0;
}

int runEnemyProductionPathCharacterizationSelfTests(
    const fs::path &resourceRoot)
{
    const std::array<Nation, 2> nations{{Nation::UnitedStates,
                                         Nation::SovietUnion}};
    const auto positionsMatch = [](XZ first, XZ second) {
        return distanceSquared(first, second) < 0.000001f;
    };

    Game3D targetGame(resourceRoot, 0xe3000001U);
    if (!checkTest(targetGame.start(2, 3, 1, nations) &&
                       Game3DTestAccess::prepareGameEventScenario(targetGame),
                   targetGame.lastError()))
        return 1;
    Enemy targetEnemy;
    targetEnemy.type = 0;
    targetEnemy.position = {13.0f, 10.0f};

    Game3DTestAccess::setEnemyTargetPlayerState(
        targetGame, 0, false, {13.0f, 10.1f});
    Game3DTestAccess::setEnemyTargetPlayerState(
        targetGame, 1, true, {13.0f, -4.0f});
    if (!checkTest(
            positionsMatch(
                Game3DTestAccess::chooseEnemyTarget(targetGame, targetEnemy),
                kGovernmentBaseCenter),
            "enemy targeting did not ignore an inactive nearer player and "
            "an active player farther than the base"))
        return 1;

    const XZ firstEqualPlayer{14.0f, 10.0f};
    Game3DTestAccess::setEnemyTargetPlayerState(
        targetGame, 0, true, firstEqualPlayer);
    Game3DTestAccess::setEnemyTargetPlayerState(
        targetGame, 1, true, {12.0f, 10.0f});
    if (!checkTest(
            positionsMatch(
                Game3DTestAccess::chooseEnemyTarget(targetGame, targetEnemy),
                firstEqualPlayer),
            "equal-distance enemy targets no longer preserve the first "
            "active player"))
        return 1;

    const XZ closerSecondPlayer{14.0f, 10.0f};
    Game3DTestAccess::setEnemyTargetPlayerState(
        targetGame, 0, true, {18.0f, 10.0f});
    Game3DTestAccess::setEnemyTargetPlayerState(
        targetGame, 1, true, closerSecondPlayer);
    if (!checkTest(
            positionsMatch(
                Game3DTestAccess::chooseEnemyTarget(targetGame, targetEnemy),
                closerSecondPlayer),
            "a strictly closer later player did not replace the earlier "
            "enemy target"))
        return 1;

    Game3D steeringGame(resourceRoot, 0xe3000002U);
    if (!checkTest(steeringGame.start(2, 3, 1, nations),
                   steeringGame.lastError()))
        return 1;
    const float belowTargetThreshold = std::nextafter(
        0.8f, -std::numeric_limits<float>::infinity());
    const float belowAxisThreshold = std::nextafter(
        0.7f, -std::numeric_limits<float>::infinity());
    const auto runSteeringCase = [&](XZ playerPosition,
                                     std::vector<ScriptedRandomSource::Step> steps,
                                     CardinalDirection expectedDirection,
                                     std::string &diagnostic) {
        if (!Game3DTestAccess::prepareGameEventScenario(steeringGame))
        {
            diagnostic = "unable to prepare steering scenario";
            return false;
        }
        Game3DTestAccess::setEnemyTargetPlayerState(
            steeringGame, 0, true, playerPosition);
        Game3DTestAccess::setEnemyTargetPlayerState(
            steeringGame, 1, false, playerPosition);
        Enemy enemy;
        enemy.id = 301;
        enemy.type = 0;
        enemy.position = {13.0f, 10.0f};
        enemy.driveDirection = CardinalDirection::South;
        enemy.movementDirection = CardinalDirection::South;
        enemy.directionTimer = 0.11f;
        enemy.directionDecisionInterval = 0.10f;
        enemy.movementDelay = 1.0f;
        enemy.fireCooldown = 1.0f;
        Game3DTestAccess::installEnemyUpdateScenario(steeringGame, enemy);

        ScriptedRandomSource random(std::move(steps));
        Game3DTestAccess::updateEnemies(steeringGame, 0.0f, random);
        if (!random.complete())
        {
            diagnostic = random.diagnostic();
            return false;
        }
        if (steeringGame.enemies().size() != 1U)
        {
            diagnostic = "steering update removed the live enemy";
            return false;
        }
        const Enemy &updated = steeringGame.enemies().front();
        if (updated.driveDirection != expectedDirection ||
            std::fabs(updated.directionDecisionInterval - 0.30f) > 0.00001f)
        {
            diagnostic = "steering direction or first-draw interval changed";
            return false;
        }
        return true;
    };

    std::string steeringDiagnostic;
    if (!checkTest(
            runSteeringCase(
                {20.0f, 11.0f},
                {ScriptedRandomSource::real(0.25f),
                 ScriptedRandomSource::real(belowTargetThreshold),
                 ScriptedRandomSource::real(belowAxisThreshold)},
                CardinalDirection::East, steeringDiagnostic),
            "horizontal-major east steering consumed or applied the wrong "
            "random draws: " + steeringDiagnostic))
        return 1;
    steeringDiagnostic.clear();
    if (!checkTest(
            runSteeringCase(
                {6.0f, 9.0f},
                {ScriptedRandomSource::real(0.25f),
                 ScriptedRandomSource::real(belowTargetThreshold),
                 ScriptedRandomSource::real(belowAxisThreshold)},
                CardinalDirection::West, steeringDiagnostic),
            "horizontal-major west steering consumed or applied the wrong "
            "random draws: " + steeringDiagnostic))
        return 1;
    steeringDiagnostic.clear();
    if (!checkTest(
            runSteeringCase(
                {20.0f, 9.0f},
                {ScriptedRandomSource::real(0.25f),
                 ScriptedRandomSource::real(belowTargetThreshold),
                 ScriptedRandomSource::real(0.7f)},
                CardinalDirection::North, steeringDiagnostic),
            "the strict 0.7 horizontal-axis threshold or draw order changed: " +
                steeringDiagnostic))
        return 1;
    steeringDiagnostic.clear();
    if (!checkTest(
            runSteeringCase(
                {20.0f, 11.0f},
                {ScriptedRandomSource::real(0.25f),
                 ScriptedRandomSource::real(0.8f),
                 ScriptedRandomSource::integer(0, 3, 3)},
                CardinalDirection::West, steeringDiagnostic),
            "the strict 0.8 target threshold did not switch the third draw "
                "from real to integer: " + steeringDiagnostic))
        return 1;

    Game3D drawOrderGame(resourceRoot, 0xe3000005U);
    if (!checkTest(drawOrderGame.start(1, 3, 1, nations),
                   drawOrderGame.lastError()))
        return 1;
    Game3DTestAccess::setEnemyTargetPlayerState(
        drawOrderGame, 0, true, {20.0f, 11.0f});
    Enemy targetAndFireEnemy;
    targetAndFireEnemy.id = 451;
    targetAndFireEnemy.type = 0;
    targetAndFireEnemy.position = {13.0f, 10.0f};
    targetAndFireEnemy.driveDirection = CardinalDirection::South;
    targetAndFireEnemy.movementDirection = CardinalDirection::South;
    targetAndFireEnemy.directionTimer = 0.11f;
    targetAndFireEnemy.directionDecisionInterval = 0.10f;
    targetAndFireEnemy.movementDelay = 1.0f;
    targetAndFireEnemy.fireCooldown = 0.0f;
    Game3DTestAccess::installEnemyUpdateScenario(
        drawOrderGame, targetAndFireEnemy);

    Enemy wanderAndFireEnemy = targetAndFireEnemy;
    wanderAndFireEnemy.id = 452;
    wanderAndFireEnemy.position = {8.0f, 10.0f};
    Game3DTestAccess::appendEnemyUpdateScenario(
        drawOrderGame, wanderAndFireEnemy);
    ScriptedRandomSource interleavedRandom({
        ScriptedRandomSource::real(0.25f),
        ScriptedRandomSource::real(belowTargetThreshold),
        ScriptedRandomSource::real(belowAxisThreshold),
        ScriptedRandomSource::real(0.375f),
        ScriptedRandomSource::real(0.50f),
        ScriptedRandomSource::real(0.8f),
        ScriptedRandomSource::integer(0, 3, 3),
        ScriptedRandomSource::real(0.625f)});
    Game3DTestAccess::updateEnemies(drawOrderGame, 0.0f,
                                    interleavedRandom);
    const bool twoEnemyDrawOrderPreserved =
        interleavedRandom.complete() && drawOrderGame.enemies().size() == 2U &&
        drawOrderGame.enemies()[0].driveDirection == CardinalDirection::East &&
        drawOrderGame.enemies()[1].driveDirection == CardinalDirection::West &&
        drawOrderGame.enemies()[0].fireCooldown == 0.375f &&
        drawOrderGame.enemies()[1].fireCooldown == 0.625f &&
        drawOrderGame.shells().size() == 2U &&
        drawOrderGame.shells()[0].ownerIndex == 451 &&
        drawOrderGame.shells()[1].ownerIndex == 452 &&
        drawOrderGame.eventsThisUpdate().size() == 2U;
    if (!checkTest(
            twoEnemyDrawOrderPreserved,
            "enemy steering/fire draws stopped interleaving per enemy as "
            "R-R-R-R then R-R-I-R: " + interleavedRandom.diagnostic()))
        return 1;

    Game3D movingFireGame(resourceRoot, 0xe3000007U);
    if (!checkTest(
            movingFireGame.start(1, 3, 1, nations) &&
                Game3DTestAccess::prepareGameEventScenario(movingFireGame),
            movingFireGame.lastError()))
        return 1;
    Enemy movingFireEnemy;
    movingFireEnemy.id = 453;
    movingFireEnemy.type = 0;
    movingFireEnemy.position = {13.0f, 10.0f};
    movingFireEnemy.driveDirection = CardinalDirection::East;
    movingFireEnemy.movementDirection = CardinalDirection::East;
    movingFireEnemy.directionTimer = 0.0f;
    movingFireEnemy.directionDecisionInterval = 100.0f;
    movingFireEnemy.movementDelay = 0.0f;
    movingFireEnemy.fireCooldown = 0.0f;
    movingFireEnemy.blockedTimer = 0.25f;
    movingFireEnemy.dustCooldown = 0.0f;
    Game3DTestAccess::installEnemyUpdateScenario(
        movingFireGame, movingFireEnemy);
    // Keep a round from this gun alive so the due fire decision still consumes
    // its reload draw without adding muzzle particles. That leaves the single
    // track-dust particle independently observable through the production FX
    // wrapper.
    Game3DTestAccess::addEventShell(
        movingFireGame, ShellOwner::Enemy, movingFireEnemy.id,
        {1.0f, 1.0f}, cardinalVector(CardinalDirection::South));
    constexpr float kMovingFireStep = 0.05f;
    constexpr float kMovingFireReloadRoll = 0.375f;
    ScriptedRandomSource movingFireRandom(
        {ScriptedRandomSource::real(kMovingFireReloadRoll)},
        [&]() {
            return movingFireGame.enemies().size() == 1U &&
                   movingFireGame.effects().activeCount() == 1U &&
                   std::fabs(
                       movingFireGame.enemies()[0].dustCooldown -
                       kEnemyTrackDustCooldown) < 0.00001f &&
                   movingFireGame.enemies()[0].blockedTimer == 0.0f;
        });
    Game3DTestAccess::updateEnemies(
        movingFireGame, kMovingFireStep, movingFireRandom);
    const float movingFireSpeed = enemyMovementSpeedForType(
        movingFireEnemy.type, movingFireGame.advancedSettings());
    const XZ expectedMovingFirePosition =
        movingFireEnemy.position + cardinalVector(CardinalDirection::East) *
                                       (movingFireSpeed * kMovingFireStep);
    const float expectedMovingFireReload = intervalForEnemyRate(
        kMovingFireReloadRoll,
        movingFireGame.advancedSettings().enemyFireRatePercent);
    if (!checkTest(
            movingFireRandom.complete() &&
                movingFireGame.enemies().size() == 1U &&
                positionsMatch(movingFireGame.enemies()[0].position,
                               expectedMovingFirePosition) &&
                movingFireGame.enemies()[0].moving &&
                movingFireGame.enemies()[0].movementDirection ==
                    CardinalDirection::East &&
                movingFireGame.enemies()[0].blockedTimer == 0.0f &&
                std::fabs(movingFireGame.enemies()[0].dustCooldown - 0.19f) <
                    0.00001f &&
                std::fabs(movingFireGame.enemies()[0].fireCooldown -
                          expectedMovingFireReload) < 0.00001f &&
                movingFireGame.effects().activeCount() == 1U &&
                movingFireGame.shells().size() == 1U &&
                movingFireGame.shells()[0].ownerIndex == movingFireEnemy.id &&
                movingFireGame.eventsThisUpdate().empty(),
            "open enemy movement did not commit before one dust emission and "
            "the due suppressed-fire reload draw: " +
                movingFireRandom.diagnostic()))
        return 1;

    Game3D blockedFireGame(resourceRoot, 0xe3000008U);
    if (!checkTest(
            blockedFireGame.start(1, 3, 1, nations) &&
                Game3DTestAccess::prepareGameEventScenario(blockedFireGame),
            blockedFireGame.lastError()))
        return 1;
    for (int playerIndex = 0;
         playerIndex < static_cast<int>(blockedFireGame.players().size());
         ++playerIndex)
    {
        Game3DTestAccess::setEnemyTargetPlayerState(
            blockedFireGame, playerIndex, false, {13.0f, 10.0f});
    }
    Enemy blockedFireEnemy;
    blockedFireEnemy.id = 454;
    blockedFireEnemy.type = 3;
    blockedFireEnemy.position = {-10.0f, -10.0f};
    blockedFireEnemy.target = kGovernmentBaseCenter;
    blockedFireEnemy.driveDirection = CardinalDirection::East;
    blockedFireEnemy.movementDirection = CardinalDirection::East;
    blockedFireEnemy.directionTimer = 0.0f;
    blockedFireEnemy.directionDecisionInterval = 100.0f;
    blockedFireEnemy.movementDelay = 0.0f;
    blockedFireEnemy.fireCooldown = 0.0f;
    blockedFireEnemy.blockedTimer = 0.10f;
    blockedFireEnemy.dustCooldown = 0.0f;
    Game3DTestAccess::installEnemyUpdateScenario(
        blockedFireGame, blockedFireEnemy);
    constexpr float kBlockedFireStep = 0.05f;
    constexpr float kBlockedFireReloadRoll = 0.625f;
    ScriptedRandomSource blockedFireRandom(
        {ScriptedRandomSource::real(kBlockedFireReloadRoll)},
        [&]() {
            return blockedFireGame.enemies().size() == 1U &&
                   !blockedFireGame.enemies()[0].moving &&
                   std::fabs(
                       blockedFireGame.enemies()[0].blockedTimer - 0.15f) <
                       0.00001f &&
                   blockedFireGame.shells().empty();
        });
    Game3DTestAccess::updateEnemies(
        blockedFireGame, kBlockedFireStep, blockedFireRandom);
    float expectedBlockedFireReload = kBlockedFireReloadRoll;
    expectedBlockedFireReload *= 0.4f;
    expectedBlockedFireReload = intervalForEnemyRate(
        expectedBlockedFireReload,
        blockedFireGame.advancedSettings().enemyFireRatePercent);
    const XZ expectedBlockedShellVelocity =
        cardinalVector(CardinalDirection::East) * kBaseShellSpeed;
    const bool blockedOverrideWasRequired = !armorEnemyShouldFire(
        CardinalDirection::East, blockedFireEnemy.position,
        kGovernmentBaseCenter, false);
    if (!checkTest(
            blockedOverrideWasRequired && blockedFireRandom.complete() &&
                blockedFireGame.enemies().size() == 1U &&
                positionsMatch(blockedFireGame.enemies()[0].position,
                               blockedFireEnemy.position) &&
                !blockedFireGame.enemies()[0].moving &&
                blockedFireGame.enemies()[0].movementDirection ==
                    CardinalDirection::East &&
                std::fabs(blockedFireGame.enemies()[0].blockedTimer - 0.15f) <
                    0.00001f &&
                blockedFireGame.enemies()[0].dustCooldown == 0.0f &&
                std::fabs(blockedFireGame.enemies()[0].fireCooldown -
                          expectedBlockedFireReload) < 0.00001f &&
                blockedFireGame.shells().size() == 1U &&
                blockedFireGame.shells()[0].owner == ShellOwner::Enemy &&
                blockedFireGame.shells()[0].ownerIndex == blockedFireEnemy.id &&
                distanceSquared(blockedFireGame.shells()[0].velocity,
                                expectedBlockedShellVelocity) < 0.000001f &&
                blockedFireGame.eventsThisUpdate().size() == 1U &&
                blockedFireGame.eventsThisUpdate()[0].type ==
                    GameEventType::ShellFired &&
                blockedFireGame.eventsThisUpdate()[0].direction ==
                    CardinalDirection::East,
            "blocked Armor movement did not feed the same-frame firing override "
            "or consumed more than its reload draw: " +
                blockedFireRandom.diagnostic()))
        return 1;

    Game3D iceFireGame(resourceRoot, 0xe3000009U);
    XZ iceFireAnchor{};
    if (!checkTest(
            iceFireGame.start(1, 3, kTerrainFixtureStage, nations) &&
                Game3DTestAccess::preparePlayerIceInputScenario(
                    iceFireGame, iceFireAnchor),
            "could not prepare Armor ice-fire fixture"))
        return 1;
    const XZ iceFireTarget =
        iceFireAnchor + cardinalVector(CardinalDirection::North) * 2.0f;
    Game3DTestAccess::setEnemyTargetPlayerState(
        iceFireGame, 0, true, iceFireTarget);
    Game3DTestAccess::setEnemyCreationShowcase(iceFireGame, false);

    Enemy iceFireEnemy;
    iceFireEnemy.id = 455;
    iceFireEnemy.type = tanks3d::game::kArmorEnemyType;
    iceFireEnemy.position = iceFireAnchor;
    iceFireEnemy.driveDirection = CardinalDirection::East;
    iceFireEnemy.movementDirection = CardinalDirection::North;
    iceFireEnemy.moving = true;
    iceFireEnemy.directionTimer = 0.0f;
    iceFireEnemy.directionDecisionInterval = 100.0f;
    iceFireEnemy.movementDelay = 1.0f;
    iceFireEnemy.fireCooldown = 0.0f;
    iceFireEnemy.dustCooldown = 0.70f;
    iceFireEnemy.iceSlipTimer = 0.20f;
    iceFireEnemy.onIce = true;
    Game3DTestAccess::installEnemyUpdateScenario(
        iceFireGame, iceFireEnemy);

    constexpr float kIceFireRoll = 0.625f;
    ScriptedRandomSource iceFireRandom(
        {ScriptedRandomSource::real(kIceFireRoll)});
    Game3DTestAccess::updateEnemies(
        iceFireGame, 0.0f, iceFireRandom);
    const float expectedIceFireReload = intervalForEnemyRate(
        kIceFireRoll * 0.4f,
        iceFireGame.advancedSettings().enemyFireRatePercent);
    const XZ expectedIceShellPosition = shellSpawnPosition(
        iceFireAnchor, CardinalDirection::East);
    const XZ expectedIceShellVelocity =
        cardinalVector(CardinalDirection::East) * kBaseShellSpeed;
    const bool iceAimUsesTravelDirection =
        armorEnemyShouldFire(CardinalDirection::North, iceFireAnchor,
                             iceFireTarget, false) &&
        !armorEnemyShouldFire(CardinalDirection::East, iceFireAnchor,
                              iceFireTarget, false);
    if (!checkTest(
            iceAimUsesTravelDirection && iceFireRandom.complete() &&
                iceFireGame.map().isIce(iceFireAnchor) &&
                iceFireGame.enemies().size() == 1U &&
                iceFireGame.enemies()[0].moving &&
                iceFireGame.enemies()[0].driveDirection ==
                    CardinalDirection::East &&
                iceFireGame.enemies()[0].movementDirection ==
                    CardinalDirection::North &&
                positionsMatch(iceFireGame.enemies()[0].position,
                               iceFireAnchor) &&
                positionsMatch(iceFireGame.enemies()[0].target,
                               iceFireTarget) &&
                std::fabs(iceFireGame.enemies()[0].iceSlipTimer - 0.20f) <
                    0.00001f &&
                std::fabs(iceFireGame.enemies()[0].fireCooldown -
                          expectedIceFireReload) < 0.00001f &&
                iceFireGame.shells().size() == 1U &&
                iceFireGame.shells()[0].owner == ShellOwner::Enemy &&
                iceFireGame.shells()[0].ownerIndex == iceFireEnemy.id &&
                positionsMatch(iceFireGame.shells()[0].position,
                               expectedIceShellPosition) &&
                distanceSquared(iceFireGame.shells()[0].velocity,
                                expectedIceShellVelocity) < 0.000001f &&
                iceFireGame.eventsThisUpdate().size() == 1U &&
                iceFireGame.eventsThisUpdate()[0].type ==
                    GameEventType::ShellFired &&
                iceFireGame.eventsThisUpdate()[0].direction ==
                    CardinalDirection::East,
            "Armor ice fire did not aim by travel direction and launch by "
            "drive direction through the real Game3D adapter: " +
                iceFireRandom.diagnostic()))
        return 1;

    Game3D heavyFireGame(resourceRoot, 0xe3000003U);
    AdvancedGameSettings tunedHeavyFireSettings;
    tunedHeavyFireSettings.enemyFireRatePercent = 30;
    if (!checkTest(heavyFireGame.start(2, 3, 1, nations,
                                       tunedHeavyFireSettings),
                   heavyFireGame.lastError()))
        return 1;
    const bool enemyLaunchObserverDefaultedOff =
        !Game3DTestAccess::hasEnemyShellLaunchPresentationObserver(
            heavyFireGame);
    const bool audioRequestObserverDefaultedOff =
        !Game3DTestAccess::hasAudioRequestObserver(heavyFireGame);
    std::vector<EnemyShellLaunchPresentationSnapshot>
        enemyLaunchSnapshots;
    Game3DTestAccess::captureEnemyShellLaunchPresentation(
        heavyFireGame, enemyLaunchSnapshots);
    std::vector<AudioCue> enemyFireAudioRequests;
    Game3DTestAccess::captureAudioRequests(
        heavyFireGame, enemyFireAudioRequests);
    Game3DTestAccess::requestAudioCueForTest(
        heavyFireGame, AudioCue::PlayerFired);
    const bool audioRequestObserverPositiveControl =
        enemyFireAudioRequests ==
            std::vector<AudioCue>{AudioCue::PlayerFired};
    enemyFireAudioRequests.clear();
    const auto runHeavyFireCase = [&](CardinalDirection direction,
                                      XZ enemyPosition,
                                      bool addOwnedShell,
                                      bool addDistractorShells,
                                      bool expectNewShell,
                                      std::string &diagnostic) {
        enemyLaunchSnapshots.clear();
        enemyFireAudioRequests.clear();
        if (!Game3DTestAccess::prepareGameEventScenario(heavyFireGame))
        {
            diagnostic = "unable to prepare heavy-fire scenario";
            return false;
        }
        for (int playerIndex = 0;
             playerIndex < static_cast<int>(heavyFireGame.players().size());
             ++playerIndex)
        {
            Game3DTestAccess::setEnemyTargetPlayerState(
                heavyFireGame, playerIndex, false, {13.0f, 10.0f});
        }

        Enemy enemy;
        enemy.id = 401;
        enemy.type = 3;
        enemy.position = enemyPosition;
        enemy.driveDirection = direction;
        enemy.movementDirection = direction;
        enemy.directionTimer = 0.0f;
        enemy.directionDecisionInterval = 100.0f;
        enemy.movementDelay = 1.0f;
        enemy.fireCooldown = 0.0f;
        Game3DTestAccess::installEnemyUpdateScenario(heavyFireGame, enemy);
        if (addOwnedShell)
        {
            Game3DTestAccess::addEventShell(
                heavyFireGame, ShellOwner::Enemy, enemy.id,
                {1.0f, 1.0f}, cardinalVector(CardinalDirection::South),
                false, true, 0.0f);
        }
        if (addDistractorShells)
        {
            Game3DTestAccess::addEventShell(
                heavyFireGame, ShellOwner::Player, enemy.id,
                {2.0f, 2.0f}, cardinalVector(CardinalDirection::North));
            Game3DTestAccess::addEventShell(
                heavyFireGame, ShellOwner::Enemy, enemy.id + 1,
                {3.0f, 3.0f}, cardinalVector(CardinalDirection::West));
        }
        const std::size_t initialShellCount = heavyFireGame.shells().size();

        ScriptedRandomSource random({ScriptedRandomSource::real(0.625f)});
        Game3DTestAccess::updateEnemies(heavyFireGame, 0.0f, random);
        if (!random.complete())
        {
            diagnostic = random.diagnostic();
            return false;
        }
        const std::size_t expectedShellCount =
            initialShellCount + (expectNewShell ? 1U : 0U);
        const std::size_t expectedEventCount = expectNewShell ? 1U : 0U;
        const bool launchSnapshotAvailabilityCorrect =
            expectNewShell ? !enemyLaunchSnapshots.empty()
                           : enemyLaunchSnapshots.empty();
        const std::size_t expectedEffectCount =
            expectNewShell && !enemyLaunchSnapshots.empty()
                ? enemyLaunchSnapshots.back().effectCount
                : 0U;
        const float expectedReload = intervalForEnemyRate(
            0.625f * 0.4f,
            heavyFireGame.advancedSettings().enemyFireRatePercent);
        if (heavyFireGame.enemies().size() != 1U ||
            heavyFireGame.shells().size() != expectedShellCount ||
            heavyFireGame.eventsThisUpdate().size() != expectedEventCount ||
            !launchSnapshotAvailabilityCorrect ||
            !audioRequestObserverDefaultedOff ||
            !audioRequestObserverPositiveControl ||
            heavyFireGame.effects().activeCount() != expectedEffectCount ||
            std::fabs(heavyFireGame.enemies().front().fireCooldown -
                      expectedReload) > 0.00001f ||
            !enemyFireAudioRequests.empty())
        {
            diagnostic = "fire decision, event, or reload result changed";
            return false;
        }
        if (!expectNewShell)
        {
            if (!enemyLaunchSnapshots.empty())
            {
                diagnostic = "suppressed fire emitted a launch side effect";
                return false;
            }
            return true;
        }
        const Shell &shell = heavyFireGame.shells().back();
        const XZ expectedVelocity =
            cardinalVector(direction) * kBaseShellSpeed;
        const XZ expectedPosition =
            shellSpawnPosition(enemy.position, direction);
        const GameEvent &event = heavyFireGame.eventsThisUpdate().back();
        GameEvent expectedEvent = shellEvent(
            GameEventType::ShellFired, shell, expectedPosition);
        expectedEvent.direction = direction;
        if (shell.owner != ShellOwner::Enemy || shell.ownerIndex != enemy.id ||
            !positionsMatch(shell.position, expectedPosition) ||
            distanceSquared(shell.velocity, expectedVelocity) > 0.000001f ||
            shell.power || shell.impacting ||
            std::fabs(shell.life - 4.0f) > 0.00001f ||
            !(event == expectedEvent))
        {
            diagnostic = "heavy shell or event payload changed";
            return false;
        }

        const bool cameraStayedQuiet =
            Game3DTestAccess::cameraShake(heavyFireGame, 0) == 0.0f &&
            Game3DTestAccess::cameraShake(heavyFireGame, 1) == 0.0f;
        if (!enemyLaunchObserverDefaultedOff ||
            enemyLaunchSnapshots.size() != 3U || !cameraStayedQuiet ||
            enemyLaunchSnapshots[0].step !=
                EnemyShellLaunchPresentationStep::ShellInserted ||
            enemyLaunchSnapshots[1].step !=
                EnemyShellLaunchPresentationStep::EventAppended ||
            enemyLaunchSnapshots[2].step !=
                EnemyShellLaunchPresentationStep::MuzzleFlashSpawned ||
            enemyLaunchSnapshots[0].shells.size() != initialShellCount + 1U ||
            !enemyLaunchSnapshots[0].events.empty() ||
            enemyLaunchSnapshots[0].effectCount != 0U ||
            enemyLaunchSnapshots[1].shells.size() != initialShellCount + 1U ||
            enemyLaunchSnapshots[1].events.size() != 1U ||
            enemyLaunchSnapshots[1].effectCount != 0U ||
            enemyLaunchSnapshots[2].shells.size() != initialShellCount + 1U ||
            enemyLaunchSnapshots[2].events.size() != 1U ||
            enemyLaunchSnapshots[2].effectCount == 0U ||
            std::any_of(
                enemyLaunchSnapshots.begin(), enemyLaunchSnapshots.end(),
                [](const EnemyShellLaunchPresentationSnapshot &snapshot) {
                    return snapshot.enemyFireCooldown != 0.0f ||
                           snapshot.cameraShake[0] != 0.0f ||
                           snapshot.cameraShake[1] != 0.0f;
                }))
        {
            diagnostic =
                "shell/event/muzzle presentation order or cooldown timing changed";
            return false;
        }
        return true;
    };

    std::string heavyFireDiagnostic;
    if (!checkTest(
            runHeavyFireCase(CardinalDirection::North, {13.0f, 24.6f},
                             false, true, true, heavyFireDiagnostic),
            "north-facing heavy tank failed its aligned fire path: " +
                heavyFireDiagnostic))
        return 1;
    heavyFireDiagnostic.clear();
    if (!checkTest(
            runHeavyFireCase(CardinalDirection::East, {12.0f, 23.6f},
                             false, false, true, heavyFireDiagnostic),
            "east-facing heavy tank failed its aligned fire path: " +
                heavyFireDiagnostic))
        return 1;
    heavyFireDiagnostic.clear();
    if (!checkTest(
            runHeavyFireCase(CardinalDirection::West, {14.0f, 23.6f},
                             false, false, true, heavyFireDiagnostic),
            "west-facing heavy tank failed its aligned fire path: " +
                heavyFireDiagnostic))
        return 1;
    heavyFireDiagnostic.clear();
    if (!checkTest(
            runHeavyFireCase(CardinalDirection::South, {13.0f, 22.6f},
                             false, false, true, heavyFireDiagnostic),
            "south-facing heavy tank failed its aligned fire path: " +
                heavyFireDiagnostic))
        return 1;
    heavyFireDiagnostic.clear();
    if (!checkTest(
            runHeavyFireCase(CardinalDirection::None, {13.0f, 24.6f},
                             false, false, false, heavyFireDiagnostic),
            "directionless heavy tank fired despite having no aim axis: " +
                heavyFireDiagnostic))
        return 1;
    heavyFireDiagnostic.clear();
    if (!checkTest(
            runHeavyFireCase(CardinalDirection::North, {11.0f, 24.6f},
                             false, false, false, heavyFireDiagnostic),
            "heavy tank fired at the excluded exact-2.0 lateral boundary: " +
                heavyFireDiagnostic))
        return 1;
    heavyFireDiagnostic.clear();
    if (!checkTest(
            runHeavyFireCase(CardinalDirection::North, {13.0f, 24.6f},
                             true, false, false, heavyFireDiagnostic),
            "an existing owned shell did not suppress a second heavy round "
            "while still consuming the reload draw: " + heavyFireDiagnostic))
        return 1;

    Game3D spawnGuardGame(resourceRoot, 0xe3000006U);
    if (!checkTest(
            spawnGuardGame.start(1, 3, 17, nations) &&
                Game3DTestAccess::prepareGameEventScenario(spawnGuardGame),
            spawnGuardGame.lastError()))
        return 1;

    Game3DTestAccess::armEnemySpawn(spawnGuardGame, 0, 0.40f, 2);
    const int exhaustedNextId = Game3DTestAccess::nextEnemyId(spawnGuardGame);
    ScriptedRandomSource exhaustedRandom({});
    Game3DTestAccess::spawnEnemyIfNeeded(
        spawnGuardGame, 0.15f, exhaustedRandom);
    if (!checkTest(
            exhaustedRandom.complete() && spawnGuardGame.enemies().empty() &&
                Game3DTestAccess::enemiesRemainingToSpawn(spawnGuardGame) ==
                    0 &&
                Game3DTestAccess::nextEnemySpawn(spawnGuardGame) == 2 &&
                Game3DTestAccess::nextEnemyId(spawnGuardGame) ==
                    exhaustedNextId &&
                std::fabs(Game3DTestAccess::enemySpawnTimer(spawnGuardGame) -
                          0.40f) < 0.00001f,
            "an exhausted spawn queue changed its timer or consumed RNG: " +
                exhaustedRandom.diagnostic()))
        return 1;

    Game3DTestAccess::prepareGameEventScenario(spawnGuardGame);
    Game3DTestAccess::armEnemySpawn(spawnGuardGame, 1, 0.20f, 1);
    const int waitingNextId = Game3DTestAccess::nextEnemyId(spawnGuardGame);
    ScriptedRandomSource waitingRandom({});
    Game3DTestAccess::spawnEnemyIfNeeded(
        spawnGuardGame, 0.05f, waitingRandom);
    if (!checkTest(
            waitingRandom.complete() && spawnGuardGame.enemies().empty() &&
                Game3DTestAccess::enemiesRemainingToSpawn(spawnGuardGame) ==
                    1 &&
                Game3DTestAccess::nextEnemySpawn(spawnGuardGame) == 1 &&
                Game3DTestAccess::nextEnemyId(spawnGuardGame) ==
                    waitingNextId &&
                std::fabs(Game3DTestAccess::enemySpawnTimer(spawnGuardGame) -
                          0.15f) < 0.00001f,
            "a positive post-delta spawn timer spawned or consumed RNG: " +
                waitingRandom.diagnostic()))
        return 1;

    Game3DTestAccess::prepareGameEventScenario(spawnGuardGame);
    for (int enemyIndex = 0; enemyIndex < 4; ++enemyIndex)
    {
        Game3DTestAccess::addEventEnemy(
            spawnGuardGame, 510 + enemyIndex, 0, 1,
            {4.0f + static_cast<float>(enemyIndex) * 5.0f, 5.0f});
    }
    Game3DTestAccess::armEnemySpawn(spawnGuardGame, 1, 0.40f, 2);
    const int fullNextId = Game3DTestAccess::nextEnemyId(spawnGuardGame);
    ScriptedRandomSource fullRandom({});
    Game3DTestAccess::spawnEnemyIfNeeded(spawnGuardGame, 0.15f, fullRandom);
    if (!checkTest(
            fullRandom.complete() && spawnGuardGame.enemies().size() == 4U &&
                Game3DTestAccess::enemiesRemainingToSpawn(spawnGuardGame) ==
                    1 &&
                Game3DTestAccess::nextEnemySpawn(spawnGuardGame) == 2 &&
                Game3DTestAccess::nextEnemyId(spawnGuardGame) == fullNextId &&
                std::fabs(Game3DTestAccess::enemySpawnTimer(spawnGuardGame) -
                          0.25f) < 0.00001f,
            "a full enemy set failed to accumulate cooldown or consumed RNG: " +
                fullRandom.diagnostic()))
        return 1;

    AdvancedGameSettings successfulSpawnSettings;
    successfulSpawnSettings.enemyFireRatePercent = 30;
    successfulSpawnSettings.enemySpawnRatePercent = -30;
    Game3D successfulSpawnGame(resourceRoot, 0xe3000007U);
    if (!checkTest(
            successfulSpawnGame.start(1, 3, 17, nations,
                                      successfulSpawnSettings) &&
                Game3DTestAccess::prepareGameEventScenario(
                    successfulSpawnGame),
            successfulSpawnGame.lastError()))
        return 1;
    Game3DTestAccess::addEventEnemy(
        successfulSpawnGame, 520, 0, 1, kEnemySpawnPoints[1]);
    Game3DTestAccess::armEnemySpawn(successfulSpawnGame, 3, 0.25f, 1);
    const int successfulNextId =
        Game3DTestAccess::nextEnemyId(successfulSpawnGame);
    ScriptedRandomSource successfulRandom({
        ScriptedRandomSource::real(0.50f),
        ScriptedRandomSource::integer(0, 2, 2),
        ScriptedRandomSource::real(0.50f),
        ScriptedRandomSource::real(0.80f)},
        [&]() {
            return successfulSpawnGame.enemies().size() == 1U &&
                   successfulSpawnGame.enemies()[0].id == 520 &&
                   Game3DTestAccess::enemiesRemainingToSpawn(
                       successfulSpawnGame) == 3 &&
                   Game3DTestAccess::nextEnemySpawn(successfulSpawnGame) ==
                       1 &&
                   Game3DTestAccess::nextEnemyId(successfulSpawnGame) ==
                       successfulNextId + 1 &&
                   Game3DTestAccess::enemySpawnTimer(successfulSpawnGame) ==
                       0.0f &&
                   successfulSpawnGame.eventsThisUpdate().empty() &&
                   successfulSpawnGame.effects().activeCount() == 0U;
        });
    Game3DTestAccess::spawnEnemyIfNeeded(
        successfulSpawnGame, 0.25f, successfulRandom);
    const Enemy &spawnedEnemy = successfulSpawnGame.enemies().back();
    const float expectedInitialFireCooldown = intervalForEnemyRate(
        kEnemyInitialFireDelay,
        successfulSpawnGame.advancedSettings().enemyFireRatePercent);
    const float expectedNormalSpawnInterval = intervalForEnemyRate(
        kEnemySpawnInterval,
        successfulSpawnGame.advancedSettings().enemySpawnRatePercent);
    if (!checkTest(
            successfulRandom.complete() &&
                successfulSpawnGame.enemies().size() == 2U &&
                spawnedEnemy.id == successfulNextId &&
                positionsMatch(spawnedEnemy.position,
                               kEnemySpawnPoints[2]) &&
                positionsMatch(spawnedEnemy.target, kGovernmentBaseCenter) &&
                spawnedEnemy.yaw == cardinalYaw(CardinalDirection::South) &&
                spawnedEnemy.driveDirection == CardinalDirection::South &&
                spawnedEnemy.movementDirection == CardinalDirection::South &&
                spawnedEnemy.type == tanks3d::game::kPowerEnemyType &&
                spawnedEnemy.armor == 4 && !spawnedEnemy.carriesBonus &&
                !spawnedEnemy.destroyed && !spawnedEnemy.moving &&
                spawnedEnemy.frozenTimer == 0.0f &&
                std::fabs(spawnedEnemy.fireCooldown -
                          expectedInitialFireCooldown) < 0.00001f &&
                spawnedEnemy.creationTimer == kEnemyCreationDuration &&
                spawnedEnemy.deathTimer == 0.0f &&
                spawnedEnemy.blockedTimer == 0.0f &&
                spawnedEnemy.dustCooldown == 0.0f &&
                spawnedEnemy.directionTimer == 0.0f &&
                spawnedEnemy.directionDecisionInterval == 0.1f &&
                spawnedEnemy.movementDelay == 0.1f &&
                spawnedEnemy.iceSlipTimer == 0.0f && !spawnedEnemy.onIce &&
                Game3DTestAccess::enemiesRemainingToSpawn(
                    successfulSpawnGame) == 2 &&
                Game3DTestAccess::nextEnemySpawn(successfulSpawnGame) == 0 &&
                Game3DTestAccess::nextEnemyId(successfulSpawnGame) ==
                    successfulNextId + 1 &&
                std::fabs(Game3DTestAccess::enemySpawnTimer(
                              successfulSpawnGame) -
                          expectedNormalSpawnInterval) < 0.00001f &&
                successfulSpawnGame.eventsThisUpdate().empty() &&
                successfulSpawnGame.effects().activeCount() == 0U,
            "exact-zero spawn did not skip a blocked slot and commit the "
            "complete tuned transaction: " + successfulRandom.diagnostic()))
        return 1;

    Game3D armorSpawnGame(resourceRoot, 0xe3000009U);
    if (!checkTest(
            armorSpawnGame.start(1, 3, 17, nations) &&
                Game3DTestAccess::prepareGameEventScenario(armorSpawnGame),
            armorSpawnGame.lastError()))
        return 1;
    Game3DTestAccess::addEventEnemy(
        armorSpawnGame, 530, 0, 1, kEnemySpawnPoints[0]);
    Game3DTestAccess::armEnemySpawn(armorSpawnGame, 2, 0.0f, 0);
    const int armorNextId = Game3DTestAccess::nextEnemyId(armorSpawnGame);
    ScriptedRandomSource armorSpawnRandom(
        {ScriptedRandomSource::real(0.0f),
         ScriptedRandomSource::real(0.50f),
         ScriptedRandomSource::real(0.80f)},
        [&]() {
            return armorSpawnGame.enemies().size() == 1U &&
                   armorSpawnGame.enemies()[0].id == 530 &&
                   Game3DTestAccess::enemiesRemainingToSpawn(
                       armorSpawnGame) == 2 &&
                   Game3DTestAccess::nextEnemySpawn(armorSpawnGame) == 0 &&
                   Game3DTestAccess::nextEnemyId(armorSpawnGame) ==
                       armorNextId + 1 &&
                   Game3DTestAccess::enemySpawnTimer(armorSpawnGame) == 0.0f &&
                   armorSpawnGame.eventsThisUpdate().empty() &&
                   armorSpawnGame.effects().activeCount() == 0U;
        });
    Game3DTestAccess::spawnEnemyIfNeeded(
        armorSpawnGame, 0.0f, armorSpawnRandom);
    const Enemy &armorSpawned = armorSpawnGame.enemies().back();
    const float expectedArmorInitialFireCooldown = intervalForEnemyRate(
        kEnemyInitialFireDelay,
        armorSpawnGame.advancedSettings().enemyFireRatePercent);
    const float expectedArmorSpawnInterval = intervalForEnemyRate(
        kEnemySpawnInterval,
        armorSpawnGame.advancedSettings().enemySpawnRatePercent);
    if (!checkTest(
            armorSpawnRandom.complete() &&
                armorSpawnGame.enemies().size() == 2U &&
                armorSpawned.id == armorNextId &&
                positionsMatch(armorSpawned.position,
                               kEnemySpawnPoints[1]) &&
                positionsMatch(armorSpawned.target,
                               kGovernmentBaseCenter) &&
                armorSpawned.yaw == cardinalYaw(CardinalDirection::South) &&
                armorSpawned.driveDirection == CardinalDirection::South &&
                armorSpawned.movementDirection ==
                    CardinalDirection::South &&
                armorSpawned.type == tanks3d::game::kArmorEnemyType &&
                armorSpawned.armor == 4 && !armorSpawned.carriesBonus &&
                !armorSpawned.destroyed && !armorSpawned.moving &&
                armorSpawned.frozenTimer == 0.0f &&
                std::fabs(armorSpawned.fireCooldown -
                          expectedArmorInitialFireCooldown) < 0.00001f &&
                armorSpawned.creationTimer == kEnemyCreationDuration &&
                armorSpawned.deathTimer == 0.0f &&
                armorSpawned.blockedTimer == 0.0f &&
                armorSpawned.dustCooldown == 0.0f &&
                armorSpawned.directionTimer == 0.0f &&
                armorSpawned.directionDecisionInterval == 0.1f &&
                armorSpawned.movementDelay == 0.1f &&
                armorSpawned.iceSlipTimer == 0.0f && !armorSpawned.onIce &&
                Game3DTestAccess::enemiesRemainingToSpawn(armorSpawnGame) ==
                    1 &&
                Game3DTestAccess::nextEnemySpawn(armorSpawnGame) == 2 &&
                Game3DTestAccess::nextEnemyId(armorSpawnGame) ==
                    armorNextId + 1 &&
                std::fabs(Game3DTestAccess::enemySpawnTimer(armorSpawnGame) -
                          expectedArmorSpawnInterval) < 0.00001f &&
                armorSpawnGame.eventsThisUpdate().empty() &&
                armorSpawnGame.effects().activeCount() == 0U,
            "Armor spawn changed its R-R-R transcript, intermediate ID "
            "reservation, or complete commit: " +
                armorSpawnRandom.diagnostic()))
        return 1;

    Game3D blockedSpawnGame(resourceRoot, 0xe3000004U);
    if (!checkTest(
            blockedSpawnGame.start(1, 3, 1, nations) &&
                Game3DTestAccess::prepareGameEventScenario(blockedSpawnGame),
            blockedSpawnGame.lastError()))
        return 1;
    for (std::size_t spawnIndex = 0; spawnIndex < kEnemySpawnPoints.size();
         ++spawnIndex)
    {
        Game3DTestAccess::addEventEnemy(
            blockedSpawnGame, 500 + static_cast<int>(spawnIndex), 0, 1,
            kEnemySpawnPoints[spawnIndex]);
    }
    Game3DTestAccess::armEnemySpawn(blockedSpawnGame, 1, 0.0f, 0);
    const int blockedNextSpawnBefore =
        Game3DTestAccess::nextEnemySpawn(blockedSpawnGame);
    const int blockedNextIdBefore =
        Game3DTestAccess::nextEnemyId(blockedSpawnGame);
    ScriptedRandomSource firstBlockedAttempt({});
    Game3DTestAccess::spawnEnemyIfNeeded(
        blockedSpawnGame, 0.0f, firstBlockedAttempt);
    const float expectedRetryInterval = intervalForEnemyRate(
        kEnemySpawnRetryInterval,
        blockedSpawnGame.advancedSettings().enemySpawnRatePercent);
    if (!checkTest(
            firstBlockedAttempt.complete() &&
                blockedSpawnGame.enemies().size() == 3U &&
                Game3DTestAccess::enemiesRemainingToSpawn(blockedSpawnGame) ==
                    1 &&
                Game3DTestAccess::nextEnemySpawn(blockedSpawnGame) ==
                    blockedNextSpawnBefore &&
                Game3DTestAccess::nextEnemyId(blockedSpawnGame) ==
                    blockedNextIdBefore &&
                std::fabs(Game3DTestAccess::enemySpawnTimer(blockedSpawnGame) -
                          expectedRetryInterval) < 0.00001f,
            "three blocked enemy spawn points consumed RNG, spawned a tank, "
            "or missed the retry interval: " +
                firstBlockedAttempt.diagnostic()))
        return 1;

    ScriptedRandomSource secondBlockedAttempt({});
    Game3DTestAccess::spawnEnemyIfNeeded(
        blockedSpawnGame,
        Game3DTestAccess::enemySpawnTimer(blockedSpawnGame),
        secondBlockedAttempt);
    if (!checkTest(
            secondBlockedAttempt.complete() &&
                blockedSpawnGame.enemies().size() == 3U &&
                Game3DTestAccess::enemiesRemainingToSpawn(blockedSpawnGame) ==
                    1 &&
                Game3DTestAccess::nextEnemySpawn(blockedSpawnGame) ==
                    blockedNextSpawnBefore &&
                Game3DTestAccess::nextEnemyId(blockedSpawnGame) ==
                    blockedNextIdBefore &&
                std::fabs(Game3DTestAccess::enemySpawnTimer(blockedSpawnGame) -
                          expectedRetryInterval) < 0.00001f,
            "the fully blocked retry did not re-arm without consuming RNG: " +
                secondBlockedAttempt.diagnostic()))
        return 1;

    return 0;
}

int runEnemyLifecycleProductionPathSelfTests(const fs::path &resourceRoot)
{
    const std::array<Nation, 2> nations{{Nation::UnitedStates,
                                         Nation::SovietUnion}};
    const auto nearlyEqual = [](float first, float second) {
        return std::fabs(first - second) < 0.00001f;
    };
    const auto positionsMatch = [](XZ first, XZ second) {
        return distanceSquared(first, second) < 0.000001f;
    };
    const auto makeLifecycleEnemy = []() {
        Enemy enemy;
        enemy.id = 601;
        enemy.type = 0;
        enemy.position = {8.0f, 8.0f};
        enemy.target = {4.0f, 4.0f};
        enemy.driveDirection = CardinalDirection::East;
        enemy.movementDirection = CardinalDirection::North;
        enemy.moving = true;
        enemy.fireCooldown = 0.80f;
        enemy.dustCooldown = 0.70f;
        enemy.directionTimer = 0.20f;
        enemy.directionDecisionInterval = 100.0f;
        enemy.movementDelay = 0.90f;
        enemy.iceSlipTimer = 0.33f;
        enemy.onIce = true;
        return enemy;
    };

    Game3D lifecycleGame(resourceRoot, 0xe3100001U);
    if (!checkTest(lifecycleGame.start(1, 3, 1, nations),
                   lifecycleGame.lastError()))
        return 1;
    const auto prepareOpenLifecycleScenario = [&]() {
        if (!Game3DTestAccess::prepareGameEventScenario(lifecycleGame))
            return false;
        Game3DTestAccess::setEnemyCreationShowcase(lifecycleGame, false);
        for (int playerIndex = 0;
             playerIndex < static_cast<int>(lifecycleGame.players().size());
             ++playerIndex)
        {
            Game3DTestAccess::setEnemyTargetPlayerState(
                lifecycleGame, playerIndex, false, {13.0f, 10.0f});
        }
        return true;
    };

    if (!prepareOpenLifecycleScenario())
        return 1;
    Enemy destroyedEnemy = makeLifecycleEnemy();
    destroyedEnemy.destroyed = true;
    destroyedEnemy.deathTimer = 0.10f;
    destroyedEnemy.creationTimer = 0.45f;
    destroyedEnemy.frozenTimer = 0.45f;
    Game3DTestAccess::installEnemyUpdateScenario(lifecycleGame,
                                                 destroyedEnemy);
    // A completed enemy remains observable while one of its rounds is alive.
    Game3DTestAccess::addEventShell(
        lifecycleGame, ShellOwner::Enemy, destroyedEnemy.id,
        {1.0f, 1.0f}, cardinalVector(CardinalDirection::South));
    ScriptedRandomSource destroyedRandom({});
    Game3DTestAccess::updateEnemies(lifecycleGame, 0.25f,
                                    destroyedRandom);
    if (!checkTest(
            destroyedRandom.complete() && lifecycleGame.enemies().size() == 1U &&
                lifecycleGame.shells().size() == 1U &&
                nearlyEqual(lifecycleGame.enemies()[0].deathTimer, 0.0f) &&
                lifecycleGame.enemies()[0].moving &&
                lifecycleGame.enemies()[0].movementDirection ==
                    CardinalDirection::North &&
                positionsMatch(lifecycleGame.enemies()[0].position,
                               destroyedEnemy.position) &&
                nearlyEqual(lifecycleGame.enemies()[0].creationTimer, 0.45f) &&
                nearlyEqual(lifecycleGame.enemies()[0].frozenTimer, 0.45f) &&
                nearlyEqual(lifecycleGame.enemies()[0].fireCooldown, 0.80f) &&
                nearlyEqual(lifecycleGame.enemies()[0].dustCooldown, 0.70f),
            "destroyed enemy lifecycle did not clamp only death time or "
            "unexpectedly cleared its pre-existing momentum: " +
                destroyedRandom.diagnostic()))
        return 1;

    if (!prepareOpenLifecycleScenario())
        return 1;
    Enemy creatingEnemy = makeLifecycleEnemy();
    creatingEnemy.creationTimer = 0.45f;
    creatingEnemy.frozenTimer = 0.10f;
    Game3DTestAccess::installEnemyUpdateScenario(lifecycleGame,
                                                 creatingEnemy);
    ScriptedRandomSource creatingRandom({});
    Game3DTestAccess::updateEnemies(lifecycleGame, 0.25f,
                                    creatingRandom);
    if (!checkTest(
            creatingRandom.complete() && lifecycleGame.enemies().size() == 1U &&
                nearlyEqual(lifecycleGame.enemies()[0].creationTimer, 0.20f) &&
                nearlyEqual(lifecycleGame.enemies()[0].frozenTimer, 0.0f) &&
                !lifecycleGame.enemies()[0].moving &&
                lifecycleGame.enemies()[0].movementDirection ==
                    CardinalDirection::North &&
                nearlyEqual(lifecycleGame.enemies()[0].iceSlipTimer, 0.33f) &&
                lifecycleGame.enemies()[0].onIce &&
                nearlyEqual(lifecycleGame.enemies()[0].fireCooldown, 0.80f) &&
                nearlyEqual(lifecycleGame.enemies()[0].dustCooldown, 0.70f) &&
                nearlyEqual(lifecycleGame.enemies()[0].directionTimer, 0.20f) &&
                nearlyEqual(lifecycleGame.enemies()[0].movementDelay, 0.90f),
            "creating enemy did not decrement creation/frozen timers while "
            "clearing movement and preserving travel/cooldowns: " +
                creatingRandom.diagnostic()))
        return 1;

    if (!prepareOpenLifecycleScenario())
        return 1;
    Enemy showcaseEnemy = makeLifecycleEnemy();
    showcaseEnemy.creationTimer = 0.45f;
    showcaseEnemy.frozenTimer = 0.45f;
    Game3DTestAccess::installEnemyUpdateScenario(lifecycleGame,
                                                 showcaseEnemy);
    Game3DTestAccess::setEnemyCreationShowcase(lifecycleGame, true);
    ScriptedRandomSource showcaseRandom({});
    Game3DTestAccess::updateEnemies(lifecycleGame, 0.25f,
                                    showcaseRandom);
    Game3DTestAccess::setEnemyCreationShowcase(lifecycleGame, false);
    if (!checkTest(
            showcaseRandom.complete() && lifecycleGame.enemies().size() == 1U &&
                nearlyEqual(lifecycleGame.enemies()[0].creationTimer, 0.45f) &&
                nearlyEqual(lifecycleGame.enemies()[0].frozenTimer, 0.20f) &&
                !lifecycleGame.enemies()[0].moving &&
                lifecycleGame.enemies()[0].movementDirection ==
                    CardinalDirection::North &&
                nearlyEqual(lifecycleGame.enemies()[0].iceSlipTimer, 0.33f) &&
                nearlyEqual(lifecycleGame.enemies()[0].fireCooldown, 0.80f) &&
                nearlyEqual(lifecycleGame.enemies()[0].dustCooldown, 0.70f),
            "creation showcase did not hold creation while advancing frozen "
            "time and preserving the gated movement state: " +
                showcaseRandom.diagnostic()))
        return 1;

    if (!prepareOpenLifecycleScenario())
        return 1;
    Enemy frozenEnemy = makeLifecycleEnemy();
    frozenEnemy.frozenTimer = 0.10f;
    frozenEnemy.dustCooldown = 0.10f;
    frozenEnemy.iceSlipTimer = 0.40f;
    Game3DTestAccess::installEnemyUpdateScenario(lifecycleGame,
                                                 frozenEnemy);
    ScriptedRandomSource frozenRandom({});
    Game3DTestAccess::updateEnemies(lifecycleGame, 0.25f,
                                    frozenRandom);
    if (!checkTest(
            frozenRandom.complete() && lifecycleGame.enemies().size() == 1U &&
                nearlyEqual(lifecycleGame.enemies()[0].frozenTimer, 0.0f) &&
                nearlyEqual(lifecycleGame.enemies()[0].dustCooldown, 0.0f) &&
                nearlyEqual(lifecycleGame.enemies()[0].iceSlipTimer, 0.15f) &&
                nearlyEqual(lifecycleGame.enemies()[0].fireCooldown, 0.80f) &&
                !lifecycleGame.enemies()[0].moving &&
                lifecycleGame.enemies()[0].movementDirection ==
                    CardinalDirection::North &&
                nearlyEqual(lifecycleGame.enemies()[0].directionTimer, 0.20f) &&
                nearlyEqual(lifecycleGame.enemies()[0].movementDelay, 0.90f),
            "frozen enemy did not clamp frozen/dust timers, advance ice, or "
            "correctly pause fire and steering: " + frozenRandom.diagnostic()))
        return 1;

    Game3D activeGame(resourceRoot, 0xe3100002U);
    XZ iceAnchor{};
    if (!checkTest(
            activeGame.start(1, 3, kTerrainFixtureStage, nations) &&
                Game3DTestAccess::preparePlayerIceInputScenario(activeGame,
                                                                 iceAnchor),
            "could not prepare the enemy lifecycle ice fixture"))
        return 1;
    Game3DTestAccess::setEnemyTargetPlayerState(
        activeGame, 0, false, {13.0f, 10.0f});
    Game3DTestAccess::setEnemyCreationShowcase(activeGame, false);
    Enemy activeEnemy = makeLifecycleEnemy();
    activeEnemy.position = iceAnchor;
    activeEnemy.iceSlipTimer = 0.20f;
    Game3DTestAccess::installEnemyUpdateScenario(activeGame, activeEnemy);
    ScriptedRandomSource activeRandom({});
    constexpr float kActiveStep = 0.04f;
    Game3DTestAccess::updateEnemies(activeGame, kActiveStep, activeRandom);
    const float activeDistance = enemyMovementSpeedForType(
        activeEnemy.type, activeGame.advancedSettings()) * kActiveStep;
    const XZ expectedActivePosition =
        iceAnchor + cardinalVector(CardinalDirection::North) * activeDistance;
    if (!checkTest(
            activeRandom.complete() && activeGame.enemies().size() == 1U &&
                activeGame.map().isIce(iceAnchor) &&
                activeGame.enemies()[0].moving &&
                activeGame.enemies()[0].driveDirection ==
                    CardinalDirection::East &&
                activeGame.enemies()[0].movementDirection ==
                    CardinalDirection::North &&
                positionsMatch(activeGame.enemies()[0].position,
                               expectedActivePosition) &&
                nearlyEqual(activeGame.enemies()[0].iceSlipTimer, 0.16f) &&
                nearlyEqual(activeGame.enemies()[0].fireCooldown, 0.76f) &&
                nearlyEqual(activeGame.enemies()[0].dustCooldown, 0.66f) &&
                nearlyEqual(activeGame.enemies()[0].directionTimer, 0.24f) &&
                nearlyEqual(activeGame.enemies()[0].movementDelay, 0.86f) &&
                activeGame.shells().empty() &&
                activeGame.eventsThisUpdate().empty(),
            "active enemy did not consume fire/dust/movement timers or carry "
            "its prior travel direction across ice without RNG: " +
                activeRandom.diagnostic()))
        return 1;

    return 0;
}

int runShellCollisionSelfTests(const fs::path &resourceRoot)
{
    std::string error;
    Shell playerRound;
    playerRound.position = {0.0f, 0.0f};
    playerRound.velocity = {10.0f, 0.0f};
    playerRound.owner = ShellOwner::Player;
    playerRound.ownerIndex = 0;
    Shell enemyRound;
    enemyRound.position = {1.0f, 0.0f};
    enemyRound.velocity = {-10.0f, 0.0f};
    enemyRound.owner = ShellOwner::Enemy;
    XZ shellCollision{};
    if (!checkTest(shellCancellationPoint(playerRound, enemyRound, 0.05f,
                                          shellCollision) &&
                       std::fabs(shellCollision.x - 0.5f) < 0.001f,
                   "opposing shells did not cancel continuously"))
        return 1;
    enemyRound.position = {1.0f, 0.42f};
    if (!checkTest(shellCancellationPoint(playerRound, enemyRound, 0.05f,
                                          shellCollision),
                   "offset opposing shells missed the classic 8x8 AABB"))
        return 1;
    enemyRound.position = {1.0f, 0.50f};
    if (!checkTest(!shellCancellationPoint(playerRound, enemyRound, 0.05f,
                                           shellCollision),
                   "edge-only projectile contact incorrectly cancelled"))
        return 1;
    enemyRound.position = {0.24f, -1.0f};
    enemyRound.velocity = {0.0f, 20.0f};
    if (!checkTest(shellCancellationPoint(playerRound, enemyRound, 0.05f,
                                          shellCollision),
                   "perpendicular opposing shells did not cancel"))
        return 1;
    Shell sameGunRound = playerRound;
    sameGunRound.position = {0.1f, 0.0f};
    if (!checkTest(!shellsCanCancel(playerRound, sameGunRound),
                   "shells fired by the same gun cancelled each other"))
        return 1;
    sameGunRound.ownerIndex = 1;
    if (!checkTest(!shellsCanCancel(playerRound, sameGunRound),
                   "P1 and P2 shells should pass through each other"))
        return 1;

    StageMap runtimeCancellationMap;
    if (!runtimeCancellationMap.load(resourceRoot, 1, error))
        return 1;
    Shell runtimePlayerRound = playerRound;
    runtimePlayerRound.position = {5.5f, 1.0f};
    runtimePlayerRound.velocity = {10.0f, 0.0f};
    Shell runtimeEnemyRound = enemyRound;
    runtimeEnemyRound.position = {5.5f, 1.0f};
    runtimeEnemyRound.velocity = {-10.0f, 0.0f};
    runtimeEnemyRound.owner = ShellOwner::Enemy;
    const XZ runtimePlayerStart{5.0f, 1.0f};
    const XZ runtimeEnemyStart{6.0f, 1.0f};
    if (!checkTest(resolveSweptShellCancellation(
                       runtimePlayerRound, runtimeEnemyRound,
                       runtimePlayerStart, runtimeEnemyStart, 0.05f,
                       runtimeCancellationMap, shellCollision) &&
                       runtimePlayerRound.impacting &&
                       runtimeEnemyRound.impacting &&
                       std::fabs(shellCollision.x - 5.5f) < 0.001f,
                   "runtime shell-cancellation path diverged from its swept test"))
        return 1;

    Shell impactRound = playerRound;
    beginShellImpact(impactRound, {3.0f, 4.0f});
    if (!checkTest(impactRound.impacting &&
                       std::fabs(impactRound.life - kShellImpactDuration) <
                           0.0001f &&
                       lengthSquared(impactRound.velocity) < 0.0001f &&
                       distanceSquared(impactRound.position,
                                       {3.0f, 4.0f}) < 0.0001f &&
                       !shellsCanCancel(impactRound, enemyRound),
                   "destroyed projectile did not retain its 200 ms slot"))
        return 1;

    return 0;
}

struct SeededEnemySample
{
    int type = 0;
    int armor = 0;
    bool carriesBonus = false;
};

bool operator==(const SeededEnemySample &first,
                const SeededEnemySample &second)
{
    return first.type == second.type && first.armor == second.armor &&
           first.carriesBonus == second.carriesBonus;
}

struct SeededBonusSample
{
    int type = 0;
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

bool operator==(const SeededBonusSample &first,
                const SeededBonusSample &second)
{
    return first.type == second.type &&
           first.x == second.x && first.y == second.y && first.z == second.z;
}

struct SeededRandomTranscript
{
    std::vector<SeededEnemySample> enemies;
    std::vector<SeededBonusSample> bonuses;
};

bool operator==(const SeededRandomTranscript &first,
                const SeededRandomTranscript &second)
{
    return first.enemies == second.enemies && first.bonuses == second.bonuses;
}

std::string randomSeedLabel(std::uint32_t seed)
{
    std::ostringstream label;
    label << "0x" << std::hex << std::setw(8) << std::setfill('0') << seed;
    return label.str();
}

bool captureSeededRandomTranscript(const fs::path &resourceRoot,
                                   std::uint32_t seed,
                                   SeededRandomTranscript &transcript,
                                   std::string &error)
{
    constexpr int kEnemySamples = 64;
    constexpr int kBonusSamples = 32;
    const std::array<Nation, 2> nations{{Nation::UnitedStates,
                                         Nation::SovietUnion}};
    Game3D game(resourceRoot, seed);
    if (game.randomSeed() != seed)
    {
        error = "Game3D did not retain the injected seed";
        return false;
    }
    if (!game.start(1, 3, 17, nations))
    {
        error = game.lastError();
        return false;
    }

    transcript.enemies.clear();
    transcript.bonuses.clear();
    transcript.enemies.reserve(kEnemySamples);
    transcript.bonuses.reserve(kBonusSamples);
    for (int index = 0; index < kEnemySamples; ++index)
    {
        Enemy enemy;
        if (!Game3DTestAccess::sampleRandomEnemy(game, enemy))
        {
            error = "unable to sample enemy " + std::to_string(index);
            return false;
        }
        transcript.enemies.push_back(
            {enemy.type, enemy.armor, enemy.carriesBonus});
    }

    Game3DTestAccess::makeBandageEligible(game);
    for (int index = 0; index < kBonusSamples; ++index)
    {
        bonus_assets::Pickup pickup;
        if (!Game3DTestAccess::sampleRandomBonus(game, pickup))
        {
            error = "unable to sample bonus " + std::to_string(index);
            return false;
        }
        transcript.bonuses.push_back(
            {static_cast<int>(pickup.type),
             pickup.position.x, 0.0f, pickup.position.z});
    }
    return true;
}

bool restartPreservesRandomStream(const fs::path &resourceRoot,
                                  std::uint32_t seed,
                                  std::string &error)
{
    constexpr int kPrefixSamples = 7;
    constexpr int kTailSamples = 8;
    const std::array<Nation, 2> nations{{Nation::UnitedStates,
                                         Nation::SovietUnion}};
    Game3D restarted(resourceRoot, seed);
    Game3D uninterrupted(resourceRoot, seed);
    if (!restarted.start(1, 3, 17, nations) ||
        !uninterrupted.start(1, 3, 17, nations))
    {
        error = "unable to start restart-continuity sessions";
        return false;
    }

    for (int index = 0; index < kPrefixSamples; ++index)
    {
        Enemy restartedEnemy;
        Enemy uninterruptedEnemy;
        if (!Game3DTestAccess::sampleRandomEnemy(restarted,
                                                  restartedEnemy) ||
            !Game3DTestAccess::sampleRandomEnemy(uninterrupted,
                                                  uninterruptedEnemy))
        {
            error = "unable to consume restart-continuity prefix";
            return false;
        }
    }
    if (!restarted.restart() || restarted.randomSeed() != seed)
    {
        error = "restart failed or changed the retained seed";
        return false;
    }

    std::vector<SeededEnemySample> restartedTail;
    std::vector<SeededEnemySample> uninterruptedTail;
    restartedTail.reserve(kTailSamples);
    uninterruptedTail.reserve(kTailSamples);
    for (int index = 0; index < kTailSamples; ++index)
    {
        Enemy restartedEnemy;
        Enemy uninterruptedEnemy;
        if (!Game3DTestAccess::sampleRandomEnemy(restarted,
                                                  restartedEnemy) ||
            !Game3DTestAccess::sampleRandomEnemy(uninterrupted,
                                                  uninterruptedEnemy))
        {
            error = "unable to consume restart-continuity tail";
            return false;
        }
        restartedTail.push_back({restartedEnemy.type, restartedEnemy.armor,
                                 restartedEnemy.carriesBonus});
        uninterruptedTail.push_back(
            {uninterruptedEnemy.type, uninterruptedEnemy.armor,
             uninterruptedEnemy.carriesBonus});
    }
    if (restartedTail != uninterruptedTail)
    {
        error = "restart replayed or consumed the gameplay random stream";
        return false;
    }
    return true;
}

int runSeededRandomnessSelfTests(const fs::path &resourceRoot)
{
    constexpr std::uint32_t kFirstSeed = 0x5eed1234U;
    constexpr std::uint32_t kSecondSeed = 0xc0ffee42U;
    const std::string firstSeed = randomSeedLabel(kFirstSeed);
    const std::string secondSeed = randomSeedLabel(kSecondSeed);
    SeededRandomTranscript first;
    SeededRandomTranscript repeated;
    SeededRandomTranscript alternate;
    std::string error;
    if (!checkTest(captureSeededRandomTranscript(
                       resourceRoot, kFirstSeed, first, error),
                   "seeded random scenario failed for " + firstSeed +
                       ": " + error))
        return 1;
    error.clear();
    if (!checkTest(captureSeededRandomTranscript(
                       resourceRoot, kFirstSeed, repeated, error),
                   "repeated random scenario failed for " + firstSeed +
                       ": " + error))
        return 1;
    error.clear();
    if (!checkTest(captureSeededRandomTranscript(
                       resourceRoot, kSecondSeed, alternate, error),
                   "alternate random scenario failed for " + secondSeed +
                       ": " + error))
        return 1;
    if (!checkTest(first == repeated,
                   "enemy type/armor/carrier or bonus type/position changed "
                   "while replaying seed " + firstSeed))
        return 1;
    if (!checkTest(!(first == alternate),
                   "different seeds produced the same composite transcript: " +
                       firstSeed + " and " + secondSeed))
        return 1;

    bool validRanges = true;
    const auto inspect = [&](const SeededRandomTranscript &transcript) {
        for (const SeededEnemySample &enemy : transcript.enemies)
        {
            validRanges = validRanges && enemy.type >= 0 &&
                          enemy.type < kEnemyTypeCount && enemy.armor >= 1 &&
                          enemy.armor <= 4;
        }
        for (const SeededBonusSample &bonus : transcript.bonuses)
        {
            const float xSixteenths = bonus.x * 16.0f;
            const float zSixteenths = bonus.z * 16.0f;
            const bool onPixelGrid =
                std::fabs(xSixteenths - std::round(xSixteenths)) < 0.0001f &&
                std::fabs(zSixteenths - std::round(zSixteenths)) < 0.0001f &&
                std::fabs(bonus.y) < 0.0001f;
            const bool overlapsBase =
                bonus.x + 1.0f > kGovernmentBaseCenter.x - 1.0f &&
                bonus.x - 1.0f < kGovernmentBaseCenter.x + 1.0f &&
                bonus.z + 1.0f > kGovernmentBaseCenter.z - 1.0f &&
                bonus.z - 1.0f < kGovernmentBaseCenter.z + 1.0f;
            validRanges = validRanges && bonus.type >= 0 &&
                          bonus.type < static_cast<int>(bonus_assets::Type::Count) &&
                          xSixteenths >= 16.0f && xSixteenths <= 399.0f &&
                          zSixteenths >= 16.0f && zSixteenths <= 399.0f &&
                          onPixelGrid && !overlapsBase;
        }
    };
    inspect(first);
    inspect(alternate);
    const std::array<Nation, 2> digestNations{{Nation::UnitedStates,
                                               Nation::SovietUnion}};
    Game3D digestGame(resourceRoot, 0x5eed5678U);
    const bool digestStarted = digestGame.start(
        1, 3, 17, digestNations);
    if (digestStarted)
    {
        Game3DTestAccess::armEnemySpawn(
            digestGame, 7, 0.125f, 2, 23);
        static const std::string kExpectedSpawnDigestPrefix =
            "Tanks3D-session-v1|17,1,3,0,1,3,0,0,0,7,2,23,0x1p-3,";
        const SessionDigest digest = digestGame.sessionDigest();
        validRanges = validRanges &&
            digest.state.compare(0, kExpectedSpawnDigestPrefix.size(),
                                 kExpectedSpawnDigestPrefix) == 0;
    }
    else
        validRanges = false;
    if (!checkTest(validRanges,
                   "seeded output or spawn digest prefix changed for seeds " +
                       firstSeed + " and " + secondSeed))
        return 1;
    error.clear();
    if (!checkTest(restartPreservesRandomStream(
                       resourceRoot, kFirstSeed, error),
                   "restart continuity failed for seed " + firstSeed +
                       ": " + error))
        return 1;
    return 0;
}

float largestFloatBelow(double threshold)
{
    float atOrAbove = static_cast<float>(threshold);
    while (static_cast<double>(atOrAbove) < threshold)
    {
        atOrAbove = std::nextafter(
            atOrAbove, std::numeric_limits<float>::infinity());
    }
    float below = std::nextafter(
        atOrAbove, -std::numeric_limits<float>::infinity());
    while (static_cast<double>(below) >= threshold)
    {
        below = std::nextafter(
            below, -std::numeric_limits<float>::infinity());
    }
    return below;
}

float smallestFloatAtOrAbove(double threshold)
{
    float candidate = static_cast<float>(threshold);
    while (static_cast<double>(candidate) < threshold)
    {
        candidate = std::nextafter(
            candidate, std::numeric_limits<float>::infinity());
    }
    return candidate;
}

bool sampleScriptedEnemy(
    const fs::path &resourceRoot, int stage,
    std::vector<ScriptedRandomSource::Step> steps,
    Enemy &sample, std::string &error)
{
    const std::array<Nation, 2> nations{{Nation::UnitedStates,
                                         Nation::SovietUnion}};
    Game3D game(resourceRoot, 0x51c71e01U);
    if (!game.start(1, 3, stage, nations))
    {
        error = game.lastError();
        return false;
    }
    ScriptedRandomSource random(std::move(steps));
    if (!Game3DTestAccess::sampleRandomEnemy(game, random, sample))
    {
        error = "scripted enemy did not spawn";
        return false;
    }
    if (!random.complete())
    {
        error = random.diagnostic();
        return false;
    }
    return true;
}

bool sampleScriptedBonus(
    const fs::path &resourceRoot, int maximumHitPoints,
    bool makeBandageEligible,
    std::vector<ScriptedRandomSource::Step> steps,
    bonus_assets::Pickup &sample, std::string &error)
{
    const std::array<Nation, 2> nations{{Nation::UnitedStates,
                                         Nation::SovietUnion}};
    AdvancedGameSettings settings;
    settings.playerMaximumHitPoints = maximumHitPoints;
    Game3D game(resourceRoot, 0x51c71e02U);
    if (!game.start(1, 3, 17, nations, settings))
    {
        error = game.lastError();
        return false;
    }
    if (makeBandageEligible)
        Game3DTestAccess::makeBandageEligible(game);
    ScriptedRandomSource random(std::move(steps));
    if (!Game3DTestAccess::sampleRandomBonus(game, random, sample))
    {
        error = "scripted bonus was not released";
        return false;
    }
    if (!random.complete())
    {
        error = random.diagnostic();
        return false;
    }
    return true;
}

int runRandomProbabilityBoundarySelfTests(const fs::path &resourceRoot)
{
    constexpr std::uint32_t kAdapterSeed = 0x51c71e00U;
    Mt19937RandomSource adapter(kAdapterSeed);
    std::mt19937 reference(kAdapterSeed);
    std::uniform_real_distribution<float> adapterReal(0.0f, 1.0f);
    std::uniform_real_distribution<float> referenceReal(0.0f, 1.0f);
    std::uniform_int_distribution<int> adapterType(0, 2);
    std::uniform_int_distribution<int> referenceType(0, 2);
    std::uniform_int_distribution<int> adapterPixel(0, 383);
    std::uniform_int_distribution<int> referencePixel(0, 383);
    const bool adapterParity =
        adapter.draw(adapterReal) == referenceReal(reference) &&
        adapter.draw(adapterType) == referenceType(reference) &&
        adapter.draw(adapterReal) == referenceReal(reference) &&
        adapter.draw(adapterPixel) == referencePixel(reference) &&
        adapter.draw(adapterReal) == referenceReal(reference);
    if (!checkTest(adapterParity,
                   "Mt19937RandomSource changed mixed distribution output "
                   "for seed " + randomSeedLabel(kAdapterSeed)))
        return 1;

    constexpr int kBoundaryStage = 17;
    const float armorTankChance =
        enemyArmorTankChanceForStage(kBoundaryStage);
    const float belowArmorTankChance = std::nextafter(
        armorTankChance, -std::numeric_limits<float>::infinity());
    // This value only selects the regular-tank wiring branch. Step past the
    // possible one-ULP constexpr/runtime FMA difference; exact strictness is
    // asserted independently below.
    const float aboveArmorTankChance = std::nextafter(
        std::nextafter(armorTankChance,
                       std::numeric_limits<float>::infinity()),
        std::numeric_limits<float>::infinity());
    const float belowCarrierChance = std::nextafter(
        kBonusCarrierChance, -std::numeric_limits<float>::infinity());
    if (!checkTest(
            enemyRollCreatesArmorTank(kBoundaryStage,
                                      belowArmorTankChance) &&
                !enemyRollCreatesArmorTank(kBoundaryStage,
                                           armorTankChance) &&
                enemyRollCreatesBonusCarrier(belowCarrierChance) &&
                !enemyRollCreatesBonusCarrier(kBonusCarrierChance),
            "armor-tank or bonus-carrier strict probability boundary changed"))
        return 1;

    const EnemyArmorThresholds stage17 =
        enemyArmorThresholdsForStage(17);
    const bool stage17Boundaries =
        std::fabs(stage17.oneHit - 0.25) < 0.0000001 &&
        std::fabs(stage17.twoHits - 0.50) < 0.0000001 &&
        std::fabs(stage17.threeHits - 0.75) < 0.0000001 &&
        enemyArmorForRoll(17, largestFloatBelow(stage17.oneHit)) == 1 &&
        enemyArmorForRoll(17, smallestFloatAtOrAbove(stage17.oneHit)) == 2 &&
        enemyArmorForRoll(17, largestFloatBelow(stage17.twoHits)) == 2 &&
        enemyArmorForRoll(17, smallestFloatAtOrAbove(stage17.twoHits)) == 3 &&
        enemyArmorForRoll(17, largestFloatBelow(stage17.threeHits)) == 3 &&
        enemyArmorForRoll(17, smallestFloatAtOrAbove(stage17.threeHits)) == 4;
    if (!checkTest(stage17Boundaries,
                   "stage-17 armor thresholds lost strict 1/2/3/4 HP boundaries"))
        return 1;

    const EnemyArmorThresholds stage18 =
        enemyArmorThresholdsForStage(18);
    const bool stage18Boundaries =
        stage18.oneHit > 0.0 && stage18.oneHit < stage18.twoHits &&
        stage18.twoHits < stage18.threeHits && stage18.threeHits < 1.0 &&
        enemyArmorForRoll(18, largestFloatBelow(stage18.oneHit)) == 1 &&
        enemyArmorForRoll(18, smallestFloatAtOrAbove(stage18.oneHit)) == 2 &&
        enemyArmorForRoll(18, largestFloatBelow(stage18.twoHits)) == 2 &&
        enemyArmorForRoll(18, smallestFloatAtOrAbove(stage18.twoHits)) == 3 &&
        enemyArmorForRoll(18, largestFloatBelow(stage18.threeHits)) == 3 &&
        enemyArmorForRoll(18, smallestFloatAtOrAbove(stage18.threeHits)) == 4;
    if (!checkTest(stage18Boundaries,
                   "post-stage-17 armor threshold branch is incorrect"))
        return 1;

    Enemy armorTank;
    std::string armorTankError;
    const bool armorTankWiring = sampleScriptedEnemy(
        resourceRoot, 17,
        {ScriptedRandomSource::real(belowArmorTankChance),
         ScriptedRandomSource::real(belowCarrierChance),
         ScriptedRandomSource::real(
             largestFloatBelow(stage17.oneHit))},
        armorTank, armorTankError);
    if (!checkTest(armorTankWiring && armorTank.type == 3 &&
                       armorTank.carriesBonus && armorTank.armor == 1,
                   "armor-tank spawn wiring consumed the wrong draws: " +
                       armorTankError))
        return 1;

    Enemy regularTank;
    std::string regularTankError;
    const bool regularTankWiring = sampleScriptedEnemy(
        resourceRoot, 17,
        {ScriptedRandomSource::real(aboveArmorTankChance),
         ScriptedRandomSource::integer(0, 2, 2),
         ScriptedRandomSource::real(kBonusCarrierChance),
         ScriptedRandomSource::real(
             smallestFloatAtOrAbove(stage17.threeHits))},
        regularTank, regularTankError);
    if (!checkTest(regularTankWiring && regularTank.type == 2 &&
                       !regularTank.carriesBonus && regularTank.armor == 4,
                   "regular-tank spawn wiring consumed the wrong draws: " +
                       regularTankError))
        return 1;

    bonus_assets::Pickup fullHealthBonus;
    bonus_assets::Pickup oneHitPointBonus;
    std::string fullHealthError;
    std::string oneHitPointError;
    const bool fullHealthDisabled = sampleScriptedBonus(
        resourceRoot, 3, false,
        {ScriptedRandomSource::integer(0, 7, 7),
         ScriptedRandomSource::integer(0, 383, 0),
         ScriptedRandomSource::integer(0, 383, 0)},
        fullHealthBonus, fullHealthError);
    const bool oneHitPointDisabled = sampleScriptedBonus(
        resourceRoot, 1, false,
        {ScriptedRandomSource::integer(0, 7, 7),
         ScriptedRandomSource::integer(0, 383, 0),
         ScriptedRandomSource::integer(0, 383, 0)},
        oneHitPointBonus, oneHitPointError);
    if (!checkTest(
            fullHealthDisabled && oneHitPointDisabled &&
                fullHealthBonus.type == bonus_assets::Type::Boat &&
                oneHitPointBonus.type == bonus_assets::Type::Boat,
            "Bandage appeared in a full-health or 1-HP type range: " +
                fullHealthError + " " + oneHitPointError))
        return 1;

    bonus_assets::Pickup bandageSlotEight;
    bonus_assets::Pickup bandageSlotNine;
    std::string slotEightError;
    std::string slotNineError;
    const bool slotEightMapped = sampleScriptedBonus(
        resourceRoot, 3, true,
        {ScriptedRandomSource::integer(0, 9, 8),
         ScriptedRandomSource::integer(0, 383, 0),
         ScriptedRandomSource::integer(0, 383, 0)},
        bandageSlotEight, slotEightError);
    const bool slotNineMapped = sampleScriptedBonus(
        resourceRoot, 3, true,
        {ScriptedRandomSource::integer(0, 9, 9),
         ScriptedRandomSource::integer(0, 383, 0),
         ScriptedRandomSource::integer(0, 383, 0)},
        bandageSlotNine, slotNineError);
    if (!checkTest(
            slotEightMapped && slotNineMapped &&
                bandageSlotEight.type == bonus_assets::Type::Bandage &&
                bandageSlotNine.type == bonus_assets::Type::Bandage,
            "Bandage double-weight slots or eligible range changed: " +
                slotEightError + " " + slotNineError))
        return 1;

    bonus_assets::Pickup retriedBonus;
    std::string retryError;
    const bool rejectedBasePosition = sampleScriptedBonus(
        resourceRoot, 3, false,
        {ScriptedRandomSource::integer(0, 7, 0),
         ScriptedRandomSource::integer(0, 383, 192),
         ScriptedRandomSource::integer(0, 383, 362),
         ScriptedRandomSource::integer(0, 383, 0),
         ScriptedRandomSource::integer(0, 383, 0)},
        retriedBonus, retryError);
    if (!checkTest(
            bonusOverlapsGovernmentBase({13.0f, 23.625f}) &&
                !bonusOverlapsGovernmentBase({11.0f, 23.625f}) &&
                rejectedBasePosition &&
                retriedBonus.type == bonus_assets::Type::Grenade &&
                retriedBonus.position.x == 1.0f &&
                retriedBonus.position.z == 1.0f,
            "base-overlap rejection did not consume one x/z pair and retry: " +
                retryError))
        return 1;
    return 0;
}

void pressDirection(DirectionButtonFrame &button)
{
    button.held = true;
    button.pressed = true;
}

int playerShellCount(const Game3D &game, int playerIndex)
{
    return static_cast<int>(std::count_if(
        game.shells().begin(), game.shells().end(),
        [playerIndex](const Shell &shell) {
            return shell.owner == ShellOwner::Player &&
                   shell.ownerIndex == playerIndex;
        }));
}

int runScriptedPlayerInputSelfTests(const fs::path &resourceRoot)
{
    const std::array<Nation, 2> nations{{Nation::UnitedStates,
                                         Nation::SovietUnion}};
    const auto prepareArena = [&](Game3D &game, int playerCount) {
        return game.start(playerCount, 3, 1, nations) &&
               Game3DTestAccess::preparePlayerInputScenario(game);
    };

    struct DirectionBindingCase
    {
        KeyboardKey key;
        int playerIndex;
        CardinalDirection direction;
    };
    const std::array<DirectionBindingCase, 8> directionBindings{{
        {KEY_UP, 0, CardinalDirection::North},
        {KEY_DOWN, 0, CardinalDirection::South},
        {KEY_LEFT, 0, CardinalDirection::West},
        {KEY_RIGHT, 0, CardinalDirection::East},
        {KEY_W, 1, CardinalDirection::North},
        {KEY_S, 1, CardinalDirection::South},
        {KEY_A, 1, CardinalDirection::West},
        {KEY_D, 1, CardinalDirection::East}}};
    const auto directionButton = [](const PlayerControlFrame &controls,
                                    CardinalDirection direction)
        -> const DirectionButtonFrame & {
        switch (direction)
        {
        case CardinalDirection::North: return controls.north;
        case CardinalDirection::South: return controls.south;
        case CardinalDirection::West: return controls.west;
        case CardinalDirection::East: return controls.east;
        case CardinalDirection::None: return controls.north;
        }
        return controls.north;
    };
    const auto activeDirectionButtonCount = [](const PlayerInputFrame &frame) {
        int count = 0;
        for (const PlayerControlFrame &controls : frame.players)
        {
            for (const DirectionButtonFrame *button : {
                     &controls.north, &controls.south,
                     &controls.west, &controls.east})
                if (button->held || button->pressed)
                    ++count;
        }
        return count;
    };
    bool directionKeyMapCorrect = true;
    for (const DirectionBindingCase &binding : directionBindings)
    {
        const PlayerInputFrame mapped = playerInputFrameFromKeyState(
            [&](KeyboardKey key) { return key == binding.key; },
            [&](KeyboardKey key) { return key == binding.key; });
        const DirectionButtonFrame &button = directionButton(
            mapped.players[static_cast<std::size_t>(binding.playerIndex)],
            binding.direction);
        directionKeyMapCorrect = directionKeyMapCorrect && button.held &&
            button.pressed && activeDirectionButtonCount(mapped) == 1 &&
            !mapped.players[0].fireHeld && !mapped.players[1].fireHeld;
    }
    if (!checkTest(directionKeyMapCorrect,
                   "Arrow/WASD adapter mapping changed"))
        return 1;

    struct FireBindingCase
    {
        KeyboardKey key;
        int playerIndex;
    };
    const std::array<FireBindingCase, 6> fireBindings{{
        {KEY_RIGHT_ALT, 0}, {KEY_RIGHT_CONTROL, 0}, {KEY_SPACE, 0},
        {KEY_LEFT_ALT, 1}, {KEY_LEFT_CONTROL, 1}, {KEY_F, 1}}};
    bool fireKeyMapCorrect = true;
    for (const FireBindingCase &binding : fireBindings)
    {
        const PlayerInputFrame mapped = playerInputFrameFromKeyState(
            [&](KeyboardKey key) { return key == binding.key; },
            [](KeyboardKey) { return false; });
        fireKeyMapCorrect = fireKeyMapCorrect &&
            mapped.players[static_cast<std::size_t>(binding.playerIndex)]
                .fireHeld &&
            !mapped.players[static_cast<std::size_t>(1 - binding.playerIndex)]
                 .fireHeld &&
            activeDirectionButtonCount(mapped) == 0;
    }
    if (!checkTest(fireKeyMapCorrect,
                   "P1/P2 three-key fire adapter mapping changed"))
        return 1;

    Game3D priorityGame(resourceRoot, 0x1f2a0001U);
    if (!checkTest(prepareArena(priorityGame, 1),
                   "could not prepare scripted input priority arena"))
        return 1;
    PlayerInputFrame allPressed;
    PlayerControlFrame &allDirections = allPressed.players[0];
    pressDirection(allDirections.north);
    pressDirection(allDirections.south);
    pressDirection(allDirections.west);
    pressDirection(allDirections.east);
    priorityGame.update(0.0f, allPressed);
    if (!checkTest(
            priorityGame.players()[0].driveDirection ==
                    CardinalDirection::East &&
                priorityGame.players()[0].movementDirection ==
                    CardinalDirection::East,
            "same-frame pressed priority is no longer East > West > South > North"))
        return 1;

    PlayerInputFrame newNorth;
    newNorth.players[0].east.held = true;
    pressDirection(newNorth.players[0].north);
    priorityGame.update(0.0f, newNorth);
    if (!checkTest(
            priorityGame.players()[0].driveDirection ==
                CardinalDirection::North,
            "newly pressed direction did not override a held old direction"))
        return 1;

    PlayerInputFrame selectEast;
    pressDirection(selectEast.players[0].east);
    priorityGame.update(0.0f, selectEast);
    PlayerInputFrame stickyEast;
    stickyEast.players[0].east.held = true;
    stickyEast.players[0].south.held = true;
    priorityGame.update(0.0f, stickyEast);
    const bool heldDirectionStayedSelected =
        priorityGame.players()[0].driveDirection ==
        CardinalDirection::East;
    PlayerInputFrame fallback;
    fallback.players[0].north.held = true;
    fallback.players[0].south.held = true;
    fallback.players[0].west.held = true;
    priorityGame.update(0.0f, fallback);
    const bool fallbackChoseNorth =
        priorityGame.players()[0].driveDirection ==
        CardinalDirection::North;
    priorityGame.update(0.0f, {});
    if (!checkTest(heldDirectionStayedSelected && fallbackChoseNorth &&
                       !priorityGame.players()[0].moving &&
                       priorityGame.players()[0].driveDirection ==
                           CardinalDirection::North,
                   "held-direction stickiness, fallback order, or facing retention changed"))
        return 1;

    const XZ pressedOnlyStart = priorityGame.players()[0].position;
    const float pressedOnlyYaw = priorityGame.players()[0].yaw;
    PlayerInputFrame pressedOnlyWest;
    pressedOnlyWest.players[0].west.pressed = true;
    priorityGame.update(0.0f, pressedOnlyWest);
    if (!checkTest(
            priorityGame.players()[0].driveDirection ==
                    CardinalDirection::West &&
                priorityGame.players()[0].movementDirection ==
                    CardinalDirection::West &&
                !priorityGame.players()[0].moving &&
                priorityGame.players()[0].yaw == pressedOnlyYaw &&
                distanceSquared(priorityGame.players()[0].position,
                                pressedOnlyStart) == 0.0f,
            "pressed-only input no longer changes facing without propulsion or yaw"))
        return 1;

    Game3D turnAndFireGame(resourceRoot, 0x1f2a0002U);
    if (!checkTest(prepareArena(turnAndFireGame, 1),
                   "could not prepare turn-and-fire arena"))
        return 1;
    const XZ firingStart = turnAndFireGame.players()[0].position;
    PlayerInputFrame turnAndFire;
    pressDirection(turnAndFire.players[0].west);
    turnAndFire.players[0].fireHeld = true;
    turnAndFireGame.update(0.0f, turnAndFire);
    const bool firedWest = turnAndFireGame.shells().size() == 1U &&
        turnAndFireGame.shells()[0].ownerIndex == 0 &&
        turnAndFireGame.shells()[0].velocity.x < 0.0f &&
        std::fabs(turnAndFireGame.shells()[0].velocity.z) < 0.0001f &&
        turnAndFireGame.shells()[0].position.x < firingStart.x;
    if (!checkTest(
            turnAndFireGame.players()[0].driveDirection ==
                    CardinalDirection::West &&
                firedWest,
            "same-frame turn and fire did not use the new direction"))
        return 1;

    Game3D movingPlayerFireGame(resourceRoot, 0x1f2a0008U);
    if (!checkTest(prepareArena(movingPlayerFireGame, 1),
                   "could not prepare moving player fire arena"))
        return 1;
    const XZ movingPlayerFireStart =
        movingPlayerFireGame.players()[0].position;
    std::vector<PlayerShellLaunchPresentationSnapshot>
        movingPlayerFireSnapshots;
    const std::vector<AudioCue> noMovingPlayerAudioRequests;
    bool movingLaunchSawDustCommit = false;
    Game3DTestAccess::capturePlayerShellLaunchPresentation(
        movingPlayerFireGame, noMovingPlayerAudioRequests,
        movingPlayerFireSnapshots,
        [&](PlayerShellLaunchPresentationStep step) {
            if (step == PlayerShellLaunchPresentationStep::ShellInserted)
            {
                movingLaunchSawDustCommit =
                    movingPlayerFireGame.effects().activeCount() > 0U &&
                    std::fabs(
                        movingPlayerFireGame.players()[0].dustCooldown -
                        kPlayerTrackDustCooldown) < 0.0001f;
            }
        });
    PlayerInputFrame movingPlayerFire;
    pressDirection(movingPlayerFire.players[0].east);
    movingPlayerFire.players[0].fireHeld = true;
    movingPlayerFireGame.update(0.05f, movingPlayerFire);
    const auto firedEvent = std::find_if(
        movingPlayerFireGame.eventsThisUpdate().begin(),
        movingPlayerFireGame.eventsThisUpdate().end(),
        [](const GameEvent &event) {
            return event.type == GameEventType::ShellFired &&
                   event.sourcePlayerId == 0;
        });
    const XZ expectedMovedPlayer{
        movingPlayerFireStart.x + 0.25f, movingPlayerFireStart.z};
    const XZ expectedMovingPlayerSpawn{
        movingPlayerFireStart.x + 0.875f, movingPlayerFireStart.z};
    if (!checkTest(
            distanceSquared(movingPlayerFireGame.players()[0].position,
                            expectedMovedPlayer) < 0.000001f &&
                std::fabs(movingPlayerFireGame.players()[0].dustCooldown -
                          kPlayerTrackDustCooldown) < 0.0001f &&
                movingPlayerFireSnapshots.size() == 5U &&
                movingPlayerFireSnapshots[0].step ==
                    PlayerShellLaunchPresentationStep::ShellInserted &&
                movingPlayerFireSnapshots[0].effectCount > 0U &&
                movingLaunchSawDustCommit &&
                distanceSquared(
                    movingPlayerFireSnapshots[0].intent.tankPosition,
                    expectedMovedPlayer) < 0.000001f &&
                distanceSquared(
                    movingPlayerFireSnapshots[0].intent.position,
                    expectedMovingPlayerSpawn) < 0.000001f &&
                firedEvent != movingPlayerFireGame.eventsThisUpdate().end() &&
                firedEvent->direction == CardinalDirection::East &&
                distanceSquared(firedEvent->position,
                                expectedMovingPlayerSpawn) < 0.000001f,
            "movement/dust no longer commits before same-frame shell spawn"))
        return 1;

    Game3D blockedPlayerFireGame(resourceRoot, 0x1f2a000bU);
    if (!checkTest(prepareArena(blockedPlayerFireGame, 2),
                   "could not prepare blocked player fire arena"))
        return 1;
    const XZ blockedPlayerStart =
        blockedPlayerFireGame.players()[0].position;
    Game3DTestAccess::recenterPlayer(
        blockedPlayerFireGame, 1,
        {blockedPlayerStart.x + 1.90f, blockedPlayerStart.z});
    std::vector<PlayerShellLaunchPresentationSnapshot>
        blockedPlayerFireSnapshots;
    const std::vector<AudioCue> noBlockedPlayerAudioRequests;
    Game3DTestAccess::capturePlayerShellLaunchPresentation(
        blockedPlayerFireGame, noBlockedPlayerAudioRequests,
        blockedPlayerFireSnapshots);
    PlayerInputFrame blockedPlayerFire;
    pressDirection(blockedPlayerFire.players[0].east);
    blockedPlayerFire.players[0].fireHeld = true;
    blockedPlayerFireGame.update(0.05f, blockedPlayerFire);
    const auto blockedFireEvent = std::find_if(
        blockedPlayerFireGame.eventsThisUpdate().begin(),
        blockedPlayerFireGame.eventsThisUpdate().end(),
        [](const GameEvent &event) {
            return event.type == GameEventType::ShellFired &&
                   event.sourcePlayerId == 0;
        });
    const XZ expectedBlockedPlayerSpawn{
        blockedPlayerStart.x + 0.625f, blockedPlayerStart.z};
    if (!checkTest(
            distanceSquared(blockedPlayerFireGame.players()[0].position,
                            blockedPlayerStart) < 0.000001f &&
                blockedPlayerFireGame.players()[0].driveDirection ==
                    CardinalDirection::East &&
                blockedPlayerFireGame.players()[0].movementDirection ==
                    CardinalDirection::East &&
                !blockedPlayerFireGame.players()[0].moving &&
                blockedPlayerFireGame.players()[0].dustCooldown == 0.0f &&
                blockedPlayerFireSnapshots.size() == 5U &&
                blockedPlayerFireSnapshots[0].step ==
                    PlayerShellLaunchPresentationStep::ShellInserted &&
                blockedPlayerFireSnapshots[0].effectCount == 0U &&
                blockedFireEvent !=
                    blockedPlayerFireGame.eventsThisUpdate().end() &&
                blockedFireEvent->direction == CardinalDirection::East &&
                distanceSquared(blockedFireEvent->position,
                                expectedBlockedPlayerSpawn) < 0.000001f,
            "blocked movement emitted dust or skipped same-frame fire"))
        return 1;

    RecordingAudioOutput playerLaunchAudio;
    Game3D playerLaunchGame(resourceRoot, 0x1f2a0009U,
                            &playerLaunchAudio);
    if (!checkTest(playerLaunchGame.start(2, 3, 1, nations) &&
                       Game3DTestAccess::preparePlayerInputScenario(
                           playerLaunchGame),
                   "could not prepare player launch adapter arena"))
        return 1;
    Game3DTestAccess::setPlayerLevel(playerLaunchGame, 3, 1);
    const bool playerLaunchObserverDefaultedOff =
        !Game3DTestAccess::hasPlayerShellLaunchPresentationObserver(
            playerLaunchGame);
    const bool playerLaunchAudioObserverDefaultedOff =
        !Game3DTestAccess::hasAudioRequestObserver(playerLaunchGame);
    std::vector<PlayerShellLaunchPresentationSnapshot>
        playerLaunchSnapshots;
    std::vector<AudioCue> playerLaunchAudioRequests;
    std::vector<PlayerShellLaunchTraceStep> playerLaunchTrace;
    playerLaunchAudio.clear();
    playerLaunchAudio.onCall = [&playerLaunchTrace](
                                   const AudioOutputCall &call) {
        if (call.type == AudioOutputCallType::Play &&
            call.cue == AudioCue::PlayerFired)
        {
            playerLaunchTrace.push_back(
                PlayerShellLaunchTraceStep::AudioOutput);
        }
    };
    Game3DTestAccess::captureAudioRequests(
        playerLaunchGame, playerLaunchAudioRequests,
        [&playerLaunchTrace](AudioCue cue) {
            if (cue == AudioCue::PlayerFired)
            {
                playerLaunchTrace.push_back(
                    PlayerShellLaunchTraceStep::SemanticAudio);
            }
        });
    Game3DTestAccess::capturePlayerShellLaunchPresentation(
        playerLaunchGame, playerLaunchAudioRequests,
        playerLaunchSnapshots,
        [&playerLaunchTrace](PlayerShellLaunchPresentationStep step) {
            switch (step)
            {
            case PlayerShellLaunchPresentationStep::ShellInserted:
                playerLaunchTrace.push_back(
                    PlayerShellLaunchTraceStep::ShellInserted);
                break;
            case PlayerShellLaunchPresentationStep::EventAppended:
                playerLaunchTrace.push_back(
                    PlayerShellLaunchTraceStep::EventAppended);
                break;
            case PlayerShellLaunchPresentationStep::MuzzleFlashSpawned:
                playerLaunchTrace.push_back(
                    PlayerShellLaunchTraceStep::MuzzleFlashSpawned);
                break;
            case PlayerShellLaunchPresentationStep::CameraShakeCommitted:
                playerLaunchTrace.push_back(
                    PlayerShellLaunchTraceStep::CameraShakeCommitted);
                break;
            case PlayerShellLaunchPresentationStep::AudioRequested:
                playerLaunchTrace.push_back(
                    PlayerShellLaunchTraceStep::AudioRequested);
                break;
            }
        });
    PlayerInputFrame playerTwoFire;
    playerTwoFire.players[1].fireHeld = true;
    playerLaunchGame.update(0.0f, playerTwoFire);
    const XZ playerTwoPosition = playerLaunchGame.players()[1].position;
    const XZ expectedPlayerTwoShellPosition{
        playerTwoPosition.x, playerTwoPosition.z - 0.625f};
    const XZ expectedPlayerTwoShellVelocity{0.0f, -12.7075f};
    const bool everyLaunchStepSawReset = std::all_of(
        playerLaunchSnapshots.begin(), playerLaunchSnapshots.end(),
        [](const PlayerShellLaunchPresentationSnapshot &snapshot) {
            return std::fabs(snapshot.playerFireCooldown -
                             kPlayerReloadTime) < 0.0001f;
        });
    if (!checkTest(
            playerLaunchObserverDefaultedOff &&
                playerLaunchAudioObserverDefaultedOff &&
                playerLaunchSnapshots.size() == 5U &&
                everyLaunchStepSawReset &&
                playerLaunchTrace ==
                    std::vector<PlayerShellLaunchTraceStep>({
                        PlayerShellLaunchTraceStep::ShellInserted,
                        PlayerShellLaunchTraceStep::EventAppended,
                        PlayerShellLaunchTraceStep::MuzzleFlashSpawned,
                        PlayerShellLaunchTraceStep::CameraShakeCommitted,
                        PlayerShellLaunchTraceStep::AudioOutput,
                        PlayerShellLaunchTraceStep::SemanticAudio,
                        PlayerShellLaunchTraceStep::AudioRequested}),
            "player launch callback, output, or semantic-audio order changed"))
        return 1;
    if (!checkTest(
            playerLaunchSnapshots.size() == 5U &&
                playerLaunchSnapshots[0].shells.size() == 1U &&
                playerLaunchSnapshots[0].events.empty() &&
                playerLaunchSnapshots[0].effectCount == 0U &&
                playerLaunchSnapshots[0].cameraShake[0] == 0.0f &&
                playerLaunchSnapshots[0].cameraShake[1] == 0.0f &&
                playerLaunchSnapshots[0].audioRequests.empty() &&
                playerLaunchSnapshots[1].events.size() == 1U &&
                playerLaunchSnapshots[1].effectCount == 0U &&
                playerLaunchSnapshots[2].effectCount > 0U &&
                playerLaunchSnapshots[2].cameraShake[1] == 0.0f &&
                std::fabs(playerLaunchSnapshots[3].cameraShake[1] -
                          0.085f) < 0.0001f &&
                playerLaunchSnapshots[3].cameraShake[0] == 0.0f &&
                playerLaunchSnapshots[3].audioRequests.empty() &&
                playerLaunchSnapshots[4].audioRequests ==
                    std::vector<AudioCue>({AudioCue::PlayerFired}) &&
                playerLaunchGame.shells().size() == 1U &&
                playerLaunchGame.shells()[0].owner == ShellOwner::Player &&
                playerLaunchGame.shells()[0].ownerIndex == 1 &&
                playerLaunchGame.shells()[0].power &&
                !playerLaunchGame.shells()[0].impacting &&
                std::fabs(playerLaunchGame.shells()[0].life - 4.0f) <
                    0.0001f &&
                distanceSquared(playerLaunchGame.shells()[0].position,
                                expectedPlayerTwoShellPosition) < 0.000001f &&
                distanceSquared(playerLaunchGame.shells()[0].velocity,
                                expectedPlayerTwoShellVelocity) < 0.000001f &&
                playerLaunchGame.eventsThisUpdate().size() == 1U &&
                playerLaunchGame.eventsThisUpdate()[0].type ==
                    GameEventType::ShellFired &&
                playerLaunchGame.eventsThisUpdate()[0].cause ==
                    GameEventCause::PlayerShell &&
                playerLaunchGame.eventsThisUpdate()[0].sourcePlayerId == 1 &&
                playerLaunchGame.eventsThisUpdate()[0].direction ==
                    CardinalDirection::North &&
                distanceSquared(
                    playerLaunchSnapshots[0].intent.tankPosition,
                    playerTwoPosition) < 0.000001f &&
                distanceSquared(playerLaunchSnapshots[0].intent.position,
                                expectedPlayerTwoShellPosition) < 0.000001f,
            "player launch scalar payload or five-step adapter state changed"))
        return 1;

    Game3DTestAccess::setCameraShake(playerLaunchGame, 1, 0.30f);
    const std::size_t strongerShakeSnapshotStart =
        playerLaunchSnapshots.size();
    Game3DTestAccess::firePlayerForEvent(playerLaunchGame, 1);
    bool strongerShakePreserved =
        playerLaunchSnapshots.size() == strongerShakeSnapshotStart + 5U;
    for (std::size_t snapshotIndex = strongerShakeSnapshotStart;
         snapshotIndex < playerLaunchSnapshots.size(); ++snapshotIndex)
    {
        strongerShakePreserved = strongerShakePreserved &&
            std::fabs(playerLaunchSnapshots[snapshotIndex].cameraShake[1] -
                      0.30f) < 0.0001f;
    }
    if (!checkTest(
            strongerShakePreserved &&
                std::fabs(Game3DTestAccess::cameraShake(
                              playerLaunchGame, 1) -
                          0.30f) < 0.0001f &&
                playerShellCount(playerLaunchGame, 1) == 2,
            "player fire lowered a stronger existing camera shake"))
        return 1;

    RecordingAudioOutput fullSlotAudio;
    Game3D fullSlotGame(resourceRoot, 0x1f2a000aU, &fullSlotAudio);
    if (!checkTest(fullSlotGame.start(1, 3, 1, nations) &&
                       Game3DTestAccess::preparePlayerInputScenario(
                           fullSlotGame),
                   "could not prepare full player-shell slot arena"))
        return 1;
    Game3DTestAccess::addEventShell(
        fullSlotGame, ShellOwner::Player, 0, {3.0f, 3.0f}, {},
        false, true, kShellImpactDuration);
    Game3DTestAccess::addEventShell(
        fullSlotGame, ShellOwner::Enemy, 80, {5.0f, 5.0f},
        cardinalVector(CardinalDirection::East));
    Game3DTestAccess::addEventShell(
        fullSlotGame, ShellOwner::Player, 1, {7.0f, 7.0f},
        cardinalVector(CardinalDirection::West));
    std::vector<PlayerShellLaunchPresentationSnapshot> fullSlotSnapshots;
    std::vector<AudioCue> fullSlotAudioRequests;
    Game3DTestAccess::captureAudioRequests(fullSlotGame,
                                           fullSlotAudioRequests);
    Game3DTestAccess::capturePlayerShellLaunchPresentation(
        fullSlotGame, fullSlotAudioRequests, fullSlotSnapshots);
    fullSlotAudio.clear();
    PlayerInputFrame fillLastSlot;
    fillLastSlot.players[0].fireHeld = true;
    fullSlotGame.update(0.0f, fillLastSlot);
    const bool impactingShellOccupiedOneSlot =
        playerShellCount(fullSlotGame, 0) == 2 &&
        fullSlotGame.shells().size() == 4U &&
        fullSlotSnapshots.size() == 5U;
    const std::size_t shellCountBeforeRejection =
        fullSlotGame.shells().size();
    const std::size_t eventCountBeforeRejection =
        fullSlotGame.eventsThisUpdate().size();
    const std::size_t effectCountBeforeRejection =
        fullSlotGame.effects().activeCount();
    const std::array<float, 2> cameraBeforeRejection{{
        Game3DTestAccess::cameraShake(fullSlotGame, 0),
        Game3DTestAccess::cameraShake(fullSlotGame, 1)}};
    const std::size_t audioOutputCountBeforeRejection =
        fullSlotAudio.calls.size();
    const std::size_t audioRequestCountBeforeRejection =
        fullSlotAudioRequests.size();
    const std::size_t snapshotCountBeforeRejection =
        fullSlotSnapshots.size();
    Game3DTestAccess::firePlayerForEvent(fullSlotGame, 0);
    if (!checkTest(
            impactingShellOccupiedOneSlot &&
                fullSlotGame.shells().size() == shellCountBeforeRejection &&
                fullSlotGame.eventsThisUpdate().size() ==
                    eventCountBeforeRejection &&
                fullSlotGame.effects().activeCount() ==
                    effectCountBeforeRejection &&
                Game3DTestAccess::cameraShake(fullSlotGame, 0) ==
                    cameraBeforeRejection[0] &&
                Game3DTestAccess::cameraShake(fullSlotGame, 1) ==
                    cameraBeforeRejection[1] &&
                fullSlotAudio.calls.size() ==
                    audioOutputCountBeforeRejection &&
                fullSlotAudioRequests.size() ==
                    audioRequestCountBeforeRejection &&
                fullSlotSnapshots.size() == snapshotCountBeforeRejection &&
                std::fabs(fullSlotGame.players()[0].fireCooldown -
                          kPlayerReloadTime) < 0.0001f,
            "full player-shell slot changed reset or emitted presentation"))
        return 1;

    Game3D heldFireGame(resourceRoot, 0x1f2a0003U);
    if (!checkTest(prepareArena(heldFireGame, 1),
                   "could not prepare held-fire arena"))
        return 1;
    PlayerInputFrame heldFire;
    heldFire.players[0].fireHeld = true;
    heldFireGame.update(0.0f, heldFire);
    if (!checkTest(playerShellCount(heldFireGame, 0) == 1 &&
                       std::fabs(heldFireGame.players()[0].fireCooldown -
                                 kPlayerReloadTime) < 0.0001f,
                   "initial held fire did not create one shell and reset cooldown"))
        return 1;
    heldFireGame.update(0.05f, heldFire);
    heldFireGame.update(0.05f, heldFire);
    heldFireGame.update(0.021f, heldFire);
    const bool repeatedAtCooldown =
        playerShellCount(heldFireGame, 0) == 2;
    heldFireGame.update(0.05f, heldFire);
    heldFireGame.update(0.05f, heldFire);
    heldFireGame.update(0.021f, heldFire);
    if (!checkTest(repeatedAtCooldown &&
                       playerShellCount(heldFireGame, 0) == 2 &&
                       std::fabs(heldFireGame.players()[0].fireCooldown -
                                 kPlayerReloadTime) < 0.0001f,
                   "held-fire repeat or active-shell-cap cooldown reset changed"))
        return 1;
    const bool markedImpacting =
        Game3DTestAccess::markPlayerShellImpactingAndReadyToFire(
            heldFireGame, 0);
    heldFireGame.update(0.0f, heldFire);
    if (!checkTest(markedImpacting &&
                       playerShellCount(heldFireGame, 0) == 2 &&
                       std::any_of(
                           heldFireGame.shells().begin(),
                           heldFireGame.shells().end(),
                           [](const Shell &shell) {
                               return shell.owner == ShellOwner::Player &&
                                      shell.ownerIndex == 0 &&
                                      shell.impacting;
                           }) &&
                       std::fabs(heldFireGame.players()[0].fireCooldown -
                                 kPlayerReloadTime) < 0.0001f,
                   "impacting shell stopped occupying an active firing slot"))
        return 1;

    Game3D twoPlayerGame(resourceRoot, 0x1f2a0004U);
    if (!checkTest(prepareArena(twoPlayerGame, 2),
                   "could not prepare two-player input arena"))
        return 1;
    const XZ playerOneStart = twoPlayerGame.players()[0].position;
    const XZ playerTwoStart = twoPlayerGame.players()[1].position;
    PlayerInputFrame twoPlayerInput;
    pressDirection(twoPlayerInput.players[0].east);
    pressDirection(twoPlayerInput.players[1].west);
    twoPlayerInput.players[0].fireHeld = true;
    twoPlayerInput.players[1].fireHeld = true;
    twoPlayerGame.update(0.05f, twoPlayerInput);
    if (!checkTest(
            twoPlayerGame.players()[0].position.x > playerOneStart.x &&
                std::fabs(twoPlayerGame.players()[0].position.z -
                          playerOneStart.z) < 0.0001f &&
                twoPlayerGame.players()[1].position.x < playerTwoStart.x &&
                std::fabs(twoPlayerGame.players()[1].position.z -
                          playerTwoStart.z) < 0.0001f &&
                playerShellCount(twoPlayerGame, 0) == 1 &&
                playerShellCount(twoPlayerGame, 1) == 1,
            "P1 and P2 movement or fire inputs were not independent"))
        return 1;

    // The blocked-fire fixture above proves live player collision. This
    // geometry then distinguishes P2 seeing P1's old position (blocked) from
    // seeing P1's already-committed same-frame position (accepted).
    Game3D orderedPlayerGame(resourceRoot, 0x1f2a000cU);
    if (!checkTest(prepareArena(orderedPlayerGame, 2),
                   "could not prepare ordered two-player movement arena"))
        return 1;
    const XZ orderedPlayerOneStart =
        orderedPlayerGame.players()[0].position;
    const XZ orderedPlayerTwoStart{
        orderedPlayerOneStart.x - 1.90f, orderedPlayerOneStart.z};
    Game3DTestAccess::recenterPlayer(
        orderedPlayerGame, 1, orderedPlayerTwoStart);
    Game3DTestAccess::setPlayerLevel(orderedPlayerGame, 1, 0);
    PlayerInputFrame orderedPlayerInput;
    pressDirection(orderedPlayerInput.players[0].east);
    pressDirection(orderedPlayerInput.players[1].east);
    orderedPlayerGame.update(0.05f, orderedPlayerInput);
    const XZ expectedOrderedPlayerOne{
        orderedPlayerOneStart.x + 0.325f, orderedPlayerOneStart.z};
    const XZ expectedOrderedPlayerTwo{
        orderedPlayerTwoStart.x + 0.25f, orderedPlayerTwoStart.z};
    if (!checkTest(
            distanceSquared(orderedPlayerGame.players()[0].position,
                            expectedOrderedPlayerOne) < 0.000001f &&
                distanceSquared(orderedPlayerGame.players()[1].position,
                                expectedOrderedPlayerTwo) < 0.000001f &&
                orderedPlayerGame.players()[0].moving &&
                orderedPlayerGame.players()[1].moving,
            "P2 did not see P1's same-frame move or upgraded speed drifted"))
        return 1;

    Game3D boatGame(resourceRoot, 0x1f2a000dU);
    XZ boatAnchor{};
    if (!checkTest(
            boatGame.start(1, 3, kTerrainFixtureStage, nations) &&
                Game3DTestAccess::preparePlayerBoatInputScenario(
                    boatGame, boatAnchor),
            "could not find a safe 2x2 water movement fixture"))
        return 1;
    PlayerInputFrame eastOnWater;
    pressDirection(eastOnWater.players[0].east);
    boatGame.update(0.05f, eastOnWater);
    const bool waterBlockedWithoutBoat =
        distanceSquared(boatGame.players()[0].position, boatAnchor) <
            0.000001f &&
        !boatGame.players()[0].moving &&
        boatGame.players()[0].dustCooldown == 0.0f;
    Game3DTestAccess::setPlayerHasBoat(boatGame, 0, true);
    boatGame.update(0.05f, eastOnWater);
    const XZ expectedBoatPosition{boatAnchor.x + 0.25f, boatAnchor.z};
    if (!checkTest(
            waterBlockedWithoutBoat && boatGame.players()[0].hasBoat &&
                boatGame.players()[0].moving &&
                distanceSquared(boatGame.players()[0].position,
                                expectedBoatPosition) < 0.000001f &&
                std::fabs(boatGame.players()[0].dustCooldown -
                          kPlayerTrackDustCooldown) < 0.0001f,
            "hasBoat was not forwarded to the live water collision adapter"))
        return 1;

    Game3D iceGame(resourceRoot, 0x1f2a0005U);
    XZ iceAnchor{};
    if (!checkTest(iceGame.start(1, 3, kTerrainFixtureStage, nations) &&
                       Game3DTestAccess::preparePlayerIceInputScenario(
                           iceGame, iceAnchor),
                   "could not find a safe 2x2 ice input fixture"))
        return 1;
    PlayerInputFrame northOnIce;
    pressDirection(northOnIce.players[0].north);
    iceGame.update(0.0f, northOnIce);
    PlayerInputFrame turnEastOnIce;
    pressDirection(turnEastOnIce.players[0].east);
    turnEastOnIce.players[0].fireHeld = true;
    Game3DTestAccess::recenterPlayer(iceGame, 0, iceAnchor);
    iceGame.update(0.05f, turnEastOnIce);
    const auto iceFireEvent = std::find_if(
        iceGame.eventsThisUpdate().begin(),
        iceGame.eventsThisUpdate().end(),
        [](const GameEvent &event) {
            return event.type == GameEventType::ShellFired &&
                   event.sourcePlayerId == 0;
        });
    const bool iceFireUsedDriveDirection =
        iceFireEvent != iceGame.eventsThisUpdate().end() &&
        iceFireEvent->direction == CardinalDirection::East &&
        iceGame.players()[0].movementDirection == CardinalDirection::North;
    PlayerInputFrame holdEastOnIce;
    holdEastOnIce.players[0].east.held = true;
    for (int frame = 0; frame < 6; ++frame)
    {
        Game3DTestAccess::recenterPlayer(iceGame, 0, iceAnchor);
        iceGame.update(0.05f, holdEastOnIce);
    }
    const bool iceKeptOldTravel =
        iceGame.players()[0].driveDirection == CardinalDirection::East &&
        iceGame.players()[0].movementDirection == CardinalDirection::North;
    Game3DTestAccess::recenterPlayer(iceGame, 0, iceAnchor);
    iceGame.update(0.031f, holdEastOnIce);
    if (!checkTest(iceGame.map().isIce(iceAnchor) &&
                       iceFireUsedDriveDirection && iceKeptOldTravel &&
                       iceGame.players()[0].movementDirection ==
                           CardinalDirection::East &&
                       iceGame.players()[0].moving,
                   "ice fire direction or 380 ms turn delay changed"))
        return 1;

    XZ releaseAnchor{};
    if (!checkTest(Game3DTestAccess::preparePlayerIceInputScenario(
                       iceGame, releaseAnchor),
                   "could not reset the ice release fixture"))
        return 1;
    iceGame.update(0.0f, northOnIce);
    for (int frame = 0; frame < 7; ++frame)
    {
        Game3DTestAccess::recenterPlayer(iceGame, 0, releaseAnchor);
        iceGame.update(0.05f, {});
    }
    const bool releaseStillSliding = iceGame.players()[0].moving &&
        iceGame.players()[0].movementDirection == CardinalDirection::North;
    Game3DTestAccess::recenterPlayer(iceGame, 0, releaseAnchor);
    iceGame.update(0.031f, {});
    if (!checkTest(releaseStillSliding && !iceGame.players()[0].moving &&
                       iceGame.players()[0].driveDirection ==
                           CardinalDirection::North,
                   "released input did not preserve then end ice momentum"))
        return 1;

    Game3D introInputGame(resourceRoot, 0x1f2a0006U);
    if (!checkTest(introInputGame.start(1, 3, 1, nations),
                   introInputGame.lastError()))
        return 1;
    PlayerInputFrame ignoredIntroInput;
    pressDirection(ignoredIntroInput.players[0].east);
    ignoredIntroInput.players[0].fireHeld = true;
    const CardinalDirection introFacing =
        introInputGame.players()[0].driveDirection;
    introInputGame.update(0.05f, ignoredIntroInput);
    if (!checkTest(introInputGame.stageIntro() &&
                       introInputGame.players()[0].driveDirection ==
                           introFacing &&
                       introInputGame.shells().empty() &&
                       std::fabs(introInputGame.players()[0].creationTimer -
                                 1.0f) < 0.0001f,
                   "stage intro consumed or buffered scripted player input"))
        return 1;

    Game3D activeDeathBypassGame(resourceRoot, 0x1f2a000eU);
    if (!checkTest(prepareArena(activeDeathBypassGame, 1),
                   "could not prepare active death-bypass arena"))
        return 1;
    Game3DTestAccess::setPlayerDeathEntryState(
        activeDeathBypassGame, 0, 17, true, 2, 0.02f);
    activeDeathBypassGame.update(0.05f, {});
    if (!checkTest(activeDeathBypassGame.players()[0].active &&
                       activeDeathBypassGame.players()[0].lives == 2 &&
                       std::fabs(
                           activeDeathBypassGame.players()[0].deathTimer -
                           0.02f) < 0.0001f,
                   "active player entered the inactive death transaction"))
        return 1;

    Game3D playerGateGame(resourceRoot, 0x1f2a0007U);
    if (!checkTest(prepareArena(playerGateGame, 1),
                   "could not prepare player input gate arena"))
        return 1;
    Game3DTestAccess::setPlayerWaitingForDeath(playerGateGame, 0);
    Game3DTestAccess::setPlayerFrameEntryState(
        playerGateGame, 0, 0.73f, 0.64f, 0.43f, 0.52f, 0.31f,
        false);
    const Player inactiveBefore = playerGateGame.players()[0];
    playerGateGame.update(0.05f, ignoredIntroInput);
    const Player &inactiveAfter = playerGateGame.players()[0];
    if (!checkTest(
            std::fabs(inactiveAfter.dustCooldown - 0.38f) < 0.0001f &&
                std::fabs(inactiveAfter.shieldTimer - 0.47f) < 0.0001f &&
                std::fabs(inactiveAfter.streakPopupTimer - 0.26f) <
                    0.0001f &&
                std::fabs(inactiveAfter.creationTimer - 0.73f) < 0.0001f &&
                std::fabs(inactiveAfter.fireCooldown - 0.64f) < 0.0001f &&
                std::fabs(inactiveAfter.deathTimer -
                          (inactiveBefore.deathTimer - 0.05f)) < 0.0001f &&
                inactiveAfter.lives == inactiveBefore.lives,
            "inactive frame changed the unconditional clock debit or death transition"))
        return 1;
    if (!checkTest(!inactiveAfter.active &&
                       inactiveAfter.driveDirection ==
                           inactiveBefore.driveDirection &&
                       !playerGateGame.players()[0].moving &&
                       distanceSquared(inactiveAfter.position,
                                       inactiveBefore.position) == 0.0f &&
                       playerGateGame.shells().empty(),
                   "inactive death state consumed scripted input"))
        return 1;

    if (!Game3DTestAccess::preparePlayerInputScenario(playerGateGame))
        return 1;
    Game3DTestAccess::setPlayerFrameEntryState(
        playerGateGame, 0, 0.03f, 0.04f, 0.40f, 0.50f, 0.30f,
        true);
    const XZ creationStart = playerGateGame.players()[0].position;
    const CardinalDirection creationFacing =
        playerGateGame.players()[0].driveDirection;
    playerGateGame.update(0.05f, ignoredIntroInput);
    if (!checkTest(
            playerGateGame.players()[0].driveDirection == creationFacing &&
                distanceSquared(playerGateGame.players()[0].position,
                                creationStart) == 0.0f &&
                !playerGateGame.players()[0].moving &&
                playerGateGame.shells().empty() &&
                playerGateGame.players()[0].creationTimer == 0.0f &&
                std::fabs(playerGateGame.players()[0].fireCooldown - 0.04f) <
                    0.0001f &&
                std::fabs(playerGateGame.players()[0].dustCooldown - 0.35f) <
                    0.0001f &&
                std::fabs(playerGateGame.players()[0].shieldTimer - 0.45f) <
                    0.0001f &&
                std::fabs(
                    playerGateGame.players()[0].streakPopupTimer - 0.25f) <
                    0.0001f,
            "creation crossing fell through to input, movement, or fire"))
        return 1;
    playerGateGame.update(0.05f, {});
    if (!checkTest(
            playerGateGame.players()[0].driveDirection == creationFacing &&
                distanceSquared(playerGateGame.players()[0].position,
                                creationStart) == 0.0f &&
                !playerGateGame.players()[0].moving &&
                playerGateGame.shells().empty() &&
                playerGateGame.players()[0].creationTimer == 0.0f &&
                playerGateGame.players()[0].fireCooldown == 0.0f,
            "creation crossing buffered input or skipped next-frame fire debit"))
        return 1;

    if (!Game3DTestAccess::preparePlayerInputScenario(playerGateGame))
        return 1;
    Game3DTestAccess::setPlayerFrameEntryState(
        playerGateGame, 0, 0.0f, 0.44f, 0.33f, 0.22f, 0.11f,
        false);
    const XZ readyStart = playerGateGame.players()[0].position;
    const CardinalDirection readyFacing =
        playerGateGame.players()[0].driveDirection;
    playerGateGame.update(0.05f, {});
    if (!checkTest(
            playerGateGame.players()[0].creationTimer == 0.0f &&
                std::fabs(playerGateGame.players()[0].fireCooldown - 0.39f) <
                    0.0001f &&
                std::fabs(playerGateGame.players()[0].dustCooldown - 0.28f) <
                    0.0001f &&
                std::fabs(playerGateGame.players()[0].shieldTimer - 0.17f) <
                    0.0001f &&
                std::fabs(
                    playerGateGame.players()[0].streakPopupTimer - 0.06f) <
                    0.0001f &&
                playerGateGame.players()[0].driveDirection == readyFacing &&
                distanceSquared(playerGateGame.players()[0].position,
                                readyStart) == 0.0f &&
                !playerGateGame.players()[0].moving &&
                playerGateGame.shells().empty(),
            "ready empty-input frame changed the five-clock debit policy"))
        return 1;
    return 0;
}

bool eventTypesAre(const std::vector<GameEvent> &events,
                   std::initializer_list<GameEventType> expected)
{
    if (events.size() != expected.size())
        return false;
    return std::equal(events.begin(), events.end(), expected.begin(),
                      [](const GameEvent &event, GameEventType type) {
                          return event.type == type;
                      });
}

bool shellMapCorePresentationStepsAre(
    const std::vector<ShellMapCorePresentationSnapshot> &snapshots,
    std::initializer_list<ShellMapCorePresentationStep> expected)
{
    if (snapshots.size() != expected.size())
        return false;
    return std::equal(
        snapshots.begin(), snapshots.end(), expected.begin(),
        [](const ShellMapCorePresentationSnapshot &snapshot,
           ShellMapCorePresentationStep step) {
            return snapshot.step == step;
        });
}

template <typename Action>
const Action *shellMapCorePresentationActionAt(
    const std::vector<ShellMapCorePresentationSnapshot> &snapshots,
    std::size_t index)
{
    if (index >= snapshots.size())
        return nullptr;
    return std::get_if<Action>(&snapshots[index].action);
}

const Shell *latestShell(
    const ShellMapCorePresentationSnapshot &snapshot)
{
    return snapshot.shells.empty() ? nullptr : &snapshot.shells.back();
}

bool presentationFloat3Near(Float3 first, Float3 second,
                            float tolerance = 0.00001f)
{
    return std::fabs(first.x - second.x) <= tolerance &&
           std::fabs(first.y - second.y) <= tolerance &&
           std::fabs(first.z - second.z) <= tolerance;
}

bool presentationColorIs(Rgba8 value, Rgba8 expected)
{
    return value.r == expected.r && value.g == expected.g &&
           value.b == expected.b && value.a == expected.a;
}

bool shellTankPresentationStepsAre(
    const std::vector<ShellTankPresentationSnapshot> &snapshots,
    std::initializer_list<ShellTankPresentationStep> expected)
{
    if (snapshots.size() != expected.size())
        return false;
    return std::equal(
        snapshots.begin(), snapshots.end(), expected.begin(),
        [](const ShellTankPresentationSnapshot &snapshot,
           ShellTankPresentationStep step) {
            return snapshot.step == step;
        });
}

template <typename Action>
const Action *shellTankPresentationActionAt(
    const std::vector<ShellTankPresentationSnapshot> &snapshots,
    std::size_t index)
{
    if (index >= snapshots.size())
        return nullptr;
    return std::get_if<Action>(&snapshots[index].action);
}

bool shellTankImpactWasDeferred(
    const std::vector<ShellTankPresentationSnapshot> &snapshots)
{
    if (snapshots.size() < 2U)
        return false;
    const ShellTankPresentationSnapshot &committed = snapshots.back();
    const XZ contact = committed.physicalImpact.outcome.position;
    const XZ incomingVelocity =
        committed.physicalImpact.outcome.incomingVelocity;
    const bool stayedFlying = std::all_of(
        snapshots.begin(), snapshots.end() - 1,
        [&](const ShellTankPresentationSnapshot &snapshot) {
            return !snapshot.shell.impacting &&
                   lengthSquared(incomingVelocity) > 0.000001f &&
                   distanceSquared(snapshot.shell.velocity,
                                   incomingVelocity) < 0.000001f &&
                   distanceSquared(snapshot.shell.position, contact) <
                       0.000001f;
        });
    return stayedFlying && committed.shell.impacting &&
           lengthSquared(committed.shell.velocity) < 0.000001f &&
           distanceSquared(committed.shell.position, contact) < 0.000001f &&
           std::fabs(committed.shell.life - kShellImpactDuration) <
               0.000001f;
}

int eventCount(const std::vector<GameEvent> &events, GameEventType type)
{
    return static_cast<int>(std::count_if(
        events.begin(), events.end(), [&](const GameEvent &event) {
            return event.type == type;
        }));
}

struct DeterministicSessionCapture
{
    SessionDigest digest;
    std::vector<GameEvent> events;
    std::vector<std::size_t> eventCountsPerFrame;
};

bool captureDeterministicSession(const fs::path &resourceRoot,
                                 std::uint32_t seed,
                                 DeterministicSessionCapture &capture,
                                 std::string &error)
{
    const std::array<Nation, 2> nations{{Nation::UnitedStates,
                                         Nation::SovietUnion}};
    Game3D game(resourceRoot, seed);
    if (!game.start(1, 3, 1, nations))
    {
        error = game.lastError();
        return false;
    }

    capture.events.clear();
    capture.eventCountsPerFrame.clear();
    constexpr int kFrames = 480;
    for (int frame = 0; frame < kFrames; ++frame)
    {
        PlayerInputFrame input;
        if (frame >= 260 && frame < 450)
        {
            input.players[0].north.held = true;
            input.players[0].north.pressed = frame == 260;
            input.players[0].fireHeld = true;
        }
        game.update(1.0f / 60.0f, input);
        capture.eventCountsPerFrame.push_back(
            game.eventsThisUpdate().size());
        capture.events.insert(capture.events.end(),
                              game.eventsThisUpdate().begin(),
                              game.eventsThisUpdate().end());
    }
    capture.digest = game.sessionDigest();
    return true;
}

int runObservableGameEventSelfTests(const fs::path &resourceRoot)
{
    const std::array<Nation, 2> nations{{Nation::UnitedStates,
                                         Nation::SovietUnion}};
    constexpr ShellCancellationPresentationStep kBulletHitAudioRequested =
        ShellCancellationPresentationStep::BulletHitAudioRequested;

    Game3D firingGame(resourceRoot, 0xe0010001U);
    if (!checkTest(firingGame.start(1, 3, 1, nations) &&
                       Game3DTestAccess::prepareGameEventScenario(firingGame),
                   firingGame.lastError()))
        return 1;
    Game3DTestAccess::firePlayerForEvent(firingGame, 0);
    Game3DTestAccess::firePlayerForEvent(firingGame, 0);
    Game3DTestAccess::firePlayerForEvent(firingGame, 0);
    const std::vector<GameEvent> &playerFireEvents =
        firingGame.eventsThisUpdate();
    if (!checkTest(eventTypesAre(
                       playerFireEvents,
                       {GameEventType::ShellFired,
                        GameEventType::ShellFired}) &&
                       playerFireEvents[0].cause ==
                           GameEventCause::PlayerShell &&
                       playerFireEvents[0].sourcePlayerId == 0 &&
                       playerFireEvents[0].direction ==
                           CardinalDirection::North &&
                       firingGame.shells().size() == 2U,
                   "successful fire or active-shell rejection emitted the "
                   "wrong events"))
        return 1;
    firingGame.update(0.0f, {});
    if (!checkTest(firingGame.eventsThisUpdate().empty(),
                   "an event survived into the next simulation update"))
        return 1;

    Game3D enemyFireGame(resourceRoot, 0xe0010002U);
    if (!enemyFireGame.start(1, 3, 1, nations) ||
        !Game3DTestAccess::prepareGameEventScenario(enemyFireGame))
        return 1;
    Game3DTestAccess::addEventEnemy(enemyFireGame, 71, 2, 1,
                                    {12.0f, 10.0f});
    Game3DTestAccess::fireEnemyForEvent(enemyFireGame, 0);
    Shell expectedEnemyFireShell;
    expectedEnemyFireShell.position = shellSpawnPosition(
        {12.0f, 10.0f}, CardinalDirection::South);
    expectedEnemyFireShell.velocity =
        cardinalVector(CardinalDirection::South) * kFastShellSpeed;
    expectedEnemyFireShell.owner = ShellOwner::Enemy;
    expectedEnemyFireShell.ownerIndex = 71;
    GameEvent expectedEnemyFireEvent = shellEvent(
        GameEventType::ShellFired, expectedEnemyFireShell,
        expectedEnemyFireShell.position);
    expectedEnemyFireEvent.direction = CardinalDirection::South;
    if (!checkTest(
            enemyFireGame.shells().size() == 1U &&
                distanceSquared(enemyFireGame.shells()[0].position,
                                expectedEnemyFireShell.position) <
                    0.000001f &&
                distanceSquared(enemyFireGame.shells()[0].velocity,
                                expectedEnemyFireShell.velocity) <
                    0.000001f &&
                enemyFireGame.shells()[0].owner == ShellOwner::Enemy &&
                enemyFireGame.shells()[0].ownerIndex == 71 &&
                !enemyFireGame.shells()[0].power &&
                !enemyFireGame.shells()[0].impacting &&
                std::fabs(enemyFireGame.shells()[0].life - 4.0f) <
                    0.00001f &&
                enemyFireGame.eventsThisUpdate().size() == 1U &&
                enemyFireGame.eventsThisUpdate()[0] ==
                    expectedEnemyFireEvent &&
                enemyFireGame.effects().activeCount() > 0U &&
                std::fabs(enemyFireGame.enemies()[0].fireCooldown - 0.4f) <
                    0.00001f &&
                Game3DTestAccess::cameraShake(enemyFireGame, 0) == 0.0f &&
                Game3DTestAccess::cameraShake(enemyFireGame, 1) == 0.0f,
            "enemy Power shell, event, muzzle FX, reload, or quiet camera "
            "payload changed"))
        return 1;

    std::vector<ShellCancellationPresentationSnapshot>
        cancellationPresentationRecords;
    Game3D cancellationGame(resourceRoot, 0xe0010003U);
    if (!cancellationGame.start(1, 3, 1, nations) ||
        !Game3DTestAccess::prepareGameEventScenario(cancellationGame))
        return 1;
    Game3DTestAccess::addEventShell(cancellationGame, ShellOwner::Player, 0,
                                    {12.0f, 10.0f}, {10.0f, 0.0f});
    Game3DTestAccess::addEventShell(cancellationGame, ShellOwner::Enemy, 81,
                                    {12.0f, 10.0f}, {-10.0f, 0.0f});
    Game3DTestAccess::captureShellCancellationPresentation(
        cancellationGame, cancellationPresentationRecords);
    const std::size_t cancellationEffectCountBefore =
        cancellationGame.effects().activeCount();
    cancellationGame.update(0.0f, {});
    if (!checkTest(eventTypesAre(cancellationGame.eventsThisUpdate(),
                                 {GameEventType::ShellCancelled}) &&
                       cancellationGame.eventsThisUpdate()[0].sourcePlayerId == 0 &&
                       cancellationGame.eventsThisUpdate()[0].sourceEnemyId == 81 &&
                       cancellationGame.shells().size() == 2U &&
                       cancellationGame.shells()[0].impacting &&
                       cancellationGame.shells()[1].impacting,
                   "opposing-shell cancellation did not emit one pair event"))
        return 1;
    if (!checkTest(
            cancellationPresentationRecords.size() == 3U &&
                cancellationPresentationRecords[0].step ==
                    ShellCancellationPresentationStep::EventAppended &&
                cancellationPresentationRecords[0]
                        .command.appendEvent.event ==
                    cancellationGame.eventsThisUpdate()[0] &&
                cancellationPresentationRecords[1].step ==
                    ShellCancellationPresentationStep::ImpactFxSpawned &&
                std::fabs(
                    cancellationPresentationRecords[1]
                            .command.spawnImpact.position.x -
                    12.0f) < 0.001f &&
                std::fabs(
                    cancellationPresentationRecords[1]
                            .command.spawnImpact.elevation -
                    0.67f) < 0.001f &&
                std::fabs(
                    cancellationPresentationRecords[1]
                            .command.spawnImpact.position.z -
                    10.0f) < 0.001f &&
                std::fabs(
                    cancellationPresentationRecords[1]
                        .command.spawnImpact.normalXZ.x) <
                    0.001f &&
                std::fabs(
                    cancellationPresentationRecords[1]
                            .command.spawnImpact.normalY -
                    1.0f) < 0.001f &&
                std::fabs(
                    cancellationPresentationRecords[1]
                        .command.spawnImpact.normalXZ.z) <
                    0.001f &&
                !cancellationPresentationRecords[1]
                     .command.spawnImpact.heavy &&
                cancellationPresentationRecords[2].step ==
                    kBulletHitAudioRequested &&
                cancellationPresentationRecords[2]
                        .command.requestAudio.cue ==
                    AudioCue::BulletHit &&
                std::all_of(
                    cancellationPresentationRecords.begin(),
                    cancellationPresentationRecords.end(),
                    [](const ShellCancellationPresentationSnapshot &snapshot) {
                        return distanceSquared(
                                   snapshot.command.appendEvent.event.position,
                                   {12.0f, 10.0f}) < 0.000001f &&
                               distanceSquared(
                                   snapshot.command.spawnImpact.position,
                                   {12.0f, 10.0f}) < 0.000001f;
                    }) &&
                cancellationPresentationRecords[0].eventCount == 1U &&
                cancellationPresentationRecords[1].eventCount == 1U &&
                cancellationPresentationRecords[2].eventCount == 1U &&
                cancellationPresentationRecords[0].effectCount ==
                    cancellationEffectCountBefore &&
                cancellationPresentationRecords[1].effectCount >
                    cancellationPresentationRecords[0].effectCount &&
                cancellationPresentationRecords[2].effectCount ==
                    cancellationPresentationRecords[1].effectCount &&
                std::all_of(
                    cancellationPresentationRecords.begin(),
                    cancellationPresentationRecords.end(),
                    [](const ShellCancellationPresentationSnapshot &snapshot) {
                        return snapshot.impactingShellCount == 2U;
                    }) &&
                cancellationGame.effects().activeCount() ==
                    cancellationPresentationRecords[2].effectCount,
            "single cancellation presentation lost event-FX-audio order or "
            "payload"))
        return 1;
    cancellationGame.update(0.0f, {});
    if (!checkTest(cancellationGame.eventsThisUpdate().empty() &&
                       cancellationPresentationRecords.size() == 3U,
                   "impacting shell pair emitted cancellation twice"))
        return 1;

    Game3D sweptCancellationGame(resourceRoot, 0xe0011003U);
    if (!sweptCancellationGame.start(1, 3, 1, nations) ||
        !Game3DTestAccess::prepareGameEventScenario(sweptCancellationGame))
        return 1;
    Game3DTestAccess::addEventShell(
        sweptCancellationGame, ShellOwner::Player, 0,
        {10.0f, 10.0f}, {10.0f, 0.0f});
    Game3DTestAccess::addEventShell(
        sweptCancellationGame, ShellOwner::Enemy, 181,
        {11.0f, 10.0f}, {-10.0f, 0.0f});
    sweptCancellationGame.update(0.05f, {});
    const std::vector<GameEvent> &sweptCancellationEvents =
        sweptCancellationGame.eventsThisUpdate();
    if (!checkTest(
            eventTypesAre(sweptCancellationEvents,
                          {GameEventType::ShellCancelled}) &&
                std::fabs(sweptCancellationEvents[0].position.x - 10.5f) <
                    0.001f &&
                std::fabs(sweptCancellationEvents[0].position.z - 10.0f) <
                    0.001f &&
                sweptCancellationEvents[0].sourcePlayerId == 0 &&
                sweptCancellationEvents[0].sourceEnemyId == 181 &&
                sweptCancellationEvents[0].cause == GameEventCause::None &&
                sweptCancellationGame.shells().size() == 2U &&
                sweptCancellationGame.shells()[0].impacting &&
                sweptCancellationGame.shells()[1].impacting,
            "moving shell crossing lost its cancellation event snapshot"))
        return 1;

    std::vector<ShellCancellationPresentationSnapshot>
        orderedCancellationPresentationRecords;
    Game3D cancellationOrderGame(resourceRoot, 0xe0010013U);
    if (!cancellationOrderGame.start(1, 3, 2, nations) ||
        !Game3DTestAccess::prepareGameEventScenario(cancellationOrderGame))
        return 1;
    Game3DTestAccess::addEventShell(
        cancellationOrderGame, ShellOwner::Player, 0,
        {5.0f, 5.0f}, {});
    Game3DTestAccess::addEventShell(
        cancellationOrderGame, ShellOwner::Enemy, 82,
        {5.0f, 5.0f}, {});
    Game3DTestAccess::addEventShell(
        cancellationOrderGame, ShellOwner::Enemy, 83,
        {20.0f, 5.0f}, {});
    Game3DTestAccess::addEventShell(
        cancellationOrderGame, ShellOwner::Player, 1,
        {20.0f, 5.0f}, {});
    Game3DTestAccess::captureShellCancellationPresentation(
        cancellationOrderGame, orderedCancellationPresentationRecords);
    cancellationOrderGame.update(0.0f, {});
    const std::vector<GameEvent> &orderedCancellationEvents =
        cancellationOrderGame.eventsThisUpdate();
    if (!checkTest(
            eventTypesAre(orderedCancellationEvents,
                          {GameEventType::ShellCancelled,
                           GameEventType::ShellCancelled}) &&
                orderedCancellationEvents[0].sourcePlayerId == 0 &&
                orderedCancellationEvents[0].sourceEnemyId == 82 &&
                orderedCancellationEvents[1].sourcePlayerId == 1 &&
                orderedCancellationEvents[1].sourceEnemyId == 83 &&
                cancellationOrderGame.shells().size() == 4U &&
                std::all_of(cancellationOrderGame.shells().begin(),
                            cancellationOrderGame.shells().end(),
                            [](const Shell &shell) {
                                return shell.impacting;
                            }),
            "multiple shell cancellations lost greedy event order or owners"))
        return 1;
    if (!checkTest(
            orderedCancellationPresentationRecords.size() == 6U &&
                orderedCancellationPresentationRecords[0].step ==
                    ShellCancellationPresentationStep::EventAppended &&
                orderedCancellationPresentationRecords[1].step ==
                    ShellCancellationPresentationStep::ImpactFxSpawned &&
                orderedCancellationPresentationRecords[2].step ==
                    kBulletHitAudioRequested &&
                orderedCancellationPresentationRecords[3].step ==
                    ShellCancellationPresentationStep::EventAppended &&
                orderedCancellationPresentationRecords[4].step ==
                    ShellCancellationPresentationStep::ImpactFxSpawned &&
                orderedCancellationPresentationRecords[5].step ==
                    kBulletHitAudioRequested &&
                orderedCancellationPresentationRecords[0]
                        .command.appendEvent.event ==
                    orderedCancellationEvents[0] &&
                orderedCancellationPresentationRecords[3]
                        .command.appendEvent.event ==
                    orderedCancellationEvents[1] &&
                distanceSquared(
                    orderedCancellationPresentationRecords[0]
                        .command.spawnImpact.position,
                    {5.0f, 5.0f}) < 0.000001f &&
                distanceSquared(
                    orderedCancellationPresentationRecords[1]
                        .command.spawnImpact.position,
                    {5.0f, 5.0f}) < 0.000001f &&
                distanceSquared(
                    orderedCancellationPresentationRecords[2]
                        .command.spawnImpact.position,
                    {5.0f, 5.0f}) < 0.000001f &&
                distanceSquared(
                    orderedCancellationPresentationRecords[3]
                        .command.spawnImpact.position,
                    {20.0f, 5.0f}) < 0.000001f &&
                distanceSquared(
                    orderedCancellationPresentationRecords[4]
                        .command.spawnImpact.position,
                    {20.0f, 5.0f}) < 0.000001f &&
                distanceSquared(
                    orderedCancellationPresentationRecords[5]
                        .command.spawnImpact.position,
                    {20.0f, 5.0f}) < 0.000001f &&
                orderedCancellationPresentationRecords[2]
                        .command.requestAudio.cue ==
                    AudioCue::BulletHit &&
                orderedCancellationPresentationRecords[5]
                        .command.requestAudio.cue ==
                    AudioCue::BulletHit &&
                orderedCancellationPresentationRecords[0].eventCount == 1U &&
                orderedCancellationPresentationRecords[1].eventCount == 1U &&
                orderedCancellationPresentationRecords[2].eventCount == 1U &&
                orderedCancellationPresentationRecords[3].eventCount == 2U &&
                orderedCancellationPresentationRecords[4].eventCount == 2U &&
                orderedCancellationPresentationRecords[5].eventCount == 2U &&
                orderedCancellationPresentationRecords[1].effectCount >
                    orderedCancellationPresentationRecords[0].effectCount &&
                orderedCancellationPresentationRecords[2].effectCount ==
                    orderedCancellationPresentationRecords[1].effectCount &&
                orderedCancellationPresentationRecords[3].effectCount ==
                    orderedCancellationPresentationRecords[2].effectCount &&
                orderedCancellationPresentationRecords[4].effectCount >
                    orderedCancellationPresentationRecords[3].effectCount &&
                orderedCancellationPresentationRecords[5].effectCount ==
                    orderedCancellationPresentationRecords[4].effectCount &&
                std::all_of(
                    orderedCancellationPresentationRecords.begin(),
                    orderedCancellationPresentationRecords.end(),
                    [](const ShellCancellationPresentationSnapshot &snapshot) {
                        return snapshot.impactingShellCount == 4U;
                    }),
            "multiple cancellations stopped interleaving event-FX-audio per "
            "pair"))
        return 1;

    std::vector<ShellMapCorePresentationSnapshot> brickPresentation;
    Game3D brickGame(resourceRoot, 0xe0010004U);
    if (!brickGame.start(1, 3, 1, nations) ||
        !Game3DTestAccess::prepareGameEventScenario(brickGame, false))
        return 1;
    int brickRow = -1;
    int brickColumn = -1;
    for (int row = 0; row < kMapSize && brickRow < 0; ++row)
        for (int column = 0; column < kMapSize; ++column)
            if (brickGame.map().tile(row, column) == '#')
            {
                brickRow = row;
                brickColumn = column;
                break;
            }
    const unsigned char brickBefore = brickRow >= 0
                                          ? brickGame.map().brickMask(
                                                brickRow, brickColumn)
                                          : 0U;
    Game3DTestAccess::addEventShell(
        brickGame, ShellOwner::Player, 0,
        {brickColumn + 0.5f, brickRow + 0.5f}, {0.0f, -1.0f});
    Game3DTestAccess::captureShellMapCorePresentation(
        brickGame, brickPresentation);
    brickGame.update(0.0f, {});
    if (!checkTest(brickRow >= 0 &&
                       eventTypesAre(brickGame.eventsThisUpdate(),
                                     {GameEventType::BrickHit}) &&
                       brickGame.eventsThisUpdate()[0].row == brickRow &&
                       brickGame.eventsThisUpdate()[0].column == brickColumn &&
                       brickGame.eventsThisUpdate()[0].valueBefore ==
                           brickBefore &&
                       brickGame.eventsThisUpdate()[0].valueAfter ==
                           brickGame.map().brickMask(brickRow, brickColumn) &&
                       brickGame.map().brickMask(brickRow, brickColumn) !=
                           brickBefore,
                   "brick mutation and BrickHit payload diverged"))
        return 1;
    const auto *brickAppend =
        shellMapCorePresentationActionAt<AppendMapCoreEventsAction>(
            brickPresentation, 0U);
    const auto *brickFx =
        shellMapCorePresentationActionAt<
            SpawnMapCoreBrickImpactAction>(brickPresentation, 1U);
    const auto *brickAudio =
        shellMapCorePresentationActionAt<RequestMapCoreAudioAction>(
            brickPresentation, 2U);
    const auto *brickCommit =
        shellMapCorePresentationActionAt<
            CommitMapCoreShellImpactAction>(brickPresentation, 3U);
    const Shell *brickCommittedShell =
        brickPresentation.size() > 3U
            ? latestShell(brickPresentation[3])
            : nullptr;
    if (!checkTest(
            shellMapCorePresentationStepsAre(
                brickPresentation,
                {ShellMapCorePresentationStep::PhysicalEventsAppended,
                 ShellMapCorePresentationStep::BrickImpactFxSpawned,
                 ShellMapCorePresentationStep::AudioRequested,
                 ShellMapCorePresentationStep::ShellImpactCommitted}) &&
                brickPresentation[0].eventCount == 1U &&
                brickAppend != nullptr &&
                brickAppend->events == brickGame.eventsThisUpdate() &&
                brickPresentation[1].effectCount >
                    brickPresentation[0].effectCount &&
                brickFx != nullptr &&
                presentationFloat3Near(
                    brickFx->position,
                    {brickColumn + 0.5f, 0.47f,
                     brickRow + 0.5f}) &&
                presentationFloat3Near(
                    brickFx->normal, {0.0f, 0.28f, 1.0f}) &&
                !brickFx->power &&
                brickFx->destroyed ==
                    (brickGame.map().brickMask(brickRow, brickColumn) == 0U) &&
                brickAudio != nullptr &&
                brickAudio->cue == AudioCue::BrickHit &&
                brickCommit != nullptr &&
                distanceSquared(
                    brickCommit->position,
                    {brickColumn + 0.5f, brickRow + 0.5f}) < 0.000001f &&
                brickCommittedShell != nullptr &&
                brickCommittedShell->impacting &&
                distanceSquared(brickCommittedShell->position,
                                brickCommit->position) < 0.000001f &&
                lengthSquared(brickCommittedShell->velocity) <
                    0.000001f &&
                std::fabs(brickCommittedShell->life -
                          kShellImpactDuration) < 0.000001f,
            "player brick presentation lost event-FX-audio-request-impact "
            "order"))
        return 1;

    Game3D impactPriorityGame(resourceRoot, 0xe0011004U);
    if (!impactPriorityGame.start(1, 3, 1, nations) ||
        !Game3DTestAccess::prepareGameEventScenario(impactPriorityGame, false))
        return 1;
    Game3DTestAccess::addEventShell(
        impactPriorityGame, ShellOwner::Player, 0,
        {brickColumn + 0.5f, brickRow + 0.5f}, {0.0f, -1.0f});
    Game3DTestAccess::addEventShell(
        impactPriorityGame, ShellOwner::Enemy, 82,
        {brickColumn + 0.5f, brickRow + 0.5f}, {0.0f, 1.0f});
    impactPriorityGame.update(0.0f, {});
    if (!checkTest(eventTypesAre(
                       impactPriorityGame.eventsThisUpdate(),
                       {GameEventType::BrickHit,
                        GameEventType::BrickHit}) &&
                       eventCount(impactPriorityGame.eventsThisUpdate(),
                                  GameEventType::ShellCancelled) == 0,
                   "physical brick impacts did not precede shell cancellation"))
        return 1;

    Game3D wallBeforePlayerGame(resourceRoot, 0xe0012004U);
    if (!wallBeforePlayerGame.start(1, 3, 1, nations) ||
        !Game3DTestAccess::prepareGameEventScenario(wallBeforePlayerGame,
                                                    false))
        return 1;
    const XZ wallBeforePlayerPosition{
        brickColumn + 0.5f, brickRow + 0.5f};
    Game3DTestAccess::recenterPlayer(wallBeforePlayerGame, 0,
                                    wallBeforePlayerPosition);
    Game3DTestAccess::addEventShell(
        wallBeforePlayerGame, ShellOwner::Enemy, 84,
        wallBeforePlayerPosition, {0.0f, 1.0f});
    wallBeforePlayerGame.update(0.0f, {});
    if (!checkTest(
            eventTypesAre(wallBeforePlayerGame.eventsThisUpdate(),
                          {GameEventType::BrickHit}) &&
                wallBeforePlayerGame.players()[0].hitPoints == 3 &&
                wallBeforePlayerGame.players()[0].active &&
                wallBeforePlayerGame.shells().size() == 1U &&
                wallBeforePlayerGame.shells()[0].impacting,
            "brick impact stopped taking priority over a player-tank hit"))
        return 1;

    Game3D forestThroughTankGame(resourceRoot, 0xe0013004U);
    if (!forestThroughTankGame.start(1, 3, 10, nations) ||
        !Game3DTestAccess::prepareGameEventScenario(forestThroughTankGame,
                                                    false))
        return 1;
    int forestThroughRow = -1;
    int forestThroughColumn = -1;
    for (int row = 0; row < kMapSize && forestThroughRow < 0; ++row)
        for (int column = 0; column < kMapSize; ++column)
            if (forestThroughTankGame.map().tile(row, column) == '%')
            {
                forestThroughRow = row;
                forestThroughColumn = column;
                break;
            }
    const XZ forestThroughPosition{
        forestThroughColumn + 0.5f, forestThroughRow + 0.5f};
    Game3DTestAccess::recenterPlayer(forestThroughTankGame, 0,
                                    {-5.0f, -5.0f});
    Game3DTestAccess::addEventEnemy(forestThroughTankGame, 41, 2, 2,
                                    forestThroughPosition);
    Game3DTestAccess::addEventShell(
        forestThroughTankGame, ShellOwner::Player, 0,
        forestThroughPosition, {1.0f, 0.0f}, true);
    forestThroughTankGame.update(0.0f, {});
    if (!checkTest(
            forestThroughRow >= 0 && forestThroughColumn >= 0 &&
                forestThroughTankGame.map().tile(
                    forestThroughRow, forestThroughColumn) == '.' &&
                eventTypesAre(forestThroughTankGame.eventsThisUpdate(),
                              {GameEventType::TankDamaged}) &&
                forestThroughTankGame.eventsThisUpdate()[0].targetEnemyId ==
                    41 &&
                forestThroughTankGame.enemies()[0].armor == 1 &&
                forestThroughTankGame.shells().size() == 1U &&
                forestThroughTankGame.shells()[0].impacting &&
                forestThroughTankGame.shells()[0].position.x ==
                    forestThroughPosition.x &&
                forestThroughTankGame.shells()[0].position.z ==
                    forestThroughPosition.z,
            "power-shell forest pass-through stopped before the enemy commit"))
        return 1;

    std::vector<ShellTankPresentationSnapshot> enemyTankPresentation;
    Game3D enemyPresentationGame(resourceRoot, 0xe0014004U);
    if (!enemyPresentationGame.start(2, 3, 1, nations) ||
        !Game3DTestAccess::prepareGameEventScenario(
            enemyPresentationGame))
        return 1;
    const XZ enemyPresentationPosition{12.0f, 12.0f};
    Game3DTestAccess::recenterPlayer(enemyPresentationGame, 0,
                                    {10.0f, 12.0f});
    Game3DTestAccess::recenterPlayer(enemyPresentationGame, 1,
                                    {11.0f, 12.0f});
    Game3DTestAccess::setCameraShake(enemyPresentationGame, 1, 0.60f);
    Game3DTestAccess::addEventEnemy(
        enemyPresentationGame, 47, 2, 2, enemyPresentationPosition, true);
    const bool defaultBonusReleaseObserverEmpty =
        !Game3DTestAccess::hasBonusReleasePresentationObserver(
            enemyPresentationGame);
    std::vector<AudioCue> carrierAudioRequests;
    std::vector<BonusReleasePresentationSnapshot>
        bonusReleasePresentation;
    Game3DTestAccess::captureAudioRequests(
        enemyPresentationGame, carrierAudioRequests);
    Game3DTestAccess::captureBonusReleasePresentation(
        enemyPresentationGame, carrierAudioRequests,
        bonusReleasePresentation);
    Game3DTestAccess::captureShellTankPresentation(
        enemyPresentationGame, enemyTankPresentation);
    const auto bonusReleaseOrderMatches =
        [&](std::size_t previousBonusCount, int enemyArmor,
            int playerScore, const GameEvent &expectedEvent) {
            if (bonusReleasePresentation.size() != 3U)
                return false;
            const bool stepOrder =
                bonusReleasePresentation[0].step ==
                    BonusReleasePresentationStep::PickupInserted &&
                bonusReleasePresentation[1].step ==
                    BonusReleasePresentationStep::SpawnEventAppended &&
                bonusReleasePresentation[2].step ==
                    BonusReleasePresentationStep::AudioRequested;
            const bool stageStates =
                bonusReleasePresentation[0].bonuses.size() ==
                    previousBonusCount + 1U &&
                bonusReleasePresentation[0].events.empty() &&
                bonusReleasePresentation[0].audioRequests.empty() &&
                bonusReleasePresentation[1].bonuses.size() ==
                    previousBonusCount + 1U &&
                bonusReleasePresentation[1].events ==
                    std::vector<GameEvent>{expectedEvent} &&
                bonusReleasePresentation[1].audioRequests.empty() &&
                bonusReleasePresentation[2].bonuses.size() ==
                    previousBonusCount + 1U &&
                bonusReleasePresentation[2].events ==
                    std::vector<GameEvent>{expectedEvent} &&
                bonusReleasePresentation[2].audioRequests ==
                    std::vector<AudioCue>{AudioCue::BonusAppeared};
            const bool detachedPayloadAndPreCommit = std::all_of(
                bonusReleasePresentation.begin(),
                bonusReleasePresentation.end(),
                [&](const BonusReleasePresentationSnapshot &snapshot) {
                    if (snapshot.bonuses.empty() ||
                        snapshot.players.size() != 2U ||
                        snapshot.enemies.size() != 1U)
                    {
                        return false;
                    }
                    const Pickup &committed = snapshot.bonuses.back();
                    return snapshot.intent.sourceEnemyId == 47 &&
                           snapshot.intent.pickup.type == committed.type &&
                           snapshot.intent.pickup.position.x ==
                               committed.position.x &&
                           snapshot.intent.pickup.position.z ==
                               committed.position.z &&
                           snapshot.intent.pickup.age == committed.age &&
                           snapshot.intent.pickup.life == committed.life &&
                           committed.type == expectedEvent.bonusType &&
                           committed.position.x == expectedEvent.position.x &&
                           committed.position.z == expectedEvent.position.z &&
                           committed.age == 0.0f &&
                           committed.life == kPickupLifetime &&
                           snapshot.players[0].score == playerScore &&
                           snapshot.enemies[0].armor == enemyArmor &&
                           !snapshot.enemies[0].destroyed;
                });
            return stepOrder && stageStates &&
                   detachedPayloadAndPreCommit;
        };
    const std::size_t enemyHitEffectCountBefore =
        enemyPresentationGame.effects().activeCount();
    const float enemyHitCameraBefore =
        Game3DTestAccess::cameraShake(enemyPresentationGame, 0);
    Game3DTestAccess::addEventShell(
        enemyPresentationGame, ShellOwner::Player, 0,
        {12.25f, 12.0f}, {1.0f, 0.0f});
    enemyPresentationGame.update(0.0f, {});
    const auto *enemyHitEvents =
        shellTankPresentationActionAt<AppendTankEventsAction>(
            enemyTankPresentation, 0U);
    const auto *enemyHitArmor =
        shellTankPresentationActionAt<SpawnTankArmorImpactAction>(
            enemyTankPresentation, 1U);
    const auto *enemyHitRadial =
        shellTankPresentationActionAt<ApplyTankRadialCameraShakeAction>(
            enemyTankPresentation, 2U);
    const auto *enemyHitAudio =
        shellTankPresentationActionAt<RequestTankAudioAction>(
            enemyTankPresentation, 3U);
    const auto *enemyHitImpact =
        shellTankPresentationActionAt<CommitTankShellImpactAction>(
            enemyTankPresentation, 4U);
    const bool firstBonusReleaseOrder =
        !enemyPresentationGame.eventsThisUpdate().empty() &&
        bonusReleaseOrderMatches(0U, 2, 0,
                                 enemyPresentationGame.eventsThisUpdate()[0]);
    if (!checkTest(
            shellTankPresentationStepsAre(
                enemyTankPresentation,
                {ShellTankPresentationStep::PhysicalEventsAppended,
                 ShellTankPresentationStep::ArmorImpactFxSpawned,
                 ShellTankPresentationStep::CameraShakePassCompleted,
                 ShellTankPresentationStep::AudioRequested,
                 ShellTankPresentationStep::ShellImpactCommitted}) &&
                shellTankImpactWasDeferred(enemyTankPresentation) &&
                eventTypesAre(
                    enemyPresentationGame.eventsThisUpdate(),
                    {GameEventType::BonusSpawned,
                     GameEventType::TankDamaged}) &&
                enemyTankPresentation[0].eventCount == 2U &&
                enemyTankPresentation[0].effectCount ==
                    enemyHitEffectCountBefore &&
                enemyTankPresentation[1].effectCount >
                    enemyHitEffectCountBefore &&
                std::fabs(enemyTankPresentation[2].cameraShake[0] -
                          0.114f) < 0.000001f &&
                enemyTankPresentation[2].cameraShake[0] >
                    enemyHitCameraBefore &&
                enemyTankPresentation[2].cameraShake[1] == 0.60f &&
                enemyTankPresentation[3].audioCue == AudioCue::EnemyHit &&
                defaultBonusReleaseObserverEmpty &&
                firstBonusReleaseOrder &&
                carrierAudioRequests == std::vector<AudioCue>({
                    AudioCue::BonusAppeared, AudioCue::EnemyHit}) &&
                enemyHitEvents != nullptr &&
                enemyHitEvents->events.size() == 1U &&
                enemyHitEvents->events[0] ==
                    enemyPresentationGame.eventsThisUpdate()[1] &&
                enemyHitArmor != nullptr && enemyHitArmor->heavy &&
                enemyHitArmor->position.x == 12.25f &&
                enemyHitArmor->position.y == 0.52f &&
                enemyHitArmor->position.z == 12.0f &&
                enemyHitArmor->normal.x == -1.0f &&
                enemyHitArmor->normal.y == 0.28f &&
                enemyHitArmor->normal.z == 0.0f &&
                enemyHitRadial != nullptr &&
                enemyHitRadial->origin.x == enemyPresentationPosition.x &&
                enemyHitRadial->origin.z == enemyPresentationPosition.z &&
                enemyHitRadial->maximum == 0.13f &&
                enemyHitRadial->distanceFalloff == 0.008f &&
                enemyHitAudio != nullptr &&
                enemyHitAudio->cue == AudioCue::EnemyHit &&
                enemyHitImpact != nullptr &&
                enemyHitImpact->position.x == 12.25f &&
                enemyHitImpact->position.z == 12.0f &&
                std::all_of(
                    enemyTankPresentation.begin(),
                    enemyTankPresentation.end(),
                    [](const ShellTankPresentationSnapshot &snapshot) {
                        return !snapshot.preCommitFx &&
                               snapshot.physicalImpact.outcome.target ==
                                   CombatTarget::EnemyTank &&
                               snapshot.physicalImpact.enemyCommit.applied &&
                               !snapshot.physicalImpact.enemyCommit
                                    .destroyedNow &&
                               snapshot.enemies.size() == 1U &&
                               snapshot.enemies[0].armor == 1 &&
                               !snapshot.enemies[0].destroyed;
                    }) &&
                enemyTankPresentation[4].shell.impacting &&
                lengthSquared(enemyTankPresentation[4].shell.velocity) <
                    0.000001f &&
                std::fabs(enemyTankPresentation[4].shell.life -
                          kShellImpactDuration) < 0.000001f,
            "nonfatal enemy presentation lost event-armor-FX-camera-audio-"
            "request-impact order"))
        return 1;

    enemyTankPresentation.clear();
    bonusReleasePresentation.clear();
    carrierAudioRequests.clear();
    Game3DTestAccess::setCameraShake(enemyPresentationGame, 0, 0.50f);
    const std::size_t enemyDeathEffectCountBefore =
        enemyPresentationGame.effects().activeCount();
    const float enemyDeathCameraBefore =
        Game3DTestAccess::cameraShake(enemyPresentationGame, 0);
    Game3DTestAccess::addEventShell(
        enemyPresentationGame, ShellOwner::Player, 0,
        {12.25f, 12.0f}, {1.0f, 0.0f});
    enemyPresentationGame.update(0.0f, {});
    const auto *enemyDeathExplosion =
        shellTankPresentationActionAt<SpawnTankExplosionAction>(
            enemyTankPresentation, 0U);
    const auto *enemyDeathRadial =
        shellTankPresentationActionAt<ApplyTankRadialCameraShakeAction>(
            enemyTankPresentation, 1U);
    const auto *enemyDeathAudio =
        shellTankPresentationActionAt<RequestTankAudioAction>(
            enemyTankPresentation, 2U);
    const auto *enemyDeathEvents =
        shellTankPresentationActionAt<AppendTankEventsAction>(
            enemyTankPresentation, 3U);
    const auto *enemyDeathImpact =
        shellTankPresentationActionAt<CommitTankShellImpactAction>(
            enemyTankPresentation, 4U);
    const bool secondBonusReleaseOrder =
        !enemyPresentationGame.eventsThisUpdate().empty() &&
        bonusReleaseOrderMatches(1U, 1, 50,
                                 enemyPresentationGame.eventsThisUpdate()[0]);
    if (!checkTest(
            shellTankPresentationStepsAre(
                enemyTankPresentation,
                {ShellTankPresentationStep::TankExplosionFxSpawned,
                 ShellTankPresentationStep::CameraShakePassCompleted,
                 ShellTankPresentationStep::AudioRequested,
                 ShellTankPresentationStep::PhysicalEventsAppended,
                 ShellTankPresentationStep::ShellImpactCommitted}) &&
                shellTankImpactWasDeferred(enemyTankPresentation) &&
                eventTypesAre(
                    enemyPresentationGame.eventsThisUpdate(),
                    {GameEventType::BonusSpawned,
                     GameEventType::TankDestroyed}) &&
                enemyTankPresentation[0].eventCount == 1U &&
                enemyTankPresentation[0].effectCount >
                    enemyDeathEffectCountBefore &&
                std::fabs(enemyTankPresentation[1].cameraShake[0] -
                          0.50f) < 0.000001f &&
                enemyTankPresentation[1].cameraShake[0] ==
                    enemyDeathCameraBefore &&
                enemyTankPresentation[1].cameraShake[1] == 0.60f &&
                enemyTankPresentation[2].audioCue ==
                    AudioCue::EnemyDestroyed &&
                secondBonusReleaseOrder &&
                carrierAudioRequests == std::vector<AudioCue>({
                    AudioCue::BonusAppeared,
                    AudioCue::EnemyDestroyed}) &&
                enemyTankPresentation[3].eventCount == 2U &&
                enemyDeathExplosion != nullptr &&
                enemyDeathExplosion->position.x ==
                    enemyPresentationPosition.x &&
                enemyDeathExplosion->position.y == 0.42f &&
                enemyDeathExplosion->position.z ==
                    enemyPresentationPosition.z &&
                enemyDeathExplosion->color.r == 255 &&
                enemyDeathExplosion->color.g == 105 &&
                enemyDeathExplosion->color.b == 27 &&
                enemyDeathExplosion->color.a == 255 &&
                enemyDeathRadial != nullptr &&
                enemyDeathRadial->origin.x == enemyPresentationPosition.x &&
                enemyDeathRadial->origin.z == enemyPresentationPosition.z &&
                enemyDeathRadial->maximum == 0.24f &&
                enemyDeathRadial->distanceFalloff == 0.012f &&
                enemyDeathAudio != nullptr &&
                enemyDeathAudio->cue == AudioCue::EnemyDestroyed &&
                enemyDeathEvents != nullptr &&
                enemyDeathEvents->events.size() == 1U &&
                enemyDeathEvents->events[0] ==
                    enemyPresentationGame.eventsThisUpdate()[1] &&
                enemyDeathImpact != nullptr &&
                enemyDeathImpact->position.x == 12.25f &&
                enemyDeathImpact->position.z == 12.0f &&
                std::all_of(
                    enemyTankPresentation.begin(),
                    enemyTankPresentation.end(),
                    [](const ShellTankPresentationSnapshot &snapshot) {
                        return !snapshot.preCommitFx &&
                               snapshot.physicalImpact.outcome.target ==
                                   CombatTarget::EnemyTank &&
                               snapshot.physicalImpact.enemyCommit.applied &&
                               snapshot.physicalImpact.enemyCommit
                                   .destroyedNow &&
                               snapshot.enemies.size() == 1U &&
                               snapshot.enemies[0].armor == 0 &&
                               snapshot.enemies[0].destroyed;
                    }) &&
                enemyTankPresentation[4].shell.impacting &&
                lengthSquared(enemyTankPresentation[4].shell.velocity) <
                    0.000001f &&
                std::fabs(enemyTankPresentation[4].shell.life -
                          kShellImpactDuration) < 0.000001f,
            "fatal enemy presentation lost explosion-camera-audio-request-"
            "event-impact order"))
        return 1;
    enemyTankPresentation.clear();
    bonusReleasePresentation.clear();
    carrierAudioRequests.clear();
    const std::size_t rejectedEnemyEffectCountBefore =
        enemyPresentationGame.effects().activeCount();
    const float rejectedEnemyCameraBefore =
        Game3DTestAccess::cameraShake(enemyPresentationGame, 0);
    Game3DTestAccess::addEventShell(
        enemyPresentationGame, ShellOwner::Player, 0,
        {12.25f, 12.0f}, {1.0f, 0.0f});
    enemyPresentationGame.update(0.0f, {});
    if (!checkTest(enemyTankPresentation.empty() &&
                       bonusReleasePresentation.empty() &&
                       carrierAudioRequests.empty() &&
                       enemyPresentationGame.eventsThisUpdate().empty() &&
                       enemyPresentationGame.effects().activeCount() ==
                           rejectedEnemyEffectCountBefore &&
                       std::fabs(Game3DTestAccess::cameraShake(
                                     enemyPresentationGame, 0) -
                                 rejectedEnemyCameraBefore) < 0.000001f &&
                       enemyPresentationGame.shells().size() == 3U &&
                       !enemyPresentationGame.shells().back().impacting,
                   "post-destruction shell emitted a second enemy-tank "
                   "presentation chain"))
        return 1;

    Game3D failedBonusReleaseGame(resourceRoot, 0xe0014005U);
    if (!failedBonusReleaseGame.start(1, 3, 1, nations) ||
        !Game3DTestAccess::prepareGameEventScenario(
            failedBonusReleaseGame))
        return 1;
    Game3DTestAccess::recenterPlayer(
        failedBonusReleaseGame, 0, {-5.0f, -5.0f});
    Game3DTestAccess::addEventEnemy(
        failedBonusReleaseGame, 48, 2, 2, {12.0f, 12.0f}, true);
    Game3DTestAccess::addEventShell(
        failedBonusReleaseGame, ShellOwner::Player, 0,
        {12.25f, 12.0f}, {1.0f, 0.0f});
    std::vector<AudioCue> failedReleaseAudioRequests;
    std::vector<BonusReleasePresentationSnapshot>
        failedReleasePresentation;
    Game3DTestAccess::captureAudioRequests(
        failedBonusReleaseGame, failedReleaseAudioRequests);
    Game3DTestAccess::captureBonusReleasePresentation(
        failedBonusReleaseGame, failedReleaseAudioRequests,
        failedReleasePresentation);
    InvalidBonusTypeRandomSource invalidBonusRandom;
    const bool failedReleaseImpactResolved =
        Game3DTestAccess::resolveEventShellImpactWithRandom(
            failedBonusReleaseGame, 0U, invalidBonusRandom);
    if (!checkTest(
            failedReleaseImpactResolved &&
                invalidBonusRandom.integerDrawCount() == 3 &&
                failedReleasePresentation.empty() &&
                failedBonusReleaseGame.bonuses().empty() &&
                eventTypesAre(failedBonusReleaseGame.eventsThisUpdate(),
                              {GameEventType::TankDamaged}) &&
                failedBonusReleaseGame.eventsThisUpdate()[0]
                        .targetEnemyId == 48 &&
                failedBonusReleaseGame.eventsThisUpdate()[0]
                        .sourcePlayerId == 0 &&
                failedBonusReleaseGame.enemies().size() == 1U &&
                failedBonusReleaseGame.enemies()[0].armor == 1 &&
                !failedBonusReleaseGame.enemies()[0].destroyed &&
                failedBonusReleaseGame.players()[0].score == 50 &&
                failedBonusReleaseGame.players()[0]
                        .stageTally.enemyPoints[2] == 50 &&
                failedBonusReleaseGame.players()[0]
                        .stageTally.destroyed[2] == 0 &&
                failedReleaseAudioRequests ==
                    std::vector<AudioCue>{AudioCue::EnemyHit} &&
                failedBonusReleaseGame.shells().size() == 1U &&
                failedBonusReleaseGame.shells()[0].impacting,
            "invalid bonus draw emitted release side effects or aborted the "
            "validated carrier hit"))
        return 1;

    constexpr std::uint32_t kSameFrameBonusSeed = 0xe001400aU;
    Game3D sameFrameBonusOracle(resourceRoot, kSameFrameBonusSeed);
    Game3D sameFrameBonusGame(resourceRoot, kSameFrameBonusSeed);
    if (!sameFrameBonusOracle.start(1, 3, 1, nations) ||
        !sameFrameBonusGame.start(1, 3, 1, nations) ||
        !Game3DTestAccess::prepareGameEventScenario(
            sameFrameBonusOracle) ||
        !Game3DTestAccess::prepareGameEventScenario(sameFrameBonusGame))
    {
        return 1;
    }
    Pickup expectedSameFrameBonus;
    if (!Game3DTestAccess::sampleRandomBonus(
            sameFrameBonusOracle, expectedSameFrameBonus))
    {
        return 1;
    }
    Game3DTestAccess::recenterPlayer(
        sameFrameBonusGame, 0, expectedSameFrameBonus.position);
    Game3DTestAccess::addEventEnemy(
        sameFrameBonusGame, 49, 2, 2, {12.0f, 12.0f}, true);
    Game3DTestAccess::addEventShell(
        sameFrameBonusGame, ShellOwner::Player, 0,
        {12.25f, 12.0f}, {1.0f, 0.0f});
    std::vector<AudioCue> sameFrameBonusAudio;
    std::vector<BonusCollectionPresentationSnapshot>
        sameFrameBonusPresentation;
    Game3DTestAccess::captureAudioRequests(
        sameFrameBonusGame, sameFrameBonusAudio);
    Game3DTestAccess::captureBonusCollectionPresentation(
        sameFrameBonusGame, sameFrameBonusAudio,
        sameFrameBonusPresentation);
    sameFrameBonusGame.update(0.05f, {});
    const std::vector<GameEvent> &sameFrameBonusEvents =
        sameFrameBonusGame.eventsThisUpdate();
    static constexpr std::array<BonusCollectionPresentationStep, 7>
        kSameFrameBonusSteps{{
            BonusCollectionPresentationStep::CollectionEventAppended,
            BonusCollectionPresentationStep::ApplicationCommitted,
            BonusCollectionPresentationStep::CommandsConsumed,
            BonusCollectionPresentationStep::MessageCommitted,
            BonusCollectionPresentationStep::AudioRequested,
            BonusCollectionPresentationStep::EventPointsCommitted,
            BonusCollectionPresentationStep::PickupRemoved}};
    bool sameFrameBonusStepsMatch =
        sameFrameBonusPresentation.size() == kSameFrameBonusSteps.size();
    if (sameFrameBonusStepsMatch)
    {
        for (std::size_t stepIndex = 0;
             stepIndex < kSameFrameBonusSteps.size(); ++stepIndex)
        {
            sameFrameBonusStepsMatch =
                sameFrameBonusStepsMatch &&
                sameFrameBonusPresentation[stepIndex].step ==
                    kSameFrameBonusSteps[stepIndex];
        }
    }
    const bool sameFrameGrenade =
        expectedSameFrameBonus.type == BonusType::Grenade;
    const int sameFrameBonusPoints =
        tanks3d::game::kBonusBasePoints +
        (sameFrameGrenade ? tanks3d::game::kGrenadeEnemyPoints : 0);
    const std::size_t sameFrameEventCount =
        sameFrameGrenade ? 4U : 3U;
    const bool sameFrameBonusPrefix =
        sameFrameBonusEvents.size() == sameFrameEventCount &&
        sameFrameBonusEvents[0].type == GameEventType::BonusSpawned &&
        sameFrameBonusEvents[1].type == GameEventType::TankDamaged &&
        sameFrameBonusEvents[2].type == GameEventType::BonusCollected &&
        sameFrameBonusEvents[2].sourcePlayerId == 0 &&
        sameFrameBonusEvents[2].points == sameFrameBonusPoints &&
        (!sameFrameGrenade ||
         sameFrameBonusEvents[3].type == GameEventType::TankDestroyed);
    if (!checkTest(
            sameFrameBonusPrefix &&
                sameFrameBonusEvents[0].bonusType ==
                    expectedSameFrameBonus.type &&
                sameFrameBonusEvents[2].bonusType ==
                    expectedSameFrameBonus.type &&
                sameFrameBonusEvents[0].position.x ==
                    expectedSameFrameBonus.position.x &&
                sameFrameBonusEvents[0].position.z ==
                    expectedSameFrameBonus.position.z &&
                sameFrameBonusEvents[2].position.x ==
                    expectedSameFrameBonus.position.x &&
                sameFrameBonusEvents[2].position.z ==
                    expectedSameFrameBonus.position.z &&
                sameFrameBonusStepsMatch &&
                sameFrameBonusPresentation.front().intent.pickup.type ==
                    expectedSameFrameBonus.type &&
                std::fabs(sameFrameBonusPresentation.front()
                              .intent.pickup.age -
                          0.05f) < 0.0001f &&
                std::fabs(sameFrameBonusPresentation.front()
                              .intent.pickup.life -
                          (kPickupLifetime - 0.05f)) < 0.0001f &&
                sameFrameBonusPresentation.front().bonuses.size() == 1U &&
                sameFrameBonusPresentation.front().events.size() == 3U &&
                sameFrameBonusPresentation.front().events[2].points == 0 &&
                std::fabs(sameFrameBonusPresentation.front()
                              .bonuses[0].age -
                          0.05f) < 0.0001f &&
                sameFrameBonusPresentation[5].events.size() >= 3U &&
                sameFrameBonusPresentation.back().events.size() >= 3U &&
                sameFrameBonusPresentation[5].events[2].points ==
                    sameFrameBonusPoints &&
                sameFrameBonusPresentation.back().events[2].points ==
                    sameFrameBonusPoints &&
                sameFrameBonusPresentation.back().bonuses.empty() &&
                sameFrameBonusGame.bonuses().empty(),
            "carrier release was not aged and collected by the same frame's "
            "bonus-vector transaction"))
        return 1;

    Game3D directHitGame(resourceRoot, 0xe0010005U);
    if (!directHitGame.start(1, 3, 1, nations) ||
        !Game3DTestAccess::prepareGameEventScenario(directHitGame))
        return 1;
    const bool defaultTankObserversEmptyBefore =
        !Game3DTestAccess::hasTankPresentationObservers(directHitGame);
    Game3DTestAccess::recenterPlayer(directHitGame, 0, {-5.0f, -5.0f});
    Game3DTestAccess::addEventEnemy(directHitGame, 42, 3, 2,
                                    {12.0f, 12.0f}, true);
    const XZ directHitShellPosition{12.5f, 11.5f};
    Game3DTestAccess::addEventShell(directHitGame, ShellOwner::Player, 0,
                                    directHitShellPosition, {1.0f, 0.0f});
    directHitGame.update(0.0f, {});
    const std::vector<GameEvent> firstHitEvents =
        directHitGame.eventsThisUpdate();
    if (!checkTest(eventTypesAre(
                       firstHitEvents,
                       {GameEventType::BonusSpawned,
                        GameEventType::TankDamaged}) &&
                       firstHitEvents[0].sourceEnemyId == 42 &&
                       directHitGame.bonuses().size() == 1U &&
                       firstHitEvents[0].bonusType ==
                           directHitGame.bonuses()[0].type &&
                       firstHitEvents[0].position.x ==
                           directHitGame.bonuses()[0].position.x &&
                       firstHitEvents[0].position.z ==
                           directHitGame.bonuses()[0].position.z &&
                       firstHitEvents[1].cause ==
                           GameEventCause::PlayerShell &&
                       firstHitEvents[1].sourcePlayerId == 0 &&
                       firstHitEvents[1].targetEnemyId == 42 &&
                       firstHitEvents[1].enemyType == 3 &&
                       firstHitEvents[1].position.x == 12.0f &&
                       firstHitEvents[1].position.z == 12.0f &&
                       firstHitEvents[1].valueBefore == 2 &&
                       firstHitEvents[1].valueAfter == 1 &&
                       firstHitEvents[1].points == 50 &&
                       directHitGame.enemies()[0].armor == 1 &&
                       !directHitGame.enemies()[0].destroyed &&
                       directHitGame.shells().size() == 1U &&
                       directHitGame.shells()[0].impacting &&
                       directHitGame.shells()[0].position.x ==
                           directHitShellPosition.x &&
                       directHitGame.shells()[0].position.z ==
                           directHitShellPosition.z &&
                       defaultTankObserversEmptyBefore &&
                       !Game3DTestAccess::hasTankPresentationObservers(
                           directHitGame) &&
                       directHitGame.players()[0].score == 50 &&
                       directHitGame.players()[0].stageTally.totalDestroyed() == 0,
                   "carrier release, direct damage, or scoring order changed"))
        return 1;
    Game3DTestAccess::addEventShell(directHitGame, ShellOwner::Player, 0,
                                    directHitShellPosition, {1.0f, 0.0f});
    directHitGame.update(0.0f, {});
    const std::vector<GameEvent> secondHitEvents =
        directHitGame.eventsThisUpdate();
    if (!checkTest(eventTypesAre(
                       secondHitEvents,
                       {GameEventType::BonusSpawned,
                        GameEventType::TankDestroyed}) &&
                       secondHitEvents[1].cause ==
                           GameEventCause::PlayerShell &&
                       secondHitEvents[1].sourcePlayerId == 0 &&
                       secondHitEvents[0].sourceEnemyId == 42 &&
                       secondHitEvents[1].targetEnemyId == 42 &&
                       secondHitEvents[1].enemyType == 3 &&
                       secondHitEvents[1].position.x == 12.0f &&
                       secondHitEvents[1].position.z == 12.0f &&
                       secondHitEvents[1].valueBefore == 1 &&
                       secondHitEvents[1].valueAfter == 0 &&
                       secondHitEvents[1].points == 50 &&
                       directHitGame.bonuses().size() == 2U &&
                       secondHitEvents[0].bonusType ==
                           directHitGame.bonuses().back().type &&
                       secondHitEvents[0].position.x ==
                           directHitGame.bonuses().back().position.x &&
                       secondHitEvents[0].position.z ==
                           directHitGame.bonuses().back().position.z &&
                       directHitGame.enemies()[0].destroyed &&
                       !directHitGame.enemies()[0].moving &&
                       directHitGame.enemies()[0].armor == 0 &&
                       directHitGame.enemies()[0].deathTimer ==
                           kTankDeathDuration &&
                       directHitGame.players()[0].score == 100 &&
                       directHitGame.players()[0]
                               .stageTally.enemyPoints[3] == 100 &&
                       directHitGame.players()[0].directKillStreak == 1 &&
                       directHitGame.players()[0].streakPopupTimer ==
                           kStreakPopupDuration &&
                       directHitGame.players()[0].stageTally.destroyed[3] == 1,
                   "direct destroying hit was not committed before its event"))
        return 1;

    Game3D tankPriorityGame(resourceRoot, 0xe0011005U);
    if (!tankPriorityGame.start(1, 3, 1, nations) ||
        !Game3DTestAccess::prepareGameEventScenario(tankPriorityGame))
        return 1;
    Game3DTestAccess::recenterPlayer(tankPriorityGame, 0, {-5.0f, -5.0f});
    Game3DTestAccess::addEventEnemy(tankPriorityGame, 43, 1, 2,
                                    {12.0f, 12.0f});
    Game3DTestAccess::addEventShell(
        tankPriorityGame, ShellOwner::Player, 0,
        {12.25f, 12.0f}, {1.0f, 0.0f});
    Game3DTestAccess::addEventShell(
        tankPriorityGame, ShellOwner::Enemy, 83,
        {12.25f, 12.0f}, {-1.0f, 0.0f});
    tankPriorityGame.update(0.0f, {});
    if (!checkTest(
            eventTypesAre(tankPriorityGame.eventsThisUpdate(),
                          {GameEventType::TankDamaged}) &&
                eventCount(tankPriorityGame.eventsThisUpdate(),
                           GameEventType::ShellCancelled) == 0 &&
                tankPriorityGame.eventsThisUpdate()[0].targetEnemyId == 43 &&
                tankPriorityGame.enemies()[0].armor == 1 &&
                tankPriorityGame.shells().size() == 2U &&
                tankPriorityGame.shells()[0].impacting &&
                !tankPriorityGame.shells()[1].impacting,
            "enemy-tank hit stopped taking priority over shell cancellation"))
        return 1;

    Game3D invalidHitOwnerGame(resourceRoot, 0xe0012005U);
    if (!invalidHitOwnerGame.start(1, 3, 1, nations) ||
        !Game3DTestAccess::prepareGameEventScenario(invalidHitOwnerGame))
        return 1;
    Game3DTestAccess::recenterPlayer(invalidHitOwnerGame, 0,
                                    {-5.0f, -5.0f});
    Game3DTestAccess::addEventEnemy(invalidHitOwnerGame, 44, 2, 1,
                                    {12.0f, 12.0f});
    Game3DTestAccess::addEventShell(
        invalidHitOwnerGame, ShellOwner::Player, 99,
        {12.25f, 12.0f}, {1.0f, 0.0f});
    invalidHitOwnerGame.update(0.0f, {});
    if (!checkTest(
            eventTypesAre(invalidHitOwnerGame.eventsThisUpdate(),
                          {GameEventType::TankDestroyed}) &&
                invalidHitOwnerGame.eventsThisUpdate()[0].sourcePlayerId ==
                    99 &&
                invalidHitOwnerGame.eventsThisUpdate()[0].targetEnemyId ==
                    44 &&
                invalidHitOwnerGame.eventsThisUpdate()[0].points == 50 &&
                invalidHitOwnerGame.enemies()[0].destroyed &&
                invalidHitOwnerGame.enemies()[0].armor == 0 &&
                invalidHitOwnerGame.players()[0].score == 0 &&
                invalidHitOwnerGame.players()[0]
                        .stageTally.totalEnemyPoints() == 0 &&
                invalidHitOwnerGame.players()[0].directKillStreak == 0 &&
                invalidHitOwnerGame.shells().size() == 1U &&
                invalidHitOwnerGame.shells()[0].impacting,
            "invalid shell owner stopped physical damage or credited a player"))
        return 1;

    Game3D posthumousHitGame(resourceRoot, 0xe0013005U);
    if (!posthumousHitGame.start(1, 3, 1, nations) ||
        !Game3DTestAccess::prepareGameEventScenario(posthumousHitGame))
        return 1;
    Game3DTestAccess::setPlayerWaitingForDeath(posthumousHitGame, 0);
    Game3DTestAccess::addEventEnemy(posthumousHitGame, 45, 1, 1,
                                    {12.0f, 12.0f});
    Game3DTestAccess::addEventShell(
        posthumousHitGame, ShellOwner::Player, 0,
        {12.25f, 12.0f}, {1.0f, 0.0f});
    posthumousHitGame.update(0.0f, {});
    if (!checkTest(
            eventTypesAre(posthumousHitGame.eventsThisUpdate(),
                          {GameEventType::TankDestroyed}) &&
                posthumousHitGame.enemies()[0].destroyed &&
                posthumousHitGame.players()[0].score == 50 &&
                posthumousHitGame.players()[0]
                        .stageTally.enemyPoints[1] == 50 &&
                posthumousHitGame.players()[0]
                        .stageTally.destroyed[1] == 1 &&
                posthumousHitGame.players()[0].directKillStreak == 0 &&
                posthumousHitGame.players()[0].streakPopupTimer == 0.0f,
            "posthumous player shell lost score/tally or started a streak"))
        return 1;

    Game3D duplicateHitGame(resourceRoot, 0xe0014005U);
    if (!duplicateHitGame.start(1, 3, 1, nations) ||
        !Game3DTestAccess::prepareGameEventScenario(duplicateHitGame))
        return 1;
    Game3DTestAccess::recenterPlayer(duplicateHitGame, 0,
                                    {-5.0f, -5.0f});
    Game3DTestAccess::addEventEnemy(duplicateHitGame, 46, 0, 1,
                                    {12.0f, 12.0f}, true);
    Game3DTestAccess::addEventShell(
        duplicateHitGame, ShellOwner::Player, 0,
        {12.25f, 12.0f}, {1.0f, 0.0f});
    Game3DTestAccess::addEventShell(
        duplicateHitGame, ShellOwner::Player, 0,
        {12.25f, 12.0f}, {1.0f, 0.0f});
    duplicateHitGame.update(0.0f, {});
    if (!checkTest(
            eventTypesAre(duplicateHitGame.eventsThisUpdate(),
                          {GameEventType::BonusSpawned,
                           GameEventType::TankDestroyed}) &&
                duplicateHitGame.bonuses().size() == 1U &&
                duplicateHitGame.players()[0].score == 50 &&
                duplicateHitGame.players()[0]
                        .stageTally.enemyPoints[0] == 50 &&
                duplicateHitGame.players()[0]
                        .stageTally.destroyed[0] == 1 &&
                duplicateHitGame.players()[0].directKillStreak == 1 &&
                duplicateHitGame.shells().size() == 2U &&
                duplicateHitGame.shells()[0].impacting &&
                !duplicateHitGame.shells()[1].impacting,
            "duplicate same-frame outcome repeated bonus, damage, or score"))
        return 1;

    Game3D bonusCollectionOrderGame(resourceRoot, 0xe0014006U);
    if (!bonusCollectionOrderGame.start(1, 3, 1, nations) ||
        !Game3DTestAccess::prepareGameEventScenario(
            bonusCollectionOrderGame))
        return 1;
    const bool collectionObserverWasDefaultOff =
        !Game3DTestAccess::hasBonusCollectionPresentationObserver(
            bonusCollectionOrderGame);
    std::vector<AudioCue> collectionAudioRequests;
    std::vector<BonusCollectionPresentationSnapshot>
        collectionSnapshots;
    Game3DTestAccess::captureAudioRequests(
        bonusCollectionOrderGame, collectionAudioRequests);
    Game3DTestAccess::captureBonusCollectionPresentation(
        bonusCollectionOrderGame, collectionAudioRequests,
        collectionSnapshots);
    Game3DTestAccess::addEventEnemy(
        bonusCollectionOrderGame, 50, 2, 3, {11.0f, 10.0f});
    Game3DTestAccess::addEventPickup(
        bonusCollectionOrderGame, BonusType::Grenade,
        bonusCollectionOrderGame.players()[0].position);
    bonusCollectionOrderGame.update(0.05f, {});

    bool collectionOrderValid =
        collectionObserverWasDefaultOff && collectionSnapshots.size() == 7U;
    if (collectionOrderValid)
    {
        static constexpr std::array<BonusCollectionPresentationStep, 7>
            expectedSteps{{
                BonusCollectionPresentationStep::CollectionEventAppended,
                BonusCollectionPresentationStep::ApplicationCommitted,
                BonusCollectionPresentationStep::CommandsConsumed,
                BonusCollectionPresentationStep::MessageCommitted,
                BonusCollectionPresentationStep::AudioRequested,
                BonusCollectionPresentationStep::EventPointsCommitted,
                BonusCollectionPresentationStep::PickupRemoved}};
        for (std::size_t index = 0; index < expectedSteps.size(); ++index)
            collectionOrderValid = collectionOrderValid &&
                collectionSnapshots[index].step == expectedSteps[index];

        const BonusCollectionPresentationSnapshot &eventAppended =
            collectionSnapshots[0];
        const BonusCollectionPresentationSnapshot &applicationCommitted =
            collectionSnapshots[1];
        const BonusCollectionPresentationSnapshot &commandsConsumed =
            collectionSnapshots[2];
        const BonusCollectionPresentationSnapshot &messageCommitted =
            collectionSnapshots[3];
        const BonusCollectionPresentationSnapshot &audioRequested =
            collectionSnapshots[4];
        const BonusCollectionPresentationSnapshot &pointsCommitted =
            collectionSnapshots[5];
        const BonusCollectionPresentationSnapshot &pickupRemoved =
            collectionSnapshots[6];

        const auto hasPendingCollectionEvent = [](const auto &snapshot) {
            return !snapshot.events.empty() &&
                snapshot.events[0].type == GameEventType::BonusCollected &&
                snapshot.events[0].bonusType == BonusType::Grenade &&
                snapshot.events[0].sourcePlayerId == 0 &&
                snapshot.events[0].points == 0;
        };
        collectionOrderValid = collectionOrderValid &&
            !eventAppended.hasApplication &&
            eventAppended.intent.collectorIndex == 0 &&
            eventAppended.intent.playerId == 0 &&
            eventAppended.intent.pickup.type == BonusType::Grenade &&
            std::fabs(eventAppended.intent.pickup.age - 0.05f) < 0.0001f &&
            std::fabs(eventAppended.intent.pickup.life - 12.45f) < 0.0001f &&
            eventAppended.bonuses.size() == 1U &&
            hasPendingCollectionEvent(eventAppended) &&
            eventAppended.players.size() == 1U &&
            eventAppended.players[0].score == 0 &&
            eventAppended.enemies.size() == 1U &&
            eventAppended.enemies[0].armor == 3 &&
            !eventAppended.enemies[0].destroyed &&
            eventAppended.audioRequests.empty() &&
            eventAppended.effectCount == 0U && eventAppended.message.empty();

        collectionOrderValid = collectionOrderValid &&
            applicationCommitted.hasApplication &&
            applicationCommitted.application.applied &&
            applicationCommitted.application.type == BonusType::Grenade &&
            applicationCommitted.application.playerIndex == 0 &&
            applicationCommitted.application.playerId == 0 &&
            applicationCommitted.application.scoreDelta == 500 &&
            applicationCommitted.application.commands.size() == 6U &&
            hasPendingCollectionEvent(applicationCommitted) &&
            applicationCommitted.events.size() == 1U &&
            applicationCommitted.players[0].score == 500 &&
            applicationCommitted.players[0].stageTally.bonusPoints == 500 &&
            applicationCommitted.enemies[0].armor == 0 &&
            applicationCommitted.enemies[0].destroyed &&
            applicationCommitted.audioRequests.empty() &&
            applicationCommitted.effectCount == 0U &&
            applicationCommitted.cameraShake[0] == 0.0f &&
            applicationCommitted.message.empty();

        collectionOrderValid = collectionOrderValid &&
            hasPendingCollectionEvent(commandsConsumed) &&
            eventTypesAre(commandsConsumed.events,
                          {GameEventType::BonusCollected,
                           GameEventType::TankDestroyed}) &&
            commandsConsumed.events[1].targetEnemyId == 50 &&
            commandsConsumed.events[1].points == 200 &&
            commandsConsumed.audioRequests ==
                std::vector<AudioCue>({AudioCue::EnemyHit,
                                       AudioCue::EnemyHit,
                                       AudioCue::EnemyDestroyed}) &&
            commandsConsumed.effectCount > 0U &&
            std::fabs(commandsConsumed.cameraShake[0] - 0.34f) <
                0.0001f &&
            commandsConsumed.message.empty();

        collectionOrderValid = collectionOrderValid &&
            hasPendingCollectionEvent(messageCommitted) &&
            messageCommitted.message == "P1  GRENADE  +500" &&
            std::fabs(messageCommitted.messageTimer - 2.2f) < 0.0001f &&
            messageCommitted.audioRequests.size() == 3U &&
            hasPendingCollectionEvent(audioRequested) &&
            audioRequested.audioRequests ==
                std::vector<AudioCue>({AudioCue::EnemyHit,
                                       AudioCue::EnemyHit,
                                       AudioCue::EnemyDestroyed,
                                       AudioCue::BonusObtained}) &&
            pointsCommitted.events.size() == 2U &&
            pointsCommitted.events[0].points == 500 &&
            pointsCommitted.bonuses.size() == 1U &&
            pickupRemoved.events.size() == 2U &&
            pickupRemoved.events[0].points == 500 &&
            pickupRemoved.bonuses.empty();
    }
    if (!checkTest(
            collectionOrderValid,
            "bonus collection order or pre/post phase snapshots drifted"))
        return 1;

    Game3D tankCueGame(resourceRoot, 0xe0014007U);
    if (!tankCueGame.start(1, 3, 1, nations) ||
        !Game3DTestAccess::prepareGameEventScenario(tankCueGame))
        return 1;
    std::vector<AudioCue> tankCueRequests;
    Game3DTestAccess::captureAudioRequests(tankCueGame, tankCueRequests);
    Game3DTestAccess::addEventPickup(
        tankCueGame, BonusType::Tank,
        tankCueGame.players()[0].position);
    tankCueGame.update(0.0f, {});
    if (!checkTest(
            tankCueRequests ==
                    std::vector<AudioCue>({AudioCue::PlayerLifeUp}) &&
                tankCueGame.players()[0].lives == 4 &&
                eventTypesAre(tankCueGame.eventsThisUpdate(),
                              {GameEventType::BonusCollected}) &&
                tankCueGame.eventsThisUpdate()[0].points == 300 &&
                tankCueGame.bonuses().empty(),
            "Tank pickup did not request PlayerLifeUp at collection commit"))
        return 1;

    Game3D stackedBandageGame(resourceRoot, 0xe0014008U);
    if (!stackedBandageGame.start(2, 3, 1, nations) ||
        !Game3DTestAccess::prepareGameEventScenario(stackedBandageGame))
        return 1;
    Game3DTestAccess::setPlayerHitPoints(stackedBandageGame, 0, 2);
    Game3DTestAccess::setPlayerHitPoints(stackedBandageGame, 1, 2);
    Game3DTestAccess::recenterPlayer(
        stackedBandageGame, 1,
        stackedBandageGame.players()[0].position);
    std::vector<AudioCue> stackedBandageAudio;
    Game3DTestAccess::captureAudioRequests(
        stackedBandageGame, stackedBandageAudio);
    for (int pickupIndex = 0; pickupIndex < 3; ++pickupIndex)
    {
        Game3DTestAccess::addEventPickup(
            stackedBandageGame, BonusType::Bandage,
            stackedBandageGame.players()[0].position);
    }
    stackedBandageGame.update(0.05f, {});
    if (!checkTest(
            eventTypesAre(stackedBandageGame.eventsThisUpdate(),
                          {GameEventType::BonusCollected,
                           GameEventType::BonusCollected}) &&
                stackedBandageGame.eventsThisUpdate()[0].sourcePlayerId == 0 &&
                stackedBandageGame.eventsThisUpdate()[1].sourcePlayerId == 1 &&
                stackedBandageGame.eventsThisUpdate()[0].points == 300 &&
                stackedBandageGame.eventsThisUpdate()[1].points == 300 &&
                stackedBandageGame.players()[0].hitPoints == 3 &&
                stackedBandageGame.players()[1].hitPoints == 3 &&
                stackedBandageGame.players()[0].score == 300 &&
                stackedBandageGame.players()[1].score == 300 &&
                stackedBandageGame.bonuses().size() == 1U &&
                stackedBandageGame.bonuses()[0].type == BonusType::Bandage &&
                stackedBandageGame.bonuses()[0].position.x ==
                    stackedBandageGame.players()[0].position.x &&
                stackedBandageGame.bonuses()[0].position.z ==
                    stackedBandageGame.players()[0].position.z &&
                std::fabs(stackedBandageGame.bonuses()[0].age - 0.05f) <
                    0.0001f &&
                std::fabs(stackedBandageGame.bonuses()[0].life - 12.45f) <
                    0.0001f &&
                stackedBandageAudio ==
                    std::vector<AudioCue>({AudioCue::BonusObtained,
                                           AudioCue::BonusObtained}) &&
                stackedBandageGame.bonusMessage() ==
                    "P2  BANDAGE  HP 3/3  +300",
            "stacked Bandages did not re-evaluate players sequentially"))
        return 1;

    Game3D stackedGrenadeGame(resourceRoot, 0xe0014009U);
    if (!stackedGrenadeGame.start(1, 3, 1, nations) ||
        !Game3DTestAccess::prepareGameEventScenario(stackedGrenadeGame))
        return 1;
    Game3DTestAccess::addEventEnemy(
        stackedGrenadeGame, 54, 2, 3, {11.0f, 10.0f});
    Game3DTestAccess::addEventPickup(
        stackedGrenadeGame, BonusType::Grenade,
        stackedGrenadeGame.players()[0].position);
    Game3DTestAccess::addEventPickup(
        stackedGrenadeGame, BonusType::Grenade,
        stackedGrenadeGame.players()[0].position);
    std::vector<AudioCue> stackedGrenadeAudio;
    Game3DTestAccess::captureAudioRequests(
        stackedGrenadeGame, stackedGrenadeAudio);
    stackedGrenadeGame.update(0.05f, {});
    if (!checkTest(
            eventTypesAre(stackedGrenadeGame.eventsThisUpdate(),
                          {GameEventType::BonusCollected,
                           GameEventType::TankDestroyed,
                           GameEventType::BonusCollected}) &&
                stackedGrenadeGame.eventsThisUpdate()[0].points == 500 &&
                stackedGrenadeGame.eventsThisUpdate()[1].targetEnemyId == 54 &&
                stackedGrenadeGame.eventsThisUpdate()[1].points == 200 &&
                stackedGrenadeGame.eventsThisUpdate()[2].points == 300 &&
                stackedGrenadeGame.players()[0].score == 800 &&
                stackedGrenadeGame.players()[0].stageTally.bonusPoints == 800 &&
                stackedGrenadeGame.enemies()[0].destroyed &&
                stackedGrenadeGame.bonuses().empty() &&
                stackedGrenadeAudio ==
                    std::vector<AudioCue>({AudioCue::EnemyHit,
                                           AudioCue::EnemyHit,
                                           AudioCue::EnemyDestroyed,
                                           AudioCue::BonusObtained,
                                           AudioCue::BonusObtained}) &&
                stackedGrenadeGame.bonusMessage() == "P1  GRENADE  +300",
            "stacked Grenades lost nonzero event index or sequential state"))
        return 1;

    Game3D grenadeGame(resourceRoot, 0xe0010006U);
    if (!grenadeGame.start(1, 3, 1, nations) ||
        !Game3DTestAccess::prepareGameEventScenario(grenadeGame))
        return 1;
    Game3DTestAccess::addEventEnemy(grenadeGame, 51, 0, 1,
                                    {11.0f, 10.0f});
    Game3DTestAccess::addEventEnemy(grenadeGame, 52, 2, 3,
                                    {15.0f, 10.0f});
    Game3DTestAccess::addEventEnemy(grenadeGame, 53, 1, 0,
                                    {13.0f, 8.0f}, false, true);
    Game3DTestAccess::addEventPickup(
        grenadeGame, bonus_assets::Type::Grenade,
        grenadeGame.players()[0].position);
    grenadeGame.update(0.0f, {});
    const std::vector<GameEvent> grenadeEvents =
        grenadeGame.eventsThisUpdate();
    if (!checkTest(eventTypesAre(
                       grenadeEvents,
                       {GameEventType::BonusCollected,
                        GameEventType::TankDestroyed,
                        GameEventType::TankDestroyed}) &&
                       grenadeEvents[0].bonusType ==
                           bonus_assets::Type::Grenade &&
                       grenadeEvents[0].sourcePlayerId == 0 &&
                       grenadeEvents[0].position.x ==
                           grenadeGame.players()[0].position.x &&
                       grenadeEvents[0].position.z ==
                           grenadeGame.players()[0].position.z &&
                       grenadeEvents[0].points == 700 &&
                       grenadeEvents[1].cause ==
                           GameEventCause::GrenadeBonus &&
                       grenadeEvents[2].cause ==
                           GameEventCause::GrenadeBonus &&
                       grenadeEvents[1].targetEnemyId == 51 &&
                       grenadeEvents[1].sourcePlayerId == 0 &&
                       grenadeEvents[1].enemyType == 0 &&
                       grenadeEvents[1].valueBefore == 1 &&
                       grenadeEvents[1].valueAfter == 0 &&
                       grenadeEvents[1].points == 200 &&
                       grenadeEvents[2].targetEnemyId == 52 &&
                       grenadeEvents[2].sourcePlayerId == 0 &&
                       grenadeEvents[2].enemyType == 2 &&
                       grenadeEvents[2].valueBefore == 3 &&
                       grenadeEvents[2].valueAfter == 0 &&
                       grenadeEvents[2].points == 200 &&
                       grenadeGame.enemies()[0].destroyed &&
                       grenadeGame.enemies()[0].armor == 0 &&
                       grenadeGame.enemies()[0].deathTimer ==
                           kTankDeathDuration &&
                       grenadeGame.enemies()[1].destroyed &&
                       grenadeGame.enemies()[1].armor == 0 &&
                       grenadeGame.enemies()[1].deathTimer ==
                           kTankDeathDuration &&
                       grenadeGame.players()[0].score == 700 &&
                       grenadeGame.players()[0].stageTally.bonusPoints == 700 &&
                       grenadeGame.players()[0].stageTally.totalDestroyed() == 0 &&
                       grenadeGame.players()[0].directKillStreak == 0 &&
                       eventCount(grenadeEvents,
                                  GameEventType::TankDestroyed) == 2,
                   "grenade attribution entered classified direct-fire kills"))
        return 1;
    grenadeGame.update(0.0f, {});
    if (!checkTest(grenadeGame.eventsThisUpdate().empty(),
                   "grenade destruction or collection event repeated"))
        return 1;
    Game3DTestAccess::setCameraShake(grenadeGame, 0, 0.0f);
    Game3DTestAccess::addEventPickup(
        grenadeGame, bonus_assets::Type::Grenade,
        grenadeGame.players()[0].position);
    grenadeGame.update(0.0f, {});
    if (!checkTest(
            eventTypesAre(grenadeGame.eventsThisUpdate(),
                          {GameEventType::BonusCollected}) &&
                grenadeGame.eventsThisUpdate()[0].points == 300 &&
                grenadeGame.players()[0].score == 1000 &&
                grenadeGame.players()[0].stageTally.bonusPoints == 1000 &&
                std::fabs(Game3DTestAccess::cameraShake(grenadeGame, 0)) <
                    0.0001f &&
                grenadeGame.bonusMessage() == "P1  GRENADE  +300" &&
                std::fabs(grenadeGame.bonusMessageTimer() - 2.2f) <
                    0.0001f,
            "zero-target grenade did not remain a 300-point pickup"))
        return 1;

    Game3D starGame(resourceRoot, 0xe0010007U);
    if (!starGame.start(1, 3, 1, nations) ||
        !Game3DTestAccess::prepareGameEventScenario(starGame))
        return 1;
    Game3DTestAccess::addEventPickup(starGame, bonus_assets::Type::Star,
                                     starGame.players()[0].position);
    starGame.update(0.0f, {});
    if (!checkTest(eventTypesAre(starGame.eventsThisUpdate(),
                                 {GameEventType::BonusCollected}) &&
                       starGame.eventsThisUpdate()[0].sourcePlayerId == 0 &&
                       starGame.eventsThisUpdate()[0].bonusType ==
                           bonus_assets::Type::Star &&
                       starGame.eventsThisUpdate()[0].points == 300 &&
                       starGame.players()[0].level == 1 &&
                       starGame.bonuses().empty(),
                   "bonus collection event preceded eligibility or missed effect"))
        return 1;
    starGame.update(0.0f, {});
    if (!checkTest(starGame.eventsThisUpdate().empty(),
                   "removed bonus emitted a second collection event"))
        return 1;

    Game3D gunGame(resourceRoot, 0xe0011007U);
    if (!gunGame.start(1, 3, 1, nations) ||
        !Game3DTestAccess::prepareGameEventScenario(gunGame))
        return 1;
    Game3DTestAccess::setPlayerLevel(gunGame, 1);
    Game3DTestAccess::addEventPickup(
        gunGame, BonusType::Gun, gunGame.players()[0].position);
    gunGame.update(0.0f, {});
    if (!checkTest(eventTypesAre(gunGame.eventsThisUpdate(),
                                 {GameEventType::BonusCollected}) &&
                       gunGame.eventsThisUpdate()[0].bonusType ==
                           BonusType::Gun &&
                       gunGame.eventsThisUpdate()[0].points == 300 &&
                       gunGame.players()[0].level == 3 &&
                       gunGame.players()[0].score == 300 &&
                       gunGame.players()[0].stageTally.bonusPoints == 300 &&
                       gunGame.bonuses().empty(),
                   "Gun bonus did not apply the maximum-level upgrade"))
        return 1;

    Game3D bandageGateGame(resourceRoot, 0xe0011007U);
    if (!bandageGateGame.start(1, 3, 1, nations) ||
        !Game3DTestAccess::prepareGameEventScenario(bandageGateGame))
        return 1;
    Game3DTestAccess::addEventPickup(
        bandageGateGame, bonus_assets::Type::Bandage,
        bandageGateGame.players()[0].position);
    bandageGateGame.update(0.0f, {});
    if (!checkTest(bandageGateGame.eventsThisUpdate().empty() &&
                       bandageGateGame.bonuses().size() == 1U &&
                       bandageGateGame.players()[0].hitPoints == 3,
                   "ineligible full-health Bandage emitted a collection event"))
        return 1;

    Game3D invalidBonusGame(resourceRoot, 0xe0012007U);
    if (!invalidBonusGame.start(1, 3, 1, nations) ||
        !Game3DTestAccess::prepareGameEventScenario(invalidBonusGame))
        return 1;
    Game3DTestAccess::addEventPickup(
        invalidBonusGame, BonusType::Count,
        invalidBonusGame.players()[0].position);
    Game3DTestAccess::addEventPickup(
        invalidBonusGame, static_cast<BonusType>(255),
        invalidBonusGame.players()[0].position);
    invalidBonusGame.update(0.0f, {});
    if (!checkTest(
            invalidBonusGame.eventsThisUpdate().empty() &&
                invalidBonusGame.bonuses().empty() &&
                invalidBonusGame.players()[0].score == 0 &&
                invalidBonusGame.players()[0].stageTally.bonusPoints == 0 &&
                invalidBonusGame.bonusMessage().empty() &&
                invalidBonusGame.bonusMessageTimer() == 0.0f,
            "invalid bonus values awarded points or remained in play"))
        return 1;

    Game3D bonusEffectsGame(resourceRoot, 0xe0013007U);
    if (!bonusEffectsGame.start(1, 3, 1, nations) ||
        !Game3DTestAccess::prepareGameEventScenario(bonusEffectsGame))
        return 1;

    Game3DTestAccess::setPlayerProtection(bonusEffectsGame, 0, 2.0f,
                                          false);
    Game3DTestAccess::addEventPickup(
        bonusEffectsGame, BonusType::Helmet,
        bonusEffectsGame.players()[0].position);
    bonusEffectsGame.update(0.0f, {});
    const bool helmetExtended =
        eventTypesAre(bonusEffectsGame.eventsThisUpdate(),
                      {GameEventType::BonusCollected}) &&
        bonusEffectsGame.eventsThisUpdate()[0].bonusType ==
            BonusType::Helmet &&
        bonusEffectsGame.eventsThisUpdate()[0].points == 300 &&
        std::fabs(bonusEffectsGame.players()[0].shieldTimer - 10.0f) <
            0.0001f &&
        bonusEffectsGame.bonusMessage() == "P1  HELMET  +300";
    Game3DTestAccess::setPlayerProtection(bonusEffectsGame, 0, 12.0f,
                                          false);
    Game3DTestAccess::addEventPickup(
        bonusEffectsGame, BonusType::Helmet,
        bonusEffectsGame.players()[0].position);
    bonusEffectsGame.update(0.0f, {});
    if (!checkTest(
            helmetExtended &&
                eventTypesAre(bonusEffectsGame.eventsThisUpdate(),
                              {GameEventType::BonusCollected}) &&
                std::fabs(bonusEffectsGame.players()[0].shieldTimer - 12.0f) <
                    0.0001f &&
                bonusEffectsGame.players()[0].score == 600 &&
                bonusEffectsGame.players()[0].stageTally.bonusPoints == 600,
            "Helmet pickup did not extend or preserve shield duration"))
        return 1;

    Game3DTestAccess::addEventEnemy(
        bonusEffectsGame, 61, 0, 1, {10.0f, 10.0f});
    Game3DTestAccess::addEventEnemy(
        bonusEffectsGame, 62, 1, 1, {14.0f, 10.0f});
    Game3DTestAccess::addEventEnemy(
        bonusEffectsGame, 63, 2, 0, {18.0f, 10.0f}, false, true);
    // Keep the enemy inside its current freeze for this zero-dt setup so it
    // cannot fire before updateBonuses applies the Clock.
    Game3DTestAccess::setEnemyFrozenTimer(bonusEffectsGame, 0, 0.01f);
    Game3DTestAccess::setEnemyFrozenTimer(bonusEffectsGame, 1, 12.0f);
    Game3DTestAccess::setEnemyFrozenTimer(bonusEffectsGame, 2, 3.0f);
    Game3DTestAccess::addEventPickup(
        bonusEffectsGame, BonusType::Clock,
        bonusEffectsGame.players()[0].position);
    bonusEffectsGame.update(0.0f, {});
    if (!checkTest(
            eventTypesAre(bonusEffectsGame.eventsThisUpdate(),
                          {GameEventType::BonusCollected}) &&
                bonusEffectsGame.eventsThisUpdate()[0].bonusType ==
                    BonusType::Clock &&
                std::fabs(bonusEffectsGame.enemies()[0].frozenTimer - 8.0f) <
                    0.0001f &&
                std::fabs(bonusEffectsGame.enemies()[1].frozenTimer - 12.0f) <
                    0.0001f &&
                std::fabs(bonusEffectsGame.enemies()[2].frozenTimer - 3.0f) <
                    0.0001f &&
                bonusEffectsGame.players()[0].score == 900 &&
                bonusEffectsGame.bonusMessage() == "P1  CLOCK  +300",
            "Clock pickup froze the wrong enemies or shortened a freeze"))
        return 1;

    Game3DTestAccess::setPlayerLives(bonusEffectsGame, 0, 98);
    Game3DTestAccess::addEventPickup(
        bonusEffectsGame, BonusType::Tank,
        bonusEffectsGame.players()[0].position);
    bonusEffectsGame.update(0.0f, {});
    const bool tankReachedCap =
        eventTypesAre(bonusEffectsGame.eventsThisUpdate(),
                      {GameEventType::BonusCollected}) &&
        bonusEffectsGame.players()[0].lives == 99 &&
        bonusEffectsGame.players()[0].score == 1200;
    Game3DTestAccess::addEventPickup(
        bonusEffectsGame, BonusType::Tank,
        bonusEffectsGame.players()[0].position);
    bonusEffectsGame.update(0.0f, {});
    if (!checkTest(
            tankReachedCap &&
                eventTypesAre(bonusEffectsGame.eventsThisUpdate(),
                              {GameEventType::BonusCollected}) &&
                bonusEffectsGame.players()[0].lives == 99 &&
                bonusEffectsGame.players()[0].score == 1500 &&
                bonusEffectsGame.bonusMessage() ==
                    "P1  1-UP TANK  +300",
            "Tank pickup did not respect the 99-life cap"))
        return 1;

    Game3DTestAccess::setPlayerProtection(bonusEffectsGame, 0, 0.0f,
                                          false);
    Game3DTestAccess::addEventPickup(
        bonusEffectsGame, BonusType::Boat,
        bonusEffectsGame.players()[0].position);
    bonusEffectsGame.update(0.0f, {});
    if (!checkTest(
            eventTypesAre(bonusEffectsGame.eventsThisUpdate(),
                          {GameEventType::BonusCollected}) &&
                bonusEffectsGame.players()[0].hasBoat &&
                bonusEffectsGame.players()[0].score == 1800 &&
                bonusEffectsGame.bonusMessage() == "P1  BOAT  +300",
            "Boat pickup did not arm the next-hit protection"))
        return 1;

    Game3DTestAccess::setPlayerCombatState(bonusEffectsGame, 2);
    Game3DTestAccess::addEventPickup(
        bonusEffectsGame, BonusType::Bandage,
        bonusEffectsGame.players()[0].position);
    bonusEffectsGame.update(0.0f, {});
    if (!checkTest(
            eventTypesAre(bonusEffectsGame.eventsThisUpdate(),
                          {GameEventType::BonusCollected}) &&
                bonusEffectsGame.eventsThisUpdate()[0].bonusType ==
                    BonusType::Bandage &&
                bonusEffectsGame.eventsThisUpdate()[0].points == 300 &&
                bonusEffectsGame.players()[0].hitPoints == 3 &&
                bonusEffectsGame.players()[0].score == 2100 &&
                bonusEffectsGame.bonusMessage() ==
                    "P1  BANDAGE  HP 3/3  +300" &&
                std::fabs(bonusEffectsGame.bonusMessageTimer() - 2.2f) <
                    0.0001f,
            "eligible Bandage did not restore exactly one hit point"))
        return 1;

    Game3DTestAccess::setPlayerLevel(bonusEffectsGame, 3);
    Game3DTestAccess::addEventPickup(
        bonusEffectsGame, BonusType::Star,
        bonusEffectsGame.players()[0].position);
    bonusEffectsGame.update(0.0f, {});
    if (!checkTest(
            eventTypesAre(bonusEffectsGame.eventsThisUpdate(),
                          {GameEventType::BonusCollected}) &&
                bonusEffectsGame.players()[0].level == 3 &&
                bonusEffectsGame.players()[0].score == 2400 &&
                bonusEffectsGame.players()[0].stageTally.bonusPoints == 2400 &&
                bonusEffectsGame.bonuses().empty(),
            "maximum-level Star pickup changed level or common rewards"))
        return 1;

    std::vector<ShellTankPresentationSnapshot> shieldPresentation;
    Game3D shieldHitGame(resourceRoot, 0xe0012008U);
    if (!shieldHitGame.start(1, 3, 1, nations) ||
        !Game3DTestAccess::prepareGameEventScenario(shieldHitGame))
        return 1;
    Game3DTestAccess::setPlayerCombatState(shieldHitGame, 2, 3);
    Game3DTestAccess::setPlayerProtection(shieldHitGame, 0, 1.0f, true);
    const XZ shieldShellPosition{
        shieldHitGame.players()[0].position.x + 0.5f,
        shieldHitGame.players()[0].position.z - 0.5f};
    const bool shieldMovingBefore = shieldHitGame.players()[0].moving;
    Game3DTestAccess::captureShellTankPresentation(
        shieldHitGame, shieldPresentation);
    const std::size_t shieldEffectCountBefore =
        shieldHitGame.effects().activeCount();
    Game3DTestAccess::addEventShell(
        shieldHitGame, ShellOwner::Enemy, 601,
        shieldShellPosition, {1.0f, 0.0f});
    shieldHitGame.update(0.0f, {});
    const bool shieldFirstHitState =
        shieldHitGame.eventsThisUpdate().empty() &&
        shieldHitGame.players()[0].hitPoints == 2 &&
        shieldHitGame.players()[0].hasBoat &&
        shieldHitGame.players()[0].active &&
        shieldHitGame.players()[0].moving == shieldMovingBefore &&
        shieldHitGame.players()[0].shieldTimer == 1.0f &&
        shieldHitGame.players()[0].directKillStreak == 3 &&
        Game3DTestAccess::cameraShake(shieldHitGame, 0) == 0.0f &&
        shieldHitGame.shells().size() == 1U &&
        shieldHitGame.shells()[0].impacting &&
        shieldHitGame.shells()[0].position.x == shieldShellPosition.x &&
        shieldHitGame.shells()[0].position.z == shieldShellPosition.z;
    shieldHitGame.update(0.0f, {});
    const auto *shieldArmor =
        shellTankPresentationActionAt<SpawnTankArmorImpactAction>(
            shieldPresentation, 0U);
    const auto *shieldImpact =
        shellTankPresentationActionAt<CommitTankShellImpactAction>(
            shieldPresentation, 1U);
    if (!checkTest(shieldFirstHitState &&
                       shieldHitGame.eventsThisUpdate().empty() &&
                       shieldHitGame.players()[0].hitPoints == 2 &&
                       shieldHitGame.players()[0].hasBoat,
                   "Shield hit changed state, emitted an event, or repeated"))
        return 1;
    if (!checkTest(
            shellTankPresentationStepsAre(
                shieldPresentation,
                {ShellTankPresentationStep::ArmorImpactFxSpawned,
                 ShellTankPresentationStep::ShellImpactCommitted}) &&
                shellTankImpactWasDeferred(shieldPresentation) &&
                shieldPresentation[0].preCommitFx &&
                shieldPresentation[0].eventCount == 0U &&
                shieldPresentation[0].effectCount >
                    shieldEffectCountBefore &&
                shieldPresentation[0].players.size() == 1U &&
                shieldPresentation[0].players[0].hitPoints == 2 &&
                shieldPresentation[0].players[0].hasBoat &&
                shieldPresentation[0].players[0].shieldTimer == 1.0f &&
                shieldArmor != nullptr && shieldArmor->heavy &&
                shieldArmor->position.x == shieldShellPosition.x &&
                shieldArmor->position.y == 0.52f &&
                shieldArmor->position.z == shieldShellPosition.z &&
                shieldArmor->normal.x == -1.0f &&
                shieldArmor->normal.y == 0.28f &&
                shieldArmor->normal.z == 0.0f &&
                !shieldPresentation[1].preCommitFx &&
                shieldPresentation[1].physicalImpact.outcome.target ==
                    CombatTarget::PlayerTank &&
                shieldPresentation[1].physicalImpact.playerCommit.hitResult ==
                    PlayerHitResult::Shielded &&
                shieldPresentation[1].eventCount == 0U &&
                shieldPresentation[1].audioCue == AudioCue::Count &&
                shieldImpact != nullptr &&
                shieldImpact->position.x == shieldShellPosition.x &&
                shieldImpact->position.z == shieldShellPosition.z &&
                shieldPresentation[1].shell.impacting &&
                lengthSquared(shieldPresentation[1].shell.velocity) <
                    0.000001f &&
                std::fabs(shieldPresentation[1].shell.life -
                          kShellImpactDuration) < 0.000001f,
            "Shield presentation lost pre-commit armor FX or silent shell "
            "impact order"))
        return 1;

    std::vector<ShellTankPresentationSnapshot> boatPresentation;
    Game3D boatHitGame(resourceRoot, 0xe0013008U);
    if (!boatHitGame.start(1, 3, 1, nations) ||
        !Game3DTestAccess::prepareGameEventScenario(boatHitGame))
        return 1;
    Game3DTestAccess::setPlayerCombatState(boatHitGame, 2, 3);
    Game3DTestAccess::setPlayerProtection(boatHitGame, 0, 0.0f, true);
    Game3DTestAccess::setCameraShake(boatHitGame, 0, 0.60f);
    const XZ boatShellPosition{
        boatHitGame.players()[0].position.x + 0.5f,
        boatHitGame.players()[0].position.z - 0.5f};
    const bool boatMovingBefore = boatHitGame.players()[0].moving;
    Game3DTestAccess::captureShellTankPresentation(
        boatHitGame, boatPresentation);
    const std::size_t boatEffectCountBefore =
        boatHitGame.effects().activeCount();
    Game3DTestAccess::addEventShell(
        boatHitGame, ShellOwner::Enemy, 602,
        boatShellPosition, {1.0f, 0.0f});
    boatHitGame.update(0.0f, {});
    const auto *boatArmor =
        shellTankPresentationActionAt<SpawnTankArmorImpactAction>(
            boatPresentation, 0U);
    const auto *boatCamera =
        shellTankPresentationActionAt<AssignTankTargetCameraShakeAction>(
            boatPresentation, 1U);
    const auto *boatImpact =
        shellTankPresentationActionAt<CommitTankShellImpactAction>(
            boatPresentation, 2U);
    if (!checkTest(
            boatHitGame.eventsThisUpdate().empty() &&
                boatHitGame.players()[0].hitPoints == 2 &&
                !boatHitGame.players()[0].hasBoat &&
                boatHitGame.players()[0].active &&
                boatHitGame.players()[0].moving == boatMovingBefore &&
                boatHitGame.players()[0].shieldTimer == 0.0f &&
                boatHitGame.players()[0].directKillStreak == 3 &&
                Game3DTestAccess::cameraShake(boatHitGame, 0) == 0.16f &&
                boatHitGame.shells().size() == 1U &&
                boatHitGame.shells()[0].impacting &&
                boatHitGame.shells()[0].position.x == boatShellPosition.x &&
                boatHitGame.shells()[0].position.z == boatShellPosition.z,
            "Boat hit changed HP/streak or lost its exact camera/shell state"))
        return 1;
    if (!checkTest(
            shellTankPresentationStepsAre(
                boatPresentation,
                {ShellTankPresentationStep::ArmorImpactFxSpawned,
                 ShellTankPresentationStep::TargetCameraShakeAssigned,
                 ShellTankPresentationStep::ShellImpactCommitted}) &&
                shellTankImpactWasDeferred(boatPresentation) &&
                boatPresentation[0].preCommitFx &&
                boatPresentation[0].eventCount == 0U &&
                boatPresentation[0].effectCount > boatEffectCountBefore &&
                boatPresentation[0].players.size() == 1U &&
                boatPresentation[0].players[0].hitPoints == 2 &&
                boatPresentation[0].players[0].hasBoat &&
                boatPresentation[0].cameraShake[0] == 0.60f &&
                boatArmor != nullptr && boatArmor->heavy &&
                boatArmor->position.x == boatShellPosition.x &&
                boatArmor->position.y == 0.52f &&
                boatArmor->position.z == boatShellPosition.z &&
                boatArmor->normal.x == -1.0f &&
                boatArmor->normal.y == 0.28f &&
                boatArmor->normal.z == 0.0f &&
                !boatPresentation[1].preCommitFx &&
                boatPresentation[1].physicalImpact.playerCommit.hitResult ==
                    PlayerHitResult::BoatAbsorbed &&
                boatPresentation[1].players.size() == 1U &&
                boatPresentation[1].players[0].hitPoints == 2 &&
                !boatPresentation[1].players[0].hasBoat &&
                boatPresentation[1].cameraShake[0] == 0.16f &&
                boatPresentation[1].eventCount == 0U &&
                boatCamera != nullptr && boatCamera->playerIndex == 0U &&
                boatCamera->value == 0.16f &&
                boatPresentation[2].audioCue == AudioCue::Count &&
                boatImpact != nullptr &&
                boatImpact->position.x == boatShellPosition.x &&
                boatImpact->position.z == boatShellPosition.z &&
                boatPresentation[2].shell.impacting &&
                lengthSquared(boatPresentation[2].shell.velocity) <
                    0.000001f &&
                std::fabs(boatPresentation[2].shell.life -
                          kShellImpactDuration) < 0.000001f,
            "Boat presentation lost pre-commit armor FX, camera assignment, "
            "or silent shell impact order"))
        return 1;

    std::vector<ShellTankPresentationSnapshot> filteredPlayerPresentation;
    Game3D filteredPlayerHitGame(resourceRoot, 0xe0014008U);
    if (!filteredPlayerHitGame.start(2, 3, 1, nations) ||
        !Game3DTestAccess::prepareGameEventScenario(filteredPlayerHitGame))
        return 1;
    const XZ sharedPlayerHitPosition{12.0f, 12.0f};
    Game3DTestAccess::recenterPlayer(filteredPlayerHitGame, 0,
                                    sharedPlayerHitPosition);
    Game3DTestAccess::recenterPlayer(filteredPlayerHitGame, 1,
                                    sharedPlayerHitPosition);
    Game3DTestAccess::setPlayerCreationTimer(filteredPlayerHitGame, 0, 0.5f);
    Game3DTestAccess::setCameraShake(filteredPlayerHitGame, 0, 0.37f);
    Game3DTestAccess::setCameraShake(filteredPlayerHitGame, 1, 0.61f);
    Game3DTestAccess::captureShellTankPresentation(
        filteredPlayerHitGame, filteredPlayerPresentation);
    Game3DTestAccess::addEventShell(
        filteredPlayerHitGame, ShellOwner::Enemy, 603,
        {12.5f, 11.5f}, {1.0f, 0.0f});
    filteredPlayerHitGame.update(0.0f, {});
    const auto *filteredPlayerCamera =
        shellTankPresentationActionAt<AssignTankTargetCameraShakeAction>(
            filteredPlayerPresentation, 2U);
    if (!checkTest(
            eventTypesAre(filteredPlayerHitGame.eventsThisUpdate(),
                          {GameEventType::TankDamaged}) &&
                filteredPlayerHitGame.eventsThisUpdate()[0].targetPlayerId ==
                    1 &&
                filteredPlayerHitGame.players()[0].hitPoints == 3 &&
                filteredPlayerHitGame.players()[1].hitPoints == 2 &&
                Game3DTestAccess::cameraShake(filteredPlayerHitGame, 0) ==
                    0.37f &&
                Game3DTestAccess::cameraShake(filteredPlayerHitGame, 1) ==
                    0.22f &&
                shellTankPresentationStepsAre(
                    filteredPlayerPresentation,
                    {ShellTankPresentationStep::ArmorImpactFxSpawned,
                     ShellTankPresentationStep::PhysicalEventsAppended,
                     ShellTankPresentationStep::TargetCameraShakeAssigned,
                     ShellTankPresentationStep::AudioRequested,
                     ShellTankPresentationStep::ShellImpactCommitted}) &&
                filteredPlayerCamera != nullptr &&
                filteredPlayerCamera->playerIndex == 1U &&
                filteredPlayerCamera->value == 0.22f &&
                filteredPlayerHitGame.shells().size() == 1U &&
                filteredPlayerHitGame.shells()[0].impacting,
            "creating first player blocked or misrouted the next eligible "
            "player's camera command"))
        return 1;

    Game3D playerHitPriorityGame(resourceRoot, 0xe0015008U);
    if (!playerHitPriorityGame.start(1, 3, 1, nations) ||
        !Game3DTestAccess::prepareGameEventScenario(playerHitPriorityGame))
        return 1;
    Game3DTestAccess::setPlayerCombatState(playerHitPriorityGame, 3, 0);
    const XZ playerPriorityPosition =
        playerHitPriorityGame.players()[0].position;
    Game3DTestAccess::addEventShell(
        playerHitPriorityGame, ShellOwner::Enemy, 604,
        playerPriorityPosition, {1.0f, 0.0f});
    Game3DTestAccess::addEventShell(
        playerHitPriorityGame, ShellOwner::Player, 0,
        playerPriorityPosition, {-1.0f, 0.0f});
    playerHitPriorityGame.update(0.0f, {});
    if (!checkTest(
            eventTypesAre(playerHitPriorityGame.eventsThisUpdate(),
                          {GameEventType::TankDamaged}) &&
                eventCount(playerHitPriorityGame.eventsThisUpdate(),
                           GameEventType::ShellCancelled) == 0 &&
                playerHitPriorityGame.players()[0].hitPoints == 2 &&
                playerHitPriorityGame.shells().size() == 2U &&
                playerHitPriorityGame.shells()[0].impacting &&
                !playerHitPriorityGame.shells()[1].impacting,
            "player-tank hit stopped taking priority over shell cancellation"))
        return 1;

    std::vector<ShellTankPresentationSnapshot> playerDamagePresentation;
    Game3D playerDamageGame(resourceRoot, 0xe0010008U);
    if (!playerDamageGame.start(1, 3, 1, nations) ||
        !Game3DTestAccess::prepareGameEventScenario(playerDamageGame))
        return 1;
    Game3DTestAccess::setPlayerCombatState(playerDamageGame, 3, 2);
    const XZ playerDamageTargetPosition =
        playerDamageGame.players()[0].position;
    const XZ playerDamageShellPosition{
        playerDamageTargetPosition.x + 0.5f,
        playerDamageTargetPosition.z - 0.5f};
    Game3DTestAccess::captureShellTankPresentation(
        playerDamageGame, playerDamagePresentation);
    const std::size_t playerDamageEffectCountBefore =
        playerDamageGame.effects().activeCount();
    Game3DTestAccess::addEventShell(
        playerDamageGame, ShellOwner::Enemy, 61,
        playerDamageShellPosition, {1.0f, 0.0f});
    playerDamageGame.update(0.0f, {});
    if (!checkTest(eventTypesAre(playerDamageGame.eventsThisUpdate(),
                                 {GameEventType::TankDamaged}) &&
                       playerDamageGame.eventsThisUpdate()[0].cause ==
                           GameEventCause::EnemyShell &&
                       playerDamageGame.eventsThisUpdate()[0].sourceEnemyId == 61 &&
                       playerDamageGame.eventsThisUpdate()[0].targetPlayerId == 0 &&
                       playerDamageGame.eventsThisUpdate()[0].position.x ==
                           playerDamageTargetPosition.x &&
                       playerDamageGame.eventsThisUpdate()[0].position.z ==
                           playerDamageTargetPosition.z &&
                       playerDamageGame.eventsThisUpdate()[0].valueBefore == 3 &&
                       playerDamageGame.eventsThisUpdate()[0].valueAfter == 2 &&
                       playerDamageGame.players()[0].hitPoints == 2 &&
                       playerDamageGame.players()[0].active &&
                       playerDamageGame.players()[0].directKillStreak == 2 &&
                       Game3DTestAccess::cameraShake(playerDamageGame, 0) ==
                           0.22f &&
                       playerDamageGame.shells().size() == 1U &&
                       playerDamageGame.shells()[0].impacting &&
                       playerDamageGame.shells()[0].position.x ==
                           playerDamageShellPosition.x &&
                       playerDamageGame.shells()[0].position.z ==
                           playerDamageShellPosition.z,
                   "nonfatal player damage event or HP transition is incorrect"))
        return 1;
    const auto *playerDamageArmor =
        shellTankPresentationActionAt<SpawnTankArmorImpactAction>(
            playerDamagePresentation, 0U);
    const auto *playerDamageEvents =
        shellTankPresentationActionAt<AppendTankEventsAction>(
            playerDamagePresentation, 1U);
    const auto *playerDamageCamera =
        shellTankPresentationActionAt<AssignTankTargetCameraShakeAction>(
            playerDamagePresentation, 2U);
    const auto *playerDamageAudio =
        shellTankPresentationActionAt<RequestTankAudioAction>(
            playerDamagePresentation, 3U);
    const auto *playerDamageImpact =
        shellTankPresentationActionAt<CommitTankShellImpactAction>(
            playerDamagePresentation, 4U);
    if (!checkTest(
            shellTankPresentationStepsAre(
                playerDamagePresentation,
                {ShellTankPresentationStep::ArmorImpactFxSpawned,
                 ShellTankPresentationStep::PhysicalEventsAppended,
                 ShellTankPresentationStep::TargetCameraShakeAssigned,
                 ShellTankPresentationStep::AudioRequested,
                 ShellTankPresentationStep::ShellImpactCommitted}) &&
                shellTankImpactWasDeferred(playerDamagePresentation) &&
                playerDamagePresentation[0].preCommitFx &&
                playerDamagePresentation[0].eventCount == 0U &&
                playerDamagePresentation[0].effectCount >
                    playerDamageEffectCountBefore &&
                playerDamagePresentation[0].players.size() == 1U &&
                playerDamagePresentation[0].players[0].hitPoints == 3 &&
                playerDamagePresentation[0]
                        .players[0].directKillStreak == 2 &&
                playerDamageArmor != nullptr && playerDamageArmor->heavy &&
                playerDamageArmor->position.x ==
                    playerDamageShellPosition.x &&
                playerDamageArmor->position.y == 0.52f &&
                playerDamageArmor->position.z ==
                    playerDamageShellPosition.z &&
                playerDamageArmor->normal.x == -1.0f &&
                playerDamageArmor->normal.y == 0.28f &&
                playerDamageArmor->normal.z == 0.0f &&
                playerDamagePresentation[1].eventCount == 1U &&
                playerDamagePresentation[1]
                        .physicalImpact.playerCommit.hitResult ==
                    PlayerHitResult::Damaged &&
                playerDamagePresentation[1].players[0].hitPoints == 2 &&
                playerDamageEvents != nullptr &&
                playerDamageEvents->events.size() == 1U &&
                playerDamageEvents->events[0] ==
                    playerDamageGame.eventsThisUpdate()[0] &&
                playerDamagePresentation[2].cameraShake[0] == 0.22f &&
                playerDamageCamera != nullptr &&
                playerDamageCamera->playerIndex == 0U &&
                playerDamageCamera->value == 0.22f &&
                playerDamagePresentation[3].audioCue ==
                    AudioCue::PlayerHit &&
                playerDamageAudio != nullptr &&
                playerDamageAudio->cue == AudioCue::PlayerHit &&
                playerDamageImpact != nullptr &&
                playerDamageImpact->position.x ==
                    playerDamageShellPosition.x &&
                playerDamageImpact->position.z ==
                    playerDamageShellPosition.z &&
                playerDamagePresentation[4].shell.impacting &&
                lengthSquared(playerDamagePresentation[4].shell.velocity) <
                    0.000001f &&
                std::fabs(playerDamagePresentation[4].shell.life -
                          kShellImpactDuration) < 0.000001f,
            "nonfatal player presentation lost pre-commit armor FX-event-"
            "camera-audio-request-impact order"))
        return 1;

    std::vector<ShellTankPresentationSnapshot> playerDeathPresentation;
    Game3D playerDeathGame(resourceRoot, 0xe0010009U);
    if (!playerDeathGame.start(1, 3, 1, nations) ||
        !Game3DTestAccess::prepareGameEventScenario(playerDeathGame))
        return 1;
    Game3DTestAccess::setPlayerCombatState(playerDeathGame, 1, 4);
    const XZ playerDeathTargetPosition =
        playerDeathGame.players()[0].position;
    const XZ playerDeathShellPosition{
        playerDeathTargetPosition.x + 0.5f,
        playerDeathTargetPosition.z - 0.5f};
    Game3DTestAccess::captureShellTankPresentation(
        playerDeathGame, playerDeathPresentation);
    const std::size_t playerDeathEffectCountBefore =
        playerDeathGame.effects().activeCount();
    Game3DTestAccess::addEventShell(
        playerDeathGame, ShellOwner::Enemy, 62,
        playerDeathShellPosition, {1.0f, 0.0f});
    playerDeathGame.update(0.0f, {});
    if (!checkTest(eventTypesAre(playerDeathGame.eventsThisUpdate(),
                                 {GameEventType::TankDestroyed}) &&
                       playerDeathGame.eventsThisUpdate()[0].cause ==
                           GameEventCause::EnemyShell &&
                       playerDeathGame.eventsThisUpdate()[0].sourceEnemyId ==
                           62 &&
                       playerDeathGame.eventsThisUpdate()[0].targetPlayerId ==
                           0 &&
                       playerDeathGame.eventsThisUpdate()[0].position.x ==
                           playerDeathTargetPosition.x &&
                       playerDeathGame.eventsThisUpdate()[0].position.z ==
                           playerDeathTargetPosition.z &&
                       playerDeathGame.eventsThisUpdate()[0].valueBefore == 1 &&
                       playerDeathGame.eventsThisUpdate()[0].valueAfter == 0 &&
                       !playerDeathGame.players()[0].active &&
                       !playerDeathGame.players()[0].moving &&
                       playerDeathGame.players()[0].directKillStreak == 0 &&
                       playerDeathGame.players()[0].deathTimer ==
                           kTankDeathDuration &&
                       playerDeathGame.players()[0].respawnTimer == 0.0f &&
                       playerDeathGame.players()[0].shieldTimer == 0.0f &&
                       Game3DTestAccess::cameraShake(playerDeathGame, 0) ==
                           0.42f &&
                       playerDeathGame.shells().size() == 1U &&
                       playerDeathGame.shells()[0].impacting &&
                       playerDeathGame.shells()[0].position.x ==
                           playerDeathShellPosition.x &&
                       playerDeathGame.shells()[0].position.z ==
                           playerDeathShellPosition.z,
                   "fatal player hit did not commit death before its event"))
        return 1;
    const auto *playerDeathArmor =
        shellTankPresentationActionAt<SpawnTankArmorImpactAction>(
            playerDeathPresentation, 0U);
    const auto *playerDeathEvents =
        shellTankPresentationActionAt<AppendTankEventsAction>(
            playerDeathPresentation, 1U);
    const auto *playerDeathExplosion =
        shellTankPresentationActionAt<SpawnTankExplosionAction>(
            playerDeathPresentation, 2U);
    const auto *playerDeathCamera =
        shellTankPresentationActionAt<AssignTankTargetCameraShakeAction>(
            playerDeathPresentation, 3U);
    const auto *playerDeathAudio =
        shellTankPresentationActionAt<RequestTankAudioAction>(
            playerDeathPresentation, 4U);
    const auto *playerDeathImpact =
        shellTankPresentationActionAt<CommitTankShellImpactAction>(
            playerDeathPresentation, 5U);
    const Color expectedPlayerDeathColor = playerColor(0);
    if (!checkTest(
            shellTankPresentationStepsAre(
                playerDeathPresentation,
                {ShellTankPresentationStep::ArmorImpactFxSpawned,
                 ShellTankPresentationStep::PhysicalEventsAppended,
                 ShellTankPresentationStep::TankExplosionFxSpawned,
                 ShellTankPresentationStep::TargetCameraShakeAssigned,
                 ShellTankPresentationStep::AudioRequested,
                 ShellTankPresentationStep::ShellImpactCommitted}) &&
                shellTankImpactWasDeferred(playerDeathPresentation) &&
                playerDeathPresentation[0].preCommitFx &&
                playerDeathPresentation[0].eventCount == 0U &&
                playerDeathPresentation[0].effectCount >
                    playerDeathEffectCountBefore &&
                playerDeathPresentation[0].players.size() == 1U &&
                playerDeathPresentation[0].players[0].hitPoints == 1 &&
                playerDeathPresentation[0].players[0].active &&
                playerDeathPresentation[0]
                        .players[0].directKillStreak == 4 &&
                playerDeathArmor != nullptr && playerDeathArmor->heavy &&
                playerDeathArmor->position.x == playerDeathShellPosition.x &&
                playerDeathArmor->position.y == 0.52f &&
                playerDeathArmor->position.z == playerDeathShellPosition.z &&
                playerDeathArmor->normal.x == -1.0f &&
                playerDeathArmor->normal.y == 0.28f &&
                playerDeathArmor->normal.z == 0.0f &&
                playerDeathPresentation[1].eventCount == 1U &&
                playerDeathPresentation[1]
                        .physicalImpact.playerCommit.hitResult ==
                    PlayerHitResult::Destroyed &&
                playerDeathPresentation[1].players[0].hitPoints == 0 &&
                !playerDeathPresentation[1].players[0].active &&
                playerDeathPresentation[1]
                        .players[0].directKillStreak == 0 &&
                playerDeathEvents != nullptr &&
                playerDeathEvents->events.size() == 1U &&
                playerDeathEvents->events[0] ==
                    playerDeathGame.eventsThisUpdate()[0] &&
                playerDeathPresentation[2].effectCount >
                    playerDeathPresentation[1].effectCount &&
                playerDeathExplosion != nullptr &&
                playerDeathExplosion->position.x ==
                    playerDeathTargetPosition.x &&
                playerDeathExplosion->position.y == 0.42f &&
                playerDeathExplosion->position.z ==
                    playerDeathTargetPosition.z &&
                playerDeathExplosion->color.r == expectedPlayerDeathColor.r &&
                playerDeathExplosion->color.g == expectedPlayerDeathColor.g &&
                playerDeathExplosion->color.b == expectedPlayerDeathColor.b &&
                playerDeathExplosion->color.a == expectedPlayerDeathColor.a &&
                playerDeathPresentation[3].cameraShake[0] == 0.42f &&
                playerDeathCamera != nullptr &&
                playerDeathCamera->playerIndex == 0U &&
                playerDeathCamera->value == 0.42f &&
                playerDeathPresentation[4].audioCue ==
                    AudioCue::PlayerDestroyed &&
                playerDeathAudio != nullptr &&
                playerDeathAudio->cue == AudioCue::PlayerDestroyed &&
                playerDeathImpact != nullptr &&
                playerDeathImpact->position.x ==
                    playerDeathShellPosition.x &&
                playerDeathImpact->position.z ==
                    playerDeathShellPosition.z &&
                playerDeathPresentation[5].shell.impacting &&
                lengthSquared(playerDeathPresentation[5].shell.velocity) <
                    0.000001f &&
                std::fabs(playerDeathPresentation[5].shell.life -
                          kShellImpactDuration) < 0.000001f,
            "fatal player presentation lost pre-commit armor FX-event-"
            "explosion-camera-audio-request-impact order"))
        return 1;

    Game3D respawnGame(resourceRoot, 0xe001000aU);
    if (!respawnGame.start(1, 3, 1, nations) ||
        !Game3DTestAccess::prepareGameEventScenario(respawnGame))
        return 1;
    Game3DTestAccess::preparePlayerRespawnEvent(respawnGame);
    Game3DTestAccess::setPlayerDeathEntryState(
        respawnGame, 0, 37, false, 2, 0.02f);
    const XZ expectedRespawnPosition{17.0f, 25.0f};
    Game3DTestAccess::addEventShell(
        respawnGame, ShellOwner::Player, 0, {1.0f, 1.0f}, {},
        false, true, 0.25f);
    Game3DTestAccess::addEventShell(
        respawnGame, ShellOwner::Player, 37, {2.0f, 2.0f}, {},
        false, true, 0.25f);
    std::vector<AudioCue> respawnAudioRequests;
    int respawnAudioSnapshotCount = 0;
    std::vector<Player> respawnPlayersAtAudio;
    std::vector<GameEvent> respawnEventsAtAudio;
    std::vector<Shell> respawnShellsAtAudio;
    Game3DTestAccess::captureAudioRequests(
        respawnGame, respawnAudioRequests,
        [&](AudioCue cue) {
            if (cue != AudioCue::PlayerRespawn)
                return;
            ++respawnAudioSnapshotCount;
            respawnPlayersAtAudio = respawnGame.players();
            respawnEventsAtAudio = respawnGame.eventsThisUpdate();
            respawnShellsAtAudio = respawnGame.shells();
        });
    respawnGame.update(0.019f, {});
    const bool respawnWaitedForDeathBoundary =
        respawnGame.eventsThisUpdate().empty() &&
        respawnAudioRequests.empty() &&
        !respawnGame.players()[0].active &&
        respawnGame.players()[0].lives == 2 &&
        respawnGame.shells().size() == 2U;
    respawnGame.update(0.002f, {});
    if (!checkTest(respawnWaitedForDeathBoundary &&
                       eventTypesAre(respawnGame.eventsThisUpdate(),
                                 {GameEventType::PlayerRespawned}) &&
                       respawnGame.eventsThisUpdate()[0].targetPlayerId == 37 &&
                       respawnGame.eventsThisUpdate()[0].position.x ==
                           respawnGame.players()[0].position.x &&
                       respawnGame.eventsThisUpdate()[0].position.z ==
                           respawnGame.players()[0].position.z &&
                       respawnGame.eventsThisUpdate()[0].valueAfter ==
                           respawnGame.players()[0].lives &&
                       respawnGame.players()[0].lives == 1 &&
                       respawnGame.players()[0].active &&
                       respawnGame.players()[0].level == 0 &&
                       respawnGame.players()[0].hitPoints == 3 &&
                       distanceSquared(respawnGame.players()[0].position,
                                       expectedRespawnPosition) == 0.0f &&
                       respawnGame.players()[0].creationTimer == 1.0f &&
                       respawnGame.players()[0].deathTimer == 0.0f &&
                       respawnGame.players()[0].shieldTimer == 10.0f &&
                       std::fabs(respawnGame.players()[0].fireCooldown -
                                 kPlayerReloadTime) < 0.0001f &&
                       respawnGame.players()[0].dustCooldown == 0.0f &&
                       respawnGame.players()[0].directKillStreak == 0 &&
                       respawnGame.players()[0].streakPopupTimer == 0.0f &&
                       respawnGame.players()[0].score == 2375 &&
                       respawnGame.players()[0].stageTally.destroyed ==
                           std::array<int, kEnemyTypeCount>{{2, 3, 5, 7}} &&
                       respawnGame.players()[0].stageTally.enemyPoints ==
                           std::array<int, kEnemyTypeCount>{{100, 150, 250,
                                                            350}} &&
                       respawnGame.players()[0].stageTally.bonusPoints == 600 &&
                       respawnGame.players()[0]
                               .stageTally.scoreAtStageStart == 925 &&
                       respawnGame.shells().size() == 1U &&
                       respawnGame.shells()[0].ownerIndex == 37,
                   "respawn state/tally, event identity, or slot-owned shell cleanup changed"))
        return 1;
    if (!checkTest(
            respawnAudioRequests ==
                    std::vector<AudioCue>{AudioCue::PlayerRespawn} &&
                respawnAudioSnapshotCount == 1 &&
                respawnPlayersAtAudio.size() == 1U &&
                settlementPlayerVectorsMatch(respawnPlayersAtAudio,
                                             respawnGame.players()) &&
                respawnPlayersAtAudio[0].lives == 1 &&
                respawnPlayersAtAudio[0].active &&
                respawnPlayersAtAudio[0].deathTimer == 0.0f &&
                respawnPlayersAtAudio[0].creationTimer == 1.0f &&
                distanceSquared(respawnPlayersAtAudio[0].position,
                                expectedRespawnPosition) == 0.0f &&
                eventTypesAre(respawnEventsAtAudio,
                              {GameEventType::PlayerRespawned}) &&
                respawnEventsAtAudio[0].targetPlayerId == 37 &&
                respawnEventsAtAudio[0].valueAfter == 1 &&
                respawnShellsAtAudio.size() == 1U &&
                respawnShellsAtAudio[0].owner == ShellOwner::Player &&
                respawnShellsAtAudio[0].ownerIndex == 37,
            "respawn audio was not requested exactly once after life debit, "
            "slot-shell cleanup, prepared spawn, and event append"))
    {
        return 1;
    }
    respawnGame.update(0.0f, {});
    if (!checkTest(respawnGame.eventsThisUpdate().empty(),
                   "creation state repeated the respawn event"))
        return 1;

    Game3D finalLifeShellGame(resourceRoot, 0xe0010016U);
    if (!finalLifeShellGame.start(1, 3, 1, nations) ||
        !Game3DTestAccess::prepareGameEventScenario(finalLifeShellGame))
        return 1;
    Game3DTestAccess::setPlayerDeathEntryState(
        finalLifeShellGame, 0, 41, false, 1, 0.02f);
    Game3DTestAccess::addEventShell(
        finalLifeShellGame, ShellOwner::Player, 0, {1.0f, 1.0f}, {},
        false, true, 0.04f);
    finalLifeShellGame.update(0.019f, {});
    const bool finalLifeWaited =
        finalLifeShellGame.players()[0].lives == 1 &&
        !finalLifeShellGame.gameOver() &&
        finalLifeShellGame.shells().size() == 1U;
    finalLifeShellGame.update(0.002f, {});
    const bool finalShellDelayedGameOver =
        finalLifeShellGame.players()[0].lives == 0 &&
        !finalLifeShellGame.players()[0].active &&
        !finalLifeShellGame.gameOver() &&
        finalLifeShellGame.shells().size() == 1U &&
        finalLifeShellGame.shells()[0].ownerIndex == 0;
    finalLifeShellGame.update(0.05f, {});
    if (!checkTest(finalLifeWaited && finalShellDelayedGameOver &&
                       finalLifeShellGame.shells().empty() &&
                       finalLifeShellGame.gameOver(),
                   "final-life slot-owned shell did not delay Game Over until expiry"))
        return 1;

    std::vector<ShellMapCorePresentationSnapshot> wallPresentation;
    Game3D wallGame(resourceRoot, 0xe001000bU);
    if (!wallGame.start(1, 3, 1, nations) ||
        !Game3DTestAccess::prepareGameEventScenario(wallGame))
        return 1;
    const GovernmentWallSegment wall = governmentWallSegment(0);
    const int wallHealthBefore = wallGame.map().governmentWallHealth(0);
    Game3DTestAccess::addEventShell(wallGame, ShellOwner::Enemy, 63,
                                    wall.center, {0.0f, 1.0f});
    Game3DTestAccess::captureShellMapCorePresentation(
        wallGame, wallPresentation);
    wallGame.update(0.0f, {});
    if (!checkTest(eventTypesAre(wallGame.eventsThisUpdate(),
                                 {GameEventType::BaseDamaged}) &&
                       wallGame.eventsThisUpdate()[0].basePart ==
                           GovernmentBasePart::Wall &&
                       wallGame.eventsThisUpdate()[0].baseSegmentIndex == 0 &&
                       wallGame.eventsThisUpdate()[0].sourceEnemyId == 63 &&
                       wallGame.eventsThisUpdate()[0].valueBefore ==
                           wallHealthBefore &&
                       wallGame.eventsThisUpdate()[0].valueAfter ==
                           wallHealthBefore - 1 &&
                       wallGame.map().governmentWallHealth(0) ==
                           wallHealthBefore - 1,
                   "government wall damage event and health diverged"))
        return 1;
    if (!checkTest(
            shellMapCorePresentationStepsAre(
                wallPresentation,
                {ShellMapCorePresentationStep::PhysicalEventsAppended,
                 ShellMapCorePresentationStep::BrickImpactFxSpawned,
                 ShellMapCorePresentationStep::ShellImpactCommitted}) &&
                wallPresentation[0].eventCount == 1U &&
                shellMapCorePresentationActionAt<
                    AppendMapCoreEventsAction>(wallPresentation, 0U) !=
                    nullptr &&
                shellMapCorePresentationActionAt<
                    AppendMapCoreEventsAction>(wallPresentation, 0U)
                        ->events == wallGame.eventsThisUpdate() &&
                wallPresentation[1].effectCount >
                    wallPresentation[0].effectCount &&
                shellMapCorePresentationActionAt<
                    SpawnMapCoreBrickImpactAction>(
                    wallPresentation, 1U) != nullptr &&
                presentationFloat3Near(
                    shellMapCorePresentationActionAt<
                        SpawnMapCoreBrickImpactAction>(
                        wallPresentation, 1U)->position,
                    {wall.center.x, 0.54f, wall.center.z}) &&
                presentationFloat3Near(
                    shellMapCorePresentationActionAt<
                        SpawnMapCoreBrickImpactAction>(
                        wallPresentation, 1U)->normal,
                    {0.0f, 0.28f, -1.0f}) &&
                !shellMapCorePresentationActionAt<
                     SpawnMapCoreBrickImpactAction>(
                     wallPresentation, 1U)->destroyed &&
                latestShell(wallPresentation[2]) != nullptr &&
                latestShell(wallPresentation[2])->impacting &&
                lengthSquared(
                    latestShell(wallPresentation[2])->velocity) <
                    0.000001f &&
                std::fabs(latestShell(wallPresentation[2])->life -
                          kShellImpactDuration) < 0.000001f,
            "enemy wall presentation requested audio or changed ordered "
            "event-FX-impact consumption"))
        return 1;
    wallPresentation.clear();
    Game3DTestAccess::addEventPickup(
        wallGame, bonus_assets::Type::Shovel,
        wallGame.players()[0].position);
    wallGame.update(0.0f, {});
    const bool shovelCollectionRecorded =
        eventTypesAre(wallGame.eventsThisUpdate(),
                      {GameEventType::BonusCollected}) &&
        wallGame.map().governmentWallsSteel();
    const std::size_t protectedWallEffectCountBefore =
        wallGame.effects().activeCount();
    Game3DTestAccess::addEventShell(wallGame, ShellOwner::Enemy, 63,
                                    wall.center, {0.0f, 1.0f});
    wallGame.update(0.0f, {});
    if (!checkTest(shovelCollectionRecorded &&
                       wallGame.eventsThisUpdate().empty() &&
                       wallGame.map().governmentWallHealth(0) ==
                           kGovernmentWallMaximumHealth,
                   "steel-protected government wall emitted a damage event"))
        return 1;
    if (!checkTest(
            shellMapCorePresentationStepsAre(
                wallPresentation,
                {ShellMapCorePresentationStep::SurfaceImpactFxSpawned,
                 ShellMapCorePresentationStep::ShellImpactCommitted}) &&
                wallPresentation[0].eventCount == 0U &&
                shellMapCorePresentationActionAt<
                    SpawnMapCoreSurfaceImpactAction>(
                    wallPresentation, 0U) != nullptr &&
                presentationFloat3Near(
                    shellMapCorePresentationActionAt<
                        SpawnMapCoreSurfaceImpactAction>(
                        wallPresentation, 0U)->position,
                    {wall.center.x, 0.43f, wall.center.z}) &&
                presentationFloat3Near(
                    shellMapCorePresentationActionAt<
                        SpawnMapCoreSurfaceImpactAction>(
                        wallPresentation, 0U)->normal,
                    {0.0f, 1.0f, 0.0f}) &&
                shellMapCorePresentationActionAt<
                    SpawnMapCoreSurfaceImpactAction>(
                    wallPresentation, 0U)->heavy &&
                wallPresentation[0].effectCount >
                    protectedWallEffectCountBefore &&
                latestShell(wallPresentation[1]) != nullptr &&
                latestShell(wallPresentation[1])->impacting &&
                lengthSquared(latestShell(wallPresentation[1])->velocity) <
                    0.000001f,
            "steel-protected wall presentation gained an event or audio "
            "step"))
        return 1;

    wallPresentation.clear();
    const std::size_t playerSteelEffectCountBefore =
        wallGame.effects().activeCount();
    Game3DTestAccess::addEventShell(wallGame, ShellOwner::Player, 0,
                                    wall.center, {0.0f, 1.0f});
    wallGame.update(0.0f, {});
    if (!checkTest(
            shellMapCorePresentationStepsAre(
                wallPresentation,
                {ShellMapCorePresentationStep::SurfaceImpactFxSpawned,
                 ShellMapCorePresentationStep::AudioRequested,
                 ShellMapCorePresentationStep::ShellImpactCommitted}) &&
                std::all_of(
                    wallPresentation.begin(), wallPresentation.end(),
                    [](const ShellMapCorePresentationSnapshot &snapshot) {
                        return snapshot.eventCount == 0U;
                    }) &&
                shellMapCorePresentationActionAt<
                    SpawnMapCoreSurfaceImpactAction>(
                    wallPresentation, 0U) != nullptr &&
                shellMapCorePresentationActionAt<
                    SpawnMapCoreSurfaceImpactAction>(
                    wallPresentation, 0U)->heavy &&
                wallPresentation[0].effectCount >
                    playerSteelEffectCountBefore &&
                shellMapCorePresentationActionAt<
                    RequestMapCoreAudioAction>(wallPresentation, 1U) !=
                    nullptr &&
                shellMapCorePresentationActionAt<
                    RequestMapCoreAudioAction>(wallPresentation, 1U)->cue ==
                    AudioCue::SteelHit &&
                latestShell(wallPresentation[2]) != nullptr &&
                latestShell(wallPresentation[2])->impacting &&
                lengthSquared(latestShell(wallPresentation[2])->velocity) <
                    0.000001f,
            "player steel presentation lost surface-FX-audio-request-impact "
            "order"))
        return 1;

    std::vector<ShellMapCorePresentationSnapshot> boundaryPresentation;
    Game3D boundaryGame(resourceRoot, 0xe001100bU);
    if (!boundaryGame.start(1, 3, 1, nations) ||
        !Game3DTestAccess::prepareGameEventScenario(boundaryGame))
        return 1;
    const XZ boundaryPosition{0.0f, 8.0f};
    const std::size_t boundaryEffectCountBefore =
        boundaryGame.effects().activeCount();
    Game3DTestAccess::addEventShell(
        boundaryGame, ShellOwner::Player, 0, boundaryPosition,
        {-1.0f, 0.0f}, true);
    Game3DTestAccess::captureShellMapCorePresentation(
        boundaryGame, boundaryPresentation);
    boundaryGame.update(0.0f, {});
    if (!checkTest(
            shellMapCorePresentationStepsAre(
                boundaryPresentation,
                {ShellMapCorePresentationStep::SurfaceImpactFxSpawned,
                 ShellMapCorePresentationStep::AudioRequested,
                 ShellMapCorePresentationStep::ShellImpactCommitted}) &&
                std::all_of(
                    boundaryPresentation.begin(),
                    boundaryPresentation.end(),
                    [](const ShellMapCorePresentationSnapshot &snapshot) {
                        return snapshot.eventCount == 0U;
                    }) &&
                shellMapCorePresentationActionAt<
                    SpawnMapCoreSurfaceImpactAction>(
                    boundaryPresentation, 0U) != nullptr &&
                presentationFloat3Near(
                    shellMapCorePresentationActionAt<
                        SpawnMapCoreSurfaceImpactAction>(
                        boundaryPresentation, 0U)->position,
                    {boundaryPosition.x, 0.43f,
                     boundaryPosition.z}) &&
                shellMapCorePresentationActionAt<
                    SpawnMapCoreSurfaceImpactAction>(
                    boundaryPresentation, 0U)->heavy &&
                boundaryPresentation[0].effectCount >
                    boundaryEffectCountBefore &&
                shellMapCorePresentationActionAt<
                    RequestMapCoreAudioAction>(
                    boundaryPresentation, 1U) != nullptr &&
                shellMapCorePresentationActionAt<
                    RequestMapCoreAudioAction>(
                    boundaryPresentation, 1U)->cue ==
                    AudioCue::BoundaryHit &&
                latestShell(boundaryPresentation[2]) != nullptr &&
                latestShell(boundaryPresentation[2])->impacting &&
                distanceSquared(
                    latestShell(boundaryPresentation[2])->position,
                                boundaryPosition) < 0.000001f &&
                lengthSquared(
                    latestShell(boundaryPresentation[2])->velocity) <
                    0.000001f,
            "player boundary presentation lost surface-FX-audio-request-"
            "impact order"))
        return 1;

    std::vector<ShellMapCorePresentationSnapshot> breachPresentation;
    Game3D breachGame(resourceRoot, 0xe001200bU);
    if (!breachGame.start(1, 3, 1, nations) ||
        !Game3DTestAccess::prepareGameEventScenario(breachGame))
        return 1;
    Game3DTestAccess::recenterPlayer(breachGame, 0, wall.center);
    Game3DTestAccess::captureShellMapCorePresentation(
        breachGame, breachPresentation);
    Game3DTestAccess::addEventShell(
        breachGame, ShellOwner::Player, 0, wall.center,
        {0.0f, 1.0f}, true);
    breachGame.update(0.0f, {});
    breachPresentation.clear();
    const std::size_t breachEffectCountBefore =
        breachGame.effects().activeCount();
    const float breachCameraShakeBefore =
        Game3DTestAccess::cameraShake(breachGame, 0);
    Game3DTestAccess::addEventShell(
        breachGame, ShellOwner::Player, 0, wall.center,
        {0.0f, 1.0f}, true);
    breachGame.update(0.0f, {});
    if (!checkTest(
            shellMapCorePresentationStepsAre(
                breachPresentation,
                {ShellMapCorePresentationStep::PhysicalEventsAppended,
                 ShellMapCorePresentationStep::BrickImpactFxSpawned,
                 ShellMapCorePresentationStep::WallBreachImpactFxSpawned,
                 ShellMapCorePresentationStep::CameraShakePassCompleted,
                 ShellMapCorePresentationStep::AudioRequested,
                 ShellMapCorePresentationStep::ShellImpactCommitted}) &&
                std::all_of(
                    breachPresentation.begin(), breachPresentation.end(),
                    [](const ShellMapCorePresentationSnapshot &snapshot) {
                        return snapshot.eventCount == 1U;
                    }) &&
                breachPresentation[0].events[0].valueBefore == 2 &&
                breachPresentation[0].events[0].valueAfter == 0 &&
                shellMapCorePresentationActionAt<
                    SpawnMapCoreBrickImpactAction>(
                    breachPresentation, 1U) != nullptr &&
                presentationFloat3Near(
                    shellMapCorePresentationActionAt<
                        SpawnMapCoreBrickImpactAction>(
                        breachPresentation, 1U)->position,
                    {wall.center.x, 0.54f, wall.center.z}) &&
                shellMapCorePresentationActionAt<
                    SpawnMapCoreBrickImpactAction>(
                    breachPresentation, 1U)->power &&
                shellMapCorePresentationActionAt<
                    SpawnMapCoreBrickImpactAction>(
                    breachPresentation, 1U)->destroyed &&
                shellMapCorePresentationActionAt<
                    SpawnGovernmentWallBreachImpactAction>(
                    breachPresentation, 2U) != nullptr &&
                presentationFloat3Near(
                    shellMapCorePresentationActionAt<
                        SpawnGovernmentWallBreachImpactAction>(
                        breachPresentation, 2U)->position,
                    {wall.center.x, 0.48f, wall.center.z}) &&
                presentationFloat3Near(
                    shellMapCorePresentationActionAt<
                        SpawnGovernmentWallBreachImpactAction>(
                        breachPresentation, 2U)->normal,
                    {0.0f, 0.42f, -1.0f}) &&
                shellMapCorePresentationActionAt<
                    ApplyMapCoreRadialCameraShakeAction>(
                    breachPresentation, 3U) != nullptr &&
                distanceSquared(
                    shellMapCorePresentationActionAt<
                        ApplyMapCoreRadialCameraShakeAction>(
                        breachPresentation, 3U)->origin,
                    wall.center) < 0.000001f &&
                std::fabs(
                    shellMapCorePresentationActionAt<
                        ApplyMapCoreRadialCameraShakeAction>(
                        breachPresentation, 3U)->maximum - 0.22f) <
                    0.000001f &&
                std::fabs(
                    breachPresentation[3].cameraShake[0] - 0.22f) <
                    0.000001f &&
                breachPresentation[1].effectCount >
                    breachEffectCountBefore &&
                breachPresentation[2].effectCount >
                    breachPresentation[1].effectCount &&
                breachPresentation[3].effectCount ==
                    breachPresentation[2].effectCount &&
                breachPresentation[4].effectCount ==
                    breachPresentation[3].effectCount &&
                shellMapCorePresentationActionAt<
                    RequestMapCoreAudioAction>(
                    breachPresentation, 4U) != nullptr &&
                shellMapCorePresentationActionAt<
                    RequestMapCoreAudioAction>(
                    breachPresentation, 4U)->cue == AudioCue::BrickHit &&
                breachGame.map().governmentWallHealth(0) == 0 &&
                Game3DTestAccess::cameraShake(breachGame, 0) >
                    breachCameraShakeBefore &&
                std::all_of(
                    breachPresentation.begin(),
                    breachPresentation.end() - 1,
                    [](const ShellMapCorePresentationSnapshot &snapshot) {
                        return latestShell(snapshot) != nullptr &&
                               !latestShell(snapshot)->impacting;
                    }) &&
                latestShell(breachPresentation[5]) != nullptr &&
                latestShell(breachPresentation[5])->impacting &&
                lengthSquared(
                    latestShell(breachPresentation[5])->velocity) <
                    0.000001f &&
                std::fabs(latestShell(breachPresentation[5])->life -
                          kShellImpactDuration) < 0.000001f,
            "lethal wall presentation lost event-brick-FX-breach-FX-camera-"
            "audio-request-impact order"))
        return 1;

    Game3D coreWallPriorityGame(resourceRoot, 0xe001200cU);
    if (!coreWallPriorityGame.start(1, 3, 1, nations) ||
        !Game3DTestAccess::prepareGameEventScenario(coreWallPriorityGame))
        return 1;
    const XZ coreWallOverlap =
        kGovernmentBaseCenter +
        wall.outward *
            (kGovernmentCoreRadius + kShellHalfSize - 0.05f);
    const int coreWallHealthBefore =
        coreWallPriorityGame.map().governmentWallHealth(0);
    Game3DTestAccess::addEventShell(
        coreWallPriorityGame, ShellOwner::Enemy, 66,
        coreWallOverlap, wall.outward * -1.0f);
    coreWallPriorityGame.update(0.0f, {});
    if (!checkTest(
            coreWallPriorityGame.map().shellHitsGovernmentCore(
                coreWallOverlap) &&
                eventTypesAre(coreWallPriorityGame.eventsThisUpdate(),
                              {GameEventType::BaseDamaged}) &&
                coreWallPriorityGame.eventsThisUpdate()[0].basePart ==
                    GovernmentBasePart::Wall &&
                coreWallPriorityGame.eventsThisUpdate()[0]
                        .baseSegmentIndex == 0 &&
                coreWallPriorityGame.eventsThisUpdate()[0].sourceEnemyId ==
                    66 &&
                coreWallPriorityGame.eventsThisUpdate()[0].valueBefore ==
                    coreWallHealthBefore &&
                coreWallPriorityGame.eventsThisUpdate()[0].valueAfter ==
                    coreWallHealthBefore - 1 &&
                coreWallPriorityGame.map().governmentWallHealth(0) ==
                    coreWallHealthBefore - 1 &&
                coreWallPriorityGame.baseAlive() &&
                !coreWallPriorityGame.gameOver() &&
                coreWallPriorityGame.shells().size() == 1U &&
                coreWallPriorityGame.shells()[0].impacting,
            "production shell order stopped prioritizing wall over core"))
        return 1;

    std::vector<ShellMapCorePresentationSnapshot>
        repeatedCorePresentation;
    Game3D repeatedCoreGame(resourceRoot, 0xe001100cU);
    if (!repeatedCoreGame.start(1, 3, 1, nations) ||
        !Game3DTestAccess::prepareGameEventScenario(repeatedCoreGame))
        return 1;
    Game3DTestAccess::addEventShell(
        repeatedCoreGame, ShellOwner::Player, 0,
        kGovernmentBaseCenter, {0.0f, 1.0f}, true);
    Game3DTestAccess::addEventShell(
        repeatedCoreGame, ShellOwner::Enemy, 65,
        kGovernmentBaseCenter, {0.0f, -1.0f});
    Game3DTestAccess::captureShellMapCorePresentation(
        repeatedCoreGame, repeatedCorePresentation);
    repeatedCoreGame.update(0.0f, {});
    const std::vector<GameEvent> repeatedCoreEvents =
        repeatedCoreGame.eventsThisUpdate();
    if (!checkTest(eventTypesAre(repeatedCoreEvents,
                                 {GameEventType::BaseDamaged}) &&
                       repeatedCoreEvents[0].cause ==
                           GameEventCause::PlayerShell &&
                       repeatedCoreEvents[0].sourcePlayerId == 0 &&
                       repeatedCoreEvents[0].sourceEnemyId == -1 &&
                       repeatedCoreEvents[0].basePart ==
                           GovernmentBasePart::Core &&
                       repeatedCoreEvents[0].impactKind == ImpactKind::None &&
                       repeatedCoreEvents[0].power &&
                       repeatedCoreEvents[0].position.x ==
                           kGovernmentBaseCenter.x &&
                       repeatedCoreEvents[0].position.z ==
                           kGovernmentBaseCenter.z &&
                       repeatedCoreEvents[0].valueBefore == 1 &&
                       repeatedCoreEvents[0].valueAfter == 0 &&
                       repeatedCoreGame.shells().size() == 2U &&
                       repeatedCoreGame.shells()[0].impacting &&
                       repeatedCoreGame.shells()[1].impacting &&
                       !repeatedCoreGame.baseAlive() &&
                       repeatedCoreGame.gameOver(),
                   "same-frame core hits repeated damage or lost player attribution"))
        return 1;
    if (!checkTest(
            shellMapCorePresentationStepsAre(
                repeatedCorePresentation,
                {ShellMapCorePresentationStep::PhysicalEventsAppended,
                 ShellMapCorePresentationStep::CoreExplosionFxSpawned,
                 ShellMapCorePresentationStep::AudioRequested,
                 ShellMapCorePresentationStep::ShellImpactCommitted,
                 ShellMapCorePresentationStep::ShellImpactCommitted}) &&
                shellMapCorePresentationActionAt<
                    AppendMapCoreEventsAction>(
                    repeatedCorePresentation, 0U) != nullptr &&
                shellMapCorePresentationActionAt<
                    AppendMapCoreEventsAction>(
                    repeatedCorePresentation, 0U)->events.size() == 1U &&
                shellMapCorePresentationActionAt<
                    AppendMapCoreEventsAction>(
                    repeatedCorePresentation, 0U)->events[0].type ==
                    GameEventType::BaseDamaged &&
                repeatedCorePresentation[0].eventCount == 1U &&
                shellMapCorePresentationActionAt<
                    SpawnGovernmentCoreExplosionAction>(
                    repeatedCorePresentation, 1U) != nullptr &&
                presentationFloat3Near(
                    shellMapCorePresentationActionAt<
                        SpawnGovernmentCoreExplosionAction>(
                        repeatedCorePresentation, 1U)->position,
                    {kGovernmentBaseCenter.x, 0.42f,
                     kGovernmentBaseCenter.z}) &&
                presentationColorIs(
                    shellMapCorePresentationActionAt<
                        SpawnGovernmentCoreExplosionAction>(
                        repeatedCorePresentation, 1U)->color,
                    {255U, 195U, 55U, 255U}) &&
                repeatedCorePresentation[1].effectCount >
                    repeatedCorePresentation[0].effectCount &&
                shellMapCorePresentationActionAt<
                    RequestMapCoreAudioAction>(
                    repeatedCorePresentation, 2U) != nullptr &&
                repeatedCorePresentation[2].audioCue ==
                    AudioCue::EagleDestroyed &&
                shellMapCorePresentationActionAt<
                    CommitMapCoreShellImpactAction>(
                    repeatedCorePresentation, 3U) != nullptr &&
                distanceSquared(
                    shellMapCorePresentationActionAt<
                        CommitMapCoreShellImpactAction>(
                        repeatedCorePresentation, 3U)->position,
                    kGovernmentBaseCenter) < 0.000001f &&
                repeatedCorePresentation[3].shells.size() == 2U &&
                repeatedCorePresentation[3].shells[0].owner ==
                    ShellOwner::Player &&
                repeatedCorePresentation[3].shells[0].impacting &&
                repeatedCorePresentation[3].shells[1].owner ==
                    ShellOwner::Enemy &&
                !repeatedCorePresentation[3].shells[1].impacting &&
                shellMapCorePresentationActionAt<
                    CommitMapCoreShellImpactAction>(
                    repeatedCorePresentation, 4U) != nullptr &&
                distanceSquared(
                    shellMapCorePresentationActionAt<
                        CommitMapCoreShellImpactAction>(
                        repeatedCorePresentation, 4U)->position,
                    kGovernmentBaseCenter) < 0.000001f &&
                repeatedCorePresentation[4].shells.size() == 2U &&
                repeatedCorePresentation[4].shells[0].impacting &&
                repeatedCorePresentation[4].shells[1].owner ==
                    ShellOwner::Enemy &&
                repeatedCorePresentation[4].shells[1].impacting &&
                repeatedCorePresentation[4].eventCount == 1U,
            "same-frame core presentation failed to finish the live hit "
            "before silently absorbing the dead-core hit"))
        return 1;

    std::vector<ShellMapCorePresentationSnapshot> corePresentation;
    Game3D coreGame(resourceRoot, 0xe001000cU);
    if (!coreGame.start(1, 3, 1, nations) ||
        !Game3DTestAccess::prepareGameEventScenario(coreGame))
        return 1;
    Game3DTestAccess::addEventShell(coreGame, ShellOwner::Enemy, 64,
                                    kGovernmentBaseCenter, {0.0f, 1.0f});
    Game3DTestAccess::captureShellMapCorePresentation(
        coreGame, corePresentation);
    coreGame.update(0.0f, {});
    if (!checkTest(eventTypesAre(coreGame.eventsThisUpdate(),
                                 {GameEventType::BaseDamaged}) &&
                       coreGame.eventsThisUpdate()[0].cause ==
                           GameEventCause::EnemyShell &&
                       coreGame.eventsThisUpdate()[0].sourcePlayerId == -1 &&
                       coreGame.eventsThisUpdate()[0].sourceEnemyId == 64 &&
                       coreGame.eventsThisUpdate()[0].basePart ==
                           GovernmentBasePart::Core &&
                       coreGame.eventsThisUpdate()[0].impactKind ==
                           ImpactKind::None &&
                       !coreGame.eventsThisUpdate()[0].power &&
                       coreGame.eventsThisUpdate()[0].position.x ==
                           kGovernmentBaseCenter.x &&
                       coreGame.eventsThisUpdate()[0].position.z ==
                           kGovernmentBaseCenter.z &&
                       coreGame.eventsThisUpdate()[0].valueBefore == 1 &&
                       coreGame.eventsThisUpdate()[0].valueAfter == 0 &&
                       !coreGame.baseAlive() && coreGame.gameOver(),
                   "core destruction event did not match game-over state"))
        return 1;
    if (!checkTest(
            shellMapCorePresentationStepsAre(
                corePresentation,
                {ShellMapCorePresentationStep::PhysicalEventsAppended,
                 ShellMapCorePresentationStep::CoreExplosionFxSpawned,
                 ShellMapCorePresentationStep::AudioRequested,
                 ShellMapCorePresentationStep::ShellImpactCommitted}) &&
                shellMapCorePresentationActionAt<
                    AppendMapCoreEventsAction>(
                    corePresentation, 0U) != nullptr &&
                shellMapCorePresentationActionAt<
                    AppendMapCoreEventsAction>(
                    corePresentation, 0U)->events.size() == 1U &&
                shellMapCorePresentationActionAt<
                    AppendMapCoreEventsAction>(
                    corePresentation, 0U)->events[0].sourceEnemyId == 64 &&
                corePresentation[0].eventCount == 1U &&
                shellMapCorePresentationActionAt<
                    SpawnGovernmentCoreExplosionAction>(
                    corePresentation, 1U) != nullptr &&
                presentationFloat3Near(
                    shellMapCorePresentationActionAt<
                        SpawnGovernmentCoreExplosionAction>(
                        corePresentation, 1U)->position,
                    {kGovernmentBaseCenter.x, 0.42f,
                     kGovernmentBaseCenter.z}) &&
                presentationColorIs(
                    shellMapCorePresentationActionAt<
                        SpawnGovernmentCoreExplosionAction>(
                        corePresentation, 1U)->color,
                    {255U, 195U, 55U, 255U}) &&
                corePresentation[1].effectCount >
                    corePresentation[0].effectCount &&
                corePresentation[1].cameraShake ==
                    corePresentation[0].cameraShake &&
                shellMapCorePresentationActionAt<
                    RequestMapCoreAudioAction>(
                    corePresentation, 2U) != nullptr &&
                corePresentation[2].audioCue ==
                    AudioCue::EagleDestroyed &&
                shellMapCorePresentationActionAt<
                    CommitMapCoreShellImpactAction>(
                    corePresentation, 3U) != nullptr &&
                distanceSquared(
                    shellMapCorePresentationActionAt<
                        CommitMapCoreShellImpactAction>(
                        corePresentation, 3U)->position,
                    kGovernmentBaseCenter) < 0.000001f &&
                latestShell(corePresentation[3]) != nullptr &&
                latestShell(corePresentation[3])->owner ==
                    ShellOwner::Enemy &&
                latestShell(corePresentation[3])->impacting &&
                distanceSquared(latestShell(corePresentation[3])->position,
                                kGovernmentBaseCenter) < 0.000001f &&
                lengthSquared(latestShell(corePresentation[3])->velocity) <
                    0.000001f &&
                std::fabs(latestShell(corePresentation[3])->life -
                          kShellImpactDuration) < 0.000001f,
            "enemy core presentation lost event-explosion-audio-request-impact "
            "order"))
        return 1;
    bool coreEndBoundaryHeld = true;
    for (int frame = 0; frame < 61; ++frame)
    {
        coreGame.update(0.05f, {});
        coreEndBoundaryHeld = coreEndBoundaryHeld &&
            !coreGame.settling() && coreGame.eventsThisUpdate().empty();
    }
    coreGame.update(0.04f, {});
    coreEndBoundaryHeld = coreEndBoundaryHeld &&
        !coreGame.settling() && coreGame.eventsThisUpdate().empty();
    coreGame.update(0.02f, {});
    const std::vector<GameEvent> coreEndEvents =
        coreGame.eventsThisUpdate();
    if (!checkTest(coreEndBoundaryHeld && coreGame.settling() &&
                       eventTypesAre(coreEndEvents,
                                     {GameEventType::StageEnded}) &&
                       coreEndEvents[0].stage == 1 &&
                       coreEndEvents[0].stageEndReason ==
                           StageEndReason::BaseDestroyed,
                   "failed stage did not emit one final StageEnded event"))
        return 1;
    coreGame.update(0.0f, {});
    if (!checkTest(coreGame.eventsThisUpdate().empty(),
                   "settlement repeated the failed StageEnded event"))
        return 1;

    Game3D clearOverrideGame(resourceRoot, 0xe001100dU);
    if (!clearOverrideGame.start(1, 3, 1, nations) ||
        !Game3DTestAccess::prepareGameEventScenario(clearOverrideGame))
        return 1;
    Game3DTestAccess::prepareClearedStageEvent(clearOverrideGame);
    clearOverrideGame.update(0.0f, {});
    const bool clearOverrideWindowStarted =
        clearOverrideGame.stageTransition() &&
        clearOverrideGame.eventsThisUpdate().empty();
    Game3DTestAccess::addEventShell(
        clearOverrideGame, ShellOwner::Enemy, 67,
        kGovernmentBaseCenter, {0.0f, 1.0f});
    clearOverrideGame.update(0.0f, {});
    if (!checkTest(
            clearOverrideWindowStarted &&
                eventTypesAre(clearOverrideGame.eventsThisUpdate(),
                              {GameEventType::BaseDamaged}) &&
                clearOverrideGame.eventsThisUpdate()[0].basePart ==
                    GovernmentBasePart::Core &&
                clearOverrideGame.eventsThisUpdate()[0].sourceEnemyId == 67 &&
                !clearOverrideGame.baseAlive() &&
                clearOverrideGame.gameOver() &&
                !clearOverrideGame.stageTransition() &&
                !clearOverrideGame.settling(),
            "core destruction during the clear window did not override victory"))
        return 1;

    Game3D clearGame(resourceRoot, 0xe001000dU);
    if (!clearGame.start(1, 3, 1, nations) ||
        !Game3DTestAccess::prepareGameEventScenario(clearGame))
        return 1;
    Game3DTestAccess::prepareClearedStageEvent(clearGame);
    clearGame.update(0.0f, {});
    const bool clearDelayStartedWithoutTerminalEvent =
        clearGame.stageTransition() && clearGame.eventsThisUpdate().empty();
    bool clearEndBoundaryHeld = true;
    for (int frame = 0; frame < 99; ++frame)
    {
        clearGame.update(0.05f, {});
        clearEndBoundaryHeld = clearEndBoundaryHeld &&
            !clearGame.settling() && clearGame.eventsThisUpdate().empty();
    }
    clearGame.update(0.04f, {});
    clearEndBoundaryHeld = clearEndBoundaryHeld &&
        !clearGame.settling() && clearGame.eventsThisUpdate().empty();
    clearGame.update(0.02f, {});
    const std::vector<GameEvent> clearEndEvents =
        clearGame.eventsThisUpdate();
    if (!checkTest(clearDelayStartedWithoutTerminalEvent &&
                       clearEndBoundaryHeld && clearGame.settling() &&
                       eventTypesAre(clearEndEvents,
                                     {GameEventType::StageEnded}) &&
                       clearEndEvents[0].stageEndReason ==
                           StageEndReason::Cleared,
                   "five-second clear window or final StageEnded event changed"))
        return 1;
    clearGame.update(0.0f, {});
    if (!checkTest(clearGame.eventsThisUpdate().empty(),
                   "settlement repeated the cleared StageEnded event"))
        return 1;

    constexpr std::uint32_t kReplaySeed = 0xe001d157U;
    DeterministicSessionCapture firstReplay;
    DeterministicSessionCapture secondReplay;
    std::string firstReplayError;
    std::string secondReplayError;
    const bool firstCaptured = captureDeterministicSession(
        resourceRoot, kReplaySeed, firstReplay, firstReplayError);
    const bool secondCaptured = captureDeterministicSession(
        resourceRoot, kReplaySeed, secondReplay, secondReplayError);
    if (!checkTest(firstCaptured && secondCaptured &&
                       firstReplay.digest == secondReplay.digest &&
                       firstReplay.events == secondReplay.events &&
                       firstReplay.eventCountsPerFrame ==
                           secondReplay.eventCountsPerFrame &&
                       !firstReplay.digest.state.empty() &&
                       std::any_of(
                           firstReplay.events.begin(),
                           firstReplay.events.end(),
                           [](const GameEvent &event) {
                               return event.type ==
                                          GameEventType::ShellFired &&
                                      event.cause ==
                                          GameEventCause::PlayerShell &&
                                      event.sourcePlayerId == 0;
                           }) &&
                       std::any_of(
                           firstReplay.events.begin(),
                           firstReplay.events.end(),
                           [](const GameEvent &event) {
                               return event.type ==
                                          GameEventType::ShellFired &&
                                      event.cause ==
                                          GameEventCause::EnemyShell;
                           }),
                   "fixed-seed 1/60 session digest/event replay diverged: " +
                       firstReplayError + secondReplayError))
        return 1;

    return 0;
}

int runSessionTimingAndPersistenceSelfTests(const fs::path &resourceRoot)
{
    constexpr std::uint32_t kIntroSeed = 0x1a2b3c01U;
    constexpr std::uint32_t kFastSpawnSeed = 0x1a2b3c02U;
    constexpr std::uint32_t kSlowSpawnSeed = 0x1a2b3c03U;
    const std::string introSeedContext =
        " (seed " + randomSeedLabel(kIntroSeed) + ")";
    const std::string fastSpawnSeedContext =
        " (seed " + randomSeedLabel(kFastSpawnSeed) + ")";
    const std::string slowSpawnSeedContext =
        " (seed " + randomSeedLabel(kSlowSpawnSeed) + ")";
    const std::array<Nation, 2> testNations{{Nation::Germany,
                                              Nation::SovietUnion}};
    Game3D introGame(resourceRoot, kIntroSeed);
    if (!checkTest(introGame.start(1, 3, 1, testNations),
                   introGame.lastError() + introSeedContext))
        return 1;
    int introFrames = 0;
    while (introGame.stageIntro() && introFrames < 100)
    {
        introGame.update(0.05f, {});
        ++introFrames;
    }
    if (!checkTest(!introGame.stageIntro() && introFrames >= 63 &&
                       introFrames <= 65 && introGame.enemies().empty() &&
                       introGame.players().size() == 1U &&
                       std::fabs(introGame.players()[0].creationTimer - 1.0f) <
                           0.0001f,
                   "3.2-second stage intro advanced combat or spawn clocks" +
                       introSeedContext))
        return 1;
    for (int frame = 0; frame < 9; ++frame)
        introGame.update(0.05f, {});
    introGame.update(0.04f, {});
    if (!checkTest(introGame.enemies().empty(),
                   "first enemy warning appeared before the 500 ms cooldown" +
                       introSeedContext))
        return 1;
    introGame.update(0.02f, {});
    if (!checkTest(introGame.enemies().size() == 1U,
                   "first enemy did not spawn after the 500 ms cooldown" +
                       introSeedContext))
        return 1;
    if (!checkTest(introGame.enemies()[0].creationTimer > 0.99f,
                   "first enemy warning did not start at its full one second" +
                       introSeedContext))
        return 1;

    AdvancedGameSettings tunedSettings;
    tunedSettings.playerMaximumHitPoints = 6;
    tunedSettings.enemySpeedPercent = 25;
    tunedSettings.enemyFireRatePercent = -15;
    tunedSettings.enemySpawnRatePercent = 30;
    Game3D fastSpawnGame(resourceRoot, kFastSpawnSeed);
    if (!checkTest(fastSpawnGame.start(1, 3, 1, testNations,
                                       tunedSettings),
                   fastSpawnGame.lastError() + fastSpawnSeedContext))
        return 1;
    int fastIntroGuard = 0;
    while (fastSpawnGame.stageIntro() && fastIntroGuard++ < 100)
        fastSpawnGame.update(0.05f, {});
    for (int frame = 0; frame < 7; ++frame)
        fastSpawnGame.update(0.05f, {});
    fastSpawnGame.update(0.03f, {});
    const bool fastSpawnWaitedForAdjustedInterval =
        fastSpawnGame.enemies().empty();
    fastSpawnGame.update(0.01f, {});
    const AdvancedGameSettings &activeTuning =
        fastSpawnGame.advancedSettings();
    const bool fastSpawnTimingCorrect =
        fastSpawnGame.enemies().size() == 1U &&
        fastSpawnGame.enemies()[0].creationTimer > 0.99f;
    const bool tuningApplied =
        activeTuning.playerMaximumHitPoints == 6 &&
        activeTuning.enemySpeedPercent == 25 &&
        activeTuning.enemyFireRatePercent == -15 &&
        activeTuning.enemySpawnRatePercent == 30 &&
        fastSpawnGame.players()[0].maximumHitPoints == 6 &&
        fastSpawnGame.players()[0].hitPoints == 6;
    const bool tuningPersisted = fastSpawnGame.changeStage(1) &&
        fastSpawnGame.advancedSettings().playerMaximumHitPoints == 6 &&
        fastSpawnGame.advancedSettings().enemySpeedPercent == 25 &&
        fastSpawnGame.advancedSettings().enemyFireRatePercent == -15 &&
        fastSpawnGame.advancedSettings().enemySpawnRatePercent == 30 &&
        fastSpawnGame.restart() &&
        fastSpawnGame.advancedSettings().playerMaximumHitPoints == 6 &&
        fastSpawnGame.advancedSettings().enemySpawnRatePercent == 30;
    if (!checkTest(fastSpawnWaitedForAdjustedInterval &&
                       fastSpawnTimingCorrect && tuningApplied &&
                       tuningPersisted,
                   "advanced settings did not apply or persist across "
                   "stage/restart" + fastSpawnSeedContext))
        return 1;

    AdvancedGameSettings slowSpawnSettings;
    slowSpawnSettings.enemySpawnRatePercent = -30;
    Game3D slowSpawnGame(resourceRoot, kSlowSpawnSeed);
    if (!checkTest(slowSpawnGame.start(1, 3, 1, testNations,
                                       slowSpawnSettings),
                   slowSpawnGame.lastError() + slowSpawnSeedContext))
        return 1;
    int slowIntroGuard = 0;
    while (slowSpawnGame.stageIntro() && slowIntroGuard++ < 100)
        slowSpawnGame.update(0.05f, {});
    for (int frame = 0; frame < 14; ++frame)
        slowSpawnGame.update(0.05f, {});
    const bool slowSpawnWaitedForAdjustedInterval =
        slowSpawnGame.enemies().empty();
    slowSpawnGame.update(0.02f, {});
    if (!checkTest(slowSpawnWaitedForAdjustedInterval &&
                       slowSpawnGame.enemies().size() == 1U &&
                       slowSpawnGame.enemies()[0].creationTimer > 0.99f,
                   "negative spawn-rate setting or fixed warning duration is "
                   "incorrect" + slowSpawnSeedContext))
        return 1;

    AdvancedGameSettings oneHpSettings;
    oneHpSettings.playerMaximumHitPoints = 1;
    Game3D oneHpGame(resourceRoot, 0x1a2b3c04U);
    if (!checkTest(oneHpGame.start(1, 3, 1, testNations,
                                   oneHpSettings) &&
                       oneHpGame.players()[0].maximumHitPoints == 1 &&
                       oneHpGame.players()[0].hitPoints == 1 &&
                       !oneHpGame.players()[0].needsHealing() &&
                       !playerMeetsBonusTypeEligibility(
                           oneHpGame.players()[0],
                           bonus_assets::Type::Bandage),
                   "1-HP game did not disable Bandage eligibility"))
        return 1;

    Game3D bonusGame(resourceRoot, 0x1a2b3c05U);
    if (!checkTest(bonusGame.start(2, 3, 1, testNations), bonusGame.lastError()))
        return 1;
    if (!checkTest(bonusGame.players().size() == 2 &&
                       bonusGame.players()[0].nation == Nation::Germany &&
                       bonusGame.players()[1].nation == Nation::SovietUnion &&
                       bonusGame.baseNation() == Nation::Germany &&
                       bonusGame.players()[0].level == 0 &&
                       bonusGame.players()[1].level == 0,
                   "new game did not apply P1's German base and player nations"))
        return 1;
    if (!checkTest(bonusGame.changeStage(1) &&
                       bonusGame.baseNation() == Nation::Germany &&
                       bonusGame.players()[0].nation == Nation::Germany &&
                       bonusGame.players()[1].nation == Nation::SovietUnion &&
                       bonusGame.restart() &&
                       bonusGame.baseNation() == Nation::Germany &&
                       bonusGame.players()[0].nation == Nation::Germany &&
                       bonusGame.players()[1].nation == Nation::SovietUnion,
                   "stage change or restart did not preserve P1's German base"))
        return 1;
    bonusGame.spawnBonusShowcase();
    if (!checkTest(bonusGame.bonuses().size() ==
                           static_cast<std::size_t>(bonus_assets::Type::Count),
                   "bonus showcase did not create eight original pickups plus Bandage"))
        return 1;

    const std::array<Nation, 2> sovietBaseNations{{
        Nation::SovietUnion, Nation::Germany}};
    Game3D sovietBaseGame(resourceRoot, 0x1a2b3c06U);
    if (!checkTest(sovietBaseGame.start(2, 3, 1, sovietBaseNations) &&
                       sovietBaseGame.baseNation() == Nation::SovietUnion &&
                       sovietBaseGame.players()[1].nation == Nation::Germany &&
                       sovietBaseGame.changeStage(1) &&
                       sovietBaseGame.baseNation() == Nation::SovietUnion,
                   "P2 overrode P1's USSR base or it did not persist"))
        return 1;

    return 0;
}

struct FakeViewTargetAllocator
{
    std::vector<bool> hdrResults{};
    std::vector<bool> fallbackResults{};
    std::size_t hdrIndex = 0U;
    std::size_t fallbackIndex = 0U;
    unsigned int nextId = 1U;
    int fallbackWarnings = 0;
    std::vector<unsigned int> configured{};
    std::vector<unsigned int> unloaded{};
    std::vector<unsigned int> live{};
    bool doubleUnload = false;

    RenderTexture2D load(std::vector<bool> &results, std::size_t &index,
                         int width, int height)
    {
        const bool valid = index < results.size() && results[index];
        ++index;
        RenderTexture2D target{};
        target.id = nextId++;
        live.push_back(target.id);
        target.texture.id = valid ? nextId++ : 0U;
        target.texture.width = width;
        target.texture.height = height;
        target.texture.mipmaps = 1;
        target.texture.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
        target.depth.id = nextId++;
        target.depth.width = width;
        target.depth.height = height;
        target.depth.mipmaps = 1;
        target.depth.format = ViewTargets::kRaylibDepthAttachmentFormat;
        return target;
    }

    ViewTargets::Operations operations()
    {
        ViewTargets::Operations result;
        result.loadHdr = [this](int width, int height) {
            return load(hdrResults, hdrIndex, width, height);
        };
        result.loadFallback = [this](int width, int height) {
            return load(fallbackResults, fallbackIndex, width, height);
        };
        result.valid = [](const RenderTexture2D &target) {
            return IsRenderTextureValid(target);
        };
        result.configure = [this](const RenderTexture2D &target) {
            configured.push_back(target.id);
        };
        result.unload = [this](RenderTexture2D target) {
            unloaded.push_back(target.id);
            const auto found = std::find(live.begin(), live.end(), target.id);
            if (found == live.end())
                doubleUnload = true;
            else
                live.erase(found);
        };
        result.reportFallback = [this]() { ++fallbackWarnings; };
        return result;
    }
};

int runViewTargetAllocationSelfTests()
{
    {
        // Compare the terrain rejection against raylib's actual screen
        // projection across the selectable camera orbit, including tall
        // crowns at map edges, shake, and uncapped portrait co-op framing.
        int rejectedCells = 0;
        bool testedUncappedPortraitSpan = false;
        const auto visibleCellsRetained = [&rejectedCells](
            const Camera3D &camera, int width, int height) {
            const TerrainView view(camera, width, height);
            for (int row = 0; row < kMapSize; ++row)
            {
                for (int column = 0; column < kMapSize; ++column)
                {
                    if (view.containsCell(row, column))
                        continue;
                    ++rejectedCells;
                    Vector2 minimum{100000.0f, 100000.0f};
                    Vector2 maximum{-100000.0f, -100000.0f};
                    for (const float x : {-0.10f, 1.10f})
                    {
                        for (const float y : {-0.05f, 1.44f})
                        {
                            for (const float z : {-0.10f, 1.10f})
                            {
                                const Vector2 screen = GetWorldToScreenEx(
                                    {column + x, y, row + z}, camera,
                                    width, height);
                                minimum.x = std::min(minimum.x, screen.x);
                                minimum.y = std::min(minimum.y, screen.y);
                                maximum.x = std::max(maximum.x, screen.x);
                                maximum.y = std::max(maximum.y, screen.y);
                            }
                        }
                    }
                    if (!(maximum.x < 0.0f || minimum.x > width ||
                          maximum.y < 0.0f || minimum.y > height))
                        return false;
                }
            }
            return true;
        };
        for (const Vector3 focus : {
                 Vector3{0.875f, kGameplayCameraTargetHeight, 25.125f},
                 Vector3{13.0f, kGameplayCameraTargetHeight, 13.0f},
                 Vector3{25.125f, kGameplayCameraTargetHeight, 0.875f}})
        {
            for (const Vector2 viewport : {Vector2{1280, 720},
                                           Vector2{720, 1280},
                                           Vector2{900, 900}})
            {
                const float aspect = viewport.x / viewport.y;
                for (const int yawDegrees : {-45, 0, 15, 45})
                {
                    const CameraPlanarBasis basis =
                        cameraPlanarBasis(yawDegrees);
                    for (const int elevationDegrees : {40, 50, 70})
                    {
                        const GameplayCameraElevationGeometry geometry =
                            gameplayCameraElevationGeometry(elevationDegrees);
                        const float coopSpan = std::max(
                            gameplayCameraSpan({24.25f, 24.25f}, yawDegrees,
                                               elevationDegrees, aspect),
                            gameplayCameraSpan({24.25f, -24.25f}, yawDegrees,
                                               elevationDegrees, aspect));
                        testedUncappedPortraitSpan |=
                            viewport.x < viewport.y && coopSpan > 38.0f;
                        for (const float span : {kSoloCameraSpan, coopSpan})
                        {
                            for (const float shake : {0.0f, 0.12f})
                            {
                                Camera3D camera{};
                                camera.target = {
                                    focus.x + basis.rightX * shake,
                                    focus.y,
                                    focus.z + basis.rightZ * shake};
                                camera.position = {
                                    camera.target.x + basis.offsetX *
                                        (geometry.depthOffset + shake * 0.4f),
                                    camera.target.y + geometry.verticalOffset +
                                        shake * 0.55f,
                                    camera.target.z + basis.offsetZ *
                                        (geometry.depthOffset + shake * 0.4f)};
                                camera.up = {0, 1, 0};
                                camera.fovy = span;
                                camera.projection = CAMERA_ORTHOGRAPHIC;
                                const int width = static_cast<int>(viewport.x);
                                const int height = static_cast<int>(viewport.y);
                                if (!checkTest(
                                        visibleCellsRetained(camera, width, height),
                                        "terrain view rejected a visible roof or crown "
                                        "at a selectable camera angle or viewport"))
                                    return 1;
                            }
                        }
                    }
                }
            }
        }
        Camera3D invalidCamera{};
        const TerrainView unsupported(invalidCamera, 1280, 720);
        if (!checkTest(rejectedCells > 100 && testedUncappedPortraitSpan &&
                           unsupported.containsCell(1000, 1000) &&
                           TerrainView{}.containsCell(1000, 1000),
                       "terrain view did not cull offscreen cells or retain its safe fallback"))
            return 1;
    }

    {
        RenderTexture2D metadata{};
        metadata.id = 1U;
        metadata.texture = {2U, 640, 480, 1,
                            PIXELFORMAT_UNCOMPRESSED_R16G16B16A16};
        metadata.depth = {3U, 640, 480, 1, 0};
        const bool zeroDepthFormatRejected =
            !IsRenderTextureValid(metadata);
        metadata.depth.format = ViewTargets::kRaylibDepthAttachmentFormat;
        if (!checkTest(zeroDepthFormatRejected &&
                           IsRenderTextureValid(metadata),
                       "raylib depth metadata sentinel contract drifted"))
            return 1;
    }

    {
        FakeViewTargetAllocator allocator;
        allocator.hdrResults = {true};
        ViewTargets targets(allocator.operations());
        const bool first = targets.ensure(1, 1280, 720);
        const unsigned int committedId = targets.targets[0].id;
        const bool cached = targets.ensure(1, 1280, 720);
        if (!checkTest(first && cached && targets.count == 1 &&
                           targets.widths == std::array<int, 2>{{1280, 0}} &&
                           targets.height == 720 && allocator.hdrIndex == 1U &&
                           allocator.fallbackIndex == 0U &&
                           allocator.configured ==
                               std::vector<unsigned int>{committedId} &&
                           allocator.unloaded.empty(),
                       "valid HDR view target was not committed and cached"))
            return 1;
        targets.release();
        if (!checkTest(targets.count == 0 && targets.widths[0] == 0 &&
                           targets.height == 0 &&
                           allocator.unloaded ==
                               std::vector<unsigned int>{committedId} &&
                           allocator.live.empty() && !allocator.doubleUnload,
                       "view target release did not clear and unload state"))
            return 1;
    }

    {
        FakeViewTargetAllocator allocator;
        allocator.hdrResults = {false};
        allocator.fallbackResults = {true};
        ViewTargets targets(allocator.operations());
        const bool ready = targets.ensure(1, 960, 540);
        if (!checkTest(ready && targets.count == 1 &&
                           allocator.hdrIndex == 1U &&
                           allocator.fallbackIndex == 1U &&
                           allocator.fallbackWarnings == 1 &&
                           allocator.unloaded.size() == 1U &&
                           allocator.configured ==
                               std::vector<unsigned int>{targets.targets[0].id},
                       "RGBA8 fallback did not replace and release invalid HDR state"))
            return 1;
        targets.release();
    }

    {
        FakeViewTargetAllocator allocator;
        allocator.hdrResults = {true, true};
        ViewTargets targets(allocator.operations());
        if (!checkTest(targets.ensure(1, 640, 480),
                       "successful resize transaction setup failed"))
            return 1;
        const unsigned int originalId = targets.targets[0].id;
        const bool resized = targets.ensure(1, 1024, 768);
        const unsigned int replacementId = targets.targets[0].id;
        const bool replacementCommitted =
            resized && replacementId != originalId && targets.count == 1 &&
            targets.widths[0] == 1024 && targets.height == 768 &&
            allocator.configured ==
                std::vector<unsigned int>{originalId, replacementId} &&
            allocator.unloaded == std::vector<unsigned int>{originalId} &&
            allocator.live == std::vector<unsigned int>{replacementId} &&
            !allocator.doubleUnload;
        targets.release();
        if (!checkTest(replacementCommitted && allocator.live.empty() &&
                           allocator.unloaded ==
                               std::vector<unsigned int>{originalId,
                                                         replacementId} &&
                           !allocator.doubleUnload,
                       "successful resize did not transfer unique target ownership"))
            return 1;
    }

    {
        FakeViewTargetAllocator allocator;
        allocator.hdrResults = {false, true};
        allocator.fallbackResults = {false};
        ViewTargets targets(allocator.operations());
        const bool failed = targets.ensure(1, 800, 600);
        const bool retried = targets.ensure(1, 800, 600);
        if (!checkTest(!failed && retried && targets.count == 1 &&
                           allocator.hdrIndex == 2U &&
                           allocator.fallbackIndex == 1U &&
                           allocator.configured.size() == 1U &&
                           allocator.unloaded.size() == 2U,
                       "double allocation failure was cached instead of retried"))
            return 1;
        targets.release();
    }

    {
        FakeViewTargetAllocator allocator;
        allocator.hdrResults = {true, false};
        allocator.fallbackResults = {false};
        ViewTargets targets(allocator.operations());
        if (!checkTest(targets.ensure(1, 640, 480),
                       "initial transaction setup failed"))
            return 1;
        const unsigned int committedId = targets.targets[0].id;
        const bool resized = targets.ensure(1, 1024, 768);
        if (!checkTest(!resized && targets.count == 1 &&
                           targets.widths[0] == 640 && targets.height == 480 &&
                           targets.targets[0].id == committedId &&
                           std::find(allocator.unloaded.begin(),
                                     allocator.unloaded.end(), committedId) ==
                               allocator.unloaded.end(),
                       "failed resize discarded the last committed target"))
            return 1;
        targets.release();
    }

    {
        FakeViewTargetAllocator allocator;
        allocator.hdrResults = {true, false};
        allocator.fallbackResults = {false};
        ViewTargets targets(allocator.operations());
        const bool twoReady = targets.ensure(2, 1280, 720);
        const std::size_t callsBeforeInvalid = allocator.hdrIndex;
        const bool invalidCount = targets.ensure(3, 1280, 720);
        if (!checkTest(!twoReady && !invalidCount && targets.count == 0 &&
                           targets.targets[0].id == 0U &&
                           targets.targets[1].id == 0U &&
                           allocator.configured.empty() &&
                           allocator.unloaded.size() == 3U &&
                           allocator.hdrIndex == callsBeforeInvalid &&
                           allocator.live.empty() && !allocator.doubleUnload,
                       "partial two-view allocation or invalid count committed state"))
            return 1;
    }

    return 0;
}

int runVehicleMetadataSelfTests()
{
    if (!checkTest(std::string(wwii_tank_model::vehicleName(true, 0)) == "PANZER II AUSF. F" &&
                       std::string(wwii_tank_model::vehicleName(true, 1)) == "SD.KFZ. 231 6-RAD" &&
                       std::string(wwii_tank_model::vehicleName(true, 2)) == "PANZER III AUSF. L" &&
                       std::string(wwii_tank_model::vehicleName(true, 3)) == "TIGER I AUSF. E",
                   "WWII vehicle role mapping is incorrect"))
        return 1;

    static constexpr std::array<std::array<const char *, 4>, 3> expectedVehicles{{
        {{"M24 CHAFFEE", "M4A3(76)W SHERMAN", "M26 PERSHING", "T28/T95"}},
        {{"T-70", "T-34-85", "IS-2", "KV-5 PROJECT"}},
        {{"PANZER II AUSF. F", "PANZER IV AUSF. H", "TIGER I AUSF. E",
          "PANZER VIII MAUS"}}}};
    static constexpr std::array<std::array<wwii_tank_model::Vehicle, 4>, 3>
        expectedVehicleIds{{
            {{wwii_tank_model::Vehicle::M24Chaffee,
              wwii_tank_model::Vehicle::M4A3Sherman,
              wwii_tank_model::Vehicle::M26Pershing,
              wwii_tank_model::Vehicle::T28T95}},
            {{wwii_tank_model::Vehicle::T70,
              wwii_tank_model::Vehicle::T3485,
              wwii_tank_model::Vehicle::IS2,
              wwii_tank_model::Vehicle::KV5Project}},
            {{wwii_tank_model::Vehicle::PanzerIIF,
              wwii_tank_model::Vehicle::PanzerIVH,
              wwii_tank_model::Vehicle::TigerIE,
              wwii_tank_model::Vehicle::Maus}},
        }};
    static constexpr std::array<const char *, 3> expectedNations{{
        "USA", "USSR", "GERMANY"}};
    static constexpr std::array<const char *, 4> expectedTiers{{
        "LIGHT", "MEDIUM", "HEAVY", "SUPER HEAVY"}};
    for (std::size_t nationIndex = 0; nationIndex < kSelectableNations.size(); ++nationIndex)
    {
        const Nation nation = kSelectableNations[nationIndex];
        if (!checkTest(std::string(wwii_tank_model::nationName(nation)) ==
                           expectedNations[nationIndex],
                       "player nation name mapping is incorrect"))
            return 1;
        for (int level = 0; level < 4; ++level)
        {
            if (!checkTest(
                    wwii_tank_model::playerVehicle(nation, level) ==
                        expectedVehicleIds[nationIndex]
                                          [static_cast<std::size_t>(level)],
                    "one of the 12 nation/level vehicle IDs is incorrect"))
                return 1;
            const float muzzleDistance =
                wwii_tank_model::playerMuzzleDistance(nation, level);
            const float muzzleHeight =
                wwii_tank_model::playerMuzzleHeight(nation, level);
            if (!checkTest(
                    std::string(wwii_tank_model::playerVehicleName(nation, level)) ==
                            expectedVehicles[nationIndex][static_cast<std::size_t>(level)] &&
                        std::string(wwii_tank_model::tierName(level)) ==
                            expectedTiers[static_cast<std::size_t>(level)] &&
                        std::isfinite(muzzleDistance) && muzzleDistance > 0.45f &&
                        muzzleDistance < 1.10f && std::isfinite(muzzleHeight) &&
                        muzzleHeight > 0.5f && muzzleHeight < 1.2f,
                    "one of the 12 player vehicles has an invalid name or muzzle"))
                return 1;
        }
        if (!checkTest(
                wwii_tank_model::playerVehicle(nation, -1) ==
                        expectedVehicleIds[nationIndex][0] &&
                    wwii_tank_model::playerVehicle(nation, 4) ==
                        expectedVehicleIds[nationIndex][3],
                "nation vehicle progression no longer clamps to four tiers"))
            return 1;
    }
    if (!checkTest(wwii_tank_model::muzzleDistance(true, 1) < 0.60f &&
                       wwii_tank_model::muzzleDistance(true, 2) > 0.60f &&
                       wwii_tank_model::muzzleDistance(true, 2) < 1.10f,
                   "enemy arcade short-gun muzzle alignment is incorrect"))
        return 1;
    return 0;
}

enum class SelfTestSelection
{
    All,
    Unit,
    Session,
    Assets
};

template <typename Runner>
int runSelectedSelfTestSuite(SelfTestSelection selection,
                             SelfTestSelection category,
                             const char *name, Runner runner)
{
    if (selection != SelfTestSelection::All && selection != category)
        return 0;
    return runSelfTestSuite(name, runner);
}

const char *selfTestSelectionName(SelfTestSelection selection)
{
    switch (selection)
    {
    case SelfTestSelection::Unit: return "unit";
    case SelfTestSelection::Session: return "session";
    case SelfTestSelection::Assets: return "assets";
    case SelfTestSelection::All: return "complete";
    }
    return "unknown";
}

int runSelfTests(const fs::path &resourceRoot,
                 SelfTestSelection selection = SelfTestSelection::All)
{
    gSelfTestReporter.reset();
    if (runSelectedSelfTestSuite(selection, SelfTestSelection::Assets,
                                 "assets-and-audio", [&]() {
            return runAssetsAndAudioSelfTests(resourceRoot);
        }) != 0)
        return 1;
    if (runSelectedSelfTestSuite(selection, SelfTestSelection::Unit,
                                 "audio-output-boundary", [&]() {
            return runAudioOutputBoundarySelfTests(resourceRoot);
        }) != 0)
        return 1;
    if (runSelectedSelfTestSuite(selection, SelfTestSelection::Unit,
                                 "national-visual-contracts", [&]() {
            return runNationalVisualContractSelfTests(resourceRoot);
        }) != 0)
        return 1;
    if (runSelectedSelfTestSuite(selection, SelfTestSelection::Unit,
                                 "stage-and-environment-generation", [&]() {
            return runStageAndEnvironmentSelfTests(resourceRoot);
        }) != 0)
        return 1;
    if (runSelectedSelfTestSuite(selection, SelfTestSelection::Unit,
                                 "terrain-base-and-brick-rules", [&]() {
            return runTerrainBaseAndBrickSelfTests(resourceRoot);
        }) != 0)
        return 1;
    if (runSelectedSelfTestSuite(selection, SelfTestSelection::Unit,
                                 "player-lifecycle-and-bonus-rules", [&]() {
            return runPlayerLifecycleAndBonusSelfTests();
        }) != 0)
        return 1;
    if (runSelectedSelfTestSuite(selection, SelfTestSelection::Unit,
                                 "settings-progression-and-settlement", [&]() {
            return runSettingsProgressionAndSettlementSelfTests(resourceRoot);
        }) != 0)
        return 1;
    if (runSelectedSelfTestSuite(selection, SelfTestSelection::Unit,
                                 "movement-and-enemy-escape", [&]() {
            return runMovementAndEnemyEscapeSelfTests(resourceRoot);
        }) != 0)
        return 1;
    if (runSelectedSelfTestSuite(
            selection, SelfTestSelection::Unit,
            "enemy-production-path-characterization", [&]() {
                return runEnemyProductionPathCharacterizationSelfTests(
                    resourceRoot);
            }) != 0)
        return 1;
    if (runSelectedSelfTestSuite(
            selection, SelfTestSelection::Unit,
            "enemy-lifecycle-production-paths", [&]() {
                return runEnemyLifecycleProductionPathSelfTests(resourceRoot);
            }) != 0)
        return 1;
    if (runSelectedSelfTestSuite(selection, SelfTestSelection::Unit,
                                 "shell-collision-rules", [&]() {
            return runShellCollisionSelfTests(resourceRoot);
        }) != 0)
        return 1;
    if (runSelectedSelfTestSuite(selection, SelfTestSelection::Session,
                                 "seeded-randomness", [&]() {
            return runSeededRandomnessSelfTests(resourceRoot);
        }) != 0)
        return 1;
    if (runSelectedSelfTestSuite(selection, SelfTestSelection::Session,
                                 "random-probability-boundaries", [&]() {
            return runRandomProbabilityBoundarySelfTests(resourceRoot);
        }) != 0)
        return 1;
    if (runSelectedSelfTestSuite(selection, SelfTestSelection::Session,
                                 "scripted-player-input", [&]() {
            return runScriptedPlayerInputSelfTests(resourceRoot);
        }) != 0)
        return 1;
    if (runSelectedSelfTestSuite(selection, SelfTestSelection::Session,
                                 "observable-game-events", [&]() {
            return runObservableGameEventSelfTests(resourceRoot);
        }) != 0)
        return 1;
    if (runSelectedSelfTestSuite(selection, SelfTestSelection::Session,
                                 "session-timing-and-persistence", [&]() {
            return runSessionTimingAndPersistenceSelfTests(resourceRoot);
        }) != 0)
        return 1;
    if (runSelectedSelfTestSuite(selection, SelfTestSelection::Unit,
                                 "view-target-allocation", []() {
            return runViewTargetAllocationSelfTests();
        }) != 0)
        return 1;
    if (runSelectedSelfTestSuite(selection, SelfTestSelection::Unit,
                                 "vehicle-metadata", []() {
            return runVehicleMetadataSelfTests();
        }) != 0)
        return 1;

    gSelfTestReporter.finish();
    if (selection == SelfTestSelection::All)
    {
        std::cout << "Tanks3D self-test passed: classic stage 1 plus 34 deterministic generated stages with validated spawn routes, three national bases, 12 distinct WWII player vehicles, scripted two-player cardinal/ice/fire input, selectable -45 to +45 degree camera rotation and 40 to 70 degree elevation, deterministic session digests and observable rule events, configurable 1-6 HP with 1-HP Bandage disable, advanced +/-30% enemy movement/fire/spawn tuning, strict classic AABBs, cardinal/ice movement with collision-safe local escape, hit-first shell cancellation, 200/490 ms projectile/tank destruction states, death-reset direct-fire streaks and classified K.O. tallies, 20-second shovel steel, 12.5-second 3D/icon bonuses, fixed ten-frame spawn warnings, and all 22 enabled 2D audio cues.\n";
    }
    else
    {
        std::cout << "Tanks3D " << selfTestSelectionName(selection)
                  << " self-test passed.\n";
    }
    return 0;
}

#endif
