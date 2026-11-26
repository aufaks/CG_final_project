#pragma once
#include "Object/PickupObject.h"
#include "Object/Ray.h"
#include "Core/InputManager.h"
#include <memory>
#include <vector>

// 상호작용 대상의 소유권과 조준·입력 처리를 관리한다.
class InteractionSystem {
public:
    void Add(std::unique_ptr<InteractableObject> object) { objects.push_back(std::move(object)); }
    void Clear(Player& player) { focused = nullptr; player.heldObject = nullptr; objects.clear(); }
    InteractableObject* Focused() const { return focused; }
    const std::vector<std::unique_ptr<InteractableObject>>& Objects() const { return objects; }
    void Update(Player& player, bool active, float distance) {
        focused = nullptr;
        if (!active) return;
        const Ray ray = Ray::FromCamera(player.camera);
        float nearest = distance;
        for (const auto& object : objects) {
            if (object->IsDestroyed() || object.get() == player.heldObject) continue;
            float hit = 0.0f;
            if (object->GetWorldCollider().IntersectsRay(ray.origin, ray.direction, nearest, hit)) {
                nearest = hit;
                focused = object.get();
            }
        }
        auto* held = dynamic_cast<PickupObject*>(player.heldObject);
        if (held && InputManager::GetKeyDown(GLFW_KEY_Q)) {
            held->Drop(player);
            focused = nullptr;
            return;
        }
        if (focused && !focused->CanPickUp() && InputManager::GetKeyDown(GLFW_KEY_E)) focused->OnInteract(player);
        if (InputManager::GetMouseButtonDown(GLFW_MOUSE_BUTTON_LEFT)) {
            if (held) held->Use(player);
            else if (!player.heldObject && focused) {
                focused->OnPrimaryInteract(player);
                if (player.heldObject) focused = nullptr;
            }
        }
        held = dynamic_cast<PickupObject*>(player.heldObject);
        if (held) held->UpdateHeld(player);
    }
private:
    std::vector<std::unique_ptr<InteractableObject>> objects;
    InteractableObject* focused = nullptr;
};
