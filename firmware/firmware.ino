// firmware.ino — Tamagotchi firmware entry point (ESP32-C3 SuperMini).
//
// Phase 1: Desktop Mode only. The pet passively shows whatever view and
// expression it is told to over the USB serial protocol (see
// src/command.h). Drive it from a serial monitor at 115200 baud — try
// "LIST faces". See firmware/README.md for the full command list.

#include "src/command.h"
#include "src/config.h"
#include "src/modes/desktop_mode.h"
#include "src/modes/mode.h"
#include "src/renderer.h"
#include "src/transport.h"
#include "src/views/view_manager.h"

static Renderer renderer;
static Transport transport;
static ViewManager viewManager;
static DesktopMode desktopMode(transport, renderer);

// Only Desktop Mode exists in Phase 1. This Mode* indirection is the seam
// where Free Mode will slot in later as a second mode.
static Mode* currentMode = &desktopMode;

void setup() {
  transport.begin(115200);
  renderer.init();

  // Buttons are wired but unused in Phase 1 (the views are driven over
  // serial). Configured here so the pins start in a known state.
  pinMode(PIN_BTN_A, INPUT_PULLUP);
  pinMode(PIN_BTN_B, INPUT_PULLUP);
  pinMode(PIN_BTN_C, INPUT_PULLUP);

  currentMode->onEnter(viewManager);
  transport.println("Tamagotchi Phase 1 ready (Desktop Mode). Try: LIST faces");
}

void loop() {
  uint32_t now = millis();

  Command cmd;
  if (transport.poll(cmd)) currentMode->onCommand(cmd, viewManager);

  currentMode->update(now, viewManager);

  renderer.beginFrame();
  viewManager.tick(now, renderer);
  renderer.endFrame();
}
