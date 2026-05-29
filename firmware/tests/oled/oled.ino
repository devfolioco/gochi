// oled.ino — SSD1306 OLED bring-up test.
//
// Initialises the 128x64 SSD1306 over hardware I2C on the same pins the
// main firmware uses (PIN_SDA / PIN_SCL from src/config.h) and cycles a
// short sequence of frames so you can confirm the panel is wired and
// addressed correctly. Nothing else on the bus — no MPU, no buzzer.
//
// Build:  arduino-cli compile --profile c3
// Flash:  arduino-cli upload  --profile c3 --port /dev/cu.usbmodem<…>

#include <Arduino.h>
#include <U8g2lib.h>

#include "../../src/config.h"

// Match the main firmware's display constructor exactly:
// full-frame-buffer, NONAME variant (defaults to address 0x3C), hardware
// I2C with explicit SCL/SDA pins so the ESP32-C3 GPIO matrix routes the
// peripheral to the SuperMini's broken-out pins.
//
// Honors the same ROTATED_DISPLAY build-time switch as the main
// firmware (set via `.env` → ROTATED_DISPLAY=1 at the project root),
// so verifying orientation here means the main firmware will match.
#ifdef ROTATED_DISPLAY
static U8G2_SSD1306_128X64_NONAME_F_HW_I2C oled(U8G2_R2, U8X8_PIN_NONE, PIN_SCL, PIN_SDA);
#else
static U8G2_SSD1306_128X64_NONAME_F_HW_I2C oled(U8G2_R0, U8X8_PIN_NONE, PIN_SCL, PIN_SDA);
#endif

static const uint32_t FRAME_MS = 1500;
static const char* FRAMES[] = {
    "OLED OK",
    "SDA=GPIO5",
    "SCL=GPIO6",
    "0x3C",
};
static const uint8_t FRAME_COUNT = sizeof(FRAMES) / sizeof(FRAMES[0]);

static void drawFrame(uint8_t i) {
  oled.clearBuffer();
  oled.setFontPosCenter();
  const char* msg = FRAMES[i];
  int x = (OLED_W - oled.getStrWidth(msg)) / 2;
  oled.drawStr(x, OLED_H / 2, msg);
  oled.sendBuffer();
}

void setup() {
  Serial.begin(115200);
  Serial.println("oled_only: bringing up SSD1306");
  oled.setBusClock(400000);  // matches the main firmware's I2C speed
  oled.begin();
  oled.setFont(u8g2_font_ncenB14_tr);
  drawFrame(0);
}

void loop() {
  static uint32_t lastMs = 0;
  static uint8_t idx = 0;
  uint32_t now = millis();
  if (now - lastMs >= FRAME_MS) {
    lastMs = now;
    idx = (idx + 1) % FRAME_COUNT;
    drawFrame(idx);
  }
}
