// buzzer.h — passive piezo buzzer: non-blocking tone-sequence playback.
//
// play() starts a melody; update() must be called every loop to advance
// it. Nothing blocks — the ESP32's LEDC peripheral holds each tone while
// the CPU is free to render the face.
#pragma once

#include <stdint.h>

namespace buzzer {

// One step of a melody. freq is in Hz; freq == 0 is a silent rest.
struct Tone {
  uint16_t freq;
  uint16_t ms;
};

// Configure the buzzer pin. Call once from setup().
void begin();

// Start playing `count` tones from `seq`, replacing anything already
// playing. `seq` must outlive playback — point it at static data.
void play(const Tone* seq, uint8_t count);

// Advance playback. Call once per loop with millis().
void update(uint32_t now);

// Stop immediately and silence the buzzer.
void stop();

}  // namespace buzzer
