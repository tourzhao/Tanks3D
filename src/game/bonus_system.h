#ifndef TANKS3D_GAME_BONUS_SYSTEM_H
#define TANKS3D_GAME_BONUS_SYSTEM_H

#include "game/entities.h"

#include <cstddef>
#include <functional>
#include <vector>

namespace tanks3d::game
{
inline constexpr float kPickupTankHitExtent = 1.875f;
inline constexpr int kBonusBasePoints = 300;
inline constexpr int kGrenadeEnemyPoints = 200;
inline constexpr float kHelmetBonusDuration = 10.0f;
inline constexpr float kClockBonusDuration = 8.0f;
inline constexpr float kGrenadeCameraShake = 0.34f;

struct Pickup
{
    BonusType type = BonusType::Grenade;
    XZ position{};
    float age = 0.0f;
    float life = kPickupLifetime;
};

enum class PickupDisposition
{
    Retain,
    Remove,
    Collect
};

struct PickupUpdate
{
    PickupDisposition disposition = PickupDisposition::Retain;
    int collectorIndex = -1;
};

struct BonusSpawnDecision
{
    bool valid = false;
    Pickup pickup{};
};

enum class BonusReleaseOutcome : unsigned char
{
    Invalid,
    Released
};

struct BonusReleaseIntent
{
    Pickup pickup{};
    int sourceEnemyId = -1;
};

using BonusTypeSlotDraw = std::function<int(int)>;
using BonusPixelDraw = std::function<int()>;
using BonusPositionRejected = std::function<bool(XZ)>;
using BonusReleaseCommit =
    std::function<void(const BonusReleaseIntent &)>;

struct BonusReleaseRandom
{
    // Called once with the exact slot count (8 or 10).
    BonusTypeSlotDraw drawTypeSlot;
    // Called in complete X/Z pairs until the position query accepts one.
    BonusPixelDraw drawPixel;
};

enum class BonusEffectCommandType
{
    EnemyArmorHitCue,
    EnemyDestroyedCue,
    EnemyExplosion,
    EnemyDestroyedEvent,
    ActivateGovernmentSteel,
    AssignPlayerCameraShake,
    Count
};

// Detached data for one ordered presentation/world command. Commands never
// retain references into the mutable player or enemy containers.
struct BonusEffectCommand
{
    BonusEffectCommandType type = BonusEffectCommandType::Count;
    int playerIndex = -1;
    int playerId = -1;
    int enemyId = -1;
    int enemyType = -1;
    XZ position{};
    int valueBefore = 0;
    int valueAfter = 0;
    int points = 0;
    float scalar = 0.0f;
};

struct BonusApplication
{
    bool applied = false;
    BonusType type = BonusType::Count;
    int playerIndex = -1;
    int playerId = -1;
    int scoreDelta = 0;
    std::vector<BonusEffectCommand> commands{};
};

struct BonusCollectionIntent
{
    // Detached snapshot after this frame's pickup clock update.
    Pickup pickup{};
    int collectorIndex = -1;
    int playerId = -1;
};

using BonusCollectionStarted =
    std::function<void(const BonusCollectionIntent &)>;
using BonusCollectionFinished = std::function<void(
    const BonusCollectionIntent &, const BonusApplication &)>;

struct BonusCollectionCallbacks
{
    BonusCollectionStarted started;
    BonusCollectionFinished finished;
};

enum class BonusPickupOutcome : unsigned char
{
    Invalid,
    Retained,
    Discarded,
    ApplicationRejected,
    Collected
};

struct BonusPickupTransaction
{
    BonusPickupOutcome outcome = BonusPickupOutcome::Invalid;
    PickupUpdate update{};
    BonusCollectionIntent intent{};
    BonusApplication application{};
};

enum class BonusPickupBatchOutcome : unsigned char
{
    Invalid,
    Advanced
};

using BonusPickupRemoved = std::function<void(
    const BonusCollectionIntent &, const BonusApplication &)>;

bool anyPlayerNeedsHealing(const std::vector<Player> &players);
BonusSpawnDecision bonusSpawnFromDraws(int typeSlot, bool bandageEligible,
                                       int xPixel, int zPixel);

// Owns one carrier release from the Bandage-eligibility snapshot through the
// synchronous detached commit. Its transcript is T-X-Z-Q-C for immediate
// acceptance and T-X-Z-Q-(X-Z-Q)*-C for retries; T is never repeated. Invalid
// callback dependencies are atomic and invoke nothing. drawTypeSlot must return
// [0, slotCount), and drawPixel must return [0, 383]. The type draw is checked
// after the first complete T-X-Z attempt; every later position attempt is
// checked after its complete X-Z pair. An invalid draw fails closed without Q
// or C. Position rejection must eventually accept; callbacks must be
// non-throwing, must not mutate the players vector, and must not re-enter. The
// commit callback must not retain references to the intent.
BonusReleaseOutcome advanceBonusReleaseTransaction(
    const std::vector<Player> &players, int sourceEnemyId,
    const BonusReleaseRandom &random,
    const BonusPositionRejected &positionRejected,
    const BonusReleaseCommit &commit);

int bonusPointsForGrenadeTargets(std::size_t targetCount);

// Advances one pickup before evaluating collection, preserving the current 3D
// order. Invalid elapsed time is a safe no-op; malformed pickup state is
// removed. The first eligible player in vector order wins a shared overlap.
PickupUpdate updatePickup(Pickup &pickup, float elapsedTime,
                          const std::vector<Player> &players);

// Owns common score/tally credit plus all player/enemy rule state.
// Map, event, FX, camera, message, and audio consumers execute the returned
// commands in order. Invalid input fails closed without mutating state.
BonusApplication applyBonus(std::vector<Player> &players,
                            std::vector<Enemy> &enemies, int playerIndex,
                            BonusType type);

// Owns one pickup's U-B-A-F transaction: update clocks/eligibility, begin a
// detached collection observation, apply the bonus, then synchronously finish
// concrete consumption. Missing callbacks are atomic. Retain/discard outcomes
// invoke neither callback; collection invokes started then finished exactly
// once. finished also receives a defensive applied=false result so the caller
// can roll back B and retain the pickup. The caller erases only
// Collected/Discarded pickups, after this function returns. Callbacks must be
// non-throwing, observation/presentation-only, must not re-enter or
// mutate/reorder/reallocate pickup/player/enemy containers, and must not retain
// intent or application references.
BonusPickupTransaction advanceBonusPickupTransaction(
    Pickup &pickup, float elapsedTime, std::vector<Player> &players,
    std::vector<Enemy> &enemies,
    const BonusCollectionCallbacks &callbacks);

// Advances the pickup vector in stable order using one synchronous U-B-A-F
// transaction per element. Collected and discarded pickups are erased at their
// current index, so the shifted successor is processed without being skipped;
// retained, rejected, and invalid transactions remain in place. Discard is
// silent. collectedRemoved, when present, runs only after a collected pickup is
// erased and receives detached transaction values. Missing started/finished
// callbacks reject the entire batch before any pickup, player, or enemy state is
// changed. All callbacks must be non-throwing, must not re-enter, mutate or
// reallocate any participating container, change the callback set, or retain
// references supplied by this function.
BonusPickupBatchOutcome advanceBonusPickups(
    std::vector<Pickup> &pickups, float elapsedTime,
    std::vector<Player> &players, std::vector<Enemy> &enemies,
    const BonusCollectionCallbacks &callbacks,
    const BonusPickupRemoved &collectedRemoved = BonusPickupRemoved{});
} // namespace tanks3d::game

#endif
