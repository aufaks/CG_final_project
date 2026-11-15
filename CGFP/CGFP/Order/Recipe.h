#pragma once
#include "Cooking/Ingredient.h"

#include <string>
#include <vector>

// 메뉴 정의: 재료 종류와 쌓는 순서, 해금 여부
struct Recipe {
    std::string name;
    std::vector<IngredientType> ingredients; // 아래부터 쌓는 순서
    bool unlocked = false;
};
