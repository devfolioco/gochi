# ESP32-C3 SuperMini firmware — arduino-cli wrapper.
# Override the port if auto-detection picks the wrong one:
#   make flash PORT=/dev/cu.usbmodemXXXX        (macOS)
#   make flash PORT=/dev/ttyACM0                (Linux / WSL)
#   make flash PORT=COM7                        (Windows)

SKETCH := firmware
FQBN   := esp32:esp32:esp32c3:CDCOnBoot=cdc
BUILD  := $(SKETCH)/build
BAUD   := 115200

# Best-effort port auto-detect. macOS exposes the board as
# /dev/cu.usbmodem*, Linux/WSL as /dev/ttyACM*. On Windows the device
# name is COMx and there's no glob to grep — pass PORT=COMx explicitly,
# or run `make ports` (which calls `arduino-cli board list`) to find it.
ifeq ($(OS),Windows_NT)
  PORT ?=
else
  PORT ?= $(shell ls /dev/cu.usbmodem* /dev/ttyACM* 2>/dev/null | head -1)
endif

# arduino-cli invoked with the project-local config (firmware/arduino-cli.yaml)
# instead of the global ~/Library/Arduino15 one, so the repo is self-contained.
ARDUINO := arduino-cli --config-file $(CURDIR)/firmware/arduino-cli.yaml

# All firmware sources, excluding the generated build/ tree.
SRC    := $(shell find firmware -name build -prune -o -name '*.ino' -print -o -name '*.cpp' -print -o -name '*.h' -print)

# Cross-platform "open this file in the default app". macOS uses `open`,
# Linux uses `xdg-open`, MSYS / Git Bash on Windows shells out to cmd's
# built-in `start`. WSL falls into the Linux branch and works if a Linux
# browser is installed; otherwise override OPEN to `cmd.exe /c start`.
UNAME_S := $(shell uname -s 2>/dev/null)
ifeq ($(UNAME_S),Darwin)
  OPEN := open
else ifeq ($(UNAME_S),Linux)
  OPEN := xdg-open
else
  OPEN := cmd //c start
endif

# User-local build configuration. `.env` is gitignored — copy
# `.env.example` to `.env` and edit it. Each supported KEY=VALUE pair
# in .env is translated to a -D<KEY>=<value> compiler flag and applied
# to every compile invocation below (main firmware + tests).
-include .env

EXTRA_DEFINES :=
ifeq ($(ROTATED_DISPLAY),1)
  EXTRA_DEFINES += -DROTATED_DISPLAY=1
endif
# Empty when no flags are set, so arduino-cli keeps its defaults.
BUILD_PROPS := $(if $(strip $(EXTRA_DEFINES)),--build-property "build.extra_flags=$(EXTRA_DEFINES)",)

.PHONY: build db flash upload erase monitor flash-monitor ports format format-check clean
.PHONY: test-led test-oled test-buzzer test-mpu

## build         — compile the sketch
build:
	$(ARDUINO) compile --fqbn $(FQBN) $(BUILD_PROPS) --build-path $(BUILD) $(SKETCH)

## db            — regenerate build/compile_commands.json for clangd (Zed)
db:
	$(ARDUINO) compile --fqbn $(FQBN) $(BUILD_PROPS) --build-path $(BUILD) --only-compilation-database $(SKETCH)

## flash         — compile, then upload to the board
flash: build upload

## upload        — upload the last build without recompiling.
##                  If the gochi daemon is running it is holding the
##                  serial port; we ask it to release the port before the
##                  upload and reacquire on exit (success or failure).
upload:
	@_paused=0; \
	if command -v gochi >/dev/null 2>&1 && launchctl list com.tamagotchi.daemon >/dev/null 2>&1; then \
	  _paused=1; \
	  echo "→ gochi stop (releasing serial port)"; \
	  gochi stop >/dev/null; \
	fi; \
	trap '[ "$$_paused" = 1 ] && echo "→ gochi start (reacquiring)" && gochi start >/dev/null' EXIT; \
	$(ARDUINO) upload --fqbn $(FQBN) --port $(PORT) --input-dir $(BUILD) $(SKETCH)

## erase         — wipe the entire flash (factory reset).
##                  arduino-cli has no built-in erase, so we shell out
##                  to esptool from the ESP32 core install. Same daemon-
##                  pause / port-handoff dance as `upload`. The pinned
##                  core (esp32:esp32@3.3.8) ships esptool v5, hence the
##                  `erase-flash` subcommand (esptool ≤4 used `erase_flash`).
erase:
	@DATA_DIR=$$($(ARDUINO) config get directories.data 2>/dev/null); \
	if [ -z "$$DATA_DIR" ]; then \
	  echo "arduino-cli couldn't report its data directory."; exit 1; \
	fi; \
	ESPTOOL=$$(ls -1 "$$DATA_DIR"/packages/esp32/tools/esptool_py/*/esptool 2>/dev/null | sort | tail -1); \
	if [ -z "$$ESPTOOL" ]; then \
	  echo "esptool not found under $$DATA_DIR/packages/esp32/tools/esptool_py/*."; \
	  echo "Reinstall the core: arduino-cli --config-file firmware/arduino-cli.yaml core install esp32:esp32@3.3.8"; \
	  exit 1; \
	fi; \
	if [ -z "$(PORT)" ]; then \
	  echo "No port. Pass it explicitly, e.g."; \
	  echo "  make erase PORT=/dev/cu.usbmodemXXXX   (macOS)"; \
	  echo "  make erase PORT=/dev/ttyACM0           (Linux / WSL)"; \
	  echo "  make erase PORT=COM7                   (Windows; from Git Bash)"; \
	  exit 1; \
	fi; \
	_paused=0; \
	if command -v gochi >/dev/null 2>&1 && launchctl list com.tamagotchi.daemon >/dev/null 2>&1; then \
	  _paused=1; \
	  echo "→ gochi stop (releasing serial port)"; \
	  gochi stop >/dev/null; \
	fi; \
	trap '[ "$$_paused" = 1 ] && echo "→ gochi start (reacquiring)" && gochi start >/dev/null' EXIT; \
	echo "→ esptool erase-flash on $(PORT)"; \
	"$$ESPTOOL" --chip esp32c3 --port "$(PORT)" erase-flash

## monitor       — open the serial monitor (Ctrl-C to exit)
monitor:
	$(ARDUINO) monitor --port $(PORT) --config baudrate=$(BAUD)

## flash-monitor — flash, then immediately open the serial monitor
flash-monitor: flash monitor

## ports         — list connected boards / serial ports
ports:
	$(ARDUINO) board list

## format        — auto-format all firmware sources in place
format:
	clang-format -i $(SRC)

## format-check  — verify formatting without editing (CI / pre-commit)
format-check:
	clang-format --dry-run --Werror $(SRC)

## clean         — remove build artifacts
clean:
	rm -rf $(BUILD)

# --- Hardware bring-up tests (firmware/tests/<name>) -------------------
# Each test is a standalone sketch sharing the main firmware's config.h
# pin map. `make test-<name>` compiles and flashes it to the board.

## test-led       — compile + flash the LED-blink bring-up test
test-led:
	$(ARDUINO) compile --fqbn $(FQBN) $(BUILD_PROPS) --build-path firmware/tests/led/build firmware/tests/led
	$(ARDUINO) upload  --fqbn $(FQBN) --port $(PORT) --input-dir firmware/tests/led/build firmware/tests/led

## test-oled      — compile + flash the OLED bring-up test
test-oled:
	$(ARDUINO) compile --fqbn $(FQBN) $(BUILD_PROPS) --build-path firmware/tests/oled/build firmware/tests/oled
	$(ARDUINO) upload  --fqbn $(FQBN) --port $(PORT) --input-dir firmware/tests/oled/build firmware/tests/oled

## test-buzzer    — compile + flash the buzzer bring-up test
test-buzzer:
	$(ARDUINO) compile --fqbn $(FQBN) $(BUILD_PROPS) --build-path firmware/tests/buzzer/build firmware/tests/buzzer
	$(ARDUINO) upload  --fqbn $(FQBN) --port $(PORT) --input-dir firmware/tests/buzzer/build firmware/tests/buzzer

## test-mpu       — compile + flash the MPU-6050 streaming test, then
##                  open the live viewer in the default browser. Needs
##                  Chrome or Edge (Web Serial API). If `gochi` is
##                  holding the port, `gochi stop` first.
test-mpu:
	$(ARDUINO) compile --fqbn $(FQBN) $(BUILD_PROPS) --build-path firmware/tests/mpu/build firmware/tests/mpu
	$(ARDUINO) upload  --fqbn $(FQBN) --port $(PORT) --input-dir firmware/tests/mpu/build firmware/tests/mpu
	@$(OPEN) firmware/tests/mpu/visualize.html
