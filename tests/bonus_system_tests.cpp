#include "game/bonus_system.h"
#include "test_support.h"

#include <array>
#include <cmath>
#include <limits>
#include <string>
#include <vector>

namespace
{
using tanks3d::core::XZ;
using tanks3d::game::BonusApplication;
using tanks3d::game::BonusCollectionCallbacks;
using tanks3d::game::BonusCollectionIntent;
using tanks3d::game::BonusEffectCommand;
using tanks3d::game::BonusEffectCommandType;
using tanks3d::game::BonusPickupBatchOutcome;
using tanks3d::game::BonusPickupOutcome;
using tanks3d::game::BonusPickupTransaction;
using tanks3d::game::BonusReleaseIntent;
using tanks3d::game::BonusReleaseOutcome;
using tanks3d::game::BonusReleaseRandom;
using tanks3d::game::BonusSpawnDecision;
using tanks3d::game::BonusType;
using tanks3d::game::Enemy;
using tanks3d::game::Pickup;
using tanks3d::game::PickupDisposition;
using tanks3d::game::PickupUpdate;
using tanks3d::game::Player;
using tanks3d::game::anyPlayerNeedsHealing;
using tanks3d::game::advanceBonusPickups;
using tanks3d::game::advanceBonusPickupTransaction;
using tanks3d::game::advanceBonusReleaseTransaction;
using tanks3d::game::applyBonus;
using tanks3d::game::bonusPointsForGrenadeTargets;
using tanks3d::game::bonusSpawnFromDraws;
using tanks3d::game::kBonusBasePoints;
using tanks3d::game::kClockBonusDuration;
using tanks3d::game::kGrenadeCameraShake;
using tanks3d::game::kGrenadeEnemyPoints;
using tanks3d::game::kHelmetBonusDuration;
using tanks3d::game::kPickupLifetime;
using tanks3d::game::kPickupTankHitExtent;
using tanks3d::game::kTankDeathDuration;
using tanks3d::game::updatePickup;

bool nearlyEqual(float first, float second, float tolerance = 0.00001f)
{
    return first == second ||
           (std::isnan(first) && std::isnan(second)) ||
           std::fabs(first - second) <= tolerance;
}

bool samePosition(XZ first, XZ second)
{
    return nearlyEqual(first.x, second.x) && nearlyEqual(first.z, second.z);
}

bool samePickupState(const Pickup &first, const Pickup &second)
{
    return first.type == second.type &&
           samePosition(first.position, second.position) &&
           nearlyEqual(first.age, second.age) &&
           nearlyEqual(first.life, second.life);
}

bool samePlayerBonusState(const Player &first, const Player &second)
{
    return first.id == second.id && first.active == second.active &&
           samePosition(first.position, second.position) &&
           first.lives == second.lives &&
           first.maximumHitPoints == second.maximumHitPoints &&
           first.hitPoints == second.hitPoints &&
           first.level == second.level && first.hasBoat == second.hasBoat &&
           nearlyEqual(first.shieldTimer, second.shieldTimer) &&
           nearlyEqual(first.creationTimer, second.creationTimer) &&
           first.score == second.score &&
           first.directKillStreak == second.directKillStreak &&
           first.stageTally.destroyed == second.stageTally.destroyed &&
           first.stageTally.enemyPoints == second.stageTally.enemyPoints &&
           first.stageTally.bonusPoints == second.stageTally.bonusPoints;
}

bool sameEnemyBonusState(const Enemy &first, const Enemy &second)
{
    return first.id == second.id && first.type == second.type &&
           samePosition(first.position, second.position) &&
           first.armor == second.armor &&
           first.destroyed == second.destroyed &&
           first.moving == second.moving &&
           nearlyEqual(first.frozenTimer, second.frozenTimer) &&
           nearlyEqual(first.deathTimer, second.deathTimer);
}

Player testPlayer(int id = 7)
{
    Player player;
    player.id = id;
    player.position = {10.0f, 12.0f};
    player.lives = 4;
    player.maximumHitPoints = 3;
    player.hitPoints = 3;
    player.score = 100;
    player.stageTally.bonusPoints = 50;
    return player;
}

Enemy testEnemy(int id, int armor, XZ position)
{
    Enemy enemy;
    enemy.id = id;
    enemy.type = id % tanks3d::game::kEnemyTypeCount;
    enemy.armor = armor;
    enemy.position = position;
    enemy.moving = true;
    return enemy;
}

bool isDefaultFailure(const BonusApplication &application)
{
    return !application.applied && application.type == BonusType::Count &&
           application.playerIndex == -1 && application.playerId == -1 &&
           application.scoreDelta == 0 && application.commands.empty();
}

bool commandMatchesEnemy(const BonusEffectCommand &command,
                         BonusEffectCommandType type, const Enemy &snapshot,
                         int playerId, int armorBefore)
{
    return command.type == type && command.playerIndex == -1 &&
           command.playerId == playerId && command.enemyId == snapshot.id &&
           command.enemyType == snapshot.type &&
           samePosition(command.position, snapshot.position) &&
           command.valueBefore == armorBefore;
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

    reporter.beginSuite("bonus-pickup-defaults-and-contract-constants");
    const Pickup defaultPickup;
    expect(defaultPickup.type == BonusType::Grenade &&
               samePosition(defaultPickup.position, {}) &&
               nearlyEqual(defaultPickup.age, 0.0f) &&
               nearlyEqual(defaultPickup.life, kPickupLifetime),
           "default pickup state changed");
    expect(nearlyEqual(kPickupLifetime, 12.5f) &&
               nearlyEqual(kPickupTankHitExtent, 1.875f),
           "pickup lifetime or collision extent changed");
    expect(kBonusBasePoints == 300 && kGrenadeEnemyPoints == 200,
           "bonus score constants changed");
    expect(nearlyEqual(kHelmetBonusDuration, 10.0f) &&
               nearlyEqual(kClockBonusDuration, 8.0f) &&
               nearlyEqual(kGrenadeCameraShake, 0.34f),
           "bonus duration or camera command constants changed");
    Pickup positionedPickup;
    positionedPickup.position = {4.25f, 19.75f};
    expect(nearlyEqual(positionedPickup.position.x, 4.25f) &&
               nearlyEqual(positionedPickup.position.z, 19.75f),
           "pickup no longer stores renderer-independent XZ coordinates");

    reporter.beginSuite("bonus-spawn-classic-type-pool");
    for (int slot = 0; slot < 8; ++slot)
    {
        const BonusSpawnDecision healthy =
            bonusSpawnFromDraws(slot, false, 16, 32);
        const BonusSpawnDecision injured =
            bonusSpawnFromDraws(slot, true, 16, 32);
        expect(healthy.valid && injured.valid &&
                   healthy.pickup.type == static_cast<BonusType>(slot) &&
                   injured.pickup.type == static_cast<BonusType>(slot),
               "classic weighted slot changed at " + std::to_string(slot));
    }
    const BonusSpawnDecision healthyBandageSlot =
        bonusSpawnFromDraws(8, false, 16, 32);
    expect(!healthyBandageSlot.valid &&
               healthyBandageSlot.pickup.type == BonusType::Grenade &&
               samePosition(healthyBandageSlot.pickup.position, {}),
           "healthy pool admitted Bandage or changed failure defaults");
    expect(!bonusSpawnFromDraws(-1, false, 16, 32).valid &&
               !bonusSpawnFromDraws(10, true, 16, 32).valid,
           "out-of-range weighted slots became valid");

    reporter.beginSuite("bonus-spawn-bandage-weight-and-coordinates");
    const BonusSpawnDecision bandageEight =
        bonusSpawnFromDraws(8, true, 0, 0);
    const BonusSpawnDecision bandageNine =
        bonusSpawnFromDraws(9, true, 383, 383);
    expect(bandageEight.valid &&
               bandageEight.pickup.type == BonusType::Bandage,
           "first injured-only slot did not produce Bandage");
    expect(bandageNine.valid &&
               bandageNine.pickup.type == BonusType::Bandage,
           "second injured-only slot did not preserve double weight");
    expect(samePosition(bandageEight.pickup.position, {1.0f, 1.0f}),
           "minimum spawn draw did not target the first tile center");
    expect(samePosition(bandageNine.pickup.position,
                        {24.9375f, 24.9375f}),
           "maximum spawn draw changed coordinate conversion");
    const BonusSpawnDecision fractional =
        bonusSpawnFromDraws(3, false, 1, 15);
    expect(fractional.valid &&
               samePosition(fractional.pickup.position,
                            {1.0625f, 1.9375f}),
           "pixel draws lost sixteenth-tile precision");
    expect(nearlyEqual(fractional.pickup.age, 0.0f) &&
               nearlyEqual(fractional.pickup.life, kPickupLifetime),
           "spawn decision did not preserve pickup timing defaults");
    expect(!bonusSpawnFromDraws(0, false, -1, 0).valid &&
               !bonusSpawnFromDraws(0, false, 384, 0).valid,
           "invalid X draw entered the map");
    expect(!bonusSpawnFromDraws(0, false, 0, -1).valid &&
               !bonusSpawnFromDraws(0, false, 0, 384).valid,
           "invalid Z draw entered the map");

    reporter.beginSuite("bonus-release-transaction-invalid-dependencies");
    std::vector<Player> releasePlayers{testPlayer(15)};
    std::vector<char> invalidReleaseTranscript;
    const BonusReleaseRandom validReleaseRandom{
        [&](int) {
            invalidReleaseTranscript.push_back('T');
            return 0;
        },
        [&]() {
            invalidReleaseTranscript.push_back('P');
            return 0;
        }};
    const auto validPositionQuery = [&](XZ) {
        invalidReleaseTranscript.push_back('Q');
        return false;
    };
    const auto validReleaseCommit = [&](const BonusReleaseIntent &) {
        invalidReleaseTranscript.push_back('C');
    };
    const BonusReleaseOutcome missingTypeDraw =
        advanceBonusReleaseTransaction(
            releasePlayers, 1,
            BonusReleaseRandom{{}, validReleaseRandom.drawPixel},
            validPositionQuery, validReleaseCommit);
    const BonusReleaseOutcome missingPixelDraw =
        advanceBonusReleaseTransaction(
            releasePlayers, 1,
            BonusReleaseRandom{validReleaseRandom.drawTypeSlot, {}},
            validPositionQuery, validReleaseCommit);
    const BonusReleaseOutcome missingPositionQuery =
        advanceBonusReleaseTransaction(
            releasePlayers, 1, validReleaseRandom, {},
            validReleaseCommit);
    const BonusReleaseOutcome missingCommit =
        advanceBonusReleaseTransaction(
            releasePlayers, 1, validReleaseRandom,
            validPositionQuery, {});
    expect(missingTypeDraw == BonusReleaseOutcome::Invalid &&
               missingPixelDraw == BonusReleaseOutcome::Invalid &&
               missingPositionQuery == BonusReleaseOutcome::Invalid &&
               missingCommit == BonusReleaseOutcome::Invalid &&
               invalidReleaseTranscript.empty(),
           "invalid release dependencies invoked a draw, query, or commit");

    reporter.beginSuite("bonus-release-transaction-healthy-order-and-payload");
    std::vector<char> healthyReleaseTranscript;
    std::array<int, 2> healthyPixels{{1, 15}};
    std::size_t healthyPixelIndex = 0U;
    bool healthySlotRangeCorrect = false;
    BonusReleaseIntent healthyIntent;
    bool healthyCommitSawQuery = false;
    const BonusReleaseOutcome healthyRelease =
        advanceBonusReleaseTransaction(
            releasePlayers, -1,
            BonusReleaseRandom{
                [&](int slotCount) {
                    healthyReleaseTranscript.push_back('T');
                    healthySlotRangeCorrect = slotCount == 8;
                    return 7;
                },
                [&]() {
                    healthyReleaseTranscript.push_back(
                        healthyPixelIndex == 0U ? 'X' : 'Z');
                    return healthyPixels[healthyPixelIndex++];
                }},
            [&](XZ position) {
                healthyReleaseTranscript.push_back('Q');
                return !samePosition(position, {1.0625f, 1.9375f});
            },
            [&](const BonusReleaseIntent &intent) {
                healthyCommitSawQuery =
                    healthyReleaseTranscript ==
                        std::vector<char>({'T', 'X', 'Z', 'Q'});
                healthyReleaseTranscript.push_back('C');
                healthyIntent = intent;
            });
    expect(healthyRelease == BonusReleaseOutcome::Released &&
               healthySlotRangeCorrect && healthyPixelIndex == 2U &&
               healthyCommitSawQuery &&
               healthyReleaseTranscript ==
                   std::vector<char>({'T', 'X', 'Z', 'Q', 'C'}) &&
               healthyIntent.sourceEnemyId == -1 &&
               healthyIntent.pickup.type == BonusType::Boat &&
               samePosition(healthyIntent.pickup.position,
                            {1.0625f, 1.9375f}) &&
               nearlyEqual(healthyIntent.pickup.age, 0.0f) &&
               nearlyEqual(healthyIntent.pickup.life, kPickupLifetime),
           "healthy release changed its range, order, or detached payload");

    reporter.beginSuite("bonus-release-transaction-bandage-retry-transcript");
    Player injuredReleasePlayer = testPlayer(16);
    injuredReleasePlayer.hitPoints = 2;
    injuredReleasePlayer.creationTimer = 5.0f;
    std::vector<Player> injuredReleasePlayers{injuredReleasePlayer};
    const std::array<int, 6> retryPixels{{192, 362, 192, 362, 0, 0}};
    std::size_t retryPixelIndex = 0U;
    int retryTypeDraws = 0;
    int retryQueries = 0;
    int retryCommits = 0;
    bool injuredSlotRangeCorrect = false;
    BonusReleaseIntent retryIntent;
    std::vector<char> retryTranscript;
    const BonusReleaseOutcome retriedRelease =
        advanceBonusReleaseTransaction(
            injuredReleasePlayers, 77,
            BonusReleaseRandom{
                [&](int slotCount) {
                    retryTranscript.push_back('T');
                    ++retryTypeDraws;
                    injuredSlotRangeCorrect = slotCount == 10;
                    return 8;
                },
                [&]() {
                    retryTranscript.push_back(
                        retryPixelIndex % 2U == 0U ? 'X' : 'Z');
                    return retryPixels[retryPixelIndex++];
                }},
            [&](XZ position) {
                retryTranscript.push_back('Q');
                ++retryQueries;
                return samePosition(position, {13.0f, 23.625f});
            },
            [&](const BonusReleaseIntent &intent) {
                retryTranscript.push_back('C');
                ++retryCommits;
                retryIntent = intent;
            });
    expect(retriedRelease == BonusReleaseOutcome::Released &&
               injuredSlotRangeCorrect && retryTypeDraws == 1 &&
               retryPixelIndex == retryPixels.size() &&
               retryQueries == 3 && retryCommits == 1 &&
               retryTranscript == std::vector<char>({
                   'T', 'X', 'Z', 'Q', 'X', 'Z', 'Q',
                   'X', 'Z', 'Q', 'C'}) &&
               retryIntent.sourceEnemyId == 77 &&
               retryIntent.pickup.type == BonusType::Bandage &&
               samePosition(retryIntent.pickup.position, {1.0f, 1.0f}) &&
               nearlyEqual(retryIntent.pickup.age, 0.0f) &&
               nearlyEqual(retryIntent.pickup.life, kPickupLifetime),
           "Bandage release redrew its type or changed X/Z retry order");

    reporter.beginSuite("bonus-release-transaction-dynamic-draw-validation");
    const auto invalidDynamicDraw =
        [&](int typeSlot, int xPixel, int zPixel,
            const std::string &label) {
            std::array<int, 2> pixels{{xPixel, zPixel}};
            std::size_t pixelIndex = 0U;
            int typeDrawCount = 0;
            int queryCount = 0;
            int commitCount = 0;
            std::vector<char> transcript;
            const BonusReleaseOutcome outcome =
                advanceBonusReleaseTransaction(
                    releasePlayers, std::numeric_limits<int>::min(),
                    BonusReleaseRandom{
                        [&](int slotCount) {
                            transcript.push_back('T');
                            ++typeDrawCount;
                            expect(slotCount == 8,
                                   label + " changed healthy slot count");
                            return typeSlot;
                        },
                        [&]() {
                            const std::size_t index = pixelIndex++;
                            transcript.push_back(
                                index % 2U == 0U ? 'X' : 'Z');
                            return index < pixels.size() ? pixels[index] : 0;
                        }},
                    [&](XZ) {
                        transcript.push_back('Q');
                        ++queryCount;
                        return false;
                    },
                    [&](const BonusReleaseIntent &) {
                        transcript.push_back('C');
                        ++commitCount;
                    });
            expect(outcome == BonusReleaseOutcome::Invalid &&
                       typeDrawCount == 1 && pixelIndex == 2U &&
                       queryCount == 0 && commitCount == 0 &&
                       transcript ==
                           std::vector<char>({'T', 'X', 'Z'}),
                   label + " did not fail after exactly one T-X-Z draw");
        };
    invalidDynamicDraw(-1, 0, 0, "invalid type draw");
    invalidDynamicDraw(0, 384, 0, "invalid X draw");
    invalidDynamicDraw(0, 0, -1, "invalid Z draw");

    reporter.beginSuite("bonus-release-transaction-retry-then-invalid");
    const std::array<int, 4> retryInvalidPixels{{192, 362, 384, 0}};
    std::size_t retryInvalidPixelIndex = 0U;
    int retryInvalidTypeDraws = 0;
    int retryInvalidQueries = 0;
    int retryInvalidCommits = 0;
    std::vector<char> retryInvalidTranscript;
    const BonusReleaseOutcome retryThenInvalid =
        advanceBonusReleaseTransaction(
            releasePlayers, 78,
            BonusReleaseRandom{
                [&](int slotCount) {
                    retryInvalidTranscript.push_back('T');
                    ++retryInvalidTypeDraws;
                    expect(slotCount == 8,
                           "retry-invalid path changed healthy slot count");
                    return 0;
                },
                [&]() {
                    const std::size_t index = retryInvalidPixelIndex++;
                    retryInvalidTranscript.push_back(
                        index % 2U == 0U ? 'X' : 'Z');
                    return index < retryInvalidPixels.size()
                               ? retryInvalidPixels[index]
                               : 0;
                }},
            [&](XZ position) {
                retryInvalidTranscript.push_back('Q');
                ++retryInvalidQueries;
                return samePosition(position, {13.0f, 23.625f});
            },
            [&](const BonusReleaseIntent &) {
                retryInvalidTranscript.push_back('C');
                ++retryInvalidCommits;
            });
    expect(retryThenInvalid == BonusReleaseOutcome::Invalid &&
               retryInvalidTypeDraws == 1 &&
               retryInvalidPixelIndex == retryInvalidPixels.size() &&
               retryInvalidQueries == 1 && retryInvalidCommits == 0 &&
               retryInvalidTranscript == std::vector<char>({
                   'T', 'X', 'Z', 'Q', 'X', 'Z'}),
           "malformed retry candidate reached a second query or commit");

    reporter.beginSuite("bonus-grenade-point-calculation-saturation");
    const int maximumPointTotal = std::numeric_limits<int>::max();
    const std::size_t maximumScoredTargets = static_cast<std::size_t>(
        (maximumPointTotal - kBonusBasePoints) / kGrenadeEnemyPoints);
    expect(bonusPointsForGrenadeTargets(0U) == kBonusBasePoints,
           "zero-target Grenade lost its common pickup points");
    expect(bonusPointsForGrenadeTargets(1U) ==
               kBonusBasePoints + kGrenadeEnemyPoints,
           "one-target Grenade used the wrong point increment");
    expect(bonusPointsForGrenadeTargets(maximumScoredTargets) ==
               kBonusBasePoints +
                   static_cast<int>(maximumScoredTargets) *
                       kGrenadeEnemyPoints,
           "largest exactly representable Grenade total changed");
    expect(bonusPointsForGrenadeTargets(maximumScoredTargets + 1U) ==
               maximumPointTotal &&
               bonusPointsForGrenadeTargets(
                   std::numeric_limits<std::size_t>::max()) ==
                   maximumPointTotal,
           "extreme Grenade target count did not saturate at INT_MAX");

    reporter.beginSuite("bonus-healing-pool-eligibility");
    expect(!anyPlayerNeedsHealing({}),
           "empty player list enabled Bandage spawning");
    Player healthy = testPlayer();
    expect(!anyPlayerNeedsHealing({healthy}),
           "full-health player enabled Bandage spawning");
    Player injured = healthy;
    injured.hitPoints = 2;
    expect(anyPlayerNeedsHealing({healthy, injured}),
           "injured active player did not enable Bandage spawning");
    injured.active = false;
    expect(!anyPlayerNeedsHealing({injured}),
           "inactive injured player enabled Bandage spawning");
    injured.active = true;
    injured.hitPoints = 0;
    expect(!anyPlayerNeedsHealing({injured}),
           "destroyed player enabled Bandage spawning");
    injured.maximumHitPoints = 1;
    injured.hitPoints = 0;
    expect(!anyPlayerNeedsHealing({injured}),
           "one-HP ruleset enabled Bandage spawning");
    injured.maximumHitPoints = 6;
    injured.hitPoints = 5;
    expect(anyPlayerNeedsHealing({injured, healthy}),
           "six-HP injured player did not enable Bandage spawning");

    reporter.beginSuite("bonus-pickup-lifecycle");
    Pickup aging;
    aging.type = BonusType::Star;
    aging.position = {20.0f, 20.0f};
    const PickupUpdate firstAge = updatePickup(aging, 0.25f, {});
    expect(firstAge.disposition == PickupDisposition::Retain &&
               firstAge.collectorIndex == -1 &&
               nearlyEqual(aging.age, 0.25f) &&
               nearlyEqual(aging.life, 12.25f),
           "valid elapsed time did not advance pickup clocks");
    const PickupUpdate secondAge = updatePickup(aging, 1.5f, {});
    expect(secondAge.disposition == PickupDisposition::Retain &&
               nearlyEqual(aging.age, 1.75f) &&
               nearlyEqual(aging.life, 10.75f),
           "successive pickup updates lost elapsed time");
    Pickup exactExpiry;
    exactExpiry.life = 0.5f;
    const PickupUpdate exactExpired = updatePickup(exactExpiry, 0.5f, {});
    expect(exactExpired.disposition == PickupDisposition::Remove &&
               exactExpired.collectorIndex == -1 &&
               nearlyEqual(exactExpiry.age, 0.5f) &&
               nearlyEqual(exactExpiry.life, 0.0f),
           "pickup did not expire at the exact zero-life boundary");
    Pickup pastExpiry;
    pastExpiry.life = 0.1f;
    const PickupUpdate pastExpired = updatePickup(pastExpiry, 1.0f, {});
    expect(pastExpired.disposition == PickupDisposition::Remove &&
               nearlyEqual(pastExpiry.age, 1.0f) &&
               pastExpiry.life < 0.0f,
           "large elapsed time did not expire pickup");
    Player overlapping = testPlayer();
    Pickup expiresBeforeCollection;
    expiresBeforeCollection.position = overlapping.position;
    expiresBeforeCollection.life = 0.1f;
    const PickupUpdate expiryWins =
        updatePickup(expiresBeforeCollection, 0.1f, {overlapping});
    expect(expiryWins.disposition == PickupDisposition::Remove &&
               expiryWins.collectorIndex == -1,
           "collection occurred after pickup expired in the same update");

    reporter.beginSuite("bonus-pickup-invalid-time-fails-safe");
    for (float elapsedTime : {
             -0.001f, -100.0f,
             std::numeric_limits<float>::quiet_NaN(),
             std::numeric_limits<float>::infinity()})
    {
        Pickup pickup;
        pickup.type = BonusType::Clock;
        pickup.position = {3.0f, 5.0f};
        pickup.age = 2.0f;
        pickup.life = 7.0f;
        const PickupUpdate result = updatePickup(pickup, elapsedTime, {});
        expect(result.disposition == PickupDisposition::Retain &&
                   result.collectorIndex == -1 &&
                   pickup.type == BonusType::Clock &&
                   samePosition(pickup.position, {3.0f, 5.0f}) &&
                   nearlyEqual(pickup.age, 2.0f) &&
                   nearlyEqual(pickup.life, 7.0f),
               "invalid elapsed time mutated pickup state");
    }
    Pickup malformedButBadTime;
    malformedButBadTime.life = 0.0f;
    const PickupUpdate badTimePrecedesValidation =
        updatePickup(malformedButBadTime, -1.0f, {});
    expect(badTimePrecedesValidation.disposition ==
                   PickupDisposition::Retain &&
               nearlyEqual(malformedButBadTime.life, 0.0f),
           "invalid elapsed time no longer behaves as a complete no-op");

    reporter.beginSuite("bonus-pickup-malformed-state-removal");
    const auto expectRemoved = [&](Pickup pickup, const std::string &label) {
        const PickupUpdate result = updatePickup(pickup, 0.0f, {});
        expect(result.disposition == PickupDisposition::Remove &&
                   result.collectorIndex == -1,
               label + " was not removed");
    };
    Pickup malformed;
    malformed.age = std::numeric_limits<float>::quiet_NaN();
    expectRemoved(malformed, "NaN age");
    malformed = {};
    malformed.age = std::numeric_limits<float>::infinity();
    expectRemoved(malformed, "infinite age");
    malformed = {};
    malformed.life = std::numeric_limits<float>::quiet_NaN();
    expectRemoved(malformed, "NaN life");
    malformed = {};
    malformed.life = std::numeric_limits<float>::infinity();
    expectRemoved(malformed, "infinite life");
    malformed = {};
    malformed.position.x = std::numeric_limits<float>::infinity();
    expectRemoved(malformed, "infinite X");
    malformed = {};
    malformed.position.z = std::numeric_limits<float>::quiet_NaN();
    expectRemoved(malformed, "NaN Z");
    malformed = {};
    malformed.life = 0.0f;
    expectRemoved(malformed, "zero life");
    malformed = {};
    malformed.life = -1.0f;
    expectRemoved(malformed, "negative life");
    malformed = {};
    malformed.type = BonusType::Count;
    expectRemoved(malformed, "bonus sentinel");
    malformed = {};
    malformed.type = static_cast<BonusType>(255);
    expectRemoved(malformed, "out-of-range bonus type");
    malformed = {};
    malformed.age = std::numeric_limits<float>::max();
    malformed.life = std::numeric_limits<float>::max();
    const PickupUpdate overflowed = updatePickup(
        malformed, std::numeric_limits<float>::max(), {});
    expect(overflowed.disposition == PickupDisposition::Remove,
           "overflowed pickup clocks were retained");

    reporter.beginSuite("bonus-pickup-strict-aabb-collection");
    Player collector = testPlayer(20);
    const auto collectionAt = [&](XZ position) {
        Pickup pickup;
        pickup.type = BonusType::Star;
        pickup.position = position;
        return updatePickup(pickup, 0.0f, {collector});
    };
    expect(collectionAt(collector.position).disposition ==
                   PickupDisposition::Collect,
           "coincident player did not collect pickup");
    expect(collectionAt({collector.position.x + kPickupTankHitExtent - 0.0001f,
                         collector.position.z})
                   .disposition == PickupDisposition::Collect,
           "point just inside positive X extent did not collect");
    expect(collectionAt({collector.position.x - kPickupTankHitExtent + 0.0001f,
                         collector.position.z})
                   .disposition == PickupDisposition::Collect,
           "point just inside negative X extent did not collect");
    expect(collectionAt({collector.position.x + kPickupTankHitExtent,
                         collector.position.z})
                   .disposition == PickupDisposition::Retain,
           "exact positive X boundary stopped being strict");
    expect(collectionAt({collector.position.x,
                         collector.position.z - kPickupTankHitExtent})
                   .disposition == PickupDisposition::Retain,
           "exact negative Z boundary stopped being strict");
    expect(collectionAt(
               {collector.position.x + kPickupTankHitExtent - 0.0001f,
                collector.position.z + kPickupTankHitExtent - 0.0001f})
                   .disposition == PickupDisposition::Collect,
           "diagonal point inside both strict extents did not collect");
    expect(collectionAt(
               {collector.position.x + kPickupTankHitExtent - 0.0001f,
                collector.position.z + kPickupTankHitExtent})
                   .disposition == PickupDisposition::Retain,
           "one exact axis boundary was accepted by AABB collection");

    reporter.beginSuite("bonus-pickup-first-eligible-player");
    Pickup shared;
    shared.type = BonusType::Helmet;
    shared.position = {4.0f, 4.0f};
    Player inactive = testPlayer(0);
    inactive.position = shared.position;
    inactive.active = false;
    Player creating = testPlayer(1);
    creating.position = shared.position;
    creating.creationTimer = 0.01f;
    Player distant = testPlayer(2);
    distant.position = {20.0f, 20.0f};
    Player invalidTimer = testPlayer(4);
    invalidTimer.position = shared.position;
    invalidTimer.creationTimer =
        std::numeric_limits<float>::quiet_NaN();
    Player eligible = testPlayer(3);
    eligible.position = shared.position;
    PickupUpdate selected =
        updatePickup(shared, 0.0f,
                     {inactive, creating, distant, invalidTimer, eligible});
    expect(selected.disposition == PickupDisposition::Collect &&
               selected.collectorIndex == 4,
           "inactive, creating, distant, or invalid-timer player blocked "
           "later collector");
    Player first = eligible;
    first.id = 30;
    Player second = eligible;
    second.id = 31;
    shared.age = 0.0f;
    shared.life = kPickupLifetime;
    selected = updatePickup(shared, 0.0f, {first, second});
    expect(selected.disposition == PickupDisposition::Collect &&
               selected.collectorIndex == 0,
           "shared pickup did not choose first player in vector order");
    first.creationTimer = 0.0f;
    selected = updatePickup(shared, 0.0f, {first});
    expect(selected.disposition == PickupDisposition::Collect &&
               selected.collectorIndex == 0,
           "zero creation timer was not collectible");
    selected = updatePickup(shared, 0.0f, {});
    expect(selected.disposition == PickupDisposition::Retain &&
               selected.collectorIndex == -1,
           "empty player list produced a collector");

    reporter.beginSuite("bonus-pickup-bandage-selection");
    Pickup bandage;
    bandage.type = BonusType::Bandage;
    bandage.position = {8.0f, 8.0f};
    Player full = testPlayer(40);
    full.position = bandage.position;
    Player hurt = testPlayer(41);
    hurt.position = bandage.position;
    hurt.hitPoints = 1;
    PickupUpdate bandageSelection =
        updatePickup(bandage, 0.0f, {full, hurt});
    expect(bandageSelection.disposition == PickupDisposition::Collect &&
               bandageSelection.collectorIndex == 1,
           "healthy first player blocked injured Bandage collector");
    bandageSelection = updatePickup(bandage, 0.0f, {full});
    expect(bandageSelection.disposition == PickupDisposition::Retain &&
               bandageSelection.collectorIndex == -1,
           "full-health player collected Bandage");
    Player oneHitPointRules = full;
    oneHitPointRules.maximumHitPoints = 1;
    oneHitPointRules.hitPoints = 1;
    bandageSelection = updatePickup(bandage, 0.0f, {oneHitPointRules});
    expect(bandageSelection.disposition == PickupDisposition::Retain,
           "one-HP ruleset admitted Bandage collection");
    hurt.active = false;
    bandageSelection = updatePickup(bandage, 0.0f, {hurt});
    expect(bandageSelection.disposition == PickupDisposition::Retain,
           "inactive injured player collected Bandage");

    reporter.beginSuite("bonus-pickup-transaction-missing-callbacks-atomic");
    Pickup callbackGuardPickup;
    callbackGuardPickup.type = BonusType::Star;
    callbackGuardPickup.position = {10.0f, 12.0f};
    callbackGuardPickup.age = 2.0f;
    callbackGuardPickup.life = 7.0f;
    const Pickup callbackGuardPickupBefore = callbackGuardPickup;
    std::vector<Player> callbackGuardPlayers{testPlayer(45)};
    std::vector<Enemy> callbackGuardEnemies{
        testEnemy(45, 3, {6.0f, 7.0f})};
    const std::vector<Player> callbackGuardPlayersBefore =
        callbackGuardPlayers;
    const std::vector<Enemy> callbackGuardEnemiesBefore =
        callbackGuardEnemies;
    std::vector<char> callbackGuardTranscript;
    const auto callbackGuardStarted = [&](const BonusCollectionIntent &) {
        callbackGuardTranscript.push_back('B');
    };
    const auto callbackGuardFinished =
        [&](const BonusCollectionIntent &, const BonusApplication &) {
            callbackGuardTranscript.push_back('F');
        };
    const BonusPickupTransaction missingStarted =
        advanceBonusPickupTransaction(
            callbackGuardPickup, 0.5f, callbackGuardPlayers,
            callbackGuardEnemies,
            BonusCollectionCallbacks{{}, callbackGuardFinished});
    const BonusPickupTransaction missingFinished =
        advanceBonusPickupTransaction(
            callbackGuardPickup, 0.5f, callbackGuardPlayers,
            callbackGuardEnemies,
            BonusCollectionCallbacks{callbackGuardStarted, {}});
    expect(missingStarted.outcome == BonusPickupOutcome::Invalid &&
               missingFinished.outcome == BonusPickupOutcome::Invalid &&
               missingStarted.update.disposition ==
                   PickupDisposition::Retain &&
               missingStarted.update.collectorIndex == -1 &&
               missingFinished.update.disposition ==
                   PickupDisposition::Retain &&
               missingFinished.update.collectorIndex == -1 &&
               isDefaultFailure(missingStarted.application) &&
               isDefaultFailure(missingFinished.application) &&
               callbackGuardTranscript.empty() &&
               callbackGuardPickup.type == callbackGuardPickupBefore.type &&
               samePosition(callbackGuardPickup.position,
                            callbackGuardPickupBefore.position) &&
               nearlyEqual(callbackGuardPickup.age,
                           callbackGuardPickupBefore.age) &&
               nearlyEqual(callbackGuardPickup.life,
                           callbackGuardPickupBefore.life) &&
               samePlayerBonusState(callbackGuardPlayers[0],
                                    callbackGuardPlayersBefore[0]) &&
               sameEnemyBonusState(callbackGuardEnemies[0],
                                   callbackGuardEnemiesBefore[0]),
           "missing pickup transaction callback was not completely atomic");

    reporter.beginSuite("bonus-pickup-transaction-retain-and-discard");
    std::vector<char> noCollectionTranscript;
    const BonusCollectionCallbacks noCollectionCallbacks{
        [&](const BonusCollectionIntent &) {
            noCollectionTranscript.push_back('B');
        },
        [&](const BonusCollectionIntent &, const BonusApplication &) {
            noCollectionTranscript.push_back('F');
        }};
    std::vector<Player> noCollectionPlayers{testPlayer(46)};
    std::vector<Enemy> noCollectionEnemies{
        testEnemy(46, 2, {5.0f, 6.0f})};
    const Player noCollectionPlayerBefore = noCollectionPlayers[0];
    const Enemy noCollectionEnemyBefore = noCollectionEnemies[0];
    Pickup retainedPickup;
    retainedPickup.type = BonusType::Clock;
    retainedPickup.position = {20.0f, 20.0f};
    retainedPickup.age = 1.0f;
    retainedPickup.life = 7.0f;
    const BonusPickupTransaction retainedTransaction =
        advanceBonusPickupTransaction(
            retainedPickup, 0.25f, noCollectionPlayers,
            noCollectionEnemies, noCollectionCallbacks);
    expect(retainedTransaction.outcome == BonusPickupOutcome::Retained &&
               retainedTransaction.update.disposition ==
                   PickupDisposition::Retain &&
               retainedTransaction.update.collectorIndex == -1 &&
               retainedTransaction.intent.collectorIndex == -1 &&
               retainedTransaction.intent.playerId == -1 &&
               isDefaultFailure(retainedTransaction.application) &&
               retainedPickup.type == BonusType::Clock &&
               samePosition(retainedPickup.position, {20.0f, 20.0f}) &&
               nearlyEqual(retainedPickup.age, 1.25f) &&
               nearlyEqual(retainedPickup.life, 6.75f) &&
               noCollectionTranscript.empty() &&
               samePlayerBonusState(noCollectionPlayers[0],
                                    noCollectionPlayerBefore) &&
               sameEnemyBonusState(noCollectionEnemies[0],
                                   noCollectionEnemyBefore),
           "retained pickup transaction changed U state or invoked callbacks");

    Pickup discardedPickup;
    discardedPickup.type = BonusType::Helmet;
    discardedPickup.position = noCollectionPlayers[0].position;
    discardedPickup.age = 2.0f;
    discardedPickup.life = 0.5f;
    const BonusPickupTransaction discardedTransaction =
        advanceBonusPickupTransaction(
            discardedPickup, 0.5f, noCollectionPlayers,
            noCollectionEnemies, noCollectionCallbacks);
    expect(discardedTransaction.outcome == BonusPickupOutcome::Discarded &&
               discardedTransaction.update.disposition ==
                   PickupDisposition::Remove &&
               discardedTransaction.update.collectorIndex == -1 &&
               discardedTransaction.intent.collectorIndex == -1 &&
               discardedTransaction.intent.playerId == -1 &&
               isDefaultFailure(discardedTransaction.application) &&
               discardedPickup.type == BonusType::Helmet &&
               samePosition(discardedPickup.position,
                            noCollectionPlayers[0].position) &&
               nearlyEqual(discardedPickup.age, 2.5f) &&
               nearlyEqual(discardedPickup.life, 0.0f) &&
               noCollectionTranscript.empty() &&
               samePlayerBonusState(noCollectionPlayers[0],
                                    noCollectionPlayerBefore) &&
               sameEnemyBonusState(noCollectionEnemies[0],
                                   noCollectionEnemyBefore),
           "discarded pickup transaction lost exact-expiry U semantics");

    reporter.beginSuite("bonus-pickup-transaction-grenade-bf-order");
    Player transactionFirstPlayer = testPlayer(50);
    transactionFirstPlayer.position = {2.0f, 3.0f};
    Player transactionCollector = testPlayer(91);
    transactionCollector.position = {10.0f, 12.0f};
    transactionCollector.directKillStreak = 4;
    transactionCollector.stageTally.destroyed = {{1, 2, 3, 4}};
    transactionCollector.stageTally.enemyPoints = {{10, 20, 30, 40}};
    const Player transactionFirstPlayerBefore = transactionFirstPlayer;
    const Player transactionCollectorBefore = transactionCollector;
    std::vector<Player> transactionPlayers{
        transactionFirstPlayer, transactionCollector};
    Enemy transactionLight = testEnemy(52, 1, {5.0f, 6.0f});
    Enemy transactionArmored = testEnemy(55, 3, {7.0f, 8.0f});
    const Enemy transactionLightBefore = transactionLight;
    const Enemy transactionArmoredBefore = transactionArmored;
    std::vector<Enemy> transactionEnemies{
        transactionLight, transactionArmored};
    Pickup transactionGrenade;
    transactionGrenade.type = BonusType::Grenade;
    transactionGrenade.position = transactionCollector.position;
    transactionGrenade.age = 1.25f;
    transactionGrenade.life = 8.0f;
    std::vector<char> collectionTranscript;
    BonusCollectionIntent startedIntent;
    BonusCollectionIntent finishedIntent;
    BonusApplication finishedApplication;
    bool startedSawPostUpdatePreApplication = false;
    bool finishedSawPostApplication = false;
    const auto transactionIntentMatches =
        [&](const BonusCollectionIntent &intent) {
            return intent.pickup.type == BonusType::Grenade &&
                   samePosition(intent.pickup.position, {10.0f, 12.0f}) &&
                   nearlyEqual(intent.pickup.age, 1.5f) &&
                   nearlyEqual(intent.pickup.life, 7.75f) &&
                   intent.collectorIndex == 1 && intent.playerId == 91;
        };
    const auto transactionApplicationMatches =
        [&](const BonusApplication &application) {
            if (!application.applied ||
                application.type != BonusType::Grenade ||
                application.playerIndex != 1 || application.playerId != 91 ||
                application.scoreDelta != 700 ||
                application.commands.size() != 9U)
            {
                return false;
            }
            const std::vector<BonusEffectCommand> &commands =
                application.commands;
            return commandMatchesEnemy(
                       commands[0],
                       BonusEffectCommandType::EnemyDestroyedCue,
                       transactionLightBefore, 91, 1) &&
                   commandMatchesEnemy(
                       commands[1], BonusEffectCommandType::EnemyExplosion,
                       transactionLightBefore, 91, 1) &&
                   commandMatchesEnemy(
                       commands[2],
                       BonusEffectCommandType::EnemyDestroyedEvent,
                       transactionLightBefore, 91, 1) &&
                   commands[2].valueAfter == 0 &&
                   commands[2].points == kGrenadeEnemyPoints &&
                   commandMatchesEnemy(
                       commands[3],
                       BonusEffectCommandType::EnemyArmorHitCue,
                       transactionArmoredBefore, 91, 3) &&
                   commandMatchesEnemy(
                       commands[4],
                       BonusEffectCommandType::EnemyArmorHitCue,
                       transactionArmoredBefore, 91, 3) &&
                   commandMatchesEnemy(
                       commands[5],
                       BonusEffectCommandType::EnemyDestroyedCue,
                       transactionArmoredBefore, 91, 3) &&
                   commandMatchesEnemy(
                       commands[6], BonusEffectCommandType::EnemyExplosion,
                       transactionArmoredBefore, 91, 3) &&
                   commandMatchesEnemy(
                       commands[7],
                       BonusEffectCommandType::EnemyDestroyedEvent,
                       transactionArmoredBefore, 91, 3) &&
                   commands[7].valueAfter == 0 &&
                   commands[7].points == kGrenadeEnemyPoints &&
                   commands[8].type ==
                       BonusEffectCommandType::AssignPlayerCameraShake &&
                   commands[8].playerIndex == 1 &&
                   commands[8].playerId == 91 &&
                   commands[8].enemyId == -1 &&
                   commands[8].enemyType == -1 &&
                   nearlyEqual(commands[8].scalar,
                               kGrenadeCameraShake);
        };
    const BonusPickupTransaction collectedTransaction =
        advanceBonusPickupTransaction(
            transactionGrenade, 0.25f, transactionPlayers,
            transactionEnemies,
            BonusCollectionCallbacks{
                [&](const BonusCollectionIntent &intent) {
                    startedSawPostUpdatePreApplication =
                        collectionTranscript.empty() &&
                        transactionIntentMatches(intent) &&
                        samePlayerBonusState(
                            transactionPlayers[0],
                            transactionFirstPlayerBefore) &&
                        samePlayerBonusState(
                            transactionPlayers[1],
                            transactionCollectorBefore) &&
                        sameEnemyBonusState(
                            transactionEnemies[0],
                            transactionLightBefore) &&
                        sameEnemyBonusState(
                            transactionEnemies[1],
                            transactionArmoredBefore);
                    collectionTranscript.push_back('B');
                    startedIntent = intent;
                },
                [&](const BonusCollectionIntent &intent,
                    const BonusApplication &application) {
                    finishedSawPostApplication =
                        collectionTranscript ==
                            std::vector<char>({'B'}) &&
                        transactionIntentMatches(intent) &&
                        transactionApplicationMatches(application) &&
                        samePlayerBonusState(
                            transactionPlayers[0],
                            transactionFirstPlayerBefore) &&
                        transactionPlayers[1].score == 800 &&
                        transactionPlayers[1].stageTally.bonusPoints == 750 &&
                        transactionPlayers[1].stageTally.destroyed ==
                            transactionCollectorBefore.stageTally.destroyed &&
                        transactionPlayers[1].stageTally.enemyPoints ==
                            transactionCollectorBefore.stageTally.enemyPoints &&
                        transactionPlayers[1].directKillStreak == 4 &&
                        transactionEnemies[0].armor == 0 &&
                        transactionEnemies[0].destroyed &&
                        !transactionEnemies[0].moving &&
                        nearlyEqual(transactionEnemies[0].deathTimer,
                                    kTankDeathDuration) &&
                        transactionEnemies[1].armor == 0 &&
                        transactionEnemies[1].destroyed &&
                        !transactionEnemies[1].moving &&
                        nearlyEqual(transactionEnemies[1].deathTimer,
                                    kTankDeathDuration);
                    collectionTranscript.push_back('F');
                    finishedIntent = intent;
                    finishedApplication = application;
                }});
    expect(collectedTransaction.outcome == BonusPickupOutcome::Collected &&
               collectedTransaction.update.disposition ==
                   PickupDisposition::Collect &&
               collectedTransaction.update.collectorIndex == 1 &&
               transactionIntentMatches(collectedTransaction.intent) &&
               transactionApplicationMatches(
                   collectedTransaction.application) &&
               transactionIntentMatches(startedIntent) &&
               transactionIntentMatches(finishedIntent) &&
               transactionApplicationMatches(finishedApplication) &&
               startedSawPostUpdatePreApplication &&
               finishedSawPostApplication &&
               collectionTranscript == std::vector<char>({'B', 'F'}) &&
               transactionGrenade.type == BonusType::Grenade &&
               samePosition(transactionGrenade.position, {10.0f, 12.0f}) &&
               nearlyEqual(transactionGrenade.age, 1.5f) &&
               nearlyEqual(transactionGrenade.life, 7.75f) &&
               transactionPlayers[1].score == 800 &&
               transactionPlayers[1].stageTally.bonusPoints == 750 &&
               transactionEnemies[0].destroyed &&
               transactionEnemies[1].destroyed,
           "Grenade pickup transaction changed U-B-A-F order or payload");

    reporter.beginSuite(
        "bonus-pickup-transaction-adversarial-application-rejection");
    // Deliberately violates the callback contract by mutating the collector.
    // This locks only the transaction's defensive ApplicationRejected path.
    Pickup adversarialPickup;
    adversarialPickup.type = BonusType::Star;
    adversarialPickup.position = {10.0f, 12.0f};
    adversarialPickup.age = 3.0f;
    adversarialPickup.life = 6.0f;
    std::vector<Player> adversarialPlayers{testPlayer(73)};
    std::vector<Enemy> adversarialEnemies{
        testEnemy(73, 2, {6.0f, 7.0f})};
    const Enemy adversarialEnemyBefore = adversarialEnemies[0];
    std::vector<char> adversarialTranscript;
    bool adversarialFinishedSawRejection = false;
    const BonusPickupTransaction rejectedTransaction =
        advanceBonusPickupTransaction(
            adversarialPickup, 0.25f, adversarialPlayers,
            adversarialEnemies,
            BonusCollectionCallbacks{
                [&](const BonusCollectionIntent &intent) {
                    adversarialTranscript.push_back('B');
                    if (intent.collectorIndex == 0 && intent.playerId == 73)
                        adversarialPlayers[0].active = false;
                },
                [&](const BonusCollectionIntent &intent,
                    const BonusApplication &application) {
                    adversarialFinishedSawRejection =
                        adversarialTranscript ==
                            std::vector<char>({'B'}) &&
                        intent.pickup.type == BonusType::Star &&
                        samePosition(intent.pickup.position,
                                     {10.0f, 12.0f}) &&
                        nearlyEqual(intent.pickup.age, 3.25f) &&
                        nearlyEqual(intent.pickup.life, 5.75f) &&
                        intent.collectorIndex == 0 &&
                        intent.playerId == 73 &&
                        isDefaultFailure(application) &&
                        !adversarialPlayers[0].active &&
                        adversarialPlayers[0].score == 100 &&
                        adversarialPlayers[0].stageTally.bonusPoints == 50 &&
                        adversarialPlayers[0].level == 0 &&
                        sameEnemyBonusState(adversarialEnemies[0],
                                            adversarialEnemyBefore);
                    adversarialTranscript.push_back('F');
                }});
    expect(rejectedTransaction.outcome ==
                   BonusPickupOutcome::ApplicationRejected &&
               rejectedTransaction.update.disposition ==
                   PickupDisposition::Collect &&
               rejectedTransaction.update.collectorIndex == 0 &&
               rejectedTransaction.intent.pickup.type == BonusType::Star &&
               nearlyEqual(rejectedTransaction.intent.pickup.age, 3.25f) &&
               nearlyEqual(rejectedTransaction.intent.pickup.life, 5.75f) &&
               rejectedTransaction.intent.collectorIndex == 0 &&
               rejectedTransaction.intent.playerId == 73 &&
               isDefaultFailure(rejectedTransaction.application) &&
               adversarialFinishedSawRejection &&
               adversarialTranscript == std::vector<char>({'B', 'F'}) &&
               nearlyEqual(adversarialPickup.age, 3.25f) &&
               nearlyEqual(adversarialPickup.life, 5.75f) &&
               adversarialPlayers[0].score == 100 &&
               adversarialPlayers[0].stageTally.bonusPoints == 50 &&
               sameEnemyBonusState(adversarialEnemies[0],
                                   adversarialEnemyBefore),
           "adversarial collector mutation did not finish with rejection");

    reporter.beginSuite(
        "bonus-pickup-batch-empty-and-missing-callbacks-atomic");
    std::vector<Pickup> emptyBatch;
    std::vector<Player> emptyBatchPlayers;
    std::vector<Enemy> emptyBatchEnemies;
    int emptyBatchCallbackCount = 0;
    const BonusPickupBatchOutcome emptyBatchOutcome = advanceBonusPickups(
        emptyBatch, 0.25f, emptyBatchPlayers, emptyBatchEnemies,
        BonusCollectionCallbacks{
            [&](const BonusCollectionIntent &) {
                ++emptyBatchCallbackCount;
            },
            [&](const BonusCollectionIntent &, const BonusApplication &) {
                ++emptyBatchCallbackCount;
            }});
    expect(emptyBatchOutcome == BonusPickupBatchOutcome::Advanced &&
               emptyBatch.empty() && emptyBatchPlayers.empty() &&
               emptyBatchEnemies.empty() && emptyBatchCallbackCount == 0,
           "valid empty pickup batch invoked callbacks or failed");

    Player guardedBatchPlayer = testPlayer(74);
    std::vector<Player> guardedBatchPlayers{guardedBatchPlayer};
    std::vector<Enemy> guardedBatchEnemies{
        testEnemy(74, 3, {6.0f, 7.0f})};
    Pickup guardedCollect;
    guardedCollect.type = BonusType::Star;
    guardedCollect.position = guardedBatchPlayer.position;
    guardedCollect.age = 1.0f;
    guardedCollect.life = 8.0f;
    Pickup guardedDiscard;
    guardedDiscard.type = BonusType::Clock;
    guardedDiscard.position = {20.0f, 20.0f};
    guardedDiscard.age = 2.0f;
    guardedDiscard.life = 0.25f;
    std::vector<Pickup> guardedBatch{guardedCollect, guardedDiscard};
    const std::vector<Pickup> guardedBatchBefore = guardedBatch;
    const std::vector<Player> guardedBatchPlayersBefore =
        guardedBatchPlayers;
    const std::vector<Enemy> guardedBatchEnemiesBefore =
        guardedBatchEnemies;
    std::vector<char> guardedBatchTranscript;
    const auto guardedBatchStarted = [&](const BonusCollectionIntent &) {
        guardedBatchTranscript.push_back('B');
    };
    const auto guardedBatchFinished =
        [&](const BonusCollectionIntent &, const BonusApplication &) {
            guardedBatchTranscript.push_back('F');
        };
    const auto guardedBatchRemoved =
        [&](const BonusCollectionIntent &, const BonusApplication &) {
            guardedBatchTranscript.push_back('D');
        };
    const BonusPickupBatchOutcome missingBatchStarted =
        advanceBonusPickups(
            guardedBatch, 0.25f, guardedBatchPlayers,
            guardedBatchEnemies,
            BonusCollectionCallbacks{{}, guardedBatchFinished},
            guardedBatchRemoved);
    const BonusPickupBatchOutcome missingBatchFinished =
        advanceBonusPickups(
            guardedBatch, 0.25f, guardedBatchPlayers,
            guardedBatchEnemies,
            BonusCollectionCallbacks{guardedBatchStarted, {}},
            guardedBatchRemoved);
    expect(missingBatchStarted == BonusPickupBatchOutcome::Invalid &&
               missingBatchFinished == BonusPickupBatchOutcome::Invalid &&
               guardedBatch.size() == guardedBatchBefore.size() &&
               samePickupState(guardedBatch[0], guardedBatchBefore[0]) &&
               samePickupState(guardedBatch[1], guardedBatchBefore[1]) &&
               samePlayerBonusState(guardedBatchPlayers[0],
                                    guardedBatchPlayersBefore[0]) &&
               sameEnemyBonusState(guardedBatchEnemies[0],
                                   guardedBatchEnemiesBefore[0]) &&
               guardedBatchTranscript.empty(),
           "missing batch callback changed pickup clocks, containers, or rules");

    reporter.beginSuite("bonus-pickup-batch-mixed-stable-order");
    std::vector<Player> mixedBatchPlayers{testPlayer(75)};
    std::vector<Enemy> mixedBatchEnemies;
    Pickup mixedRetainedFirst;
    mixedRetainedFirst.type = BonusType::Clock;
    mixedRetainedFirst.position = {20.0f, 20.0f};
    mixedRetainedFirst.age = 1.0f;
    mixedRetainedFirst.life = 7.0f;
    Pickup mixedDiscarded;
    mixedDiscarded.type = BonusType::Helmet;
    mixedDiscarded.position = mixedBatchPlayers[0].position;
    mixedDiscarded.age = 2.0f;
    mixedDiscarded.life = 0.25f;
    Pickup mixedCollected;
    mixedCollected.type = BonusType::Star;
    mixedCollected.position = mixedBatchPlayers[0].position;
    mixedCollected.age = 3.0f;
    mixedCollected.life = 8.0f;
    Pickup mixedRetainedLast;
    mixedRetainedLast.type = BonusType::Boat;
    mixedRetainedLast.position = {22.0f, 22.0f};
    mixedRetainedLast.age = 4.0f;
    mixedRetainedLast.life = 5.0f;
    std::vector<Pickup> mixedBatch{
        mixedRetainedFirst, mixedDiscarded, mixedCollected,
        mixedRetainedLast};
    std::vector<char> mixedBatchTranscript;
    bool mixedRemovalSawErasedVector = false;
    BonusCollectionIntent mixedRemovedIntent;
    BonusApplication mixedRemovedApplication;
    const BonusPickupBatchOutcome mixedBatchOutcome = advanceBonusPickups(
        mixedBatch, 0.25f, mixedBatchPlayers, mixedBatchEnemies,
        BonusCollectionCallbacks{
            [&](const BonusCollectionIntent &) {
                mixedBatchTranscript.push_back('B');
            },
            [&](const BonusCollectionIntent &, const BonusApplication &) {
                mixedBatchTranscript.push_back('F');
            }},
        [&](const BonusCollectionIntent &intent,
            const BonusApplication &application) {
            mixedRemovalSawErasedVector =
                mixedBatch.size() == 2U &&
                mixedBatch[0].type == BonusType::Clock &&
                mixedBatch[1].type == BonusType::Boat;
            mixedRemovedIntent = intent;
            mixedRemovedApplication = application;
            mixedBatchTranscript.push_back('D');
        });
    expect(mixedBatchOutcome == BonusPickupBatchOutcome::Advanced &&
               mixedBatch.size() == 2U &&
               mixedBatch[0].type == BonusType::Clock &&
               samePosition(mixedBatch[0].position, {20.0f, 20.0f}) &&
               nearlyEqual(mixedBatch[0].age, 1.25f) &&
               nearlyEqual(mixedBatch[0].life, 6.75f) &&
               mixedBatch[1].type == BonusType::Boat &&
               samePosition(mixedBatch[1].position, {22.0f, 22.0f}) &&
               nearlyEqual(mixedBatch[1].age, 4.25f) &&
               nearlyEqual(mixedBatch[1].life, 4.75f) &&
               mixedBatchTranscript == std::vector<char>({'B', 'F', 'D'}) &&
               mixedRemovalSawErasedVector &&
               mixedRemovedIntent.pickup.type == BonusType::Star &&
               nearlyEqual(mixedRemovedIntent.pickup.age, 3.25f) &&
               nearlyEqual(mixedRemovedIntent.pickup.life, 7.75f) &&
               mixedRemovedIntent.collectorIndex == 0 &&
               mixedRemovedIntent.playerId == 75 &&
               mixedRemovedApplication.applied &&
               mixedRemovedApplication.type == BonusType::Star &&
               mixedRemovedApplication.playerIndex == 0 &&
               mixedRemovedApplication.playerId == 75 &&
               mixedRemovedApplication.scoreDelta == kBonusBasePoints &&
               mixedBatchPlayers[0].level == 1 &&
               mixedBatchPlayers[0].score == 400 &&
               mixedBatchPlayers[0].stageTally.bonusPoints == 350,
           "mixed batch skipped, reordered, double-aged, or exposed pre-erase state");

    reporter.beginSuite("bonus-pickup-batch-adjacent-removal-no-skip");
    std::vector<Player> adjacentBatchPlayers{testPlayer(76)};
    std::vector<Enemy> adjacentBatchEnemies;
    Pickup adjacentExpired;
    adjacentExpired.type = BonusType::Clock;
    adjacentExpired.position = {20.0f, 20.0f};
    adjacentExpired.life = 0.1f;
    Pickup adjacentStar;
    adjacentStar.type = BonusType::Star;
    adjacentStar.position = adjacentBatchPlayers[0].position;
    Pickup adjacentInvalid;
    adjacentInvalid.type = BonusType::Count;
    adjacentInvalid.position = adjacentBatchPlayers[0].position;
    Pickup adjacentTank;
    adjacentTank.type = BonusType::Tank;
    adjacentTank.position = adjacentBatchPlayers[0].position;
    Pickup adjacentRetained;
    adjacentRetained.type = BonusType::Boat;
    adjacentRetained.position = {22.0f, 22.0f};
    std::vector<Pickup> adjacentBatch{
        adjacentExpired, adjacentStar, adjacentInvalid, adjacentTank,
        adjacentRetained};
    std::vector<BonusType> adjacentStartedTypes;
    std::vector<BonusType> adjacentFinishedTypes;
    const BonusPickupBatchOutcome adjacentBatchOutcome = advanceBonusPickups(
        adjacentBatch, 0.1f, adjacentBatchPlayers, adjacentBatchEnemies,
        BonusCollectionCallbacks{
            [&](const BonusCollectionIntent &intent) {
                adjacentStartedTypes.push_back(intent.pickup.type);
            },
            [&](const BonusCollectionIntent &intent,
                const BonusApplication &application) {
                if (application.applied)
                    adjacentFinishedTypes.push_back(intent.pickup.type);
            }});
    expect(adjacentBatchOutcome == BonusPickupBatchOutcome::Advanced &&
               adjacentBatch.size() == 1U &&
               adjacentBatch[0].type == BonusType::Boat &&
               nearlyEqual(adjacentBatch[0].age, 0.1f) &&
               nearlyEqual(adjacentBatch[0].life,
                           kPickupLifetime - 0.1f) &&
               adjacentStartedTypes ==
                   std::vector<BonusType>({BonusType::Star,
                                           BonusType::Tank}) &&
               adjacentFinishedTypes == adjacentStartedTypes &&
               adjacentBatchPlayers[0].level == 1 &&
               adjacentBatchPlayers[0].lives == 5 &&
               adjacentBatchPlayers[0].score == 700 &&
               adjacentBatchPlayers[0].stageTally.bonusPoints == 650,
           "adjacent discard/collect erasures skipped a shifted successor");

    reporter.beginSuite(
        "bonus-pickup-batch-application-rejection-continues");
    // Deliberately violates the player-mutation callback contract to exercise
    // the defensive rejected-transaction path at the vector boundary.
    std::vector<Player> rejectedBatchPlayers{testPlayer(77)};
    std::vector<Enemy> rejectedBatchEnemies;
    Pickup rejectedBatchStar;
    rejectedBatchStar.type = BonusType::Star;
    rejectedBatchStar.position = rejectedBatchPlayers[0].position;
    rejectedBatchStar.age = 1.0f;
    rejectedBatchStar.life = 8.0f;
    Pickup acceptedBatchTank;
    acceptedBatchTank.type = BonusType::Tank;
    acceptedBatchTank.position = rejectedBatchPlayers[0].position;
    acceptedBatchTank.age = 2.0f;
    acceptedBatchTank.life = 7.0f;
    std::vector<Pickup> rejectedBatch{
        rejectedBatchStar, acceptedBatchTank};
    std::vector<BonusType> rejectedBatchStartedTypes;
    std::vector<bool> rejectedBatchApplicationStates;
    std::vector<BonusType> rejectedBatchRemovedTypes;
    bool rejectedBatchRemovalSawRetainedFirst = false;
    const BonusPickupBatchOutcome rejectedBatchOutcome = advanceBonusPickups(
        rejectedBatch, 0.25f, rejectedBatchPlayers, rejectedBatchEnemies,
        BonusCollectionCallbacks{
            [&](const BonusCollectionIntent &intent) {
                rejectedBatchStartedTypes.push_back(intent.pickup.type);
                if (intent.pickup.type == BonusType::Star)
                    rejectedBatchPlayers[0].active = false;
            },
            [&](const BonusCollectionIntent &intent,
                const BonusApplication &application) {
                rejectedBatchApplicationStates.push_back(application.applied);
                if (intent.pickup.type == BonusType::Star)
                    rejectedBatchPlayers[0].active = true;
            }},
        [&](const BonusCollectionIntent &intent,
            const BonusApplication &) {
            rejectedBatchRemovedTypes.push_back(intent.pickup.type);
            rejectedBatchRemovalSawRetainedFirst =
                rejectedBatch.size() == 1U &&
                rejectedBatch[0].type == BonusType::Star &&
                nearlyEqual(rejectedBatch[0].age, 1.25f) &&
                nearlyEqual(rejectedBatch[0].life, 7.75f);
        });
    expect(rejectedBatchOutcome == BonusPickupBatchOutcome::Advanced &&
               rejectedBatch.size() == 1U &&
               rejectedBatch[0].type == BonusType::Star &&
               nearlyEqual(rejectedBatch[0].age, 1.25f) &&
               nearlyEqual(rejectedBatch[0].life, 7.75f) &&
               rejectedBatchStartedTypes ==
                   std::vector<BonusType>({BonusType::Star,
                                           BonusType::Tank}) &&
               rejectedBatchApplicationStates ==
                   std::vector<bool>({false, true}) &&
               rejectedBatchRemovedTypes ==
                   std::vector<BonusType>({BonusType::Tank}) &&
               rejectedBatchRemovalSawRetainedFirst &&
               rejectedBatchPlayers[0].active &&
               rejectedBatchPlayers[0].level == 0 &&
               rejectedBatchPlayers[0].lives == 5 &&
               rejectedBatchPlayers[0].score == 400 &&
               rejectedBatchPlayers[0].stageTally.bonusPoints == 350,
           "rejected pickup was erased, retried, or blocked its successor");

    reporter.beginSuite("bonus-application-invalid-input-is-atomic");
    Player invalidFixture = testPlayer(50);
    invalidFixture.hitPoints = 2;
    Enemy invalidEnemy = testEnemy(5, 3, {6.0f, 7.0f});
    const auto expectRejected = [&](int playerIndex, BonusType type,
                                    const std::string &label) {
        std::vector<Player> players{invalidFixture};
        std::vector<Enemy> enemies{invalidEnemy};
        const BonusApplication result =
            applyBonus(players, enemies, playerIndex, type);
        expect(isDefaultFailure(result) &&
                   samePlayerBonusState(players[0], invalidFixture) &&
                   sameEnemyBonusState(enemies[0], invalidEnemy),
               label + " did not fail atomically");
    };
    expectRejected(-1, BonusType::Star, "negative player index");
    expectRejected(1, BonusType::Star, "past-end player index");
    expectRejected(std::numeric_limits<int>::max(), BonusType::Star,
                   "extreme player index");
    expectRejected(0, BonusType::Count, "bonus sentinel");
    expectRejected(0, static_cast<BonusType>(255),
                   "out-of-range bonus type");
    {
        Player inactivePlayer = invalidFixture;
        inactivePlayer.active = false;
        std::vector<Player> players{inactivePlayer};
        std::vector<Enemy> enemies{invalidEnemy};
        expect(isDefaultFailure(applyBonus(
                   players, enemies, 0, BonusType::Star)) &&
                   samePlayerBonusState(players[0], inactivePlayer) &&
                   sameEnemyBonusState(enemies[0], invalidEnemy),
               "inactive player received bonus state or points");
    }
    for (float creationTimer : {
             0.1f, std::numeric_limits<float>::quiet_NaN()})
    {
        Player unavailablePlayer = invalidFixture;
        unavailablePlayer.creationTimer = creationTimer;
        std::vector<Player> players{unavailablePlayer};
        std::vector<Enemy> enemies{invalidEnemy};
        expect(isDefaultFailure(applyBonus(
                   players, enemies, 0, BonusType::Star)) &&
                   samePlayerBonusState(players[0], unavailablePlayer) &&
                   sameEnemyBonusState(enemies[0], invalidEnemy),
               "unavailable player received bonus state or points");
    }
    std::vector<Player> noPlayers;
    std::vector<Enemy> noPlayerEnemies{invalidEnemy};
    const BonusApplication emptyPlayerResult =
        applyBonus(noPlayers, noPlayerEnemies, 0, BonusType::Grenade);
    expect(isDefaultFailure(emptyPlayerResult) &&
               sameEnemyBonusState(noPlayerEnemies[0], invalidEnemy),
           "empty player list mutated enemy state");
    Player fullHealth = invalidFixture;
    fullHealth.hitPoints = fullHealth.maximumHitPoints;
    std::vector<Player> healthyPlayers{fullHealth};
    std::vector<Enemy> healthyEnemies{invalidEnemy};
    const BonusApplication rejectedBandage =
        applyBonus(healthyPlayers, healthyEnemies, 0, BonusType::Bandage);
    expect(isDefaultFailure(rejectedBandage) &&
               samePlayerBonusState(healthyPlayers[0], fullHealth) &&
               sameEnemyBonusState(healthyEnemies[0], invalidEnemy),
           "ineligible Bandage credited points or mutated state");

    reporter.beginSuite("bonus-application-all-valid-types-and-attribution");
    const std::array<BonusType, 9> validTypes{{
        BonusType::Grenade, BonusType::Helmet, BonusType::Clock,
        BonusType::Shovel, BonusType::Tank, BonusType::Star, BonusType::Gun,
        BonusType::Boat, BonusType::Bandage}};
    for (BonusType type : validTypes)
    {
        Player firstPlayer = testPlayer(60);
        Player targetPlayer = testPlayer(61);
        if (type == BonusType::Bandage)
            targetPlayer.hitPoints = 2;
        std::vector<Player> players{firstPlayer, targetPlayer};
        std::vector<Enemy> enemies;
        const BonusApplication result =
            applyBonus(players, enemies, 1, type);
        expect(result.applied && result.type == type &&
                   result.playerIndex == 1 && result.playerId == 61 &&
                   result.scoreDelta == kBonusBasePoints &&
                   players[1].score == 100 + kBonusBasePoints &&
                   players[1].stageTally.bonusPoints ==
                       50 + kBonusBasePoints &&
                   samePlayerBonusState(players[0], firstPlayer),
               "valid bonus lost common credit or attribution for type " +
                   std::to_string(static_cast<int>(type)));
    }

    reporter.beginSuite("bonus-helmet-and-clock-effects");
    Player shieldPlayer = testPlayer(70);
    shieldPlayer.shieldTimer = 2.0f;
    std::vector<Player> shieldPlayers{shieldPlayer};
    std::vector<Enemy> noEnemies;
    BonusApplication shieldResult =
        applyBonus(shieldPlayers, noEnemies, 0, BonusType::Helmet);
    expect(shieldResult.applied && shieldResult.commands.empty() &&
               nearlyEqual(shieldPlayers[0].shieldTimer,
                           kHelmetBonusDuration),
           "Helmet did not raise short shield to classic duration");
    shieldPlayer.shieldTimer = 14.0f;
    shieldPlayers = {shieldPlayer};
    shieldResult = applyBonus(shieldPlayers, noEnemies, 0, BonusType::Helmet);
    expect(nearlyEqual(shieldPlayers[0].shieldTimer, 14.0f),
           "Helmet shortened an existing shield");

    std::vector<Player> clockPlayers{testPlayer(71)};
    Enemy shortFreeze = testEnemy(71, 1, {1.0f, 1.0f});
    shortFreeze.frozenTimer = 1.0f;
    Enemy longFreeze = testEnemy(72, 1, {2.0f, 2.0f});
    longFreeze.frozenTimer = 12.0f;
    Enemy destroyedFreeze = testEnemy(73, 1, {3.0f, 3.0f});
    destroyedFreeze.destroyed = true;
    destroyedFreeze.frozenTimer = 2.0f;
    std::vector<Enemy> clockEnemies{
        shortFreeze, longFreeze, destroyedFreeze};
    const BonusApplication clockResult =
        applyBonus(clockPlayers, clockEnemies, 0, BonusType::Clock);
    expect(clockResult.applied && clockResult.commands.empty() &&
               nearlyEqual(clockEnemies[0].frozenTimer,
                           kClockBonusDuration),
           "Clock did not raise live enemy freeze duration");
    expect(nearlyEqual(clockEnemies[1].frozenTimer, 12.0f),
           "Clock shortened an existing enemy freeze");
    expect(nearlyEqual(clockEnemies[2].frozenTimer, 2.0f),
           "Clock changed already-destroyed enemy");

    reporter.beginSuite("bonus-shovel-tank-and-level-effects");
    std::vector<Player> effectPlayers{testPlayer(80)};
    std::vector<Enemy> effectEnemies;
    const BonusApplication shovel =
        applyBonus(effectPlayers, effectEnemies, 0, BonusType::Shovel);
    expect(shovel.applied && shovel.commands.size() == 1U &&
               shovel.commands[0].type ==
                   BonusEffectCommandType::ActivateGovernmentSteel,
           "Shovel did not emit exactly one steel activation command");
    expect(shovel.commands[0].playerIndex == -1 &&
               shovel.commands[0].playerId == -1 &&
               shovel.commands[0].enemyId == -1 &&
               nearlyEqual(shovel.commands[0].scalar, 0.0f),
           "Shovel world command retained unrelated entity data");

    Player lifePlayer = testPlayer(81);
    lifePlayer.lives = 4;
    effectPlayers = {lifePlayer};
    BonusApplication tank =
        applyBonus(effectPlayers, effectEnemies, 0, BonusType::Tank);
    expect(tank.applied && tank.commands.empty() &&
               effectPlayers[0].lives == 5,
           "Tank bonus did not add one life");
    lifePlayer.lives = 99;
    effectPlayers = {lifePlayer};
    tank = applyBonus(effectPlayers, effectEnemies, 0, BonusType::Tank);
    expect(effectPlayers[0].lives == 99,
           "Tank bonus exceeded the 99-life cap");
    lifePlayer.lives = std::numeric_limits<int>::max();
    effectPlayers = {lifePlayer};
    tank = applyBonus(effectPlayers, effectEnemies, 0, BonusType::Tank);
    expect(effectPlayers[0].lives == 99,
           "Tank bonus did not normalize corrupt over-cap lives safely");

    for (int level : {-20, 0, 1, 2, 3, 20})
    {
        Player starPlayer = testPlayer(82);
        starPlayer.level = level;
        effectPlayers = {starPlayer};
        const BonusApplication star =
            applyBonus(effectPlayers, effectEnemies, 0, BonusType::Star);
        const int expectedLevel = level <= 0 ? 1 : (level < 3 ? level + 1 : 3);
        expect(star.applied && star.commands.empty() &&
                   effectPlayers[0].level == expectedLevel,
               "Star level progression changed from level " +
                   std::to_string(level));
    }
    Player gunPlayer = testPlayer(83);
    gunPlayer.level = -10;
    effectPlayers = {gunPlayer};
    const BonusApplication gun =
        applyBonus(effectPlayers, effectEnemies, 0, BonusType::Gun);
    expect(gun.applied && gun.commands.empty() &&
               effectPlayers[0].level == 3,
           "Gun bonus did not set maximum tank level");

    reporter.beginSuite("bonus-boat-and-bandage-effects");
    Player boatPlayer = testPlayer(90);
    boatPlayer.hasBoat = false;
    std::vector<Player> recoveryPlayers{boatPlayer};
    std::vector<Enemy> recoveryEnemies;
    BonusApplication boat =
        applyBonus(recoveryPlayers, recoveryEnemies, 0, BonusType::Boat);
    expect(boat.applied && boat.commands.empty() &&
               recoveryPlayers[0].hasBoat,
           "Boat bonus did not grant water protection");
    recoveryPlayers = {recoveryPlayers[0]};
    boat = applyBonus(recoveryPlayers, recoveryEnemies, 0, BonusType::Boat);
    expect(boat.applied && recoveryPlayers[0].hasBoat,
           "repeated Boat bonus removed existing protection");

    Player bandagePlayer = testPlayer(91);
    bandagePlayer.maximumHitPoints = 6;
    bandagePlayer.hitPoints = 2;
    recoveryPlayers = {bandagePlayer};
    BonusApplication bandageResult =
        applyBonus(recoveryPlayers, recoveryEnemies, 0, BonusType::Bandage);
    expect(bandageResult.applied && bandageResult.commands.empty() &&
               recoveryPlayers[0].hitPoints == 3 &&
               bandageResult.scoreDelta == kBonusBasePoints,
           "Bandage did not heal exactly one HP with common credit");
    bandageResult =
        applyBonus(recoveryPlayers, recoveryEnemies, 0, BonusType::Bandage);
    expect(bandageResult.applied && recoveryPlayers[0].hitPoints == 4,
           "successive eligible Bandage did not heal one additional HP");
    recoveryPlayers[0].hitPoints = recoveryPlayers[0].maximumHitPoints;
    const Player fullRecoverySnapshot = recoveryPlayers[0];
    bandageResult =
        applyBonus(recoveryPlayers, recoveryEnemies, 0, BonusType::Bandage);
    expect(isDefaultFailure(bandageResult) &&
               samePlayerBonusState(recoveryPlayers[0],
                                    fullRecoverySnapshot),
           "full-health Bandage applied points or healing");

    reporter.beginSuite("bonus-grenade-state-score-and-classification");
    Player untouchedGrenadier = testPlayer(100);
    Player grenadier = testPlayer(101);
    grenadier.directKillStreak = 4;
    grenadier.stageTally.destroyed = {{1, 2, 3, 4}};
    grenadier.stageTally.enemyPoints = {{10, 20, 30, 40}};
    const Player grenadierBefore = grenadier;
    Enemy light = testEnemy(10, 1, {2.0f, 3.0f});
    Enemy armored = testEnemy(11, 3, {5.0f, 7.0f});
    Enemy alreadyDestroyed = testEnemy(12, 4, {8.0f, 9.0f});
    alreadyDestroyed.destroyed = true;
    alreadyDestroyed.moving = false;
    alreadyDestroyed.deathTimer = 0.25f;
    Enemy zeroArmor = testEnemy(13, 0, {11.0f, 13.0f});
    Enemy negativeArmor = testEnemy(14, -4, {14.0f, 15.0f});
    const Enemy destroyedBefore = alreadyDestroyed;
    const Enemy zeroBefore = zeroArmor;
    const Enemy negativeBefore = negativeArmor;
    std::vector<Player> grenadePlayers{untouchedGrenadier, grenadier};
    std::vector<Enemy> grenadeEnemies{
        light, armored, alreadyDestroyed, zeroArmor, negativeArmor};
    const BonusApplication grenade =
        applyBonus(grenadePlayers, grenadeEnemies, 1, BonusType::Grenade);
    const int expectedGrenadePoints =
        kBonusBasePoints + 2 * kGrenadeEnemyPoints;
    expect(grenade.applied && grenade.type == BonusType::Grenade &&
               grenade.playerIndex == 1 && grenade.playerId == 101 &&
               grenade.scoreDelta == expectedGrenadePoints &&
               grenadePlayers[1].score ==
                   grenadierBefore.score + expectedGrenadePoints &&
               grenadePlayers[1].stageTally.bonusPoints ==
                   grenadierBefore.stageTally.bonusPoints +
                       expectedGrenadePoints,
           "Grenade score did not include base and live-target points");
    expect(grenadePlayers[1].stageTally.destroyed ==
                   grenadierBefore.stageTally.destroyed &&
               grenadePlayers[1].stageTally.enemyPoints ==
                   grenadierBefore.stageTally.enemyPoints &&
               grenadePlayers[1].directKillStreak ==
                   grenadierBefore.directKillStreak,
           "Grenade entered classified K.O. totals or direct-kill streak");
    expect(samePlayerBonusState(grenadePlayers[0], untouchedGrenadier),
           "Grenade mutated non-collecting player");
    for (std::size_t enemyIndex : {0U, 1U})
    {
        expect(grenadeEnemies[enemyIndex].armor == 0 &&
                   grenadeEnemies[enemyIndex].destroyed &&
                   !grenadeEnemies[enemyIndex].moving &&
                   nearlyEqual(grenadeEnemies[enemyIndex].deathTimer,
                               kTankDeathDuration),
               "Grenade did not commit destruction state for live enemy " +
                   std::to_string(enemyIndex));
    }
    expect(sameEnemyBonusState(grenadeEnemies[2], destroyedBefore),
           "Grenade reprocessed already-destroyed enemy");
    expect(sameEnemyBonusState(grenadeEnemies[3], zeroBefore) &&
               sameEnemyBonusState(grenadeEnemies[4], negativeBefore),
           "Grenade mutated corrupt non-positive armor enemy");

    reporter.beginSuite("bonus-grenade-command-order-and-payloads");
    const std::vector<BonusEffectCommand> &commands = grenade.commands;
    expect(commands.size() == 9U,
           "Grenade emitted wrong command count for armor 1 and armor 3");
    expect(commandMatchesEnemy(commands[0],
                               BonusEffectCommandType::EnemyDestroyedCue,
                               light, 101, 1) &&
               commandMatchesEnemy(commands[1],
                                   BonusEffectCommandType::EnemyExplosion,
                                   light, 101, 1) &&
               commandMatchesEnemy(commands[2],
                                   BonusEffectCommandType::EnemyDestroyedEvent,
                                   light, 101, 1),
           "armor-one enemy command prefix changed");
    expect(commands[2].valueAfter == 0 &&
               commands[2].points == kGrenadeEnemyPoints,
           "armor-one destruction event lost score transition payload");
    expect(commandMatchesEnemy(commands[3],
                               BonusEffectCommandType::EnemyArmorHitCue,
                               armored, 101, 3) &&
               commandMatchesEnemy(commands[4],
                                   BonusEffectCommandType::EnemyArmorHitCue,
                                   armored, 101, 3),
           "armored enemy did not emit armor-minus-one hit cues first");
    expect(commandMatchesEnemy(commands[5],
                               BonusEffectCommandType::EnemyDestroyedCue,
                               armored, 101, 3) &&
               commandMatchesEnemy(commands[6],
                                   BonusEffectCommandType::EnemyExplosion,
                                   armored, 101, 3) &&
               commandMatchesEnemy(commands[7],
                                   BonusEffectCommandType::EnemyDestroyedEvent,
                                   armored, 101, 3),
           "armored enemy destruction command suffix changed");
    expect(commands[7].valueAfter == 0 &&
               commands[7].points == kGrenadeEnemyPoints,
           "armored enemy destruction event lost score transition payload");
    expect(commands[8].type ==
                   BonusEffectCommandType::AssignPlayerCameraShake &&
               commands[8].playerIndex == 1 &&
               commands[8].playerId == 101 &&
               commands[8].enemyId == -1 &&
               nearlyEqual(commands[8].scalar, kGrenadeCameraShake),
           "Grenade camera command was not last or lost player attribution");
    for (std::size_t commandIndex = 0; commandIndex < 8U; ++commandIndex)
    {
        expect(commands[commandIndex].type !=
                   BonusEffectCommandType::AssignPlayerCameraShake,
               "camera command appeared before all enemy commands at index " +
                   std::to_string(commandIndex));
    }

    reporter.beginSuite("bonus-grenade-empty-and-corrupt-target-filtering");
    std::vector<Player> emptyGrenadePlayers{testPlayer(110)};
    Enemy deadTarget = testEnemy(20, 5, {2.0f, 2.0f});
    deadTarget.destroyed = true;
    Enemy zeroTarget = testEnemy(21, 0, {3.0f, 3.0f});
    Enemy negativeTarget = testEnemy(22, -1, {4.0f, 4.0f});
    std::vector<Enemy> noLiveTargets{deadTarget, zeroTarget, negativeTarget};
    const std::vector<Enemy> noLiveTargetsBefore = noLiveTargets;
    const BonusApplication emptyGrenade = applyBonus(
        emptyGrenadePlayers, noLiveTargets, 0, BonusType::Grenade);
    expect(emptyGrenade.applied &&
               emptyGrenade.scoreDelta == kBonusBasePoints &&
               emptyGrenade.commands.empty() &&
               emptyGrenadePlayers[0].score == 100 + kBonusBasePoints &&
               emptyGrenadePlayers[0].stageTally.bonusPoints ==
                   50 + kBonusBasePoints,
           "Grenade without live targets lost base credit or emitted commands");
    for (std::size_t enemyIndex = 0;
         enemyIndex < noLiveTargets.size(); ++enemyIndex)
    {
        expect(sameEnemyBonusState(noLiveTargets[enemyIndex],
                                   noLiveTargetsBefore[enemyIndex]),
               "filtered grenade target mutated at index " +
                   std::to_string(enemyIndex));
    }

    reporter.beginSuite("bonus-score-tally-and-life-saturation");
    const int maximumInt = std::numeric_limits<int>::max();
    Player saturatedGrenadier = testPlayer(120);
    saturatedGrenadier.score = maximumInt - 100;
    saturatedGrenadier.stageTally.bonusPoints = maximumInt - 50;
    std::vector<Player> saturatedPlayers{saturatedGrenadier};
    std::vector<Enemy> saturationEnemies{
        testEnemy(30, 1, {1.0f, 1.0f}),
        testEnemy(31, 2, {2.0f, 2.0f})};
    const BonusApplication saturatedGrenade = applyBonus(
        saturatedPlayers, saturationEnemies, 0, BonusType::Grenade);
    expect(saturatedGrenade.applied &&
               saturatedPlayers[0].score == maximumInt &&
               saturatedGrenade.scoreDelta == 100,
           "player score did not saturate safely at INT_MAX");
    expect(saturatedPlayers[0].stageTally.bonusPoints == maximumInt,
           "bonus tally did not saturate safely at INT_MAX");
    expect(saturatedGrenade.commands.size() == 8U &&
               saturationEnemies[0].destroyed &&
               saturationEnemies[1].destroyed,
           "score saturation suppressed grenade target effects");

    Player maximumScore = testPlayer(121);
    maximumScore.score = maximumInt;
    maximumScore.stageTally.bonusPoints = maximumInt;
    std::vector<Player> maximumPlayers{maximumScore};
    std::vector<Enemy> maximumEnemies;
    const BonusApplication maximumHelmet =
        applyBonus(maximumPlayers, maximumEnemies, 0, BonusType::Helmet);
    expect(maximumHelmet.applied && maximumHelmet.scoreDelta == 0 &&
               maximumPlayers[0].score == maximumInt &&
               maximumPlayers[0].stageTally.bonusPoints == maximumInt &&
               nearlyEqual(maximumPlayers[0].shieldTimer,
                           kHelmetBonusDuration),
           "already-saturated score blocked Helmet effect or overflowed");

    Player overCapLife = testPlayer(122);
    overCapLife.lives = maximumInt;
    std::vector<Player> overCapPlayers{overCapLife};
    const BonusApplication safeTank =
        applyBonus(overCapPlayers, maximumEnemies, 0, BonusType::Tank);
    expect(safeTank.applied && overCapPlayers[0].lives == 99 &&
               safeTank.scoreDelta == kBonusBasePoints,
           "extreme lives value overflowed Tank bonus handling");

    reporter.finish();
    return passed ? 0 : 1;
}
