// renderer.cpp — the drawing surface (see renderer.h).

#include "renderer.h"

#include <Arduino.h>  // millis

#include "display/display.h"

void Renderer::init() { display::begin(); }

void Renderer::beginFrame() { display::clear(); }

void Renderer::drawBitmap(int x, int y, const uint8_t* bmp, int w, int h) {
  display::drawBitmap(x, y, w, h, bmp);
}

void Renderer::drawText(int x, int y, const char* str) { display::text(x, y, str); }

int Renderer::textWidth(const char* str) { return display::textWidth(str); }

void Renderer::clearRect(int x, int y, int w, int h) { display::clearRect(x, y, w, h); }

void Renderer::fillRoundRect(int x, int y, int w, int h, int r) {
  display::fillRoundRect(x, y, w, h, r);
}

void Renderer::drawPixel(int x, int y) { display::drawPixel(x, y); }

void Renderer::drawLine(int x0, int y0, int x1, int y1) { display::drawLine(x0, y0, x1, y1); }

void Renderer::fillCircle(int x, int y, int r) { display::fillCircle(x, y, r); }

void Renderer::fillTriangle(int x0, int y0, int x1, int y1, int x2, int y2) {
  display::fillTriangle(x0, y0, x1, y1, x2, y2);
}

void Renderer::endFrame() {
  display::show();

  // Rolling frames-per-second, recomputed once a second.
  uint32_t now = millis();
  if (fpsWindowStartMs_ == 0) fpsWindowStartMs_ = now;
  frameCount_++;
  if (now - fpsWindowStartMs_ >= 1000) {
    fps_ = frameCount_;
    frameCount_ = 0;
    fpsWindowStartMs_ = now;
  }
}
