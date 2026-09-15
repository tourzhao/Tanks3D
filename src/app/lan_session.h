#ifndef TANKS3D_APP_LAN_SESSION_H
#define TANKS3D_APP_LAN_SESSION_H
#include "net/lan_channel.h"
#include <deque>
#include <map>
#include <optional>

namespace tanks3d::app
{
enum class LanPhase
{
    Idle,
    Waiting,
    Connecting,
    Handshake,
    Starting,
    Playing,
    Failed
};

// The host orders inputs at 60 Hz. Both machines apply the exact same packets
// to the existing seeded simulation. No socket or wall-clock access enters rules.
class LanSession
{
  public:
    LanSession(net::Channel &channel, std::string build);
    bool host(const net::RoomSettings &room, double now,
              std::uint16_t port = net::kLanPort);
    bool join(const net::Endpoint &endpoint, core::Nation nation, double now);
    void stop();
    void abort(net::LeaveReason reason);
    void update(double now, const game::PlayerControlFrame &input = {},
                std::uint8_t controls = 0);
    std::optional<net::RoomSettings> takeStart();
    void ready();
    bool nextTick(net::Packet &packet);
    void recordDigest(std::uint64_t digest);
    void flush();
    LanPhase phase() const
    {
        return phase_;
    }
    bool isHost() const
    {
        return host_;
    }
    int localPlayer() const
    {
        return host_ ? 0 : 1;
    }
    std::uint32_t tick() const
    {
        return tick_;
    }
    bool waitingForPeer() const;
    const std::string &error() const
    {
        return error_;
    }
    const net::RoomSettings &room() const
    {
        return room_;
    }

  private:
    void reset(double now);
    void fail(const std::string &message,
              net::LeaveReason reason = net::LeaveReason::Left);
    void consume(const net::Packet &packet, double now);
    void checkDigest(std::uint32_t tick);
    net::Channel &channel_;
    std::string build_, error_;
    LanPhase phase_ = LanPhase::Idle;
    net::RoomSettings room_{};
    core::Nation nation_ = core::Nation::UnitedStates;
    bool host_ = false, connected_ = false, startPending_ = false;
    bool localReady_ = false, remoteReady_ = false;
    double now_ = 0, lastUpdate_ = 0, lastReceive_ = 0;
    double lastInput_ = 0, lastSendInput_ = 0, lastHeartbeat_ = 0, started_ = 0;
    double accumulator_ = 0;
    std::uint32_t tick_ = 0, receivedTick_ = 0, peerAck_ = 0;
    std::uint16_t localButtons_ = 0, remoteButtons_ = 0;
    std::uint8_t localControls_ = 0, remoteControls_ = 0;
    std::deque<net::Packet> ticks_;
    std::map<std::uint32_t, std::uint64_t> localDigests_, remoteDigests_;
};
} // namespace tanks3d::app
#endif
