// display.cpp — SSD1306 OLED helpers (see display.h).

#include "display.h"

#include <U8g2lib.h>

#include "../config.h"

namespace {

// SSD1306 0.96" 128x64 OLED, wired over I2C — pins come from config.h
// (SuperMini GPIO5 -> SDA, GPIO6 -> SCL, 3V3 + GND for power).
//
// Hardware I2C: the ESP32-C3's I2C peripheral clocks the bus itself, far
// faster than bit-banging it from the CPU — this is what lets the display
// refresh smoothly. It routes to any GPIO through the chip's GPIO matrix,
// so PIN_SDA/PIN_SCL are still honoured.
// _F_ = full frame buffer, giving the clearBuffer()/sendBuffer() API.
// HW I2C constructor args: rotation, reset, clock (SCL), data (SDA).
//
// Rotation defaults to U8G2_R0 (panel pins at top). Define
// ROTATED_DISPLAY (via `.env` → ROTATED_DISPLAY=1, see the project
// root) for a 180°-rotated panel.
#ifdef ROTATED_DISPLAY
U8G2_SSD1306_128X64_NONAME_F_HW_I2C oled(U8G2_R2, U8X8_PIN_NONE, PIN_SCL, PIN_SDA);
#else
U8G2_SSD1306_128X64_NONAME_F_HW_I2C oled(U8G2_R0, U8X8_PIN_NONE, PIN_SCL, PIN_SDA);
#endif

}  // namespace

namespace display {

void begin() {
  oled.setBusClock(400000);  // 400 kHz I2C fast mode — OLED is alone on Wire
  oled.begin();
  oled.setFont(u8g2_font_ncenB14_tr);
}

void clear() { oled.clearBuffer(); }

void show() { oled.sendBuffer(); }

void text(int x, int y, const char* msg) {
  oled.setFontPosTop();
  oled.drawStr(x, y, msg);
}

void textCentered(int y, const char* msg) {
  oled.setFontPosTop();
  int x = (WIDTH - oled.getStrWidth(msg)) / 2;
  oled.drawStr(x, y, msg);
}

void message(const char* msg) {
  oled.clearBuffer();
  oled.setFontPosCenter();
  int x = (WIDTH - oled.getStrWidth(msg)) / 2;
  oled.drawStr(x, HEIGHT / 2, msg);
  oled.sendBuffer();
}

int textWidth(const char* msg) { return oled.getStrWidth(msg); }

void drawBitmap(int x, int y, int w, int h, const uint8_t* bmp) {
  // U8g2 takes the row stride in bytes; our bitmaps are MSB-first.
  oled.drawBitmap(x, y, w / 8, h, bmp);
}

void clearRect(int x, int y, int w, int h) {
  oled.setDrawColor(0);
  oled.drawBox(x, y, w, h);
  oled.setDrawColor(1);
}

void fillRoundRect(int x, int y, int w, int h, int r) { oled.drawRBox(x, y, w, h, r); }

void drawPixel(int x, int y) { oled.drawPixel(x, y); }

void drawLine(int x0, int y0, int x1, int y1) { oled.drawLine(x0, y0, x1, y1); }

void fillCircle(int x, int y, int r) {
  if (r > 0) oled.drawDisc(x, y, r);
}

void fillTriangle(int x0, int y0, int x1, int y1, int x2, int y2) {
  // drawTriangle() takes signed u8g2_int_t; cast explicitly so the
  // narrowing is intentional (face coordinates are well within range).
  oled.drawTriangle(static_cast<u8g2_int_t>(x0), static_cast<u8g2_int_t>(y0),
                    static_cast<u8g2_int_t>(x1), static_cast<u8g2_int_t>(y1),
                    static_cast<u8g2_int_t>(x2), static_cast<u8g2_int_t>(y2));
}

}  // namespace display
