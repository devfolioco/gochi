// procedural_face.h — the pet's face, drawn procedurally.
//
// Every expression is drawn from primitives (rounded rects, arcs, lines,
// circles) — no sprite bitmaps — which is what lets them animate and stay
// alive: eyes blink, the gaze drifts, and each expression carries its own
// idle motion (a bob, a tremble, floating "Z"s, a sliding tear, ...).
//
// Switching expression plays a quick blink: the eyes close, the new
// expression is swapped in while they are shut, then they reopen.
//
// Style reference: the Tabbie desk robot — a minimal rounded-rect-eyed
// face that "looks around". (Tabbie pans a neck servo; this pans on-screen.)
#pragma once

#include <stdint.h>

#include "../assets/expressions.h"

class Renderer;

class ProceduralFace {
 public:
  // (Re)start the idle-animation timers for the current expression.
  void reset(uint32_t now);

  // Switch expression. The first call after boot applies instantly; later
  // calls play a blink transition. Re-selecting the same one just blinks.
  void setExpression(ExpressionId id, uint32_t now);

  ExpressionId expression() const { return expr_; }

  // Trigger a blink now (only expressions with open eyes can blink).
  void blink(uint32_t now);

  // Advance the animation.
  void update(uint32_t now);

  // Draw the current frame.
  void render(Renderer& r);

 private:
  enum class Blink : uint8_t { Idle, Closing, Opening };

  void scheduleBlink_(uint32_t now);
  void pickGaze_(uint32_t now);
  void onExpressionChanged_(uint32_t now);

  ExpressionId expr_ = ExpressionId::Neutral;
  ExpressionId pendingExpr_ = ExpressionId::Neutral;
  bool initialized_ = false;
  bool changing_ = false;  // a setExpression() blink-swap is in progress

  uint32_t lastNow_ = 0;  // millis() from the latest update(), used by render()

  // Blink — eyeOpen_ scales the eyes: 0 = shut, 256 = fully open.
  Blink blink_ = Blink::Idle;
  uint32_t blinkPhaseMs_ = 0;
  uint32_t nextBlinkMs_ = 0;
  int eyeOpen_ = 256;

  // Gaze — horizontal drift in 1/16-px fixed point, so the easing moves
  // smoothly below whole-pixel steps. gazeAmp_ depends on the expression.
  int gaze16_ = 0;
  int gazeTarget16_ = 0;
  int gazeAmp_ = 0;
  uint32_t nextGazeMs_ = 0;
  uint32_t gazeTickMs_ = 0;
};
