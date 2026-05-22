// command.h — the device command model.
//
// This is the seam shared with future host tooling: Phase 2's CLI and
// server will emit this exact line protocol, so nothing here changes.
//
// Line protocol (newline-terminated, typeable in a serial monitor):
//   SHOW face <name>      switch to a face expression
//   SHOW text <string>    switch to the text view ("<string>" = rest of line)
//   SET mood <name>       Phase 1: accepted and acked, no behavior yet
//   GET state             query the current view + expression
//   GET fps               query the current display frame rate
//   LIST faces            list all expression names
//   PING                  liveness check
#pragma once

enum class CmdType { ShowFace, ShowText, SetMood, GetState, GetFps, ListFaces, Ping, Unknown };

struct Command {
  CmdType type;
  char arg1[64];  // face name, mood name, or text payload
  int arg2;       // optional numeric argument (e.g. duration ms); -1 if unset
};

// Parse one protocol line into a Command. Never fails: an unrecognized
// line yields CmdType::Unknown. The input string is not modified.
Command parseLine(const char* line);
