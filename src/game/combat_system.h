#ifndef TANKS3D_GAME_COMBAT_SYSTEM_H
#define TANKS3D_GAME_COMBAT_SYSTEM_H

#include "game/entities.h"
#include "game/game_event.h"
#include "game/stage_map.h"

#include <functional>
#include <vector>

namespace tanks3d::game
{
// These values preserve the classic 8x8 projectile footprint and the existing
// fixed-step simulation. Impacting shells remain owned until their animation
// finishes, so they continue to occupy a firing slot.
inline constexpr float kShellTankHitExtent = kTankRadius + kShellHalfSize;
inline constexpr float kShellSpawnDistance = kTankRadius - kShellHalfSize;
inline constexpr float kShellSweepStep = 0.12f;
inline constexpr float kShellImpactDuration = 0.200f;
inline constexpr float kShellCancellationExtent = kShellHalfSize * 2.0f;
inline constexpr int kDirectEnemyHitPoints = 50;

enum class CombatTarget
{
    None,
    StageMap,
    GovernmentCore,
    EnemyTank,
    PlayerTank
};

// A presentation-free snapshot of one ordered shell-impact query.
// ImpactKind remains specific to StageMap; a government-core hit therefore
// has ImpactKind::None and is distinguished by target. Likewise,
// CombatTarget::None does not mean the map was unchanged because a power shell
// can clear forest without stopping.
struct CombatOutcome
{
    CombatTarget target = CombatTarget::None;
    ImpactKind impactKind = ImpactKind::None;
    ShellImpactDetails impactDetails{};
    XZ position{};
    XZ incomingVelocity{};
    ShellOwner owner = ShellOwner::Player;
    int ownerIndex = -1;
    bool power = false;
    int governmentCoreHealthBefore = 0;
    int governmentCoreHealthAfter = 0;
    int targetEnemyIndex = -1;
    int targetEnemyId = -1;
    int targetEnemyType = -1;
    XZ targetEnemyPosition{};
    bool targetEnemyCarriedBonus = false;
    int targetEnemyArmorBefore = 0;
    int targetEnemyArmorAfter = 0;
    int targetPlayerIndex = -1;
    int targetPlayerId = -1;
    XZ targetPlayerPosition{};
    int targetPlayerHitPointsBefore = 0;
    bool targetPlayerShielded = false;
    bool targetPlayerHadBoat = false;

    bool mapStopsShell() const;
    bool stopsShell() const;
    bool damagedGovernmentCore() const;
    bool destroyedGovernmentCore() const;
    bool damagedEnemyTank() const;
    bool destroyedEnemyTank() const;
};

struct EnemyTankImpactCommit
{
    bool applied = false;
    bool destroyedNow = false;
    int creditedPlayerIndex = -1;
    int eventPoints = 0;
};

struct PlayerTankImpactCommit
{
    bool applied = false;
    PlayerHitResult hitResult = PlayerHitResult::Shielded;
    int hitPointsBefore = 0;
    int hitPointsAfter = 0;
};

// One ordered, presentation-free physical-impact transaction for a single
// shell. A resolved map/core hit is authoritative; tank hits are resolved only
// after their validated commit succeeds. The incoming shell remains untouched
// so the caller can present the result before beginning its impact animation.
struct ShellPhysicalImpactResult
{
    CombatOutcome outcome{};
    EnemyTankImpactCommit enemyCommit{};
    PlayerTankImpactCommit playerCommit{};
    bool resolved = false;
};

struct ShellFrameSchedule
{
    int stepCount = 1;
    float stepTime = 0.0f;
};

struct ShellCancellationOutcome
{
    int firstShellIndex = -1;
    int secondShellIndex = -1;
    XZ position{};
    XZ firstImpactPosition{};
    XZ secondImpactPosition{};
    ShellOwner firstOwner = ShellOwner::Player;
    int firstOwnerIndex = -1;
    ShellOwner secondOwner = ShellOwner::Enemy;
    int secondOwnerIndex = -1;
};

using CarrierBonusRelease = std::function<void(const Enemy &)>;
using PlayerTankPreCommit = std::function<void(const Player &)>;

XZ shellSpawnPosition(XZ tankPosition, CardinalDirection direction);
// The caller guarantees a live, non-impacting shell. This mutates StageMap but
// deliberately leaves the shell intact so ordered event/effect consumers can
// read the incoming state before calling beginShellImpact().
CombatOutcome resolveShellMapImpact(StageMap &map, const Shell &shell);
// This consumes the result of resolveShellMapImpact() and refuses a core hit
// when that prior result stopped at terrain, a wall, steel, or a boundary. The
// binary base state is the core's current 1 HP. StageMap and the incoming
// snapshot stay unchanged.
CombatOutcome resolveShellGovernmentCoreImpact(const StageMap &map,
                                               bool &baseAlive,
                                               const CombatOutcome &mapOutcome);
// This consumes the non-stopping environment outcome, selects the first live
// fully-created enemy under the player's shell, and evaluates exactly one
// armor hit without mutating the enemy. Pass the result immediately to the
// commit helper so its validated carrier callback precedes armor mutation.
CombatOutcome evaluatePlayerShellEnemyTankImpact(
    const std::vector<Enemy> &enemies,
    const CombatOutcome &environmentOutcome);
// Validates the evaluator snapshot before any side effect. A carrier callback
// runs only for the first valid commit and before armor changes; it must not
// mutate the enemy/player containers. The function then owns armor, direct-hit
// score/tally/streak, and fatal enemy state, but no events or presentation.
EnemyTankImpactCommit commitPlayerShellEnemyTankImpact(
    std::vector<Enemy> &enemies, std::vector<Player> &players,
    const CombatOutcome &outcome,
    const CarrierBonusRelease &releaseCarrierBonus = {});
// This consumes the still-open outcome after the enemy-target query. It only
// selects active, fully-created players with positive HP and leaves all state
// untouched until the matching commit helper runs.
CombatOutcome evaluateEnemyShellPlayerTankImpact(
    const std::vector<Player> &players,
    const CombatOutcome &previousOutcome);
// Validates the player snapshot, optionally invokes a read-only pre-commit
// callback (used by Game3D for the existing generic impact effect), then owns
// Shield/Boat/HP resolution and the fatal player lifecycle transition.
PlayerTankImpactCommit commitEnemyShellPlayerTankImpact(
    std::vector<Player> &players, const CombatOutcome &outcome,
    const PlayerTankPreCommit &beforeCommit = {});
// Resolves exactly one live shell in map -> core -> enemy -> player order. The
// callbacks preserve the transitional carrier-release and generic player-impact
// timing, and must not mutate the shell, map, enemy, or player containers. This
// function never calls beginShellImpact(); presentation consumers do that last.
ShellPhysicalImpactResult resolveShellPhysicalImpact(
    StageMap &map, bool &baseAlive, std::vector<Enemy> &enemies,
    std::vector<Player> &players, const Shell &shell,
    const CarrierBonusRelease &releaseCarrierBonus = {},
    const PlayerTankPreCommit &beforePlayerCommit = {});
// Projects a resolved physical transaction into detached value observations. Map
// damage retains scan order; presentation and shell finalization stay with the
// caller so branch-specific effect/audio timing remains unchanged.
std::vector<GameEvent> eventsForPhysicalShellImpact(
    const ShellPhysicalImpactResult &result);
void beginShellImpact(Shell &shell, XZ impactPosition);
// Ages every shell once, then derives a shared fixed micro-step count from the
// fastest still-flying shell. A shell that enters impact later in this frame
// therefore receives the full kShellImpactDuration from beginShellImpact().
ShellFrameSchedule prepareShellFrame(std::vector<Shell> &shells, float dt);
// Records every shell's starting point before moving only live, non-impacting
// shells. Keeping the snapshot separate preserves physical-hit-before-shell-
// cancellation ordering in the Game3D compatibility orchestrator.
void advanceShellMicrostep(std::vector<Shell> &shells, float stepTime,
                           std::vector<XZ> &previousPositions);
// Stable erase at the end of the frame keeps active firing slots occupied for
// the complete impact duration and preserves survivor order.
void removeExpiredShells(std::vector<Shell> &shells);
bool shellsCanCancel(const Shell &first, const Shell &second);
bool shellsOverlapForCancellation(const Shell &first, const Shell &second);
bool shellCancellationPoint(const Shell &first, const Shell &second,
                            float dt, XZ &collisionPoint,
                            XZ *firstAtCollision = nullptr,
                            XZ *secondAtCollision = nullptr);
bool resolveSweptShellCancellation(Shell &first, Shell &second,
                                   XZ firstStart, XZ secondStart, float dt,
                                   const StageMap &map, XZ &collisionPoint);
// Greedily resolves surviving opposing pairs in vector order. Each first shell
// stops at its first successful second shell; rejected candidates do not block
// later candidates. Results snapshot attribution for presentation consumers and
// never retain references into the caller-owned vector. A mismatched start
// snapshot is a safe no-op.
std::vector<ShellCancellationOutcome> resolveShellCancellations(
    std::vector<Shell> &shells,
    const std::vector<XZ> &previousPositions, float stepTime,
    const StageMap &map);
// Converts one resolver snapshot without presentation or live-shell reads. The
// resolver is the validity boundary: every returned outcome remains one event.
GameEvent eventForShellCancellation(
    const ShellCancellationOutcome &outcome);
} // namespace tanks3d::game

#endif
