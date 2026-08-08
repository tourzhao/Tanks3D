#include "app/shell_cancellation_presentation.h"

#include <utility>

namespace tanks3d::app
{
std::optional<ShellCancellationPresentationCommand>
makeShellCancellationPresentationCommand(game::GameEvent event)
{
    if (event.type != game::GameEventType::ShellCancelled)
        return std::nullopt;

    const core::XZ contactPosition = event.position;
    return ShellCancellationPresentationCommand{
        {std::move(event)},
        {contactPosition, 0.67f, {}, 1.0f, false},
        {audio::AudioCue::BulletHit}};
}
} // namespace tanks3d::app
