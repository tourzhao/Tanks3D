#include "game/settlement_system.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace tanks3d::game
{
int nextSettlementScoreCounter(int current)
{
    const int increment = current < 10       ? 1
                          : current < 100    ? 10
                          : current < 1000   ? 100
                          : current < 10000  ? 1000
                          : current < 100000 ? 10000
                                             : 100000;
    if (current > std::numeric_limits<int>::max() - increment)
        return std::numeric_limits<int>::max();
    return current + increment;
}

SettlementBeginPlan planSettlementBegin(
    int stage, int stageCount, int playerCount, bool gameOver, bool baseAlive,
    bool emitStageEnded, const std::vector<Player> &players)
{
    SettlementBeginPlan plan;
    if (stageCount <= 0 || stage < 1 || stage > stageCount ||
        playerCount < 1 || playerCount > 2 ||
        players.size() != static_cast<std::size_t>(playerCount) ||
        (!gameOver && !baseAlive))
    {
        return plan;
    }

    plan.kind = !gameOver
                    ? SettlementBeginKind::Cleared
                    : !baseAlive
                          ? SettlementBeginKind::BaseDestroyed
                          : SettlementBeginKind::PlayersDefeated;
    plan.start.stage = stage;
    plan.start.playerCount = playerCount;
    plan.start.gameOver = gameOver;
    plan.emitStageEnded = emitStageEnded;
    for (int playerIndex = 0; playerIndex < playerCount; ++playerIndex)
    {
        const std::size_t index = static_cast<std::size_t>(playerIndex);
        plan.start.scores[index] = players[index].score;
        plan.start.tallies[index] = players[index].stageTally;
    }
    return plan;
}

SettlementTransitionPlan planSettlementTransition(
    SettlementCompletion completion, int currentStage, int stageCount,
    int currentHighScore, const std::vector<Player> &players)
{
    SettlementTransitionPlan plan;
    plan.completion = completion;
    plan.stageBefore = currentStage;
    plan.stageAfter = currentStage;
    plan.highScoreBefore = currentHighScore;
    plan.highScoreAfter = currentHighScore;
    plan.playersAfter = players;

    if (completion == SettlementCompletion::None)
    {
        plan.kind = SettlementTransitionKind::None;
        return plan;
    }
    if (completion == SettlementCompletion::GameOver)
    {
        for (const Player &player : players)
            plan.highScoreAfter = std::max(plan.highScoreAfter, player.score);
        plan.kind = plan.highScoreAfter > plan.highScoreBefore
                        ? SettlementTransitionKind::ShowHighScore
                        : SettlementTransitionKind::ReturnToMenu;
        return plan;
    }
    if (completion != SettlementCompletion::AdvanceStage || stageCount <= 0 ||
        currentStage < 1 || currentStage > stageCount)
    {
        plan.playersAfter.clear();
        return plan;
    }

    plan.kind = SettlementTransitionKind::AdvanceStage;
    plan.stageAfter = currentStage == stageCount ? 1 : currentStage + 1;
    for (Player &player : plan.playersAfter)
    {
        if (player.lives <= 0)
        {
            player.lives = 2;
            player.level = 0;
        }
        else
        {
            player.lives = player.lives >= 99 ? 99 : player.lives + 1;
        }
    }
    return plan;
}

void SettlementState::reset(int stage)
{
    *this = SettlementState{};
    stage_ = stage;
}

void SettlementState::begin(const SettlementStart &start)
{
    reset(start.stage);
    if (start.playerCount < 1 || start.playerCount > 2)
        return;
    phase_ = SettlementPhase::Counting;
    gameOver_ = start.gameOver;
    playerCount_ = start.playerCount;

    bool hasDestroyedEnemies = false;
    for (int playerIndex = 0; playerIndex < playerCount_; ++playerIndex)
    {
        const std::size_t index = static_cast<std::size_t>(playerIndex);
        maximumScore_ = std::max(maximumScore_, start.scores[index]);
        tallies_[index] = start.tallies[index];
        hasDestroyedEnemies = hasDestroyedEnemies ||
                              tallies_[index].totalDestroyed() > 0;
    }

    if (!hasDestroyedEnemies)
        categoryIndex_ = kEnemyTypeCount;
    if (maximumScore_ <= 0 && talliesComplete())
        phase_ = SettlementPhase::Idle;
}

SettlementUpdate SettlementState::update(float dt)
{
    SettlementUpdate result;
    if (!std::isfinite(dt) || dt <= 0.0f)
        return result;
    if (phase_ == SettlementPhase::Counting)
    {
        countTimer_ += dt;
        while (countTimer_ >= kSettlementCountStepTime &&
               (scoreCounter_ < maximumScore_ || !talliesComplete()))
        {
            countTimer_ -= kSettlementCountStepTime;
            bool counted = false;
            if (scoreCounter_ < maximumScore_)
            {
                scoreCounter_ = std::min(
                    maximumScore_, nextSettlementScoreCounter(scoreCounter_));
                counted = true;
            }

            while (categoryIndex_ < kEnemyTypeCount)
            {
                bool categoryPending = false;
                for (int playerIndex = 0; playerIndex < playerCount_;
                     ++playerIndex)
                {
                    int &displayed = displayedKills_[
                        static_cast<std::size_t>(playerIndex)][
                        static_cast<std::size_t>(categoryIndex_)];
                    const int target = tallies_[
                        static_cast<std::size_t>(playerIndex)].destroyed[
                        static_cast<std::size_t>(categoryIndex_)];
                    if (displayed < target)
                    {
                        ++displayed;
                        categoryPending = true;
                        counted = true;
                    }
                }
                if (!categoryPending)
                {
                    ++categoryIndex_;
                    continue;
                }

                bool categoryComplete = true;
                for (int playerIndex = 0; playerIndex < playerCount_;
                     ++playerIndex)
                {
                    categoryComplete = categoryComplete &&
                        displayedKills_[
                            static_cast<std::size_t>(playerIndex)][
                            static_cast<std::size_t>(categoryIndex_)] >=
                        tallies_[static_cast<std::size_t>(playerIndex)]
                            .destroyed[static_cast<std::size_t>(categoryIndex_)];
                }
                if (categoryComplete)
                    ++categoryIndex_;
                break;
            }
            if (counted)
                ++result.countedSteps;
        }
        if (scoreCounter_ >= maximumScore_ && talliesComplete())
        {
            phase_ = SettlementPhase::Idle;
            idleTimer_ = 0.0f;
        }
    }
    else if (phase_ == SettlementPhase::Idle)
    {
        idleTimer_ += dt;
        if (idleTimer_ >= kSettlementIdleTime)
            result.completion = complete();
    }
    return result;
}

SettlementCompletion SettlementState::confirm()
{
    if (phase_ == SettlementPhase::Counting)
    {
        scoreCounter_ = maximumScore_;
        for (int playerIndex = 0; playerIndex < playerCount_; ++playerIndex)
        {
            const std::size_t index = static_cast<std::size_t>(playerIndex);
            displayedKills_[index] = tallies_[index].destroyed;
        }
        categoryIndex_ = kEnemyTypeCount;
        phase_ = SettlementPhase::Idle;
        idleTimer_ = 0.0f;
        return SettlementCompletion::None;
    }
    if (phase_ == SettlementPhase::Idle)
        return complete();
    return SettlementCompletion::None;
}

bool SettlementState::active() const
{
    return phase_ != SettlementPhase::None;
}

bool SettlementState::counting() const
{
    return phase_ == SettlementPhase::Counting;
}

SettlementPhase SettlementState::phase() const
{
    return phase_;
}

bool SettlementState::gameOver() const
{
    return gameOver_;
}

int SettlementState::stage() const
{
    return stage_;
}

int SettlementState::scoreCounter() const
{
    return scoreCounter_;
}

int SettlementState::maximumScore() const
{
    return maximumScore_;
}

const StageTally &SettlementState::tally(int playerIndex) const
{
    return tallies_[static_cast<std::size_t>(
        std::clamp(playerIndex, 0, 1))];
}

int SettlementState::displayedKills(int playerIndex, int enemyType) const
{
    if (enemyType < 0 || enemyType >= kEnemyTypeCount)
        return 0;
    return displayedKills_[static_cast<std::size_t>(
        std::clamp(playerIndex, 0, 1))][static_cast<std::size_t>(enemyType)];
}

int SettlementState::categoryIndex() const
{
    return categoryIndex_;
}

float SettlementState::countTimer() const
{
    return countTimer_;
}

float SettlementState::idleTimer() const
{
    return idleTimer_;
}

bool SettlementState::talliesComplete() const
{
    return categoryIndex_ >= kEnemyTypeCount;
}

SettlementCompletion SettlementState::complete()
{
    const SettlementCompletion completion = gameOver_
                                                ? SettlementCompletion::GameOver
                                                : SettlementCompletion::AdvanceStage;
    phase_ = SettlementPhase::None;
    gameOver_ = false;
    countTimer_ = 0.0f;
    idleTimer_ = 0.0f;
    return completion;
}
} // namespace tanks3d::game
