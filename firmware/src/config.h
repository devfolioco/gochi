// config.h — ESP32-C3 SuperMini pin map and panel geometry.
//
// Single source of truth for pins and display constants. Nothing else in
// the firmware should hard-code a GPIO number or panel dimension.
#pragma once

// --- I2C OLED (SSD1306, driven over software I2C by U8g2) --------------
#define PIN_SDA 5
#define PIN_SCL 6

// --- On-board BOOT button (GPIO9) --------------------------------------
// GPIO9 is the ESP32-C3 strapping pin: holding it LOW during power-up
// enters flash-download mode. After boot it is just a normal input — used
// here as a "next expression" button. Don't hold it while powering on.
#define PIN_BTN_BOOT 9

// --- Passive piezo buzzer ----------------------------------------------
#define PIN_BUZZER 10

// --- OLED panel --------------------------------------------------------
// U8g2's NONAME constructor already targets I2C address 0x3C; OLED_ADDR is
// kept for reference and for any future driver swap.
#define OLED_ADDR 0x3C
#define OLED_W 128
#define OLED_H 64

// --- MPU-6050 IMU (3-axis accel + gyro, on its own bit-banged I2C bus) -
// The C3 has only one hardware I2C controller (Wire), used by the OLED.
// Sharing the OLED bus didn't work — both modules' pull-ups in parallel
// pushed the rise time past the 400 kHz fast-mode budget and the two
// devices alternated ACKs at random — so the MPU lives on its own
// software-I2C bus on two ordinary GPIOs. The MPU module's on-board
// pull-ups hold the lines HIGH; we drive LOW by switching pinMode to
// OUTPUT+LOW and "release" by switching back to INPUT (lets the pull-up
// float the line HIGH).
//
// GPIO7 is unrestricted. GPIO8 is a strapping pin — must idle HIGH at
// boot, which the MPU's pull-up handles, so plug the MPU in *before*
// powering the board. If GPIO8 floats LOW at reset the chip enters
// flash-download mode and no firmware runs.
#define PIN_MPU_SDA 7
#define PIN_MPU_SCL 8

// AD0 floating / tied LOW → 0x68; AD0 tied HIGH → 0x69. Most GY-521-style
// breakouts default to 0x68 — flip this if your module pulls AD0 high.
#define MPU_ADDR 0x68
