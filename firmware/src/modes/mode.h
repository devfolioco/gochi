// mode.h — device mode interface.
//
// A Mode owns the high-level behavior loop: it reacts to commands and
// nudges the views. Phase 1 ships only DesktopMode. Free Mode (the
// autonomous battery mode) will be added later as a sibling class
// implementing this same interface — no refactor needed here.
#pragma once

#include <stdint.h>

class ViewManager;
struct Command;

class Mode {
 public:
  virtual void onEnter(ViewManager& /*vm*/) {}
  virtual void update(uint32_t now, ViewManager& vm) = 0;
  virtual void onCommand(const Command& /*cmd*/, ViewManager& /*vm*/) {}
  virtual void onExit() {}
  virtual ~Mode() {}
};
