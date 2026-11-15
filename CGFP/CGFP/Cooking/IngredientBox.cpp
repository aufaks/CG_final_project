#include "Cooking/IngredientBox.h"

#include "Character/Player.h"
#include "Render/ResourceManager.h"

#include <utility>

IngredientBox::IngredientBox(IngredientType type, SpawnFunction spawn) : type(type), spawn(std::move(spawn)) {
    mesh = ResourceManager::Get().GetCube();
    transform.scale = glm::vec3(0.5f, 0.3f, 0.5f);
    // 상자 색은 안에 든 재료 색을 어둡게
    Ingredient sample(type);
    color = sample.color * 0.7f;
}

void IngredientBox::OnInteract(Player& player) {
    if (player.IsHolding() || !spawn) return;
    if (Ingredient* ingredient = spawn(type)) player.PickUp(ingredient);
}

std::string IngredientBox::GetPrompt(const Player& player) const {
    if (player.IsHolding()) return ToString(type);
    return std::string("F: ") + ToString(type) + " 꺼내기";
}
