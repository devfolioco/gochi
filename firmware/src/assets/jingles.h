// jingles.h — a short sound for each face expression.
//
// The buzzer plays jingleFor(expr) whenever the pet's expression changes,
// so each face has a matching voice.
#pragma once

#include <stdint.h>

#include "../buzzer/buzzer.h"
#include "expressions.h"

struct Jingle {
  const buzzer::Tone* tones;
  uint8_t count;
};

// The jingle to play when the pet switches to `id`.
Jingle jingleFor(ExpressionId id);
