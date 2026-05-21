// tamagotchi.ino — ESP32-C3 SuperMini firmware
//
// Board: esp32:esp32:esp32c3 with "USB CDC On Boot" enabled, so Serial
// works over the SuperMini's native USB port (no extra UART adapter).
//
// This starting sketch is a smoke test: it blinks the onboard LED and
// prints over Serial so you can confirm flashing + monitoring both work
// before building out the actual tamagotchi logic.

// The SuperMini's onboard blue LED is on GPIO8 and is ACTIVE-LOW:
// drive the pin LOW to turn it on, HIGH to turn it off.
//
constexpr uint8_t LED_PIN = 8;

constexpr uint32_t BLINK_INTERVAL_MS = 500;

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);  // start with the LED off
  Serial.println("\n[tamagotchi] boot ok");
}

void loop() {
  digitalWrite(LED_PIN, LOW);  // LED on
  Serial.println("[tamagotchi] tick");
  delay(BLINK_INTERVAL_MS);

  digitalWrite(LED_PIN, HIGH);  // LED off
  delay(BLINK_INTERVAL_MS);
}
