#include "game/combat_system.h"
#include "test_support.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <string>
#include <utility>

namespace
{
using tanks3d::core::CardinalDirection;
using tanks3d::core::XZ;
using tanks3d::game::CombatOutcome;
using tanks3d::game::CombatTarget;
using tanks3d::game::Enemy;
using tanks3d::game::EnemyTankImpactCommit;
using tanks3d::game::GameEvent;
using tanks3d::game::GameEventCause;
using tanks3d::game::GameEventType;
using tanks3d::game::GovernmentBasePart;
using tanks3d::game::GovernmentWallSegment;
using tanks3d::game::ImpactKind;
using tanks3d::game::Player;
using tanks3d::game::PlayerHitResult;
using tanks3d::game::PlayerTankImpactCommit;
using tanks3d::game::Shell;
using tanks3d::game::ShellCancellationOutcome;
using tanks3d::game::ShellFrameSchedule;
using tanks3d::game::ShellOwner;
using tanks3d::game::ShellPhysicalImpactResult;
using tanks3d::game::StageMap;
using tanks3d::game::beginShellImpact;
using tanks3d::game::advanceShellMicrostep;
using tanks3d::game::commitPlayerShellEnemyTankImpact;
using tanks3d::game::commitEnemyShellPlayerTankImpact;
using tanks3d::game::evaluatePlayerShellEnemyTankImpact;
using tanks3d::game::evaluateEnemyShellPlayerTankImpact;
using tanks3d::game::eventForShellCancellation;
using tanks3d::game::eventsForPhysicalShellImpact;
using tanks3d::game::governmentWallSegment;
using tanks3d::game::kGovernmentBaseCenter;
using tanks3d::game::kGovernmentCoreRadius;
using tanks3d::game::kGovernmentWallCount;
using tanks3d::game::kGovernmentWallMaximumHealth;
using tanks3d::game::kDirectEnemyHitPoints;
using tanks3d::game::kShellCancellationExtent;
using tanks3d::game::kShellHalfSize;
using tanks3d::game::kShellImpactDuration;
using tanks3d::game::kShellSpawnDistance;
using tanks3d::game::kShellSweepStep;
using tanks3d::game::kShellTankHitExtent;
using tanks3d::game::kStreakPopupDuration;
using tanks3d::game::kTankDeathDuration;
using tanks3d::game::kMapSize;
using tanks3d::game::resolveSweptShellCancellation;
using tanks3d::game::resolveShellCancellations;
using tanks3d::game::prepareShellFrame;
using tanks3d::game::removeExpiredShells;
using tanks3d::game::resolveShellGovernmentCoreImpact;
using tanks3d::game::resolveShellMapImpact;
using tanks3d::game::resolveShellPhysicalImpact;
using tanks3d::game::shellCancellationPoint;
using tanks3d::game::shellEvent;
using tanks3d::game::shellSpawnPosition;
using tanks3d::game::shellsCanCancel;
using tanks3d::game::shellsOverlapForCancellation;

bool nearlyEqual(float first, float second, float tolerance = 0.0001f)
{
    return std::fabs(first - second) < tolerance;
}

bool samePosition(XZ first, XZ second, float tolerance = 0.0001f)
{
    return nearlyEqual(first.x, second.x, tolerance) &&
           nearlyEqual(first.z, second.z, tolerance);
}

Shell testShell(XZ position, XZ velocity, ShellOwner owner)
{
    Shell shell;
    shell.position = position;
    shell.velocity = velocity;
    shell.owner = owner;
    return shell;
}

struct Cell
{
    int row = -1;
    int column = -1;

    bool valid() const
    {
        return row >= 0 && column >= 0;
    }

    XZ center() const
    {
        return {column + 0.5f, row + 0.5f};
    }
};

Cell findTile(const StageMap &map, char requested)
{
    for (int row = 0; row < kMapSize; ++row)
        for (int column = 0; column < kMapSize; ++column)
            if (map.tile(row, column) == requested)
                return {row, column};
    return {};
}

bool loadStageOne(StageMap &map)
{
    std::string error;
    return map.load(std::filesystem::path{}, 1, error) && error.empty();
}

bool loadTerrainFixture(StageMap &map)
{
    std::string error;
    return map.load(std::filesystem::path{}, 2, error) && error.empty();
}

bool shellStateMatches(const Shell &shell, XZ position, XZ velocity,
                       float life = 4.0f)
{
    return samePosition(shell.position, position) &&
           samePosition(shell.velocity, velocity) &&
           !shell.impacting && nearlyEqual(shell.life, life);
}

bool sameShellState(const Shell &first, const Shell &second)
{
    return samePosition(first.position, second.position) &&
           samePosition(first.velocity, second.velocity) &&
           first.owner == second.owner &&
           first.ownerIndex == second.ownerIndex &&
           first.power == second.power &&
           first.impacting == second.impacting &&
           nearlyEqual(first.life, second.life);
}

bool breachGovernmentWalls(StageMap &map)
{
    for (int index = 0; index < kGovernmentWallCount; ++index)
        for (int hit = 0; hit < kGovernmentWallMaximumHealth; ++hit)
            map.impactShell(governmentWallSegment(index).center, false,
                            CardinalDirection::North);

    for (int index = 0; index < kGovernmentWallCount; ++index)
        if (map.governmentWallHealth(index) != 0)
            return false;
    return true;
}

CombatOutcome openEnvironmentOutcome(const Shell &shell)
{
    StageMap map;
    map.prepareShowcaseArena();
    bool baseAlive = true;
    const CombatOutcome mapOutcome = resolveShellMapImpact(map, shell);
    return resolveShellGovernmentCoreImpact(map, baseAlive, mapOutcome);
}

Enemy testEnemy(int id, int type, int armor, XZ position)
{
    Enemy enemy;
    enemy.id = id;
    enemy.type = type;
    enemy.armor = armor;
    enemy.position = position;
    enemy.moving = true;
    return enemy;
}

Player testPlayer(int id, int hitPoints, XZ position)
{
    Player player;
    player.id = id;
    player.hitPoints = hitPoints;
    player.position = position;
    player.active = true;
    player.moving = true;
    return player;
}
} // namespace

int main()
{
    tanks3d_test::Reporter reporter;
    bool passed = true;
    const auto expect = [&](bool condition, const char *message) {
        passed = reporter.check(condition, message) && passed;
    };

    reporter.beginSuite("combat-shell-geometry-and-spawn");
    expect(nearlyEqual(kShellHalfSize, 0.25f) &&
               nearlyEqual(kShellTankHitExtent, 1.125f) &&
               nearlyEqual(kShellSpawnDistance, 0.625f) &&
               nearlyEqual(kShellSweepStep, 0.12f) &&
               nearlyEqual(kShellImpactDuration, 0.200f) &&
               nearlyEqual(kShellCancellationExtent, 0.50f) &&
               kDirectEnemyHitPoints == 50 &&
               nearlyEqual(kTankDeathDuration, 0.490f),
           "classic projectile geometry or timing changed");

    const XZ tankPosition{10.0f, 11.0f};
    expect(samePosition(shellSpawnPosition(tankPosition,
                                           CardinalDirection::North),
                        {10.0f, 10.375f}),
           "north shell spawn moved away from the simulation muzzle");
    expect(samePosition(shellSpawnPosition(tankPosition,
                                           CardinalDirection::East),
                        {10.625f, 11.0f}),
           "east shell spawn moved away from the simulation muzzle");
    expect(samePosition(shellSpawnPosition(tankPosition,
                                           CardinalDirection::South),
                        {10.0f, 11.625f}),
           "south shell spawn moved away from the simulation muzzle");
    expect(samePosition(shellSpawnPosition(tankPosition,
                                           CardinalDirection::West),
                        {9.375f, 11.0f}),
           "west shell spawn moved away from the simulation muzzle");
    expect(samePosition(shellSpawnPosition(tankPosition,
                                           CardinalDirection::None),
                        tankPosition),
           "directionless shell spawn stopped remaining at the tank center");

    Shell defaults;
    expect(defaults.owner == ShellOwner::Player && defaults.ownerIndex == -1 &&
               !defaults.power && !defaults.impacting &&
               nearlyEqual(defaults.life, 4.0f),
           "Shell defaults changed during combat extraction");

    reporter.beginSuite("combat-shell-impact-and-eligibility");
    Shell playerRound = testShell({0.0f, 0.0f}, {10.0f, 0.0f},
                                  ShellOwner::Player);
    Shell enemyRound = testShell({0.25f, 0.0f}, {-10.0f, 0.0f},
                                 ShellOwner::Enemy);
    expect(shellsCanCancel(playerRound, enemyRound),
           "opposing live shells stopped being cancellation candidates");

    Shell secondPlayer = playerRound;
    secondPlayer.ownerIndex = 1;
    expect(!shellsCanCancel(playerRound, secondPlayer) &&
               !shellsOverlapForCancellation(playerRound, secondPlayer),
           "P1 and P2 shells began cancelling each other");

    Shell secondEnemy = enemyRound;
    secondEnemy.ownerIndex = 17;
    expect(!shellsCanCancel(enemyRound, secondEnemy),
           "shells from different enemies began cancelling each other");

    Shell impacting = playerRound;
    impacting.impacting = true;
    expect(!shellsCanCancel(impacting, enemyRound),
           "an impacting shell remained eligible for cancellation");
    enemyRound.impacting = true;
    expect(!shellsCanCancel(playerRound, enemyRound),
           "an impacting second shell remained eligible for cancellation");
    enemyRound.impacting = false;

    Shell expiredCandidate = playerRound;
    expiredCandidate.life = 0.0f;
    expect(shellsCanCancel(expiredCandidate, enemyRound),
           "eligibility helper began owning runtime lifetime policy");

    Shell impactTransition = playerRound;
    beginShellImpact(impactTransition, {3.0f, 4.0f});
    expect(impactTransition.impacting &&
               samePosition(impactTransition.position, {3.0f, 4.0f}) &&
               samePosition(impactTransition.velocity, {}) &&
               nearlyEqual(impactTransition.life, kShellImpactDuration),
           "shell impact did not enter the exact 200 ms occupied state");

    impactTransition.life = 0.075f;
    beginShellImpact(impactTransition, {8.0f, 9.0f});
    expect(samePosition(impactTransition.position, {3.0f, 4.0f}) &&
               nearlyEqual(impactTransition.life, 0.075f),
           "repeated impact changed an already-impacting shell");

    reporter.beginSuite("combat-swept-shell-cancellation");
    XZ collisionPoint{};
    expect(shellsOverlapForCancellation(playerRound, enemyRound) &&
               shellCancellationPoint(playerRound, enemyRound, 0.0f,
                                      collisionPoint) &&
               samePosition(collisionPoint, {0.125f, 0.0f}),
           "initially overlapping shells no longer cancel at dt zero");

    enemyRound.position = {0.499f, 0.499f};
    expect(shellsOverlapForCancellation(playerRound, enemyRound),
           "positive-area classic projectile overlap was missed");
    enemyRound.position = {0.500f, 0.0f};
    expect(!shellsOverlapForCancellation(playerRound, enemyRound),
           "edge-only classic projectile contact became a collision");

    playerRound.position = {0.0f, 0.0f};
    playerRound.velocity = {10.0f, 0.0f};
    enemyRound.position = {1.0f, 0.0f};
    enemyRound.velocity = {-10.0f, 0.0f};
    XZ firstAtCollision{};
    XZ secondAtCollision{};
    expect(shellCancellationPoint(playerRound, enemyRound, 0.05f,
                                  collisionPoint, &firstAtCollision,
                                  &secondAtCollision) &&
               nearlyEqual(collisionPoint.x, 0.5f, 0.001f) &&
               firstAtCollision.x > 0.25f &&
               secondAtCollision.x < 0.75f,
           "head-on swept collision or contact positions changed");

    enemyRound.position = {0.24f, -1.0f};
    enemyRound.velocity = {0.0f, 20.0f};
    expect(shellCancellationPoint(playerRound, enemyRound, 0.05f,
                                  collisionPoint),
           "perpendicular swept shells crossed without cancellation");

    enemyRound.position = {1.0f, 0.0f};
    enemyRound.velocity = {-10.0f, 0.0f};
    expect(!shellCancellationPoint(playerRound, enemyRound, 0.0f,
                                   collisionPoint),
           "separated shells cancelled without elapsed time");
    expect(!shellCancellationPoint(playerRound, enemyRound, -0.01f,
                                   collisionPoint),
           "separated shells cancelled for negative elapsed time");

    enemyRound.velocity = {10.0f, 0.0f};
    expect(!shellCancellationPoint(playerRound, enemyRound, 0.20f,
                                   collisionPoint),
           "parallel separated shells incorrectly cancelled");

    reporter.beginSuite("combat-runtime-cancellation-and-separation");
    StageMap openMap;
    openMap.prepareShowcaseArena();
    Shell runtimePlayer = testShell({5.5f, 5.0f}, {10.0f, 0.0f},
                                    ShellOwner::Player);
    runtimePlayer.ownerIndex = 0;
    Shell runtimeEnemy = testShell({5.5f, 5.0f}, {-10.0f, 0.0f},
                                   ShellOwner::Enemy);
    runtimeEnemy.ownerIndex = 3;
    expect(resolveSweptShellCancellation(
               runtimePlayer, runtimeEnemy, {5.0f, 5.0f}, {6.0f, 5.0f},
               0.05f, openMap, collisionPoint) &&
               runtimePlayer.impacting && runtimeEnemy.impacting &&
               nearlyEqual(runtimePlayer.life, kShellImpactDuration) &&
               nearlyEqual(runtimeEnemy.life, kShellImpactDuration) &&
               samePosition(runtimePlayer.velocity, {}) &&
               samePosition(runtimeEnemy.velocity, {}) &&
               nearlyEqual(collisionPoint.x, 5.5f, 0.001f),
           "runtime cancellation stopped applying both impact states");

    expect(!resolveSweptShellCancellation(
               runtimePlayer, runtimeEnemy, {5.0f, 5.0f}, {6.0f, 5.0f},
               0.05f, openMap, collisionPoint),
           "an impacting pair cancelled more than once");

    Shell expiredPlayer = testShell({5.5f, 5.0f}, {10.0f, 0.0f},
                                    ShellOwner::Player);
    Shell liveEnemy = testShell({5.5f, 5.0f}, {-10.0f, 0.0f},
                                ShellOwner::Enemy);
    expiredPlayer.life = 0.0f;
    expect(!resolveSweptShellCancellation(
               expiredPlayer, liveEnemy, {5.0f, 5.0f}, {6.0f, 5.0f},
               0.05f, openMap, collisionPoint),
           "an expired shell remained eligible in the runtime wrapper");
    expiredPlayer.life = 4.0f;
    liveEnemy.life = 0.0f;
    expect(!resolveSweptShellCancellation(
               expiredPlayer, liveEnemy, {5.0f, 5.0f}, {6.0f, 5.0f},
               0.05f, openMap, collisionPoint),
           "an expired second shell remained eligible in the runtime wrapper");

    StageMap walledMap;
    walledMap.prepareShowcaseArena();
    const GovernmentWallSegment wall = governmentWallSegment(0);
    const XZ firstStart = wall.center - wall.outward * 0.75f;
    const XZ secondStart = wall.center + wall.outward * 0.75f;
    Shell wallPlayer = testShell(firstStart + wall.outward,
                                 wall.outward * 10.0f,
                                 ShellOwner::Player);
    Shell wallEnemy = testShell(secondStart - wall.outward,
                                wall.outward * -10.0f,
                                ShellOwner::Enemy);
    expect(!resolveSweptShellCancellation(
               wallPlayer, wallEnemy, firstStart, secondStart, 0.10f,
               walledMap, collisionPoint) &&
               !wallPlayer.impacting && !wallEnemy.impacting,
           "opposing shells cancelled through an intact government wall");

    reporter.beginSuite("combat-shell-cancellation-batch");
    std::vector<Shell> mismatchedBatch{
        testShell({4.0f, 4.0f}, {}, ShellOwner::Player),
        testShell({4.0f, 4.0f}, {}, ShellOwner::Enemy)};
    mismatchedBatch[0].ownerIndex = 0;
    mismatchedBatch[1].ownerIndex = 90;
    const std::vector<Shell> mismatchedBefore = mismatchedBatch;
    const std::vector<ShellCancellationOutcome> mismatchedOutcomes =
        resolveShellCancellations(
            mismatchedBatch, {{4.0f, 4.0f}}, 0.0f, openMap);
    expect(mismatchedOutcomes.empty() &&
               sameShellState(mismatchedBatch[0], mismatchedBefore[0]) &&
               sameShellState(mismatchedBatch[1], mismatchedBefore[1]),
           "mismatched cancellation snapshots changed caller-owned shells");

    std::vector<Shell> sweptBatch{
        testShell({5.5f, 5.0f}, {10.0f, 0.0f}, ShellOwner::Player),
        testShell({5.5f, 5.0f}, {-10.0f, 0.0f}, ShellOwner::Enemy)};
    sweptBatch[0].ownerIndex = 1;
    sweptBatch[1].ownerIndex = 91;
    const std::vector<ShellCancellationOutcome> sweptOutcomes =
        resolveShellCancellations(
            sweptBatch, {{5.0f, 5.0f}, {6.0f, 5.0f}}, 0.05f,
            openMap);
    expect(sweptOutcomes.size() == 1U &&
               sweptOutcomes[0].firstShellIndex == 0 &&
               sweptOutcomes[0].secondShellIndex == 1 &&
               sweptOutcomes[0].firstOwner == ShellOwner::Player &&
               sweptOutcomes[0].firstOwnerIndex == 1 &&
               sweptOutcomes[0].secondOwner == ShellOwner::Enemy &&
               sweptOutcomes[0].secondOwnerIndex == 91 &&
               nearlyEqual(sweptOutcomes[0].position.x, 5.5f, 0.001f) &&
               samePosition(sweptOutcomes[0].firstImpactPosition,
                            sweptBatch[0].position) &&
               samePosition(sweptOutcomes[0].secondImpactPosition,
                            sweptBatch[1].position) &&
               sweptBatch[0].position.x < sweptOutcomes[0].position.x &&
               sweptBatch[1].position.x > sweptOutcomes[0].position.x &&
               samePosition(sweptBatch[0].velocity, {}) &&
               samePosition(sweptBatch[1].velocity, {}) &&
               nearlyEqual(sweptBatch[0].life, kShellImpactDuration) &&
               nearlyEqual(sweptBatch[1].life, kShellImpactDuration),
           "batch crossing lost contact positions, attribution, or impact state");

    std::vector<Shell> candidateBatch{
        testShell({8.0f, 8.0f}, {}, ShellOwner::Player),
        testShell({8.0f, 8.0f}, {}, ShellOwner::Player),
        testShell({14.0f, 14.0f}, {}, ShellOwner::Enemy),
        testShell({8.0f, 8.0f}, {}, ShellOwner::Enemy)};
    const std::vector<XZ> candidateStarts{
        candidateBatch[0].position, candidateBatch[1].position,
        candidateBatch[2].position, candidateBatch[3].position};
    const std::vector<ShellCancellationOutcome> candidateOutcomes =
        resolveShellCancellations(candidateBatch, candidateStarts, 0.0f,
                                  openMap);
    expect(candidateOutcomes.size() == 1U &&
               candidateOutcomes[0].firstShellIndex == 0 &&
               candidateOutcomes[0].secondShellIndex == 3 &&
               candidateBatch[0].impacting &&
               !candidateBatch[1].impacting &&
               !candidateBatch[2].impacting &&
               candidateBatch[3].impacting,
           "rejected low-index candidates blocked the first successful pair");

    std::vector<Shell> orderedBatch{
        testShell({10.0f, 10.0f}, {}, ShellOwner::Player),
        testShell({10.0f, 10.0f}, {}, ShellOwner::Enemy),
        testShell({10.0f, 10.0f}, {}, ShellOwner::Enemy),
        testShell({18.0f, 18.0f}, {}, ShellOwner::Enemy),
        testShell({18.0f, 18.0f}, {}, ShellOwner::Player)};
    orderedBatch[0].ownerIndex = 2;
    orderedBatch[1].ownerIndex = 92;
    orderedBatch[2].ownerIndex = 93;
    orderedBatch[3].ownerIndex = 94;
    orderedBatch[4].ownerIndex = 3;
    std::vector<XZ> orderedStarts;
    for (const Shell &shell : orderedBatch)
        orderedStarts.push_back(shell.position);
    const std::vector<ShellCancellationOutcome> orderedOutcomes =
        resolveShellCancellations(orderedBatch, orderedStarts, 0.0f,
                                  openMap);
    expect(orderedOutcomes.size() == 2U &&
               orderedOutcomes[0].firstShellIndex == 0 &&
               orderedOutcomes[0].secondShellIndex == 1 &&
               orderedOutcomes[0].firstOwnerIndex == 2 &&
               orderedOutcomes[0].secondOwnerIndex == 92 &&
               orderedOutcomes[1].firstShellIndex == 3 &&
               orderedOutcomes[1].secondShellIndex == 4 &&
               orderedOutcomes[1].firstOwnerIndex == 94 &&
               orderedOutcomes[1].secondOwnerIndex == 3 &&
               orderedBatch[0].impacting && orderedBatch[1].impacting &&
               !orderedBatch[2].impacting && orderedBatch[3].impacting &&
               orderedBatch[4].impacting,
           "greedy cancellation order or one-pair-per-shell rule changed");

    std::vector<Shell> inertBatch{
        testShell({9.0f, 9.0f}, {}, ShellOwner::Player),
        testShell({9.0f, 9.0f}, {}, ShellOwner::Enemy),
        testShell({16.0f, 16.0f}, {}, ShellOwner::Player),
        testShell({16.0f, 16.0f}, {}, ShellOwner::Enemy)};
    inertBatch[0].impacting = true;
    inertBatch[2].life = 0.0f;
    std::vector<XZ> inertStarts;
    for (const Shell &shell : inertBatch)
        inertStarts.push_back(shell.position);
    const std::vector<ShellCancellationOutcome> inertOutcomes =
        resolveShellCancellations(inertBatch, inertStarts, 0.0f, openMap);
    expect(inertOutcomes.empty() && inertBatch[0].impacting &&
               !inertBatch[1].impacting && !inertBatch[3].impacting,
           "impacting or expired shell re-entered batch cancellation");

    std::vector<Shell> wallBatch{wallPlayer, wallEnemy};
    const std::vector<ShellCancellationOutcome> wallBatchOutcomes =
        resolveShellCancellations(
            wallBatch, {firstStart, secondStart}, 0.10f, walledMap);
    expect(wallBatchOutcomes.empty() && !wallBatch[0].impacting &&
               !wallBatch[1].impacting,
           "batch cancellation crossed an intact government wall");

    reporter.beginSuite("combat-shell-cancellation-event-projection");
    const ShellCancellationOutcome defaultCancellation;
    GameEvent expectedDefaultCancellation;
    expectedDefaultCancellation.type = GameEventType::ShellCancelled;
    expect(eventForShellCancellation(defaultCancellation) ==
               expectedDefaultCancellation,
           "default resolver snapshot changed cancellation sentinels");

    ShellCancellationOutcome playerFirstCancellation;
    playerFirstCancellation.firstShellIndex = 4;
    playerFirstCancellation.secondShellIndex = 9;
    playerFirstCancellation.position = {6.25f, 7.75f};
    playerFirstCancellation.firstImpactPosition = {6.0f, 7.75f};
    playerFirstCancellation.secondImpactPosition = {6.5f, 7.75f};
    playerFirstCancellation.firstOwner = ShellOwner::Player;
    playerFirstCancellation.firstOwnerIndex = 0;
    playerFirstCancellation.secondOwner = ShellOwner::Enemy;
    playerFirstCancellation.secondOwnerIndex = 0;
    GameEvent expectedPlayerFirst;
    expectedPlayerFirst.type = GameEventType::ShellCancelled;
    expectedPlayerFirst.position = playerFirstCancellation.position;
    expectedPlayerFirst.sourcePlayerId = 0;
    expectedPlayerFirst.sourceEnemyId = 0;
    expect(eventForShellCancellation(playerFirstCancellation) ==
               expectedPlayerFirst,
           "player-first cancellation projection changed fields or sources");

    ShellCancellationOutcome enemyFirstCancellation;
    enemyFirstCancellation.firstShellIndex = 1;
    enemyFirstCancellation.secondShellIndex = 3;
    enemyFirstCancellation.position = {15.5f, 12.5f};
    enemyFirstCancellation.firstImpactPosition = {15.5f, 12.25f};
    enemyFirstCancellation.secondImpactPosition = {15.5f, 12.75f};
    enemyFirstCancellation.firstOwner = ShellOwner::Enemy;
    enemyFirstCancellation.firstOwnerIndex = 105;
    enemyFirstCancellation.secondOwner = ShellOwner::Player;
    enemyFirstCancellation.secondOwnerIndex = 1;
    GameEvent expectedEnemyFirst;
    expectedEnemyFirst.type = GameEventType::ShellCancelled;
    expectedEnemyFirst.position = enemyFirstCancellation.position;
    expectedEnemyFirst.sourcePlayerId = 1;
    expectedEnemyFirst.sourceEnemyId = 105;
    expect(eventForShellCancellation(enemyFirstCancellation) ==
               expectedEnemyFirst,
           "enemy-first cancellation projection changed fields or sources");

    reporter.beginSuite("combat-shell-frame-lifecycle");
    std::vector<Shell> emptyShells;
    const ShellFrameSchedule emptySchedule =
        prepareShellFrame(emptyShells, 0.25f);
    expect(emptySchedule.stepCount == 1 &&
               nearlyEqual(emptySchedule.stepTime, 0.25f),
           "empty shell frame lost its single deterministic micro-step");

    std::vector<Shell> exactBoundaryShells{
        testShell({1.0f, 1.0f}, {1.0f, 0.0f}, ShellOwner::Player)};
    const ShellFrameSchedule exactBoundarySchedule =
        prepareShellFrame(exactBoundaryShells, kShellSweepStep);
    expect(exactBoundarySchedule.stepCount == 1 &&
               nearlyEqual(exactBoundarySchedule.stepTime,
                           kShellSweepStep) &&
               nearlyEqual(exactBoundaryShells[0].life,
                           4.0f - kShellSweepStep),
           "exact sweep boundary changed frame aging or step count");

    std::vector<Shell> lifecycleShells{
        testShell({2.0f, 3.0f}, {1.0f, 0.0f}, ShellOwner::Player),
        testShell({4.0f, 5.0f}, {3.0f, 0.0f}, ShellOwner::Enemy),
        testShell({6.0f, 7.0f}, {100.0f, 0.0f}, ShellOwner::Enemy),
        testShell({8.0f, 9.0f}, {100.0f, 0.0f}, ShellOwner::Player)};
    lifecycleShells[0].ownerIndex = 10;
    lifecycleShells[1].ownerIndex = 20;
    lifecycleShells[2].ownerIndex = 30;
    lifecycleShells[2].impacting = true;
    lifecycleShells[2].life = 0.50f;
    lifecycleShells[3].ownerIndex = 40;
    lifecycleShells[3].life = 0.10f;
    const ShellFrameSchedule lifecycleSchedule =
        prepareShellFrame(lifecycleShells, 0.10f);
    expect(lifecycleSchedule.stepCount == 3 &&
               nearlyEqual(lifecycleSchedule.stepTime, 0.10f / 3.0f) &&
               nearlyEqual(lifecycleShells[0].life, 3.90f) &&
               nearlyEqual(lifecycleShells[1].life, 3.90f) &&
               nearlyEqual(lifecycleShells[2].life, 0.40f) &&
               nearlyEqual(lifecycleShells[3].life, 0.0f),
           "fastest live shell stopped owning the shared frame schedule");

    const std::vector<XZ> lifecycleStarts{
        lifecycleShells[0].position, lifecycleShells[1].position,
        lifecycleShells[2].position, lifecycleShells[3].position};
    std::vector<XZ> previousPositions;
    advanceShellMicrostep(lifecycleShells, lifecycleSchedule.stepTime,
                          previousPositions);
    expect(previousPositions.size() == lifecycleStarts.size() &&
               samePosition(previousPositions[0], lifecycleStarts[0]) &&
               samePosition(previousPositions[1], lifecycleStarts[1]) &&
               samePosition(previousPositions[2], lifecycleStarts[2]) &&
               samePosition(previousPositions[3], lifecycleStarts[3]) &&
               samePosition(lifecycleShells[0].position,
                            lifecycleStarts[0] +
                                lifecycleShells[0].velocity *
                                    lifecycleSchedule.stepTime) &&
               samePosition(lifecycleShells[1].position,
                            lifecycleStarts[1] +
                                lifecycleShells[1].velocity *
                                    lifecycleSchedule.stepTime) &&
               samePosition(lifecycleShells[2].position,
                            lifecycleStarts[2]) &&
               samePosition(lifecycleShells[3].position,
                            lifecycleStarts[3]),
           "micro-step lost start snapshots or moved an inert shell");

    beginShellImpact(lifecycleShells[0], lifecycleShells[0].position);
    expect(nearlyEqual(lifecycleShells[0].life, kShellImpactDuration),
           "same-frame impact did not receive the full occupied duration");

    removeExpiredShells(lifecycleShells);
    expect(lifecycleShells.size() == 3U &&
               lifecycleShells[0].ownerIndex == 10 &&
               lifecycleShells[1].ownerIndex == 20 &&
               lifecycleShells[2].ownerIndex == 30,
           "expired-shell cleanup changed survivor order or retention");

    reporter.beginSuite("combat-map-pass-through-outcomes");
    const CombatOutcome defaultOutcome;
    expect(!defaultOutcome.stopsShell() &&
               !defaultOutcome.mapStopsShell() &&
               !defaultOutcome.damagedGovernmentCore() &&
               !defaultOutcome.destroyedGovernmentCore() &&
               !defaultOutcome.damagedEnemyTank() &&
               !defaultOutcome.destroyedEnemyTank() &&
               defaultOutcome.target == CombatTarget::None &&
               defaultOutcome.impactKind == ImpactKind::None &&
               defaultOutcome.impactDetails.brickCount == 0 &&
               defaultOutcome.impactDetails.governmentWallIndex == -1 &&
               defaultOutcome.targetPlayerIndex == -1 &&
               defaultOutcome.targetPlayerId == -1,
           "default combat outcome stopped a shell or carried stale details");

    StageMap openMapOutcome;
    openMapOutcome.prepareShowcaseArena();
    Shell openShell = testShell({5.0f, 5.0f}, {0.0f, -8.0f},
                                ShellOwner::Enemy);
    openShell.ownerIndex = 14;
    openShell.power = true;
    const CombatOutcome openOutcome =
        resolveShellMapImpact(openMapOutcome, openShell);
    expect(!openOutcome.stopsShell() &&
               samePosition(openOutcome.position, openShell.position) &&
               samePosition(openOutcome.incomingVelocity,
                            openShell.velocity) &&
               openOutcome.owner == ShellOwner::Enemy &&
               openOutcome.ownerIndex == 14 && openOutcome.power &&
               shellStateMatches(openShell, {5.0f, 5.0f}, {0.0f, -8.0f}),
           "open-ground outcome lost its source snapshot or changed the shell");

    StageMap normalForestMap;
    const bool normalForestLoaded = loadTerrainFixture(normalForestMap);
    const Cell normalForestCell = findTile(normalForestMap, '%');
    Shell normalForestShell = testShell(
        normalForestCell.center(), {0.0f, -8.0f}, ShellOwner::Player);
    const CombatOutcome normalForestOutcome =
        resolveShellMapImpact(normalForestMap, normalForestShell);
    expect(normalForestLoaded && normalForestCell.valid() &&
               !normalForestOutcome.stopsShell() &&
               normalForestMap.tile(normalForestCell.row,
                                    normalForestCell.column) == '%' &&
               shellStateMatches(normalForestShell, normalForestCell.center(),
                                 {0.0f, -8.0f}),
           "ordinary shell stopped in or changed forest cover");

    StageMap powerForestMap;
    const bool powerForestLoaded = loadTerrainFixture(powerForestMap);
    const Cell powerForestCell = findTile(powerForestMap, '%');
    Shell powerForestShell = testShell(
        powerForestCell.center(), {0.0f, -8.0f}, ShellOwner::Player);
    powerForestShell.power = true;
    const CombatOutcome powerForestOutcome =
        resolveShellMapImpact(powerForestMap, powerForestShell);
    expect(powerForestLoaded && powerForestCell.valid() &&
               !powerForestOutcome.stopsShell() &&
               powerForestOutcome.impactKind == ImpactKind::None &&
               powerForestMap.tile(powerForestCell.row,
                                   powerForestCell.column) == '.' &&
               shellStateMatches(powerForestShell, powerForestCell.center(),
                                 {0.0f, -8.0f}),
           "power shell no longer clears forest while continuing its flight");

    reporter.beginSuite("combat-map-blocking-outcomes");
    StageMap brickMap;
    const bool brickLoaded = loadStageOne(brickMap);
    const Cell brickCell = findTile(brickMap, '#');
    Shell brickShell = testShell(brickCell.center(), {0.0f, -8.0f},
                                 ShellOwner::Player);
    brickShell.ownerIndex = 1;
    const CombatOutcome brickOutcome =
        resolveShellMapImpact(brickMap, brickShell);
    expect(brickLoaded && brickCell.valid() && brickOutcome.stopsShell() &&
               brickOutcome.mapStopsShell() &&
               brickOutcome.target == CombatTarget::StageMap &&
               brickOutcome.impactKind == ImpactKind::Brick &&
               brickOutcome.owner == ShellOwner::Player &&
               brickOutcome.ownerIndex == 1 && !brickOutcome.power &&
               brickOutcome.impactDetails.brickCount == 1 &&
               brickOutcome.impactDetails.bricks[0].row == brickCell.row &&
               brickOutcome.impactDetails.bricks[0].column == brickCell.column &&
               brickOutcome.impactDetails.bricks[0].beforeMask == 0x0fU &&
               brickOutcome.impactDetails.bricks[0].afterMask == 0x03U &&
               brickMap.brickMask(brickCell.row, brickCell.column) == 0x03U &&
               shellStateMatches(brickShell, brickCell.center(),
                                 {0.0f, -8.0f}),
           "brick outcome lost source, damage payload, or deferred shell state");

    StageMap normalSteelMap;
    const bool normalSteelLoaded = loadStageOne(normalSteelMap);
    const Cell normalSteelCell = findTile(normalSteelMap, '@');
    Shell normalSteelShell = testShell(
        normalSteelCell.center(), {-8.0f, 0.0f}, ShellOwner::Player);
    const CombatOutcome normalSteelOutcome =
        resolveShellMapImpact(normalSteelMap, normalSteelShell);
    expect(normalSteelLoaded && normalSteelCell.valid() &&
               normalSteelOutcome.stopsShell() &&
               normalSteelOutcome.impactKind == ImpactKind::Steel &&
               normalSteelOutcome.impactDetails.brickCount == 0 &&
               normalSteelMap.tile(normalSteelCell.row,
                                   normalSteelCell.column) == '@' &&
               shellStateMatches(normalSteelShell, normalSteelCell.center(),
                                 {-8.0f, 0.0f}),
           "ordinary steel outcome changed its tile or shell snapshot");

    StageMap powerSteelMap;
    const bool powerSteelLoaded = loadStageOne(powerSteelMap);
    const Cell powerSteelCell = findTile(powerSteelMap, '@');
    Shell powerSteelShell = testShell(
        powerSteelCell.center(), {8.0f, 0.0f}, ShellOwner::Player);
    powerSteelShell.power = true;
    const CombatOutcome powerSteelOutcome =
        resolveShellMapImpact(powerSteelMap, powerSteelShell);
    expect(powerSteelLoaded && powerSteelCell.valid() &&
               powerSteelOutcome.stopsShell() && powerSteelOutcome.power &&
               powerSteelOutcome.impactKind == ImpactKind::Steel &&
               powerSteelMap.tile(powerSteelCell.row,
                                  powerSteelCell.column) == '.' &&
               shellStateMatches(powerSteelShell, powerSteelCell.center(),
                                 {8.0f, 0.0f}),
           "power shell no longer clears ordinary steel and stops");

    StageMap boundaryMapOutcome;
    boundaryMapOutcome.prepareShowcaseArena();
    const XZ boundaryPosition{kShellHalfSize - 0.001f, 5.0f};
    Shell boundaryShell = testShell(boundaryPosition, {-8.0f, 0.0f},
                                    ShellOwner::Player);
    const CombatOutcome boundaryOutcome =
        resolveShellMapImpact(boundaryMapOutcome, boundaryShell);
    expect(boundaryOutcome.stopsShell() &&
               boundaryOutcome.impactKind == ImpactKind::Boundary &&
               samePosition(boundaryOutcome.position, boundaryPosition) &&
               samePosition(boundaryOutcome.incomingVelocity, {-8.0f, 0.0f}) &&
               shellStateMatches(boundaryShell, boundaryPosition,
                                 {-8.0f, 0.0f}),
           "boundary outcome lost the incoming shell snapshot");

    StageMap governmentMap;
    governmentMap.prepareShowcaseArena();
    const GovernmentWallSegment governmentWall = governmentWallSegment(0);
    Shell governmentShell = testShell(
        governmentWall.center, {0.0f, 8.0f}, ShellOwner::Enemy);
    governmentShell.ownerIndex = 9;
    const CombatOutcome governmentOutcome =
        resolveShellMapImpact(governmentMap, governmentShell);
    expect(governmentOutcome.stopsShell() &&
               governmentOutcome.impactKind == ImpactKind::GovernmentWall &&
               governmentOutcome.impactDetails.brickCount == 0 &&
               governmentOutcome.impactDetails.governmentWallIndex == 0 &&
               governmentOutcome.impactDetails.governmentWallHealthBefore == 4 &&
               governmentOutcome.impactDetails.governmentWallHealthAfter == 3 &&
               governmentMap.governmentWallHealth(0) == 3 &&
               shellStateMatches(governmentShell, governmentWall.center,
                                 {0.0f, 8.0f}),
           "government-wall outcome lost its segment health transition");

    StageMap protectedGovernmentMap;
    protectedGovernmentMap.prepareShowcaseArena();
    protectedGovernmentMap.activateGovernmentSteel();
    Shell protectedShell = testShell(
        governmentWall.center, {0.0f, 8.0f}, ShellOwner::Player);
    protectedShell.power = true;
    const CombatOutcome protectedOutcome =
        resolveShellMapImpact(protectedGovernmentMap, protectedShell);
    expect(protectedOutcome.stopsShell() && protectedOutcome.power &&
               protectedOutcome.impactKind == ImpactKind::Steel &&
               protectedOutcome.impactDetails.governmentWallIndex == 0 &&
               protectedOutcome.impactDetails.governmentWallHealthBefore == 4 &&
               protectedOutcome.impactDetails.governmentWallHealthAfter == 4 &&
               protectedGovernmentMap.governmentWallHealth(0) == 4 &&
               shellStateMatches(protectedShell, governmentWall.center,
                                 {0.0f, 8.0f}),
           "Shovel steel outcome reported damage or lost wall identity");

    reporter.beginSuite("combat-government-core-resolution");
    StageMap missedCoreMap;
    missedCoreMap.prepareShowcaseArena();
    bool missedBaseAlive = true;
    Shell missedCoreShell = testShell({5.0f, 5.0f}, {0.0f, 8.0f},
                                     ShellOwner::Enemy);
    missedCoreShell.ownerIndex = 40;
    missedCoreShell.power = true;
    const Shell missedCoreBefore = missedCoreShell;
    const CombatOutcome missedCoreMapOutcome =
        resolveShellMapImpact(missedCoreMap, missedCoreShell);
    const CombatOutcome missedCoreOutcome =
        resolveShellGovernmentCoreImpact(missedCoreMap, missedBaseAlive,
                                         missedCoreMapOutcome);
    expect(!missedCoreOutcome.stopsShell() &&
               !missedCoreOutcome.mapStopsShell() &&
               missedCoreOutcome.target == CombatTarget::None &&
               missedCoreOutcome.governmentCoreHealthBefore == 0 &&
               missedCoreOutcome.governmentCoreHealthAfter == 0 &&
               missedBaseAlive &&
               sameShellState(missedCoreShell, missedCoreBefore),
           "an open-ground core query changed the base or shell");

    StageMap playerCoreMap;
    playerCoreMap.prepareShowcaseArena();
    bool playerBaseAlive = true;
    Shell playerCoreShell = testShell(kGovernmentBaseCenter,
                                     {0.0f, -8.0f}, ShellOwner::Player);
    playerCoreShell.ownerIndex = 1;
    playerCoreShell.power = true;
    playerCoreShell.life = 2.75f;
    const Shell playerCoreBefore = playerCoreShell;
    const CombatOutcome playerCoreMapOutcome =
        resolveShellMapImpact(playerCoreMap, playerCoreShell);
    const CombatOutcome playerCoreOutcome =
        resolveShellGovernmentCoreImpact(playerCoreMap, playerBaseAlive,
                                         playerCoreMapOutcome);
    expect(playerCoreOutcome.stopsShell() &&
               !playerCoreOutcome.mapStopsShell() &&
               playerCoreOutcome.target == CombatTarget::GovernmentCore &&
               playerCoreOutcome.impactKind == ImpactKind::None &&
               playerCoreOutcome.impactDetails.brickCount == 0 &&
               playerCoreOutcome.impactDetails.governmentWallIndex == -1 &&
               playerCoreOutcome.governmentCoreHealthBefore == 1 &&
               playerCoreOutcome.governmentCoreHealthAfter == 0 &&
               playerCoreOutcome.damagedGovernmentCore() &&
               playerCoreOutcome.destroyedGovernmentCore() &&
               playerCoreOutcome.owner == ShellOwner::Player &&
               playerCoreOutcome.ownerIndex == 1 && playerCoreOutcome.power &&
               samePosition(playerCoreOutcome.position,
                            kGovernmentBaseCenter) &&
               samePosition(playerCoreOutcome.incomingVelocity,
                            {0.0f, -8.0f}) &&
               !playerBaseAlive &&
               sameShellState(playerCoreShell, playerCoreBefore),
           "player core hit lost its source, transition, or deferred shell state");

    StageMap enemyCoreMap;
    enemyCoreMap.prepareShowcaseArena();
    bool enemyBaseAlive = true;
    Shell enemyCoreShell = testShell(kGovernmentBaseCenter,
                                    {8.0f, 0.0f}, ShellOwner::Enemy);
    enemyCoreShell.ownerIndex = 64;
    const Shell enemyCoreBefore = enemyCoreShell;
    const CombatOutcome enemyCoreMapOutcome =
        resolveShellMapImpact(enemyCoreMap, enemyCoreShell);
    const CombatOutcome enemyCoreOutcome =
        resolveShellGovernmentCoreImpact(enemyCoreMap, enemyBaseAlive,
                                         enemyCoreMapOutcome);
    expect(enemyCoreOutcome.stopsShell() &&
               enemyCoreOutcome.target == CombatTarget::GovernmentCore &&
               enemyCoreOutcome.governmentCoreHealthBefore == 1 &&
               enemyCoreOutcome.governmentCoreHealthAfter == 0 &&
               enemyCoreOutcome.owner == ShellOwner::Enemy &&
               enemyCoreOutcome.ownerIndex == 64 && !enemyCoreOutcome.power &&
               !enemyBaseAlive &&
               sameShellState(enemyCoreShell, enemyCoreBefore),
           "enemy core hit changed its one-hit or source semantics");

    StageMap deadCoreMap;
    deadCoreMap.prepareShowcaseArena();
    bool deadBaseAlive = false;
    Shell deadCoreShell = testShell(kGovernmentBaseCenter,
                                   {-8.0f, 0.0f}, ShellOwner::Enemy);
    const Shell deadCoreBefore = deadCoreShell;
    const CombatOutcome deadCoreMapOutcome =
        resolveShellMapImpact(deadCoreMap, deadCoreShell);
    const CombatOutcome deadCoreOutcome =
        resolveShellGovernmentCoreImpact(deadCoreMap, deadBaseAlive,
                                         deadCoreMapOutcome);
    expect(deadCoreOutcome.stopsShell() &&
               deadCoreOutcome.target == CombatTarget::GovernmentCore &&
               deadCoreOutcome.governmentCoreHealthBefore == 0 &&
               deadCoreOutcome.governmentCoreHealthAfter == 0 &&
               !deadCoreOutcome.damagedGovernmentCore() &&
               !deadCoreOutcome.destroyedGovernmentCore() &&
               !deadBaseAlive &&
               sameShellState(deadCoreShell, deadCoreBefore),
           "an already-destroyed core stopped absorbing shells or repeated damage");

    reporter.beginSuite("combat-government-core-boundary-and-order");
    StageMap insideCoreMap;
    insideCoreMap.prepareShowcaseArena();
    const bool insideWallsBreached = breachGovernmentWalls(insideCoreMap);
    bool insideBaseAlive = true;
    const XZ insideCorePosition{
        kGovernmentBaseCenter.x + kGovernmentCoreRadius + kShellHalfSize -
            0.001f,
        kGovernmentBaseCenter.z};
    Shell insideCoreShell = testShell(insideCorePosition, {-8.0f, 0.0f},
                                     ShellOwner::Enemy);
    const Shell insideCoreBefore = insideCoreShell;
    const CombatOutcome insideCoreMapOutcome =
        resolveShellMapImpact(insideCoreMap, insideCoreShell);
    const CombatOutcome insideCoreOutcome =
        resolveShellGovernmentCoreImpact(insideCoreMap, insideBaseAlive,
                                         insideCoreMapOutcome);
    expect(insideWallsBreached && !insideCoreMapOutcome.stopsShell() &&
               insideCoreOutcome.stopsShell() &&
               insideCoreOutcome.target == CombatTarget::GovernmentCore &&
               !insideBaseAlive &&
               sameShellState(insideCoreShell, insideCoreBefore),
           "strictly overlapping core edge no longer hits");

    StageMap outsideCoreMap;
    outsideCoreMap.prepareShowcaseArena();
    const bool outsideWallsBreached = breachGovernmentWalls(outsideCoreMap);
    bool outsideBaseAlive = true;
    const XZ outsideCorePosition{
        kGovernmentBaseCenter.x + kGovernmentCoreRadius + kShellHalfSize +
            0.001f,
        kGovernmentBaseCenter.z};
    Shell outsideCoreShell = testShell(outsideCorePosition, {-8.0f, 0.0f},
                                      ShellOwner::Enemy);
    const Shell outsideCoreBefore = outsideCoreShell;
    const CombatOutcome outsideCoreMapOutcome =
        resolveShellMapImpact(outsideCoreMap, outsideCoreShell);
    const CombatOutcome outsideCoreOutcome =
        resolveShellGovernmentCoreImpact(outsideCoreMap, outsideBaseAlive,
                                         outsideCoreMapOutcome);
    expect(outsideWallsBreached && !outsideCoreMapOutcome.stopsShell() &&
               !outsideCoreOutcome.stopsShell() && outsideBaseAlive &&
               sameShellState(outsideCoreShell, outsideCoreBefore),
           "a separated core edge became a hit");

    StageMap orderedCoreMap;
    orderedCoreMap.prepareShowcaseArena();
    bool orderedBaseAlive = true;
    Shell orderedCoreShell = testShell(kGovernmentBaseCenter,
                                      {0.0f, 8.0f}, ShellOwner::Enemy);
    const Shell orderedCoreBefore = orderedCoreShell;
    const CombatOutcome orderedMapOutcome =
        resolveShellMapImpact(orderedCoreMap, orderedCoreShell);
    CombatOutcome orderedCoreOutcome;
    if (!orderedMapOutcome.stopsShell())
        orderedCoreOutcome = resolveShellGovernmentCoreImpact(
            orderedCoreMap, orderedBaseAlive, orderedMapOutcome);
    expect(!orderedMapOutcome.stopsShell() &&
               orderedMapOutcome.target == CombatTarget::None &&
               orderedCoreOutcome.stopsShell() &&
               orderedCoreOutcome.target == CombatTarget::GovernmentCore &&
               !orderedBaseAlive &&
               sameShellState(orderedCoreShell, orderedCoreBefore),
           "map-to-core resolution order or deferred shell state changed");

    StageMap wallPriorityMap;
    wallPriorityMap.prepareShowcaseArena();
    bool wallPriorityBaseAlive = true;
    const GovernmentWallSegment priorityWall = governmentWallSegment(0);
    const XZ dualOverlapPosition =
        kGovernmentBaseCenter +
        priorityWall.outward *
            (kGovernmentCoreRadius + kShellHalfSize - 0.05f);
    Shell wallPriorityShell = testShell(dualOverlapPosition,
                                       priorityWall.outward * -8.0f,
                                       ShellOwner::Enemy);
    const Shell wallPriorityBefore = wallPriorityShell;
    const int priorityWallHealthBefore =
        wallPriorityMap.governmentWallHealth(0);
    const CombatOutcome wallPriorityOutcome =
        resolveShellMapImpact(wallPriorityMap, wallPriorityShell);
    const CombatOutcome suppressedCoreOutcome =
        resolveShellGovernmentCoreImpact(
            wallPriorityMap, wallPriorityBaseAlive, wallPriorityOutcome);
    expect(wallPriorityMap.shellHitsGovernmentCore(dualOverlapPosition) &&
               wallPriorityOutcome.mapStopsShell() &&
               wallPriorityOutcome.target == CombatTarget::StageMap &&
               wallPriorityOutcome.impactKind ==
                   ImpactKind::GovernmentWall &&
               wallPriorityMap.governmentWallHealth(0) ==
                   priorityWallHealthBefore - 1 &&
               suppressedCoreOutcome.mapStopsShell() &&
               suppressedCoreOutcome.target == CombatTarget::StageMap &&
               wallPriorityBaseAlive &&
               sameShellState(wallPriorityShell, wallPriorityBefore),
           "government wall stopped losing priority over an overlapping core");

    reporter.beginSuite("combat-player-shell-enemy-targeting");
    Shell enemyOwnedShell = testShell({8.0f, 8.0f}, {7.0f, -3.0f},
                                      ShellOwner::Enemy);
    enemyOwnedShell.ownerIndex = 70;
    std::vector<Enemy> enemyOwnerTargets{
        testEnemy(10, 0, 2, enemyOwnedShell.position)};
    const CombatOutcome enemyOwnerOutcome =
        evaluatePlayerShellEnemyTankImpact(
            enemyOwnerTargets, openEnvironmentOutcome(enemyOwnedShell));
    expect(!enemyOwnerOutcome.stopsShell() &&
               enemyOwnerOutcome.target == CombatTarget::None &&
               enemyOwnerTargets[0].armor == 2,
           "enemy-owned shell began damaging another enemy tank");

    Shell missedTankShell = testShell({8.0f, 8.0f}, {7.0f, -3.0f},
                                      ShellOwner::Player);
    std::vector<Enemy> missedTankTargets{
        testEnemy(11, 1, 2,
                  {8.0f, 8.0f + kShellTankHitExtent + 0.001f})};
    const CombatOutcome missedTankOutcome =
        evaluatePlayerShellEnemyTankImpact(
            missedTankTargets, openEnvironmentOutcome(missedTankShell));
    expect(!missedTankOutcome.stopsShell() &&
               missedTankTargets[0].armor == 2,
           "separated enemy tank became a player-shell hit");

    std::vector<Enemy> filteredTargets{
        testEnemy(12, 0, 2, missedTankShell.position),
        testEnemy(13, 1, 2, missedTankShell.position),
        testEnemy(14, 2, 0, missedTankShell.position),
        testEnemy(140, 3, 2, missedTankShell.position)};
    filteredTargets[0].destroyed = true;
    filteredTargets[1].creationTimer = 0.001f;
    const CombatOutcome filteredOutcome =
        evaluatePlayerShellEnemyTankImpact(
            filteredTargets, openEnvironmentOutcome(missedTankShell));
    expect(filteredOutcome.target == CombatTarget::EnemyTank &&
               filteredOutcome.targetEnemyIndex == 3 &&
               filteredOutcome.targetEnemyId == 140 &&
               filteredTargets[0].armor == 2 &&
               filteredTargets[1].armor == 2 &&
               filteredTargets[2].armor == 0 &&
               filteredTargets[3].armor == 2,
           "destroyed, creating, or empty-armor enemy blocked a live target");

    std::vector<Enemy> edgeOnlyTargets{
        testEnemy(15, 0, 2,
                  {8.0f + kShellTankHitExtent, 8.0f})};
    const CombatOutcome edgeOnlyOutcome =
        evaluatePlayerShellEnemyTankImpact(
            edgeOnlyTargets, openEnvironmentOutcome(missedTankShell));
    expect(!edgeOnlyOutcome.stopsShell() && edgeOnlyTargets[0].armor == 2,
           "edge-only enemy-tank contact became a hit");

    std::vector<Enemy> insideTargets{
        testEnemy(16, 0, 2,
                  {8.0f + kShellTankHitExtent - 0.001f,
                   8.0f - kShellTankHitExtent + 0.001f})};
    const CombatOutcome insideOutcome =
        evaluatePlayerShellEnemyTankImpact(
            insideTargets, openEnvironmentOutcome(missedTankShell));
    expect(insideOutcome.stopsShell() &&
               insideOutcome.target == CombatTarget::EnemyTank &&
               insideOutcome.targetEnemyId == 16 &&
               insideTargets[0].armor == 2,
           "strictly overlapping enemy-tank corner stopped being a hit");

    StageMap blockedTankMap;
    blockedTankMap.prepareShowcaseArena();
    const GovernmentWallSegment blockedTankWall = governmentWallSegment(0);
    Shell blockedTankShell = testShell(blockedTankWall.center,
                                      blockedTankWall.outward * -8.0f,
                                      ShellOwner::Player);
    const CombatOutcome blockedTankMapOutcome =
        resolveShellMapImpact(blockedTankMap, blockedTankShell);
    bool blockedTankBaseAlive = true;
    const CombatOutcome blockedTankCoreOutcome =
        resolveShellGovernmentCoreImpact(
            blockedTankMap, blockedTankBaseAlive, blockedTankMapOutcome);
    std::vector<Enemy> blockedTankTargets{
        testEnemy(17, 0, 2, blockedTankShell.position)};
    const CombatOutcome blockedTankOutcome =
        evaluatePlayerShellEnemyTankImpact(blockedTankTargets,
                                           blockedTankCoreOutcome);

    StageMap corePriorityMap;
    corePriorityMap.prepareShowcaseArena();
    bool corePriorityBaseAlive = true;
    Shell corePriorityShell = testShell(kGovernmentBaseCenter,
                                       {0.0f, 8.0f},
                                       ShellOwner::Player);
    const CombatOutcome corePriorityMapOutcome =
        resolveShellMapImpact(corePriorityMap, corePriorityShell);
    const CombatOutcome corePriorityOutcome =
        resolveShellGovernmentCoreImpact(
            corePriorityMap, corePriorityBaseAlive,
            corePriorityMapOutcome);
    std::vector<Enemy> corePriorityTargets{
        testEnemy(18, 0, 2, corePriorityShell.position)};
    const CombatOutcome suppressedByCoreOutcome =
        evaluatePlayerShellEnemyTankImpact(corePriorityTargets,
                                           corePriorityOutcome);
    expect(blockedTankMapOutcome.mapStopsShell() &&
               blockedTankOutcome.mapStopsShell() &&
               blockedTankOutcome.target == CombatTarget::StageMap &&
               blockedTankOutcome.targetEnemyIndex == -1 &&
               blockedTankTargets[0].armor == 2 &&
               corePriorityOutcome.target ==
                   CombatTarget::GovernmentCore &&
               suppressedByCoreOutcome.stopsShell() &&
               suppressedByCoreOutcome.target ==
                   CombatTarget::GovernmentCore &&
               suppressedByCoreOutcome.targetEnemyIndex == -1 &&
               corePriorityTargets[0].armor == 2,
           "map or core impact stopped suppressing a later enemy-tank hit");

    std::vector<Enemy> orderedTargets{
        testEnemy(19, 0, 2, missedTankShell.position),
        testEnemy(20, 1, 2, missedTankShell.position)};
    const CombatOutcome firstOrderedOutcome =
        evaluatePlayerShellEnemyTankImpact(
            orderedTargets, openEnvironmentOutcome(missedTankShell));
    std::reverse(orderedTargets.begin(), orderedTargets.end());
    const CombatOutcome reversedOrderedOutcome =
        evaluatePlayerShellEnemyTankImpact(
            orderedTargets, openEnvironmentOutcome(missedTankShell));
    expect(firstOrderedOutcome.targetEnemyId == 19 &&
               reversedOrderedOutcome.targetEnemyId == 20 &&
               orderedTargets[0].armor == 2 && orderedTargets[1].armor == 2,
           "overlapping enemy selection stopped following vector order");

    reporter.beginSuite("combat-player-shell-enemy-outcomes");
    Shell armoredShell = testShell({8.0f, 8.0f}, {7.0f, -3.0f},
                                   ShellOwner::Player);
    armoredShell.ownerIndex = 1;
    armoredShell.power = true;
    armoredShell.life = 2.75f;
    const Shell armoredShellBefore = armoredShell;
    const XZ armoredTargetPosition{8.5f, 7.5f};
    std::vector<Enemy> armoredTargets{
        testEnemy(21, 3, 2, armoredTargetPosition)};
    armoredTargets[0].carriesBonus = true;
    const CombatOutcome armoredOutcome =
        evaluatePlayerShellEnemyTankImpact(
            armoredTargets, openEnvironmentOutcome(armoredShell));
    expect(armoredOutcome.stopsShell() &&
               armoredOutcome.target == CombatTarget::EnemyTank &&
               armoredOutcome.targetEnemyIndex == 0 &&
               armoredOutcome.targetEnemyId == 21 &&
               armoredOutcome.targetEnemyType == 3 &&
               samePosition(armoredOutcome.targetEnemyPosition,
                            armoredTargetPosition) &&
               armoredOutcome.targetEnemyCarriedBonus &&
               armoredOutcome.targetEnemyArmorBefore == 2 &&
               armoredOutcome.targetEnemyArmorAfter == 1 &&
               armoredOutcome.damagedEnemyTank() &&
               !armoredOutcome.destroyedEnemyTank() &&
               armoredOutcome.owner == ShellOwner::Player &&
               armoredOutcome.ownerIndex == 1 && armoredOutcome.power &&
               samePosition(armoredOutcome.position,
                            armoredShell.position) &&
               samePosition(armoredOutcome.incomingVelocity,
                            armoredShell.velocity) &&
               armoredTargets[0].armor == 2 &&
               !armoredTargets[0].destroyed && armoredTargets[0].moving &&
               sameShellState(armoredShell, armoredShellBefore),
           "nonfatal enemy outcome lost source, target, or read-only semantics");

    std::vector<Enemy> fatalTargets{
        testEnemy(22, 2, 1, armoredShell.position)};
    const CombatOutcome fatalOutcome =
        evaluatePlayerShellEnemyTankImpact(
            fatalTargets, openEnvironmentOutcome(armoredShell));
    expect(fatalOutcome.damagedEnemyTank() &&
               fatalOutcome.destroyedEnemyTank() &&
               fatalOutcome.targetEnemyArmorBefore == 1 &&
               fatalOutcome.targetEnemyArmorAfter == 0 &&
               fatalTargets[0].armor == 1 &&
               !fatalTargets[0].destroyed && fatalTargets[0].moving &&
               nearlyEqual(fatalTargets[0].deathTimer, 0.0f),
           "fatal enemy outcome changed caller-owned death state");

    Shell invalidOwnerShell = armoredShell;
    invalidOwnerShell.ownerIndex = 99;
    std::vector<Enemy> invalidOwnerTargets{
        testEnemy(23, 1, 2, invalidOwnerShell.position)};
    const CombatOutcome invalidOwnerOutcome =
        evaluatePlayerShellEnemyTankImpact(
            invalidOwnerTargets,
            openEnvironmentOutcome(invalidOwnerShell));
    expect(invalidOwnerOutcome.stopsShell() &&
               invalidOwnerOutcome.ownerIndex == 99 &&
               invalidOwnerOutcome.targetEnemyArmorBefore == 2 &&
               invalidOwnerOutcome.targetEnemyArmorAfter == 1 &&
               invalidOwnerTargets[0].armor == 2,
           "invalid player owner stopped an otherwise valid armor hit");

    std::vector<Enemy> carrierTargets{
        testEnemy(24, 3, 2, armoredShell.position)};
    carrierTargets[0].carriesBonus = true;
    const CombatOutcome firstCarrierOutcome =
        evaluatePlayerShellEnemyTankImpact(
            carrierTargets, openEnvironmentOutcome(armoredShell));
    carrierTargets[0].armor = firstCarrierOutcome.targetEnemyArmorAfter;
    const CombatOutcome secondCarrierOutcome =
        evaluatePlayerShellEnemyTankImpact(
            carrierTargets, openEnvironmentOutcome(armoredShell));
    expect(firstCarrierOutcome.targetEnemyCarriedBonus &&
               secondCarrierOutcome.targetEnemyCarriedBonus &&
               firstCarrierOutcome.targetEnemyArmorBefore == 2 &&
               firstCarrierOutcome.targetEnemyArmorAfter == 1 &&
               secondCarrierOutcome.targetEnemyArmorBefore == 1 &&
               secondCarrierOutcome.targetEnemyArmorAfter == 0 &&
               secondCarrierOutcome.destroyedEnemyTank() &&
               carrierTargets[0].carriesBonus,
           "armored carrier stopped requesting a bonus on every direct hit");

    Shell deferredEnemyShell = armoredShell;
    beginShellImpact(deferredEnemyShell, armoredOutcome.position);
    expect(sameShellState(armoredShell, armoredShellBefore) &&
               deferredEnemyShell.impacting &&
               samePosition(deferredEnemyShell.position,
                            armoredOutcome.position) &&
               samePosition(deferredEnemyShell.velocity, {}) &&
               nearlyEqual(deferredEnemyShell.life,
                           kShellImpactDuration),
           "enemy evaluator began owning the deferred shell impact transition");

    reporter.beginSuite("combat-player-shell-enemy-commit");
    Shell commitShell = armoredShell;
    commitShell.ownerIndex = 1;
    std::vector<Player> commitPlayers(2);
    commitPlayers[0].id = 0;
    commitPlayers[1].id = 1;
    std::vector<Enemy> commitTargets{
        testEnemy(31, 3, 2, armoredTargetPosition)};
    commitTargets[0].carriesBonus = true;
    const CombatOutcome commitOutcome =
        evaluatePlayerShellEnemyTankImpact(
            commitTargets, openEnvironmentOutcome(commitShell));
    int carrierCallbacks = 0;
    bool carrierCallbackSawPreCommitState = false;
    const EnemyTankImpactCommit nonfatalCommit =
        commitPlayerShellEnemyTankImpact(
            commitTargets, commitPlayers, commitOutcome,
            [&](const Enemy &carrier) {
                ++carrierCallbacks;
                carrierCallbackSawPreCommitState =
                    carrier.id == 31 && carrier.armor == 2 &&
                    carrier.carriesBonus && !carrier.destroyed;
            });
    expect(carrierCallbacks == 1 && carrierCallbackSawPreCommitState,
           "carrier callback stopped preceding the first armor commit");
    expect(nonfatalCommit.applied && !nonfatalCommit.destroyedNow &&
               nonfatalCommit.creditedPlayerIndex == 1 &&
               nonfatalCommit.eventPoints == kDirectEnemyHitPoints,
           "nonfatal commit result lost attribution or event points");
    expect(commitTargets[0].armor == 1 &&
               !commitTargets[0].destroyed && commitTargets[0].moving &&
               nearlyEqual(commitTargets[0].deathTimer, 0.0f) &&
               commitTargets[0].carriesBonus,
           "nonfatal commit changed unrelated enemy lifecycle state");
    expect(commitPlayers[0].score == 0 && commitPlayers[1].score == 50 &&
               commitPlayers[1].stageTally.enemyPoints[3] == 50 &&
               commitPlayers[1].stageTally.destroyed[3] == 0 &&
               commitPlayers[1].directKillStreak == 0,
           "nonfatal commit changed direct-hit score or classified tally");

    Shell noCallbackShell = commitShell;
    noCallbackShell.ownerIndex = -1;
    std::vector<Player> noCallbackPlayers;
    std::vector<Enemy> noCallbackTargets{
        testEnemy(36, 2, 2, noCallbackShell.position)};
    noCallbackTargets[0].carriesBonus = true;
    const CombatOutcome noCallbackOutcome =
        evaluatePlayerShellEnemyTankImpact(
            noCallbackTargets, openEnvironmentOutcome(noCallbackShell));
    const EnemyTankImpactCommit noCallbackCommit =
        commitPlayerShellEnemyTankImpact(
            noCallbackTargets, noCallbackPlayers, noCallbackOutcome);
    expect(noCallbackCommit.applied && !noCallbackCommit.destroyedNow &&
               noCallbackTargets[0].armor == 1 &&
               noCallbackTargets[0].carriesBonus,
           "optional carrier callback became required for armor commit");

    Shell fatalCommitShell = armoredShell;
    fatalCommitShell.ownerIndex = 0;
    std::vector<Player> fatalCommitPlayers(1);
    std::vector<Enemy> fatalCommitTargets{
        testEnemy(32, 2, 1, fatalCommitShell.position)};
    const CombatOutcome fatalCommitOutcome =
        evaluatePlayerShellEnemyTankImpact(
            fatalCommitTargets, openEnvironmentOutcome(fatalCommitShell));
    int noncarrierCallbacks = 0;
    const EnemyTankImpactCommit fatalCommit =
        commitPlayerShellEnemyTankImpact(
            fatalCommitTargets, fatalCommitPlayers, fatalCommitOutcome,
            [&](const Enemy &) { ++noncarrierCallbacks; });
    expect(fatalCommit.applied && fatalCommit.destroyedNow &&
               fatalCommit.creditedPlayerIndex == 0 &&
               noncarrierCallbacks == 0 &&
               fatalCommitTargets[0].armor == 0 &&
               fatalCommitTargets[0].destroyed &&
               !fatalCommitTargets[0].moving &&
               nearlyEqual(fatalCommitTargets[0].deathTimer,
                           kTankDeathDuration),
           "fatal commit lost the 490 ms enemy destruction state");
    expect(fatalCommitPlayers[0].score == 50 &&
               fatalCommitPlayers[0].stageTally.enemyPoints[2] == 50 &&
               fatalCommitPlayers[0].stageTally.destroyed[2] == 1 &&
               fatalCommitPlayers[0].directKillStreak == 1 &&
               nearlyEqual(fatalCommitPlayers[0].streakPopupTimer,
                           kStreakPopupDuration),
           "fatal commit changed classified score or active-player streak");

    std::vector<Player> inactiveCommitPlayers(1);
    inactiveCommitPlayers[0].active = false;
    inactiveCommitPlayers[0].directKillStreak = 2;
    inactiveCommitPlayers[0].streakPopupTimer = 0.25f;
    std::vector<Enemy> inactiveCommitTargets{
        testEnemy(33, 1, 1, fatalCommitShell.position)};
    const CombatOutcome inactiveCommitOutcome =
        evaluatePlayerShellEnemyTankImpact(
            inactiveCommitTargets, openEnvironmentOutcome(fatalCommitShell));
    const EnemyTankImpactCommit inactiveCommit =
        commitPlayerShellEnemyTankImpact(
            inactiveCommitTargets, inactiveCommitPlayers,
            inactiveCommitOutcome);
    expect(inactiveCommit.applied && inactiveCommit.destroyedNow &&
               inactiveCommitPlayers[0].score == 50 &&
               inactiveCommitPlayers[0].stageTally.enemyPoints[1] == 50 &&
               inactiveCommitPlayers[0].stageTally.destroyed[1] == 1 &&
               inactiveCommitPlayers[0].directKillStreak == 2 &&
               nearlyEqual(inactiveCommitPlayers[0].streakPopupTimer,
                           0.25f),
           "posthumous shell stopped scoring without starting a streak");

    Shell invalidCommitShell = fatalCommitShell;
    invalidCommitShell.ownerIndex = -1;
    std::vector<Player> invalidCommitPlayers(1);
    std::vector<Enemy> negativeOwnerTargets{
        testEnemy(34, 0, 1, invalidCommitShell.position)};
    const CombatOutcome negativeOwnerOutcome =
        evaluatePlayerShellEnemyTankImpact(
            negativeOwnerTargets,
            openEnvironmentOutcome(invalidCommitShell));
    const EnemyTankImpactCommit negativeOwnerCommit =
        commitPlayerShellEnemyTankImpact(
            negativeOwnerTargets, invalidCommitPlayers,
            negativeOwnerOutcome);
    invalidCommitShell.ownerIndex = 99;
    std::vector<Enemy> largeOwnerTargets{
        testEnemy(35, 0, 2, invalidCommitShell.position)};
    const CombatOutcome largeOwnerOutcome =
        evaluatePlayerShellEnemyTankImpact(
            largeOwnerTargets, openEnvironmentOutcome(invalidCommitShell));
    const EnemyTankImpactCommit largeOwnerCommit =
        commitPlayerShellEnemyTankImpact(
            largeOwnerTargets, invalidCommitPlayers, largeOwnerOutcome);
    expect(negativeOwnerCommit.applied &&
               negativeOwnerCommit.destroyedNow &&
               negativeOwnerCommit.creditedPlayerIndex == -1 &&
               negativeOwnerTargets[0].destroyed &&
               largeOwnerCommit.applied &&
               !largeOwnerCommit.destroyedNow &&
               largeOwnerCommit.creditedPlayerIndex == -1 &&
               largeOwnerTargets[0].armor == 1 &&
               invalidCommitPlayers[0].score == 0 &&
               invalidCommitPlayers[0].stageTally.totalEnemyPoints() == 0,
           "invalid owner stopped physical damage or credited a player");

    reporter.beginSuite("combat-player-shell-enemy-commit-validation");
    Shell validationShell = armoredShell;
    validationShell.ownerIndex = 0;
    std::vector<Enemy> validationTargets{
        testEnemy(41, 3, 2, validationShell.position)};
    validationTargets[0].carriesBonus = true;
    const CombatOutcome validationOutcome =
        evaluatePlayerShellEnemyTankImpact(
            validationTargets, openEnvironmentOutcome(validationShell));
    std::vector<Player> validationPlayers(1);
    int rejectedCarrierCallbacks = 0;
    const auto rejectedCarrier = [&](const Enemy &) {
        ++rejectedCarrierCallbacks;
    };

    CombatOutcome wrongTargetOutcome = validationOutcome;
    wrongTargetOutcome.target = CombatTarget::None;
    const EnemyTankImpactCommit wrongTargetCommit =
        commitPlayerShellEnemyTankImpact(
            validationTargets, validationPlayers, wrongTargetOutcome,
            rejectedCarrier);
    CombatOutcome wrongOwnerOutcome = validationOutcome;
    wrongOwnerOutcome.owner = ShellOwner::Enemy;
    const EnemyTankImpactCommit wrongOwnerCommit =
        commitPlayerShellEnemyTankImpact(
            validationTargets, validationPlayers, wrongOwnerOutcome,
            rejectedCarrier);
    expect(!wrongTargetCommit.applied && !wrongOwnerCommit.applied &&
               validationTargets[0].armor == 2 &&
               validationPlayers[0].score == 0 &&
               rejectedCarrierCallbacks == 0,
           "non-enemy or enemy-owned outcome became a state commit");

    CombatOutcome negativeIndexOutcome = validationOutcome;
    negativeIndexOutcome.targetEnemyIndex = -1;
    CombatOutcome largeIndexOutcome = validationOutcome;
    largeIndexOutcome.targetEnemyIndex = 1;
    const EnemyTankImpactCommit negativeIndexCommit =
        commitPlayerShellEnemyTankImpact(
            validationTargets, validationPlayers, negativeIndexOutcome,
            rejectedCarrier);
    const EnemyTankImpactCommit largeIndexCommit =
        commitPlayerShellEnemyTankImpact(
            validationTargets, validationPlayers, largeIndexOutcome,
            rejectedCarrier);
    expect(!negativeIndexCommit.applied && !largeIndexCommit.applied &&
               validationTargets[0].armor == 2 &&
               rejectedCarrierCallbacks == 0,
           "out-of-range target index became a state commit");

    std::vector<Enemy> wrongIdTargets = validationTargets;
    wrongIdTargets[0].id = 42;
    std::vector<Enemy> wrongTypeTargets = validationTargets;
    wrongTypeTargets[0].type = 2;
    std::vector<Enemy> wrongCarrierTargets = validationTargets;
    wrongCarrierTargets[0].carriesBonus = false;
    const EnemyTankImpactCommit wrongIdCommit =
        commitPlayerShellEnemyTankImpact(
            wrongIdTargets, validationPlayers, validationOutcome,
            rejectedCarrier);
    const EnemyTankImpactCommit wrongTypeCommit =
        commitPlayerShellEnemyTankImpact(
            wrongTypeTargets, validationPlayers, validationOutcome,
            rejectedCarrier);
    const EnemyTankImpactCommit wrongCarrierCommit =
        commitPlayerShellEnemyTankImpact(
            wrongCarrierTargets, validationPlayers, validationOutcome,
            rejectedCarrier);
    expect(!wrongIdCommit.applied && !wrongTypeCommit.applied &&
               !wrongCarrierCommit.applied &&
               wrongIdTargets[0].armor == 2 &&
               wrongTypeTargets[0].armor == 2 &&
               wrongCarrierTargets[0].armor == 2 &&
               validationPlayers[0].score == 0 &&
               rejectedCarrierCallbacks == 0,
           "stale enemy identity, type, or carrier snapshot was committed");

    std::vector<Enemy> staleArmorTargets = validationTargets;
    staleArmorTargets[0].armor = 1;
    std::vector<Enemy> destroyedTargets = validationTargets;
    destroyedTargets[0].destroyed = true;
    std::vector<Enemy> creatingTargets = validationTargets;
    creatingTargets[0].creationTimer = 0.01f;
    const EnemyTankImpactCommit staleArmorCommit =
        commitPlayerShellEnemyTankImpact(
            staleArmorTargets, validationPlayers, validationOutcome,
            rejectedCarrier);
    const EnemyTankImpactCommit destroyedCommit =
        commitPlayerShellEnemyTankImpact(
            destroyedTargets, validationPlayers, validationOutcome,
            rejectedCarrier);
    const EnemyTankImpactCommit creatingCommit =
        commitPlayerShellEnemyTankImpact(
            creatingTargets, validationPlayers, validationOutcome,
            rejectedCarrier);
    expect(!staleArmorCommit.applied && !destroyedCommit.applied &&
               !creatingCommit.applied &&
               staleArmorTargets[0].armor == 1 &&
               destroyedTargets[0].armor == 2 &&
               creatingTargets[0].armor == 2 &&
               validationPlayers[0].score == 0 &&
               rejectedCarrierCallbacks == 0,
           "stale armor or ineligible live state was committed");

    CombatOutcome skippedArmorOutcome = validationOutcome;
    skippedArmorOutcome.targetEnemyArmorAfter = 0;
    CombatOutcome emptyArmorOutcome = validationOutcome;
    emptyArmorOutcome.targetEnemyArmorBefore = 0;
    emptyArmorOutcome.targetEnemyArmorAfter = -1;
    const EnemyTankImpactCommit skippedArmorCommit =
        commitPlayerShellEnemyTankImpact(
            validationTargets, validationPlayers, skippedArmorOutcome,
            rejectedCarrier);
    const EnemyTankImpactCommit emptyArmorCommit =
        commitPlayerShellEnemyTankImpact(
            validationTargets, validationPlayers, emptyArmorOutcome,
            rejectedCarrier);
    expect(!skippedArmorCommit.applied && !emptyArmorCommit.applied &&
               validationTargets[0].armor == 2 &&
               validationPlayers[0].score == 0 &&
               rejectedCarrierCallbacks == 0,
           "malformed one-layer armor transition was committed");

    int repeatCarrierCallbacks = 0;
    std::vector<Enemy> repeatTargets = validationTargets;
    std::vector<Player> repeatPlayers(1);
    const EnemyTankImpactCommit firstRepeatCommit =
        commitPlayerShellEnemyTankImpact(
            repeatTargets, repeatPlayers, validationOutcome,
            [&](const Enemy &) { ++repeatCarrierCallbacks; });
    const EnemyTankImpactCommit secondRepeatCommit =
        commitPlayerShellEnemyTankImpact(
            repeatTargets, repeatPlayers, validationOutcome,
            [&](const Enemy &) { ++repeatCarrierCallbacks; });
    expect(firstRepeatCommit.applied && !secondRepeatCommit.applied &&
               repeatCarrierCallbacks == 1 && repeatTargets[0].armor == 1 &&
               !repeatTargets[0].destroyed &&
               repeatPlayers[0].score == 50 &&
               repeatPlayers[0].stageTally.enemyPoints[3] == 50 &&
               repeatPlayers[0].stageTally.destroyed[3] == 0,
           "repeated outcome generated another bonus, damage, or score");

    reporter.beginSuite("combat-enemy-shell-player-targeting");
    Shell playerOwnedTargetShell = testShell(
        {9.0f, 9.0f}, {-4.0f, 2.0f}, ShellOwner::Player);
    std::vector<Player> playerOwnedTargets{
        testPlayer(0, 3, playerOwnedTargetShell.position)};
    const CombatOutcome playerOwnedTargetOutcome =
        evaluateEnemyShellPlayerTankImpact(
            playerOwnedTargets,
            openEnvironmentOutcome(playerOwnedTargetShell));
    expect(playerOwnedTargetOutcome.target == CombatTarget::None &&
               playerOwnedTargetOutcome.targetPlayerIndex == -1 &&
               playerOwnedTargets[0].hitPoints == 3,
           "player-owned shell began targeting a player tank");

    Shell enemyTargetShell = testShell(
        {9.0f, 9.0f}, {-4.0f, 2.0f}, ShellOwner::Enemy);
    enemyTargetShell.ownerIndex = 77;
    std::vector<Player> missedPlayerTargets{
        testPlayer(0, 3,
                   {9.0f + kShellTankHitExtent + 0.001f, 9.0f})};
    const CombatOutcome missedPlayerOutcome =
        evaluateEnemyShellPlayerTankImpact(
            missedPlayerTargets, openEnvironmentOutcome(enemyTargetShell));
    expect(missedPlayerOutcome.target == CombatTarget::None &&
               missedPlayerTargets[0].hitPoints == 3,
           "separated player tank became an enemy-shell hit");

    std::vector<Player> filteredPlayers{
        testPlayer(0, 3, enemyTargetShell.position),
        testPlayer(1, 3, enemyTargetShell.position),
        testPlayer(2, 0, enemyTargetShell.position),
        testPlayer(3, 2, enemyTargetShell.position)};
    filteredPlayers[0].active = false;
    filteredPlayers[1].creationTimer = 0.001f;
    const CombatOutcome filteredPlayerOutcome =
        evaluateEnemyShellPlayerTankImpact(
            filteredPlayers, openEnvironmentOutcome(enemyTargetShell));
    expect(filteredPlayerOutcome.target == CombatTarget::PlayerTank &&
               filteredPlayerOutcome.targetPlayerIndex == 3 &&
               filteredPlayerOutcome.targetPlayerId == 3 &&
               filteredPlayerOutcome.targetPlayerHitPointsBefore == 2 &&
               filteredPlayers[0].hitPoints == 3 &&
               filteredPlayers[1].hitPoints == 3 &&
               filteredPlayers[2].hitPoints == 0 &&
               filteredPlayers[3].hitPoints == 2,
           "inactive, creating, or empty-HP player blocked a live target");

    std::vector<Player> edgePlayerTargets{
        testPlayer(4, 3,
                   {9.0f + kShellTankHitExtent, 9.0f})};
    const CombatOutcome edgePlayerOutcome =
        evaluateEnemyShellPlayerTankImpact(
            edgePlayerTargets, openEnvironmentOutcome(enemyTargetShell));
    std::vector<Player> insidePlayerTargets{
        testPlayer(5, 3,
                   {9.0f + kShellTankHitExtent - 0.001f,
                    9.0f - kShellTankHitExtent + 0.001f})};
    const CombatOutcome insidePlayerOutcome =
        evaluateEnemyShellPlayerTankImpact(
            insidePlayerTargets, openEnvironmentOutcome(enemyTargetShell));
    expect(edgePlayerOutcome.target == CombatTarget::None &&
               insidePlayerOutcome.target == CombatTarget::PlayerTank &&
               insidePlayerOutcome.targetPlayerId == 5,
           "strict player-tank AABB edge semantics changed");

    std::vector<Player> blockedPlayerTargets{
        testPlayer(6, 3, blockedTankShell.position)};
    const CombatOutcome blockedPlayerOutcome =
        evaluateEnemyShellPlayerTankImpact(blockedPlayerTargets,
                                           blockedTankOutcome);
    std::vector<Player> corePlayerTargets{
        testPlayer(7, 3, corePriorityShell.position)};
    const CombatOutcome corePlayerOutcome =
        evaluateEnemyShellPlayerTankImpact(corePlayerTargets,
                                           corePriorityOutcome);
    expect(blockedPlayerOutcome.target == CombatTarget::StageMap &&
               blockedPlayerOutcome.targetPlayerIndex == -1 &&
               corePlayerOutcome.target == CombatTarget::GovernmentCore &&
               corePlayerOutcome.targetPlayerIndex == -1 &&
               blockedPlayerTargets[0].hitPoints == 3 &&
               corePlayerTargets[0].hitPoints == 3,
           "map or core impact stopped suppressing a player-tank hit");

    std::vector<Player> orderedPlayers{
        testPlayer(8, 3, enemyTargetShell.position),
        testPlayer(9, 3, enemyTargetShell.position)};
    const CombatOutcome firstOrderedPlayerOutcome =
        evaluateEnemyShellPlayerTankImpact(
            orderedPlayers, openEnvironmentOutcome(enemyTargetShell));
    std::reverse(orderedPlayers.begin(), orderedPlayers.end());
    const CombatOutcome reversedOrderedPlayerOutcome =
        evaluateEnemyShellPlayerTankImpact(
            orderedPlayers, openEnvironmentOutcome(enemyTargetShell));
    expect(firstOrderedPlayerOutcome.targetPlayerId == 8 &&
               reversedOrderedPlayerOutcome.targetPlayerId == 9 &&
               orderedPlayers[0].hitPoints == 3 &&
               orderedPlayers[1].hitPoints == 3,
           "overlapping player selection stopped following vector order");

    std::vector<Player> protectedSnapshotPlayers{
        testPlayer(10, 2, {9.5f, 8.5f})};
    protectedSnapshotPlayers[0].shieldTimer = 1.25f;
    protectedSnapshotPlayers[0].hasBoat = true;
    const CombatOutcome protectedSnapshotOutcome =
        evaluateEnemyShellPlayerTankImpact(
            protectedSnapshotPlayers,
            openEnvironmentOutcome(enemyTargetShell));
    expect(protectedSnapshotOutcome.target == CombatTarget::PlayerTank &&
               protectedSnapshotOutcome.owner == ShellOwner::Enemy &&
               protectedSnapshotOutcome.ownerIndex == 77 &&
               samePosition(protectedSnapshotOutcome.position,
                            enemyTargetShell.position) &&
               samePosition(protectedSnapshotOutcome.targetPlayerPosition,
                            protectedSnapshotPlayers[0].position) &&
               protectedSnapshotOutcome.targetPlayerHitPointsBefore == 2 &&
               protectedSnapshotOutcome.targetPlayerShielded &&
               protectedSnapshotOutcome.targetPlayerHadBoat &&
               protectedSnapshotPlayers[0].hitPoints == 2 &&
               protectedSnapshotPlayers[0].hasBoat,
           "player outcome lost source, target, protection, or read-only state");

    reporter.beginSuite("combat-enemy-shell-player-commit");
    std::vector<Player> shieldCommitPlayers{
        testPlayer(0, 3, enemyTargetShell.position)};
    shieldCommitPlayers[0].shieldTimer = 2.0f;
    shieldCommitPlayers[0].hasBoat = true;
    shieldCommitPlayers[0].directKillStreak = 4;
    shieldCommitPlayers[0].streakPopupTimer = 0.35f;
    shieldCommitPlayers[0].deathTimer = 0.2f;
    shieldCommitPlayers[0].respawnTimer = 0.7f;
    const CombatOutcome shieldCommitOutcome =
        evaluateEnemyShellPlayerTankImpact(
            shieldCommitPlayers, openEnvironmentOutcome(enemyTargetShell));
    int shieldPreCommitCalls = 0;
    bool shieldCallbackSawPreCommit = false;
    const PlayerTankImpactCommit shieldCommit =
        commitEnemyShellPlayerTankImpact(
            shieldCommitPlayers, shieldCommitOutcome,
            [&](const Player &player) {
                ++shieldPreCommitCalls;
                shieldCallbackSawPreCommit =
                    player.hitPoints == 3 && player.shieldTimer == 2.0f &&
                    player.hasBoat && player.active;
            });
    expect(shieldCommit.applied &&
               shieldCommit.hitResult == PlayerHitResult::Shielded &&
               shieldCommit.hitPointsBefore == 3 &&
               shieldCommit.hitPointsAfter == 3 &&
               shieldPreCommitCalls == 1 && shieldCallbackSawPreCommit &&
               shieldCommitPlayers[0].hitPoints == 3 &&
               shieldCommitPlayers[0].hasBoat &&
               shieldCommitPlayers[0].active &&
               shieldCommitPlayers[0].moving &&
               nearlyEqual(shieldCommitPlayers[0].shieldTimer, 2.0f) &&
               shieldCommitPlayers[0].directKillStreak == 4 &&
               nearlyEqual(shieldCommitPlayers[0].streakPopupTimer, 0.35f) &&
               nearlyEqual(shieldCommitPlayers[0].deathTimer, 0.2f) &&
               nearlyEqual(shieldCommitPlayers[0].respawnTimer, 0.7f),
           "shielded commit changed Boat, HP, streak, or lifecycle state");

    std::vector<Player> boatCommitPlayers{
        testPlayer(1, 2, enemyTargetShell.position)};
    boatCommitPlayers[0].hasBoat = true;
    boatCommitPlayers[0].directKillStreak = 3;
    const CombatOutcome boatCommitOutcome =
        evaluateEnemyShellPlayerTankImpact(
            boatCommitPlayers, openEnvironmentOutcome(enemyTargetShell));
    int boatPreCommitCalls = 0;
    const PlayerTankImpactCommit boatCommit =
        commitEnemyShellPlayerTankImpact(
            boatCommitPlayers, boatCommitOutcome,
            [&](const Player &player) {
                if (player.hasBoat && player.hitPoints == 2)
                    ++boatPreCommitCalls;
            });
    expect(boatCommit.applied &&
               boatCommit.hitResult == PlayerHitResult::BoatAbsorbed &&
               boatCommit.hitPointsBefore == 2 &&
               boatCommit.hitPointsAfter == 2 &&
               boatPreCommitCalls == 1 &&
               !boatCommitPlayers[0].hasBoat &&
               boatCommitPlayers[0].hitPoints == 2 &&
               boatCommitPlayers[0].active &&
               boatCommitPlayers[0].moving &&
               nearlyEqual(boatCommitPlayers[0].shieldTimer, 0.0f) &&
               boatCommitPlayers[0].directKillStreak == 3,
           "Boat commit changed HP, streak, or active state");

    std::vector<Player> damageCommitPlayers{
        testPlayer(2, 3, enemyTargetShell.position)};
    damageCommitPlayers[0].directKillStreak = 2;
    damageCommitPlayers[0].streakPopupTimer = 0.4f;
    const CombatOutcome damageCommitOutcome =
        evaluateEnemyShellPlayerTankImpact(
            damageCommitPlayers, openEnvironmentOutcome(enemyTargetShell));
    const PlayerTankImpactCommit damageCommit =
        commitEnemyShellPlayerTankImpact(damageCommitPlayers,
                                         damageCommitOutcome);
    expect(damageCommit.applied &&
               damageCommit.hitResult == PlayerHitResult::Damaged &&
               damageCommit.hitPointsBefore == 3 &&
               damageCommit.hitPointsAfter == 2 &&
               damageCommitPlayers[0].hitPoints == 2 &&
               damageCommitPlayers[0].active &&
               damageCommitPlayers[0].moving &&
               damageCommitPlayers[0].directKillStreak == 2 &&
               nearlyEqual(damageCommitPlayers[0].streakPopupTimer, 0.4f) &&
               nearlyEqual(damageCommitPlayers[0].deathTimer, 0.0f),
           "nonfatal player commit changed lifecycle or streak state");

    std::vector<Player> fatalPlayerCommitPlayers{
        testPlayer(3, 1, enemyTargetShell.position)};
    fatalPlayerCommitPlayers[0].lives = 5;
    fatalPlayerCommitPlayers[0].level = 3;
    fatalPlayerCommitPlayers[0].directKillStreak = 6;
    fatalPlayerCommitPlayers[0].streakPopupTimer = 0.8f;
    fatalPlayerCommitPlayers[0].respawnTimer = 1.5f;
    const CombatOutcome fatalPlayerCommitOutcome =
        evaluateEnemyShellPlayerTankImpact(
            fatalPlayerCommitPlayers,
            openEnvironmentOutcome(enemyTargetShell));
    const PlayerTankImpactCommit fatalPlayerCommit =
        commitEnemyShellPlayerTankImpact(
            fatalPlayerCommitPlayers, fatalPlayerCommitOutcome);
    expect(fatalPlayerCommit.applied &&
               fatalPlayerCommit.hitResult == PlayerHitResult::Destroyed &&
               fatalPlayerCommit.hitPointsBefore == 1 &&
               fatalPlayerCommit.hitPointsAfter == 0 &&
               fatalPlayerCommitPlayers[0].hitPoints == 0 &&
               !fatalPlayerCommitPlayers[0].active &&
               !fatalPlayerCommitPlayers[0].moving &&
               nearlyEqual(fatalPlayerCommitPlayers[0].deathTimer,
                           kTankDeathDuration) &&
               nearlyEqual(fatalPlayerCommitPlayers[0].respawnTimer, 0.0f) &&
               nearlyEqual(fatalPlayerCommitPlayers[0].shieldTimer, 0.0f) &&
               fatalPlayerCommitPlayers[0].directKillStreak == 0 &&
               nearlyEqual(fatalPlayerCommitPlayers[0].streakPopupTimer,
                           0.0f) &&
               fatalPlayerCommitPlayers[0].lives == 5 &&
               fatalPlayerCommitPlayers[0].level == 3,
           "fatal player commit lost exact lifecycle or progression state");

    Shell unknownEnemyShell = enemyTargetShell;
    unknownEnemyShell.ownerIndex = 999;
    std::vector<Player> unknownSourcePlayers{
        testPlayer(4, 2, unknownEnemyShell.position)};
    const CombatOutcome unknownSourceOutcome =
        evaluateEnemyShellPlayerTankImpact(
            unknownSourcePlayers, openEnvironmentOutcome(unknownEnemyShell));
    const PlayerTankImpactCommit unknownSourceCommit =
        commitEnemyShellPlayerTankImpact(unknownSourcePlayers,
                                         unknownSourceOutcome);
    expect(unknownSourceOutcome.ownerIndex == 999 &&
               unknownSourceCommit.applied &&
               unknownSourceCommit.hitResult == PlayerHitResult::Damaged &&
               unknownSourcePlayers[0].hitPoints == 1,
           "unknown enemy source ID stopped physical player damage");

    reporter.beginSuite("combat-enemy-shell-player-commit-validation");
    std::vector<Player> validationHitPlayers{
        testPlayer(11, 2, enemyTargetShell.position)};
    validationHitPlayers[0].hasBoat = true;
    const CombatOutcome validationHitOutcome =
        evaluateEnemyShellPlayerTankImpact(
            validationHitPlayers, openEnvironmentOutcome(enemyTargetShell));
    int rejectedPlayerCallbacks = 0;
    const auto rejectedPlayerCallback = [&](const Player &) {
        ++rejectedPlayerCallbacks;
    };

    CombatOutcome wrongPlayerTargetOutcome = validationHitOutcome;
    wrongPlayerTargetOutcome.target = CombatTarget::None;
    CombatOutcome playerOwnedHitOutcome = validationHitOutcome;
    playerOwnedHitOutcome.owner = ShellOwner::Player;
    const PlayerTankImpactCommit wrongPlayerTargetCommit =
        commitEnemyShellPlayerTankImpact(
            validationHitPlayers, wrongPlayerTargetOutcome,
            rejectedPlayerCallback);
    const PlayerTankImpactCommit playerOwnedHitCommit =
        commitEnemyShellPlayerTankImpact(
            validationHitPlayers, playerOwnedHitOutcome,
            rejectedPlayerCallback);
    expect(!wrongPlayerTargetCommit.applied &&
               !playerOwnedHitCommit.applied &&
               validationHitPlayers[0].hasBoat &&
               validationHitPlayers[0].hitPoints == 2 &&
               rejectedPlayerCallbacks == 0,
           "wrong target or shell owner became a player state commit");

    CombatOutcome negativePlayerIndexOutcome = validationHitOutcome;
    negativePlayerIndexOutcome.targetPlayerIndex = -1;
    CombatOutcome largePlayerIndexOutcome = validationHitOutcome;
    largePlayerIndexOutcome.targetPlayerIndex = 1;
    CombatOutcome emptyPlayerHpOutcome = validationHitOutcome;
    emptyPlayerHpOutcome.targetPlayerHitPointsBefore = 0;
    const PlayerTankImpactCommit negativePlayerIndexCommit =
        commitEnemyShellPlayerTankImpact(
            validationHitPlayers, negativePlayerIndexOutcome,
            rejectedPlayerCallback);
    const PlayerTankImpactCommit largePlayerIndexCommit =
        commitEnemyShellPlayerTankImpact(
            validationHitPlayers, largePlayerIndexOutcome,
            rejectedPlayerCallback);
    const PlayerTankImpactCommit emptyPlayerHpCommit =
        commitEnemyShellPlayerTankImpact(
            validationHitPlayers, emptyPlayerHpOutcome,
            rejectedPlayerCallback);
    expect(!negativePlayerIndexCommit.applied &&
               !largePlayerIndexCommit.applied &&
               !emptyPlayerHpCommit.applied &&
               validationHitPlayers[0].hasBoat &&
               validationHitPlayers[0].hitPoints == 2 &&
               rejectedPlayerCallbacks == 0,
           "invalid player index or HP snapshot became a commit");

    std::vector<Player> wrongPlayerId = validationHitPlayers;
    wrongPlayerId[0].id = 12;
    std::vector<Player> inactiveHitPlayers = validationHitPlayers;
    inactiveHitPlayers[0].active = false;
    std::vector<Player> creatingHitPlayers = validationHitPlayers;
    creatingHitPlayers[0].creationTimer = 0.01f;
    const PlayerTankImpactCommit wrongPlayerIdCommit =
        commitEnemyShellPlayerTankImpact(
            wrongPlayerId, validationHitOutcome, rejectedPlayerCallback);
    const PlayerTankImpactCommit inactiveHitCommit =
        commitEnemyShellPlayerTankImpact(
            inactiveHitPlayers, validationHitOutcome,
            rejectedPlayerCallback);
    const PlayerTankImpactCommit creatingHitCommit =
        commitEnemyShellPlayerTankImpact(
            creatingHitPlayers, validationHitOutcome,
            rejectedPlayerCallback);
    expect(!wrongPlayerIdCommit.applied && !inactiveHitCommit.applied &&
               !creatingHitCommit.applied &&
               wrongPlayerId[0].hasBoat &&
               inactiveHitPlayers[0].hasBoat &&
               creatingHitPlayers[0].hasBoat &&
               rejectedPlayerCallbacks == 0,
           "stale identity or ineligible player state was committed");

    std::vector<Player> staleHpPlayers = validationHitPlayers;
    staleHpPlayers[0].hitPoints = 1;
    std::vector<Player> staleShieldPlayers = validationHitPlayers;
    staleShieldPlayers[0].shieldTimer = 0.5f;
    std::vector<Player> staleBoatPlayers = validationHitPlayers;
    staleBoatPlayers[0].hasBoat = false;
    const PlayerTankImpactCommit staleHpCommit =
        commitEnemyShellPlayerTankImpact(
            staleHpPlayers, validationHitOutcome,
            rejectedPlayerCallback);
    const PlayerTankImpactCommit staleShieldCommit =
        commitEnemyShellPlayerTankImpact(
            staleShieldPlayers, validationHitOutcome,
            rejectedPlayerCallback);
    const PlayerTankImpactCommit staleBoatCommit =
        commitEnemyShellPlayerTankImpact(
            staleBoatPlayers, validationHitOutcome,
            rejectedPlayerCallback);
    expect(!staleHpCommit.applied && !staleShieldCommit.applied &&
               !staleBoatCommit.applied &&
               staleHpPlayers[0].hitPoints == 1 &&
               staleShieldPlayers[0].hasBoat &&
               !staleBoatPlayers[0].hasBoat &&
               rejectedPlayerCallbacks == 0,
           "stale HP, Shield, or Boat snapshot was committed");

    int repeatedBoatCallbacks = 0;
    std::vector<Player> repeatedBoatPlayers = validationHitPlayers;
    const PlayerTankImpactCommit firstBoatCommit =
        commitEnemyShellPlayerTankImpact(
            repeatedBoatPlayers, validationHitOutcome,
            [&](const Player &) { ++repeatedBoatCallbacks; });
    const PlayerTankImpactCommit secondBoatCommit =
        commitEnemyShellPlayerTankImpact(
            repeatedBoatPlayers, validationHitOutcome,
            [&](const Player &) { ++repeatedBoatCallbacks; });
    expect(firstBoatCommit.applied && !secondBoatCommit.applied &&
               repeatedBoatCallbacks == 1 &&
               !repeatedBoatPlayers[0].hasBoat &&
               repeatedBoatPlayers[0].hitPoints == 2,
           "repeated Boat outcome consumed protection or callback twice");

    int repeatedDamageCallbacks = 0;
    std::vector<Player> repeatedDamagePlayers{
        testPlayer(12, 3, enemyTargetShell.position)};
    const CombatOutcome repeatedDamageOutcome =
        evaluateEnemyShellPlayerTankImpact(
            repeatedDamagePlayers,
            openEnvironmentOutcome(enemyTargetShell));
    const PlayerTankImpactCommit firstDamageCommit =
        commitEnemyShellPlayerTankImpact(
            repeatedDamagePlayers, repeatedDamageOutcome,
            [&](const Player &) { ++repeatedDamageCallbacks; });
    const PlayerTankImpactCommit secondDamageCommit =
        commitEnemyShellPlayerTankImpact(
            repeatedDamagePlayers, repeatedDamageOutcome,
            [&](const Player &) { ++repeatedDamageCallbacks; });
    expect(firstDamageCommit.applied && !secondDamageCommit.applied &&
               repeatedDamageCallbacks == 1 &&
               repeatedDamagePlayers[0].hitPoints == 2,
           "repeated damage outcome removed another HP or callback twice");

    int repeatedFatalCallbacks = 0;
    std::vector<Player> repeatedFatalPlayers{
        testPlayer(13, 1, enemyTargetShell.position)};
    const CombatOutcome repeatedFatalOutcome =
        evaluateEnemyShellPlayerTankImpact(
            repeatedFatalPlayers, openEnvironmentOutcome(enemyTargetShell));
    const PlayerTankImpactCommit firstFatalCommit =
        commitEnemyShellPlayerTankImpact(
            repeatedFatalPlayers, repeatedFatalOutcome,
            [&](const Player &) { ++repeatedFatalCallbacks; });
    const PlayerTankImpactCommit secondFatalCommit =
        commitEnemyShellPlayerTankImpact(
            repeatedFatalPlayers, repeatedFatalOutcome,
            [&](const Player &) { ++repeatedFatalCallbacks; });
    expect(firstFatalCommit.applied && !secondFatalCommit.applied &&
               repeatedFatalCallbacks == 1 &&
               repeatedFatalPlayers[0].hitPoints == 0 &&
               !repeatedFatalPlayers[0].active &&
               nearlyEqual(repeatedFatalPlayers[0].deathTimer,
                           kTankDeathDuration),
           "repeated fatal outcome restarted player death or callback");

    reporter.beginSuite("combat-ordered-physical-impact-resolution");
    StageMap guardedPhysicalMap;
    guardedPhysicalMap.prepareShowcaseArena();
    bool guardedBaseAlive = true;
    std::vector<Enemy> guardedEnemies{
        testEnemy(201, 0, 2, {8.0f, 8.0f})};
    std::vector<Player> guardedPlayers{
        testPlayer(0, 3, {8.0f, 8.0f})};
    Shell guardedShell = testShell({8.0f, 8.0f}, {4.0f, 0.0f},
                                   ShellOwner::Player);
    guardedShell.impacting = true;
    const Shell guardedBefore = guardedShell;
    int guardedCallbacks = 0;
    const ShellPhysicalImpactResult guardedResult =
        resolveShellPhysicalImpact(
            guardedPhysicalMap, guardedBaseAlive, guardedEnemies,
            guardedPlayers, guardedShell,
            [&](const Enemy &) { ++guardedCallbacks; },
            [&](const Player &) { ++guardedCallbacks; });
    Shell expiredPhysicalShell = guardedShell;
    expiredPhysicalShell.impacting = false;
    expiredPhysicalShell.life = 0.0f;
    const Shell expiredBefore = expiredPhysicalShell;
    const ShellPhysicalImpactResult expiredResult =
        resolveShellPhysicalImpact(
            guardedPhysicalMap, guardedBaseAlive, guardedEnemies,
            guardedPlayers, expiredPhysicalShell,
            [&](const Enemy &) { ++guardedCallbacks; },
            [&](const Player &) { ++guardedCallbacks; });
    expect(!guardedResult.resolved && !expiredResult.resolved &&
               guardedResult.outcome.target == CombatTarget::None &&
               expiredResult.outcome.target == CombatTarget::None &&
               guardedCallbacks == 0 && guardedBaseAlive &&
               guardedEnemies[0].armor == 2 &&
               guardedPlayers[0].hitPoints == 3 &&
               sameShellState(guardedShell, guardedBefore) &&
               sameShellState(expiredPhysicalShell, expiredBefore),
           "inert shells entered ordered physical-impact resolution");

    StageMap forestPhysicalMap;
    const bool loadedForestPhysicalMap =
        loadTerrainFixture(forestPhysicalMap);
    const Cell forestPhysicalCell = findTile(forestPhysicalMap, '%');
    Shell forestPhysicalShell = testShell(
        forestPhysicalCell.center(), {3.0f, -2.0f}, ShellOwner::Player);
    forestPhysicalShell.ownerIndex = 0;
    forestPhysicalShell.power = true;
    const Shell forestPhysicalBefore = forestPhysicalShell;
    bool forestBaseAlive = true;
    std::vector<Enemy> noForestEnemies;
    std::vector<Player> forestPlayers{
        testPlayer(0, 3, {2.0f, 2.0f})};
    const ShellPhysicalImpactResult forestPhysicalResult =
        resolveShellPhysicalImpact(
            forestPhysicalMap, forestBaseAlive, noForestEnemies,
            forestPlayers, forestPhysicalShell);
    expect(loadedForestPhysicalMap && forestPhysicalCell.valid() &&
               !forestPhysicalResult.resolved &&
               forestPhysicalResult.outcome.target == CombatTarget::None &&
               eventsForPhysicalShellImpact(forestPhysicalResult).empty() &&
               forestPhysicalMap.tile(forestPhysicalCell.row,
                                      forestPhysicalCell.column) == '.' &&
               forestBaseAlive &&
               sameShellState(forestPhysicalShell, forestPhysicalBefore),
           "power-shell forest mutation stopped flight or changed the shell");

    StageMap mapPriorityPhysicalMap;
    const bool loadedMapPriority = loadStageOne(mapPriorityPhysicalMap);
    const Cell physicalBrick = findTile(mapPriorityPhysicalMap, '#');
    const unsigned char physicalBrickBefore =
        physicalBrick.valid()
            ? mapPriorityPhysicalMap.brickMask(physicalBrick.row,
                                               physicalBrick.column)
            : 0U;
    Shell mapPriorityShell = testShell(
        physicalBrick.center(), {0.0f, -4.0f}, ShellOwner::Enemy);
    mapPriorityShell.ownerIndex = 202;
    const Shell mapPriorityBefore = mapPriorityShell;
    bool mapPriorityBaseAlive = true;
    std::vector<Enemy> mapPriorityEnemies{
        testEnemy(203, 1, 2, physicalBrick.center())};
    std::vector<Player> mapPriorityPlayers{
        testPlayer(0, 3, physicalBrick.center())};
    int mapPriorityCallbacks = 0;
    const ShellPhysicalImpactResult mapPriorityResult =
        resolveShellPhysicalImpact(
            mapPriorityPhysicalMap, mapPriorityBaseAlive,
            mapPriorityEnemies, mapPriorityPlayers, mapPriorityShell,
            [&](const Enemy &) { ++mapPriorityCallbacks; },
            [&](const Player &) { ++mapPriorityCallbacks; });
    expect(loadedMapPriority && physicalBrick.valid() &&
               mapPriorityResult.resolved &&
               mapPriorityResult.outcome.target == CombatTarget::StageMap &&
               mapPriorityResult.outcome.impactKind == ImpactKind::Brick &&
               mapPriorityPhysicalMap.brickMask(
                   physicalBrick.row, physicalBrick.column) !=
                   physicalBrickBefore &&
               mapPriorityEnemies[0].armor == 2 &&
               mapPriorityPlayers[0].hitPoints == 3 &&
               mapPriorityCallbacks == 0 && mapPriorityBaseAlive &&
               sameShellState(mapPriorityShell, mapPriorityBefore),
           "map impact stopped suppressing later tank commits");

    StageMap corePriorityPhysicalMap;
    corePriorityPhysicalMap.prepareShowcaseArena();
    bool corePriorityPhysicalBaseAlive = true;
    Shell corePriorityPhysicalShell = testShell(
        kGovernmentBaseCenter, {0.0f, 4.0f}, ShellOwner::Player);
    const Shell corePriorityPhysicalBefore = corePriorityPhysicalShell;
    std::vector<Enemy> corePriorityPhysicalEnemies{
        testEnemy(204, 2, 1, kGovernmentBaseCenter)};
    std::vector<Player> corePriorityPhysicalPlayers{
        testPlayer(0, 3, kGovernmentBaseCenter)};
    int corePriorityCallbacks = 0;
    const ShellPhysicalImpactResult corePriorityPhysicalResult =
        resolveShellPhysicalImpact(
            corePriorityPhysicalMap, corePriorityPhysicalBaseAlive,
            corePriorityPhysicalEnemies, corePriorityPhysicalPlayers,
            corePriorityPhysicalShell,
            [&](const Enemy &) { ++corePriorityCallbacks; },
            [&](const Player &) { ++corePriorityCallbacks; });
    expect(corePriorityPhysicalResult.resolved &&
               corePriorityPhysicalResult.outcome.target ==
                   CombatTarget::GovernmentCore &&
               corePriorityPhysicalResult.outcome.destroyedGovernmentCore() &&
               !corePriorityPhysicalBaseAlive &&
               corePriorityPhysicalEnemies[0].armor == 1 &&
               corePriorityPhysicalPlayers[0].hitPoints == 3 &&
               corePriorityCallbacks == 0 &&
               sameShellState(corePriorityPhysicalShell,
                              corePriorityPhysicalBefore),
           "government core stopped suppressing later tank commits");

    Shell deadCorePhysicalShell = testShell(
        kGovernmentBaseCenter, {0.0f, -4.0f}, ShellOwner::Enemy);
    const Shell deadCorePhysicalBefore = deadCorePhysicalShell;
    const ShellPhysicalImpactResult deadCorePhysicalResult =
        resolveShellPhysicalImpact(
            corePriorityPhysicalMap, corePriorityPhysicalBaseAlive,
            corePriorityPhysicalEnemies, corePriorityPhysicalPlayers,
            deadCorePhysicalShell,
            [&](const Enemy &) { ++corePriorityCallbacks; },
            [&](const Player &) { ++corePriorityCallbacks; });
    expect(deadCorePhysicalResult.resolved &&
               deadCorePhysicalResult.outcome.target ==
                   CombatTarget::GovernmentCore &&
               deadCorePhysicalResult.outcome.governmentCoreHealthBefore == 0 &&
               deadCorePhysicalResult.outcome.governmentCoreHealthAfter == 0 &&
               !deadCorePhysicalResult.outcome.damagedGovernmentCore() &&
               corePriorityPhysicalPlayers[0].hitPoints == 3 &&
               corePriorityCallbacks == 0 &&
               sameShellState(deadCorePhysicalShell, deadCorePhysicalBefore),
           "destroyed government core stopped silently absorbing shells");

    StageMap enemyPhysicalMap;
    enemyPhysicalMap.prepareShowcaseArena();
    bool enemyPhysicalBaseAlive = true;
    Shell enemyPhysicalShell = testShell(
        {8.0f, 8.0f}, {5.0f, -1.0f}, ShellOwner::Player);
    enemyPhysicalShell.ownerIndex = 1;
    enemyPhysicalShell.power = true;
    const Shell enemyPhysicalBefore = enemyPhysicalShell;
    std::vector<Enemy> enemyPhysicalTargets{
        testEnemy(205, 3, 2, enemyPhysicalShell.position)};
    enemyPhysicalTargets[0].carriesBonus = true;
    std::vector<Player> enemyPhysicalPlayers{
        testPlayer(0, 3, {2.0f, 2.0f}),
        testPlayer(1, 3, {3.0f, 3.0f})};
    int carrierPhysicalCallbacks = 0;
    int carrierObservedArmor = -1;
    int carrierObservedScore = -1;
    int unexpectedPlayerCallbacks = 0;
    const ShellPhysicalImpactResult enemyPhysicalResult =
        resolveShellPhysicalImpact(
            enemyPhysicalMap, enemyPhysicalBaseAlive, enemyPhysicalTargets,
            enemyPhysicalPlayers, enemyPhysicalShell,
            [&](const Enemy &carrier) {
                ++carrierPhysicalCallbacks;
                carrierObservedArmor = carrier.armor;
                carrierObservedScore = enemyPhysicalPlayers[1].score;
            },
            [&](const Player &) { ++unexpectedPlayerCallbacks; });
    expect(enemyPhysicalResult.resolved &&
               enemyPhysicalResult.outcome.target == CombatTarget::EnemyTank &&
               enemyPhysicalResult.enemyCommit.applied &&
               !enemyPhysicalResult.enemyCommit.destroyedNow &&
               enemyPhysicalResult.enemyCommit.creditedPlayerIndex == 1 &&
               enemyPhysicalResult.enemyCommit.eventPoints ==
                   kDirectEnemyHitPoints &&
               carrierPhysicalCallbacks == 1 && carrierObservedArmor == 2 &&
               carrierObservedScore == 0 && unexpectedPlayerCallbacks == 0 &&
               enemyPhysicalTargets[0].armor == 1 &&
               !enemyPhysicalTargets[0].destroyed &&
               enemyPhysicalPlayers[1].score == kDirectEnemyHitPoints &&
               sameShellState(enemyPhysicalShell, enemyPhysicalBefore),
           "ordered enemy commit lost carrier timing, attribution, or shell deferral");

    std::vector<Enemy> fatalPhysicalTargets{
        testEnemy(206, 2, 1, enemyPhysicalShell.position)};
    std::vector<Player> fatalPhysicalPlayers{
        testPlayer(0, 3, {2.0f, 2.0f}),
        testPlayer(1, 3, {3.0f, 3.0f})};
    const ShellPhysicalImpactResult fatalEnemyPhysicalResult =
        resolveShellPhysicalImpact(
            enemyPhysicalMap, enemyPhysicalBaseAlive, fatalPhysicalTargets,
            fatalPhysicalPlayers, enemyPhysicalShell);
    expect(fatalEnemyPhysicalResult.resolved &&
               fatalEnemyPhysicalResult.enemyCommit.applied &&
               fatalEnemyPhysicalResult.enemyCommit.destroyedNow &&
               fatalPhysicalTargets[0].armor == 0 &&
               fatalPhysicalTargets[0].destroyed &&
               !fatalPhysicalTargets[0].moving &&
               nearlyEqual(fatalPhysicalTargets[0].deathTimer,
                           kTankDeathDuration) &&
               fatalPhysicalPlayers[1].stageTally.destroyed[2] == 1 &&
               fatalPhysicalPlayers[1].directKillStreak == 1 &&
               sameShellState(enemyPhysicalShell, enemyPhysicalBefore),
           "ordered fatal enemy commit lost destruction or streak state");

    const auto resolvePhysicalPlayerHit = [&](Player player, int ownerIndex,
                                              int &callbackCount,
                                              int &observedHitPoints,
                                              bool &observedBoat) {
        StageMap playerPhysicalMap;
        playerPhysicalMap.prepareShowcaseArena();
        bool playerPhysicalBaseAlive = true;
        Shell playerPhysicalShell = testShell(
            player.position, {-4.0f, 1.0f}, ShellOwner::Enemy);
        playerPhysicalShell.ownerIndex = ownerIndex;
        std::vector<Enemy> noPlayerHitEnemies;
        std::vector<Player> physicalPlayers{player};
        const ShellPhysicalImpactResult result = resolveShellPhysicalImpact(
            playerPhysicalMap, playerPhysicalBaseAlive, noPlayerHitEnemies,
            physicalPlayers, playerPhysicalShell, {},
            [&](const Player &target) {
                ++callbackCount;
                observedHitPoints = target.hitPoints;
                observedBoat = target.hasBoat;
            });
        return std::make_pair(result, physicalPlayers[0]);
    };

    int shieldPhysicalCallbacks = 0;
    int shieldObservedHitPoints = -1;
    bool shieldObservedBoat = false;
    Player shieldPhysicalPlayer = testPlayer(10, 3, {10.0f, 10.0f});
    shieldPhysicalPlayer.shieldTimer = 1.0f;
    const auto shieldPhysical = resolvePhysicalPlayerHit(
        shieldPhysicalPlayer, 301, shieldPhysicalCallbacks,
        shieldObservedHitPoints, shieldObservedBoat);
    expect(shieldPhysical.first.resolved &&
               shieldPhysical.first.outcome.target ==
                   CombatTarget::PlayerTank &&
               shieldPhysical.first.outcome.ownerIndex == 301 &&
               shieldPhysical.first.playerCommit.applied &&
               shieldPhysical.first.playerCommit.hitResult ==
                   PlayerHitResult::Shielded &&
               shieldPhysicalCallbacks == 1 &&
               shieldObservedHitPoints == 3 &&
               shieldPhysical.second.hitPoints == 3 &&
               shieldPhysical.second.shieldTimer > 0.0f,
           "ordered Shield commit lost pre-commit state or source attribution");

    int boatPhysicalCallbacks = 0;
    int boatObservedHitPoints = -1;
    bool boatObservedBoat = false;
    Player boatPhysicalPlayer = testPlayer(11, 3, {11.0f, 11.0f});
    boatPhysicalPlayer.hasBoat = true;
    const auto boatPhysical = resolvePhysicalPlayerHit(
        boatPhysicalPlayer, 302, boatPhysicalCallbacks,
        boatObservedHitPoints, boatObservedBoat);
    expect(boatPhysical.first.resolved &&
               boatPhysical.first.playerCommit.hitResult ==
                   PlayerHitResult::BoatAbsorbed &&
               boatPhysicalCallbacks == 1 && boatObservedHitPoints == 3 &&
               boatObservedBoat && boatPhysical.second.hitPoints == 3 &&
               !boatPhysical.second.hasBoat,
           "ordered Boat commit lost pre-commit state or hit priority");

    int damagePhysicalCallbacks = 0;
    int damageObservedHitPoints = -1;
    bool damageObservedBoat = false;
    const auto damagePhysical = resolvePhysicalPlayerHit(
        testPlayer(12, 2, {12.0f, 12.0f}), 999,
        damagePhysicalCallbacks, damageObservedHitPoints,
        damageObservedBoat);
    expect(damagePhysical.first.resolved &&
               damagePhysical.first.outcome.ownerIndex == 999 &&
               damagePhysical.first.playerCommit.hitResult ==
                   PlayerHitResult::Damaged &&
               damagePhysical.first.playerCommit.hitPointsBefore == 2 &&
               damagePhysical.first.playerCommit.hitPointsAfter == 1 &&
               damagePhysicalCallbacks == 1 &&
               damageObservedHitPoints == 2 &&
               damagePhysical.second.hitPoints == 1 &&
               damagePhysical.second.active,
           "ordered player damage rejected an unknown source or changed callback timing");

    int fatalPlayerPhysicalCallbacks = 0;
    int fatalPlayerObservedHitPoints = -1;
    bool fatalPlayerObservedBoat = false;
    Player fatalPhysicalPlayer = testPlayer(13, 1, {13.0f, 13.0f});
    fatalPhysicalPlayer.directKillStreak = 4;
    const auto fatalPlayerPhysical = resolvePhysicalPlayerHit(
        fatalPhysicalPlayer, 303, fatalPlayerPhysicalCallbacks,
        fatalPlayerObservedHitPoints, fatalPlayerObservedBoat);
    expect(fatalPlayerPhysical.first.resolved &&
               fatalPlayerPhysical.first.playerCommit.hitResult ==
                   PlayerHitResult::Destroyed &&
               fatalPlayerPhysicalCallbacks == 1 &&
               fatalPlayerObservedHitPoints == 1 &&
               fatalPlayerPhysical.second.hitPoints == 0 &&
               !fatalPlayerPhysical.second.active &&
               !fatalPlayerPhysical.second.moving &&
               nearlyEqual(fatalPlayerPhysical.second.deathTimer,
                           kTankDeathDuration) &&
               fatalPlayerPhysical.second.directKillStreak == 0,
           "ordered fatal player commit lost pre-commit or lifecycle state");

    reporter.beginSuite("combat-physical-impact-event-projection");
    const ShellPhysicalImpactResult unresolvedProjection;
    expect(eventsForPhysicalShellImpact(unresolvedProjection).empty(),
           "unresolved physical result emitted an event");

    ShellPhysicalImpactResult mapProjection;
    mapProjection.resolved = true;
    mapProjection.outcome.target = CombatTarget::StageMap;
    // A footprint may damage brick before steel becomes the stopping kind.
    // Project recorded damage, rather than switching only on the final kind.
    mapProjection.outcome.impactKind = ImpactKind::Steel;
    mapProjection.outcome.owner = ShellOwner::Player;
    mapProjection.outcome.ownerIndex = 4;
    mapProjection.outcome.power = true;
    mapProjection.outcome.position = {7.25f, 8.75f};
    mapProjection.outcome.impactDetails.brickCount = 2;
    mapProjection.outcome.impactDetails.bricks[0] = {3, 5, 15U, 12U};
    mapProjection.outcome.impactDetails.bricks[1] = {3, 6, 15U, 3U};
    mapProjection.outcome.impactDetails.governmentWallIndex = 1;
    mapProjection.outcome.impactDetails.governmentWallHealthBefore = 3;
    mapProjection.outcome.impactDetails.governmentWallHealthAfter = 2;

    GameEvent firstBrick = shellEvent(
        GameEventType::BrickHit, ShellOwner::Player, 4,
        mapProjection.outcome.position, true);
    firstBrick.impactKind = ImpactKind::Brick;
    firstBrick.row = 3;
    firstBrick.column = 5;
    firstBrick.valueBefore = 15;
    firstBrick.valueAfter = 12;
    GameEvent secondBrick = firstBrick;
    secondBrick.column = 6;
    secondBrick.valueAfter = 3;
    GameEvent wallDamage = shellEvent(
        GameEventType::BaseDamaged, ShellOwner::Player, 4,
        mapProjection.outcome.position, true);
    wallDamage.impactKind = ImpactKind::GovernmentWall;
    wallDamage.basePart = GovernmentBasePart::Wall;
    wallDamage.baseSegmentIndex = 1;
    wallDamage.valueBefore = 3;
    wallDamage.valueAfter = 2;
    const std::vector<GameEvent> expectedMapEvents{
        firstBrick, secondBrick, wallDamage};
    expect(eventsForPhysicalShellImpact(mapProjection) == expectedMapEvents,
           "map projection lost brick order, wall damage, or shell attribution");

    ShellPhysicalImpactResult upperClampedMap = mapProjection;
    upperClampedMap.outcome.impactDetails.brickCount = 99;
    upperClampedMap.outcome.impactDetails.governmentWallIndex = -1;
    const std::vector<GameEvent> upperClampedEvents =
        eventsForPhysicalShellImpact(upperClampedMap);
    expect(upperClampedEvents.size() == 2 &&
               upperClampedEvents[0] == firstBrick &&
               upperClampedEvents[1] == secondBrick,
           "malformed high brick count escaped the bounded event snapshot");

    ShellPhysicalImpactResult inertMap = mapProjection;
    inertMap.outcome.impactDetails = {};
    const std::vector<GameEvent> steelEvents =
        eventsForPhysicalShellImpact(inertMap);
    inertMap.outcome.impactKind = ImpactKind::Boundary;
    const std::vector<GameEvent> boundaryEvents =
        eventsForPhysicalShellImpact(inertMap);
    inertMap.outcome.impactDetails.brickCount = -8;
    inertMap.outcome.impactDetails.governmentWallIndex = 0;
    inertMap.outcome.impactDetails.governmentWallHealthBefore = 2;
    inertMap.outcome.impactDetails.governmentWallHealthAfter = 2;
    const std::vector<GameEvent> unchangedWallEvents =
        eventsForPhysicalShellImpact(inertMap);
    expect(steelEvents.empty() && boundaryEvents.empty() &&
               unchangedWallEvents.empty(),
           "inert terrain or unchanged steel wall emitted a damage event");

    ShellPhysicalImpactResult coreProjection;
    coreProjection.resolved = true;
    coreProjection.outcome.target = CombatTarget::GovernmentCore;
    coreProjection.outcome.owner = ShellOwner::Enemy;
    coreProjection.outcome.ownerIndex = 77;
    coreProjection.outcome.position = {12.5f, 23.5f};
    coreProjection.outcome.governmentCoreHealthBefore = 1;
    coreProjection.outcome.governmentCoreHealthAfter = 0;
    GameEvent expectedCore = shellEvent(
        GameEventType::BaseDamaged, ShellOwner::Enemy, 77,
        coreProjection.outcome.position, false);
    expectedCore.basePart = GovernmentBasePart::Core;
    expectedCore.valueBefore = 1;
    expectedCore.valueAfter = 0;
    expect(eventsForPhysicalShellImpact(coreProjection) ==
               std::vector<GameEvent>{expectedCore},
           "live government core projection changed fields or attribution");
    coreProjection.outcome.governmentCoreHealthBefore = 0;
    expect(eventsForPhysicalShellImpact(coreProjection).empty(),
           "destroyed government core emitted duplicate damage");

    ShellPhysicalImpactResult enemyProjection;
    enemyProjection.resolved = true;
    enemyProjection.outcome.target = CombatTarget::EnemyTank;
    enemyProjection.outcome.owner = ShellOwner::Player;
    enemyProjection.outcome.ownerIndex = 999;
    enemyProjection.outcome.power = true;
    enemyProjection.outcome.position = {4.0f, 5.0f};
    enemyProjection.outcome.targetEnemyPosition = {9.0f, 10.0f};
    enemyProjection.outcome.targetEnemyId = 81;
    enemyProjection.outcome.targetEnemyType = 3;
    enemyProjection.outcome.targetEnemyArmorBefore = 3;
    enemyProjection.outcome.targetEnemyArmorAfter = 2;
    enemyProjection.enemyCommit.applied = true;
    enemyProjection.enemyCommit.eventPoints = kDirectEnemyHitPoints;
    GameEvent expectedEnemyDamage = shellEvent(
        GameEventType::TankDamaged, ShellOwner::Player, 999,
        enemyProjection.outcome.targetEnemyPosition, true);
    expectedEnemyDamage.targetEnemyId = 81;
    expectedEnemyDamage.enemyType = 3;
    expectedEnemyDamage.valueBefore = 3;
    expectedEnemyDamage.valueAfter = 2;
    expectedEnemyDamage.points = kDirectEnemyHitPoints;
    expect(eventsForPhysicalShellImpact(enemyProjection) ==
               std::vector<GameEvent>{expectedEnemyDamage},
           "nonfatal enemy projection reread live state or lost points");

    ShellPhysicalImpactResult fatalEnemyProjection = enemyProjection;
    fatalEnemyProjection.outcome.targetEnemyArmorBefore = 1;
    fatalEnemyProjection.outcome.targetEnemyArmorAfter = 0;
    fatalEnemyProjection.enemyCommit.destroyedNow = true;
    GameEvent expectedEnemyDeath = expectedEnemyDamage;
    expectedEnemyDeath.type = GameEventType::TankDestroyed;
    expectedEnemyDeath.valueBefore = 1;
    expectedEnemyDeath.valueAfter = 0;
    expect(eventsForPhysicalShellImpact(fatalEnemyProjection) ==
               std::vector<GameEvent>{expectedEnemyDeath},
           "fatal enemy projection changed its typed destruction event");
    fatalEnemyProjection.enemyCommit.applied = false;
    expect(eventsForPhysicalShellImpact(fatalEnemyProjection).empty(),
           "rejected enemy commit emitted an event");

    ShellPhysicalImpactResult playerProjection;
    playerProjection.resolved = true;
    playerProjection.outcome.target = CombatTarget::PlayerTank;
    playerProjection.outcome.owner = ShellOwner::Enemy;
    playerProjection.outcome.ownerIndex = 91;
    playerProjection.outcome.targetPlayerPosition = {11.0f, 12.0f};
    playerProjection.outcome.targetPlayerId = 1;
    playerProjection.playerCommit.applied = true;
    playerProjection.playerCommit.hitResult = PlayerHitResult::Damaged;
    playerProjection.playerCommit.hitPointsBefore = 4;
    playerProjection.playerCommit.hitPointsAfter = 3;
    GameEvent expectedPlayerDamage = shellEvent(
        GameEventType::TankDamaged, ShellOwner::Enemy, 91,
        playerProjection.outcome.targetPlayerPosition, false);
    expectedPlayerDamage.targetPlayerId = 1;
    expectedPlayerDamage.valueBefore = 4;
    expectedPlayerDamage.valueAfter = 3;
    expect(eventsForPhysicalShellImpact(playerProjection) ==
               std::vector<GameEvent>{expectedPlayerDamage},
           "player damage projection changed source, target, or HP values");

    ShellPhysicalImpactResult fatalPlayerProjection = playerProjection;
    fatalPlayerProjection.playerCommit.hitResult = PlayerHitResult::Destroyed;
    fatalPlayerProjection.playerCommit.hitPointsBefore = 1;
    fatalPlayerProjection.playerCommit.hitPointsAfter = 0;
    GameEvent expectedPlayerDeath = expectedPlayerDamage;
    expectedPlayerDeath.type = GameEventType::TankDestroyed;
    expectedPlayerDeath.valueBefore = 1;
    expectedPlayerDeath.valueAfter = 0;
    expect(eventsForPhysicalShellImpact(fatalPlayerProjection) ==
               std::vector<GameEvent>{expectedPlayerDeath},
           "player destruction projection changed its typed event");

    playerProjection.playerCommit.hitResult = PlayerHitResult::Shielded;
    const std::vector<GameEvent> shieldEvents =
        eventsForPhysicalShellImpact(playerProjection);
    playerProjection.playerCommit.hitResult = PlayerHitResult::BoatAbsorbed;
    const std::vector<GameEvent> boatEvents =
        eventsForPhysicalShellImpact(playerProjection);
    playerProjection.playerCommit.applied = false;
    playerProjection.playerCommit.hitResult = PlayerHitResult::Damaged;
    const std::vector<GameEvent> rejectedPlayerEvents =
        eventsForPhysicalShellImpact(playerProjection);
    expect(shieldEvents.empty() && boatEvents.empty() &&
               rejectedPlayerEvents.empty(),
           "non-damaging or rejected player commit emitted an event");

    GameEvent fullEvent = expectedEnemyDeath;
    fullEvent.direction = CardinalDirection::East;
    fullEvent.sourceEnemyId = 7;
    fullEvent.targetPlayerId = 1;
    fullEvent.bonusType = tanks3d::game::BonusType::Star;
    fullEvent.impactKind = ImpactKind::GovernmentWall;
    fullEvent.basePart = GovernmentBasePart::Wall;
    fullEvent.stageEndReason = tanks3d::game::StageEndReason::Cleared;
    fullEvent.stage = 8;
    fullEvent.row = 9;
    fullEvent.column = 10;
    fullEvent.baseSegmentIndex = 1;
    expect(fullEvent == fullEvent,
           "equal game-event values stopped comparing equal");
    const auto fieldDiffers = [&](const auto &change) {
        GameEvent changed = fullEvent;
        change(changed);
        return !(fullEvent == changed);
    };
    bool everyFieldMatters = true;
    everyFieldMatters &= fieldDiffers(
        [](GameEvent &event) { event.type = GameEventType::BrickHit; });
    everyFieldMatters &= fieldDiffers(
        [](GameEvent &event) { event.cause = GameEventCause::EnemyShell; });
    everyFieldMatters &= fieldDiffers(
        [](GameEvent &event) { event.position.x += 1.0f; });
    everyFieldMatters &= fieldDiffers(
        [](GameEvent &event) { event.position.z += 1.0f; });
    everyFieldMatters &= fieldDiffers([](GameEvent &event) {
        event.direction = CardinalDirection::West;
    });
    everyFieldMatters &= fieldDiffers(
        [](GameEvent &event) { event.sourcePlayerId += 1; });
    everyFieldMatters &= fieldDiffers(
        [](GameEvent &event) { event.sourceEnemyId += 1; });
    everyFieldMatters &= fieldDiffers(
        [](GameEvent &event) { event.targetPlayerId += 1; });
    everyFieldMatters &= fieldDiffers(
        [](GameEvent &event) { event.targetEnemyId += 1; });
    everyFieldMatters &= fieldDiffers(
        [](GameEvent &event) { event.enemyType += 1; });
    everyFieldMatters &= fieldDiffers([](GameEvent &event) {
        event.bonusType = tanks3d::game::BonusType::Gun;
    });
    everyFieldMatters &= fieldDiffers(
        [](GameEvent &event) { event.impactKind = ImpactKind::Brick; });
    everyFieldMatters &= fieldDiffers([](GameEvent &event) {
        event.basePart = GovernmentBasePart::Core;
    });
    everyFieldMatters &= fieldDiffers([](GameEvent &event) {
        event.stageEndReason = tanks3d::game::StageEndReason::BaseDestroyed;
    });
    everyFieldMatters &= fieldDiffers(
        [](GameEvent &event) { event.stage += 1; });
    everyFieldMatters &= fieldDiffers(
        [](GameEvent &event) { event.row += 1; });
    everyFieldMatters &= fieldDiffers(
        [](GameEvent &event) { event.column += 1; });
    everyFieldMatters &= fieldDiffers(
        [](GameEvent &event) { event.baseSegmentIndex += 1; });
    everyFieldMatters &= fieldDiffers(
        [](GameEvent &event) { event.valueBefore += 1; });
    everyFieldMatters &= fieldDiffers(
        [](GameEvent &event) { event.valueAfter += 1; });
    everyFieldMatters &= fieldDiffers(
        [](GameEvent &event) { event.points += 1; });
    everyFieldMatters &= fieldDiffers(
        [](GameEvent &event) { event.power = !event.power; });
    expect(everyFieldMatters,
           "GameEvent equality ignored a serialized observation field");

    reporter.finish();
    return passed ? 0 : 1;
}
