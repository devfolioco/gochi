# tamagotchi

Firmware for an **ESP32-C3 SuperMini**, written in C++ with the Arduino
core and built/flashed entirely from the terminal.

## Toolchain

- [`arduino-cli`](https://arduino.github.io/arduino-cli/) — build, upload, monitor
- ESP32 Arduino core `3.3.8` (`esp32:esp32`)
- Editor: [Zed](https://zed.dev) — `.ino` files are treated as C++; clangd
  reads `firmware/tamagotchi/build/compile_commands.json` for code intelligence

One-time setup is already done. To reproduce on another machine:

```sh
brew install arduino-cli
arduino-cli config init
arduino-cli config add board_manager.additional_urls \
  https://espressif.github.io/arduino-esp32/package_esp32_index.json
arduino-cli core update-index
arduino-cli core install esp32:esp32
```

## Daily workflow

| Command             | What it does                                  |
| ------------------- | --------------------------------------------- |
| `make build`        | Compile the sketch                            |
| `make flash`        | Compile + upload to the board                 |
| `make monitor`      | Open the serial monitor (115200, Ctrl-C exits)|
| `make flash-monitor`| Flash, then open the monitor                  |
| `make db`           | Regenerate `compile_commands.json` for Zed    |
| `make format`       | Auto-format all sources (`clang-format`)      |
| `make format-check` | Check formatting without editing (CI-friendly)|
| `make ports`        | List connected boards                         |
| `make clean`        | Delete build artifacts                        |

## Linting & formatting

- **Formatting** — `clang-format`, configured in `.clang-format` (Google style,
  2-space, 100 cols). Zed formats on save via clangd; `make format` does the
  whole tree from the terminal.
- **Linting** — `clang-tidy`, configured in `.clang-tidy` (bug-finding checks).
  It runs *inside the editor*: clangd has clang-tidy built in, enabled by the
  `--clang-tidy` flag in `.zed/settings.json`. No separate binary needed. For a
  terminal lint pass, `brew install llvm` provides a standalone `clang-tidy`.

The board's USB port is auto-detected. If it's wrong, pass it explicitly:

```sh
make flash PORT=/dev/cu.usbmodemXXXX
```

## Board notes

- **USB CDC On Boot is enabled** in the FQBN (`CDCOnBoot=cdc`) so `Serial`
  works over the SuperMini's native USB — no separate UART adapter needed.
- Onboard blue LED is on **GPIO8** and is **active-LOW** (LOW = on).
- If an upload fails to start, force download mode: hold **BOOT**, tap
  **RESET**, release **BOOT**, then re-run `make flash`.
