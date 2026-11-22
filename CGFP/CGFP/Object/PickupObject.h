#pragma once
#include "Object/InteractableObject.h"
#include "Character/Player.h"

// E키로 집을 수 있는 오브젝트. 실제 모델과 렌더링 위치는 씬이 관리한다.
class PickupObject : public InteractableObject {
public:
    explicit PickupObject(const Collider& bounds) : pickingBounds(bounds) {}

    bool CanPickUp() const override { return true; }
    Collider GetWorldCollider() const override { return pickingBounds; }
    void SetPickingBounds(const Collider& bounds) { pickingBounds = bounds; }

    void OnInteract(Player& player) override {
        if (!player.heldObject) player.heldObject = this;
    }

private:
    Collider pickingBounds;
};
