#ifndef TANKS3D_GAME_PLAYER_SYSTEM_H
#define TANKS3D_GAME_PLAYER_SYSTEM_H

#include "core/coordinates.h"

#include <array>
#include <functional>
#include <optional>

namespace tanks3d::game
{
using core::CardinalDirection;

struct DirectionButtonFrame
{
    // Application snapshots always satisfy pressed => held. Synthetic input
    // may violate that constraint; planning still preserves legacy behavior.
    bool held = false;
    bool pressed = false;
};

struct PlayerControlFrame
{
    DirectionButtonFrame north;
    DirectionButtonFrame south;
    DirectionButtonFrame west;
    DirectionButtonFrame east;
    bool fireHeld = false;
};

struct PlayerInputFrame
{
    std::array<PlayerControlFrame, 2> players{};
};

struct PlayerControlPlan
{
    CardinalDirection driveDirection = CardinalDirection::None;
    bool propelling = false;
    bool fireHeld = false;
};

// Resolves only data-only direction selection and fire intent. Separate frame
// and movement transactions own scalar timing and ice/collision/position
// rules; clock storage/commit, projectiles, and concrete presentation remain
// with the caller.
PlayerControlPlan planPlayerControl(
    CardinalDirection currentDrive, const PlayerControlFrame &controls);

enum class PlayerFramePhase : unsigned char
{
    Invalid,
    Inactive,
    Creating,
    Ready
};

struct PlayerFrameClockState
{
    float creationTimer = 0.0f;
    float fireCooldown = 0.0f;
    float dustCooldown = 0.0f;
    float shieldTimer = 0.0f;
    float streakPopupTimer = 0.0f;
};

struct PlayerFrameStart
{
    bool valid = false;
    PlayerFramePhase phase = PlayerFramePhase::Invalid;
    PlayerFrameClockState clocks{};
};

// Debits only the scalar clocks at the start of one player's frame and chooses
// the lifecycle phase from the entry snapshot. Dust, shield, and streak clocks
// run in every valid lifecycle phase; creation runs only for an active player
// that entered in Creating, and fire runs only in Ready. Crossing creation to
// zero therefore still returns Creating. Non-finite or negative elapsed time
// is rejected atomically. Clock fields are intentionally not validated:
// runtime callers normally provide finite values, while malformed clocks retain
// the legacy entry comparison and ordered std::max behavior on the selected
// phase's debits.
PlayerFrameStart beginPlayerFrame(
    bool active, const PlayerFrameClockState &clocks, float elapsed);

struct PlayerDeathState
{
    float deathTimer = 0.0f;
    int lives = 0;
};

enum class PlayerDeathTransition
{
    None,
    Waiting,
    Respawn,
    Eliminated,
    Invalid
};

// Advances only an inactive player's scalar destruction countdown and life
// debit. The caller has already passed beginPlayerFrame's Inactive gate and
// retains Player storage, owned-shell cleanup, spawn, events, and audio.
// Non-finite or negative elapsed time is atomic. The death clock intentionally
// retains the legacy entry comparison and ordered std::max behavior for
// malformed values. A timer that crosses zero debits a life in the same frame;
// non-positive life counts saturate at zero without signed overflow.
PlayerDeathTransition advanceInactivePlayerDeath(
    PlayerDeathState &state, float elapsed);

inline constexpr float kPlayerTrackDustCooldown = 0.120f;

// Entity-free snapshot of the fields owned by one active movement attempt.
// `moving` enters as the previous frame's momentum and leaves as the current
// frame result. Runtime callers guarantee finite position, yaw, and ice clock.
struct PlayerMovementState
{
    core::XZ position{};
    float yaw = 0.0f;
    CardinalDirection driveDirection = CardinalDirection::None;
    CardinalDirection movementDirection = CardinalDirection::None;
    bool moving = false;
    float iceSlipTimer = 0.0f;
    bool onIce = false;
};

enum class PlayerSpawnProgression : unsigned char
{
    Preserve,
    Reset
};

enum class PlayerSpawnOutcome : unsigned char
{
    Invalid,
    Prepared
};

// Entity-free snapshot of every Player field owned by creation/stage-entry
// reset. Identity, nation, lives, maximum HP, score, and stage tally stay with
// the caller and therefore cannot be changed by this transaction.
struct PlayerSpawnState
{
    PlayerMovementState movement{};
    PlayerFrameClockState clocks{};
    int hitPoints = 0;
    int level = 0;
    bool active = false;
    bool hasBoat = false;
    float respawnTimer = 0.0f;
    float deathTimer = 0.0f;
    int directKillStreak = 0;
};

struct PlayerSpawnParameters
{
    // The application retains entity-id-to-slot policy and supplies the
    // resolved world position. Maximum HP intentionally remains unvalidated:
    // legacy reset copies it verbatim, including malformed sentinel values.
    core::XZ spawnPosition{};
    int maximumHitPoints = 0;
    PlayerSpawnProgression progression =
        PlayerSpawnProgression::Preserve;
    float creationDuration = -1.0f;
    float initialFireCooldown = -1.0f;
    float shieldDuration = -1.0f;
};

// Prepares initial creation, stage entry, or a new life without owning Player
// storage or presentation. Invalid position/duration/mode input is atomic.
// Existing HP, level, and streak values otherwise retain legacy comparison
// and copy behavior; in particular, they are not clamped or validated.
PlayerSpawnOutcome preparePlayerSpawnState(
    PlayerSpawnState &state, const PlayerSpawnParameters &parameters);

struct PlayerMovementParameters
{
    // None and unknown cardinal values are intentionally accepted so legacy
    // zero-vector movement remains observable rather than failing closed.
    CardinalDirection driveDirection = CardinalDirection::None;
    bool propelling = false;
    float elapsed = -1.0f;
    float movementSpeed = -1.0f;
    // The application derives this fact from StageMap before entering the
    // scalar boundary. The raw dust clock preserves the legacy `<= 0` gate.
    bool surfaceIsIce = false;
    float dustCooldown = 0.0f;
};

struct PlayerTrackDustIntent
{
    core::XZ position{};
    core::XZ velocity{};
};

struct PlayerMovementUpdate
{
    bool valid = false;
    // For a valid attempt these flags are mutually exclusive. Both are false
    // when stationary; a permitted zero-distance move is still accepted.
    bool blocked = false;
    bool movementAccepted = false;
    std::optional<PlayerTrackDustIntent> dust{};
};

using PlayerPositionAvailable = std::function<bool(core::XZ)>;

// Commits only scalar movement state after the caller has passed lifecycle
// gates and debited per-frame clocks. Availability queries retain the legacy
// optional-lane-snap then forward-candidate order. A blocked forward move keeps
// an accepted snap but cancels ice carry. Dust is detached; the caller owns FX
// and commits kPlayerTrackDustCooldown after presentation. Invalid elapsed
// time, speed, overflowing distance, or an empty callback leaves state
// untouched and issues no query.
// The callback must be non-throwing and must not re-enter, mutate state or
// parameters through external aliases, or invalidate caller-owned entities or
// containers that supplied the snapshot.
PlayerMovementUpdate advanceActivePlayerMovement(
    PlayerMovementState &state,
    const PlayerMovementParameters &parameters,
    const PlayerPositionAvailable &positionAvailable);

enum class PlayerFireOutcome : unsigned char
{
    Invalid,
    NotRequested,
    CoolingDown,
    ShellLimitReached,
    Fired
};

struct PlayerFireParameters
{
    bool requested = false;
    int activeShellCount = 0;
    // This is the owning slot/index, matching Shell::ownerIndex. It is not a
    // mutable Player::id copied from an entity.
    int playerIndex = -1;
    int playerLevel = 0;
    // Runtime callers guarantee a finite world position. Direction values are
    // intentionally not validated so legacy None/unknown directions retain
    // their center-spawn, zero-velocity behavior.
    core::XZ tankPosition{};
    CardinalDirection direction = CardinalDirection::None;
    // CombatSystem owns the shared spawn geometry and the application owns
    // the reload clock. Passing both values keeps this boundary independent.
    float reloadInterval = -1.0f;
    float shellSpawnDistance = -1.0f;
};

struct PlayerShellLaunchIntent
{
    core::XZ tankPosition{};
    core::XZ position{};
    core::XZ velocity{};
    CardinalDirection direction = CardinalDirection::None;
    int playerIndex = -1;
    int playerLevel = 0;
    bool power = false;
};

using PlayerShellLaunch =
    std::function<void(const PlayerShellLaunchIntent &)>;

// Owns only the post-movement scalar firing transaction. Invalid static input,
// an idle trigger, and a cooling clock are atomic. A due attempt resets the
// clock before testing the active-shell limit, matching the 2D firing timer;
// the synchronous callback runs only for an accepted launch and already sees
// that reset. The callback must be non-throwing, must not re-enter, mutate, or
// invalidate either input, and must not retain the intent reference. The caller
// retains Shell construction and all presentation.
PlayerFireOutcome advancePlayerFireTransaction(
    float &fireCooldown, const PlayerFireParameters &parameters,
    const PlayerShellLaunch &launch);
} // namespace tanks3d::game

#endif
