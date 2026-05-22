// display.cpp — SSD1306 OLED helpers (see display.h).

#include "display.h"

#include <U8g2lib.h>

namespace {

// SSD1306 0.96" 128x64 OLED, wired over I2C.
// SuperMini GPIO5 -> SDA, GPIO6 -> SCL (3V3 + GND for power).
constexpr uint8_t OLED_SDA = 5;
constexpr uint8_t OLED_SCL = 6;

// Software (bit-banged) I2C so the exact SDA/SCL pins above are honoured;
// _F_ = full frame buffer, giving the clearBuffer()/sendBuffer() API.
U8G2_SSD1306_128X64_NONAME_F_SW_I2C oled(U8G2_R0, OLED_SCL, OLED_SDA, U8X8_PIN_NONE);

}  // namespace

namespace display {

void begin() {
  oled.begin();
  oled.setFont(u8g2_font_ncenB14_tr);
}

void clear() { oled.clearBuffer(); }

void show() { oled.sendBuffer(); }

void text(int x, int y, const char *msg) {
  oled.setFontPosTop();
  oled.drawStr(x, y, msg);
}

void textCentered(int y, const char *msg) {
  oled.setFontPosTop();
  int x = (WIDTH - oled.getStrWidth(msg)) / 2;
  oled.drawStr(x, y, msg);
}

void message(const char *msg) {
  oled.clearBuffer();
  oled.setFontPosCenter();
  int x = (WIDTH - oled.getStrWidth(msg)) / 2;
  oled.drawStr(x, HEIGHT / 2, msg);
  oled.sendBuffer();
}

}  // namespace display
