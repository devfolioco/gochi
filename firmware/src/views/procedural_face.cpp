// procedural_face.cpp — the pet's face, drawn procedurally (see header).

#include "procedural_face.h"

#include <esp_random.h>

#include "../config.h"
#include "../renderer.h"

namespace {

// --- Layout ------------------------------------------------------------
const int EYE_CY = 27;  // default eye vertical center
const int EYE_DX = 25;  // eye horizontal offset from the screen center

// --- Blink timing ------------------------------------------------------
const uint32_t BLINK_CLOSE_MS = 70;  // eyes shut quickly
const uint32_t BLINK_OPEN_MS = 115;  // and open a little slower

// --- Gaze drift --------------------------------------------------------
const uint32_t GAZE_TICK_MS = 16;  // ease-step cadence (~60 Hz)
const int GAZE_EASE_DIV = 16;      // higher = slower, more graceful glide
const int GAZE_MIN_HOLD_MS = 1500;
const int GAZE_MAX_HOLD_MS = 3400;

// Uniform random integer in [lo, hi]. esp_random() is a true RNG, so the
// idle motion differs every run.
int randRange(int lo, int hi) {
  return lo + static_cast<int>(esp_random() % static_cast<uint32_t>(hi - lo + 1));
}

// Triangle wave: oscillates -amp..+amp over `period` ms. Used for the
// per-expression idle motion (bob, tremble, ...).
int wave(uint32_t now, uint32_t period, int amp) {
  if (period == 0) return 0;
  int half = static_cast<int>(period / 2);
  int x = static_cast<int>(now % period);
  if (x < half) return -amp + 2 * amp * x / half;
  return amp - 2 * amp * (x - half) / half;
}

// --- Per-expression personality ---------------------------------------

// Expressions whose eyes are open rounded rects can blink on their own.
bool exprBlinks(ExpressionId e) {
  return e == ExpressionId::Neutral || e == ExpressionId::Sad || e == ExpressionId::Excited ||
         e == ExpressionId::Angry || e == ExpressionId::Shy;
}

// How far (px) the whole face drifts left/right for each expression.
int gazeAmpFor(ExpressionId e) {
  switch (e) {
    case ExpressionId::Neutral:
      return 13;
    case ExpressionId::Happy:
      return 10;
    case ExpressionId::Love:
      return 8;
    case ExpressionId::Sad:
      return 6;
    case ExpressionId::Sleepy:
      return 4;
    case ExpressionId::Blink:
      return 3;
    case ExpressionId::Horny:
      return 5;  // a sly sidelong glance
    case ExpressionId::Shy:
      return 7;  // timid glances
    default:
      return 0;  // surprised / excited / angry / dead
  }
}

// --- Drawing primitives ------------------------------------------------

// A thick line, drawn as overlapping discs (rounded caps).
void thickLine(Renderer& r, int x0, int y0, int x1, int y1, int rad) {
  int dx = x1 - x0;
  int dy = y1 - y0;
  int steps = (dx < 0 ? -dx : dx) > (dy < 0 ? -dy : dy) ? (dx < 0 ? -dx : dx) : (dy < 0 ? -dy : dy);
  if (steps < 1) steps = 1;
  for (int i = 0; i <= steps; i++) {
    r.fillCircle(x0 + dx * i / steps, y0 + dy * i / steps, rad);
  }
}

// An open eye: a rounded rect. `open` (0..256) squashes its height (blink).
void roundEye(Renderer& r, int ex, int ey, int w, int hMax, int rad, int open) {
  int h = hMax * open / 256;
  if (h < 3) h = 3;  // never fully vanish — leave a slit
  int rr = rad;
  if (rr > h / 2) rr = h / 2;
  if (rr > w / 2) rr = w / 2;
  if (rr < 1) rr = 1;
  r.fillRoundRect(ex - w / 2, ey - h / 2, w, h, rr);
}

// A curved eye: a thick parabola. depth > 0 curves up at the ends (a
// content "happy" eye); depth < 0 droops. `open` scales it for transitions.
void curveEye(Renderer& r, int ex, int ey, int halfW, int depth, int thick, int open) {
  int d = depth * open / 256;
  int th = thick * open / 256;
  if (open > 0 && th < 2) th = 2;
  if (th < 1 || halfW < 1) return;
  int hw2 = halfW * halfW;
  for (int dx = -halfW; dx <= halfW; dx++) {
    int yc = ey - d / 2 + d * (hw2 - dx * dx) / hw2;
    for (int t = 0; t < th; t++) r.drawPixel(ex + dx, yc - th / 2 + t);
  }
}

// An "X" eye (dead). `open` scales the arm length.
void xEye(Renderer& r, int ex, int ey, int size, int open) {
  int s = size * open / 256;
  if (s < 2) return;
  thickLine(r, ex - s, ey - s, ex + s, ey + s, 1);
  thickLine(r, ex - s, ey + s, ex + s, ey - s, 1);
}

// A heart (love eyes). `open` scales the whole shape.
void heart(Renderer& r, int hx, int hy, int size, int open) {
  int s = size * open / 256;
  if (s < 5) return;
  int lobeR = s * 28 / 100;
  int lobeDX = s * 32 / 100;  // lobes spread enough to leave the top dip
  int lobeDY = s * 14 / 100;
  if (lobeR < 1) lobeR = 1;
  r.fillCircle(hx - lobeDX, hy - lobeDY, lobeR);
  r.fillCircle(hx + lobeDX, hy - lobeDY, lobeR);
  int topY = hy - lobeDY;
  r.fillTriangle(hx - s * 56 / 100, topY, hx + s * 56 / 100, topY, hx, hy + s * 58 / 100);
}

// A curved mouth. depth > 0 = smile, depth < 0 = frown. `baseY` is the y
// of the mouth's outer ends.
void mouthCurve(Renderer& r, int mx, int baseY, int halfW, int depth, int thick) {
  if (halfW < 1) return;
  int hw2 = halfW * halfW;
  for (int dx = -halfW; dx <= halfW; dx++) {
    int y = baseY + depth * (hw2 - dx * dx) / hw2;
    for (int t = 0; t < thick; t++) r.drawPixel(mx + dx, y + t);
  }
}

// A short flat mouth.
void flatMouth(Renderer& r, int mx, int y, int halfW) {
  for (int dx = -halfW; dx <= halfW; dx++) {
    r.drawPixel(mx + dx, y);
    r.drawPixel(mx + dx, y + 1);
  }
}

// An open mouth (a filled blob).
void openMouth(Renderer& r, int mx, int my, int w, int h) {
  int rr = (w < h ? w : h) / 2;
  if (rr < 1) rr = 1;
  r.fillRoundRect(mx - w / 2, my - h / 2, w, h, rr);
}

// The letter "Z" (sleepy).
void drawZChar(Renderer& r, int x, int y, int sz) {
  thickLine(r, x, y, x + sz, y, 1);
  thickLine(r, x + sz, y, x, y + sz, 1);
  thickLine(r, x, y + sz, x + sz, y + sz, 1);
}

// A teardrop (sad).
void drawTear(Renderer& r, int x, int y, int s) {
  r.fillCircle(x, y, s);
  r.fillTriangle(x - s, y, x + s, y, x, y - s * 2);
}

// A blush patch — three short diagonal strokes (love).
void drawBlush(Renderer& r, int x, int y) {
  for (int i = 0; i < 3; i++) {
    int xx = x + i * 3;
    r.drawLine(xx, y + 2, xx + 3, y - 2);
  }
}

}  // namespace

// --- ProceduralFace ----------------------------------------------------

void ProceduralFace::reset(uint32_t now) {
  lastNow_ = now;
  blink_ = Blink::Idle;
  changing_ = false;
  eyeOpen_ = 256;
  gaze16_ = 0;
  gazeTickMs_ = now;
  onExpressionChanged_(now);
}

void ProceduralFace::setExpression(ExpressionId id, uint32_t now) {
  lastNow_ = now;
  if (!initialized_) {
    // First face after boot appears instantly — no blink-in.
    initialized_ = true;
    expr_ = id;
    pendingExpr_ = id;
    eyeOpen_ = 256;
    gaze16_ = 0;
    gazeTickMs_ = now;
    onExpressionChanged_(now);
    return;
  }
  // Blink-swap: close the eyes now, swap to `id` once they are shut.
  pendingExpr_ = id;
  changing_ = true;
  blink_ = Blink::Closing;
  blinkPhaseMs_ = now;
}

void ProceduralFace::blink(uint32_t now) {
  if (blink_ == Blink::Idle && !changing_ && exprBlinks(expr_)) {
    blink_ = Blink::Closing;
    blinkPhaseMs_ = now;
  }
}

void ProceduralFace::scheduleBlink_(uint32_t now) {
  int lo = 2600;
  int hi = 5200;
  if (expr_ == ExpressionId::Excited) {  // excited blinks eagerly
    lo = 900;
    hi = 2100;
  }
  nextBlinkMs_ = now + static_cast<uint32_t>(randRange(lo, hi));
}

void ProceduralFace::pickGaze_(uint32_t now) {
  if (gazeAmp_ <= 0) {
    gazeTarget16_ = 0;
    nextGazeMs_ = now + 2000;
    return;
  }
  // Deliberate spots, center weighted so the face mostly faces forward.
  const int spots[] = {-gazeAmp_, -gazeAmp_ / 2, 0, 0, 0, gazeAmp_ / 2, gazeAmp_};
  int prev = gazeTarget16_ / 16;
  int spot = prev;
  for (int tries = 0; tries < 6 && spot == prev; tries++) {
    spot = spots[randRange(0, 6)];
  }
  gazeTarget16_ = spot * 16;
  nextGazeMs_ = now + static_cast<uint32_t>(randRange(GAZE_MIN_HOLD_MS, GAZE_MAX_HOLD_MS));
}

void ProceduralFace::onExpressionChanged_(uint32_t now) {
  gazeAmp_ = gazeAmpFor(expr_);
  pickGaze_(now);
  scheduleBlink_(now);
}

void ProceduralFace::update(uint32_t now) {
  lastNow_ = now;

  // --- Blink / expression-swap state machine ------------------------
  switch (blink_) {
    case Blink::Idle:
      if (exprBlinks(expr_) && static_cast<int32_t>(now - nextBlinkMs_) >= 0) {
        blink_ = Blink::Closing;
        blinkPhaseMs_ = now;
      }
      break;
    case Blink::Closing: {
      uint32_t t = now - blinkPhaseMs_;
      if (t >= BLINK_CLOSE_MS) {
        eyeOpen_ = 0;
        blink_ = Blink::Opening;
        blinkPhaseMs_ = now;
        if (changing_) {  // eyes are shut — swap in the new expression
          expr_ = pendingExpr_;
          changing_ = false;
          onExpressionChanged_(now);
        }
      } else {
        eyeOpen_ = 256 - static_cast<int>(t * 256 / BLINK_CLOSE_MS);
      }
      break;
    }
    case Blink::Opening: {
      uint32_t t = now - blinkPhaseMs_;
      if (t >= BLINK_OPEN_MS) {
        eyeOpen_ = 256;
        blink_ = Blink::Idle;
        scheduleBlink_(now);
      } else {
        eyeOpen_ = static_cast<int>(t * 256 / BLINK_OPEN_MS);
      }
      break;
    }
  }

  // --- Gaze drift ---------------------------------------------------
  if (static_cast<int32_t>(now - nextGazeMs_) >= 0) pickGaze_(now);
  if (static_cast<int32_t>(now - gazeTickMs_) > 500) gazeTickMs_ = now - GAZE_TICK_MS;
  while (static_cast<int32_t>(now - gazeTickMs_) >= static_cast<int32_t>(GAZE_TICK_MS)) {
    gazeTickMs_ += GAZE_TICK_MS;
    gaze16_ += (gazeTarget16_ - gaze16_) / GAZE_EASE_DIV;
  }
}

void ProceduralFace::render(Renderer& r) {
  const int cx = OLED_W / 2;
  const int gx = (gaze16_ >= 0 ? gaze16_ + 8 : gaze16_ - 8) / 16;
  const uint32_t t = lastNow_;
  int elx = cx - EYE_DX + gx;  // left eye center x
  int erx = cx + EYE_DX + gx;  // right eye center x
  int mx = cx + gx;            // mouth center x

  switch (expr_) {
    case ExpressionId::Neutral: {
      roundEye(r, elx, EYE_CY, 20, 28, 9, eyeOpen_);
      roundEye(r, erx, EYE_CY, 20, 28, 9, eyeOpen_);
      mouthCurve(r, mx, 41, 11, 4, 2);
      break;
    }
    case ExpressionId::Happy: {
      int bob = wave(t, 1500, 2);
      int ey = EYE_CY + bob;
      curveEye(r, elx, ey, 11, 8, 5, eyeOpen_);
      curveEye(r, erx, ey, 11, 8, 5, eyeOpen_);
      mouthCurve(r, mx, 43 + bob, 12, 5, 2);
      break;
    }
    case ExpressionId::Sad: {
      int ey = EYE_CY + 3;
      roundEye(r, elx, ey, 18, 21, 8, eyeOpen_);
      roundEye(r, erx, ey, 18, 21, 8, eyeOpen_);
      // Sad brows: inner end high, outer end low.
      thickLine(r, elx - 11, ey - 8, elx + 8, ey - 13, 1);
      thickLine(r, erx - 8, ey - 13, erx + 11, ey - 8, 1);
      mouthCurve(r, mx, 52, 11, -5, 2);  // frown
      // A tear wells up and slides down, repeating.
      uint32_t tp = t % 4200;
      if (tp > 500 && tp < 3400) {
        int ty = ey + 7 + static_cast<int>(tp - 500) * 20 / 2900;
        drawTear(r, elx - 8, ty, 2);
      }
      break;
    }
    case ExpressionId::Sleepy: {
      int bob = wave(t, 3400, 2);  // slow breathing
      int ey = EYE_CY + 3 + bob;
      curveEye(r, elx, ey, 11, -3, 4, 256);  // half-shut, drooping
      curveEye(r, erx, ey, 11, -3, 4, 256);
      flatMouth(r, mx, 47 + bob, 5);
      // Two "Z"s drift up and to the right, then fade out.
      for (int k = 0; k < 2; k++) {
        uint32_t ph = (t + static_cast<uint32_t>(k) * 1400) % 2800;
        if (ph > 2300) continue;  // fade-out gap
        int prog = static_cast<int>(ph) * 100 / 2800;
        drawZChar(r, 86 + prog * 10 / 100, 24 - prog * 20 / 100, 5 + prog * 5 / 100);
      }
      break;
    }
    case ExpressionId::Excited: {
      int bob = wave(t, 240, 3);  // fast bounce
      int ey = EYE_CY - 1 + bob;
      roundEye(r, elx, ey, 22, 28, 10, eyeOpen_);
      roundEye(r, erx, ey, 22, 28, 10, eyeOpen_);
      openMouth(r, mx, 48 + bob, 15, 13);
      break;
    }
    case ExpressionId::Surprised: {
      int ey = EYE_CY - 2;
      roundEye(r, elx, ey, 24, 32, 11, eyeOpen_);  // wide-eyed
      roundEye(r, erx, ey, 24, 32, 11, eyeOpen_);
      openMouth(r, mx, 50, 9, 9);  // small "o"
      break;
    }
    case ExpressionId::Angry: {
      int shake = wave(t, 130, 1);  // tense tremble
      int el = elx + shake;
      int er = erx + shake;
      int ey = EYE_CY + 2;
      roundEye(r, el, ey, 20, 24, 8, eyeOpen_);
      roundEye(r, er, ey, 20, 24, 8, eyeOpen_);
      // Angry brows: inner end low, outer end high.
      thickLine(r, el - 11, ey - 14, el + 9, ey - 7, 2);
      thickLine(r, er - 9, ey - 7, er + 11, ey - 14, 2);
      mouthCurve(r, mx + shake, 52, 10, -4, 2);  // frown
      break;
    }
    case ExpressionId::Blink: {
      int bob = wave(t, 3800, 1);  // calm, eyes-closed resting
      int ey = EYE_CY + bob;
      curveEye(r, elx, ey, 10, 5, 4, 256);
      curveEye(r, erx, ey, 10, 5, 4, 256);
      mouthCurve(r, mx, 42 + bob, 7, 2, 2);
      break;
    }
    case ExpressionId::Love: {
      int bob = wave(t, 1500, 1);
      int pulse = wave(t, 820, 3);  // heartbeat
      int s = 18 + pulse;
      int ey = EYE_CY - 1 + bob;
      heart(r, elx, ey, s, eyeOpen_);
      heart(r, erx, ey, s, eyeOpen_);
      mouthCurve(r, mx, 46 + bob, 10, 4, 2);
      drawBlush(r, elx - 16, ey + 13);
      drawBlush(r, erx + 7, ey + 13);
      break;
    }
    case ExpressionId::Horny: {
      int bob = wave(t, 1100, 1);
      int wig = wave(t, 320, 2);   // tongue wiggle
      int lid = wave(t, 2600, 2);  // slow, sultry lid droop
      int ey = EYE_CY + bob;
      // Heavy-lidded "bedroom" eyes: a round eye with its top erased to
      // leave a flat-topped slit, finished with a thick eyelid line.
      roundEye(r, elx, ey, 20, 24, 8, eyeOpen_);
      roundEye(r, erx, ey, 20, 24, 8, eyeOpen_);
      int lidH = 11 + lid;
      r.clearRect(elx - 11, ey - 13, 22, lidH);
      r.clearRect(erx - 11, ey - 13, 22, lidH);
      int ly = ey - 13 + lidH;
      thickLine(r, elx - 9, ly, elx + 9, ly, 1);
      thickLine(r, erx - 9, ly, erx + 9, ly, 1);
      // A smirk with a tongue lolling out and wiggling.
      mouthCurve(r, mx, 43 + bob, 9, 3, 2);
      openMouth(r, mx + wig, 50 + bob, 8, 9);
      drawBlush(r, elx - 16, ey + 16);
      drawBlush(r, erx + 7, ey + 16);
      break;
    }
    case ExpressionId::Shy: {
      int bob = wave(t, 1700, 1);    // gentle, timid sway
      int flush = wave(t, 1900, 1);  // the blush flushing
      int ey = EYE_CY + 1 + bob;
      // Small, timid eyes and a tiny modest smile.
      roundEye(r, elx, ey, 15, 17, 6, eyeOpen_);
      roundEye(r, erx, ey, 15, 17, 6, eyeOpen_);
      mouthCurve(r, mx, 42 + bob, 6, 2, 2);
      // A big bashful blush — four diagonal strokes per cheek.
      int by = ey + 12;
      for (int i = 0; i < 4; i++) {
        int xl = elx - 19 + i * 4;
        int xr = erx + 7 + i * 4;
        r.drawLine(xl, by + 3 + flush, xl + 5, by - 3 - flush);
        r.drawLine(xr, by + 3 + flush, xr + 5, by - 3 - flush);
      }
      break;
    }
    case ExpressionId::Dead: {
      xEye(r, elx, EYE_CY, 9, eyeOpen_);
      xEye(r, erx, EYE_CY, 9, eyeOpen_);
      flatMouth(r, mx, 49, 7);
      break;
    }
    case ExpressionId::Count:
      break;
  }
}
