#pragma once
#include "Cooking/Ingredient.h"

// 재료 상자: 상호작용하면 재료를 새로 생성
class IngredientBox : public InteractableObject {
public:
    IngredientType type = IngredientType::Bottom_Bun;
};
