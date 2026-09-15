#ifndef TANKS3D_NET_LAN_CHANNEL_H
#define TANKS3D_NET_LAN_CHANNEL_H
#include "net/lan_protocol.h"
#include <deque>

namespace tanks3d::net
{
enum class ChannelState
{
    Idle,
    Listening,
    Connecting,
    Connected,
    Failed
};

class Channel
{
  public:
    virtual ~Channel() = default;
    virtual bool listen(std::uint16_t port, const std::string &address = "0.0.0.0") = 0;
    virtual bool connect(const Endpoint &endpoint) = 0;
    virtual void poll() = 0;
    virtual bool send(const Packet &packet) = 0;
    virtual bool receive(Packet &packet) = 0;
    virtual void close() = 0;
    virtual ChannelState state() const = 0;
    virtual const std::string &error() const = 0;
    virtual std::uint16_t port() const = 0;
};

class TcpChannel final : public Channel
{
  public:
    ~TcpChannel() override;
    TcpChannel() = default;
    TcpChannel(const TcpChannel &) = delete;
    TcpChannel &operator=(const TcpChannel &) = delete;
    bool listen(std::uint16_t port, const std::string &address = "0.0.0.0") override;
    bool connect(const Endpoint &endpoint) override;
    void poll() override;
    bool send(const Packet &packet) override;
    bool receive(Packet &packet) override;
    void close() override;
    ChannelState state() const override
    {
        return state_;
    }
    const std::string &error() const override
    {
        return error_;
    }
    std::uint16_t port() const override
    {
        return port_;
    }

  private:
    bool configure(int socket);
    void fail(const std::string &message);
    int listener_ = -1;
    int socket_ = -1;
    ChannelState state_ = ChannelState::Idle;
    std::uint16_t port_ = 0;
    std::string error_;
    std::vector<std::uint8_t> incoming_, outgoing_;
    std::deque<Packet> packets_;
};

std::vector<std::string> localIpv4Addresses();
// Compatibility identity only, not peer authentication. Requires the same executable.
std::string executableFingerprint();
} // namespace tanks3d::net
#endif
