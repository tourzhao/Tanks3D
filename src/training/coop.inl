// Optional cooperative C ABI. Included after the unchanged single-player API.
// Owns one production world; no alternative movement/combat implementation.
namespace tanks3d::training
{
constexpr int kCoopChannels = 15;
constexpr int kCoopStateSize = 384;
constexpr int kCoopObservationSize = kCoopChannels * 676 + kCoopStateSize;
constexpr int kCoopInfoSize = 40;

struct CoopEnvironment
{
    Environment world;
    std::array<int, 2> previousDirections{};
    // Per slot: kills, destructions, shots, pickups, own-HQ damage events.
    std::array<std::array<int, 5>, 2> counts{};
    // Newest first: four intervals, two slots, dx/2, dz/2, direction/4, fire.
    std::array<std::array<std::array<float, 4>, 2>, 4> history{};

    explicit CoopEnvironment(fs::path root) : world(std::move(root)) {}

    static bool ready(const Player &player)
    {
        return player.active && player.creationTimer <= 0 && player.hitPoints > 0;
    }

    void reset(std::uint32_t seed, int stage, int lives, int limit)
    {
        if (stage < 1 || stage > 35 || lives < 1 || lives > 99 ||
            limit < 3 || limit > 216000)
            throw std::invalid_argument("Invalid cooperative episode settings");
        auto candidate = std::make_unique<Game3D>(world.resources, seed);
        if (!candidate->start(2, lives, stage, {{Nation::UnitedStates, Nation::SovietUnion}}))
            throw std::runtime_error(candidate->lastError());
        for (int i = 0; candidate->stageIntro() && i < 1800; ++i)
            candidate->update(kStep, {});
        if (candidate->stageIntro())
            throw std::runtime_error("Opening timer did not finish");
        world.game = std::move(candidate);
        world.ticks = world.previousDirection = world.kills = world.deaths = 0;
        world.shots = world.baseHits = world.pickups = 0;
        world.maxTicks = limit;
        world.done = world.won = world.truncated = false;
        world.reward = 0;
        previousDirections = {};
        counts = {};
        history = {};
    }

    void account(const GameEvent &event)
    {
        const int source = event.sourcePlayerId, target = event.targetPlayerId;
        const bool playerSource = source >= 0 && source < 2;
        const bool playerTarget = target >= 0 && target < 2;
        if (event.type == GameEventType::ShellFired && playerSource)
        {
            ++world.shots;
            ++counts[source][2];
        }
        if (event.type == GameEventType::TankDestroyed)
        {
            if (event.targetEnemyId >= 0)
            {
                ++world.kills;
                world.reward += 3;
                if (playerSource)
                    ++counts[source][0];
            }
            if (playerTarget)
            {
                ++world.deaths;
                ++counts[target][1];
                world.reward -= 3;
            }
        }
        if (event.type == GameEventType::TankDamaged)
        {
            if (playerTarget)
                world.reward -= 1;
            else if (event.targetEnemyId >= 0)
                world.reward += .25f;
        }
        if (event.type == GameEventType::BonusCollected && playerSource)
        {
            ++world.pickups;
            ++counts[source][3];
            world.reward += 1;
        }
        if (event.type == GameEventType::BaseDamaged && playerSource)
        {
            ++world.baseHits;
            ++counts[source][4];
            world.reward -= 2;
        }
    }

    void step(const std::array<int, 2> &actions)
    {
        if (!world.game || world.done)
            throw std::logic_error("Reset is required before stepping");
        for (int action : actions)
            if (action < 0 || action >= 10)
                throw std::invalid_argument("Cooperative actions must both be in [0,9]");
        const auto before = world.game->players();
        world.reward = 0;
        for (int frame = 0; frame < kRepeat && !world.done; ++frame)
        {
            PlayerInputFrame input;
            for (int slot = 0; slot < 2; ++slot)
            {
                auto &player = input.players[slot];
                const int direction = actions[slot] / 2;
                DirectionButtonFrame *buttons[]{nullptr, &player.north, &player.south,
                                                &player.west, &player.east};
                if (direction)
                    *buttons[direction] = {true, frame == 0 && direction != previousDirections[slot]};
                player.fireHeld = actions[slot] % 2 != 0;
            }
            world.game->update(kStep, input);
            ++world.ticks;
            for (const auto &event : world.game->eventsThisUpdate())
                account(event);
            world.won = world.game->settling() && !world.game->settlementWasGameOver();
            const bool lost = world.game->gameOver();
            world.truncated = !world.won && !lost && world.ticks >= world.maxTicks;
            world.done = world.won || lost || world.truncated;
            if (world.won)
                world.reward += 50;
            if (lost)
                world.reward -= world.game->baseAlive() ? 30 : 50;
        }
        for (int index = 3; index > 0; --index)
            history[index] = history[index - 1];
        for (int slot = 0; slot < 2; ++slot)
        {
            const auto &after = world.game->players()[slot];
            const bool continuous = ready(before[slot]) && ready(after);
            history[0][slot] = {
                continuous ? (after.position.x - before[slot].position.x) / 2 : 0,
                continuous ? (after.position.z - before[slot].position.z) / 2 : 0,
                (actions[slot] / 2) / 4.0f, static_cast<float>(actions[slot] % 2)};
            previousDirections[slot] = actions[slot] / 2;
        }
    }

    void observe(float *out) const
    {
        if (!world.game || !out)
            throw std::logic_error("Missing cooperative episode or output buffer");
        std::array<std::array<float, kObservationSize>, 2> original{};
        for (int slot = 0; slot < 2; ++slot)
            world.observe(original[slot].data(), slot);
        std::fill(out, out + 2 * kCoopObservationSize, 0.0f);
        for (int slot = 0; slot < 2; ++slot)
        {
            float *map = out + slot * kCoopObservationSize;
            float *state = map + kCoopChannels * 676;
            std::copy_n(original[slot].data(), kChannels * 676, map);
            std::copy_n(original[slot].data() + kChannels * 676, kStateSize, state);
            std::copy_n(original[1 - slot].data() + kChannels * 676, 16, state + 256);
            const auto &self = world.game->players()[slot];
            const auto &ally = world.game->players()[1 - slot];
            const int col = static_cast<int>(ally.position.x), row = static_cast<int>(ally.position.z);
            if (col >= 0 && col < 26 && row >= 0 && row < 26 && ally.active)
                map[14 * 676 + row * 26 + col] = 1;
            state[272] = (ally.position.x - self.position.x) / 26;
            state[273] = (ally.position.z - self.position.z) / 26;
            state[274] = static_cast<float>(slot);
            state[275] = 1; // This schema always represents a two-player world.
            std::vector<const Shell *> shells;
            for (const auto &shell : world.game->shells())
                if (!shell.impacting)
                    shells.push_back(&shell);
            std::stable_sort(shells.begin(), shells.end(), [&](const Shell *a, const Shell *b)
            {
                return distanceSquared(a->position, self.position) <
                       distanceSquared(b->position, self.position);
            });
            for (std::size_t i = 0; i < std::min<std::size_t>(24, shells.size()); ++i)
                state[276 + i] = shells[i]->owner == ShellOwner::Enemy ? 0 :
                    (shells[i]->ownerIndex == slot ? 1 : -1);
            for (int interval = 0; interval < 4; ++interval)
                for (int relative = 0; relative < 2; ++relative)
                {
                    const int owner = relative == 0 ? slot : 1 - slot;
                    std::copy(history[interval][owner].begin(), history[interval][owner].end(),
                              state + 300 + interval * 8 + relative * 4);
                }
            state[332] = ready(self);
            state[333] = ready(ally);
            state[334] = !self.active && self.lives <= 0;
            state[335] = !ally.active && ally.lives <= 0;
            state[336] = self.deathTimer;
            state[337] = ally.deathTimer;
            state[338] = self.score / 10000.0f;
            state[339] = ally.score / 10000.0f;
        }
    }

    void info(double *out) const
    {
        if (!world.game || !out)
            throw std::logic_error("Missing cooperative episode or info buffer");
        std::fill(out, out + kCoopInfoSize, 0.0);
        world.info(out);
        out[10] = world.game->players()[0].score + world.game->players()[1].score;
        for (int slot = 0; slot < 2; ++slot)
        {
            const auto &player = world.game->players()[slot];
            for (int i = 0; i < 5; ++i)
                out[14 + slot * 10 + i] = counts[slot][i];
            out[19 + slot * 10] = player.score;
            out[20 + slot * 10] = player.lives;
            out[21 + slot * 10] = player.active;
            out[22 + slot * 10] = ready(player);
            out[23 + slot * 10] = !player.active && player.lives <= 0;
        }
    }
};

CoopEnvironment &getCoop(void *handle)
{
    if (!handle)
        throw std::invalid_argument("Null cooperative environment handle");
    return *static_cast<CoopEnvironment *>(handle);
}
} // namespace tanks3d::training

extern "C"
{
    int t3coop_version() { return 1; }
    int t3coop_observation_size() { return tanks3d::training::kCoopObservationSize; }
    int t3coop_info_size() { return tanks3d::training::kCoopInfoSize; }
    void *t3coop_create(const char *resources)
    {
        void *handle = nullptr;
        tanks3d::training::guarded([&]()
        {
            if (!resources || !fs::is_directory(resources))
                throw std::invalid_argument("Missing resources");
            handle = new tanks3d::training::CoopEnvironment(resources);
        });
        return handle;
    }
    void t3coop_destroy(void *handle)
    {
        delete static_cast<tanks3d::training::CoopEnvironment *>(handle);
    }
    int t3coop_reset(void *handle, std::uint32_t seed, int stage, int lives, int maxTicks,
                     float *observation, double *info)
    {
        return tanks3d::training::guarded([&]()
        {
            if (!observation || !info)
                throw std::invalid_argument("Missing cooperative output buffers");
            auto &e = tanks3d::training::getCoop(handle);
            e.reset(seed, stage, lives, maxTicks);
            e.observe(observation);
            e.info(info);
        });
    }
    int t3coop_step(void *handle, int p1, int p2, float *observation, double *info)
    {
        return tanks3d::training::guarded([&]()
        {
            if (!observation || !info)
                throw std::invalid_argument("Missing cooperative output buffers");
            auto &e = tanks3d::training::getCoop(handle);
            e.step({p1, p2});
            e.observe(observation);
            e.info(info);
        });
    }
    std::uint64_t t3coop_digest(void *handle)
    {
        std::uint64_t result = 0;
        tanks3d::training::guarded([&]()
        {
            auto &e = tanks3d::training::getCoop(handle);
            if (!e.world.game)
                throw std::logic_error("Reset is required before reading a digest");
            result = tanks3d::net::stateHash(e.world.game->sessionDigest().state);
        });
        return result;
    }
    int t3coop_render(void *handle, unsigned char *rgba, int width, int height, int hidden)
    {
        int result = -1;
        const int checked = tanks3d::training::guarded([&]()
        {
            result = t3ai_render(&tanks3d::training::getCoop(handle).world,
                                 rgba, width, height, hidden);
            if (result != 0)
                throw std::runtime_error(tanks3d::training::lastError);
        });
        return checked == 0 ? result : checked;
    }
}
