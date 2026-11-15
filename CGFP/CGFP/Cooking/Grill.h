#pragma once
#include "Cooking/Ingredient.h"

#include <vector>

// 불판. 올려둔 패티에 시간을 더해 생 → 익음 → 탐으로 바꾼다.
// 패티는 E로 직접 집어 간다(집으면 목록에서 빠짐).
class Grill : public InteractableObject {
public:
    static constexpr int kMaxPatties = 4;

    Grill();

    bool TryPlace(InteractableObject& item) override; // 패티만 받는다
    void Update(float deltaTime) override;
    std::string GetPrompt(const Player& player) const override;

private:
    glm::vec3 SlotPosition(size_t slot, const Ingredient& patty) const;

    std::vector<Ingredient*> patties; // 오브젝트 소유는 씬
};
