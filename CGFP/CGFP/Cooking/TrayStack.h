#pragma once
#include "Object/PickupObject.h"
#include "Render/ShaderManager.h"
#include <functional>

// 탁자에 쌓인 쟁반 더미. 빈손으로 선택하면 쟁반 하나를 꺼낸다.
class TrayStack : public InteractableObject {
public:
    TrayStack(std::unique_ptr<Model> tray, const glm::mat4& base, std::function<void()> dispense)
        : tray(std::move(tray)), base(base), dispense(std::move(dispense)) {}

    Collider GetWorldCollider() const override {
        const glm::vec3 min = tray->GetBoundsMin() - glm::vec3(0.04f, 0.03f, 0.04f);
        const glm::vec3 max = tray->GetBoundsMax() + glm::vec3(0.04f, (kCount - 1) * kSpacing + 0.03f, 0.04f);
        return Collider::FromLocalBounds(min, max, base);
    }
    void OnPrimaryInteract(Player&) override { if (dispense) dispense(); }
    const Model& GetModel() const { return *tray; }
    const glm::mat4& GetMatrix() const { return base; }
    void Draw(ShaderManager& shaders, const glm::vec3& camera) const {
        for (int i = 0; i < kCount; ++i)
            tray->Draw(shaders, Layer(i), camera);
    }
    void DrawGeometry(ShaderManager& shaders, const glm::mat4& transform) const {
        for (int i = 0; i < kCount; ++i) {
            shaders.SetMat4("uModel", transform * glm::translate(glm::mat4(1.0f), glm::vec3(0, i * kSpacing, 0)));
            tray->DrawGeometry();
        }
    }
    static constexpr int kCount = 4;
    // 쟁반의 높이 0.05보다 약간 넓게 두어 표면이 겹쳐 깜빡이지 않게 한다.
    static constexpr float kSpacing = 0.051f;
private:
    glm::mat4 Layer(int i) const { return base * glm::translate(glm::mat4(1.0f), glm::vec3(0, i * kSpacing, 0)); }
    std::unique_ptr<Model> tray;
    glm::mat4 base{1.0f};
    std::function<void()> dispense;
};

class TrayPickup;

// 탁자 전체를 조준할 수 있고, 트레이는 윗면 중앙에 놓는다.
class TrayPlacementSurface : public InteractableObject {
public:
    TrayPlacementSurface(const Collider& bounds, const glm::vec3& topCenter)
        : bounds(bounds), topCenter(topCenter) {}
    Collider GetWorldCollider() const override { return bounds; }
    bool CanFocus(const Player& player) const override;
    glm::mat4 Placement(const Model& tray) const {
        return glm::translate(glm::mat4(1.0f), topCenter + glm::vec3(0, 0.005f - tray.GetBoundsMin().y, 0));
    }
    void SetPlaced(TrayPickup* tray) { placed = tray; }
private:
    Collider bounds;
    glm::vec3 topCenter;
    TrayPickup* placed = nullptr; // 상호작용 배열이 소유하고, 씬 종료까지 유지된다.
};

// 트레이의 넓은 윗면이 1인칭 화면에서 보이도록 기울인다.
class TrayPickup : public PickupObject {
public:
    explicit TrayPickup(std::unique_ptr<Model> model) : PickupObject(std::move(model), glm::mat4(1.0f)) {}
    void UseOn(Player& player, InteractableObject* target) override {
        auto* surface = dynamic_cast<TrayPlacementSurface*>(target);
        if (!surface || !surface->CanFocus(player)) return;
        SetWorldMatrix(surface->Placement(GetModel()));
        surface->SetPlaced(this);
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
        matrix[3] = glm::vec4(player.camera.position + forward * 0.75f + right * 0.27f - up * 0.20f, 1.0f);
        matrix = glm::rotate(matrix, glm::radians(55.0f), glm::vec3(1, 0, 0));
        const glm::vec3 center = (GetModel().GetBoundsMin() + GetModel().GetBoundsMax()) * 0.5f;
        SetHeldMatrix(glm::translate(matrix, -center));
    }
    void Drop(Player& player) override {
        const glm::vec3 forward = player.camera.GetFlatForward();
        const glm::vec3 right = player.camera.GetRight();
        glm::mat4 matrix(1.0f);
        matrix[0] = glm::vec4(right, 0.0f);
        matrix[1] = glm::vec4(0, 1, 0, 0);
        matrix[2] = glm::vec4(-forward, 0.0f);
        matrix[3] = glm::vec4(player.transform.position + forward * 0.85f + glm::vec3(0, 0.02f - GetModel().GetBoundsMin().y, 0), 1.0f);
        SetWorldMatrix(matrix);
        player.heldObject = nullptr;
    }
};

inline bool TrayPlacementSurface::CanFocus(const Player& player) const {
    if (!dynamic_cast<TrayPickup*>(player.heldObject)) return false;
    return !placed || placed == player.heldObject || !placed->GetWorldCollider().Intersects(bounds);
}
