# Hardware bring-up tests

Tiny, single-purpose Arduino sketches that exercise one peripheral at a
time on the same ESP32-C3 SuperMini the tamagotchi runs on. Useful when
bringing up a fresh board or chasing a hardware fault: if the main
firmware misbehaves you can rule out one component at a time without
the rest of the codebase getting in the way.

Each sketch:

- lives in its own folder so `arduino-cli` can build it on its own,
- pulls its pin numbers from the main firmware's `src/config.h` so the
  wiring stays in sync with the real device — change a pin there and
  the tests follow,
- ships its own `sketch.yaml` pinned to the same board + library
  versions as the main firmware.

## Sketches

| Folder     | What it does                                                       |
|------------|--------------------------------------------------------------------|
| `led/`     | Blinks the on-board LED (`LED_BUILTIN`) at 1 Hz.                   |
| `oled/`    | Brings up the SSD1306 on `PIN_SDA`/`PIN_SCL` and cycles text.      |
| `buzzer/`  | Plays a short scale on the passive piezo on `PIN_BUZZER`.          |
| `mpu/`     | Streams MPU-6050 accel + gyro samples; pair with `visualize.mjs`.  |

## Building & flashing

From the repo root, via the Makefile wrapper:

```sh
make test-led       # compile + flash to the board
make test-oled
make test-buzzer
make test-mpu       # flashes the IMU streaming sketch AND opens the viewer
```

Override the serial port with `PORT=/dev/cu.usbmodemXXXX`, same as the
main `make flash`.

## Visualising the MPU-6050

`make test-mpu` flashes the streaming sketch and then opens
`firmware/tests/mpu/visualize.html` in your default browser in one go.
The page is a self-contained single file (no build, no deps) that uses
the **Web Serial API** to talk to the board directly from the browser.
Click **Connect…**, pick the `/dev/cu.usbmodemXXXX` port, and you get:

- a top-down SVG plane in a CSS-3D perspective scene that **banks with
  roll** and **pitches forward/back** as you tilt the board,
- live numeric values + bipolar bars for gyro x/y/z (°/s) and accel
  x/y/z (g),
- derived pitch / roll readouts (accel-only, low-pass filtered so the
  plane doesn't jitter on raw noise),
- sample rate in the footer.

Browser support: **Chrome or Edge** (Web Serial isn't shipped in Firefox
or Safari). If the `gochi` daemon is holding the port, run `gochi stop`
first or the Connect dialog will report "device busy".

Or invoke `arduino-cli` directly from any sketch folder:

```sh
arduino-cli compile --profile c3
arduino-cli upload  --profile c3 --port /dev/cu.usbmodem<…>
```
