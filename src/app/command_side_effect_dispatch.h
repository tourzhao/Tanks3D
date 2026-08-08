#ifndef TANKS3D_APP_COMMAND_SIDE_EFFECT_DISPATCH_H
#define TANKS3D_APP_COMMAND_SIDE_EFFECT_DISPATCH_H

#include "app/command_side_effect_sink.h"
#include "app/shell_cancellation_presentation.h"
#include "app/shell_map_core_presentation.h"
#include "app/shell_tank_presentation.h"
#include "game/bonus_system.h"

namespace tanks3d::app
{
enum class CommandSideEffectDisposition : unsigned char
{
    Applied,
    DomainCommitRequired,
    Ignored,
    Rejected
};

CommandSideEffectDisposition dispatchCommandSideEffect(
    const AppendShellCancellationEventAction &action,
    CommandSideEffectSink &sink);
CommandSideEffectDisposition dispatchCommandSideEffect(
    const SpawnShellCancellationImpactAction &action,
    CommandSideEffectSink &sink);
CommandSideEffectDisposition dispatchCommandSideEffect(
    const RequestShellCancellationAudioAction &action,
    CommandSideEffectSink &sink);

CommandSideEffectDisposition dispatchCommandSideEffect(
    const ShellMapCorePresentationAction &action,
    CommandSideEffectSink &sink);
CommandSideEffectDisposition dispatchCommandSideEffect(
    const ShellTankPresentationAction &action,
    CommandSideEffectSink &sink);

// Bonus commands include one world-state commit (Shovel steel) and one
// sentinel. The dispatcher applies only detached event/FX/camera/audio leaves
// and reports the domain commit back to Game3D for synchronous execution.
CommandSideEffectDisposition dispatchCommandSideEffect(
    const game::BonusEffectCommand &command,
    CommandSideEffectSink &sink);
} // namespace tanks3d::app

#endif
