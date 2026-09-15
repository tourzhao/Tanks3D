#include "../src/training/native.cpp"
#include <iostream>
#include "coop_training_native_tests.inl"

int main()
{
    testCooperativeTraining();
    using namespace tanks3d::training;
    void *handle = t3ai_create("resources");
    if (!handle || t3ai_digest(handle) != 0 || std::string(t3ai_error()).empty())
        return 5;
    t3ai_destroy(handle);
    Environment env("resources");
    std::array<float, kObservationSize> observation{};
    for (int stage = 1; stage <= 35; ++stage)
    {
        env.reset(77, stage, 3, 7200);
        env.observe(observation.data());
        for (int row = 0; row < 26; ++row)
            for (int col = 0; col < 26; ++col)
            {
                const int at = row * 26 + col;
                const auto &map = env.game->map();
                if (observation[4 * 676 + at] != (map.tile(row, col) == '@'))
                    return 1;
                for (int bit = 0; bit < 4; ++bit)
                    if (observation[bit * 676 + at] != ((map.brickMask(row, col) >> bit) & 1))
                        return 2;
            }
    }
    env.reset(731, 1, 3, 7200);
    Game3D canonical("resources", 731U);
    canonical.start(1, 3, 1, {{Nation::UnitedStates, Nation::SovietUnion}});
    while (canonical.stageIntro())
        canonical.update(kStep, {});
    int previous = 0;
    for (int decision = 0; decision < 1600 && !env.done; ++decision)
    {
        const int action = decision < 300 || decision > 600 ? 3 : 7;
        const int before = env.ticks;
        env.step(action);
        for (int frame = 0; frame < env.ticks - before; ++frame)
        {
            PlayerInputFrame input;
            auto &p = input.players[0];
            DirectionButtonFrame *buttons[]{nullptr, &p.north, &p.south, &p.west, &p.east};
            *buttons[action / 2] = {true, frame == 0 && action / 2 != previous};
            p.fireHeld = true;
            canonical.update(kStep, input);
        }
        previous = action / 2;
        if (canonical.sessionDigest().state != env.game->sessionDigest().state)
            return 3;
    }
    if (env.kills < 1 || env.shots < 1)
        return 4;
    std::cout << "PASS all 35 terrain observations, brick quadrants and steel\n"
              << "PASS training adapter matches production state/RNG at " << env.ticks
              << " ticks; kills=" << env.kills << " shots=" << env.shots << "\n";
}
