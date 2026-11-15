#pragma once
#include "Cooking/Ingredient.h"

#include <functional>

// 재료 상자. F로 상호작용하면 재료를 새로 만들어 플레이어 손에 쥐여 준다.
// 오브젝트 생성은 씬이 하므로 spawn 함수를 받아서 쓴다.
class IngredientBox : public InteractableObject {
public:
    using SpawnFunction = std::function<Ingredient*(IngredientType)>;

    IngredientBox(IngredientType type, SpawnFunction spawn);

    void OnInteract(Player& player) override;
    std::string GetPrompt(const Player& player) const override;

    IngredientType GetType() const { return type; }

private:
    IngredientType type;
    SpawnFunction spawn;
};
