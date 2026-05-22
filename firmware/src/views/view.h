// view.h — the View interface.
//
// A View is one drawable screen. The active View is advanced once per
// loop: update() steps animation/scroll state, then render() draws the
// frame. onEnter()/onExit() bracket a switch between views.
#pragma once

#include <stdint.h>

class Renderer;

class View {
 public:
  virtual void onEnter() {}
  virtual void update(uint32_t now) = 0;
  virtual void render(Renderer& r) = 0;
  virtual void onExit() {}
  virtual ~View() {}
};
