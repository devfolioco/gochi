// buzzer.ino — passive piezo buzzer bring-up test.
//
// Drives the buzzer on PIN_BUZZER (from src/config.h) through the ESP32's
// LEDC peripheral — same approach as the main firmware's buzzer module,
// but inlined here so this sketch has no dependencies beyond Arduino.
// Plays a short ascending scale on a loop with a pause between repeats,
// so you can confirm tone *and* timing are working.
//
// Build:  arduino-cli compile --profile c3
// Flash:  arduino-cli upload  --profile c3 --port /dev/cu.usbmodem<…>

#include <Arduino.h>

#include "../../src/config.h"

struct Tone {
  uint16_t freq;  // Hz; 0 = silent rest
  uint16_t ms;
};

// C5 major scale, then a one-second rest before the next pass.
static const Tone SCALE[] = {
    {523, 200},  // C5
    {587, 200},  // D5
    {659, 200},  // E5
    {698, 200},  // F5
    {784, 200},  // G5
    {880, 200},  // A5
    {988, 200},  // B5
    {1047, 400}, // C6
    {0, 1000},   // pause
};
static const uint8_t SCALE_LEN = sizeof(SCALE) / sizeof(SCALE[0]);

static uint8_t idx_ = 0;
static uint32_t noteStartMs_ = 0;

static void playNote(uint8_t i) {
  // ledcWriteTone(pin, 0) silences the channel — perfect for rests.
  ledcWriteTone(PIN_BUZZER, SCALE[i].freq);
}

void setup() {
  Serial.begin(115200);
  Serial.println("buzzer: playing C5 major scale on PIN_BUZZER");
  // Attach pin to an LEDC channel. The initial freq/resolution are
  // placeholders — ledcWriteTone() reprograms them per note.
  ledcAttach(PIN_BUZZER, 2000, 10);
  noteStartMs_ = millis();
  playNote(0);
}

void loop() {
  uint32_t now = millis();
  if (now - noteStartMs_ < SCALE[idx_].ms) return;
  noteStartMs_ = now;
  idx_ = (idx_ + 1) % SCALE_LEN;
  playNote(idx_);
}
