#ifndef TANKS3D_APP_LAN_GAME_BRIDGE_H
#define TANKS3D_APP_LAN_GAME_BRIDGE_H
#include "net/lan_protocol.h"

namespace tanks3d::app
{
enum class LanTickResult
{
    Continue,
    ReturnToMenu,
    CannotLoad
};

template <typename Game>
LanTickResult applyLanTick(Game &game, const net::Packet &packet)
{
    if ((packet.controls & net::Control::Confirm) &&
        (game.settling() || game.highScoreDisplay()))
        game.confirmSettlement();
    else if ((packet.controls & net::Control::Pause) && !game.endingSequence())
        game.togglePause();
    if ((packet.controls & net::Control::Restart) && !game.endingSequence() &&
        !game.restart())
        return LanTickResult::CannotLoad;
    if (game.consumeMenuRequest())
        return LanTickResult::ReturnToMenu;
    game::PlayerInputFrame input;
    for (std::size_t i = 0; i < input.players.size(); ++i)
        input.players[i] = net::unpackInput(packet.buttons[i]);
    game.update(net::kLanStep, input);
    return game.consumeMenuRequest() ? LanTickResult::ReturnToMenu
                                     : LanTickResult::Continue;
}
} // namespace tanks3d::app
#endif
