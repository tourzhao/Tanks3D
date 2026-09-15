# Local network co-op

Two computers can share the existing two-player defense game over IPv4 Wi-Fi
or Ethernet. The host controls P1 (gold); the joining computer controls P2
(green). Both see the shared co-op battlefield. This is available in the current
source build; it is not in the published Alpha 4 download.

## Start a room

1. Build with `make`, then copy the same `build/Tanks3D.app` to the other Mac.
   Both Macs must be able to run that build. Close older running game instances
   before opening the new app. Executable fingerprints must match: independently
   rebuilt, re-signed, or differently packaged executables can be rejected even
   when they have the same displayed version.
2. On each computer, select your own **P1 NATION** in the main setup menu.
   The host also chooses stage, lives and advanced gameplay settings.
3. On the host, open **LOCAL NETWORK → CREATE ROOM**. The room displays its
   address and TCP port (normally `41987`).
4. On the second computer, open **LOCAL NETWORK**, select **HOST IP**, and type
   the displayed IPv4 address. `Cmd+V` can paste an address. Press Enter to join.
   `192.168.1.20:41987` is also accepted. Play starts when both games are ready.

If macOS asks for Local Network access, allow it for Tanks 3D. The app includes
an `NSLocalNetworkUsageDescription` explaining this connection. This follows
[Apple's local network privacy guidance](https://developer.apple.com/documentation/technotes/tn3179-understanding-local-network-privacy).

On **either** computer, use Arrow keys or WASD and Space/F to control your own
tank. A connected controller also controls your own tank with its existing
stick/D-pad and fire bindings. Enter/Plus pauses or resumes the shared game;
either player can confirm a battle report. Only the host's R key restarts.
Esc/Minus leaves the room and informs the other player. Input becomes neutral
when the game window loses focus; networking continues in the background.

Camera angles and Pixel Style stay local to each computer. Nation choices,
starting stage, lives and advanced gameplay rules are shared. Pickups, damage,
upgrades, enemy selection, map layouts and progression retain the existing
co-op rules. The stage-skip development keys N/B are disabled during LAN play.

## Terminal launch

Use the same executable on both sides (do not mix an app-bundle executable and
a separately signed standalone build):

```sh
# Host
./build/Tanks3D --lan-host

# Guest: replace the example with the host's displayed address
./build/Tanks3D --lan-join=192.168.1.20
```

A different port can be supplied with `--lan-host=42000` and
`--lan-join=192.168.1.20:42000`. Stage and camera options still work, for example
`--stage=10 --lan-host`. LAN launch modes cannot be combined with quick-start,
release screenshot/performance capture, or showcase modes.

## Connection behavior and limits

- The first version supports exactly two computers and manual IPv4 joining.
  It has no discovery browser, online matchmaking, PvP or mid-game join.
- Use a trusted local network. The room has no account or password system;
  the executable fingerprint detects compatibility, not player identity.
- TCP uses nonblocking I/O and `TCP_NODELAY`. The host orders inputs at 60 Hz;
  both machines advance the existing seeded simulation with those exact frames.
  Short direction presses are latched until sent/applied. No client prediction
  is used, so Wi-Fi latency remains visible in control response.
- Every 120 ticks, both peers compare a hash of the full canonical gameplay
  digest, including map mutations, entities, settlement and random state.
  A mismatch stops the room instead of allowing two different battles to run.
- If client input stops for half a second or it falls 30 ticks behind, the host
  stops advancing until it catches up. A silent connection times out after
  eight seconds. An explicit leave or closed connection is reported immediately.
- After a disconnect, create/join a new room. Progress is not restored by a
  reconnect, and the host is not transferred to the other computer.
- If joining fails, check the displayed IP, the chosen port, macOS Local Network
  access and the firewall's app permission. Guest Wi-Fi/client isolation can
  prevent two devices on the same access point from connecting.

## Verification

```sh
make clean
make test
make test-sanitize
make test-lan-sockets
make test-lan-sockets-sanitize
```

`make test-lan` runs the socket-free framing, input, handshake, authority,
ordering, lag, timeout, leave, mismatch and desynchronization tests. It is
included in `make test`. The socket targets run the real production simulation
in two independent processes connected through an ephemeral loopback TCP port;
no public interface is opened by those tests. They compare 1,200 ticks with
movement, firing, terrain damage, pause/resume and a shared restart. The last
command instruments both the transport and simulation with ASan/UBSan.

Local evidence belongs under `build/release-evidence/lan-20260912/`. The native
review additionally runs two actual application main loops with hidden GPU
windows and injected keyboard states. This is useful same-machine validation;
a physical two-Mac Wi-Fi/Ethernet and controller session is still a separate
manual check.
