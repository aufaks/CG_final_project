#pragma once
#include "Object/InteractableObject.h"

enum class IngredientType {
    BottomBun,
    Patty,
    Cheese,
    Lettuce,
    Tomato,
    TopBun,
    Count,
};

// 굽기 상태는 패티만 쓴다. 다른 재료는 항상 Raw.
enum class CookState { Raw, Cooked, Burnt };

const char* ToString(IngredientType type);
const char* ToString(CookState state);

// 재료. 종류(enum)와 굽기 상태를 가진다. E로 들 수 있다.
class Ingredient : public InteractableObject {
public:
    static constexpr float kCookedTime = 8.0f;  // 불판에서 이 시간이 지나면 익음
    static constexpr float kBurntTime = 16.0f;  // 이 시간이 지나면 탐

    explicit Ingredient(IngredientType type);

    IngredientType GetType() const { return type; }
    CookState GetCookState() const { return cookState; }
    bool IsCookable() const { return type == IngredientType::Patty; }
    void AddCookTime(float deltaTime); // Grill이 호출

    float GetThickness() const { return transform.scale.y; } // 접시에 쌓을 때 높이

    bool CanPickUp() const override { return true; }
    std::string GetPrompt(const Player& player) const override;

private:
    void UpdateColor();

    IngredientType type;
    CookState cookState = CookState::Raw;
    float cookTime = 0.0f;
};
