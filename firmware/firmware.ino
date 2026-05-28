// firmware.ino — Tamagotchi firmware entry point (ESP32-C3 SuperMini).
//
// The pet boots into Desktop Mode (Free Mode is currently disabled — flip
// FREE_MODE_ENABLED to 1 to restore autonomous expression/mood drift).
// It reacts to: serial commands, BOOT-button taps, and IMU motion
// (pickup → surprised, shake → sad). Motion-triggered faces auto-revert
// to neutral after IMU_FACE_HOLD_MS. The buzzer jingles on every face
// change in Desktop Mode. See firmware/README.

#include "src/assets/expressions.h"
#include "src/assets/jingles.h"
#include "src/buzzer/buzzer.h"
#include "src/command.h"
#include "src/config.h"
#include "src/imu/motion.h"
#include "src/imu/mpu6050.h"
#include "src/modes/desktop_mode.h"
#include "src/modes/free_mode.h"
#include "src/modes/mode.h"
#include "src/mood.h"
#include "src/renderer.h"
#include "src/transport.h"
#include "src/views/view_manager.h"

// Flip to 1 to re-enable Free Mode: the pet wanders through moods and
// expressions on its own, and Desktop Mode drifts back here after ~60 s
// idle. Disabled for now — the pet sits in Desktop Mode forever, showing
// only what host commands / BOOT taps / IMU gestures put on it.
#define FREE_MODE_ENABLED 0

// The pet's mood — shared between the modes: SET mood writes it, Free Mode
// reads and slowly evolves it. RAM-only (a reboot resets it to content).
static Mood petMood = Mood::Content;

static Renderer renderer;
static Transport transport;
static ViewManager viewManager;
static DesktopMode desktopMode(transport, renderer, petMood);
#if FREE_MODE_ENABLED
static FreeMode freeMode(petMood);
#endif

// Default mode: Free if enabled (the pet lives on its own), otherwise
// Desktop (the pet only reacts to commands / buttons / gestures).
#if FREE_MODE_ENABLED
static Mode* currentMode = &freeMode;
#else
static Mode* currentMode = &desktopMode;
#endif
static uint32_t lastCmdMs = 0;  // time of the last command
static ExpressionId jingledExpr = ExpressionId::Count;

// No serial command for this long, while in Desktop Mode, drifts to Free.
// Unused when FREE_MODE_ENABLED is 0 — kept here so the constant survives
// the flip.
static const uint32_t IDLE_TO_FREE_MS = 60000;

// IMU-triggered faces (surprised / sad) auto-revert to neutral after
// this long, so the pet "calms down" instead of staying anxious or sad
// forever. Only takes effect if the face hasn't been changed by anything
// else in the meantime — a serial command or BOOT-button tap during the
// hold cancels the auto-revert.
static const uint32_t IMU_FACE_HOLD_MS = 5000;
static uint32_t imuFaceExpiryMs = 0;                   // 0 = no pending revert
static ExpressionId imuSetFace = ExpressionId::Count;  // what motion last set

// Hand off between modes via the Mode onExit/onEnter hooks.
static void setMode(Mode& mode) {
  if (currentMode == &mode) return;
  currentMode->onExit();
  currentMode = &mode;
  currentMode->onEnter(viewManager);
}

// Debounced BOOT-button (GPIO9) press detector. Returns true once on each
// release->press edge. Active-low: LOW means pressed.
static bool bootButtonPressed(uint32_t now) {
  static bool raw = false;
  static bool stable = false;
  static uint32_t changeMs = 0;
  bool reading = digitalRead(PIN_BTN_BOOT) == LOW;
  if (reading != raw) {
    raw = reading;
    changeMs = now;
  }
  if (now - changeMs >= 25 && reading != stable) {
    stable = reading;
    if (stable) return true;  // settled into "pressed"
  }
  return false;
}

void setup() {
  transport.begin(115200);
  renderer.init();
  buzzer::begin();

  // Bring up the IMU on its bit-banged I2C bus. A false return just
  // means the MPU isn't connected — the rest of the pet boots fine and
  // imu::isReady() will keep the motion path skipped.
  if (imu::begin()) {
    motion::begin();
  }

  // The on-board BOOT button cycles expressions.
  pinMode(PIN_BTN_BOOT, INPUT_PULLUP);

  currentMode->onEnter(viewManager);
#if FREE_MODE_ENABLED
  transport.println("Tamagotchi ready (Free Mode). Send any command for Desktop Mode.");
#else
  transport.println("Tamagotchi ready (Desktop Mode — Free Mode disabled).");
#endif
}

void loop() {
  uint32_t now = millis();

  Command cmd;
  if (transport.poll(cmd)) {
    lastCmdMs = now;
    setMode(desktopMode);  // any command means a host is driving the pet
    currentMode->onCommand(cmd, viewManager);
  }

  // BOOT button: a tap cycles to the next expression (and, like a
  // command, hands control to Desktop Mode).
  if (bootButtonPressed(now)) {
    lastCmdMs = now;
    setMode(desktopMode);
    uint8_t next = (static_cast<uint8_t>(viewManager.face().expression()) + 1) % expressionCount();
    viewManager.face().setExpression(static_cast<ExpressionId>(next));
  }

  // IMU: pickup → anxious-looking `surprised`; shake → `sad` (cries).
  // Acts like a BOOT-button tap: hands control to Desktop Mode, resets
  // the idle timer, snaps to the face view so the reaction is always
  // visible even if a text/image view was up.
  if (imu::isReady()) {
    imu::Sample s;
    if (imu::read(s)) {
      motion::Event ev = motion::update(s, now);
      if (ev != motion::Event::None) {
        lastCmdMs = now;
        setMode(desktopMode);
        viewManager.setView(&viewManager.face());
        ExpressionId target =
            (ev == motion::Event::Pickup) ? ExpressionId::Surprised : ExpressionId::Sad;
        viewManager.face().setExpression(target);
        // Schedule the calm-down: in IMU_FACE_HOLD_MS we'll revert to
        // neutral, unless something else has changed the face by then.
        imuSetFace = target;
        imuFaceExpiryMs = now + IMU_FACE_HOLD_MS;
      }
    }
  }

  // Calm-down timer: if motion set the face and the hold has elapsed,
  // revert to neutral. The `current face still matches` guard means a
  // serial command or BOOT-button tap during the hold cancels the
  // revert — we don't overwrite the user's choice with neutral.
  if (imuFaceExpiryMs != 0 && (int32_t)(now - imuFaceExpiryMs) >= 0) {
    imuFaceExpiryMs = 0;
    if (viewManager.face().expression() == imuSetFace) {
      viewManager.face().setExpression(ExpressionId::Neutral);
    }
    imuSetFace = ExpressionId::Count;
  }

#if FREE_MODE_ENABLED
  if (currentMode == &desktopMode && now - lastCmdMs >= IDLE_TO_FREE_MS) {
    setMode(freeMode);  // gone idle — let the pet live on its own
  }
#endif

  currentMode->update(now, viewManager);

  // Buzzer synced to the face. In Desktop Mode every expression change
  // jingles; Free Mode stays quieter and plays its own jingle only on
  // mood shifts, so it is not gated in here.
  ExpressionId expr = viewManager.face().expression();
  if (expr != jingledExpr) {
    jingledExpr = expr;
    if (currentMode == &desktopMode) {
      Jingle jingle = jingleFor(expr);
      buzzer::play(jingle.tones, jingle.count);
    }
  }
  buzzer::update(now);

  renderer.beginFrame();
  viewManager.tick(now, renderer);
  renderer.endFrame();
}
