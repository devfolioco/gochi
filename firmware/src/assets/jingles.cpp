// jingles.cpp — a short sound for each face expression (see jingles.h).

#include "jingles.h"

#include <stddef.h>

namespace {

using buzzer::Tone;

// Each jingle: {frequency Hz, duration ms}; freq 0 is a rest. Most tones sit
// around 700-3000 Hz, with only brief bright accents above that.
const Tone NEUTRAL[] = {{1180, 55}, {0, 24}, {970, 48}};  // compact status ping
const Tone HAPPY[] = {{840, 54}, {0, 22}, {1180, 62}, {0, 18}, {1660, 58}, {0, 22}, {2320, 96}};
const Tone SAD[] = {{1320, 170}, {0, 60}, {1040, 190}, {0, 72}, {780, 240}, {640, 220}};
const Tone SLEEPY[] = {{1160, 230}, {0, 130}, {880, 260}, {0, 160}, {690, 340}, {560, 260}};
const Tone EXCITED[] = {{1020, 36}, {0, 18},    {1480, 34}, {0, 16},    {2050, 38},
                        {0, 20},    {2760, 42}, {0, 16},    {3380, 32}, {2460, 84}};
const Tone SURPRISED[] = {{940, 34}, {0, 26}, {2860, 88}, {3420, 32}};
const Tone ANGRY[] = {{690, 42}, {760, 36}, {0, 22}, {700, 40},  {780, 34},
                      {0, 24},   {720, 46}, {0, 18}, {1240, 42}, {3220, 46}};
const Tone BLINK[] = {{1840, 30}, {0, 16}, {1360, 24}};  // tiny optical tick
const Tone LOVE[] = {{760, 72}, {0, 28},    {1120, 74}, {0, 24},    {1620, 82},
                     {0, 30},   {2260, 86}, {0, 22},    {3160, 42}, {2540, 128}};
const Tone HORNY[] = {{900, 44}, {0, 32},    {1450, 46}, {0, 26},   {2050, 68},
                      {0, 38},   {1720, 56}, {0, 24},    {2380, 92}};
const Tone SHY[] = {{820, 50}, {0, 92}, {1160, 44}, {0, 64}, {1480, 70}};
const Tone DEAD[] = {{1260, 210}, {0, 88}, {940, 240}, {0, 110}, {690, 280}, {520, 360}};

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
