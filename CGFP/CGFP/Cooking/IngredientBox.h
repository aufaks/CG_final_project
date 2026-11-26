#pragma once
#include "Cooking/Ingredient.h"
#include "Render/Model.h"
#include "Render/ShaderManager.h"
#include "Render/Texture.h"
#include "Object/PickupObject.h"
#include <cmath>
#include <functional>

// 꺼낸 재료가 종류를 유지하도록 집기 오브젝트에 저장한다.
class IngredientPickup : public PickupObject {
public:
    IngredientPickup(IngredientType type, std::unique_ptr<Model> model)
        : PickupObject(std::move(model), glm::mat4(1.0f)), type(type) {}
    IngredientType GetType() const { return type; }
private:
    IngredientType type;
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
