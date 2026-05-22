// config.h — ESP32-C3 SuperMini pin map and panel geometry.
//
// Single source of truth for pins and display constants. Nothing else in
// the firmware should hard-code a GPIO number or panel dimension.
#pragma once

// --- I2C OLED (SSD1306, driven over software I2C by U8g2) --------------
#define PIN_SDA 5
#define PIN_SCL 6

// --- Push buttons (active-low, wired to GND, internal pull-ups) --------
// NOTE: GPIO2 is an ESP32-C3 strapping pin. With INPUT_PULLUP it idles
// HIGH, which is the level the bootloader expects, so BTN_A is safe as
// long as the button is not held down during power-up / flashing. Move it
// to a free GPIO if that ever causes boot trouble. GPIO3 and GPIO4 are not
// strapping pins. (Buttons are wired but unused in Phase 1.)
#define PIN_BTN_A 2
#define PIN_BTN_B 3
#define PIN_BTN_C 4

// --- Passive piezo buzzer (unused in Phase 1) --------------------------
#define PIN_BUZZER 10

// --- OLED panel --------------------------------------------------------
// U8g2's NONAME constructor already targets I2C address 0x3C; OLED_ADDR is
// kept for reference and for any future driver swap.
#define OLED_ADDR 0x3C
#define OLED_W 128
#define OLED_H 64
