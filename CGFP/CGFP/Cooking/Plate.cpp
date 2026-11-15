#include "Cooking/Plate.h"

#include "Character/Player.h"
#include "Render/ResourceManager.h"

Plate::Plate() {
    mesh = ResourceManager::Get().GetCube();
    transform.scale = glm::vec3(0.4f, 0.04f, 0.4f);
    color = glm::vec3(0.95f);
}

bool Plate::TryPlace(InteractableObject& item) {
    auto* ingredient = dynamic_cast<Ingredient*>(&item);
    if (!ingredient || stack.size() >= kMaxStack) return false;
    ingredient->SetInteractEnabled(false); // 쌓인 재료는 따로 집지 않는다
    stack.push_back(ingredient);
    Update(0.0f);
    return true;
}

void Plate::Update(float) {
    float y = transform.position.y + transform.scale.y * 0.5f;
    for (Ingredient* ingredient : stack) {
        const float thickness = ingredient->GetThickness();
        ingredient->transform.position = glm::vec3(transform.position.x, y + thickness * 0.5f, transform.position.z);
        ingredient->transform.rotation = transform.rotation;
        y += thickness;
    }
}

std::string Plate::GetPrompt(const Player& player) const {
    if (!player.IsHolding()) return "E: 접시 들기";
    if (dynamic_cast<Ingredient*>(player.GetHeldObject())) return "E: 접시에 올리기";
    return {};
}

std::vector<IngredientType> Plate::GetLayers() const {
    std::vector<IngredientType> layers;
    layers.reserve(stack.size());
    for (const Ingredient* ingredient : stack) layers.push_back(ingredient->GetType());
    return layers;
}

void Plate::ClearStack() {
    for (Ingredient* ingredient : stack) ingredient->Destroy();
    stack.clear();
}
