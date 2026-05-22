// view_manager.h — owns the views and runs the active one.
#pragma once

#include <stdint.h>

#include "face_view.h"
#include "text_view.h"

class Renderer;

// ViewManager owns the single FaceView and TextView and tracks which is
// active. setView() drives the onExit/onEnter handoff; tick() advances and
// draws the active view once per loop.
class ViewManager {
 public:
  void setView(View* v);
  void tick(uint32_t now, Renderer& r);

  FaceView& face() { return face_; }
  TextView& text() { return text_; }

  bool isFaceActive() const { return active_ == &face_; }
  bool isTextActive() const { return active_ == &text_; }

  // "face", "text", or "none" — used by the GET state response.
  const char* activeViewName() const;

 private:
  FaceView face_;
  TextView text_;
  View* active_ = nullptr;
};
