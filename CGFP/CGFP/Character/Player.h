#pragma once
#include "Character/Camera.h"
#include "Object/Object.h"

#include <vector>

class InteractableObject;

// 플레이어: WASD 이동, 벽 충돌, 마우스 시선, 들고 있는 물체(heldObject).
// transform.position은 발 위치. 카메라는 eyeHeight만큼 위에 둔다.
class Player : public Object {
public:
    Player();

    void Update(float deltaTime) override;
    void Render(ShaderManager&) const override {} // 1인칭이라 몸은 그리지 않음
    Collider GetWorldCollider() const override;

    // 이동을 막을 오브젝트 목록(solid인 것만 검사). 씬이 소유한다
    void SetObstacles(const std::vector<Object*>* list) { obstacles = list; }
    void SetPosition(const glm::vec3& feet);

    Camera& GetCamera() { return camera; }
    const Camera& GetCamera() const { return camera; }

    bool IsHolding() const { return heldObject != nullptr; }
    InteractableObject* GetHeldObject() const { return heldObject; }
    void PickUp(InteractableObject* object);
    InteractableObject* Release(); // 손에서 놓고 그 물체를 돌려준다

    float moveSpeed = 3.5f;
    float mouseSensitivity = 0.08f;
    float eyeHeight = 1.6f;
    float radius = 0.3f;
    float height = 1.8f;

private:
    Collider ColliderAt(const glm::vec3& feet) const;
    bool CollidesAt(const glm::vec3& feet) const;
    void TryMove(const glm::vec3& delta); // 축별로 나눠 움직여서 벽을 따라 미끄러지게 한다
    void UpdateHeldObject();

    Camera camera;
    InteractableObject* heldObject = nullptr;
    const std::vector<Object*>* obstacles = nullptr;
};
