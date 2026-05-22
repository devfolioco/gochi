// firmware.ino — Tamagotchi firmware entry point (ESP32-C3 SuperMini).
//
// Desktop Mode: the pet shows whatever view and expression it is told to
// over the USB serial protocol (see src/command.h), and the buzzer plays
// a short jingle synced to each expression. Drive it from a serial
// monitor at 115200 baud — try "LIST faces". See firmware/README.md.

#include "src/assets/expressions.h"
#include "src/assets/jingles.h"
#include "src/buzzer/buzzer.h"
#include "src/command.h"
#include "src/config.h"
#include "src/modes/desktop_mode.h"
#include "src/modes/mode.h"
#include "src/renderer.h"
#include "src/transport.h"
#include "src/views/view_manager.h"

static Renderer renderer;
static Transport transport;
static ViewManager viewManager;
static DesktopMode desktopMode(transport, renderer);

// Only Desktop Mode exists for now. This Mode* indirection is the seam
// where Free Mode will slot in later as a second mode.
static Mode* currentMode = &desktopMode;

// Last expression the buzzer reacted to — see loop().
static ExpressionId buzzedExpr = ExpressionId::Count;

void setup() {
  transport.begin(115200);
  renderer.init();
  buzzer::begin();

  // Buttons are wired but unused — configured so the pins start known.
  pinMode(PIN_BTN_A, INPUT_PULLUP);
  pinMode(PIN_BTN_B, INPUT_PULLUP);
  pinMode(PIN_BTN_C, INPUT_PULLUP);

  currentMode->onEnter(viewManager);
  transport.println("Tamagotchi ready (Desktop Mode). Try: LIST faces");
}

void loop() {
  uint32_t now = millis();

  Command cmd;
  if (transport.poll(cmd)) currentMode->onCommand(cmd, viewManager);

  currentMode->update(now, viewManager);

  // Buzzer synced to the face: whenever the displayed expression changes
  // — whatever triggered it — play that expression's jingle.
  ExpressionId expr = viewManager.face().expression();
  if (expr != buzzedExpr) {
    buzzedExpr = expr;
    Jingle jingle = jingleFor(expr);
    buzzer::play(jingle.tones, jingle.count);
  }
  buzzer::update(now);

  renderer.beginFrame();
  viewManager.tick(now, renderer);
  renderer.endFrame();
}
