// text_view.cpp — the scrolling-text view (see text_view.h).

#include "text_view.h"

#include <Arduino.h>
#include <string.h>

#include "../config.h"
#include "../renderer.h"

namespace {
const uint32_t SCROLL_STEP_MS = 40;  // time between scroll steps
const int SCROLL_PX = 2;             // pixels per scroll step
const int TEXT_TOP_Y = 24;           // top edge — vertically centers the font
}  // namespace

void TextView::setText(const char* s) {
  if (s == nullptr) s = "";
  strncpy(buf_, s, CAPACITY - 1);
  buf_[CAPACITY - 1] = '\0';
  measured_ = false;  // width depends on the font; remeasure on next render
  scrollX_ = 0;
  lastScrollMs_ = millis();
}

void TextView::onEnter() {
  measured_ = false;
  lastScrollMs_ = millis();
}

void TextView::update(uint32_t now) {
  if (!measured_ || textW_ <= OLED_W) return;  // fits — nothing to scroll
  if (now - lastScrollMs_ >= SCROLL_STEP_MS) {
    lastScrollMs_ = now;
    scrollX_ -= SCROLL_PX;
    if (scrollX_ <= -textW_) scrollX_ = OLED_W;  // wrap back in from the right
  }
}

void TextView::render(Renderer& r) {
  if (!measured_) {
    textW_ = r.textWidth(buf_);
    measured_ = true;
    // Wide text starts off the right edge and scrolls in.
    scrollX_ = (textW_ > OLED_W) ? OLED_W : 0;
  }

  if (textW_ <= OLED_W) {
    r.drawText((OLED_W - textW_) / 2, TEXT_TOP_Y, buf_);  // centered
  } else {
    r.drawText(scrollX_, TEXT_TOP_Y, buf_);  // marquee
  }
}
