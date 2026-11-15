#include "Event/SpecialEvent.h"

void SpecialEvent::Trigger() {
    if (active) return;
    active = true;
    finished = false;
    timer = 0.0f;
}

void SpecialEvent::Update(float deltaTime) {
    if (!active) return;
    timer += deltaTime;
    if (timer >= duration) Finish();
}

void SpecialEvent::Finish() {
    active = false;
    finished = true;
}
