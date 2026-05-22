// desktop_mode.cpp — the Phase 1 desktop-pet mode (see desktop_mode.h).

#include "desktop_mode.h"

#include <Arduino.h>
#include <stdio.h>

#include "../assets/expressions.h"
#include "../command.h"
#include "../renderer.h"
#include "../transport.h"
#include "../views/view_manager.h"

namespace {

// Idle fallback: if no command arrives for this long, blink so the pet
// never looks frozen. The gate lets the behavior be disabled wholesale.
const bool IDLE_FALLBACK = true;
const uint32_t IDLE_TIMEOUT_MS = 30000;

}  // namespace

DesktopMode::DesktopMode(Transport& tx, Renderer& renderer) : tx_(tx), renderer_(renderer) {}

void DesktopMode::onEnter(ViewManager& vm) {
  vm.setView(&vm.face());
  vm.face().setExpression(ExpressionId::Neutral);
  lastCmdMs_ = millis();
}

void DesktopMode::update(uint32_t now, ViewManager& vm) {
  if (!IDLE_FALLBACK) return;
  // Only the face view needs this: the text view already scrolls, so it
  // never looks frozen.
  if (!vm.isFaceActive()) return;
  if (now - lastCmdMs_ >= IDLE_TIMEOUT_MS) {
    vm.face().blinkOnce();  // subtle "still alive" cue
    lastCmdMs_ = now;       // schedule the next idle blink
  }
}

void DesktopMode::onCommand(const Command& cmd, ViewManager& vm) {
  lastCmdMs_ = millis();
  switch (cmd.type) {
    case CmdType::ShowFace: {
      const Expression* e = findExpressionByName(cmd.arg1);
      if (e == nullptr) {
        tx_.println("ERR unknown face");
        return;
      }
      vm.face().setExpression(e->id);
      vm.setView(&vm.face());
      tx_.println("OK");
      break;
    }
    case CmdType::ShowText:
      vm.text().setText(cmd.arg1);
      vm.setView(&vm.text());
      tx_.println("OK");
      break;
    case CmdType::SetMood:
      // Phase 1: accepted and acked only. Free Mode will act on mood later.
      tx_.println("OK");
      break;
    case CmdType::GetState:
      sendState_(vm);
      break;
    case CmdType::GetFps: {
      char buf[24];
      snprintf(buf, sizeof(buf), "fps %d", renderer_.fps());
      tx_.println(buf);
      break;
    }
    case CmdType::ListFaces:
      sendFaceList_();
      break;
    case CmdType::Ping:
      tx_.println("PONG");
      break;
    case CmdType::Unknown:
    default:
      tx_.println("ERR unknown command");
      break;
  }
}

void DesktopMode::sendState_(ViewManager& vm) {
  char buf[96];
  snprintf(buf, sizeof(buf), "{\"view\":\"%s\",\"expr\":\"%s\"}", vm.activeViewName(),
           expressionName(vm.face().expression()));
  tx_.println(buf);
}

void DesktopMode::sendFaceList_() {
  char buf[160];
  size_t n = 0;
  for (uint8_t i = 0; i < expressionCount(); i++) {
    const char* name = getExpression(static_cast<ExpressionId>(i)).name;
    if (i > 0 && n + 1 < sizeof(buf)) buf[n++] = ',';
    for (const char* s = name; *s != '\0' && n + 1 < sizeof(buf); s++) {
      buf[n++] = *s;
    }
  }
  buf[n] = '\0';
  tx_.println(buf);
}
