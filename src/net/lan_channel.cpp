#include "net/lan_channel.h"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <sstream>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif
#include <arpa/inet.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

namespace tanks3d::net
{
namespace
{
bool wouldBlock()
{
    return errno == EAGAIN || errno == EWOULDBLOCK;
}
void closeSocket(int &socket)
{
    if (socket >= 0)
        ::close(socket);
    socket = -1;
}
} // namespace

TcpChannel::~TcpChannel()
{
    close();
}
void TcpChannel::close()
{
    closeSocket(socket_);
    closeSocket(listener_);
    state_ = ChannelState::Idle;
    port_ = 0;
    incoming_.clear();
    outgoing_.clear();
    packets_.clear();
    error_.clear();
}
void TcpChannel::fail(const std::string &message)
{
    closeSocket(socket_);
    closeSocket(listener_);
    state_ = ChannelState::Failed;
    error_ = message;
    incoming_.clear();
    outgoing_.clear();
}
bool TcpChannel::configure(int socket)
{
    if (socket < 0 || fcntl(socket, F_SETFL, O_NONBLOCK) < 0 ||
        fcntl(socket, F_SETFD, FD_CLOEXEC) < 0)
        return false;
    const int enabled = 1;
    if (setsockopt(socket, IPPROTO_TCP, TCP_NODELAY, &enabled, sizeof(enabled)) < 0)
        return false;
#ifdef SO_NOSIGPIPE
    if (setsockopt(socket, SOL_SOCKET, SO_NOSIGPIPE, &enabled, sizeof(enabled)) < 0)
        return false;
#endif
    return true;
}

bool TcpChannel::listen(std::uint16_t port, const std::string &address)
{
    close();
    sockaddr_in endpoint{};
    endpoint.sin_family = AF_INET;
    endpoint.sin_port = htons(port);
    if (inet_pton(AF_INET, address.c_str(), &endpoint.sin_addr) != 1)
    {
        fail("Invalid listening address.");
        return false;
    }
    listener_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (!configure(listener_))
    {
        fail("Cannot create the LAN socket.");
        return false;
    }
    const int enabled = 1;
    setsockopt(listener_, SOL_SOCKET, SO_REUSEADDR, &enabled, sizeof(enabled));
    if (::bind(listener_, reinterpret_cast<sockaddr *>(&endpoint), sizeof(endpoint)) <
            0 ||
        ::listen(listener_, 1) < 0)
    {
        fail(std::string("Cannot host: ") + std::strerror(errno));
        return false;
    }
    socklen_t size = sizeof(endpoint);
    if (getsockname(listener_, reinterpret_cast<sockaddr *>(&endpoint), &size) < 0)
    {
        fail("Cannot read the room port.");
        return false;
    }
    port_ = ntohs(endpoint.sin_port);
    state_ = ChannelState::Listening;
    return true;
}

bool TcpChannel::connect(const Endpoint &endpoint)
{
    close();
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(endpoint.port);
    Endpoint validated;
    if (!parseEndpoint(endpoint.address + ":" + std::to_string(endpoint.port),
                       validated) ||
        inet_pton(AF_INET, endpoint.address.c_str(), &address.sin_addr) != 1)
    {
        fail("Enter an IPv4 address, for example 192.168.1.20.");
        return false;
    }
    socket_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (!configure(socket_))
    {
        fail("Cannot create the LAN socket.");
        return false;
    }
    const int result =
        ::connect(socket_, reinterpret_cast<sockaddr *>(&address), sizeof(address));
    if (result < 0 && errno != EINPROGRESS)
    {
        fail(std::string("Cannot join: ") + std::strerror(errno));
        return false;
    }
    port_ = endpoint.port;
    state_ = result == 0 ? ChannelState::Connected : ChannelState::Connecting;
    return true;
}

void TcpChannel::poll()
{
    if (state_ == ChannelState::Listening)
    {
        socket_ = ::accept(listener_, nullptr, nullptr);
        if (socket_ < 0)
        {
            if (!wouldBlock() && errno != EINTR)
                fail("Unable to accept the joining player.");
            return;
        }
        if (!configure(socket_))
        {
            fail("Unable to configure the joining connection.");
            return;
        }
        closeSocket(listener_);
        state_ = ChannelState::Connected;
    }
    if (state_ == ChannelState::Connecting)
    {
        pollfd descriptor{socket_, POLLOUT, 0};
        const int result = ::poll(&descriptor, 1, 0);
        if (result < 0 && errno != EINTR)
        {
            fail("Connection check failed.");
            return;
        }
        if (result <= 0)
            return;
        int error = 0;
        socklen_t size = sizeof(error);
        if (getsockopt(socket_, SOL_SOCKET, SO_ERROR, &error, &size) < 0 || error != 0)
        {
            fail("Cannot reach the room. Check the IP, firewall and Local Network "
                 "access.");
            return;
        }
        state_ = ChannelState::Connected;
    }
    if (state_ != ChannelState::Connected)
        return;
    for (int budget = 0; budget < 16 && !outgoing_.empty(); ++budget)
    {
#ifdef MSG_NOSIGNAL
        constexpr int flags = MSG_NOSIGNAL;
#else
        constexpr int flags = 0;
#endif
        const auto sent = ::send(socket_, outgoing_.data(), outgoing_.size(), flags);
        if (sent < 0)
        {
            if (wouldBlock() || errno == EINTR)
                break;
            fail("The other player disconnected.");
            return;
        }
        if (sent == 0)
            break;
        outgoing_.erase(outgoing_.begin(), outgoing_.begin() + sent);
    }
    bool closed = false;
    for (int budget = 0; budget < 16; ++budget)
    {
        std::array<std::uint8_t, 4096> bytes{};
        const auto count = ::recv(socket_, bytes.data(), bytes.size(), 0);
        if (count == 0)
        {
            closed = true;
            break;
        }
        if (count < 0)
        {
            if (wouldBlock() || errno == EINTR)
                break;
            closed = true;
            break;
        }
        if (incoming_.size() + count > kMaximumBufferedBytes)
        {
            fail("The peer sent too much data.");
            return;
        }
        incoming_.insert(incoming_.end(), bytes.begin(), bytes.begin() + count);
        for (;;)
        {
            Packet packet;
            bool available = false;
            if (!decodePacket(incoming_, packet, available))
            {
                fail("Invalid LAN protocol data.");
                return;
            }
            if (!available)
                break;
            if (packets_.size() >= 512)
            {
                fail("The peer sent too many packets.");
                return;
            }
            packets_.push_back(std::move(packet));
        }
    }
    if (closed)
        fail("The other player disconnected.");
}

bool TcpChannel::send(const Packet &packet)
{
    if (state_ != ChannelState::Connected)
        return false;
    const auto bytes = encodePacket(packet);
    if (bytes.empty() || outgoing_.size() + bytes.size() > kMaximumBufferedBytes)
    {
        fail("LAN send buffer exceeded its limit.");
        return false;
    }
    outgoing_.insert(outgoing_.end(), bytes.begin(), bytes.end());
    return true;
}
bool TcpChannel::receive(Packet &packet)
{
    if (packets_.empty())
        return false;
    packet = std::move(packets_.front());
    packets_.pop_front();
    return true;
}

std::string executableFingerprint()
{
#ifdef __APPLE__
    std::uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);
    if (size == 0 || size > 65536)
        return {};
    std::vector<char> path(size);
    if (_NSGetExecutablePath(path.data(), &size) != 0)
        return {};
    std::ifstream input(path.data(), std::ios::binary);
#else
    std::ifstream input("/proc/self/exe", std::ios::binary);
#endif
    if (!input)
        return {};
    std::uint64_t hash = 14695981039346656037ULL, length = 0;
    std::array<char, 16384> buffer{};
    while (input.read(buffer.data(), buffer.size()) || input.gcount() > 0)
    {
        for (std::streamsize i = 0; i < input.gcount(); ++i)
        {
            hash ^= static_cast<unsigned char>(buffer[i]);
            hash *= 1099511628211ULL;
        }
        length += static_cast<std::uint64_t>(input.gcount());
    }
    if (!input.eof() || length == 0)
        return {};
    std::ostringstream result;
    result << "tanks3d-lan-v1/" << std::hex << hash << '/' << length;
    return result.str();
}

std::vector<std::string> localIpv4Addresses()
{
    std::vector<std::string> result;
    ifaddrs *addresses = nullptr;
    if (getifaddrs(&addresses) != 0)
        return result;
    for (const auto *it = addresses; it; it = it->ifa_next)
    {
        if (!it->ifa_addr || it->ifa_addr->sa_family != AF_INET ||
            !(it->ifa_flags & IFF_UP) || (it->ifa_flags & IFF_LOOPBACK))
            continue;
        const auto *address = reinterpret_cast<const sockaddr_in *>(it->ifa_addr);
        char text[INET_ADDRSTRLEN]{};
        if (inet_ntop(AF_INET, &address->sin_addr, text, sizeof(text)) &&
            std::find(result.begin(), result.end(), text) == result.end())
            result.emplace_back(text);
    }
    freeifaddrs(addresses);
    return result;
}
} // namespace tanks3d::net
