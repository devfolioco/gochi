# tamagotchi

Firmware for an **ESP32-C3 SuperMini**, written in C++ with the Arduino
core and built/flashed entirely from the terminal.

## Toolchain

- [`arduino-cli`](https://arduino.github.io/arduino-cli/) — build, upload, monitor
- ESP32 Arduino core `3.3.8` (`esp32:esp32`)
- Editor: [Zed](https://zed.dev) — `.ino` files are treated as C++; clangd
  reads `firmware/build/compile_commands.json` for code intelligence

The `arduino-cli` config is committed to the repo (`firmware/arduino-cli.yaml`),
so `make` and the editor both use it via `--config-file` — no global setup
needed. To reproduce on another machine, install the tool and the ESP32 core:

```sh
brew install arduino-cli
arduino-cli --config-file firmware/arduino-cli.yaml core update-index
arduino-cli --config-file firmware/arduino-cli.yaml core install esp32:esp32
```

## Daily workflow

| Command             | What it does                                  |
| ------------------- | --------------------------------------------- |
| `make build`        | Compile the sketch                            |
| `make flash`        | Compile + upload to the board                 |
| `make erase`        | Wipe the entire flash (factory reset)         |
| `make monitor`      | Open the serial monitor (115200, Ctrl-C exits)|
| `make flash-monitor`| Flash, then open the monitor                  |
| `make db`           | Regenerate `compile_commands.json` for Zed    |
| `make format`       | Auto-format all sources (`clang-format`)      |
| `make format-check` | Check formatting without editing (CI-friendly)|
| `make ports`        | List connected boards                         |
| `make clean`        | Delete build artifacts                        |

### Hardware bring-up tests

Four standalone sketches under `firmware/tests/` for verifying each
peripheral on its own — useful when bringing up a fresh board or
chasing a hardware fault, and they don't need the `gochi` CLI / daemon
installed. Each one compiles + flashes in a single target:

| Command          | What it does                                                 |
| ---------------- | ------------------------------------------------------------ |
| `make test-led`  | Blinks `LED_BUILTIN` at 1 Hz                                 |
| `make test-oled` | Cycles four frames on the SSD1306                            |
| `make test-buzzer` | Plays a C5-major scale on the piezo                        |
| `make test-mpu`  | Streams MPU-6050 samples **and** opens a live browser viewer |

`make test-mpu` also opens `firmware/tests/mpu/visualize.html` — a
single-page Web Serial viewer (Chrome / Edge only) that draws a 3D
plane reacting to roll / pitch plus live numeric values for all six
axes. See [`firmware/tests/README.md`](firmware/tests/README.md) for
details.

## Build-time configuration (`.env`)

A few build-time knobs live in a user-local `.env` at the repo root.
The Makefile `-include`s it automatically and translates each
supported `KEY=VALUE` pair into a `-D<KEY>=<value>` compiler flag,
applied to **every** compile — main firmware and bring-up tests
alike. `.env` is gitignored; the committed template is `.env.example`.

```sh
cp .env.example .env       # then edit
make test-oled             # verify the new settings on the panel
make flash                 # main firmware picks them up too
```

| Variable           | Effect                                                              | Default |
| ------------------ | ------------------------------------------------------------------- | ------- |
| `ROTATED_DISPLAY`  | `=1` flips the OLED 180° (`U8G2_R2`) — for upside-down mounted panels | `0`     |

Format: plain `KEY=value`, one per line — no quotes, no `export`,
`#`-prefixed lines are comments. See [`.env.example`](.env.example).

## Linting & formatting

- **Formatting** — `clang-format`, configured in `.clang-format` (Google style,
  2-space, 100 cols). Zed formats on save via clangd; `make format` does the
  whole tree from the terminal.
- **Linting** — `clang-tidy`, configured in `.clang-tidy` (bug-finding checks).
  It runs *inside the editor*: clangd has clang-tidy built in, enabled by the
  `--clang-tidy` flag in `.zed/settings.json`. No separate binary needed. For a
  terminal lint pass, `brew install llvm` provides a standalone `clang-tidy`.

The board's USB port is auto-detected on macOS (`/dev/cu.usbmodem*`)
and Linux (`/dev/ttyACM*`). On Windows it isn't auto-detected — run
`make ports` (or Device Manager → **Ports (COM & LPT)**) to find the
COMx and pass it explicitly:

```sh
make flash PORT=/dev/cu.usbmodemXXXX    # macOS
make flash PORT=/dev/ttyACM0            # Linux / WSL
make flash PORT=COM7                    # Windows
```

## Board notes

- **USB CDC On Boot is enabled** in the FQBN (`CDCOnBoot=cdc`) so `Serial`
  works over the SuperMini's native USB — no separate UART adapter needed.
- Onboard blue LED is on **GPIO8** and is **active-LOW** (LOW = on).
- If an upload fails to start, force download mode: hold **BOOT**, tap
  **RESET**, release **BOOT**, then re-run `make flash`.
