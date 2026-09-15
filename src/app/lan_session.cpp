#include "app/lan_session.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace tanks3d::app
{
using namespace net;
namespace
{
std::uint16_t latch(std::uint16_t previous, std::uint16_t next)
{
    return next | (previous & 0xf0U);
}
} // namespace
LanSession::LanSession(Channel &channel, std::string build)
    : channel_(channel), build_(std::move(build))
{
}

void LanSession::reset(double now)
{
    channel_.close();
    phase_ = LanPhase::Idle;
    error_.clear();
    connected_ = startPending_ = localReady_ = remoteReady_ = false;
    now_ = lastUpdate_ = lastReceive_ = lastInput_ = lastSendInput_ = lastHeartbeat_ =
        started_ = now;
    accumulator_ = 0;
    tick_ = receivedTick_ = peerAck_ = 0;
    localButtons_ = remoteButtons_ = 0;
    localControls_ = remoteControls_ = 0;
    ticks_.clear();
    localDigests_.clear();
    remoteDigests_.clear();
}
bool LanSession::host(const RoomSettings &room, double now, std::uint16_t port)
{
    reset(now);
    host_ = true;
    room_ = room;
    if (!validRoom(room) || build_.empty() || build_.size() > 128)
    {
        fail("Invalid room configuration.");
        return false;
    }
    if (!channel_.listen(port))
    {
        fail(channel_.error());
        return false;
    }
    phase_ = LanPhase::Waiting;
    return true;
}
bool LanSession::join(const Endpoint &endpoint, core::Nation nation, double now)
{
    reset(now);
    host_ = false;
    nation_ = nation;
    if (static_cast<unsigned>(nation) >= 3 || build_.empty() || build_.size() > 128)
    {
        fail("Invalid player configuration.");
        return false;
    }
    if (!channel_.connect(endpoint))
    {
        fail(channel_.error());
        return false;
    }
    phase_ = LanPhase::Connecting;
    return true;
}
void LanSession::stop()
{
    if (channel_.state() == ChannelState::Connected)
    {
        Packet packet;
        packet.kind = PacketKind::Leave;
        channel_.send(packet);
        channel_.poll();
    }
    reset(now_);
}
void LanSession::fail(const std::string &message, LeaveReason reason)
{
    // Own the text before closing a channel that may own the supplied string.
    const std::string owned = message;
    if (channel_.state() == ChannelState::Connected)
    {
        Packet packet;
        packet.kind = PacketKind::Leave;
        packet.reason = reason;
        channel_.send(packet);
        channel_.poll();
    }
    channel_.close();
    phase_ = LanPhase::Failed;
    error_ = owned;
    ticks_.clear();
    startPending_ = false;
}
void LanSession::abort(LeaveReason reason)
{
    fail(leaveMessage(reason), reason);
}

void LanSession::consume(const Packet &packet, double now)
{
    lastReceive_ = now;
    switch (packet.kind)
    {
    case PacketKind::Hello:
        if (!host_ || phase_ != LanPhase::Handshake)
            break;
        if (packet.build != build_)
        {
            abort(LeaveReason::Incompatible);
            return;
        }
        room_.nations[1] = packet.nation;
        phase_ = LanPhase::Starting;
        startPending_ = true;
        {
            Packet welcome;
            welcome.kind = PacketKind::Welcome;
            welcome.room = room_;
            welcome.build = build_;
            channel_.send(welcome);
        }
        return;
    case PacketKind::Welcome:
        if (host_ || phase_ != LanPhase::Handshake)
            break;
        if (packet.build != build_)
        {
            abort(LeaveReason::Incompatible);
            return;
        }
        room_ = packet.room;
        startPending_ = true;
        phase_ = LanPhase::Starting;
        return;
    case PacketKind::Ready:
        if (!host_ || remoteReady_ || phase_ != LanPhase::Starting)
            break;
        remoteReady_ = true;
        lastInput_ = now;
        if (localReady_)
        {
            phase_ = LanPhase::Playing;
            accumulator_ = 0;
        }
        return;
    case PacketKind::Input:
        if (!host_ || !remoteReady_ ||
            (phase_ != LanPhase::Playing && phase_ != LanPhase::Starting) ||
            packet.tick > tick_ || packet.tick < peerAck_)
            break;
        peerAck_ = packet.tick;
        lastInput_ = now;
        remoteButtons_ = latch(remoteButtons_, packet.buttons[0]);
        // Restart is deliberately controlled by the host only.
        remoteControls_ |= packet.controls & (Control::Pause | Control::Confirm);
        return;
    case PacketKind::Tick:
        if (host_ || phase_ != LanPhase::Playing || ticks_.size() >= 120 ||
            receivedTick_ == std::numeric_limits<std::uint32_t>::max() ||
            packet.tick != receivedTick_ + 1)
            break;
        receivedTick_ = packet.tick;
        ticks_.push_back(packet);
        return;
    case PacketKind::Digest:
        if (phase_ != LanPhase::Playing || packet.tick == 0 ||
            packet.tick % kDigestInterval != 0 ||
            packet.tick > (host_ ? tick_ : receivedTick_) ||
            remoteDigests_.count(packet.tick) || remoteDigests_.size() >= 8)
            break;
        remoteDigests_[packet.tick] = packet.digest;
        checkDigest(packet.tick);
        return;
    case PacketKind::Heartbeat:
        return;
    case PacketKind::Leave:
        fail(leaveMessage(packet.reason), packet.reason);
        return;
    }
    fail("Unexpected LAN message. The room has stopped.");
}

void LanSession::update(double now, const game::PlayerControlFrame &input,
                        std::uint8_t controls)
{
    if (phase_ == LanPhase::Idle || phase_ == LanPhase::Failed)
        return;
    if (!std::isfinite(now) || now < lastUpdate_)
    {
        fail("The LAN clock is invalid.");
        return;
    }
    now_ = now;
    if (phase_ == LanPhase::Playing)
    {
        accumulator_ += std::min(now - lastUpdate_, 0.1);
        localButtons_ = latch(localButtons_, packInput(input));
        localControls_ |= controls & 7U;
    }
    lastUpdate_ = now;
    channel_.poll();
    if (!connected_ && channel_.state() == ChannelState::Connected)
    {
        connected_ = true;
        started_ = lastReceive_ = now;
        phase_ = LanPhase::Handshake;
        if (!host_)
        {
            Packet hello;
            hello.kind = PacketKind::Hello;
            hello.build = build_;
            hello.nation = nation_;
            channel_.send(hello);
        }
    }
    Packet packet;
    while (phase_ != LanPhase::Failed && channel_.receive(packet))
        consume(packet, now);
    if (phase_ == LanPhase::Failed)
        return;
    if (channel_.state() == ChannelState::Failed)
    {
        fail(channel_.error());
        return;
    }
    if ((connected_ && now - lastReceive_ > 8) ||
        (!connected_ && !host_ && now - started_ > 10) ||
        (connected_ && phase_ != LanPhase::Playing && now - started_ > 15))
    {
        fail("Connection timed out. Check the network and create a new room.");
        return;
    }
    if (connected_ && now - lastHeartbeat_ >= 0.5)
    {
        channel_.send(Packet{});
        lastHeartbeat_ = now;
    }
    if (phase_ == LanPhase::Playing && !host_ && now - lastSendInput_ >= 1.0 / 60.0)
    {
        Packet message;
        message.kind = PacketKind::Input;
        message.tick = tick_;
        message.buttons[0] = localButtons_;
        message.controls = localControls_;
        channel_.send(message);
        localButtons_ &= ~0xf0U;
        localControls_ = 0;
        lastSendInput_ = now;
    }
}

std::optional<RoomSettings> LanSession::takeStart()
{
    if (!startPending_)
        return std::nullopt;
    startPending_ = false;
    return room_;
}
void LanSession::ready()
{
    if (phase_ != LanPhase::Starting || localReady_)
        return;
    localReady_ = true;
    if (!host_)
    {
        Packet packet;
        packet.kind = PacketKind::Ready;
        channel_.send(packet);
        phase_ = LanPhase::Playing;
        accumulator_ = 0;
    }
    else if (remoteReady_)
    {
        phase_ = LanPhase::Playing;
        accumulator_ = 0;
    }
}
bool LanSession::waitingForPeer() const
{
    return phase_ == LanPhase::Playing &&
           (host_ ? (tick_ - peerAck_ >= 30 || now_ - lastInput_ > 0.5)
                  : now_ - lastReceive_ > 0.5);
}
bool LanSession::nextTick(Packet &packet)
{
    if (phase_ != LanPhase::Playing)
        return false;
    if (host_)
    {
        if (waitingForPeer())
        {
            accumulator_ = 0;
            return false;
        }
        if (accumulator_ + 1e-9 < 1.0 / 60.0)
            return false;
        if (tick_ == std::numeric_limits<std::uint32_t>::max())
        {
            fail("The room tick limit was reached.");
            return false;
        }
        accumulator_ -= 1.0 / 60.0;
        Packet tick;
        tick.kind = PacketKind::Tick;
        tick.tick = ++tick_;
        tick.buttons = {{localButtons_, remoteButtons_}};
        tick.controls = localControls_ | remoteControls_;
        localButtons_ &= ~0xf0U;
        remoteButtons_ &= ~0xf0U;
        localControls_ = remoteControls_ = 0;
        if (!channel_.send(tick))
        {
            fail(channel_.error());
            return false;
        }
        packet = tick;
        return true;
    }
    if (ticks_.empty())
        return false;
    packet = ticks_.front();
    ticks_.pop_front();
    tick_ = packet.tick;
    return true;
}
void LanSession::checkDigest(std::uint32_t tick)
{
    const auto local = localDigests_.find(tick), remote = remoteDigests_.find(tick);
    if (local == localDigests_.end() || remote == remoteDigests_.end())
        return;
    if (local->second != remote->second)
    {
        abort(LeaveReason::Desynchronized);
        return;
    }
    localDigests_.erase(local);
    remoteDigests_.erase(remote);
}
void LanSession::recordDigest(std::uint64_t digest)
{
    if (phase_ != LanPhase::Playing || tick_ == 0 || tick_ % kDigestInterval != 0)
        return;
    if (localDigests_.size() >= 8 || localDigests_.count(tick_))
    {
        fail("The other player stopped verifying the game state.");
        return;
    }
    localDigests_[tick_] = digest;
    checkDigest(tick_);
    if (phase_ != LanPhase::Playing)
        return;
    Packet packet;
    packet.kind = PacketKind::Digest;
    packet.tick = tick_;
    packet.digest = digest;
    channel_.send(packet);
}
void LanSession::flush()
{
    channel_.poll();
}
} // namespace tanks3d::app
