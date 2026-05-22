// display.h — SSD1306 OLED helpers.
//
// Wraps U8g2 so the rest of the firmware can draw without knowing anything
// about the display driver, the I2C pins, or off-screen buffering.
#pragma once

#include <stdint.h>

namespace display {

// Panel geometry — handy for callers laying things out by hand.
constexpr uint8_t WIDTH = 128;
constexpr uint8_t HEIGHT = 64;

// Initialise the OLED. Call once from setup() before any draw call.
void begin();

// Wipe the off-screen buffer. Drawing is buffered: nothing reaches the
// panel until show() is called.
void clear();

// Push the off-screen buffer to the panel.
void show();

// Draw a line of text with its top-left corner at (x, y). x may be
// negative (used for marquee scrolling); off-screen pixels are clipped.
void text(int x, int y, const char* msg);

// Draw a line of text horizontally centered, with its top edge at y.
void textCentered(int y, const char* msg);

// Convenience: clear, draw one fully-centered line, show — in one call.
void message(const char* msg);

// Pixel width of msg in the current font.
int textWidth(const char* msg);

// Draw a 1-bit bitmap with its top-left corner at (x, y). The bitmap is
// MSB-first (bit 7 = leftmost pixel); w must be a multiple of 8.
void drawBitmap(int x, int y, int w, int h, const uint8_t* bmp);

// Erase a rectangular region of the off-screen buffer.
void clearRect(int x, int y, int w, int h);

// Draw a filled rounded rectangle with its top-left corner at (x, y).
void fillRoundRect(int x, int y, int w, int h, int r);

// Set a single pixel.
void drawPixel(int x, int y);

// Draw a 1px line between two points.
void drawLine(int x0, int y0, int x1, int y1);

// Draw a filled circle (disc) of the given radius, centered at (x, y).
void fillCircle(int x, int y, int r);

// Draw a filled triangle through the three given points.
void fillTriangle(int x0, int y0, int x1, int y1, int x2, int y2);

}  // namespace display
