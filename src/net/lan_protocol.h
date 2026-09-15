#ifndef TANKS3D_NET_LAN_PROTOCOL_H
#define TANKS3D_NET_LAN_PROTOCOL_H

#include "core/gameplay_rules.h"
#include "core/nation.h"
#include "game/player_system.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace tanks3d::net
{
inline constexpr std::uint16_t kLanPort = 41987;
inline constexpr float kLanStep = 1.0f / 60.0f;
inline constexpr std::uint32_t kDigestInterval = 120;
inline constexpr std::size_t kMaximumPacketSize = 256;
inline constexpr std::size_t kMaximumBufferedBytes = 65536;

enum class PacketKind : std::uint8_t
{
    Hello = 1,
    Welcome,
    Ready,
    Input,
    Tick,
    Digest,
    Leave,
    Heartbeat
};

enum Control : std::uint8_t
{
    Pause = 1,
    Restart = 2,
    Confirm = 4
};

enum class LeaveReason : std::uint8_t
{
    Left,
    Incompatible,
    Desynchronized,
    CannotStart
};

struct RoomSettings
{
    std::uint32_t seed = 0;
    int stage = 1;
    int lives = 10;
    std::array<core::Nation, 2> nations{
        {core::Nation::UnitedStates, core::Nation::SovietUnion}};
    core::AdvancedGameSettings advanced{};
};

struct Packet
{
    PacketKind kind = PacketKind::Heartbeat;
    RoomSettings room{};
    std::string build;
    std::uint32_t tick = 0;
    std::array<std::uint16_t, 2> buttons{};
    std::uint8_t controls = 0;
    std::uint64_t digest = 0;
    core::Nation nation = core::Nation::UnitedStates;
    LeaveReason reason = LeaveReason::Left;
};

struct Endpoint
{
    std::string address;
    std::uint16_t port = kLanPort;
};

bool parseEndpoint(const std::string &text, Endpoint &endpoint);
bool validRoom(const RoomSettings &room);
std::uint16_t packInput(const game::PlayerControlFrame &input);
game::PlayerControlFrame unpackInput(std::uint16_t bits);
std::uint64_t stateHash(const std::string &state);
std::vector<std::uint8_t> encodePacket(const Packet &packet);
// Reads at most one complete packet; incomplete data is retained. Malformed
// input returns false, with no partially decoded packet committed.
bool decodePacket(std::vector<std::uint8_t> &bytes, Packet &packet, bool &available);
const char *leaveMessage(LeaveReason reason);
} // namespace tanks3d::net
#endif
