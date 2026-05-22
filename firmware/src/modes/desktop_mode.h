// desktop_mode.h — the Phase 1 mode: a passive, command-driven desktop pet.
#pragma once

#include <stdint.h>

#include "mode.h"

class Transport;
class Renderer;

// DesktopMode is a passive desktop pet: it shows whatever view and
// expression it is told to over the serial protocol, and blinks every so
// often while idle so the display never looks frozen.
//
// Free Mode (autonomous battery mode) will be added later as a sibling
// Mode; the main sketch will choose between them.
class DesktopMode : public Mode {
 public:
  // The transport sends command responses (OK / ERR / payloads); the
  // renderer is queried for the GET fps command.
  DesktopMode(Transport& tx, Renderer& renderer);

  void onEnter(ViewManager& vm) override;
  void update(uint32_t now, ViewManager& vm) override;
  void onCommand(const Command& cmd, ViewManager& vm) override;

 private:
  void sendState_(ViewManager& vm);
  void sendFaceList_();

  Transport& tx_;
  Renderer& renderer_;
  uint32_t lastCmdMs_ = 0;  // time of the last received command
};
