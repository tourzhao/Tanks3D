#include "game/bonus_rules.h"
#include "game/entities.h"

#include "test_support.h"

#include <array>
#include <cmath>
#include <string>
#include <vector>

namespace
{
using tanks3d::core::CardinalDirection;
using tanks3d::core::Nation;
using tanks3d::game::BonusType;
using tanks3d::game::Enemy;
using tanks3d::game::Player;
using tanks3d::game::PlayerHitResult;
using tanks3d::game::Shell;
using tanks3d::game::ShellOwner;
using tanks3d::game::StageTally;
using tanks3d::game::enemyDestructionComplete;
using tanks3d::game::kBandageWeight;
using tanks3d::game::kClassicPickupTypeCount;
using tanks3d::game::kEnemyTypeCount;
using tanks3d::game::kPickupFastBlinkStart;
using tanks3d::game::kPickupLifetime;
using tanks3d::game::kStreakPopupDuration;
using tanks3d::game::playerMeetsBonusTypeEligibility;
using tanks3d::game::resolvePlayerHit;
using tanks3d::game::typeForWeightedSlot;
using tanks3d::game::weightedTypeSlotCount;

bool nearlyEqual(float first, float second, float tolerance = 0.00001f)
{
    return std::fabs(first - second) <= tolerance;
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

    reporter.beginSuite("game-bonus-rules");
    const std::array<BonusType, 10> bonusTypes{{
        BonusType::Grenade, BonusType::Helmet, BonusType::Clock,
        BonusType::Shovel, BonusType::Tank, BonusType::Star, BonusType::Gun,
        BonusType::Boat, BonusType::Bandage, BonusType::Count}};
    for (std::size_t index = 0; index < bonusTypes.size(); ++index)
    {
        expect(static_cast<std::size_t>(bonusTypes[index]) == index,
               "bonus enum order changed at index " +
                   std::to_string(index));
    }
    expect(kClassicPickupTypeCount == 8 && kBandageWeight == 2,
           "bonus weight constants changed");
    expect(nearlyEqual(kPickupLifetime, 12.5f) &&
               nearlyEqual(kPickupFastBlinkStart, 9.375f),
           "pickup timing constants changed");
    expect(weightedTypeSlotCount(false) == 8 &&
               weightedTypeSlotCount(true) == 10,
           "weighted slot counts changed");

    for (int slot = 0; slot < kClassicPickupTypeCount; ++slot)
    {
        const BonusType expected = static_cast<BonusType>(slot);
        expect(typeForWeightedSlot(slot, false) == expected,
               "healthy slot mapping changed at " + std::to_string(slot));
        expect(typeForWeightedSlot(slot, true) == expected,
               "injured classic slot mapping changed at " +
                   std::to_string(slot));
    }
    expect(typeForWeightedSlot(8, true) == BonusType::Bandage &&
               typeForWeightedSlot(9, true) == BonusType::Bandage,
           "Bandage no longer has two eligible slots");
    expect(typeForWeightedSlot(-1, false) == BonusType::Grenade &&
               typeForWeightedSlot(99, false) == BonusType::Boat &&
               typeForWeightedSlot(-1, true) == BonusType::Grenade &&
               typeForWeightedSlot(99, true) == BonusType::Bandage,
           "weighted slot clamping changed");

    reporter.beginSuite("game-player-hits-and-healing");
    for (int maximumHitPoints = 1; maximumHitPoints <= 6;
         ++maximumHitPoints)
    {
        Player player;
        player.maximumHitPoints = maximumHitPoints;
        player.hitPoints = maximumHitPoints;
        for (int hit = 1; hit <= maximumHitPoints; ++hit)
        {
            const PlayerHitResult result = resolvePlayerHit(player);
            const PlayerHitResult expected =
                hit == maximumHitPoints ? PlayerHitResult::Destroyed
                                        : PlayerHitResult::Damaged;
            expect(result == expected &&
                       player.hitPoints == maximumHitPoints - hit,
                   "HP hit sequence changed for maximum " +
                       std::to_string(maximumHitPoints) + ", hit " +
                       std::to_string(hit));
        }
        expect(resolvePlayerHit(player) == PlayerHitResult::Destroyed &&
                   player.hitPoints == 0,
               "fatal HP clamping changed for maximum " +
                   std::to_string(maximumHitPoints));
    }

    Player protectedPlayer;
    protectedPlayer.hitPoints = 2;
    protectedPlayer.shieldTimer = 0.01f;
    protectedPlayer.hasBoat = true;
    expect(resolvePlayerHit(protectedPlayer) == PlayerHitResult::Shielded &&
               protectedPlayer.hasBoat && protectedPlayer.hitPoints == 2,
           "shield no longer preserves Boat and HP");
    protectedPlayer.shieldTimer = 0.0f;
    expect(resolvePlayerHit(protectedPlayer) ==
                   PlayerHitResult::BoatAbsorbed &&
               !protectedPlayer.hasBoat && protectedPlayer.hitPoints == 2,
           "Boat absorption no longer precedes HP damage");
    expect(resolvePlayerHit(protectedPlayer) == PlayerHitResult::Damaged &&
               protectedPlayer.hitPoints == 1,
           "unprotected hit no longer follows Boat absorption");
    protectedPlayer.shieldTimer = -0.01f;
    protectedPlayer.hasBoat = true;
    expect(resolvePlayerHit(protectedPlayer) ==
                   PlayerHitResult::BoatAbsorbed &&
               protectedPlayer.hitPoints == 1,
           "non-positive shield boundary changed");

    for (int maximumHitPoints = 1; maximumHitPoints <= 6;
         ++maximumHitPoints)
    {
        for (int hitPoints = 0; hitPoints <= maximumHitPoints; ++hitPoints)
        {
            Player player;
            player.maximumHitPoints = maximumHitPoints;
            player.hitPoints = hitPoints;
            const bool expectedEligible =
                maximumHitPoints > 1 && hitPoints > 0 &&
                hitPoints < maximumHitPoints;
            expect(player.needsHealing() == expectedEligible &&
                       playerMeetsBonusTypeEligibility(
                           player, BonusType::Bandage) ==
                           expectedEligible,
                   "Bandage eligibility changed at HP " +
                       std::to_string(hitPoints) + "/" +
                       std::to_string(maximumHitPoints));
            const bool healed = player.healOneHitPoint();
            expect(healed == expectedEligible &&
                       player.hitPoints ==
                           hitPoints + (expectedEligible ? 1 : 0),
                   "one-point healing changed at HP " +
                       std::to_string(hitPoints) + "/" +
                       std::to_string(maximumHitPoints));
        }
    }
    Player inactivePlayer;
    inactivePlayer.maximumHitPoints = 6;
    inactivePlayer.hitPoints = 1;
    inactivePlayer.active = false;
    expect(!inactivePlayer.needsHealing() &&
               !playerMeetsBonusTypeEligibility(
                   inactivePlayer, BonusType::Bandage) &&
               !inactivePlayer.healOneHitPoint() &&
               inactivePlayer.hitPoints == 1,
           "inactive player became Bandage-eligible");
    expect(playerMeetsBonusTypeEligibility(inactivePlayer, BonusType::Star),
           "non-Bandage eligibility changed");
    expect(!playerMeetsBonusTypeEligibility(
               inactivePlayer, BonusType::Count) &&
               !playerMeetsBonusTypeEligibility(
                   inactivePlayer, static_cast<BonusType>(255)),
           "bonus sentinel or out-of-range type became collectible");

    reporter.beginSuite("game-scoring-and-streaks");
    StageTally tally;
    tally.reset(900);
    expect(tally.scoreAtStageStart == 900 && tally.totalDestroyed() == 0 &&
               tally.totalEnemyPoints() == 0 && tally.stagePoints() == 0,
           "stage tally reset changed");
    tally.creditEnemy(-1, 100, true);
    tally.creditEnemy(kEnemyTypeCount, 100, true);
    expect(tally.totalDestroyed() == 0 && tally.totalEnemyPoints() == 0,
           "invalid enemy category changed a tally");
    tally.creditEnemy(0, -100, true);
    tally.creditEnemy(1, 200, false);
    tally.creditEnemy(2, 300, true);
    tally.creditBonus(-500);
    tally.creditBonus(700);
    expect(tally.destroyed[0] == 1 && tally.destroyed[1] == 0 &&
               tally.destroyed[2] == 1 && tally.destroyed[3] == 0 &&
               tally.enemyPoints[0] == 0 && tally.enemyPoints[1] == 200 &&
               tally.enemyPoints[2] == 300 && tally.totalDestroyed() == 2 &&
               tally.totalEnemyPoints() == 500 && tally.bonusPoints == 700 &&
               tally.stagePoints() == 1200,
           "classified score or bonus accounting changed");

    Player scoringPlayer;
    scoringPlayer.creditDirectEnemyHit(1, 100, false);
    expect(scoringPlayer.score == 100 &&
               scoringPlayer.stageTally.enemyPoints[1] == 100 &&
               scoringPlayer.stageTally.destroyed[1] == 0 &&
               scoringPlayer.directKillStreak == 0,
           "non-fatal direct hit accounting changed");
    scoringPlayer.creditDirectEnemyHit(2, 300, true);
    expect(scoringPlayer.score == 400 &&
               scoringPlayer.stageTally.destroyed[2] == 1 &&
               scoringPlayer.directKillStreak == 1 &&
               nearlyEqual(scoringPlayer.streakPopupTimer,
                           kStreakPopupDuration),
           "direct kill accounting or streak changed");
    scoringPlayer.active = false;
    scoringPlayer.creditDirectEnemyHit(3, 400, true);
    expect(scoringPlayer.score == 800 &&
               scoringPlayer.stageTally.destroyed[3] == 1 &&
               scoringPlayer.directKillStreak == 1,
           "in-flight shell after player death changed tally or streak");
    scoringPlayer.resetDirectKillStreak();
    expect(scoringPlayer.directKillStreak == 0 &&
               nearlyEqual(scoringPlayer.streakPopupTimer, 0.0f),
           "streak reset changed");

    StageTally grenadeTally;
    grenadeTally.creditBonus(kEnemyTypeCount * 200);
    expect(grenadeTally.totalDestroyed() == 0 &&
               grenadeTally.totalEnemyPoints() == 0 &&
               grenadeTally.bonusPoints == 800,
           "grenade points entered classified K.O. rows");

    reporter.beginSuite("game-enemy-shell-lifecycle");
    Enemy enemy;
    enemy.id = 7;
    expect(!enemyDestructionComplete(enemy, {}),
           "live enemy reported destruction complete");
    enemy.destroyed = true;
    enemy.deathTimer = 0.01f;
    expect(!enemyDestructionComplete(enemy, {}),
           "enemy completed before death timer elapsed");
    enemy.deathTimer = 0.0f;
    expect(enemyDestructionComplete(enemy, {}),
           "enemy without owned shells did not complete");
    Shell playerShell;
    playerShell.owner = ShellOwner::Player;
    playerShell.ownerIndex = 7;
    Shell otherEnemyShell;
    otherEnemyShell.owner = ShellOwner::Enemy;
    otherEnemyShell.ownerIndex = 8;
    expect(enemyDestructionComplete(enemy,
                                    {playerShell, otherEnemyShell}),
           "unrelated shell blocked enemy cleanup");
    Shell ownedEnemyShell;
    ownedEnemyShell.owner = ShellOwner::Enemy;
    ownedEnemyShell.ownerIndex = 7;
    expect(!enemyDestructionComplete(enemy, {ownedEnemyShell}),
           "owned in-flight shell no longer delays enemy cleanup");

    const Player defaultPlayer;
    const Enemy defaultEnemy;
    const Shell defaultShell;
    expect(defaultPlayer.nation == Nation::UnitedStates &&
               defaultPlayer.driveDirection == CardinalDirection::North &&
               defaultPlayer.maximumHitPoints == 3 &&
               defaultPlayer.hitPoints == 3 && defaultPlayer.active,
           "Player defaults changed during extraction");
    expect(defaultEnemy.id == -1 && defaultEnemy.armor == 1 &&
               defaultEnemy.driveDirection == CardinalDirection::South &&
               nearlyEqual(defaultEnemy.yaw, 3.14159265358979323846f) &&
               nearlyEqual(defaultEnemy.directionDecisionInterval, 0.1f) &&
               nearlyEqual(defaultEnemy.movementDelay, 0.1f),
           "Enemy defaults changed during extraction");
    expect(defaultShell.owner == ShellOwner::Player &&
               defaultShell.ownerIndex == -1 && !defaultShell.power &&
               !defaultShell.impacting && nearlyEqual(defaultShell.life, 4.0f),
           "Shell defaults changed during extraction");

    reporter.finish();
    return passed ? 0 : 1;
}
