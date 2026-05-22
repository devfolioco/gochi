# Tamagotchi Firmware — Phase 1 (Desktop Mode)

Firmware for an ESP32-C3 SuperMini virtual pet. A 128x64 SSD1306 OLED
shows an animated face. In Phase 1 the pet runs in **Desktop Mode**: it
passively displays whatever view and expression it is told to over the USB
serial link.

Everything is exercised from a serial monitor at **115200 baud**.

## Serial command protocol

One command per line. Responses come back one per line — `OK`,
`ERR <reason>`, or a data payload. Command keywords are case-insensitive.

| Command              | Effect                                         | Response                          |
| -------------------- | ---------------------------------------------- | --------------------------------- |
| `SHOW face <name>`   | Switch to the face view, set the expression    | `OK` / `ERR unknown face`         |
| `SHOW text <string>` | Switch to the text view (`<string>` = rest)    | `OK`                              |
| `SET mood <name>`    | Phase 1: accepted and acked, no behavior yet   | `OK`                              |
| `GET state`          | Query the current view + expression            | `{"view":"face","expr":"happy"}`  |
| `GET fps`            | Query the current display frame rate           | `fps 38`                          |
| `LIST faces`         | List all expression names                      | `neutral,happy,sad,...`           |
| `PING`               | Liveness check                                 | `PONG`                            |

Anything unrecognized returns `ERR unknown command`.

Expressions: `neutral`, `happy`, `sad`, `sleepy`, `excited`, `surprised`,
`angry`, `blink`, `love`, `dead`.

Notes:
- The `neutral` face is drawn procedurally — minimal rounded-rectangle
  eyes and a soft smile — and is kept alive: it blinks on its own and
  drifts gently left/right as if glancing around. The other expressions
  are still sprite-based (they will migrate to the procedural face).
- Changing the expression plays a short eyelid-blink transition; the new
  face is swapped in while the eyes are shut.
- Multi-frame sprite expressions animate (e.g. `happy` blinks its eyes,
  `sleepy` floats a `Z`, `excited` bobs).
- Text wider than the screen scrolls as a marquee; shorter text is
  centered.
- The text payload is limited to 63 characters.

## Manual test checklist

1. **Boot** — the OLED shows the `neutral` face.
2. `LIST faces` — prints all 10 expression names.
3. `SHOW face happy` — blinks into the happy face; it animates; `OK`.
4. `SHOW face sleepy` — blinks into the sleepy face.
5. `SHOW text hello there` — switches to the text view; long text scrolls;
   `OK`.
6. `GET state` — prints the current state JSON.
7. `PING` — prints `PONG`.
8. `SHOW face banana` — prints `ERR unknown face`.
9. **Idle fallback** — leave it untouched on a face for ~30 s; the pet
   blinks on its own so the display never looks frozen.

## Source layout

```
firmware.ino              entry point — wiring + setup()/loop()
src/config.h              pin map and panel geometry
src/command.{h,cpp}       line protocol + parser (shared with Phase 2)
src/transport.{h,cpp}     buffered serial line I/O
src/renderer.{h,cpp}      frame-oriented drawing surface
src/display/              U8g2-backed SSD1306 driver wrapper
src/views/                View interface, FaceView, TextView, ViewManager
src/modes/                Mode interface + DesktopMode
src/assets/expressions.*  expression registry + placeholder sprites
```

## Beyond Phase 1

- **Free Mode** — an autonomous battery mode — will be added as a second
  `Mode` (sibling to `DesktopMode`); no interface changes needed.
- **Phase 2 host tooling** — a CLI and server — will speak this exact
  serial protocol, so the firmware needs no protocol changes.
- Placeholder face art can be replaced with custom sprites; see the
  image2cpp workflow documented at the top of `src/assets/expressions.h`.
