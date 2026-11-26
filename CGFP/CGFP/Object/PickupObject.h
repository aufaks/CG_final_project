#pragma once
#include "Object/InteractableObject.h"
#include "Character/Player.h"
#include "Render/Model.h"
#include <memory>

// 집을 수 있는 물건의 모델, 배치와 피킹 범위를 함께 관리한다.
class PickupObject : public InteractableObject {
public:
    PickupObject(std::unique_ptr<Model> model, const glm::mat4& matrix, float dropScale = 1.0f)
        : model(std::move(model)), matrix(matrix), dropScale(dropScale) { UpdateBounds(); }

    bool CanPickUp() const override { return true; }
    Collider GetWorldCollider() const override { return pickingBounds; }
    const Model& GetModel() const { return *model; }
    const glm::mat4& GetMatrix() const { return matrix; }

    virtual void Use(Player&) {} // 도구별 사용 동작은 파생 클래스에서 구현한다.
    virtual void UpdateHeld(const Player& player) {
        const glm::vec3 forward = player.camera.GetForward();
        const glm::vec3 right = player.camera.GetRight();
        const glm::vec3 up = glm::normalize(glm::cross(right, forward));
        matrix = glm::mat4(1.0f);
        matrix[0] = glm::vec4(right, 0.0f);
        matrix[1] = glm::vec4(up, 0.0f);
        matrix[2] = glm::vec4(-forward, 0.0f);
        matrix[3] = glm::vec4(player.camera.position + forward * 0.55f + right * 0.44f - up * 0.42f, 1.0f);
        matrix = glm::rotate(matrix, glm::radians(25.0f), glm::vec3(0, 0, 1));
        matrix = glm::rotate(matrix, glm::radians(-50.0f), glm::vec3(1, 0, 0));
    }
protected:
    void SetHeldMatrix(const glm::mat4& heldMatrix) { matrix = heldMatrix; }

public:
    void Drop(Player& player) {
        const glm::vec3 forward = player.camera.GetFlatForward();
        matrix = glm::mat4(1.0f);
        matrix[0] = glm::vec4(player.camera.GetRight(), 0.0f);
        matrix[1] = glm::vec4(forward, 0.0f);
        matrix[2] = glm::vec4(0, 1, 0, 0);
        matrix[3] = glm::vec4(player.transform.position + forward * 0.85f + glm::vec3(0, 0.02f, 0), 1.0f);
        matrix = glm::scale(matrix, glm::vec3(dropScale));
        UpdateBounds();
        player.heldObject = nullptr;
    }

    void OnInteract(Player& player) override {
        if (!player.heldObject) player.heldObject = this;
    }

private:
    void UpdateBounds() {
        pickingBounds = Collider::FromLocalBounds(model->GetBoundsMin(), model->GetBoundsMax(), matrix);
        pickingBounds.min -= glm::vec3(0.01f, 0.06f, 0.01f);
        pickingBounds.max += glm::vec3(0.01f, 0.06f, 0.01f);
    }
    std::unique_ptr<Model> model;
    glm::mat4 matrix{1.0f};
    float dropScale = 1.0f;
    Collider pickingBounds;
};
