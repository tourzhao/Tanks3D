// Review-only probe; no production source or game rules are modified.
#include "../../../src/training/native.cpp"
#include <chrono>

int main()
{
    using Clock = std::chrono::steady_clock;
    using tanks3d::app::AiPlayerController;
    std::vector<double> decisionMicros;
    int rounds = 0, cleared = 0, lost = 0, baseHits = 0, aiShots = 0;
    std::cout << "stage,clock,decisions,shots,base_hits,cleared,lost\n";
    for (int stage = 1; stage <= 35; ++stage)
        for (int jitter = 0; jitter < 2; ++jitter)
        {
            Game3D game("resources", 4600000 + stage);
            if (!game.start(2, 10, stage, {Nation::UnitedStates, Nation::SovietUnion}))
                return 2;
            AiPlayerController controller;
            int localShots = 0, localBaseHits = 0;
            double seconds = 0;
            for (int tick = 0; seconds < 60 && !game.gameOver() && !game.settling(); ++tick)
            {
                constexpr float elapsedPattern[]{1.0f / 120, 1.0f / 40, 1.0f / 60, .05f};
                const float elapsed = jitter ? elapsedPattern[tick % 4] : 1.0f / 60;
                const auto beforeCount = controller.decisions();
                PlayerInputFrame input;
                const auto start = Clock::now();
                controller.update(elapsed, !game.stageIntro(), game.stage(), game.map(),
                                  game.players(), game.enemies(), input);
                const auto stop = Clock::now();
                if (controller.decisions() != beforeCount)
                    decisionMicros.push_back(std::chrono::duration<double, std::micro>(stop - start).count());
                if (input.players[0].fireHeld || input.players[0].north.held ||
                    input.players[0].south.held || input.players[0].west.held || input.players[0].east.held)
                    return 3;
                game.update(elapsed, input);
                seconds += elapsed;
                for (const auto &event : game.eventsThisUpdate())
                {
                    if (event.sourcePlayerId == 1 && event.type == GameEventType::ShellFired)
                        ++localShots;
                    if (event.sourcePlayerId == 1 && event.type == GameEventType::BaseDamaged)
                        ++localBaseHits;
                }
            }
            ++rounds;
            cleared += game.settling() && !game.settlementWasGameOver();
            lost += game.gameOver();
            baseHits += localBaseHits;
            aiShots += localShots;
            std::cout << stage << ',' << jitter << ',' << controller.decisions() << ','
                      << localShots << ',' << localBaseHits << ','
                      << (game.settling() && !game.settlementWasGameOver()) << ',' << game.gameOver() << '\n';
        }
    std::sort(decisionMicros.begin(), decisionMicros.end());
    const auto percentile = [&](double fraction)
    { return decisionMicros[static_cast<std::size_t>((decisionMicros.size() - 1) * fraction)]; };
    std::cout << std::fixed << std::setprecision(3)
              << "SUMMARY rounds=" << rounds << " decisions=" << decisionMicros.size()
              << " shots=" << aiShots << " base_hits=" << baseHits
              << " clear=" << cleared << " lost=" << lost
              << " mean_us=" << std::accumulate(decisionMicros.begin(), decisionMicros.end(), 0.0) / decisionMicros.size()
              << " p50_us=" << percentile(.50) << " p95_us=" << percentile(.95)
              << " p99_us=" << percentile(.99) << " max_us=" << decisionMicros.back() << '\n';
    return baseHits ? 4 : 0;
}
