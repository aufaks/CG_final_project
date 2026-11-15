#pragma once
#include "Object/InteractableObject.h"

// TODO: 재료 종류 채우기
enum class IngredientType {
    Bun,
    Patty,
    Lettuce,
};

enum class CookState { Raw, Cooked, Burnt }; // 생 / 익음 / 탐

// 재료
class Ingredient : public InteractableObject {
public:
    IngredientType type = IngredientType::Bun;
    CookState cookState = CookState::Raw;
};
