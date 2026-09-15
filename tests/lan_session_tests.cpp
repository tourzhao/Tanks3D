#include "app/lan_session.h"
#include "test_support.h"
#include <cstdlib>
#include <iostream>
#include <limits>

using namespace tanks3d;
namespace
{
tanks3d_test::Reporter reporter;
void require(bool value, const std::string &message)
{
    if (!reporter.check(value, message))
        std::exit(1);
}
class MemoryChannel final : public net::Channel
{
  public:
    MemoryChannel *peer = nullptr;
    net::ChannelState current = net::ChannelState::Idle;
    std::vector<std::uint8_t> bytes;
    std::string message;
    bool listen(std::uint16_t, const std::string &) override
    {
        current = net::ChannelState::Listening;
        return true;
    }
    bool connect(const net::Endpoint &) override
    {
        current = peer->current = net::ChannelState::Connected;
        return true;
    }
    void poll() override
    {
    }
    bool send(const net::Packet &packet) override
    {
        const auto encoded = net::encodePacket(packet);
        if (encoded.empty())
            return false;
        peer->bytes.insert(peer->bytes.end(), encoded.begin(), encoded.end());
        return true;
    }
    bool receive(net::Packet &packet) override
    {
        bool available = false;
        if (!net::decodePacket(bytes, packet, available))
        {
            current = net::ChannelState::Failed;
            message = "bad wire";
        }
        return available;
    }
    void close() override
    {
        current = net::ChannelState::Idle;
        bytes.clear();
    }
    net::ChannelState state() const override
    {
        return current;
    }
    const std::string &error() const override
    {
        return message;
    }
    std::uint16_t port() const override
    {
        return net::kLanPort;
    }
};
struct Pair
{
    MemoryChannel a, b;
    app::LanSession host{a, "same-build"}, guest{b, "same-build"};
    double now = 0;
    Pair()
    {
        a.peer = &b;
        b.peer = &a;
    }
    void start()
    {
        net::RoomSettings room;
        room.seed = 731;
        room.stage = 9;
        room.lives = 4;
        require(host.host(room, 0), "host starts");
        require(guest.join({"127.0.0.1", net::kLanPort}, core::Nation::Germany, 0),
                "guest joins");
        for (int i = 0; i < 12; ++i)
        {
            now += 0.001;
            host.update(now);
            guest.update(now);
            if (host.takeStart())
                host.ready();
            if (guest.takeStart())
                guest.ready();
        }
        require(host.phase() == app::LanPhase::Playing &&
                    guest.phase() == app::LanPhase::Playing,
                "both enter play only after handshake");
        require(guest.room().seed == 731 && guest.room().stage == 9 &&
                    guest.room().lives == 4 &&
                    host.room().nations[1] == core::Nation::Germany,
                "host settings and guest nation agree");
    }
};
} // namespace
int main()
{
    reporter.beginSuite("lan-protocol-fragmentation-and-bounds");
    net::Packet source;
    source.kind = net::PacketKind::Welcome;
    source.room.seed = 0x01020304;
    source.room.advanced.enemySpeedPercent = -25;
    source.build = "same-build";
    const auto encoded = net::encodePacket(source);
    std::vector<std::uint8_t> bytes;
    net::Packet decoded;
    bool available = false;
    for (std::size_t i = 0; i < encoded.size(); ++i)
    {
        bytes.push_back(encoded[i]);
        require(net::decodePacket(bytes, decoded, available),
                "every TCP fragment accepted");
        require(available == (i + 1 == encoded.size()),
                "never commit an incomplete packet");
    }
    require(decoded.room.seed == source.room.seed &&
                decoded.room.advanced.enemySpeedPercent == -25 &&
                decoded.build == source.build,
            "settings round trip");
    for (auto kind :
         {net::PacketKind::Hello, net::PacketKind::Ready, net::PacketKind::Input,
          net::PacketKind::Tick, net::PacketKind::Digest, net::PacketKind::Leave,
          net::PacketKind::Heartbeat})
    {
        source.kind = kind;
        source.tick = 42;
        source.buttons = {{511, 321}};
        source.controls = 7;
        source.digest = 0xfedcba9876543210ULL;
        bytes = net::encodePacket(source);
        require(net::decodePacket(bytes, decoded, available) && available &&
                    bytes.empty() && decoded.kind == kind,
                "message round trip");
    }
    auto bad = encoded;
    bad[4] = 255;
    require(!net::decodePacket(bad, decoded, available), "wrong protocol rejected");
    bad = encoded;
    bad[6] = 255;
    require(!net::decodePacket(bad, decoded, available), "oversized frame rejected");
    bad = encoded;
    bad[12] = 0;
    require(!net::decodePacket(bad, decoded, available), "invalid stage rejected");
    for (unsigned bits = 0; bits < 512; ++bits)
        require(net::packInput(net::unpackInput(bits)) == bits,
                "input bits preserve direction edges and fire");
    reporter.beginSuite("lan-endpoint-validation");
    net::Endpoint endpoint;
    for (const char *text :
         {"127.0.0.1", "192.168.1.34:41987", "10.0.0.3:65535", "169.254.3.6"})
        require(net::parseEndpoint(text, endpoint), "valid endpoint");
    for (const char *text :
         {"", "localhost", "192.168.1", "192.168.1.2:0", "192.168.1.2:65536",
          "1.2.3.256", "1.2.3.4:1:2", "1.2.3.4x", "0.0.0.0", "255.255.255.255",
          "224.0.0.1", "01.2.3.4"})
        require(!net::parseEndpoint(text, endpoint), "invalid endpoint rejected");
    reporter.beginSuite("lan-ordered-input-authority-and-digests");
    Pair pair;
    pair.start();
    std::uint32_t compared = 0;
    bool guestEdge = false, hostEdge = false;
    for (int frame = 0; frame < 700; ++frame)
    {
        pair.now += 1.0 / 120;
        game::PlayerControlFrame p1, p2;
        p1.north.held = true;
        p2.east.held = true;
        p1.north.pressed = frame == 7;
        p2.east.pressed = frame == 9;
        p2.fireHeld = true;
        pair.host.update(pair.now, p1);
        pair.guest.update(pair.now, p2, frame == 20 ? net::Control::Restart : 0);
        net::Packet tick;
        while (pair.host.nextTick(tick))
        {
            if (tick.buttons[0] & 16)
            {
                require(!hostEdge, "host edge applied once");
                hostEdge = true;
            }
            require((tick.controls & net::Control::Restart) == 0,
                    "guest cannot restart the room");
            if (tick.tick % net::kDigestInterval == 0)
                pair.host.recordDigest(tick.tick * 73ULL);
        }
        pair.host.flush();
        p2.east.pressed = false;
        pair.guest.update(pair.now, p2);
        while (pair.guest.nextTick(tick))
        {
            require(tick.tick == ++compared, "guest receives every tick in order");
            if (tick.buttons[1] & 128)
            {
                require(!guestEdge, "guest edge applied once");
                guestEdge = true;
            }
            if (tick.tick % net::kDigestInterval == 0)
                pair.guest.recordDigest(tick.tick * 73ULL);
        }
    }
    require(compared > 300 && hostEdge && guestEdge,
            "sustained session and short tap edges survive render cadence");
    require(pair.host.phase() == app::LanPhase::Playing &&
                pair.guest.phase() == app::LanPhase::Playing,
            "matching digests keep session active");
    reporter.beginSuite("lan-lag-timeout-and-quit");
    auto before = pair.host.tick();
    for (int i = 0; i < 120; ++i)
    {
        pair.now += 1.0 / 60;
        pair.host.update(pair.now);
        net::Packet tick;
        while (pair.host.nextTick(tick))
        {
        }
    }
    require(pair.host.waitingForPeer() && pair.host.tick() - before <= 30,
            "host freezes instead of running indefinitely without peer");
    pair.host.update(pair.now + 9);
    require(pair.host.phase() == app::LanPhase::Failed, "silent peer times out");
    Pair quitting;
    quitting.start();
    quitting.guest.stop();
    quitting.host.update(.02);
    require(quitting.host.phase() == app::LanPhase::Failed &&
                !quitting.host.error().empty(),
            "explicit quit is reported");
    reporter.beginSuite("lan-version-sequence-and-desync-rejection");
    Pair mismatch;
    app::LanSession other(mismatch.b, "different-build");
    mismatch.host.host({}, 0);
    other.join({"127.0.0.1", net::kLanPort}, core::Nation::Germany, 0);
    other.update(.001);
    mismatch.host.update(.002);
    require(mismatch.host.phase() == app::LanPhase::Failed &&
                mismatch.host.error().find("Different") != std::string::npos,
            "wrong executable rejected before play");
    Pair sequence;
    sequence.start();
    net::Packet wrong;
    wrong.kind = net::PacketKind::Tick;
    wrong.tick = 8;
    sequence.a.send(wrong);
    sequence.guest.update(.02);
    require(sequence.guest.phase() == app::LanPhase::Failed,
            "skipped tick fails closed");
    Pair desync;
    desync.start();
    for (int i = 0; i < 260 && desync.guest.phase() == app::LanPhase::Playing; ++i)
    {
        desync.now += 1.0 / 60;
        desync.host.update(desync.now);
        desync.guest.update(desync.now);
        net::Packet tick;
        while (desync.host.nextTick(tick))
            if (tick.tick % 120 == 0)
                desync.host.recordDigest(1);
        desync.guest.update(desync.now);
        while (desync.guest.nextTick(tick))
            if (tick.tick % 120 == 0)
                desync.guest.recordDigest(2);
    }
    require(desync.guest.phase() == app::LanPhase::Failed ||
                desync.host.phase() == app::LanPhase::Failed,
            "different game states stop the room");
    reporter.finish();
    return 0;
}
