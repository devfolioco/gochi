// command.cpp — protocol line parser (see command.h).

#include "command.h"

#include <string.h>   // strncpy, size_t
#include <strings.h>  // strcasecmp

namespace {

// Copy the next whitespace-delimited token from *pp into out (NUL-terminated,
// capped at cap). Advances *pp past the token and any trailing whitespace.
void nextToken(const char** pp, char* out, size_t cap) {
  const char* p = *pp;
  while (*p == ' ' || *p == '\t') p++;
  size_t n = 0;
  while (*p != '\0' && *p != ' ' && *p != '\t') {
    if (n + 1 < cap) out[n++] = *p;
    p++;
  }
  out[n] = '\0';
  while (*p == ' ' || *p == '\t') p++;
  *pp = p;
}

// Copy src into the fixed-size dst, always NUL-terminating.
void copyArg(char* dst, size_t cap, const char* src) {
  strncpy(dst, src, cap - 1);
  dst[cap - 1] = '\0';
}

}  // namespace

Command parseLine(const char* line) {
  Command cmd;
  cmd.type = CmdType::Unknown;
  cmd.arg1[0] = '\0';
  cmd.arg2 = -1;  // unused in Phase 1; reserved for Phase 2 numeric args
  if (line == nullptr) return cmd;

  const char* p = line;
  char verb[8];
  nextToken(&p, verb, sizeof(verb));

  if (strcasecmp(verb, "PING") == 0) {
    cmd.type = CmdType::Ping;
  } else if (strcasecmp(verb, "GET") == 0) {
    char what[8];
    nextToken(&p, what, sizeof(what));
    if (strcasecmp(what, "state") == 0)
      cmd.type = CmdType::GetState;
    else if (strcasecmp(what, "fps") == 0)
      cmd.type = CmdType::GetFps;
  } else if (strcasecmp(verb, "LIST") == 0) {
    char what[8];
    nextToken(&p, what, sizeof(what));
    if (strcasecmp(what, "faces") == 0) cmd.type = CmdType::ListFaces;
  } else if (strcasecmp(verb, "SET") == 0) {
    char what[8];
    nextToken(&p, what, sizeof(what));
    if (strcasecmp(what, "mood") == 0) {
      cmd.type = CmdType::SetMood;
      char name[sizeof(cmd.arg1)];
      nextToken(&p, name, sizeof(name));
      copyArg(cmd.arg1, sizeof(cmd.arg1), name);
    }
  } else if (strcasecmp(verb, "SHOW") == 0) {
    char what[8];
    nextToken(&p, what, sizeof(what));
    if (strcasecmp(what, "face") == 0) {
      cmd.type = CmdType::ShowFace;
      char name[sizeof(cmd.arg1)];
      nextToken(&p, name, sizeof(name));
      copyArg(cmd.arg1, sizeof(cmd.arg1), name);
    } else if (strcasecmp(what, "text") == 0) {
      cmd.type = CmdType::ShowText;
      // The text payload is the rest of the line, taken verbatim.
      copyArg(cmd.arg1, sizeof(cmd.arg1), p);
    }
  }
  return cmd;
}
