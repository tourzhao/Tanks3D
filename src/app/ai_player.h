#ifndef TANKS3D_APP_AI_PLAYER_H
#define TANKS3D_APP_AI_PLAYER_H

#include "game/entities.h"
#include "game/player_system.h"
#include "game/stage_map.h"

#include <array>
#include <cstdint>
#include <utility>
#include <vector>

namespace tanks3d::app
{
// The subset read by TacticalDefender v1, with the same float32 encoding as
// the cooperative observation. No simulator handle, RNG or future intent.
struct AiObservation
{
    static constexpr int kCells = 26 * 26;
    std::array<float, 10 * kCells> terrain{};
    std::array<float, 340> state{};

    float cell(int channel, int row, int column) const
    {
        return terrain[channel * kCells + row * 26 + column];
    }
};

AiObservation observeAiPlayer(const game::StageMap &map,
                              const std::vector<game::Player> &players,
                              const std::vector<game::Enemy> &enemies,
                              int slot);

// Native port of the selected training/rule_policy.py v1 plus the unchanged
// HQ firing guard. Route/history belong to this instance, not the game.
class TacticalAi
{
public:
    void reset();
    int predict(const AiObservation &observation);
    static bool safeFire(double x, double z, int direction);

private:
    using Node = std::pair<int, int>;
    void plan(const AiObservation &observation, const std::vector<int> &enemies,
              bool guardian);
    int chooseAction(const AiObservation &observation);
    std::vector<Node> route_;
    std::uint64_t calls_ = 0;
    std::uint64_t nextPlan_ = 0;
    std::pair<double, double> previousPosition_{};
    bool hasPreviousPosition_ = false;
    int previousAction_ = 0;
    int stalled_ = 0;
    int guardian_ = -1;
};

game::PlayerControlFrame aiActionInput(int action, int previousDirection);

// Holds a 20 Hz decision across the existing elapsed-time game updates. Only
// P2 commands are replaced; pause/intro/report time never advances the policy.
class AiPlayerController
{
public:
    void reset();
    void update(float elapsed, bool battleRunning, int stage,
                const game::StageMap &map,
                const std::vector<game::Player> &players,
                const std::vector<game::Enemy> &enemies,
                game::PlayerInputFrame &input);
    std::uint64_t decisions() const { return decisions_; }

private:
    TacticalAi policy_;
    double untilDecision_ = 0;
    int stage_ = -1;
    int action_ = 0;
    int previousDirection_ = 0;
    std::uint64_t decisions_ = 0;
};
} // namespace tanks3d::app
#endif
