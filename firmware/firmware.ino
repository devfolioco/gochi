// firmware.ino — ESP32-C3 SuperMini firmware.
//
// OLED smoke-test: shows "hello" centered on the SSD1306 display.

#include "src/display/display.h"

void setup() {
  display::begin();
  display::message("hello");
}

void loop() {
}
