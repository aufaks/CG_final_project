#pragma once
#include "Object/InteractableObject.h"

// 버거 재료 종류(아래쪽 빵부터 위쪽 빵까지)
enum class IngredientType {
    Bottom_Bun,
    Patty,
    Lettuce,
    Cheese,
    Pickle,
    Tomato,
    Onion,
    Ketchup,
    Mustard,
    Top_Bun
};

enum class CookState { Raw, Cooked, Burnt }; // 생 / 익음 / 탐

// 재료
class Ingredient : public InteractableObject {
public:
    IngredientType type = IngredientType::Bottom_Bun;
    CookState cookState = CookState::Raw;

    void Update(float deltaTime) override {
        (void)deltaTime;
        if (type != IngredientType::Patty) return;

        switch (cookState) {
        case CookState::Raw:
            color = glm::vec3(0.62f, 0.18f, 0.14f);
            break;
        case CookState::Cooked:
            color = glm::vec3(0.36f, 0.12f, 0.055f);
            break;
        case CookState::Burnt:
            color = glm::vec3(0.055f, 0.045f, 0.035f);
            break;
        }
    }
};
