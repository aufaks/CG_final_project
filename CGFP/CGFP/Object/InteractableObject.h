#pragma once
#include "Object/Object.h"

#include <string>

class Player;

// E·F 상호작용 대상의 부모. 요리 오브젝트(Ingredient, Plate, Grill, IngredientBox, Seat)가 전부 상속한다.
//   E: 빈손이면 CanPickUp()인 대상을 든다. 들고 있으면 조준 대상의 TryPlace()에 놓고, 실패하면 바닥/작업대에 내려놓는다.
//   F: 조준 대상의 OnInteract()
class InteractableObject : public Object {
public:
    virtual bool CanPickUp() const { return false; }
    virtual void OnInteract(Player& player) {}
    // 플레이어가 들고 있는 item을 이 오브젝트에 놓는다(예: 접시에 재료, 불판에 패티). 받으면 true
    virtual bool TryPlace(InteractableObject& item) { return false; }
    // 조준했을 때 보여 줄 안내 문구. 빈 문자열이면 표시하지 않음
    virtual std::string GetPrompt(const Player& player) const;

    bool IsHeld() const { return held; }
    void SetHeld(bool value) { held = value; }
    // false면 레이 피킹 대상에서 빠진다(예: 접시 위에 쌓인 재료)
    bool IsInteractEnabled() const { return interactEnabled; }
    void SetInteractEnabled(bool value) { interactEnabled = value; }

protected:
    bool held = false;
    bool interactEnabled = true;
};
