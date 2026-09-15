// Exercise the production simulation and LAN driver in two independent OS
// processes over real loopback TCP. No window, audio device or public port.
#define main tanks3dApplicationMain
#include "../src/main.cpp"
#undef main

#include <csignal>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

namespace
{
constexpr std::uint32_t kTestTicks = 1200;
struct Result
{
    std::uint64_t digest = 0;
    int ticks = 0, shots = 0, brickHits = 0, activeEnemies = 0, pauseChanges = 0,
        restarts = 0;
    float distance[2]{};
};

double monotonicSeconds()
{
    return std::chrono::duration<double>(
               std::chrono::steady_clock::now().time_since_epoch())
               .count() *
           4.0;
}

bool runRole(tanks3d::app::LanSession &session, const fs::path &resources,
             Result &result)
{
    Game3D game(resources, 0U);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(20);
    bool paused = false, resumed = false, restarted = false;
    std::array<XZ, 2> previous{};
    while (std::chrono::steady_clock::now() < deadline)
    {
        PlayerInputFrame inputs;
        auto &input = inputs.players[0];
        // Fire away from the headquarters; an eastward shot at spawn height
        // legitimately destroys the friendly base and makes restart ineligible.
        const bool turn = session.tick() >= 480 && session.tick() < 510;
        input.north.held = !turn;
        input.west.held = turn && session.isHost();
        input.east.held = turn && !session.isHost();
        input.fireHeld = true;
        std::uint8_t controls = 0;
        if (session.isHost())
        {
            if (session.tick() >= 300 && !paused)
            {
                controls = tanks3d::net::Control::Pause;
                paused = true;
            }
            if (session.tick() >= 360 && !resumed)
            {
                controls = tanks3d::net::Control::Pause;
                resumed = true;
            }
            if (session.tick() >= 650 && !restarted)
            {
                controls = tanks3d::net::Control::Restart;
                restarted = true;
            }
        }
        session.update(monotonicSeconds(), input, controls);
        if (auto room = session.takeStart())
        {
            game = Game3D(resources, room->seed);
            if (!game.start(2, room->lives, room->stage, room->nations, room->advanced))
                return false;
            for (int i = 0; i < 2; ++i)
                previous[i] = game.players()[i].position;
            session.ready();
        }
        tanks3d::net::Packet packet;
        while (session.tick() < kTestTicks && session.nextTick(packet))
        {
            const bool wasPaused = game.paused();
            if (tanks3d::app::applyLanTick(game, packet) !=
                tanks3d::app::LanTickResult::Continue)
            {
                std::cerr << "Unexpected room end at tick " << packet.tick
                          << " role=" << session.localPlayer() << "\n";
                return false;
            }
            if (game.paused() != wasPaused)
                ++result.pauseChanges;
            if (packet.controls & tanks3d::net::Control::Restart)
            {
                if (!game.stageIntro())
                {
                    std::cerr << "Restart did not enter stage intro: game over="
                              << game.gameOver() << " tick=" << packet.tick << "\n";
                    return false;
                }
                ++result.restarts;
            }
            if (packet.tick % tanks3d::net::kDigestInterval == 0)
                session.recordDigest(
                    tanks3d::net::stateHash(game.sessionDigest().state));
            for (const auto &event : game.eventsThisUpdate())
            {
                if (event.type == GameEventType::ShellFired)
                    ++result.shots;
                if (event.type == GameEventType::BrickHit)
                    ++result.brickHits;
            }
            result.activeEnemies =
                std::max(result.activeEnemies, static_cast<int>(game.enemies().size()));
            for (int i = 0; i < 2; ++i)
            {
                const XZ position = game.players()[i].position;
                if (!(packet.controls & tanks3d::net::Control::Restart))
                    result.distance[i] +=
                        std::sqrt(distanceSquared(position, previous[i]));
                previous[i] = position;
            }
        }
        session.flush();
        if (session.tick() == kTestTicks)
        {
            result.digest = tanks3d::net::stateHash(game.sessionDigest().state);
            result.ticks = static_cast<int>(session.tick());
            return true;
        }
        if (session.phase() == tanks3d::app::LanPhase::Failed)
        {
            std::cerr << session.error() << '\n';
            return false;
        }
        std::this_thread::sleep_for(std::chrono::microseconds(250));
    }
    std::cerr << "LAN game test deadline exceeded\n";
    return false;
}
} // namespace

int main(int argc, char **argv)
{
    const fs::path resources = argc > 1 ? argv[1] : "resources";
    const auto fingerprint = tanks3d::net::executableFingerprint();
    if (fingerprint.empty())
        return 1;
    tanks3d::net::TcpChannel channel;
    // Bind loopback before creating the session: the wrapper below prevents a
    // test host from ever listening on other interfaces.
    class LoopbackChannel final : public tanks3d::net::Channel
    {
      public:
        tanks3d::net::TcpChannel tcp;
        bool listen(std::uint16_t port, const std::string &) override
        {
            return tcp.listen(port, "127.0.0.1");
        }
        bool connect(const tanks3d::net::Endpoint &e) override
        {
            return tcp.connect(e);
        }
        void poll() override
        {
            tcp.poll();
        }
        bool send(const tanks3d::net::Packet &p) override
        {
            return tcp.send(p);
        }
        bool receive(tanks3d::net::Packet &p) override
        {
            return tcp.receive(p);
        }
        void close() override
        {
            tcp.close();
        }
        tanks3d::net::ChannelState state() const override
        {
            return tcp.state();
        }
        const std::string &error() const override
        {
            return tcp.error();
        }
        std::uint16_t port() const override
        {
            return tcp.port();
        }
    } hostChannel;
    tanks3d::app::LanSession host(hostChannel, fingerprint);
    tanks3d::net::RoomSettings room;
    room.seed = 90125;
    room.lives = 10;
    if (!host.host(room, monotonicSeconds(), 0))
    {
        std::cerr << host.error() << '\n';
        return 2;
    }
    const auto port = hostChannel.port();
    int descriptors[2];
    if (pipe(descriptors) < 0)
        return 3;
    const pid_t child = fork();
    if (child < 0)
        return 4;
    if (child == 0)
    {
        ::close(descriptors[0]);
        host.stop();
        tanks3d::net::TcpChannel guestChannel;
        tanks3d::app::LanSession guest(guestChannel, fingerprint);
        Result result;
        const bool success =
            guest.join({"127.0.0.1", port}, Nation::Germany, monotonicSeconds()) &&
            runRole(guest, resources, result);
        if (success)
            ::write(descriptors[1], &result, sizeof(result));
        // The final TCP digest has already been flushed; keep the connection
        // open briefly so the host can process it before the test child exits.
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        ::close(descriptors[1]);
        _exit(success ? 0 : 5);
    }
    ::close(descriptors[1]);
    Result local, remote;
    const bool success = runRole(host, resources, local);
    const auto settleDeadline =
        std::chrono::steady_clock::now() + std::chrono::milliseconds(80);
    while (std::chrono::steady_clock::now() < settleDeadline)
    {
        host.update(monotonicSeconds());
        host.flush();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    if (!success)
        kill(child, SIGTERM);
    int status = 0;
    waitpid(child, &status, 0);
    const auto count = ::read(descriptors[0], &remote, sizeof(remote));
    ::close(descriptors[0]);
    if (!success || !WIFEXITED(status) || WEXITSTATUS(status) != 0 ||
        count != sizeof(remote))
    {
        std::cerr << "Process test failure: host=" << success
                  << " child status=" << status << " bytes=" << count << "\n";
        return 6;
    }
    if (local.digest != remote.digest || local.ticks != kTestTicks ||
        remote.ticks != kTestTicks || local.shots < 10 || local.brickHits < 1 ||
        local.activeEnemies < 1 || local.distance[0] < 2 || local.distance[1] < 2 ||
        local.pauseChanges != 2 || remote.pauseChanges != 2 || local.restarts != 1 ||
        remote.restarts != 1)
    {
        std::cerr << "Outcome mismatch: ticks=" << local.ticks << "," << remote.ticks
                  << " digest=" << local.digest << "," << remote.digest
                  << " pauses=" << local.pauseChanges << "," << remote.pauseChanges
                  << " restarts=" << local.restarts << "," << remote.restarts
                  << " shots=" << local.shots << " bricks=" << local.brickHits
                  << " enemies=" << local.activeEnemies
                  << " distance=" << local.distance[0] << "," << local.distance[1]
                  << "\n";
        return 7;
    }
    std::cout << "PASS two processes / real loopback TCP / " << local.ticks
              << " identical gameplay ticks\n"
              << "digest=" << std::hex << local.digest << std::dec
              << " shots=" << local.shots << " brick hits=" << local.brickHits
              << " max enemies=" << local.activeEnemies
              << " distances=" << local.distance[0] << "," << local.distance[1] << "\n"
              << "PASS shared pause, resume, restart, both-player input and periodic "
                 "full-state checks\n";
    return 0;
}
