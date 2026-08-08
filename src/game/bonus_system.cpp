#include "game/bonus_system.h"

#include "core/gameplay_rules.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace tanks3d::game
{
namespace
{
constexpr int kPickupSpawnMaximumPixel = 383;
constexpr float kPickupSpawnHalfSizePixels = 16.0f;
constexpr float kPickupSpawnPixelsPerTile = 16.0f;
constexpr int kMaximumPlayerLives = 99;

int saturatingAddPositive(int current, int increment)
{
    const int maximum = std::numeric_limits<int>::max();
    return current > maximum - increment ? maximum : current + increment;
}

BonusEffectCommand enemyCommand(BonusEffectCommandType type,
                                const Enemy &enemy, int playerId,
                                int armorBefore)
{
    BonusEffectCommand command;
    command.type = type;
    command.playerId = playerId;
    command.enemyId = enemy.id;
    command.enemyType = enemy.type;
    command.position = enemy.position;
    command.valueBefore = armorBefore;
    return command;
}
} // namespace

bool anyPlayerNeedsHealing(const std::vector<Player> &players)
{
    return std::any_of(players.begin(), players.end(), [](const Player &player) {
        return player.needsHealing();
    });
}

int bonusPointsForGrenadeTargets(std::size_t targetCount)
{
    const int maximum = std::numeric_limits<int>::max();
    const std::size_t maximumTargets = static_cast<std::size_t>(
        (maximum - kBonusBasePoints) / kGrenadeEnemyPoints);
    if (targetCount > maximumTargets)
        return maximum;
    return kBonusBasePoints +
           static_cast<int>(targetCount) * kGrenadeEnemyPoints;
}

BonusSpawnDecision bonusSpawnFromDraws(int typeSlot, bool bandageEligible,
                                       int xPixel, int zPixel)
{
    BonusSpawnDecision decision;
    const int slotCount = weightedTypeSlotCount(bandageEligible);
    if (typeSlot < 0 || typeSlot >= slotCount || xPixel < 0 ||
        xPixel > kPickupSpawnMaximumPixel || zPixel < 0 ||
        zPixel > kPickupSpawnMaximumPixel)
    {
        return decision;
    }

    decision.valid = true;
    decision.pickup.type = typeForWeightedSlot(typeSlot, bandageEligible);
    decision.pickup.position = {
        (static_cast<float>(xPixel) + kPickupSpawnHalfSizePixels) /
            kPickupSpawnPixelsPerTile,
        (static_cast<float>(zPixel) + kPickupSpawnHalfSizePixels) /
            kPickupSpawnPixelsPerTile};
    return decision;
}

BonusReleaseOutcome advanceBonusReleaseTransaction(
    const std::vector<Player> &players, int sourceEnemyId,
    const BonusReleaseRandom &random,
    const BonusPositionRejected &positionRejected,
    const BonusReleaseCommit &commit)
{
    if (!random.drawTypeSlot || !random.drawPixel || !positionRejected ||
        !commit)
    {
        return BonusReleaseOutcome::Invalid;
    }

    // Eligibility is deliberately sampled once. Position retries retain the
    // originally selected type even if caller-owned state changes in between.
    const bool bandageEligible = anyPlayerNeedsHealing(players);
    const int slotCount = weightedTypeSlotCount(bandageEligible);
    const int typeSlot = random.drawTypeSlot(slotCount);
    for (;;)
    {
        // Preserve the old defensive path: even a malformed type or X draw is
        // validated only after the complete first X/Z pair has been consumed.
        const int xPixel = random.drawPixel();
        const int zPixel = random.drawPixel();
        const BonusSpawnDecision spawn = bonusSpawnFromDraws(
            typeSlot, bandageEligible, xPixel, zPixel);
        if (!spawn.valid)
            return BonusReleaseOutcome::Invalid;
        if (positionRejected(spawn.pickup.position))
            continue;

        BonusReleaseIntent intent;
        intent.pickup = spawn.pickup;
        intent.sourceEnemyId = sourceEnemyId;
        commit(intent);
        return BonusReleaseOutcome::Released;
    }
}

PickupUpdate updatePickup(Pickup &pickup, float elapsedTime,
                          const std::vector<Player> &players)
{
    if (!std::isfinite(elapsedTime) || elapsedTime < 0.0f)
        return {};

    pickup.age += elapsedTime;
    pickup.life -= elapsedTime;
    if (!std::isfinite(pickup.age) || !std::isfinite(pickup.life) ||
        !std::isfinite(pickup.position.x) ||
        !std::isfinite(pickup.position.z) || pickup.life <= 0.0f ||
        !isValidBonusType(pickup.type))
    {
        return {PickupDisposition::Remove, -1};
    }

    for (std::size_t index = 0; index < players.size(); ++index)
    {
        const Player &player = players[index];
        if (!player.active || !std::isfinite(player.creationTimer) ||
            player.creationTimer > 0.0f ||
            !axisAlignedCentersOverlap(player.position, pickup.position,
                                       kPickupTankHitExtent) ||
            !playerMeetsBonusTypeEligibility(player, pickup.type))
        {
            continue;
        }
        return {PickupDisposition::Collect, static_cast<int>(index)};
    }
    return {};
}

BonusApplication applyBonus(std::vector<Player> &players,
                            std::vector<Enemy> &enemies, int playerIndex,
                            BonusType type)
{
    BonusApplication application;
    if (playerIndex < 0 ||
        static_cast<std::size_t>(playerIndex) >= players.size() ||
        !isValidBonusType(type))
    {
        return application;
    }

    Player &player = players[static_cast<std::size_t>(playerIndex)];
    if (!player.active || !std::isfinite(player.creationTimer) ||
        player.creationTimer > 0.0f ||
        !playerMeetsBonusTypeEligibility(player, type))
        return application;

    application.applied = true;
    application.type = type;
    application.playerIndex = playerIndex;
    application.playerId = player.id;

    const int scoreBefore = player.score;
    int bonusPoints = kBonusBasePoints;
    if (type == BonusType::Grenade)
    {
        const std::size_t targetCount = static_cast<std::size_t>(std::count_if(
            enemies.begin(), enemies.end(), [](const Enemy &enemy) {
                return !enemy.destroyed && enemy.armor > 0;
            }));
        bonusPoints = bonusPointsForGrenadeTargets(targetCount);
    }
    player.score = saturatingAddPositive(player.score, bonusPoints);
    player.stageTally.bonusPoints = saturatingAddPositive(
        player.stageTally.bonusPoints, bonusPoints);
    application.scoreDelta = player.score - scoreBefore;

    if (type == BonusType::Grenade)
    {
        for (Enemy &enemy : enemies)
        {
            if (enemy.destroyed || enemy.armor <= 0)
                continue;
            const int armorBefore = enemy.armor;
            for (int armorHit = 1; armorHit < armorBefore; ++armorHit)
            {
                application.commands.push_back(enemyCommand(
                    BonusEffectCommandType::EnemyArmorHitCue, enemy,
                    player.id, armorBefore));
            }
            application.commands.push_back(enemyCommand(
                BonusEffectCommandType::EnemyDestroyedCue, enemy, player.id,
                armorBefore));
            application.commands.push_back(enemyCommand(
                BonusEffectCommandType::EnemyExplosion, enemy, player.id,
                armorBefore));

            enemy.armor = 0;
            enemy.destroyed = true;
            enemy.moving = false;
            enemy.deathTimer = kTankDeathDuration;

            BonusEffectCommand event = enemyCommand(
                BonusEffectCommandType::EnemyDestroyedEvent, enemy,
                player.id, armorBefore);
            event.valueAfter = 0;
            event.points = kGrenadeEnemyPoints;
            application.commands.push_back(event);
        }
        if (bonusPoints > kBonusBasePoints)
        {
            BonusEffectCommand camera;
            camera.type = BonusEffectCommandType::AssignPlayerCameraShake;
            camera.playerIndex = playerIndex;
            camera.playerId = player.id;
            camera.scalar = kGrenadeCameraShake;
            application.commands.push_back(camera);
        }
    }
    else if (type == BonusType::Helmet)
    {
        player.shieldTimer = std::max(player.shieldTimer,
                                      kHelmetBonusDuration);
    }
    else if (type == BonusType::Clock)
    {
        for (Enemy &enemy : enemies)
        {
            if (!enemy.destroyed)
                enemy.frozenTimer = std::max(enemy.frozenTimer,
                                             kClockBonusDuration);
        }
    }
    else if (type == BonusType::Shovel)
    {
        BonusEffectCommand command;
        command.type = BonusEffectCommandType::ActivateGovernmentSteel;
        application.commands.push_back(command);
    }
    else if (type == BonusType::Tank)
    {
        player.lives = player.lives >= kMaximumPlayerLives
                           ? kMaximumPlayerLives
                           : player.lives + 1;
    }
    else if (type == BonusType::Star)
    {
        player.level = core::upgradedPlayerLevel(player.level);
    }
    else if (type == BonusType::Gun)
    {
        player.level = 3;
    }
    else if (type == BonusType::Boat)
    {
        player.hasBoat = true;
    }
    else
    {
        // Validation above leaves Bandage as the only remaining value.
        (void)player.healOneHitPoint();
    }
    return application;
}

BonusPickupTransaction advanceBonusPickupTransaction(
    Pickup &pickup, float elapsedTime, std::vector<Player> &players,
    std::vector<Enemy> &enemies,
    const BonusCollectionCallbacks &callbacks)
{
    BonusPickupTransaction transaction;
    if (!callbacks.started || !callbacks.finished)
        return transaction;

    transaction.update = updatePickup(pickup, elapsedTime, players);
    if (transaction.update.disposition == PickupDisposition::Retain)
    {
        transaction.outcome = BonusPickupOutcome::Retained;
        return transaction;
    }
    if (transaction.update.disposition == PickupDisposition::Remove)
    {
        transaction.outcome = BonusPickupOutcome::Discarded;
        return transaction;
    }

    const std::size_t collectorIndex = static_cast<std::size_t>(
        transaction.update.collectorIndex);
    transaction.intent.pickup = pickup;
    transaction.intent.collectorIndex = transaction.update.collectorIndex;
    transaction.intent.playerId = players[collectorIndex].id;

    callbacks.started(transaction.intent);

    transaction.application = applyBonus(
        players, enemies, transaction.intent.collectorIndex,
        transaction.intent.pickup.type);
    callbacks.finished(transaction.intent, transaction.application);
    transaction.outcome = transaction.application.applied
                              ? BonusPickupOutcome::Collected
                              : BonusPickupOutcome::ApplicationRejected;
    return transaction;
}

BonusPickupBatchOutcome advanceBonusPickups(
    std::vector<Pickup> &pickups, float elapsedTime,
    std::vector<Player> &players, std::vector<Enemy> &enemies,
    const BonusCollectionCallbacks &callbacks,
    const BonusPickupRemoved &collectedRemoved)
{
    if (!callbacks.started || !callbacks.finished)
        return BonusPickupBatchOutcome::Invalid;

    for (std::size_t pickupIndex = 0; pickupIndex < pickups.size();)
    {
        Pickup &pickup = pickups[pickupIndex];
        const BonusPickupTransaction transaction =
            advanceBonusPickupTransaction(
                pickup, elapsedTime, players, enemies, callbacks);
        const bool remove =
            transaction.outcome == BonusPickupOutcome::Discarded ||
            transaction.outcome == BonusPickupOutcome::Collected;

        if (remove)
        {
            pickups.erase(pickups.begin() +
                          static_cast<std::ptrdiff_t>(pickupIndex));
            if (transaction.outcome == BonusPickupOutcome::Collected &&
                collectedRemoved)
            {
                collectedRemoved(transaction.intent, transaction.application);
            }
        }
        else
        {
            ++pickupIndex;
        }
    }
    return BonusPickupBatchOutcome::Advanced;
}
} // namespace tanks3d::game
