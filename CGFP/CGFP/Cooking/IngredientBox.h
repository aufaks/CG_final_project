#pragma once
#include "Cooking/Ingredient.h"
#include "Render/Model.h"
#include "Render/ShaderManager.h"
#include "Render/Texture.h"
#include "Object/PickupObject.h"
#include "Cooking/TrayStack.h"
#include <cmath>
#include <functional>

// 꺼낸 재료가 종류를 유지하도록 집기 오브젝트에 저장한다.
class IngredientPickup : public PickupObject {
public:
    IngredientPickup(IngredientType type, std::unique_ptr<Model> model,
                     const glm::mat4& matrix = glm::mat4(1.0f), float dropScale = 1.0f)
        : PickupObject(std::move(model), matrix, dropScale), type(type) {}
    IngredientType GetType() const { return type; }
    bool CanFocus(const Player&) const override { return !attached; }
    bool IsAttached() const { return attached; }
    void UseOn(Player& player, InteractableObject* target) override {
        auto* tray = dynamic_cast<TrayPickup*>(target);
        if (!tray) return;
        if (type == IngredientType::Ketchup || type == IngredientType::Mustard) {
            // 소스통은 손에 유지하고 뿌려진 소스만 추가한다.
            tray->AddSauce(type);
            return;
        }
        tray->AddIngredient(GetModel(), type);
        attached = true;
        player.heldObject = nullptr;
    }
    void UpdateHeld(const Player& player) override {
        const glm::vec3 forward = player.camera.GetForward();
        const glm::vec3 right = player.camera.GetRight();
        const glm::vec3 up = glm::normalize(glm::cross(right, forward));
        glm::mat4 matrix(1.0f);
        matrix[0] = glm::vec4(right, 0.0f);
        matrix[1] = glm::vec4(up, 0.0f);
        matrix[2] = glm::vec4(-forward, 0.0f);
        const bool isSauce = type == IngredientType::Ketchup || type == IngredientType::Mustard;
        matrix[3] = glm::vec4(player.camera.position + forward * 0.65f + right * 0.34f - up * (isSauce ? 0.21f : 0.17f), 1.0f);
        // 납작한 재료의 윗면을 카메라 쪽으로 기울인다.
        if (!isSauce)
            matrix = glm::rotate(matrix, glm::radians(65.0f), glm::vec3(1, 0, 0));
        // 펌프 주둥이(+X)가 플레이어 앞쪽을 향하도록 돌린다.
        matrix = glm::rotate(matrix, glm::radians(isSauce ? 90.0f : -15.0f), glm::vec3(0, 1, 0));
        matrix = glm::scale(matrix, glm::vec3(isSauce ? 2.25f : 1.4f));
        const glm::vec3 center = (GetModel().GetBoundsMin() + GetModel().GetBoundsMax()) * 0.5f;
        SetHeldMatrix(glm::translate(matrix, -center));
    }
private:
    IngredientType type;
    bool attached = false;
};

// 재료 상자: 상호작용하면 재료를 새로 생성
class IngredientBox : public InteractableObject {
public:
    IngredientBox(IngredientType ingredient, const glm::vec3& position, Mesh* boxMesh, Texture* contentsTexture,
                  std::function<void(IngredientType)> dispense)
        : type(ingredient), contentsTexture(contentsTexture), dispense(std::move(dispense)) {
        mesh = boxMesh;
        transform.position = position;
        transform.scale = glm::vec3(0.52f, 0.50f, 0.52f);
        color = glm::vec3(0.61f, 0.48f, 0.24f);
        preview = CreateIngredientShape(type);
    }
    // 재료마다 텍스처 모음의 한 칸을 상자 윗면 전체에 입힌다.
    static std::unique_ptr<Mesh> CreateIngredientShape(IngredientType type) {
        int tile = 0;
        switch (type) {
        case IngredientType::Patty: tile = 1; break;
        case IngredientType::Lettuce: tile = 2; break;
        case IngredientType::Cheese: tile = 3; break;
        case IngredientType::Pickle: tile = 4; break;
        case IngredientType::Tomato: tile = 5; break;
        case IngredientType::Onion: tile = 6; break;
        case IngredientType::Top_Bun: tile = 7; break;
        default: break;
        }
        const float u0 = (tile % 4) * 0.25f + 0.002f, u1 = (tile % 4 + 1) * 0.25f - 0.002f;
        const float v0 = tile < 4 ? 0.502f : 0.002f, v1 = tile < 4 ? 0.998f : 0.498f;
        std::vector<MeshVertex> vertices(4);
        vertices[0].position = {-0.25f, 0, -0.25f}; vertices[0].uv = {u0, v0};
        vertices[1].position = { 0.25f, 0, -0.25f}; vertices[1].uv = {u1, v0};
        vertices[2].position = { 0.25f, 0,  0.25f}; vertices[2].uv = {u1, v1};
        vertices[3].position = {-0.25f, 0,  0.25f}; vertices[3].uv = {u0, v1};
        for (auto& vertex : vertices) vertex.normal = glm::vec3(0, 1, 0);
        const std::vector<uint32_t> indices{0, 2, 1, 0, 3, 2};
        auto shape = std::make_unique<Mesh>();
        shape->Upload(vertices, indices);
        return shape;
    }
    // 바닥과 네 벽, 바깥으로 펼친 덮개를 가진 열린 종이 상자.
    static std::unique_ptr<Mesh> CreateBoxMesh() {
        std::vector<MeshVertex> vertices;
        std::vector<uint32_t> indices;
        auto quad = [&](glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 d) {
            const glm::vec3 normal = glm::normalize(glm::cross(b - a, c - a));
            for (int side = 0; side < 2; ++side) {
                const uint32_t first = static_cast<uint32_t>(vertices.size());
                for (const auto& p : { a, b, c, d }) {
                    MeshVertex v; v.position = p; v.normal = side ? -normal : normal;
                    vertices.push_back(v);
                }
                if (side == 0) indices.insert(indices.end(), {first, first + 1, first + 2, first, first + 2, first + 3});
                else indices.insert(indices.end(), {first, first + 2, first + 1, first, first + 3, first + 2});
            }
        };
        quad({-.5f,-.5f,-.5f}, {.5f,-.5f,-.5f}, {.5f,-.5f,.5f}, {-.5f,-.5f,.5f});
        quad({-.5f,-.5f,-.5f}, {-.5f,.5f,-.5f}, {.5f,.5f,-.5f}, {.5f,-.5f,-.5f});
        quad({.5f,-.5f,.5f}, {.5f,.5f,.5f}, {-.5f,.5f,.5f}, {-.5f,-.5f,.5f});
        quad({-.5f,-.5f,.5f}, {-.5f,.5f,.5f}, {-.5f,.5f,-.5f}, {-.5f,-.5f,-.5f});
        quad({.5f,-.5f,-.5f}, {.5f,.5f,-.5f}, {.5f,.5f,.5f}, {.5f,-.5f,.5f});
        quad({-.5f,.5f,-.5f}, {-.5f,.5f,.5f}, {-.72f,.59f,.5f}, {-.72f,.59f,-.5f});
        quad({.5f,.5f,.5f}, {.5f,.5f,-.5f}, {.72f,.59f,-.5f}, {.72f,.59f,.5f});
        quad({.5f,.5f,-.5f}, {-.5f,.5f,-.5f}, {-.5f,.59f,-.72f}, {.5f,.59f,-.72f});
        quad({-.5f,.5f,.5f}, {.5f,.5f,.5f}, {.5f,.59f,.72f}, {-.5f,.59f,.72f});
        auto result = std::make_unique<Mesh>();
        result->Upload(vertices, indices);
        return result;
    }
    void OnPrimaryInteract(Player&) override { if (dispense) dispense(type); }
    IngredientType GetType() const { return type; }
    void DrawPreview(ShaderManager& shaders) const {
        if (!preview || !contentsTexture) return;
        shaders.SetBool("uUseTexture", true);
        shaders.SetFloat("uOpacity", 1.0f);
        shaders.SetVec3("uColor", glm::vec3(1.0f));
        contentsTexture->Bind(0);
        shaders.SetMat4("uModel", glm::translate(glm::mat4(1.0f),
            transform.position + glm::vec3(0, 0.235f, 0)));
        preview->Draw();
    }
private:
    IngredientType type;
    std::unique_ptr<Mesh> preview;
    Texture* contentsTexture = nullptr; // ResourceManager가 소유한다.
    std::function<void(IngredientType)> dispense;
};
