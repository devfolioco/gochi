// transport.h — buffered serial line transport.
//
// Wraps the USB serial port: accumulates incoming bytes until a newline,
// then parses the completed line into a Command. Phase 2's host CLI will
// speak this same protocol over this same transport.
#pragma once

#include <stddef.h>

#include "command.h"

class Transport {
 public:
  // Open the serial port at the given baud rate.
  void begin(unsigned long baud);

  // Non-blocking: if a complete newline-terminated line has arrived, parse
  // it into `out` and return true. Otherwise return false.
  bool poll(Command& out);

  // Send one response line (a newline is appended).
  void println(const char* s);

 private:
  static const size_t kLineCap = 128;  // max line length, incl. NUL
  char buf_[kLineCap];
  size_t len_ = 0;
  bool overflow_ = false;
};
