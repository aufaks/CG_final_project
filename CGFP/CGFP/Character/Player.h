#pragma once
#include "Character/Camera.h"
#include "Object/Object.h"

class InteractableObject;

// 플레이어: WASD 이동, 충돌, 들고 있는 물체
class Player : public Object {
public:
    void Update(float deltaTime) override {}

    Camera camera;
    InteractableObject* heldObject = nullptr;
};
