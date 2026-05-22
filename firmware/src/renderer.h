// renderer.h — the drawing surface handed to each View.
//
// A thin, frame-oriented wrapper over the src/display module (the
// U8g2-backed SSD1306 driver), so views never touch the display library
// directly. A View draws one frame between beginFrame() and endFrame().
#pragma once

#include <stdint.h>

class Renderer {
 public:
  // Initialize the display. Call once at startup.
  void init();

  // Start a new frame (clears the off-screen buffer).
  void beginFrame();

  // Draw a 1-bit bitmap (MSB-first; w must be a multiple of 8) with its
  // top-left corner at (x, y).
  void drawBitmap(int x, int y, const uint8_t* bmp, int w, int h);

  // Draw a string with its top-left corner at (x, y). x may be negative
  // (used for marquee scrolling); off-screen pixels are clipped.
  void drawText(int x, int y, const char* str);

  // Pixel width of `str` in the current font.
  int textWidth(const char* str);

  // Erase a rectangular region of the current frame (e.g. a wipe effect).
  void clearRect(int x, int y, int w, int h);

  // Draw a filled rounded rectangle with its top-left corner at (x, y).
  void fillRoundRect(int x, int y, int w, int h, int r);

  // Set a single pixel.
  void drawPixel(int x, int y);

  // Draw a 1px line between two points.
  void drawLine(int x0, int y0, int x1, int y1);

  // Draw a filled circle of the given radius, centered at (x, y).
  void fillCircle(int x, int y, int r);

  // Draw a filled triangle through the three given points.
  void fillTriangle(int x0, int y0, int x1, int y1, int x2, int y2);

  // Push the finished frame to the panel.
  void endFrame();

  // Frames pushed to the panel per second, refreshed about once a second.
  // Reads 0 until the first second has elapsed.
  int fps() const { return fps_; }

 private:
  int frameCount_ = 0;
  int fps_ = 0;
  uint32_t fpsWindowStartMs_ = 0;
};
