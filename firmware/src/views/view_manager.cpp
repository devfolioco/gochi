// view_manager.cpp — owns the views and runs the active one (see header).

#include "view_manager.h"

void ViewManager::setView(View* v) {
  if (v == active_) return;
  if (active_ != nullptr) active_->onExit();
  active_ = v;
  if (active_ != nullptr) active_->onEnter();
}

void ViewManager::tick(uint32_t now, Renderer& r) {
  if (active_ == nullptr) return;
  active_->update(now);
  active_->render(r);
}

const char* ViewManager::activeViewName() const {
  if (active_ == &face_) return "face";
  if (active_ == &text_) return "text";
  return "none";
}
