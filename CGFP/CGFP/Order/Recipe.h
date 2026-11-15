#pragma once
#include "Cooking/Ingredient.h"

#include <string>
#include <vector>

class Plate;

// 메뉴 정의: 재료 종류와 쌓는 순서(아래부터), 가격, 해금 여부.
struct Recipe {
    std::string name;
    std::vector<IngredientType> layers;
    int price = 0;
    int unlockDay = 1;     // 이 날짜부터 주문 가능
    bool unlocked = false; // DayManager::StartDay가 갱신

    // 접시 재료 순서가 같고, 패티가 전부 '익음'이면 true
    bool Matches(const Plate& plate) const;
};
