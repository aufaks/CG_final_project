#pragma once
#include "Cooking/Ingredient.h"

#include <vector>

// 접시. 재료를 받을 때마다 위로 쌓는다.
// 서빙 판정은 오브젝트가 아니라 GetLayers()의 재료 종류 목록으로 Recipe와 비교한다.
class Plate : public InteractableObject {
public:
    static constexpr size_t kMaxStack = 10;

    Plate();

    bool CanPickUp() const override { return true; }
    bool TryPlace(InteractableObject& item) override; // Ingredient만 받는다
    void Update(float deltaTime) override;            // 쌓인 재료가 접시를 따라다니게 한다
    std::string GetPrompt(const Player& player) const override;

    const std::vector<Ingredient*>& GetStack() const { return stack; }
    std::vector<IngredientType> GetLayers() const; // 아래부터 위로
    bool IsEmpty() const { return stack.empty(); }
    void ClearStack(); // 서빙 후 재료를 없앤다

private:
    std::vector<Ingredient*> stack; // 아래부터 위로. 오브젝트 소유는 씬
};
