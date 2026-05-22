// text_view.h — the scrolling-text view.
#pragma once

#include <stdint.h>

#include "view.h"

// TextView shows a line of text. If it is wider than the screen it scrolls
// horizontally (marquee); otherwise it is centered.
class TextView : public View {
 public:
  static const int CAPACITY = 128;  // text buffer size, incl. NUL

  // Replace the text and restart the scroll.
  void setText(const char* s);

  // The current text.
  const char* text() const { return buf_; }

  void onEnter() override;
  void update(uint32_t now) override;
  void render(Renderer& r) override;

 private:
  char buf_[CAPACITY] = {0};
  int textW_ = 0;  // pixel width, measured on first render
  bool measured_ = false;
  int scrollX_ = 0;  // current left edge of the text
  uint32_t lastScrollMs_ = 0;
};
