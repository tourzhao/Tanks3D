void testCooperativeTraining()
{
    using namespace tanks3d::training;
    const auto require = [](bool condition, const char *message)
    {
        if (!condition)
            throw std::runtime_error(message);
    };
    CoopEnvironment env("resources");
    std::vector<float> observations(2 * kCoopObservationSize);
    std::array<double, kCoopInfoSize> info{};
    int checkedTicks = 0;
    for (int stage = 1; stage <= 35; ++stage)
    {
        env.reset(52000U + stage, stage, 3, 7200);
        Game3D canonical("resources", 52000U + stage);
        require(canonical.start(2, 3, stage, {{Nation::UnitedStates, Nation::SovietUnion}}),
                "Cooperative canonical start");
        while (canonical.stageIntro())
            canonical.update(kStep, {});
        std::array<int, 2> previous{};
        for (int decision = 0; decision < 180 && !env.world.done; ++decision)
        {
            const std::array<int, 2> actions{(decision / 13) % 10, (decision / 7 + 3) % 10};
            const int before = env.world.ticks;
            env.step(actions);
            for (int frame = 0; frame < env.world.ticks - before; ++frame)
            {
                PlayerInputFrame input;
                for (int slot = 0; slot < 2; ++slot)
                {
                    auto &p = input.players[slot];
                    const int direction = actions[slot] / 2;
                    DirectionButtonFrame *buttons[]{nullptr, &p.north, &p.south, &p.west, &p.east};
                    if (direction)
                        *buttons[direction] = {true, frame == 0 && direction != previous[slot]};
                    p.fireHeld = actions[slot] % 2 != 0;
                }
                canonical.update(kStep, input);
                ++checkedTicks;
            }
            for (int slot = 0; slot < 2; ++slot)
                previous[slot] = actions[slot] / 2;
            require(canonical.sessionDigest().state == env.world.game->sessionDigest().state,
                    "Cooperative production state or RNG diverged");
            require(canonical.eventsThisUpdate() == env.world.game->eventsThisUpdate(),
                    "Cooperative event order diverged");
            env.observe(observations.data());
            env.info(info.data());
            const auto digest = canonical.sessionDigest().state;
            for (int slot = 0; slot < 2; ++slot)
            {
                const float *state = observations.data() + slot * kCoopObservationSize + kCoopChannels * 676;
                const float *other = observations.data() + (1 - slot) * kCoopObservationSize + kCoopChannels * 676;
                require(std::equal(state + 256, state + 272, other), "Ally/self field mapping");
                require(state[274] == slot && state[332] == CoopEnvironment::ready(canonical.players()[slot]),
                        "Slot identity and actor availability");
                require(state[303] == actions[slot] % 2 && state[307] == actions[1 - slot] % 2,
                        "History must contain executed self/ally commands");
            }
            require(digest == env.world.game->sessionDigest().state, "Observation mutated game");
            require(info[4] == info[14] + info[24] && info[5] == info[15] + info[25],
                    "Team events must be counted once");
        }
    }
    env.reset(13, 1, 3, 3);
    auto digest = env.world.game->sessionDigest().state;
    require(t3coop_step(&env, 1, 10, observations.data(), info.data()) != 0,
            "Invalid second action accepted");
    require(digest == env.world.game->sessionDigest().state && env.world.ticks == 0,
            "Invalid action partially advanced world");
    env.step({0, 0});
    require(env.world.done && env.world.truncated && !env.world.won && env.world.reward == 0,
            "Artificial time limit must not become team defeat");
    require(t3coop_step(&env, 0, 0, observations.data(), info.data()) != 0,
            "Step after terminal accepted");
    env.reset(13, 1, 3, 7200);
    auto &players = const_cast<std::vector<Player> &>(env.world.game->players());
    players[0].active = false;
    players[0].lives = players[0].hitPoints = 0;
    players[0].deathTimer = 0;
    env.step({0, 0});
    require(!env.world.done, "P1 elimination must not terminate P2");
    players[1].active = false;
    players[1].lives = 2;
    players[1].deathTimer = .01f;
    env.step({0, 0});
    require(!env.world.done && players[1].lives == 1 && players[1].active &&
            players[1].creationTimer > 0, "Real P2 respawn progression");
    players[1].active = false;
    players[1].lives = 0;
    players[1].deathTimer = 0;
    env.step({0, 0});
    require(env.world.done && !env.world.truncated && env.world.reward == -30,
            "Both players defeated must terminate once");
    for (int slot = 0; slot < 2; ++slot)
    {
        env.reset(13, 1, 3, 7200);
        GameEvent event;
        event.type = GameEventType::TankDamaged;
        event.targetPlayerId = slot;
        env.account(event);
        require(env.world.reward == -1, "Damage to P2 must not reward enemy damage");
        event.type = GameEventType::TankDestroyed;
        env.account(event);
        event = {};
        event.sourcePlayerId = slot;
        event.targetEnemyId = 23;
        event.type = GameEventType::TankDestroyed;
        env.account(event);
        event.type = GameEventType::BonusCollected;
        env.account(event);
        event.type = GameEventType::BaseDamaged;
        env.account(event);
        require(env.world.reward == -2 && env.world.kills == 1 && env.world.deaths == 1 &&
                env.world.pickups == 1 && env.world.baseHits == 1, "Symmetric event ledger");
    }
    std::cout << "PASS cooperative state/RNG/events on all 35 maps at " << checkedTicks
              << " ticks; observations, joint actions, reward symmetry, P2 respawn and team terminals\n";
}
