#ifndef TANKS3D_LAN_MENU_H
#define TANKS3D_LAN_MENU_H
#include "app/input_adapter.h"
#include "app/lan_session.h"
#include <algorithm>
#include <array>
#include <raylib.h>
#include <string>
#include <vector>

namespace tanks3d
{
enum class LanMenuAction
{
    None,
    Host,
    Join,
    Back
};

class LanMenu
{
  public:
    std::string address;
    std::string error;
    std::vector<std::string> hostAddresses;
    int selected = 0;
    bool editing = false;

    LanMenuAction update(const app::UiInputFrame &input)
    {
        if (editing)
        {
            if (input.cancelPressed)
            {
                editing = false;
                return LanMenuAction::None;
            }
            for (int key = GetCharPressed(); key != 0; key = GetCharPressed())
                if (((key >= '0' && key <= '9') || key == '.' || key == ':') &&
                    address.size() < 21)
                    address += static_cast<char>(key);
            if (IsKeyPressed(KEY_BACKSPACE) && !address.empty())
                address.pop_back();
            if ((IsKeyDown(KEY_LEFT_SUPER) || IsKeyDown(KEY_LEFT_CONTROL)) &&
                IsKeyPressed(KEY_V))
            {
                const char *clipboard = GetClipboardText();
                if (clipboard)
                {
                    std::string candidate;
                    for (int i = 0; i < 22 && clipboard[i]; ++i)
                        candidate += clipboard[i];
                    net::Endpoint endpoint;
                    if (net::parseEndpoint(candidate, endpoint))
                        address = candidate;
                }
            }
            if (input.confirmPressed)
            {
                editing = false;
                return LanMenuAction::Join;
            }
            return LanMenuAction::None;
        }
        if (input.cancelPressed)
            return LanMenuAction::Back;
        if (input.upPressed)
            selected = (selected + 3) % 4;
        if (input.downPressed)
            selected = (selected + 1) % 4;
        if (!input.confirmPressed)
            return LanMenuAction::None;
        if (selected == 0)
            return LanMenuAction::Host;
        if (selected == 3)
            return LanMenuAction::Back;
        if (selected == 2 || address.empty())
        {
            selected = 2;
            editing = true;
            return LanMenuAction::None;
        }
        return LanMenuAction::Join;
    }

    void draw(const app::LanSession &session, int stage, int lives,
              const std::string &nation, std::uint16_t port = net::kLanPort) const
    {
        const int width = GetScreenWidth(), height = GetScreenHeight();
        const int panelWidth = std::min(860, width - 40),
                  left = (width - panelWidth) / 2;
        const bool compact = height < 650;
        const int top = compact ? 78 : 132, row = compact ? 43 : 52;
        const auto centered = [&](const std::string &text, int y, int size, Color color)
        {
            while (size > 11 && MeasureText(text.c_str(), size) > width - 40)
                --size;
            DrawText(text.c_str(), (width - MeasureText(text.c_str(), size)) / 2, y,
                     size, color);
        };
        BeginDrawing();
        ClearBackground({10, 20, 28, 255});
        DrawRectangleGradientV(0, 0, width, height, {28, 53, 63, 255},
                               {7, 12, 18, 255});
        centered("LOCAL NETWORK CO-OP", compact ? 20 : 42, compact ? 30 : 42, GOLD);
        const auto phase = session.phase();
        const bool choosing =
            phase == app::LanPhase::Idle || phase == app::LanPhase::Failed;
        if (choosing)
        {
            centered("Two computers. Two tanks. One base to defend.", top - 32, 18,
                     {180, 211, 213, 255});
            const std::array<std::string, 4> labels{
                {"CREATE ROOM", "JOIN ROOM",
                 "HOST IP   " +
                     (address.empty() ? std::string("Enter address") : address) +
                     (editing ? "_" : ""),
                 "BACK"}};
            for (int i = 0; i < 4; ++i)
            {
                const int y = top + i * row;
                DrawRectangleRounded(
                    {static_cast<float>(left), static_cast<float>(y),
                     static_cast<float>(panelWidth), static_cast<float>(row - 6)},
                    .12f, 6,
                    i == selected ? Color{41, 78, 81, 255} : Color{10, 24, 31, 230});
                DrawText(labels[i].c_str(), left + 22, y + 12, 20,
                         i == selected ? RAYWHITE : Color{153, 182, 187, 255});
            }
            centered("YOUR NATION: " + nation + "    HOST STAGE: " +
                         std::to_string(stage) + "    LIVES: " + std::to_string(lives),
                     top + 4 * row + 18, 17, {233, 205, 122, 255});
            centered("Choose your nation and host rules in the main setup menu.",
                     top + 4 * row + 48, 16, {175, 197, 200, 255});
            centered(
                editing
                    ? "Type IPv4[:port] or paste with Cmd+V. Enter joins; Esc cancels."
                    : "ARROWS / PAD: SELECT    ENTER: OPEN    ESC: BACK",
                height - 67, 16, LIGHTGRAY);
            const std::string message = !error.empty() ? error : session.error();
            if (!message.empty())
                centered(message, height - 37, 15, {255, 137, 111, 255});
        }
        else
        {
            centered(session.isHost() ? "YOU ARE P1 / HOST" : "YOU ARE P2 / JOINING",
                     top + 4, 27, session.isHost() ? GOLD : Color{86, 223, 142, 255});
            centered(phase == app::LanPhase::Waiting
                         ? "Waiting for another player..."
                         : "Connecting and preparing the same battle...",
                     top + 62, 22, RAYWHITE);
            if (session.isHost())
            {
                centered("On the other computer, choose JOIN and enter:", top + 112, 18,
                         LIGHTGRAY);
                if (hostAddresses.empty())
                    centered("No active LAN IPv4 address found.", top + 150, 22,
                             ORANGE);
                for (std::size_t i = 0;
                     i < std::min<std::size_t>(hostAddresses.size(), 3); ++i)
                    centered(hostAddresses[i] + ":" + std::to_string(port),
                             top + 150 + static_cast<int>(i) * 34, 27,
                             Color{132, 227, 216, 255});
            }
            else
                centered(address, top + 140, 28, {132, 227, 216, 255});
            centered("Use the same app build on both computers, on the same network.",
                     height - 100, 17, LIGHTGRAY);
            centered("Allow Local Network access if macOS asks.", height - 73, 16,
                     {175, 197, 200, 255});
            centered("ESC / MINUS: CANCEL", height - 40, 18, GOLD);
        }
        EndDrawing();
    }
};

inline void drawLanStatus(const std::string &status)
{
    if (status.empty())
        return;
    const int width = GetScreenWidth(), height = GetScreenHeight();
    DrawRectangle(4, height - 49, width - 8, 45, {8, 22, 29, 235});
    const int size = 16;
    DrawText(status.c_str(),
             std::max(8, (width - MeasureText(status.c_str(), size)) / 2), height - 45,
             size, {245, 222, 146, 255});
    const char *controls = "ARROWS / WASD / PAD: MOVE    SPACE / FACE / RT: FIRE    "
                           "ENTER / +: PAUSE    ESC / -: LEAVE";
    int font = 14;
    while (font > 9 && MeasureText(controls, font) > width - 20)
        --font;
    DrawText(controls, (width - MeasureText(controls, font)) / 2, height - 23, font,
             {180, 205, 208, 255});
}
} // namespace tanks3d
#endif
