// jingles.cpp — a short sound for each face expression (see jingles.h).

#include "jingles.h"

#include <stddef.h>

namespace {

using buzzer::Tone;

// Each jingle: {frequency Hz, duration ms}; freq 0 is a rest. Kept short
// and mostly in the 400-1500 Hz range, where a piezo buzzer is loudest.
const Tone NEUTRAL[] = {{587, 90}};                                   // a soft blip
const Tone HAPPY[] = {{523, 90}, {659, 90}, {784, 90}, {1047, 150}};  // rising run
const Tone SAD[] = {{659, 200}, {587, 200}, {494, 350}};              // a descending sigh
const Tone SLEEPY[] = {{784, 260}, {587, 260}, {494, 430}};           // a slow yawn
const Tone EXCITED[] = {{880, 60},  {0, 35}, {1047, 60}, {0, 35},
                        {1175, 60}, {0, 35}, {1319, 110}};  // fast beeps
const Tone SURPRISED[] = {{784, 60}, {1319, 190}};          // a quick "!"
const Tone ANGRY[] = {
    {1175, 90}, {988, 90}, {1175, 90}, {988, 90}, {1175, 150}};         // an agitated trill
const Tone BLINK[] = {{659, 55}};                                       // a tiny tick
const Tone LOVE[] = {{659, 110}, {784, 110}, {880, 150}, {1047, 220}};  // sweet tune
const Tone HORNY[] = {{659, 90}, {988, 90}, {1245, 130}, {988, 100}, {740, 170}};  // a wolf whistle
const Tone SHY[] = {{587, 90}, {0, 70}, {523, 150}};                   // a timid two-note
const Tone DEAD[] = {{784, 150}, {587, 150}, {440, 170}, {349, 480}};  // game over

template <size_t N>
Jingle make(const Tone (&tones)[N]) {
  return Jingle{tones, static_cast<uint8_t>(N)};
}

}  // namespace

Jingle jingleFor(ExpressionId id) {
  switch (id) {
    case ExpressionId::Happy:
      return make(HAPPY);
    case ExpressionId::Sad:
      return make(SAD);
    case ExpressionId::Sleepy:
      return make(SLEEPY);
    case ExpressionId::Excited:
      return make(EXCITED);
    case ExpressionId::Surprised:
      return make(SURPRISED);
    case ExpressionId::Angry:
      return make(ANGRY);
    case ExpressionId::Blink:
      return make(BLINK);
    case ExpressionId::Love:
      return make(LOVE);
    case ExpressionId::Horny:
      return make(HORNY);
    case ExpressionId::Shy:
      return make(SHY);
    case ExpressionId::Dead:
      return make(DEAD);
    case ExpressionId::Neutral:
    case ExpressionId::Count:
    default:
      return make(NEUTRAL);
  }
}
