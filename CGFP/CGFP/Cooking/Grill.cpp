#include "Cooking/Grill.h"

#include "Character/Player.h"
#include "Render/ResourceManager.h"

#include <algorithm>

Grill::Grill() {
    mesh = ResourceManager::Get().GetCube();
    transform.scale = glm::vec3(1.2f, 0.1f, 0.8f);
    color = glm::vec3(0.25f);
}

bool Grill::TryPlace(InteractableObject& item) {
    auto* patty = dynamic_cast<Ingredient*>(&item);
    if (!patty || !patty->IsCookable() || static_cast<int>(patties.size()) >= kMaxPatties) return false;
    patty->transform.position = SlotPosition(patties.size(), *patty);
    patty->transform.rotation = glm::vec3(0.0f);
    patties.push_back(patty);
    return true;
}

void Grill::Update(float deltaTime) {
    // 플레이어가 집어 갔거나 없어진 패티는 목록에서 뺀다
    patties.erase(std::remove_if(patties.begin(), patties.end(),
        [](Ingredient* p) { return p->IsHeld() || p->IsDestroyed(); }), patties.end());

    for (size_t i = 0; i < patties.size(); ++i) {
        patties[i]->transform.position = SlotPosition(i, *patties[i]);
        patties[i]->AddCookTime(deltaTime);
    }
}

std::string Grill::GetPrompt(const Player& player) const {
    const auto* held = dynamic_cast<const Ingredient*>(player.GetHeldObject());
    if (held && held->IsCookable()) return "E: 불판에 올리기";
    return "불판 (패티 " + std::to_string(patties.size()) + "/" + std::to_string(kMaxPatties) + ")";
}

glm::vec3 Grill::SlotPosition(size_t slot, const Ingredient& patty) const {
    // 2x2 칸
    const float x = (slot % 2 == 0 ? -0.25f : 0.25f) * transform.scale.x;
    const float z = (slot / 2 == 0 ? -0.25f : 0.25f) * transform.scale.z;
    const float y = transform.scale.y * 0.5f + patty.GetThickness() * 0.5f;
    return transform.position + glm::vec3(x, y, z);
}
