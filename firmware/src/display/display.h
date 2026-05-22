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

// Draw a line of text with its top-left corner at (x, y).
void text(int x, int y, const char *msg);

// Draw a line of text horizontally centered, with its top edge at y.
void textCentered(int y, const char *msg);

// Convenience: clear, draw one fully-centered line, show — in one call.
void message(const char *msg);

}  // namespace display
