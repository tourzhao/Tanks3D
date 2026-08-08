#include "game/settlement_system.h"
#include "test_support.h"

#include <cmath>
#include <limits>
#include <string>

namespace
{
using tanks3d::game::SettlementCompletion;
using tanks3d::game::SettlementBeginKind;
using tanks3d::game::SettlementBeginPlan;
using tanks3d::game::SettlementPhase;
using tanks3d::game::SettlementStart;
using tanks3d::game::SettlementState;
using tanks3d::game::SettlementTransitionKind;
using tanks3d::game::SettlementTransitionPlan;
using tanks3d::game::SettlementUpdate;
using tanks3d::game::Player;
using tanks3d::game::StageTally;
using tanks3d::game::kEnemyTypeCount;
using tanks3d::game::kSettlementCountStepTime;
using tanks3d::game::kSettlementIdleTime;
using tanks3d::game::nextSettlementScoreCounter;
using tanks3d::game::planSettlementBegin;
using tanks3d::game::planSettlementTransition;

bool nearlyEqual(float first, float second, float tolerance = 0.0005f)
{
    return std::fabs(first - second) < tolerance;
}

StageTally tallyWithKills(int first, int second, int third, int fourth)
{
    StageTally tally;
    tally.destroyed = {{first, second, third, fourth}};
    return tally;
}

bool samePlayer(const Player &first, const Player &second)
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

bool samePlayers(const std::vector<Player> &first,
                 const std::vector<Player> &second)
{
    if (first.size() != second.size())
        return false;
    for (std::size_t index = 0; index < first.size(); ++index)
    {
        if (!samePlayer(first[index], second[index]))
            return false;
    }
    return true;
}

bool sameTally(const StageTally &first, const StageTally &second)
{
    return first.destroyed == second.destroyed &&
           first.enemyPoints == second.enemyPoints &&
           first.bonusPoints == second.bonusPoints &&
           first.scoreAtStageStart == second.scoreAtStageStart;
}

bool sameStart(const SettlementStart &first, const SettlementStart &second)
{
    return first.stage == second.stage &&
           first.playerCount == second.playerCount &&
           first.gameOver == second.gameOver &&
           first.scores == second.scores &&
           sameTally(first.tallies[0], second.tallies[0]) &&
           sameTally(first.tallies[1], second.tallies[1]);
}

bool defaultInvalidBeginPlan(const SettlementBeginPlan &plan)
{
    return plan.kind == SettlementBeginKind::Invalid &&
           !plan.emitStageEnded && sameStart(plan.start, SettlementStart{});
}

Player detailedPlayer(int id, int lives, int level, int score)
{
    Player player;
    player.id = id;
    player.nation = id == 0 ? tanks3d::game::Nation::UnitedStates
                            : tanks3d::game::Nation::SovietUnion;
    player.position = {3.25f + static_cast<float>(id),
                       8.75f - static_cast<float>(id)};
    player.yaw = 45.0f + 90.0f * static_cast<float>(id);
    player.driveDirection = id == 0
                                ? tanks3d::game::CardinalDirection::East
                                : tanks3d::game::CardinalDirection::West;
    player.movementDirection = id == 0
                                   ? tanks3d::game::CardinalDirection::South
                                   : tanks3d::game::CardinalDirection::North;
    player.lives = lives;
    player.maximumHitPoints = 6;
    player.hitPoints = 2 + id;
    player.level = level;
    player.active = id == 0;
    player.moving = id != 0;
    player.hasBoat = id == 0;
    player.shieldTimer = 1.25f + static_cast<float>(id);
    player.creationTimer = 0.2f + static_cast<float>(id);
    player.respawnTimer = 0.3f + static_cast<float>(id);
    player.deathTimer = 0.4f + static_cast<float>(id);
    player.fireCooldown = 0.5f + static_cast<float>(id);
    player.dustCooldown = 0.6f + static_cast<float>(id);
    player.iceSlipTimer = 0.7f + static_cast<float>(id);
    player.onIce = id != 0;
    player.score = score;
    player.directKillStreak = 4 + id;
    player.streakPopupTimer = 0.8f + static_cast<float>(id);
    player.stageTally.destroyed = {{id + 1, id + 2, id + 3, id + 4}};
    player.stageTally.enemyPoints = {{50, 100, 150, 200}};
    player.stageTally.bonusPoints = 300 + id;
    player.stageTally.scoreAtStageStart = score - 500;
    return player;
}
} // namespace

int main()
{
    tanks3d_test::Reporter reporter;
    reporter.reset();
    bool passed = true;
    const auto expect = [&](bool condition, const std::string &message) {
        if (!reporter.check(condition, message))
            passed = false;
    };

    reporter.beginSuite("settlement-score-counter-and-timing");
    expect(nearlyEqual(kSettlementCountStepTime, 0.1f) &&
               nearlyEqual(kSettlementIdleTime, 5.0f),
           "classic settlement timing constants changed");
    expect(nextSettlementScoreCounter(0) == 1 &&
               nextSettlementScoreCounter(9) == 10,
           "single-point score-counting band changed");
    expect(nextSettlementScoreCounter(10) == 20 &&
               nextSettlementScoreCounter(90) == 100,
           "ten-point score-counting band changed");
    expect(nextSettlementScoreCounter(100) == 200 &&
               nextSettlementScoreCounter(900) == 1000,
           "hundred-point score-counting band changed");
    expect(nextSettlementScoreCounter(1000) == 2000 &&
               nextSettlementScoreCounter(9000) == 10000,
           "thousand-point score-counting band changed");
    expect(nextSettlementScoreCounter(10000) == 20000 &&
               nextSettlementScoreCounter(90000) == 100000,
           "ten-thousand-point score-counting band changed");
    expect(nextSettlementScoreCounter(100000) == 200000,
           "hundred-thousand-point score-counting band changed");
    const int maximumInt = std::numeric_limits<int>::max();
    expect(nextSettlementScoreCounter(maximumInt - 100000) == maximumInt &&
               nextSettlementScoreCounter(maximumInt - 99999) == maximumInt &&
               nextSettlementScoreCounter(maximumInt) == maximumInt,
           "score counter did not saturate safely at INT_MAX");

    reporter.beginSuite("settlement-begin-cleared-showcase-and-detachment");
    std::vector<Player> clearedPlayers{{detailedPlayer(0, 3, 2, 2350)}};
    const std::vector<Player> clearedPlayersBefore = clearedPlayers;
    SettlementBeginPlan clearedPlan = planSettlementBegin(
        7, 35, 1, false, true, true, clearedPlayers);
    expect(clearedPlan.kind == SettlementBeginKind::Cleared &&
               clearedPlan.emitStageEnded && clearedPlan.start.stage == 7 &&
               clearedPlan.start.playerCount == 1 &&
               !clearedPlan.start.gameOver,
           "cleared report plan lost its reason, stage, or event intent");
    expect(clearedPlan.start.scores[0] == 2350 &&
               clearedPlan.start.scores[1] == 0 &&
               sameTally(clearedPlan.start.tallies[0],
                         clearedPlayers[0].stageTally) &&
               sameTally(clearedPlan.start.tallies[1], StageTally{}),
           "one-player report copied the wrong score/tally scope");
    expect(samePlayers(clearedPlayers, clearedPlayersBefore),
           "cleared report planning changed complete player input");

    const SettlementBeginPlan showcasePlan = planSettlementBegin(
        7, 35, 1, false, true, false, clearedPlayers);
    expect(showcasePlan.kind == SettlementBeginKind::Cleared &&
               !showcasePlan.emitStageEnded &&
               sameStart(showcasePlan.start, clearedPlan.start),
           "showcase planning changed the report instead of only suppressing its event");

    const SettlementStart detachedStart = clearedPlan.start;
    clearedPlayers[0].score = 999999;
    clearedPlayers[0].stageTally.destroyed[0] = 99;
    expect(sameStart(clearedPlan.start, detachedStart),
           "begin plan retained references to live player state");
    clearedPlan.start.scores[0] = -1;
    clearedPlan.start.tallies[0].bonusPoints = -1;
    expect(clearedPlayers[0].score == 999999 &&
               clearedPlayers[0].stageTally.destroyed[0] == 99 &&
               clearedPlayers[0].stageTally.bonusPoints ==
                   clearedPlayersBefore[0].stageTally.bonusPoints,
           "mutating the detached begin plan changed live player state");

    reporter.beginSuite("settlement-begin-game-over-reasons-and-extremes");
    std::vector<Player> gameOverPlayers{{
        detailedPlayer(0, 0, 3, std::numeric_limits<int>::max()),
        detailedPlayer(1, 0, 1, std::numeric_limits<int>::min() + 500)}};
    gameOverPlayers[1].score = std::numeric_limits<int>::min();
    const std::vector<Player> gameOverPlayersBefore = gameOverPlayers;
    const SettlementBeginPlan baseDestroyedPlan = planSettlementBegin(
        35, 35, 2, true, false, true, gameOverPlayers);
    expect(baseDestroyedPlan.kind == SettlementBeginKind::BaseDestroyed &&
               baseDestroyedPlan.emitStageEnded &&
               baseDestroyedPlan.start.stage == 35 &&
               baseDestroyedPlan.start.playerCount == 2 &&
               baseDestroyedPlan.start.gameOver,
           "base-destroyed report selected the wrong reason or metadata");
    expect(baseDestroyedPlan.start.scores[0] ==
                   std::numeric_limits<int>::max() &&
               baseDestroyedPlan.start.scores[1] ==
                   std::numeric_limits<int>::min() &&
               sameTally(baseDestroyedPlan.start.tallies[0],
                         gameOverPlayers[0].stageTally) &&
               sameTally(baseDestroyedPlan.start.tallies[1],
                         gameOverPlayers[1].stageTally),
           "two-player game-over report lost extreme scores or exact tallies");

    const SettlementBeginPlan playersDefeatedPlan = planSettlementBegin(
        std::numeric_limits<int>::max(), std::numeric_limits<int>::max(), 2,
        true, true, false, gameOverPlayers);
    expect(playersDefeatedPlan.kind == SettlementBeginKind::PlayersDefeated &&
               !playersDefeatedPlan.emitStageEnded &&
               playersDefeatedPlan.start.stage ==
                   std::numeric_limits<int>::max() &&
               playersDefeatedPlan.start.gameOver &&
               playersDefeatedPlan.start.scores ==
                   baseDestroyedPlan.start.scores,
           "players-defeated report or valid INT_MAX stage was rejected");
    expect(sameTally(playersDefeatedPlan.start.tallies[0],
                     baseDestroyedPlan.start.tallies[0]) &&
               sameTally(playersDefeatedPlan.start.tallies[1],
                         baseDestroyedPlan.start.tallies[1]) &&
               samePlayers(gameOverPlayers, gameOverPlayersBefore),
           "game-over reason planning changed tallies or complete player input");

    reporter.beginSuite("settlement-begin-invalid-input-is-atomic");
    const std::vector<std::array<int, 2>> invalidStageBounds{{
        {{1, 0}},
        {{0, 35}},
        {{36, 35}},
        {{std::numeric_limits<int>::min(),
          std::numeric_limits<int>::max()}},
        {{std::numeric_limits<int>::max(), 35}},
        {{1, std::numeric_limits<int>::min()}}}};
    for (const std::array<int, 2> &bounds : invalidStageBounds)
    {
        const SettlementBeginPlan invalidPlan = planSettlementBegin(
            bounds[0], bounds[1], 2, true, false, true,
            gameOverPlayers);
        expect(defaultInvalidBeginPlan(invalidPlan),
               "invalid stage bounds exposed a partial report snapshot");
    }

    for (int invalidPlayerCount :
         {std::numeric_limits<int>::min(), 0, 3,
          std::numeric_limits<int>::max()})
    {
        const SettlementBeginPlan invalidPlan = planSettlementBegin(
            1, 35, invalidPlayerCount, true, false, true,
            gameOverPlayers);
        expect(defaultInvalidBeginPlan(invalidPlan),
               "invalid player count exposed a partial report snapshot");
    }

    const std::vector<Player> noPlayers;
    const std::vector<Player> onePlayer{{detailedPlayer(0, 1, 0, 50)}};
    for (const SettlementBeginPlan &mismatchedPlan : {
             planSettlementBegin(1, 35, 1, true, false, true, noPlayers),
             planSettlementBegin(1, 35, 1, true, false, true,
                                 gameOverPlayers),
             planSettlementBegin(1, 35, 2, true, false, true, onePlayer)})
    {
        expect(defaultInvalidBeginPlan(mismatchedPlan),
               "player-vector mismatch exposed a partial report snapshot");
    }

    const SettlementBeginPlan contradictoryPlan = planSettlementBegin(
        1, 35, 2, false, false, true, gameOverPlayers);
    expect(defaultInvalidBeginPlan(contradictoryPlan) &&
               samePlayers(gameOverPlayers, gameOverPlayersBefore),
           "cleared report with a destroyed base did not fail atomically");

    const std::vector<Player> transitionPlayers{{
        detailedPlayer(0, 3, 2, 2100),
        detailedPlayer(1, 0, 3, 2600)}};
    const std::vector<Player> transitionPlayersBefore = transitionPlayers;

    reporter.beginSuite("settlement-transition-none-and-invalid-input");
    const SettlementTransitionPlan noTransition = planSettlementTransition(
        SettlementCompletion::None, 7, 35, 2000, transitionPlayers);
    expect(noTransition.kind == SettlementTransitionKind::None &&
               noTransition.completion == SettlementCompletion::None &&
               noTransition.stageBefore == 7 && noTransition.stageAfter == 7 &&
               noTransition.highScoreBefore == 2000 &&
               noTransition.highScoreAfter == 2000 &&
               samePlayers(noTransition.playersAfter, transitionPlayers) &&
               samePlayers(transitionPlayers, transitionPlayersBefore),
           "no-op completion changed its detached or input player snapshot");

    const SettlementCompletion invalidCompletion =
        static_cast<SettlementCompletion>(255);
    const SettlementTransitionPlan invalidKind = planSettlementTransition(
        invalidCompletion, 7, 35, 2000, transitionPlayers);
    expect(invalidKind.kind == SettlementTransitionKind::Invalid &&
               invalidKind.completion == invalidCompletion &&
               invalidKind.stageBefore == 7 && invalidKind.stageAfter == 7 &&
               invalidKind.highScoreBefore == 2000 &&
               invalidKind.highScoreAfter == 2000 &&
               invalidKind.playersAfter.empty() &&
               samePlayers(transitionPlayers, transitionPlayersBefore),
           "unknown completion exposed partial progression or changed input");

    for (const std::array<int, 2> invalidStage :
         {std::array<int, 2>{{1, 0}}, std::array<int, 2>{{0, 35}},
          std::array<int, 2>{{36, 35}}})
    {
        const SettlementTransitionPlan invalidStagePlan =
            planSettlementTransition(
                SettlementCompletion::AdvanceStage, invalidStage[0],
                invalidStage[1], 2000, transitionPlayers);
        expect(invalidStagePlan.kind == SettlementTransitionKind::Invalid &&
                   invalidStagePlan.stageBefore == invalidStage[0] &&
                   invalidStagePlan.stageAfter == invalidStage[0] &&
                   invalidStagePlan.highScoreBefore == 2000 &&
                   invalidStagePlan.highScoreAfter == 2000 &&
                   invalidStagePlan.playersAfter.empty() &&
                   samePlayers(transitionPlayers, transitionPlayersBefore),
               "invalid stage bounds exposed partial progression or changed input");
    }

    reporter.beginSuite("settlement-transition-game-over-high-score");
    const SettlementTransitionPlan twoPlayerRecord = planSettlementTransition(
        SettlementCompletion::GameOver, 7, 35, 2000, transitionPlayers);
    expect(twoPlayerRecord.kind == SettlementTransitionKind::ShowHighScore &&
               twoPlayerRecord.completion == SettlementCompletion::GameOver &&
               twoPlayerRecord.stageBefore == 7 &&
               twoPlayerRecord.stageAfter == 7 &&
               twoPlayerRecord.highScoreBefore == 2000 &&
               twoPlayerRecord.highScoreAfter == 2600 &&
               samePlayers(twoPlayerRecord.playersAfter, transitionPlayers) &&
               samePlayers(transitionPlayers, transitionPlayersBefore),
           "game over did not select player two's strict high score atomically");

    std::vector<Player> tiedPlayers = transitionPlayers;
    tiedPlayers[0].score = 2599;
    tiedPlayers[1].score = 2600;
    const std::vector<Player> tiedPlayersBefore = tiedPlayers;
    const SettlementTransitionPlan tiedScore = planSettlementTransition(
        SettlementCompletion::GameOver, 35, 35, 2600, tiedPlayers);
    expect(tiedScore.kind == SettlementTransitionKind::ReturnToMenu &&
               tiedScore.highScoreBefore == 2600 &&
               tiedScore.highScoreAfter == 2600 &&
               tiedScore.stageBefore == 35 && tiedScore.stageAfter == 35 &&
               samePlayers(tiedScore.playersAfter, tiedPlayers) &&
               samePlayers(tiedPlayers, tiedPlayersBefore),
           "equal score incorrectly beat the record or changed player input");

    std::vector<Player> maximumScorePlayers = transitionPlayers;
    maximumScorePlayers[0].score = maximumInt;
    maximumScorePlayers[1].score = maximumInt - 1;
    const std::vector<Player> maximumScorePlayersBefore = maximumScorePlayers;
    const SettlementTransitionPlan maximumRecord = planSettlementTransition(
        SettlementCompletion::GameOver, 12, 35, maximumInt - 1,
        maximumScorePlayers);
    const SettlementTransitionPlan maximumTie = planSettlementTransition(
        SettlementCompletion::GameOver, 12, 35, maximumInt,
        maximumScorePlayers);
    expect(maximumRecord.kind == SettlementTransitionKind::ShowHighScore &&
               maximumRecord.highScoreAfter == maximumInt &&
               maximumTie.kind == SettlementTransitionKind::ReturnToMenu &&
               maximumTie.highScoreAfter == maximumInt &&
               samePlayers(maximumRecord.playersAfter, maximumScorePlayers) &&
               samePlayers(maximumTie.playersAfter, maximumScorePlayers) &&
               samePlayers(maximumScorePlayers, maximumScorePlayersBefore),
           "INT_MAX high-score comparison overflowed or mutated players");

    reporter.beginSuite("settlement-transition-stage-progression");
    const SettlementTransitionPlan ordinaryAdvance = planSettlementTransition(
        SettlementCompletion::AdvanceStage, 7, 35, 2000,
        transitionPlayers);
    std::vector<Player> expectedOrdinaryPlayers = transitionPlayers;
    expectedOrdinaryPlayers[0].lives = 4;
    expectedOrdinaryPlayers[1].lives = 2;
    expectedOrdinaryPlayers[1].level = 0;
    expect(ordinaryAdvance.kind == SettlementTransitionKind::AdvanceStage &&
               ordinaryAdvance.completion ==
                   SettlementCompletion::AdvanceStage &&
               ordinaryAdvance.stageBefore == 7 &&
               ordinaryAdvance.stageAfter == 8 &&
               ordinaryAdvance.highScoreBefore == 2000 &&
               ordinaryAdvance.highScoreAfter == 2000 &&
               samePlayers(ordinaryAdvance.playersAfter,
                           expectedOrdinaryPlayers) &&
               samePlayers(transitionPlayers, transitionPlayersBefore),
           "ordinary advance changed fields beyond lives/level or mutated input");

    std::vector<Player> finalStagePlayers{{
        detailedPlayer(0, 99, 3, 3100),
        detailedPlayer(1, 1, 1, 1700)}};
    const std::vector<Player> finalStagePlayersBefore = finalStagePlayers;
    const SettlementTransitionPlan wrappedAdvance = planSettlementTransition(
        SettlementCompletion::AdvanceStage, 35, 35, 2600,
        finalStagePlayers);
    std::vector<Player> expectedWrappedPlayers = finalStagePlayers;
    expectedWrappedPlayers[0].lives = 99;
    expectedWrappedPlayers[1].lives = 2;
    expect(wrappedAdvance.kind == SettlementTransitionKind::AdvanceStage &&
               wrappedAdvance.stageBefore == 35 &&
               wrappedAdvance.stageAfter == 1 &&
               wrappedAdvance.highScoreBefore == 2600 &&
               wrappedAdvance.highScoreAfter == 2600 &&
               samePlayers(wrappedAdvance.playersAfter,
                           expectedWrappedPlayers) &&
               samePlayers(finalStagePlayers, finalStagePlayersBefore),
           "final-stage wrap, survivor life cap, or detached copy changed");

    reporter.beginSuite("settlement-default-reset-and-accessors");
    SettlementState state;
    expect(!state.active() && !state.counting() && !state.gameOver() &&
               state.phase() == SettlementPhase::None && state.stage() == 1,
           "default settlement state is not inactive at stage one");
    expect(state.scoreCounter() == 0 && state.maximumScore() == 0 &&
               state.categoryIndex() == 0 && nearlyEqual(state.countTimer(), 0.0f) &&
               nearlyEqual(state.idleTimer(), 0.0f),
           "default settlement counters or timers are not clear");
    expect(state.tally(0).totalDestroyed() == 0 &&
               state.displayedKills(0, 0) == 0 &&
               state.displayedKills(0, -1) == 0 &&
               state.displayedKills(0, kEnemyTypeCount) == 0,
           "default or invalid-index settlement queries returned data");

    SettlementStart dirtyStart;
    dirtyStart.stage = 9;
    dirtyStart.gameOver = true;
    dirtyStart.scores[0] = 250;
    dirtyStart.tallies[0] = tallyWithKills(1, 0, 0, 0);
    state.begin(dirtyStart);
    state.update(kSettlementCountStepTime);
    state.reset(35);
    expect(!state.active() && !state.gameOver() && state.stage() == 35 &&
               state.phase() == SettlementPhase::None,
           "reset did not restore the inactive runtime state or requested stage");
    expect(state.scoreCounter() == 0 && state.maximumScore() == 0 &&
               state.categoryIndex() == 0 &&
               state.tally(0).totalDestroyed() == 0 &&
               state.displayedKills(0, 0) == 0 &&
               nearlyEqual(state.countTimer(), 0.0f) &&
               nearlyEqual(state.idleTimer(), 0.0f),
           "reset retained report counters, tallies, or timers");
    const SettlementUpdate inactiveUpdate = state.update(100.0f);
    expect(state.confirm() == SettlementCompletion::None &&
               inactiveUpdate.completion == SettlementCompletion::None &&
               inactiveUpdate.countedSteps == 0 && !state.active(),
           "inactive confirm or update produced a transition");
    for (int invalidPlayerCount : {0, 3, std::numeric_limits<int>::max()})
    {
        dirtyStart.playerCount = invalidPlayerCount;
        state.begin(dirtyStart);
        expect(!state.active() && !state.gameOver() && state.stage() == 9 &&
                   state.scoreCounter() == 0 &&
                   state.tally(0).totalDestroyed() == 0,
               "invalid player count did not fail closed: " +
                   std::to_string(invalidPlayerCount));
    }

    reporter.beginSuite("settlement-begin-snapshot-and-player-scope");
    SettlementStart snapshotStart;
    snapshotStart.stage = 7;
    snapshotStart.playerCount = 2;
    snapshotStart.gameOver = true;
    snapshotStart.scores = {{350, 725}};
    snapshotStart.tallies[0] = tallyWithKills(2, 0, 1, 0);
    snapshotStart.tallies[1] = tallyWithKills(1, 0, 2, 1);
    snapshotStart.tallies[0].bonusPoints = 300;
    snapshotStart.tallies[1].enemyPoints[3] = 200;
    state.begin(snapshotStart);
    expect(state.active() && state.counting() && state.gameOver() &&
               state.phase() == SettlementPhase::Counting && state.stage() == 7,
           "begin did not enter the requested game-over report");
    expect(state.maximumScore() == 725 && state.scoreCounter() == 0 &&
               state.tally(0).destroyed[2] == 1 &&
               state.tally(0).bonusPoints == 300 &&
               state.tally(1).enemyPoints[3] == 200,
           "begin did not snapshot scores and per-player tallies");
    expect(state.categoryIndex() == 0 && state.displayedKills(0, 0) == 0 &&
               state.displayedKills(1, 3) == 0 &&
               nearlyEqual(state.countTimer(), 0.0f) &&
               nearlyEqual(state.idleTimer(), 0.0f),
           "begin did not clear visible report progress");
    snapshotStart.scores[1] = 1;
    snapshotStart.tallies[0].destroyed[2] = 99;
    snapshotStart.tallies[1].enemyPoints[3] = 0;
    expect(state.maximumScore() == 725 && state.tally(0).destroyed[2] == 1 &&
               state.tally(1).enemyPoints[3] == 200,
           "settlement report retained references to its input snapshot");
    expect(state.tally(-1).bonusPoints == 300 &&
               state.tally(2).enemyPoints[3] == 200,
           "tally player-index clamping changed");
    expect(state.displayedKills(-1, 0) == state.displayedKills(0, 0) &&
               state.displayedKills(2, 3) == state.displayedKills(1, 3) &&
               state.displayedKills(0, -1) == 0 &&
               state.displayedKills(0, kEnemyTypeCount) == 0,
           "displayed-kill index validation changed");

    SettlementStart singlePlayerStart;
    singlePlayerStart.stage = 4;
    singlePlayerStart.playerCount = 1;
    singlePlayerStart.scores = {{50, 999999}};
    singlePlayerStart.tallies[1] = tallyWithKills(9, 9, 9, 9);
    state.begin(singlePlayerStart);
    expect(state.maximumScore() == 50 && state.categoryIndex() == kEnemyTypeCount &&
               state.tally(1).totalDestroyed() == 0 && state.counting(),
           "one-player report consumed inactive player-two data");

    SettlementStart emptyStart;
    emptyStart.stage = 12;
    emptyStart.playerCount = 2;
    state.begin(emptyStart);
    expect(state.active() && !state.counting() &&
               state.phase() == SettlementPhase::Idle &&
               state.maximumScore() == 0 &&
               state.categoryIndex() == kEnemyTypeCount,
           "empty report did not begin directly in the idle phase");

    reporter.beginSuite("settlement-count-boundary-and-category-order");
    SettlementStart orderedStart;
    orderedStart.stage = 3;
    orderedStart.playerCount = 2;
    orderedStart.scores = {{50, 20}};
    orderedStart.tallies[0] = tallyWithKills(2, 0, 1, 0);
    orderedStart.tallies[1] = tallyWithKills(1, 0, 2, 1);
    state.begin(orderedStart);
    const SettlementUpdate halfStep = state.update(0.05f);
    expect(halfStep.countedSteps == 0 && state.scoreCounter() == 0 &&
               state.displayedKills(0, 0) == 0 && state.categoryIndex() == 0 &&
               nearlyEqual(state.countTimer(), 0.05f),
           "counting advanced before the 0.1-second boundary");
    const SettlementUpdate firstStep = state.update(0.05f);
    expect(firstStep.countedSteps == 1 && state.scoreCounter() == 1 &&
               state.displayedKills(0, 0) == 1 &&
               state.displayedKills(1, 0) == 1 && state.categoryIndex() == 0 &&
               nearlyEqual(state.countTimer(), 0.0f),
           "first score/K.O. step did not occur at 0.1 seconds");
    const SettlementUpdate secondStep = state.update(0.1f);
    expect(secondStep.countedSteps == 1 && state.scoreCounter() == 2 &&
               state.displayedKills(0, 0) == 2 &&
               state.displayedKills(1, 0) == 1 && state.categoryIndex() == 1,
           "first K.O. category did not wait for both players");
    const SettlementUpdate thirdStep = state.update(0.1f);
    expect(thirdStep.countedSteps == 1 && state.scoreCounter() == 3 &&
               state.displayedKills(0, 2) == 1 &&
               state.displayedKills(1, 2) == 1 && state.categoryIndex() == 2,
           "empty K.O. category was not skipped in display order");
    const SettlementUpdate fourthStep = state.update(0.1f);
    expect(fourthStep.countedSteps == 1 && state.scoreCounter() == 4 &&
               state.displayedKills(0, 2) == 1 &&
               state.displayedKills(1, 2) == 2 && state.categoryIndex() == 3,
           "asymmetric K.O. totals did not complete in lockstep");
    const SettlementUpdate fifthStep = state.update(0.1f);
    expect(fifthStep.countedSteps == 1 && state.scoreCounter() == 5 &&
               state.displayedKills(1, 3) == 1 &&
               state.categoryIndex() == kEnemyTypeCount && state.counting(),
           "last K.O. category or remaining score work ended early");

    reporter.beginSuite("settlement-large-dt-and-cue-steps");
    SettlementStart catchupStart;
    catchupStart.stage = 6;
    catchupStart.scores[0] = 25;
    catchupStart.tallies[0] = tallyWithKills(2, 0, 0, 0);
    state.begin(catchupStart);
    const SettlementUpdate catchup = state.update(0.35f);
    expect(catchup.countedSteps == 3 &&
               catchup.completion == SettlementCompletion::None &&
               state.scoreCounter() == 3,
           "large counting update did not report one cue step per cadence");
    expect(state.displayedKills(0, 0) == 2 &&
               state.categoryIndex() == kEnemyTypeCount && state.counting() &&
               nearlyEqual(state.countTimer(), 0.05f, 0.001f),
           "large counting update lost K.O. order or fractional time");
    const SettlementUpdate finishCatchup = state.update(100.0f);
    expect(finishCatchup.countedSteps == 9 &&
               finishCatchup.completion == SettlementCompletion::None &&
               state.scoreCounter() == 25 &&
               state.phase() == SettlementPhase::Idle,
           "catch-up update did not finish all remaining score bands");
    expect(nearlyEqual(state.idleTimer(), 0.0f) && state.countTimer() > 99.0f,
           "counting overflow time incorrectly consumed the idle hold");

    SettlementStart scoreOnlyStart;
    scoreOnlyStart.scores[0] = 25;
    state.begin(scoreOnlyStart);
    const SettlementUpdate nanUpdate = state.update(
        std::numeric_limits<float>::quiet_NaN());
    const SettlementUpdate infinityUpdate = state.update(
        std::numeric_limits<float>::infinity());
    const SettlementUpdate negativeUpdate = state.update(-1.0f);
    expect(nanUpdate.countedSteps == 0 &&
               infinityUpdate.countedSteps == 0 &&
               negativeUpdate.countedSteps == 0 &&
               state.scoreCounter() == 0 && nearlyEqual(state.countTimer(), 0.0f),
           "invalid elapsed time polluted settlement progress");
    const SettlementUpdate scoreOnly = state.update(0.3f);
    expect(scoreOnly.countedSteps == 3 && state.scoreCounter() == 3 &&
               state.categoryIndex() == kEnemyTypeCount &&
               state.tally(0).totalDestroyed() == 0,
           "score-only report did not produce cadence steps");

    SettlementStart tallyOnlyStart;
    tallyOnlyStart.scores[0] = 0;
    tallyOnlyStart.tallies[0] = tallyWithKills(2, 0, 0, 0);
    state.begin(tallyOnlyStart);
    const SettlementUpdate tallyOnly = state.update(0.31f);
    expect(tallyOnly.countedSteps == 2 && state.scoreCounter() == 0 &&
               state.displayedKills(0, 0) == 2 &&
               state.phase() == SettlementPhase::Idle,
           "K.O.-only report did not count or enter idle");

    SettlementStart scoreFinishesFirstStart;
    scoreFinishesFirstStart.scores[0] = 1;
    scoreFinishesFirstStart.tallies[0] = tallyWithKills(2, 0, 0, 0);
    state.begin(scoreFinishesFirstStart);
    const SettlementUpdate scoreFinishesFirst = state.update(0.1f);
    expect(scoreFinishesFirst.countedSteps == 1 && state.scoreCounter() == 1 &&
               state.displayedKills(0, 0) == 1 && state.counting() &&
               state.categoryIndex() == 0,
           "report entered idle while classified K.O. work remained");

    reporter.beginSuite("settlement-int-max-saturation");
    SettlementStart maximumStart;
    maximumStart.stage = 15;
    maximumStart.scores[0] = maximumInt;
    state.begin(maximumStart);
    const SettlementUpdate maximumUpdate = state.update(2200.0f);
    expect(maximumUpdate.countedSteps == 21520,
           "INT_MAX score used the wrong number of cadence steps");
    expect(maximumUpdate.completion == SettlementCompletion::None &&
               state.scoreCounter() == maximumInt &&
               state.maximumScore() == maximumInt &&
               state.phase() == SettlementPhase::Idle,
           "INT_MAX score did not saturate and complete counting");
    expect(state.scoreCounter() >= 0 && state.countTimer() > 0.0f &&
               nearlyEqual(state.idleTimer(), 0.0f),
           "INT_MAX counting overflowed or consumed idle time");

    reporter.beginSuite("settlement-confirm-and-snapshot-retention");
    SettlementStart confirmStart;
    confirmStart.stage = 11;
    confirmStart.playerCount = 2;
    confirmStart.scores = {{500, 350}};
    confirmStart.tallies[0] = tallyWithKills(2, 0, 1, 0);
    confirmStart.tallies[1] = tallyWithKills(1, 1, 0, 2);
    state.begin(confirmStart);
    state.update(0.1f);
    expect(state.scoreCounter() == 1 && state.displayedKills(0, 0) == 1 &&
               state.counting(),
           "confirm fixture did not begin with partial progress");
    expect(state.confirm() == SettlementCompletion::None && state.active() &&
               !state.counting() && state.phase() == SettlementPhase::Idle,
           "confirm while Counting did not reveal and enter Idle");
    expect(state.scoreCounter() == 500 &&
               state.categoryIndex() == kEnemyTypeCount &&
               state.displayedKills(0, 0) == 2 &&
               state.displayedKills(0, 2) == 1 &&
               state.displayedKills(1, 1) == 1 &&
               state.displayedKills(1, 3) == 2,
           "confirm did not reveal every score and classified K.O. total");
    expect(nearlyEqual(state.idleTimer(), 0.0f),
           "confirm did not restart the idle hold");
    expect(state.confirm() == SettlementCompletion::AdvanceStage &&
               !state.active() && !state.gameOver() &&
               state.phase() == SettlementPhase::None,
           "confirm while Idle did not complete a successful report");
    expect(state.stage() == 11 && state.scoreCounter() == 500 &&
               state.maximumScore() == 500 && state.tally(0).destroyed[2] == 1 &&
               state.displayedKills(1, 3) == 2 &&
               nearlyEqual(state.countTimer(), 0.0f) &&
               nearlyEqual(state.idleTimer(), 0.0f),
           "completion did not retain the immutable report snapshot");
    const SettlementUpdate completedUpdate = state.update(10.0f);
    expect(state.confirm() == SettlementCompletion::None &&
               completedUpdate.completion == SettlementCompletion::None &&
               completedUpdate.countedSteps == 0,
           "completed report emitted a duplicate transition");

    reporter.beginSuite("settlement-idle-timeout-and-completion-kind");
    SettlementStart timeoutStart;
    timeoutStart.stage = 8;
    state.begin(timeoutStart);
    const SettlementUpdate beforeTimeout = state.update(4.999f);
    expect(beforeTimeout.completion == SettlementCompletion::None &&
               state.active() && state.phase() == SettlementPhase::Idle &&
               nearlyEqual(state.idleTimer(), 4.999f),
           "successful report ended before the five-second hold");
    const SettlementUpdate stillBeforeTimeout = state.update(0.0005f);
    expect(stillBeforeTimeout.completion == SettlementCompletion::None &&
               state.active() && state.idleTimer() < kSettlementIdleTime,
           "sub-boundary idle update completed the report");
    const SettlementUpdate timeout = state.update(0.0006f);
    expect(timeout.completion == SettlementCompletion::AdvanceStage &&
               !state.active() && !state.gameOver() &&
               state.phase() == SettlementPhase::None &&
               nearlyEqual(state.countTimer(), 0.0f) &&
               nearlyEqual(state.idleTimer(), 0.0f),
           "successful report did not complete at five seconds");

    SettlementStart gameOverStart;
    gameOverStart.stage = 12;
    gameOverStart.gameOver = true;
    state.begin(gameOverStart);
    expect(state.active() && state.gameOver() &&
               state.phase() == SettlementPhase::Idle,
           "empty game-over report did not begin in Idle");
    const SettlementUpdate gameOverTimeout = state.update(5.0f);
    expect(gameOverTimeout.completion == SettlementCompletion::GameOver &&
               !state.active() && !state.gameOver() &&
               state.phase() == SettlementPhase::None,
           "game-over report returned the wrong completion kind");
    expect(state.stage() == 12 && state.maximumScore() == 0 &&
               state.categoryIndex() == kEnemyTypeCount &&
               nearlyEqual(state.countTimer(), 0.0f) &&
               nearlyEqual(state.idleTimer(), 0.0f),
           "game-over completion did not retain its report snapshot");

    reporter.finish();
    return passed ? 0 : 1;
}
