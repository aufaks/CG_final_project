#pragma once
#include "Object/Object.h"

class Player;

// 상호작용 부모. E·F 상호작용 대상(요리 오브젝트)은 전부 상속한다.
class InteractableObject : public Object {
public:
    virtual void OnInteract(Player& player) {}
    virtual bool CanPickUp() const { return false; }
};
