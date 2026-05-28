// free_mode.cpp — the autonomous mode (see free_mode.h).

#include "free_mode.h"

#include <Arduino.h>
#include <esp_random.h>

#include "../assets/expressions.h"
#include "../assets/jingles.h"
#include "../buzzer/buzzer.h"
#include "../views/view_manager.h"

namespace {

// Each mood's expression pool — what the pet tends to show. Repeated
// entries are weighted heavier. `sexy` and `dead` are deliberately left
// out of autonomous rotation; they stay manual-only (SHOW face ...).
const ExpressionId CONTENT[] = {ExpressionId::Neutral, ExpressionId::Neutral, ExpressionId::Happy,
                                ExpressionId::Blink};
const ExpressionId PLAYFUL[] = {ExpressionId::Happy, ExpressionId::Excited,
                                ExpressionId::Surprised};
const ExpressionId GRUMPY[] = {ExpressionId::Angry, ExpressionId::Sad, ExpressionId::Neutral};
const ExpressionId SLEEPY[] = {ExpressionId::Sleepy, ExpressionId::Sleepy, ExpressionId::Blink,
                               ExpressionId::Neutral};
const ExpressionId AFFECTIONATE[] = {ExpressionId::Love, ExpressionId::Happy, ExpressionId::Shy};

struct Pool {
  const ExpressionId* ids;
  uint8_t count;
};

Pool poolFor(Mood m) {
  switch (m) {
    case Mood::Playful:
      return {PLAYFUL, sizeof(PLAYFUL) / sizeof(*PLAYFUL)};
    case Mood::Grumpy:
      return {GRUMPY, sizeof(GRUMPY) / sizeof(*GRUMPY)};
    case Mood::Sleepy:
      return {SLEEPY, sizeof(SLEEPY) / sizeof(*SLEEPY)};
    case Mood::Affectionate:
      return {AFFECTIONATE, sizeof(AFFECTIONATE) / sizeof(*AFFECTIONATE)};
    case Mood::Content:
    case Mood::Count:
    default:
      return {CONTENT, sizeof(CONTENT) / sizeof(*CONTENT)};
  }
}

// Timings (ms). The pet changes expression every ~14-16 min and may
// drift to a new mood every ~110-130 min (≈ 2 h, with some jitter so
// it doesn't feel clockwork). Note: a mood drift also picks a fresh
// expression, so the practical expression cadence is the *minimum* of
// these two intervals.
const int EXPR_MIN_MS = 14 * 60 * 1000;   // 840 000
const int EXPR_MAX_MS = 16 * 60 * 1000;   // 960 000
const int MOOD_MIN_MS = 110 * 60 * 1000;  // 6 600 000  (~1 h 50 min)
const int MOOD_MAX_MS = 130 * 60 * 1000;  // 7 800 000  (~2 h 10 min)

int randRange(int lo, int hi) {
  return lo + static_cast<int>(esp_random() % static_cast<uint32_t>(hi - lo + 1));
}

}  // namespace

FreeMode::FreeMode(Mood& mood) : mood_(mood) {}

void FreeMode::scheduleExpr_(uint32_t now) {
  nextExprMs_ = now + static_cast<uint32_t>(randRange(EXPR_MIN_MS, EXPR_MAX_MS));
}

void FreeMode::scheduleMood_(uint32_t now) {
  nextMoodMs_ = now + static_cast<uint32_t>(randRange(MOOD_MIN_MS, MOOD_MAX_MS));
}

void FreeMode::pickExpression_(uint32_t now, ViewManager& vm, bool sound) {
  Pool pool = poolFor(mood_);
  ExpressionId cur = vm.face().expression();
  ExpressionId pick = cur;
  for (int tries = 0; tries < 6 && pick == cur; tries++) {
    pick = pool.ids[randRange(0, pool.count - 1)];
  }
  vm.face().setExpression(pick);
  if (sound) {
    Jingle jingle = jingleFor(pick);
    buzzer::play(jingle.tones, jingle.count);
  }
  scheduleExpr_(now);
}

void FreeMode::onEnter(ViewManager& vm) {
  vm.setView(&vm.face());
  uint32_t now = millis();
  pickExpression_(now, vm, false);  // settle on a mood-appropriate face
  scheduleMood_(now);
}

void FreeMode::update(uint32_t now, ViewManager& vm) {
  if (static_cast<int32_t>(now - nextMoodMs_) >= 0) {
    // Drift to a new mood. Content appears twice — the gentle "home" mood.
    static const Mood DRIFT[] = {Mood::Content, Mood::Content, Mood::Playful,
                                 Mood::Grumpy,  Mood::Sleepy,  Mood::Affectionate};
    Mood next = mood_;
    for (int tries = 0; tries < 6 && next == mood_; tries++) {
      next = DRIFT[randRange(0, 5)];
    }
    mood_ = next;
    scheduleMood_(now);
    pickExpression_(now, vm, true);  // a mood shift gets a (quiet) jingle
  } else if (static_cast<int32_t>(now - nextExprMs_) >= 0) {
    pickExpression_(now, vm, false);  // a routine expression tick — silent
  }
}
