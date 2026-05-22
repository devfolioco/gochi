// face_view.cpp — the View that shows the pet's face (see face_view.h).

#include "face_view.h"

#include <Arduino.h>  // millis

void FaceView::setExpression(ExpressionId id) { face_.setExpression(id, millis()); }

void FaceView::blinkOnce() { face_.blink(millis()); }

void FaceView::onEnter() { face_.reset(millis()); }

void FaceView::update(uint32_t now) { face_.update(now); }

void FaceView::render(Renderer& r) { face_.render(r); }
