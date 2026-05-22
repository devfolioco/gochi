// firmware.ino — ESP32-C3 SuperMini firmware
//
// Basic blink: toggles the onboard LED on and off once a second.

// The SuperMini's onboard blue LED is on GPIO8 and is ACTIVE-LOW:
// drive the pin LOW to turn it on, HIGH to turn it off.
constexpr uint8_t LED_PIN = 8;

constexpr uint32_t BLINK_INTERVAL_MS = 1000;

void setup() {
  pinMode(LED_PIN, OUTPUT);
}

void loop() {
  digitalWrite(LED_PIN, LOW);  // LED on
  delay(BLINK_INTERVAL_MS);
  digitalWrite(LED_PIN, HIGH);  // LED off
  delay(BLINK_INTERVAL_MS);
}
