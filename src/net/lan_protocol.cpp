#include "net/lan_protocol.h"

#include <algorithm>
#include <limits>

namespace tanks3d::net
{
namespace
{
constexpr std::uint8_t kWireVersion = 1;
void put(std::vector<std::uint8_t> &out, std::uint64_t value, int count)
{
    for (int i = count - 1; i >= 0; --i)
        out.push_back(static_cast<std::uint8_t>(value >> (i * 8)));
}

bool read(const std::vector<std::uint8_t> &in, std::size_t &cursor,
          std::uint64_t &value, int count)
{
    if (cursor + static_cast<std::size_t>(count) > in.size())
        return false;
    value = 0;
    for (int i = 0; i < count; ++i)
        value = (value << 8) | in[cursor++];
    return true;
}

bool decimal(const std::string &text, unsigned limit, unsigned &value)
{
    if (text.empty() || text.size() > 5)
        return false;
    value = 0;
    for (char c : text)
    {
        if (c < '0' || c > '9')
            return false;
        value = value * 10 + static_cast<unsigned>(c - '0');
        if (value > limit)
            return false;
    }
    return true;
}
} // namespace

bool parseEndpoint(const std::string &text, Endpoint &endpoint)
{
    if (text.size() > 21)
        return false;
    Endpoint candidate;
    const auto colon = text.find(':');
    candidate.address = text.substr(0, colon);
    if (colon != std::string::npos)
    {
        unsigned port = 0;
        if (!decimal(text.substr(colon + 1), 65535, port) || port == 0)
            return false;
        candidate.port = static_cast<std::uint16_t>(port);
    }
    std::size_t begin = 0;
    std::array<unsigned, 4> octets{};
    for (std::size_t i = 0; i < octets.size(); ++i)
    {
        const auto end = candidate.address.find('.', begin);
        if ((end == std::string::npos) != (i == 3))
            return false;
        const auto part = candidate.address.substr(begin, end - begin);
        if (part.size() > 3 || (part.size() > 1 && part[0] == '0') ||
            !decimal(part, 255, octets[i]))
            return false;
        begin = end + 1;
    }
    if (octets[0] == 0 || octets[0] >= 224 || (octets[0] == 255 && octets[3] == 255))
        return false;
    endpoint = candidate;
    return true;
}

bool validRoom(const RoomSettings &room)
{
    const auto &a = room.advanced;
    const auto tuning = [](int value)
    {
        return value >= -30 && value <= 30 && value % 5 == 0;
    };
    return room.stage >= 1 && room.stage <= 35 && room.lives >= 1 && room.lives <= 99 &&
           static_cast<unsigned>(room.nations[0]) < 3 &&
           static_cast<unsigned>(room.nations[1]) < 3 &&
           a.playerMaximumHitPoints >= 1 && a.playerMaximumHitPoints <= 6 &&
           tuning(a.enemySpeedPercent) && tuning(a.enemyFireRatePercent) &&
           tuning(a.enemySpawnRatePercent);
}

std::uint16_t packInput(const game::PlayerControlFrame &input)
{
    std::uint16_t result = input.fireHeld ? 256 : 0;
    const std::array<game::DirectionButtonFrame, 4> directions{
        {input.north, input.south, input.west, input.east}};
    for (std::size_t i = 0; i < directions.size(); ++i)
    {
        if (directions[i].held)
            result |= 1U << i;
        if (directions[i].pressed)
            result |= 1U << (i + 4);
    }
    return result;
}

game::PlayerControlFrame unpackInput(std::uint16_t bits)
{
    game::PlayerControlFrame input;
    std::array<game::DirectionButtonFrame *, 4> directions{
        {&input.north, &input.south, &input.west, &input.east}};
    for (std::size_t i = 0; i < directions.size(); ++i)
        *directions[i] = {(bits & (1U << i)) != 0, (bits & (1U << (i + 4))) != 0};
    input.fireHeld = (bits & 256) != 0;
    return input;
}

std::uint64_t stateHash(const std::string &state)
{
    std::uint64_t hash = 14695981039346656037ULL;
    for (unsigned char c : state)
    {
        hash ^= c;
        hash *= 1099511628211ULL;
    }
    return hash;
}

std::vector<std::uint8_t> encodePacket(const Packet &packet)
{
    std::vector<std::uint8_t> out{
        'T', '3', 'L', 'N', kWireVersion, static_cast<std::uint8_t>(packet.kind), 0, 0};
    switch (packet.kind)
    {
    case PacketKind::Hello:
        if (packet.build.empty() || packet.build.size() > 128 ||
            static_cast<unsigned>(packet.nation) >= 3)
            return {};
        put(out, static_cast<unsigned>(packet.nation), 1);
        out.insert(out.end(), packet.build.begin(), packet.build.end());
        break;
    case PacketKind::Welcome:
        if (!validRoom(packet.room) || packet.build.empty() ||
            packet.build.size() > 128)
            return {};
        put(out, packet.room.seed, 4);
        put(out, packet.room.stage, 1);
        put(out, packet.room.lives, 1);
        for (auto nation : packet.room.nations)
            put(out, static_cast<unsigned>(nation), 1);
        put(out, packet.room.advanced.playerMaximumHitPoints, 1);
        put(out, packet.room.advanced.enemySpeedPercent + 30, 1);
        put(out, packet.room.advanced.enemyFireRatePercent + 30, 1);
        put(out, packet.room.advanced.enemySpawnRatePercent + 30, 1);
        out.insert(out.end(), packet.build.begin(), packet.build.end());
        break;
    case PacketKind::Input:
    case PacketKind::Tick:
        if (packet.buttons[0] > 511 || packet.buttons[1] > 511 || packet.controls > 7)
            return {};
        put(out, packet.tick, 4);
        put(out, packet.buttons[0], 2);
        if (packet.kind == PacketKind::Tick)
            put(out, packet.buttons[1], 2);
        put(out, packet.controls, 1);
        break;
    case PacketKind::Digest:
        put(out, packet.tick, 4);
        put(out, packet.digest, 8);
        break;
    case PacketKind::Leave:
        if (static_cast<unsigned>(packet.reason) > 3)
            return {};
        put(out, static_cast<unsigned>(packet.reason), 1);
        break;
    case PacketKind::Ready:
    case PacketKind::Heartbeat:
        break;
    default:
        return {};
    }
    out[6] = static_cast<std::uint8_t>(out.size() >> 8);
    out[7] = static_cast<std::uint8_t>(out.size());
    return out;
}

bool decodePacket(std::vector<std::uint8_t> &bytes, Packet &packet, bool &available)
{
    available = false;
    if (bytes.size() > kMaximumBufferedBytes)
        return false;
    if (bytes.size() < 8)
        return true;
    if (bytes[0] != 'T' || bytes[1] != '3' || bytes[2] != 'L' || bytes[3] != 'N' ||
        bytes[4] != kWireVersion || bytes[5] < 1 || bytes[5] > 8)
        return false;
    const std::size_t size = (static_cast<unsigned>(bytes[6]) << 8) | bytes[7];
    if (size < 8 || size > kMaximumPacketSize)
        return false;
    if (bytes.size() < size)
        return true;
    const std::vector<std::uint8_t> body(bytes.begin(), bytes.begin() + size);
    Packet candidate;
    candidate.kind = static_cast<PacketKind>(body[5]);
    std::size_t cursor = 8;
    const auto get = [&](int count)
    {
        std::uint64_t value = 0;
        if (!read(body, cursor, value, count))
            cursor = size + 1;
        return value;
    };
    switch (candidate.kind)
    {
    case PacketKind::Hello:
        candidate.nation = static_cast<core::Nation>(get(1));
        if (cursor > size || size - cursor > 128 || cursor == size)
            return false;
        candidate.build.assign(body.begin() + cursor, body.end());
        cursor = size;
        if (static_cast<unsigned>(candidate.nation) >= 3)
            return false;
        break;
    case PacketKind::Welcome:
        candidate.room.seed = static_cast<std::uint32_t>(get(4));
        candidate.room.stage = static_cast<int>(get(1));
        candidate.room.lives = static_cast<int>(get(1));
        for (auto &nation : candidate.room.nations)
            nation = static_cast<core::Nation>(get(1));
        candidate.room.advanced.playerMaximumHitPoints = static_cast<int>(get(1));
        candidate.room.advanced.enemySpeedPercent = static_cast<int>(get(1)) - 30;
        candidate.room.advanced.enemyFireRatePercent = static_cast<int>(get(1)) - 30;
        candidate.room.advanced.enemySpawnRatePercent = static_cast<int>(get(1)) - 30;
        if (!validRoom(candidate.room) || cursor >= size || size - cursor > 128)
            return false;
        candidate.build.assign(body.begin() + cursor, body.end());
        cursor = size;
        break;
    case PacketKind::Input:
    case PacketKind::Tick:
        candidate.tick = static_cast<std::uint32_t>(get(4));
        candidate.buttons[0] = static_cast<std::uint16_t>(get(2));
        if (candidate.kind == PacketKind::Tick)
            candidate.buttons[1] = static_cast<std::uint16_t>(get(2));
        candidate.controls = static_cast<std::uint8_t>(get(1));
        if (candidate.buttons[0] > 511 || candidate.buttons[1] > 511 ||
            candidate.controls > 7)
            return false;
        break;
    case PacketKind::Digest:
        candidate.tick = static_cast<std::uint32_t>(get(4));
        candidate.digest = get(8);
        break;
    case PacketKind::Leave:
        candidate.reason = static_cast<LeaveReason>(get(1));
        if (static_cast<unsigned>(candidate.reason) > 3)
            return false;
        break;
    case PacketKind::Ready:
    case PacketKind::Heartbeat:
        break;
    }
    if (cursor != size)
        return false;
    packet = candidate;
    available = true;
    bytes.erase(bytes.begin(), bytes.begin() + size);
    return true;
}

const char *leaveMessage(LeaveReason reason)
{
    switch (reason)
    {
    case LeaveReason::Incompatible:
        return "Different game builds. Use the same app build on both computers.";
    case LeaveReason::Desynchronized:
        return "Game states differ. The room has stopped; create a new room.";
    case LeaveReason::CannotStart:
        return "The other computer could not start this stage.";
    default:
        return "The other player left the room.";
    }
}
} // namespace tanks3d::net
