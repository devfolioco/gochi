// led.ino — on-board LED blink test.
//
// Smallest possible "is the board alive and flashed correctly?" sketch.
// Toggles the SuperMini's onboard blue LED at 1 Hz.
//
// **Why we don't use LED_BUILTIN:** the ESP32 Arduino core's generic
// esp32c3 variant defines LED_BUILTIN as `SOC_GPIO_PIN_COUNT +
// PIN_RGB_LED` — a sentinel value meant for the new rgbLedWrite()
// neopixel API on boards that actually have an RGB LED. The SuperMini
// has a plain single-colour LED, not a neopixel, so the sentinel maps
// to no real GPIO and digitalWrite() silently no-ops on it. Hardcoding
// GPIO 8 (which is what HOW-TO-BUILD.md and the project README
// document as the SuperMini's LED) sidesteps the issue.
//
// Build:  arduino-cli compile --profile c3
// Flash:  arduino-cli upload  --profile c3 --port /dev/cu.usbmodem<…>

#include <Arduino.h>

// SuperMini on-board LED. Active-LOW: LOW = on, HIGH = off. Note this
// is also the MPU's bit-banged SCL line in the main firmware (see
// PIN_MPU_SCL in src/config.h) — that's fine for this isolated test
// because the MPU isn't on the bus, but unplug the MPU module before
// running this sketch if you want a clean view of the LED.
static const uint8_t PIN_LED = 8;

static const uint32_t BLINK_MS = 500;  // half-period — 1 Hz overall

void setup() {
  Serial.begin(115200);
  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, HIGH);  // active-low: start off

  // Quick startup pattern — three fast on/off pulses — so the LED is
  // unmistakably alive before the slower steady-state blink takes
  // over. Catches the case where you glanced away during the first
  // half-cycle of the 1 Hz loop and thought nothing happened.
  for (uint8_t i = 0; i < 3; ++i) {
    digitalWrite(PIN_LED, LOW);
    delay(120);
    digitalWrite(PIN_LED, HIGH);
    delay(120);
  }

  Serial.println("led: blinking GPIO 8 (onboard LED) at 1 Hz");
}

void loop() {
  static uint32_t lastMs = 0;
  static bool on = false;
  uint32_t now = millis();
  if (now - lastMs >= BLINK_MS) {
    lastMs = now;
    on = !on;
    digitalWrite(PIN_LED, on ? LOW : HIGH);
  }
}
