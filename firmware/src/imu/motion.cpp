// motion.cpp — gesture detector (see motion.h).
//
// The math is deliberately cheap — this runs on every loop tick alongside
// the renderer. No FFTs, no allocations, no floats beyond what the sample
// already carries.
//
// Pickup detector
//   At rest the accelerometer reads ~1.0 g on whichever axis is "down".
//   When you grab the pet and lift, the upward acceleration adds to gravity
//   and |a| spikes above 1.0 g for as long as you keep accelerating it.
//   We low-pass |a| with a single-pole IIR (alpha = 0.2) and fire once
//   the smoothed magnitude stays above PICKUP_G for PICKUP_HOLD_MS.
//
// Shake detector
//   Shaking produces alternating large positive/negative peaks on the
//   axis being waggled. We track the most recent peak's sign on each
//   axis, and count zero-crossings where the peak magnitude exceeds
//   SHAKE_PEAK_G. Four crossings within SHAKE_WINDOW_MS counts as a shake.
//
// Both detectors honour a shared refractory period — once any event
// fires, none fire again for REFRACTORY_MS. That way one vigorous shake
// reads as one Shake, and a pickup-and-jostle reads as a single Pickup
// rather than Pickup → Shake → Pickup.

#include "motion.h"

#include <math.h>

namespace {

// Thresholds — tuned by hand on a desk. Easy to expose later.
//
// Shake is checked *before* pickup because a real shake also satisfies
// pickup conditions (|a| sustained > 1.25 g over 250 ms), so without
// this ordering pickup would always win and shake would never fire.
constexpr float PICKUP_G = 1.25f;
constexpr uint32_t PICKUP_HOLD_MS = 250;

// Shake: oscillating accel with sign flips on any axis. Softened from
// the original (1.8 g / 4 flips / 600 ms) — gentle wiggling now fires.
// SHAKE_PEAK_G is intentionally close to PICKUP_G (1.25 g); the two
// stay distinct because pickup needs *sustained* low-pass magnitude
// while shake needs *oscillating* peaks with sign flips.
constexpr float SHAKE_PEAK_G = 0.6f;
constexpr uint32_t SHAKE_WINDOW_MS = 800;
constexpr uint8_t SHAKE_CROSSINGS = 3;

constexpr uint32_t REFRACTORY_MS = 1500;

constexpr float SMOOTH_ALPHA = 0.2f;  // 0..1, higher = less smoothing

// --- State ------------------------------------------------------------

float magSmoothed = 1.0f;      // low-passed |a| in g
uint32_t pickupHighSince = 0;  // first ms |a| crossed PICKUP_G
uint32_t lastEventMs = 0;      // last fired event (for refractory)

// Per-axis shake bookkeeping. lastSign is +1/-1 once a peak is seen.
struct AxisState {
  int8_t lastSign = 0;
  uint8_t crossings = 0;
  uint32_t firstCrossingMs = 0;
};
AxisState axes[3];

inline AxisState& axisX() { return axes[0]; }
inline AxisState& axisY() { return axes[1]; }
inline AxisState& axisZ() { return axes[2]; }

// Update one axis with its current acceleration (in g) and return true
// if this axis has accumulated enough crossings within the window.
bool feedAxis(AxisState& a, float val, uint32_t now) {
  // Forget stale crossings — keeps a slow tap-tap-...-pause-tap from
  // falsely triggering a shake five seconds later.
  if (a.crossings > 0 && now - a.firstCrossingMs > SHAKE_WINDOW_MS) {
    a.crossings = 0;
    a.lastSign = 0;
  }

  if (fabsf(val) < SHAKE_PEAK_G) return false;
  int8_t sign = (val > 0) ? 1 : -1;
  if (a.lastSign == 0) {
    a.lastSign = sign;
    return false;
  }
  if (sign == a.lastSign) return false;

  // Sign flipped while magnitude was above threshold → one crossing.
  a.lastSign = sign;
  if (a.crossings == 0) a.firstCrossingMs = now;
  ++a.crossings;
  return a.crossings >= SHAKE_CROSSINGS;
}

void resetShakeState() {
  for (AxisState& a : axes) {
    a.lastSign = 0;
    a.crossings = 0;
  }
}

}  // namespace

namespace motion {

void begin() {
  magSmoothed = 1.0f;
  pickupHighSince = 0;
  lastEventMs = 0;
  resetShakeState();
}

Event update(const imu::Sample& s, uint32_t now) {
  // Refractory: stay quiet for a bit after any event so a single gesture
  // fires once, not a barrage.
  if (lastEventMs != 0 && now - lastEventMs < REFRACTORY_MS) {
    // Keep the smoother warm so we don't fire spuriously on exit.
    float mag = sqrtf(s.accel.x * s.accel.x + s.accel.y * s.accel.y + s.accel.z * s.accel.z);
    magSmoothed += SMOOTH_ALPHA * (mag - magSmoothed);
    pickupHighSince = 0;
    return Event::None;
  }

  // Keep the smoother warm regardless of which detector wins below.
  float mag = sqrtf(s.accel.x * s.accel.x + s.accel.y * s.accel.y + s.accel.z * s.accel.z);
  magSmoothed += SMOOTH_ALPHA * (mag - magSmoothed);

  // --- Shake (per-axis zero-crossings of large peaks) ---
  // Checked first because a vigorous shake also satisfies pickup
  // conditions; checking pickup first would always win.
  bool shookX = feedAxis(axisX(), s.accel.x, now);
  bool shookY = feedAxis(axisY(), s.accel.y, now);
  // Z carries gravity; subtract a static 1 g so we react to lateral
  // shake on z too without gravity counting as a permanent positive peak.
  bool shookZ = feedAxis(axisZ(), s.accel.z - 1.0f, now);
  if (shookX || shookY || shookZ) {
    lastEventMs = now;
    resetShakeState();
    pickupHighSince = 0;
    return Event::Shake;
  }

  // --- Pickup (low-passed |a|) ---
  if (magSmoothed > PICKUP_G) {
    if (pickupHighSince == 0) pickupHighSince = now;
    if (now - pickupHighSince >= PICKUP_HOLD_MS) {
      pickupHighSince = 0;
      lastEventMs = now;
      resetShakeState();
      return Event::Pickup;
    }
  } else {
    pickupHighSince = 0;
  }

  return Event::None;
}

}  // namespace motion
