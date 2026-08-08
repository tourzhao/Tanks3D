#ifndef TANKS3D_GAME_GAME_EVENT_H
#define TANKS3D_GAME_GAME_EVENT_H

#include "core/coordinates.h"
#include "game/bonus_rules.h"
#include "game/entities.h"
#include "game/stage_map.h"

namespace tanks3d::game
{
enum class GameEventType : unsigned char
{
    ShellFired,
    ShellCancelled,
    BrickHit,
    TankDamaged,
    TankDestroyed,
    PlayerRespawned,
    BonusSpawned,
    BonusCollected,
    BaseDamaged,
    StageEnded
};

enum class GameEventCause : unsigned char
{
    None,
    PlayerShell,
    EnemyShell,
    GrenadeBonus
};

enum class GovernmentBasePart : unsigned char
{
    None,
    Wall,
    Core
};

enum class StageEndReason : unsigned char
{
    None,
    Cleared,
    BaseDestroyed,
    PlayersDefeated
};

// Presentation-neutral observation payload. Fields that do not apply to an
// event keep their sentinel values, and no member owns mutable game state.
struct GameEvent
{
    GameEventType type = GameEventType::ShellFired;
    GameEventCause cause = GameEventCause::None;
    XZ position{};
    CardinalDirection direction = CardinalDirection::None;
    int sourcePlayerId = -1;
    int sourceEnemyId = -1;
    int targetPlayerId = -1;
    int targetEnemyId = -1;
    int enemyType = -1;
    BonusType bonusType = BonusType::Count;
    ImpactKind impactKind = ImpactKind::None;
    GovernmentBasePart basePart = GovernmentBasePart::None;
    StageEndReason stageEndReason = StageEndReason::None;
    int stage = 0;
    int row = -1;
    int column = -1;
    int baseSegmentIndex = -1;
    int valueBefore = 0;
    int valueAfter = 0;
    int points = 0;
    bool power = false;
};

inline bool operator==(const GameEvent &first, const GameEvent &second)
{
    return first.type == second.type && first.cause == second.cause &&
           first.position.x == second.position.x &&
           first.position.z == second.position.z &&
           first.direction == second.direction &&
           first.sourcePlayerId == second.sourcePlayerId &&
           first.sourceEnemyId == second.sourceEnemyId &&
           first.targetPlayerId == second.targetPlayerId &&
           first.targetEnemyId == second.targetEnemyId &&
           first.enemyType == second.enemyType &&
           first.bonusType == second.bonusType &&
           first.impactKind == second.impactKind &&
           first.basePart == second.basePart &&
           first.stageEndReason == second.stageEndReason &&
           first.stage == second.stage && first.row == second.row &&
           first.column == second.column &&
           first.baseSegmentIndex == second.baseSegmentIndex &&
           first.valueBefore == second.valueBefore &&
           first.valueAfter == second.valueAfter &&
           first.points == second.points && first.power == second.power;
}

inline GameEvent shellEvent(GameEventType type, ShellOwner owner,
                            int ownerIndex, XZ position, bool power)
{
    GameEvent event;
    event.type = type;
    event.position = position;
    event.power = power;
    if (owner == ShellOwner::Player)
    {
        event.cause = GameEventCause::PlayerShell;
        event.sourcePlayerId = ownerIndex;
    }
    else
    {
        event.cause = GameEventCause::EnemyShell;
        event.sourceEnemyId = ownerIndex;
    }
    return event;
}

inline GameEvent shellEvent(GameEventType type, const Shell &shell,
                            XZ position)
{
    return shellEvent(type, shell.owner, shell.ownerIndex, position,
                      shell.power);
}
} // namespace tanks3d::game

#endif
