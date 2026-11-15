#pragma once
#include "Character/Player.h"
#include "Cooking/Grill.h"
#include "Cooking/Ingredient.h"
#include "Cooking/IngredientBox.h"
#include "Cooking/Plate.h"
#include "Cooking/Seat.h"
#include "Object/InteractableObject.h"
#include "Object/StaticObject.h"
#include "Order/CustomerManager.h"
#include "Order/DayManager.h"
#include "Order/OrderManager.h"
#include "Render/UI.h"
#include "Scene/Scene.h"

#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

// 영업 루프: 이동 → 조준/상호작용 → 요리 → 손님/주문 → 하루 끝 판정.
// 게임 규칙은 Update에서만 바꾸고, Render는 상태를 그리기만 한다.
class PlayScene : public Scene {
public:
    void Enter() override;
    void Exit() override;
    void Update(float deltaTime) override;
    void Render() override;

private:
    static constexpr float kReach = 3.0f; // 상호작용 최대 거리(m)

    void BuildMap(); // TODO(1단계): 회색 박스 대신 집게리아 OBJ 배치
    Ingredient* SpawnIngredient(IngredientType type);
    void HandleInteraction();
    void DropHeldObject();
    void CheckDayEnd();
    void RemoveDestroyedObjects();

    // 오브젝트를 만들어 씬에 등록한다. 상호작용 대상이면 interactables, 아니면 statics에 들어간다
    template <class T, class... Args>
    T* Spawn(Args&&... args) {
        auto object = std::make_unique<T>(std::forward<Args>(args)...);
        T* raw = object.get();
        if constexpr (std::is_base_of_v<InteractableObject, T>) interactables.push_back(raw);
        else statics.push_back(raw);
        objects.push_back(std::move(object));
        return raw;
    }
    StaticObject* SpawnBox(const glm::vec3& center, const glm::vec3& size, const glm::vec3& color, bool solid = true);

    Player player;
    std::vector<std::unique_ptr<Object>> objects;    // 씬의 모든 오브젝트 소유
    std::vector<InteractableObject*> interactables; // 레이 피킹 대상
    std::vector<Object*> statics;                   // 벽·바닥·작업대(충돌, 내려놓기 위치)
    std::vector<Seat*> seats;

    OrderManager orderManager;
    CustomerManager customerManager;
    DayManager dayManager;
    UI ui;

    InteractableObject* aimTarget = nullptr;
    bool dayEnding = false;
    bool sceneFinished = false;
};
