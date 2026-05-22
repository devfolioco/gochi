# Tamagotchi Firmware — Desktop Mode

Firmware for an ESP32-C3 SuperMini virtual pet. A 128x64 SSD1306 OLED
shows an animated face and a piezo buzzer gives it a voice. The pet runs
in **Desktop Mode**: it displays whatever view and expression it is told
to over the USB serial link.

## Serial command protocol

One command per line, at **115200 baud**. Responses come back one per
line — `OK`, `ERR <reason>`, or a data payload. Keywords are
case-insensitive.

| Command              | Effect                                       | Response                         |
| -------------------- | -------------------------------------------- | -------------------------------- |
| `SHOW face <name>`   | Switch to the face view, set the expression  | `OK` / `ERR unknown face`        |
| `SHOW text <string>` | Switch to the text view (`<string>` = rest)  | `OK`                             |
| `SET mood <name>`    | Accepted and acked, no behavior yet          | `OK`                             |
| `GET state`          | Query the current view + expression          | `{"view":"face","expr":"happy"}` |
| `GET fps`            | Query the current display frame rate         | `fps 38`                         |
| `LIST faces`         | List all expression names                    | `neutral,happy,sad,...`          |
| `PING`               | Liveness check                               | `PONG`                           |

Anything unrecognized returns `ERR unknown command`.

Expressions: `neutral`, `happy`, `sad`, `sleepy`, `excited`, `surprised`,
`angry`, `blink`, `love`, `horny`, `shy`, `dead`.

## The face

Every expression is drawn procedurally — no sprite bitmaps — so each one
animates and stays alive: the eyes blink, the gaze drifts left/right, and
each expression has its own idle motion (a sliding tear, floating `Z`s, a
bob, a tremble, pulsing hearts, ...). Changing expression plays a blink —
the eyes close, the new face swaps in, the eyes reopen.

Text wider than the screen scrolls as a marquee; shorter text is centered
(payload limited to 63 characters).

## The buzzer

A passive piezo buzzer on **GPIO10** gives the pet a voice: whenever the
displayed expression changes, it plays a short jingle matching that face
— a rising run for `happy`, a descending sigh for `sad`, an agitated
trill for `angry`, a wolf-whistle for `horny`, a "game over" slide for
`dead`, and so on. Playback is non-blocking (LEDC-driven).

## Manual test checklist

1. **Boot** — the OLED shows the `neutral` face; a short blip plays.
2. `LIST faces` — prints all 12 expression names.
3. `SHOW face happy` — blinks into the happy face; its jingle plays; `OK`.
4. `SHOW face love` — heart eyes, blush, a pulsing beat + sweet tune.
5. `SHOW text hello there` — switches to the text view; long text scrolls.
6. `GET state` — prints the current state JSON.
7. `PING` — prints `PONG`.
8. `SHOW face banana` — prints `ERR unknown face`.
9. **Idle** — leave it on a face; it blinks and looks around on its own.

## Source layout

```
firmware.ino                 entry point — wiring + setup()/loop()
src/config.h                 pin map and panel geometry
src/command.{h,cpp}          line protocol + parser
src/transport.{h,cpp}        buffered serial line I/O
src/renderer.{h,cpp}         frame-oriented drawing surface
src/display/                 U8g2-backed SSD1306 driver (hardware I2C)
src/buzzer/                  non-blocking piezo tone-sequence player
src/views/                   View interface, FaceView, TextView, ViewManager
src/views/procedural_face.*  the procedural face — all 12 expressions
src/modes/                   Mode interface + DesktopMode
src/assets/expressions.*     expression id/name registry
src/assets/jingles.*         a buzzer jingle per expression
```

## What's next

- **Free Mode** — an autonomous mode — will be added as a second `Mode`
  (sibling to `DesktopMode`); the `Mode` interface already supports it.
- A host CLI / server can drive the device over this same serial protocol.
