// expressions.cpp — the expression registry (see expressions.h).
//
// Just names + ids; the actual faces are drawn procedurally by
// ProceduralFace.

#include "expressions.h"

#include <stddef.h>   // size_t
#include <strings.h>  // strcasecmp

namespace {

const Expression EXPRESSIONS[] = {
    {ExpressionId::Neutral, "neutral"}, {ExpressionId::Happy, "happy"},
    {ExpressionId::Sad, "sad"},         {ExpressionId::Sleepy, "sleepy"},
    {ExpressionId::Excited, "excited"}, {ExpressionId::Surprised, "surprised"},
    {ExpressionId::Angry, "angry"},     {ExpressionId::Blink, "blink"},
    {ExpressionId::Love, "love"},       {ExpressionId::Dead, "dead"},
};

static_assert(sizeof(EXPRESSIONS) / sizeof(EXPRESSIONS[0]) ==
                  static_cast<size_t>(ExpressionId::Count),
              "EXPRESSIONS registry size must match ExpressionId::Count");

}  // namespace

uint8_t expressionCount() { return static_cast<uint8_t>(ExpressionId::Count); }

const Expression& getExpression(ExpressionId id) {
  uint8_t i = static_cast<uint8_t>(id);
  if (i >= static_cast<uint8_t>(ExpressionId::Count)) i = 0;
  return EXPRESSIONS[i];
}

const char* expressionName(ExpressionId id) { return getExpression(id).name; }

const Expression* findExpressionByName(const char* name) {
  if (name == nullptr || name[0] == '\0') return nullptr;
  for (uint8_t i = 0; i < static_cast<uint8_t>(ExpressionId::Count); i++) {
    if (strcasecmp(EXPRESSIONS[i].name, name) == 0) return &EXPRESSIONS[i];
  }
  return nullptr;
}
