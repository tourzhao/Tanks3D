// The legacy presentation adapter and the engine-independent bridge must apply
// identical controls, AI and session transitions to the shared canonical world.
#define main tanks3dSessionAdapterUnusedApplicationMain
#include "../src/main.cpp"
#undef main
#include "../src/godot/sample_core.cpp"

namespace
{
void requireAdapter(bool condition, const char *message)
{
    if (!condition)
        throw std::runtime_error(message);
}

void verifyPresentationOwnership()
{
    Game3D source("resources", 91U);
    requireAdapter(source.start(1, 3, 1, {{Nation::UnitedStates, Nation::SovietUnion}}),
                   "legacy source could not start");
    Game3DTestAccess::prepareGameEventScenario(source);
    Game3D copied(source);
    Game3D moved(std::move(copied));
    Game3D assigned("resources", 2U);
    assigned = moved;
    Game3D moveAssigned("resources", 3U);
    moveAssigned = std::move(assigned);
    PlayerInputFrame fire;
    fire.players[0].fireHeld = true;
    moveAssigned.update(1.0f / 60.0f, fire);
    requireAdapter(moveAssigned.effects().activeCount() > 0,
                   "moved adapter lost the actual legacy muzzle effect");
    requireAdapter(source.effects().activeCount() == 0 && moved.effects().activeCount() == 0,
                   "copied adapter dispatched effects into another instance");
}

void verifyBothFrontends()
{
    using namespace tanks3d::godot_sample;
    std::unique_ptr<void, decltype(&tanks_sample_destroy)> handle(
        tanks_sample_create("resources"), tanks_sample_destroy);
    requireAdapter(handle != nullptr, "bridge creation failed");
    for (int stage = 1; stage <= kStageCount; ++stage)
    {
        Game3D legacy("resources", 731U);
        requireAdapter(legacy.start(2, 3, stage, {{Nation::UnitedStates, Nation::SovietUnion}}),
                       "legacy frontend could not start");
        requireAdapter(tanks_sample_reset(handle.get(), 731, stage, 2, 1) == 0,
                       "bridge frontend could not start");
        tanks3d::app::AiPlayerController ai;
        unsigned previous = 0;
        for (int tick = 0; tick < 1200; ++tick)
        {
            constexpr unsigned tape[]{17U, 25U, 24U, 16U, 20U, 0U, 18U};
            const unsigned bits = tape[(tick / 19) % 7];
            PlayerInputFrame input;
            input.players[0].north = {bool(bits & 1), bool((bits & 1) && !(previous & 1))};
            input.players[0].south = {bool(bits & 2), bool((bits & 2) && !(previous & 2))};
            input.players[0].west = {bool(bits & 4), bool((bits & 4) && !(previous & 4))};
            input.players[0].east = {bool(bits & 8), bool((bits & 8) && !(previous & 8))};
            input.players[0].fireHeld = bits & 16;
            unsigned ui = 0;
            if ((tick == 300 || tick == 330) && !legacy.endingSequence())
            {
                legacy.togglePause();
                ui = 64;
            }
            if (tick % 120 == 0 && (legacy.settling() || legacy.highScoreDisplay()))
            {
                legacy.confirmSettlement();
                ui = 32;
            }
            const float dt = tick % 3 == 0 ? 0.024f : 1.0f / 60.0f;
            const bool active = !legacy.paused() && !legacy.stageIntro() && !legacy.gameOver() &&
                                !legacy.settling() && !legacy.highScoreDisplay();
            ai.update(dt, active, legacy.stage(), legacy.map(), legacy.players(), legacy.enemies(), input);
            legacy.update(dt, input);
            legacy.consumeMenuRequest();
            requireAdapter(tanks_sample_step(handle.get(), dt, bits | ui, 0) == 0,
                           "bridge rejected legal input");
            if (!(legacy.sessionDigest() == get(handle.get()).game->sessionDigest()))
                throw std::runtime_error("raylib adapter and engine-independent bridge diverged at stage " +
                                         std::to_string(stage) + " tick " + std::to_string(tick));
            const auto rendered = legacy.cameraForPlayer(0);
            const auto neutral = get(handle.get()).game->cameraForPlayer(0);
            requireAdapter(rendered.fovy == neutral.fovy,
                           "neutral camera span differs from original viewport");
            // A snapshot/audio drain must remain an observation, never a tick.
            const auto before = legacy.sessionDigest();
            requireAdapter(tanks_sample_snapshot(handle.get()) && tanks_sample_drain_audio(handle.get()),
                           "bridge observations failed");
            requireAdapter(before == get(handle.get()).game->sessionDigest(),
                           "frontend observation changed the deterministic world");
            previous = bits;
        }
    }
}
} // namespace

int main()
{
    try
    {
        verifyPresentationOwnership();
        verifyBothFrontends();
        std::cout << "PASS canonical session adapters: 35 stages, 42,000 updates, RNG/state/camera parity; "
                     "legacy copy/move effect ownership\n";
    }
    catch (const std::exception &error)
    {
        std::cerr << "FAIL canonical session adapters: " << error.what() << '\n';
        return 1;
    }
}
