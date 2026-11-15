#include "Cooking/Ingredient.h"

#include "Character/Player.h"
#include "Render/ResourceManager.h"

const char* ToString(IngredientType type) {
    switch (type) {
    case IngredientType::BottomBun: return "아래 빵";
    case IngredientType::Patty: return "패티";
    case IngredientType::Cheese: return "치즈";
    case IngredientType::Lettuce: return "양상추";
    case IngredientType::Tomato: return "토마토";
    case IngredientType::TopBun: return "위 빵";
    default: return "?";
    }
}

const char* ToString(CookState state) {
    switch (state) {
    case CookState::Raw: return "생";
    case CookState::Cooked: return "익음";
    case CookState::Burnt: return "탐";
    default: return "?";
    }
}

Ingredient::Ingredient(IngredientType type) : type(type) {
    // TODO(2단계): 재료별 모델/텍스처로 교체. 지금은 납작한 상자
    mesh = ResourceManager::Get().GetCube();
    const float thickness = (type == IngredientType::Lettuce || type == IngredientType::Cheese) ? 0.02f : 0.05f;
    transform.scale = glm::vec3(0.25f, thickness, 0.25f);
    UpdateColor();
}

void Ingredient::AddCookTime(float deltaTime) {
    if (!IsCookable()) return;
    cookTime += deltaTime;
    if (cookTime >= kBurntTime) cookState = CookState::Burnt;
    else if (cookTime >= kCookedTime) cookState = CookState::Cooked;
    UpdateColor();
}

std::string Ingredient::GetPrompt(const Player& player) const {
    if (player.IsHolding()) return {};
    std::string text = std::string("E: ") + ToString(type);
    if (IsCookable()) text += std::string("(") + ToString(cookState) + ")";
    return text + " 들기";
}

void Ingredient::UpdateColor() {
    switch (type) {
    case IngredientType::BottomBun:
    case IngredientType::TopBun: color = { 0.85f, 0.65f, 0.35f }; break;
    case IngredientType::Cheese: color = { 1.0f, 0.85f, 0.2f }; break;
    case IngredientType::Lettuce: color = { 0.35f, 0.8f, 0.3f }; break;
    case IngredientType::Tomato: color = { 0.9f, 0.2f, 0.15f }; break;
    case IngredientType::Patty:
        if (cookState == CookState::Raw) color = { 0.9f, 0.5f, 0.5f };
        else if (cookState == CookState::Cooked) color = { 0.45f, 0.25f, 0.1f };
        else color = { 0.1f, 0.08f, 0.06f };
        break;
    default: break;
    }
}
