# Tamagotchi CLI

A Node CLI for driving the [Tamagotchi firmware](../firmware/) over USB
serial, with an optional HTTP frontend for non-CLI clients.
TypeScript sources are loaded under Node via [`tsx`](https://tsx.is) —
`serialport`'s native module hits an unsupported libuv function under
Bun ([oven-sh/bun#18546](https://github.com/oven-sh/bun/issues/18546)),
so Node owns the runtime. (Bun is fine for `install`/`link`.)

The CLI is a thin client. The real work happens in two long-lived
pieces:

- **Daemon** — owns the USB serial port, listens on a Unix domain
  socket (`~/.tamagotchi/daemon.sock`). Required. The CLI talks to it
  directly.
- **HTTP frontend** — optional TCP listener on `:7474` that
  reverse-proxies to the daemon. Useful for `curl`, AI agents, web
  UIs, or anything that's easier with HTTP than a Unix socket.

`gochi setup` installs both as platform-native auto-start jobs that
come up at login:

| Platform | Service backend             |
| -------- | --------------------------- |
| macOS    | `launchctl` LaunchAgents    |
| Linux    | `systemctl --user` units    |
| Windows  | Task Scheduler logon tasks  |

**All command endpoints return HTTP 200** — even when the pet is
unplugged — so agents see a steady, calm API and never see error codes
for a missing device. The `connected` flag in the body signals state.

## Install

```sh
npm i -g @0xpv/gochi
gochi setup           # one-time: installs daemon + HTTP frontend
```

Confirm:

```sh
gochi daemon status   # daemon launchd + socket
gochi health          # daemon-reported device state
```

Local dev (no npm publish):

```sh
cd cli
bun install
bun link            # registers `gochi` globally
gochi setup
```

## CLI

```sh
gochi --version
gochi --help

# faces — name it, or omit to pick from an interactive menu
gochi face happy
gochi face                # opens a select with all 12 faces
gochi mood playful
gochi mood                # opens a select with all 5 moods

# text
gochi text hello there    # extra args are joined

# image — auto-resized to 128x64, dithered to 1-bit
gochi image ./logo.png
gochi image ./photo.jpg --no-dither -t 96   # plain threshold
gochi image ./icon.png --invert --bg white  # invert + white letterbox

# queries
gochi get state
gochi get fps
gochi list faces
gochi ping
gochi health

# interactive hardware self-test — picks a component from a menu,
# asks y/n after each, drops troubleshooting tips on 'no'.
gochi test               # menu: serial / OLED / buzzer / all
gochi test oled          # jump straight to one component
gochi test all           # run them all in order
```

Faces: `neutral happy sad sleepy excited surprised angry blink love horny shy dead`.
Moods: `content playful grumpy sleepy affectionate`.

The CLI talks to the daemon over `~/.tamagotchi/daemon.sock` by default.
Set `GOCHI_URL=http://host:port` to point it at a remote daemon's
HTTP frontend instead.

## Daemon

The daemon is the only process that holds the serial port. It's
hotplug-aware: it polls the OS port list and only opens devices that
advertise the Espressif USB VID (`303a`), so plugging in an unrelated
USB-serial board (an Arduino, a different ESP) won't get probed or
reset.

```sh
gochi daemon status   # plist + socket + connected device
gochi daemon run      # foreground (used by launchd; rare for users)
```

### Releasing the port temporarily

`gochi stop` tells the daemon to drop the serial port without
shutting itself down. Use it before any tool that needs exclusive
access to `/dev/cu.usbmodem*` — most commonly `arduino-cli upload`.

```sh
gochi stop            # release the port
# ...flash firmware, run a monitor, whatever...
gochi start           # daemon reconnects on the next ~1.5s tick
```

The firmware Makefile wraps `make flash` with this automatically, so
you don't normally type these by hand.

## HTTP frontend (optional)

Enabled by default after `setup`. Turn it off if you don't need a TCP
listener on your machine:

```sh
gochi server status    # is the HTTP frontend running?
gochi server disable   # turn it off (persists across reboots)
gochi server enable    # bring it back
gochi server run       # foreground (used by launchd)
```

Default port: **7474**. Override with `GOCHI_PORT`.

### HTTP API

All responses are JSON. `GET /health` is the only endpoint that lets
you know whether the pet is connected; **every command endpoint
returns 200 either way** — when offline, the response is
`{"ok": true, "connected": false, "message": "device offline; ..."}`.

| Method | Path     | Body                 | Sends to device |
| ------ | -------- | -------------------- | --------------- |
| GET    | /health  | —                    | (none)          |
| POST   | /face    | `{"name":"..."}`     | `SHOW face …`   |
| POST   | /text    | `{"text":"..."}`     | `SHOW text …`   |
| POST   | /image   | `{"data":"<b64>"}`   | `SHOW image …` (128×64 1bpp, MSB-first) |
| POST   | /mood    | `{"name":"..."}`     | `SET mood …`    |
| GET    | /state   | —                    | `GET state`     |
| GET    | /fps     | —                    | `GET fps`       |
| GET    | /faces   | —                    | `LIST faces`    |
| POST   | /ping    | —                    | `PING`          |

Quick check (HTTP frontend must be enabled):

```sh
curl http://localhost:7474/health
curl -X POST http://localhost:7474/face -H 'content-type: application/json' -d '{"name":"happy"}'
curl http://localhost:7474/state
```

## How it finds the pet

The daemon polls `SerialPort.list()` every ~1.5 s, filters to
Espressif's VID (`303a`), and opens any newly-arrived matching port.
Unplug the pet and the daemon notices on the next tick; plug it back in
and it reconnects within ~1.5 s. The daemon never opens non-Espressif
ports, so unrelated USB-serial devices stay untouched.

## Layout

```
cli/
  bin/gochi.js            Node wrapper — spawns `node tsx src/cli.ts ...`
  src/cli.ts              CLI dispatcher (commander)
  src/transport.ts        wraps a serial port with the pet's protocol
  src/discovery.ts        hotplug watcher (VID-filtered SerialPort.list polling)
  src/daemon.ts           the UDS daemon, owns the serial port
  src/server.ts           optional TCP HTTP reverse-proxy to the daemon
  src/ipc.ts              shared UDS paths + HTTP-over-UDS helpers
  src/service/            setup / enable / disable / status, per-platform
    index.ts                platform dispatcher
    common.ts               shared helpers (path resolution, health checks)
    darwin.ts               macOS launchd backend
    linux.ts                systemd --user backend
    windows.ts              Task Scheduler backend
  src/client.ts           CLI's transport (UDS by default, TCP if GOCHI_URL set)
  src/image.ts            PNG/JPG → 128×64 1bpp (dither + MSB-pack)
```

## Notes

- Auto-start is supported on macOS (launchd), Linux (`systemctl --user`),
  and Windows (Task Scheduler). On Linux, services stop when you log
  out unless you run `sudo loginctl enable-linger $USER` — `setup`
  prints a hint if lingering isn't on. If your environment can't host
  one of these (minimal container, WSL1, etc.), run `gochi daemon
  run` (and optionally `gochi server run`) in a terminal manually.
- The daemon is the only thing that holds the serial port — running
  `arduino-cli monitor` or the Arduino IDE at the same time will fight
  for it. Stop one of them.
- Mood lives in firmware RAM (resets to `content` on reboot).
- Upgrading from a pre-`setup` install: `gochi setup` will tear
  down the legacy `com.tamagotchi.server` plist for you.
