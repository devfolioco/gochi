# ESP32-C3 SuperMini firmware — arduino-cli wrapper.
# Override the port if auto-detection picks the wrong one:
#   make flash PORT=/dev/cu.usbmodemXXXX

SKETCH := firmware/tamagotchi
FQBN   := esp32:esp32:esp32c3:CDCOnBoot=cdc
BUILD  := $(SKETCH)/build
BAUD   := 115200
PORT   ?= $(shell ls /dev/cu.usbmodem* 2>/dev/null | head -1)

# All firmware sources, excluding the generated build/ tree.
SRC    := $(shell find firmware -name build -prune -o -name '*.ino' -print -o -name '*.cpp' -print -o -name '*.h' -print)

.PHONY: build db flash upload monitor flash-monitor ports format format-check clean

## build         — compile the sketch
build:
	arduino-cli compile --fqbn $(FQBN) --build-path $(BUILD) $(SKETCH)

## db            — regenerate build/compile_commands.json for clangd (Zed)
db:
	arduino-cli compile --fqbn $(FQBN) --build-path $(BUILD) --only-compilation-database $(SKETCH)

## flash         — compile, then upload to the board
flash: build upload

## upload        — upload the last build without recompiling
upload:
	arduino-cli upload --fqbn $(FQBN) --port $(PORT) --input-dir $(BUILD) $(SKETCH)

## monitor       — open the serial monitor (Ctrl-C to exit)
monitor:
	arduino-cli monitor --port $(PORT) --config baudrate=$(BAUD)

## flash-monitor — flash, then immediately open the serial monitor
flash-monitor: flash monitor

## ports         — list connected boards / serial ports
ports:
	arduino-cli board list

## format        — auto-format all firmware sources in place
format:
	clang-format -i $(SRC)

## format-check  — verify formatting without editing (CI / pre-commit)
format-check:
	clang-format --dry-run --Werror $(SRC)

## clean         — remove build artifacts
clean:
	rm -rf $(BUILD)
