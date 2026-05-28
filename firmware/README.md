# Tamagotchi Firmware

Firmware for an ESP32-C3 SuperMini virtual pet. A 128x64 SSD1306 OLED
shows an animated face and a piezo buzzer gives it a voice.

## Modes

The pet has two modes and switches between them on its own:

- **Free Mode** — the default; it boots into this. The pet runs
  autonomously: it wanders through *moods* and shows matching expressions,
  with no host needed.
- **Desktop Mode** — entered the moment any serial command arrives. The
  pet shows exactly what it is told.

After ~60 s with no command, Desktop Mode drifts back to Free Mode. (To
return to Free Mode immediately, just reboot — it boots into Free Mode.)

## BOOT button

The on-board **BOOT button** (GPIO9) cycles to the next expression on each
tap, stepping through all 12 faces. A tap also hands control to Desktop
Mode (so the choice sticks), so the pet drifts back to Free Mode ~60 s
after the last tap. Don't *hold* the button while powering on — that puts
the chip in flash-download mode.

## Serial command protocol

One command per line, at **115200 baud**. Responses come back one per
line — `OK`, `ERR <reason>`, or a data payload. Keywords are
case-insensitive. Receiving any command switches the pet to Desktop Mode.

| Command              | Effect                                       | Response                         |
| -------------------- | -------------------------------------------- | -------------------------------- |
| `SHOW face <name>`   | Switch to the face view, set the expression  | `OK` / `ERR unknown face`        |
| `SHOW text <string>` | Switch to the text view (`<string>` = rest)  | `OK`                             |
| `SHOW image <b64>`   | Push a 128×64 1bpp frame, MSB-first packed   | `OK` / `ERR bad image base64`    |
| `SET mood <name>`    | Set the pet's mood (drives Free Mode)        | `OK` / `ERR unknown mood`        |
| `GET state`          | Query the current view + expression          | `{"view":"face","expr":"happy"}` |
| `GET fps`            | Query the current display frame rate         | `fps 38`                         |
| `LIST faces`         | List all expression names                    | `neutral,happy,sad,...`          |
| `PING`               | Liveness check                               | `PONG`                           |
| `SCAN i2c`           | Enumerate every device on both I2C buses     | `{"A":["0x3C"],"B":["0x68"]}`    |

Anything unrecognized returns `ERR unknown command`.

Expressions: `neutral`, `happy`, `sad`, `sleepy`, `excited`, `surprised`,
`angry`, `blink`, `love`, `sexy`, `shy`, `dead`.

Moods: `content`, `playful`, `grumpy`, `sleepy`, `affectionate`.

## Free Mode

Each mood has a pool of expressions the pet tends to show:

| Mood         | Tends to show               |
| ------------ | --------------------------- |
| Content      | neutral, happy, blink       |
| Playful      | happy, excited, surprised   |
| Grumpy       | angry, sad, neutral         |
| Sleepy       | sleepy, blink, neutral      |
| Affectionate | love, happy, shy            |

It picks a fresh expression every ~10–17 s, and every ~45–90 s the mood
may drift to a new one (a gentle random walk, with `content` as home).
`SET mood` sets the mood; it keeps drifting afterward. `sexy` and `dead`
are kept out of the autonomous rotation — they are manual-only.

## The image view

`SHOW image <base64>` paints a host-supplied 128×64 1-bit bitmap on the
OLED — 1024 bytes of pixels (16 bytes per row, MSB = leftmost pixel),
base64-encoded. The bitmap stays on screen until the next view command.
The CLI's `tamagotchi image <file>` does the resize / dither / pack /
encode automatically.

## The face

Every expression is drawn procedurally — no sprite bitmaps — so each one
animates: the eyes blink, the gaze drifts, and each expression has its
own idle motion (a sliding tear, floating `Z`s, a tremble, pulsing
hearts, ...). Changing expression plays a blink — eyes close, the new
face swaps in, eyes reopen. Text wider than the screen scrolls.

## The buzzer

A passive piezo buzzer on **GPIO10** plays a short jingle matching the
face. In Desktop Mode every expression change jingles; in Free Mode it
stays quieter — a jingle plays only when the mood shifts. Non-blocking
(LEDC-driven).

## The IMU (MPU-6050)

A 6-axis MPU-6050 (the GY-521 module works) shares the OLED's I2C bus
and lets the pet react to being handled:

| MPU pin  | Board                          |
| -------- | ------------------------------ |
| VCC      | **3V3** (not 5V)               |
| GND      | GND                            |
| SDA      | GPIO5 (same as OLED)           |
| SCL      | GPIO6 (same as OLED)           |
| AD0      | leave floating → I2C addr 0x68 |
| XDA/XCL/INT | unused                      |

Two gestures are detected, each treated like a BOOT-button tap — they
hand control to Desktop Mode, snap to the face view, and the pet drifts
back to Free Mode ~60 s later:

- **Pickup** — sustained |a| above ~1.25 g for 250 ms (a brisk lift)
  → **`surprised`** expression. The pet looks anxious about being held.
- **Shake** — rapid back-and-forth, ≥4 large peaks within 600 ms on any
  axis → **`sad`** expression. The pet cries (it has a tear animation).

After any event the detector goes quiet for 1.5 s, so a single gesture
fires once instead of a burst. If the MPU-6050 isn't connected, the
boot log prints `IMU: not detected — motion disabled` and the rest of
the pet behaves exactly as before.

The default I2C address is `0x68`. If your breakout ties **AD0 HIGH**,
edit `MPU_ADDR` in `src/config.h` to `0x69` and reflash. To verify the
sensor end-to-end, run `gochi test imu`.

## Manual test checklist

1. **Boot** — the OLED comes up in Free Mode; the face changes on its own.
2. `SHOW face happy` — switches to Desktop Mode, blinks into happy; `OK`.
3. `SHOW face love` — heart eyes, blush, a pulsing beat + jingle.
4. `SET mood playful` — `OK`; the mood is set for when Free Mode resumes.
5. `GET state` / `GET fps` / `PING` — query responses.
6. `SHOW face banana` — `ERR unknown face`.
7. **BOOT button** — tap it; the face steps to the next expression.
8. **Idle** — stop sending commands; after ~60 s it returns to Free Mode.

## Source layout

```
firmware.ino                 entry point — wiring, mode supervisor
src/config.h                 pin map and panel geometry
src/mood.{h,cpp}             the pet's mood (shared state)
src/command.{h,cpp}          line protocol + parser
src/transport.{h,cpp}        buffered serial line I/O
src/renderer.{h,cpp}         frame-oriented drawing surface
src/display/                 U8g2-backed SSD1306 driver (hardware I2C)
src/buzzer/                  non-blocking piezo tone-sequence player
src/imu/                     MPU-6050 driver + pickup / shake detector
src/views/                   View interface, FaceView, TextView, ImageView, ViewManager
src/views/procedural_face.*  the procedural face — all 12 expressions
src/modes/                   Mode interface, DesktopMode, FreeMode
src/assets/expressions.*     expression id/name registry
src/assets/jingles.*         a buzzer jingle per expression
```
