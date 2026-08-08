#ifndef TANKS3D_GAME_SETTLEMENT_SYSTEM_H
#define TANKS3D_GAME_SETTLEMENT_SYSTEM_H

#include "game/entities.h"

#include <array>
#include <vector>

namespace tanks3d::game
{
inline constexpr float kSettlementCountStepTime = 0.1f;
inline constexpr float kSettlementIdleTime = 5.0f;

enum class SettlementPhase : unsigned char
{
    None,
    Counting,
    Idle
};

enum class SettlementCompletion : unsigned char
{
    None,
    AdvanceStage,
    GameOver
};

struct SettlementStart
{
    int stage = 1;
    int playerCount = 1;
    bool gameOver = false;
    std::array<int, 2> scores{};
    std::array<StageTally, 2> tallies{};
};

enum class SettlementBeginKind : unsigned char
{
    Invalid,
    Cleared,
    BaseDestroyed,
    PlayersDefeated
};

// Detached report-entry plan. Concrete StageEnded events and audio silencing
// remain at the Game3D orchestration edge.
struct SettlementBeginPlan
{
    SettlementBeginKind kind = SettlementBeginKind::Invalid;
    SettlementStart start{};
    bool emitStageEnded = false;
};

// Pure report-entry planning. Valid input copies only the configured players'
// score/tally fields. Invalid input returns the default plan without exposing a
// partial snapshot. The result owns its data and retains no player references.
SettlementBeginPlan planSettlementBegin(
    int stage, int stageCount, int playerCount, bool gameOver, bool baseAlive,
    bool emitStageEnded, const std::vector<Player> &players);

struct SettlementUpdate
{
    SettlementCompletion completion = SettlementCompletion::None;
    int countedSteps = 0;
};

enum class SettlementTransitionKind : unsigned char
{
    Invalid,
    None,
    AdvanceStage,
    ShowHighScore,
    ReturnToMenu
};

// Detached completion plan. It changes only the next-stage lives/level fields
// in playersAfter; concrete stage loading, world reset, audio, and navigation
// remain at the Game3D orchestration edge.
struct SettlementTransitionPlan
{
    SettlementTransitionKind kind = SettlementTransitionKind::Invalid;
    SettlementCompletion completion = SettlementCompletion::None;
    int stageBefore = 1;
    int stageAfter = 1;
    int highScoreBefore = 0;
    int highScoreAfter = 0;
    std::vector<Player> playersAfter{};
};

// Pure completion planning. None is a valid no-op. AdvanceStage requires a
// positive stage count and a current stage in [1, stageCount]. GameOver uses a
// strict high-score comparison. Invalid input returns Invalid without exposing
// a partially advanced player snapshot.
SettlementTransitionPlan planSettlementTransition(
    SettlementCompletion completion, int currentStage, int stageCount,
    int currentHighScore, const std::vector<Player> &players);

// Owns the deterministic, presentation-free stage report state machine.
// Game3D remains responsible for events, audio, navigation, and stage loading.
class SettlementState
{
public:
    void reset(int stage = 1);
    // Invalid player counts fail closed to a reset, inactive report.
    void begin(const SettlementStart &start);
    // Non-finite and non-positive elapsed times are ignored.
    SettlementUpdate update(float dt);
    SettlementCompletion confirm();

    bool active() const;
    bool counting() const;
    SettlementPhase phase() const;
    bool gameOver() const;
    int stage() const;
    int scoreCounter() const;
    int maximumScore() const;
    const StageTally &tally(int playerIndex) const;
    int displayedKills(int playerIndex, int enemyType) const;
    int categoryIndex() const;
    float countTimer() const;
    float idleTimer() const;

private:
    bool talliesComplete() const;
    SettlementCompletion complete();

    SettlementPhase phase_ = SettlementPhase::None;
    bool gameOver_ = false;
    int stage_ = 1;
    int playerCount_ = 1;
    int scoreCounter_ = 0;
    int maximumScore_ = 0;
    std::array<StageTally, 2> tallies_{};
    std::array<std::array<int, kEnemyTypeCount>, 2> displayedKills_{};
    int categoryIndex_ = 0;
    float countTimer_ = 0.0f;
    float idleTimer_ = 0.0f;
};

int nextSettlementScoreCounter(int current);
} // namespace tanks3d::game

#endif
