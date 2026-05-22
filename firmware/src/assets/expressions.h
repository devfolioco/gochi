// expressions.h — the set of face expressions.
//
// Expressions are drawn procedurally by ProceduralFace (see
// views/procedural_face.*) — there are no sprite bitmaps. This registry
// just maps each expression to a stable id and a lowercase name used by
// the serial protocol (SHOW face <name>, LIST faces, GET state).
#pragma once

#include <stdint.h>

// Stable index for every expression. The EXPRESSIONS table in
// expressions.cpp is kept in sync (a static_assert there enforces it).
enum class ExpressionId : uint8_t {
  Neutral,
  Happy,
  Sad,
  Sleepy,
  Excited,
  Surprised,
  Angry,
  Blink,
  Love,
  Dead,
  Count  // sentinel: number of expressions, not an expression itself
};

struct Expression {
  ExpressionId id;
  const char* name;  // lowercase, used by the serial protocol
};

// Number of registered expressions (== ExpressionId::Count).
uint8_t expressionCount();

// Expression for an id. Out-of-range ids fall back to the first expression.
const Expression& getExpression(ExpressionId id);

// Convenience: the lowercase name for an id.
const char* expressionName(ExpressionId id);

// Look up an expression by name (case-insensitive). Returns nullptr if no
// expression matches — the serial parser uses this to reject bad names.
const Expression* findExpressionByName(const char* name);
