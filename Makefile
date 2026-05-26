# ESP32-C3 SuperMini firmware — arduino-cli wrapper.
# Override the port if auto-detection picks the wrong one:
#   make flash PORT=/dev/cu.usbmodemXXXX

SKETCH := firmware
FQBN   := esp32:esp32:esp32c3:CDCOnBoot=cdc
BUILD  := $(SKETCH)/build
BAUD   := 115200
PORT   ?= $(shell ls /dev/cu.usbmodem* 2>/dev/null | head -1)

# arduino-cli invoked with the project-local config (firmware/arduino-cli.yaml)
# instead of the global ~/Library/Arduino15 one, so the repo is self-contained.
ARDUINO := arduino-cli --config-file $(CURDIR)/firmware/arduino-cli.yaml

# All firmware sources, excluding the generated build/ tree.
SRC    := $(shell find firmware -name build -prune -o -name '*.ino' -print -o -name '*.cpp' -print -o -name '*.h' -print)

.PHONY: build db flash upload monitor flash-monitor ports format format-check clean

## build         — compile the sketch
build:
	$(ARDUINO) compile --fqbn $(FQBN) --build-path $(BUILD) $(SKETCH)

## db            — regenerate build/compile_commands.json for clangd (Zed)
db:
	$(ARDUINO) compile --fqbn $(FQBN) --build-path $(BUILD) --only-compilation-database $(SKETCH)

## flash         — compile, then upload to the board
flash: build upload

## upload        — upload the last build without recompiling.
##                  If the tamagotchi daemon is running it is holding the
##                  serial port; we ask it to release the port before the
##                  upload and reacquire on exit (success or failure).
upload:
	@_paused=0; \
	if command -v tamagotchi >/dev/null 2>&1 && launchctl list com.tamagotchi.daemon >/dev/null 2>&1; then \
	  _paused=1; \
	  echo "→ tamagotchi stop (releasing serial port)"; \
	  tamagotchi stop >/dev/null; \
	fi; \
	trap '[ "$$_paused" = 1 ] && echo "→ tamagotchi start (reacquiring)" && tamagotchi start >/dev/null' EXIT; \
	$(ARDUINO) upload --fqbn $(FQBN) --port $(PORT) --input-dir $(BUILD) $(SKETCH)

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
