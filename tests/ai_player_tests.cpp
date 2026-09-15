// Same transitional production-world seam as training/LAN integration tests.
#include "../src/training/native.cpp"
#include <stdexcept>

extern "C"
{
void *t3rule_create() { return new tanks3d::app::TacticalAi; }
void t3rule_destroy(void *handle) { delete static_cast<tanks3d::app::TacticalAi *>(handle); }
void t3rule_reset(void *handle) { static_cast<tanks3d::app::TacticalAi *>(handle)->reset(); }
int t3rule_predict(void *rule, void *world, int slot)
{
    const auto &game = *tanks3d::training::getCoop(world).world.game;
    const auto observation = tanks3d::app::observeAiPlayer(game.map(), game.players(), game.enemies(), slot);
    return observation.state[332] > .5f
        ? static_cast<tanks3d::app::TacticalAi *>(rule)->predict(observation) : 0;
}
void t3rule_observe(void *world, int slot, float *terrain, float *state)
{
    const auto &game = *tanks3d::training::getCoop(world).world.game;
    const auto observation = tanks3d::app::observeAiPlayer(game.map(), game.players(), game.enemies(), slot);
    std::copy(observation.terrain.begin(), observation.terrain.end(), terrain);
    std::copy(observation.state.begin(), observation.state.end(), state);
}
}

#ifndef TANKS3D_AI_PLAYER_PROBE_ONLY
namespace
{
void requireAi(bool value, const char *message)
{
    if (!value)
        throw std::runtime_error(message);
}

void testAiMenu()
{
    MenuSettings menu;
    UiInputFrame right, left, one, two;
    right.rightPressed = left.leftPressed = one.selectOnePlayerPressed = two.selectTwoPlayerPressed = true;
    requireAi(menuPlayerModeLabel(menu) == "1 PLAYER", "Default must remain solo");
    updateMenu(menu, right);
    requireAi(menu.playerCount == 2 && !menu.aiPlayer2, "Second mode must remain human co-op");
    updateMenu(menu, right);
    requireAi(menu.playerCount == 2 && menu.aiPlayer2 && menuPlayerModeLabel(menu) == "AI AS P2",
              "AI mode must create the real P2");
    requireAi(menuRowCount(menu) == 7 && lanMenuRow(menu) == 5 && advancedMenuRow(menu) == 6,
              "AI mode must retain P2 nation/LAN/advanced rows");
    menu.selected = 4;
    const auto nation = menu.nations[1];
    updateMenu(menu, right);
    requireAi(menu.nations[1] != nation && menu.aiPlayer2, "AI P2 nation must remain selectable");
    menu.selected = 0;
    updateMenu(menu, right);
    requireAi(menu.playerCount == 1 && !menu.aiPlayer2, "Third mode must wrap to solo");
    updateMenu(menu, left);
    requireAi(menu.aiPlayer2, "Reverse cycle must reach AI");
    menu.selected = advancedMenuRow(menu);
    updateMenu(menu, one);
    requireAi(!menu.aiPlayer2 && menu.playerCount == 1 && menu.selected == advancedMenuRow(menu),
              "1 shortcut must leave AI and preserve advanced selection");
    menu.selected = 0;
    updateMenu(menu, left);
    menu.selected = lanMenuRow(menu);
    updateMenu(menu, two);
    requireAi(!menu.aiPlayer2 && menu.playerCount == 2 && menu.selected == lanMenuRow(menu),
              "2 shortcut must select human co-op and preserve LAN selection");
}

void testAiCommandsAndClock()
{
    using namespace tanks3d::app;
    for (int action = 0; action < 10; ++action)
    {
        const auto edge = aiActionInput(action, 0);
        const auto held = aiActionInput(action, action / 2);
        const int direction = action / 2;
        const DirectionButtonFrame *buttons[]{nullptr, &edge.north, &edge.south, &edge.west, &edge.east};
        const DirectionButtonFrame *repeated[]{nullptr, &held.north, &held.south, &held.west, &held.east};
        for (int d = 1; d <= 4; ++d)
            requireAi(buttons[d]->held == (d == direction) && buttons[d]->pressed == (d == direction) &&
                          repeated[d]->held == (d == direction) && !repeated[d]->pressed,
                      "AI direction edges must match native action repeats");
        requireAi(edge.fireHeld == (action % 2 != 0), "AI fire encoding changed");
    }
    tanks3d::training::CoopEnvironment environment("resources");
    environment.reset(3000000, 1, 3, 7200);
    const auto &game = *environment.world.game;
    auto players = game.players();
    for (auto &player : players)
        player.creationTimer = player.respawnTimer = 0;
    AiPlayerController controller;
    PlayerInputFrame input;
    input.players[0].north = {true, true};
    input.players[0].fireHeld = true;
    for (int tick = 0; tick < 60; ++tick)
    {
        input.players[1].east = {true, true};
        controller.update(1.0f / 60, true, 1, game.map(), players, game.enemies(), input);
        requireAi(input.players[0].north.held && input.players[0].north.pressed &&
                      input.players[0].fireHeld && !input.players[0].east.held,
                  "AI must never consume or replace P1 input");
        requireAi(!input.players[1].east.held, "Human P2 commands must be discarded in AI mode");
    }
    requireAi(controller.decisions() == 20, "AI decision clock must be 20 Hz at 60 Hz game updates");
    for (int rate : {20, 30, 120, 144})
    {
        AiPlayerController clock;
        for (int tick = 0; tick < rate; ++tick)
            clock.update(1.0f / rate, true, 1, game.map(), players, game.enemies(), input);
        requireAi(clock.decisions() == 20, "AI decision rate must not depend on display refresh");
    }
    for (int tick = 0; tick < 60; ++tick)
        controller.update(.05f, false, 1, game.map(), players, game.enemies(), input);
    requireAi(controller.decisions() == 20 && !input.players[1].fireHeld && !input.players[1].north.held,
              "Pause/intro/report must not advance AI or retain commands");
    controller.update(1.0f / 60, true, 1, game.map(), players, game.enemies(), input);
    requireAi(controller.decisions() == 21, "Resume must refresh AI immediately");
    controller.update(1.0f / 60, true, 2, game.map(), players, game.enemies(), input);
    requireAi(controller.decisions() == 1, "Stage change must reset AI history and clock");
    players[1].active = false;
    controller.update(.05f, true, 2, game.map(), players, game.enemies(), input);
    requireAi(controller.decisions() == 1 && !input.players[1].fireHeld, "Dead P2 must be neutral");
    controller.reset();
    requireAi(controller.decisions() == 0, "Restart must clear AI state");
    players[1].active = true;
    controller.update(10, true, 1, game.map(), players, game.enemies(), input);
    requireAi(controller.decisions() == 1, "A frame hitch must not create an unbounded planning backlog");
}

void testAiWithHumanCommands()
{
    using namespace tanks3d::app;
    Game3D game("resources", 3000000);
    requireAi(game.start(2, 10, 1, {Nation::UnitedStates, Nation::SovietUnion}),
              "Human/AI game failed to start");
    AiPlayerController controller;
    const auto initial = game.players();
    std::array<bool, 2> moved{}, fired{};
    for (int tick = 0; tick < 1200; ++tick)
    {
        // Real variable-time update path, with explicit human P1 commands.
        const float elapsed = tick % 2 == 0 ? 1.0f / 120 : 1.0f / 40;
        PlayerInputFrame input;
        input.players[0].north = {true, tick == 0};
        input.players[0].fireHeld = true;
        controller.update(elapsed, !game.stageIntro(), game.stage(), game.map(),
                          game.players(), game.enemies(), input);
        game.update(elapsed, input);
        for (int slot = 0; slot < 2; ++slot)
            moved[slot] = moved[slot] ||
                std::abs(game.players()[slot].position.x - initial[slot].position.x) > 1 ||
                std::abs(game.players()[slot].position.z - initial[slot].position.z) > 1;
        for (const auto &event : game.eventsThisUpdate())
            if (event.type == GameEventType::ShellFired &&
                event.sourcePlayerId >= 0 && event.sourcePlayerId < 2)
                fired[event.sourcePlayerId] = true;
    }
    requireAi(moved[0] && moved[1] && fired[0] && fired[1],
              "Human commands and AI must both move/fire in the real elapsed-time world");
    requireAi(!game.endingSequence(), "Mixed-input smoke ended before pause/restart checks");
    game.togglePause();
    const auto frozen = game.sessionDigest().state;
    const auto decisions = controller.decisions();
    for (int tick = 0; tick < 60; ++tick)
    {
        PlayerInputFrame input;
        controller.update(.05f, !game.paused(), game.stage(), game.map(),
                          game.players(), game.enemies(), input);
        game.update(.05f, input);
    }
    requireAi(game.sessionDigest().state == frozen && controller.decisions() == decisions,
              "Paused real world and AI history must remain frozen");
    game.togglePause();
    PlayerInputFrame input;
    controller.update(.05f, true, game.stage(), game.map(), game.players(), game.enemies(), input);
    game.update(.05f, input);
    requireAi(controller.decisions() == decisions + 1 && game.sessionDigest().state != frozen,
              "Resume must reactivate AI and real simulation");
    requireAi(game.restart(), "AI game restart failed");
    controller.reset();
    controller.update(.05f, !game.stageIntro(), game.stage(), game.map(),
                      game.players(), game.enemies(), input);
    requireAi(game.playerCount() == 2 && game.stageIntro() && controller.decisions() == 0,
              "Restart must keep both players and defer AI until battle starts");
    std::cout << "PASS human P1 + AI P2 movement/fire, variable dt, actual pause/resume/restart\n";
}

void testAiProductionWorld()
{
    using namespace tanks3d::app;
    tanks3d::training::CoopEnvironment environment("resources");
    environment.reset(3000000, 1, 3, 7200);
    auto &game = *environment.world.game;
    std::array<TacticalAi, 2> policies;
    while (!environment.world.done)
    {
        const auto before = game.sessionDigest().state;
        std::array<int, 2> actions{};
        for (int slot = 0; slot < 2; ++slot)
        {
            const auto observation = observeAiPlayer(game.map(), game.players(), game.enemies(), slot);
            if (observation.state[332] > .5f)
                actions[slot] = policies[slot].predict(observation);
        }
        requireAi(before == game.sessionDigest().state, "AI observation/planning mutated gameplay or RNG");
        environment.step(actions);
    }
    requireAi(environment.world.won && environment.world.kills == 20 && environment.world.baseHits == 0,
              "Native AI pair must complete the known Stage 1 episode without self-HQ hits");
    requireAi(game.settling(), "Clear must enter the real battle report");
    for (int i = 0; i < 120 && game.stage() == 1; ++i)
    {
        game.confirmSettlement();
        game.update(.05f, {});
    }
    requireAi(game.stage() == 2 && game.stageIntro(), "AI clear must retain original report/stage progression");
    std::cout << "PASS native AI clear, no gameplay/RNG writes, real report and Stage 2 transition\n";
}
} // namespace

int main()
{
    try
    {
        testAiMenu();
        testAiCommandsAndClock();
        testAiWithHumanCommands();
        testAiProductionWorld();
        std::cout << "PASS AI menu modes/nations/shortcuts, command isolation, 20 Hz clock, pause/resume/restart\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << "AI player test failed: " << error.what() << '\n';
        return 1;
    }
}
#endif
