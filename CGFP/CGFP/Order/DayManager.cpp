#include "Order/DayManager.h"

#include "Order/Recipe.h"

void DayManager::StartDay(std::vector<Recipe>& recipes) {
    sales = 0;
    timeLeft = businessDuration;
    open = true;
    for (Recipe& recipe : recipes) recipe.unlocked = recipe.unlockDay <= day;
}

void DayManager::Update(float deltaTime) {
    if (!open) return;
    timeLeft -= deltaTime;
    if (timeLeft <= 0.0f) {
        timeLeft = 0.0f;
        open = false;
    }
}

int DayManager::GetQuota() const {
    // TODO: 기획서의 날짜별 할당량으로 교체
    return 20 + (day - 1) * 15;
}
