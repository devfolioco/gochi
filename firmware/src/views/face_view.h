// face_view.h — the View that shows the pet's face.
#pragma once

#include <stdint.h>

#include "../assets/expressions.h"
#include "procedural_face.h"
#include "view.h"

// FaceView adapts ProceduralFace to the View interface. All of the drawing
// and the idle "alive" animation (blinking, gaze drift, per-expression
// motion) live in ProceduralFace; FaceView just forwards to it.
class FaceView : public View {
 public:
  // Switch expression (plays a blink transition, except on the first call).
  void setExpression(ExpressionId id);

  // The expression currently being displayed.
  ExpressionId expression() const { return face_.expression(); }

  // Blink in place without changing the expression.
  void blinkOnce();

  void onEnter() override;
  void update(uint32_t now) override;
  void render(Renderer& r) override;

 private:
  ProceduralFace face_;
};
