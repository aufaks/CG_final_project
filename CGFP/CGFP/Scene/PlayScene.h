#pragma once
#include "Character/Player.h"
#include "Object/InteractableObject.h"
#include "Object/InteractionSystem.h"
#include "Cooking/Ingredient.h"
#include "Character/Squidward.h"
#include "Order/CustomerManager.h"
#include "Order/OrderManager.h"
#include "Render/Model.h"
#include "Render/UI.h"
#include "Scene/Scene.h"

#include <memory>
#include <optional>
#include <vector>

class Ray;
class PickupObject;

class PlayScene : public Scene {
public:
    void Enter() override;
    void Exit() override;
    void Update(float deltaTime) override;
    void Render() override;

private:
    void LoadMap();
    void SetupCustomers();
    void SetupIngredientBoxes();
    void DispenseIngredient(IngredientType type);
    glm::vec3 MapToWorld(const glm::vec3& objPosition) const; // 맵 OBJ 좌표 → 월드 좌표
    void DrawOrderTickets(int width, int height);
    void DrawCrosshair(int width, int height);

    std::unique_ptr<Model> map;
    std::unique_ptr<Mesh> ingredientBoxMesh;
    glm::mat4 mapMatrix{ 1.0f };
    glm::vec3 mapCenter{ 0.0f };
    float mapScale = 1.0f;
    Player player;
    Squidward squidward;
    OrderManager orderManager;
    CustomerManager customerManager;
    UI ui;
    InteractionSystem interactions;
    PickupObject* spatulaPickup = nullptr; // interactables가 소유하는 동안에만 유효
    std::optional<IngredientType> pendingIngredient;
    bool squidwardLoaded = false;
    bool customersLoaded = false;
    bool loadRequested = false; // 첫 프레임은 화면을 먼저 그리고, 다음 Update에서 맵을 읽는다
    bool loadAttempted = false;
};
