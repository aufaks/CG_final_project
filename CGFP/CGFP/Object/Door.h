#pragma once
#include "Object/InteractableObject.h"

class Mesh;

// 맵에 고정된 출입문. 상호작용하면 플레이어를 문 반대편으로 이동시킨다.
class Door : public InteractableObject {
public:
    Door(const Collider& pickingBounds, const glm::vec3& firstSide,
         const glm::vec3& secondSide, const Mesh* outlineMesh);

    Collider GetWorldCollider() const override { return pickingBounds; }
    void OnInteract(Player& player) override;
    const Mesh* GetOutlineMesh() const { return outlineMesh; }

private:
    Collider pickingBounds;
    glm::vec3 firstSide;
    glm::vec3 secondSide;
    const Mesh* outlineMesh = nullptr; // 맵이 소유하며 Door보다 오래 유지된다
};
