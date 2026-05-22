// buzzer.cpp — passive piezo buzzer (see buzzer.h).

#include "buzzer.h"

#include <Arduino.h>

#include "../config.h"

namespace {

const buzzer::Tone* seq_ = nullptr;
uint8_t count_ = 0;
uint8_t index_ = 0;
uint32_t noteStartMs_ = 0;
bool playing_ = false;

// Drive the buzzer at `freq` Hz; freq 0 silences it (rests and stop()).
void output_(uint16_t freq) { ledcWriteTone(PIN_BUZZER, freq); }

}  // namespace

namespace buzzer {

void begin() {
  // Attach the pin to an LEDC channel; ledcWriteTone() drives the
  // frequency from here on. The initial freq/resolution are placeholders.
  ledcAttach(PIN_BUZZER, 2000, 10);
  output_(0);
}

void play(const Tone* seq, uint8_t count) {
  if (seq == nullptr || count == 0) {
    stop();
    return;
  }
  seq_ = seq;
  count_ = count;
  index_ = 0;
  noteStartMs_ = millis();
  playing_ = true;
  output_(seq_[0].freq);
}

void update(uint32_t now) {
  if (!playing_) return;
  if (now - noteStartMs_ < seq_[index_].ms) return;  // current note still on
  index_++;
  if (index_ >= count_) {
    stop();
    return;
  }
  noteStartMs_ = now;
  output_(seq_[index_].freq);
}

void stop() {
  playing_ = false;
  output_(0);
}

}  // namespace buzzer
