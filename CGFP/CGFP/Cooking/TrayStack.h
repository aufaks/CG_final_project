#pragma once
#include "Object/PickupObject.h"
#include "Cooking/Ingredient.h"
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
    Collider GetWorldCollider() const override {
        glm::vec3 min = GetModel().GetBoundsMin() - glm::vec3(0.01f, 0.06f, 0.01f);
        glm::vec3 max = GetModel().GetBoundsMax() + glm::vec3(0.01f, 0.06f, 0.01f);
        max.y = glm::max(max.y, stackTop + 0.03f);
        return Collider::FromLocalBounds(min, max, GetMatrix());
    }
    void AddIngredient(const Model& model, IngredientType type) {
        AddLayer(&model, type, nullptr);
    }
    bool AddSauce(IngredientType type) {
        auto sauce = std::make_unique<Model>();
        const char* path = type == IngredientType::Ketchup
            ? "obj/imported/burger-ingredients/KetchupPoured.obj"
            : "obj/imported/burger-ingredients/MustardPoured.obj";
        if (!sauce->LoadObj(path)) return false;
        const Model* model = sauce.get();
        AddLayer(model, type, std::move(sauce));
        return true;
    }
    void DrawContents(ShaderManager& shaders, const glm::vec3& camera) const {
        for (const auto& layer : layers)
            layer.model->Draw(shaders, GetMatrix() * layer.localMatrix, camera);
    }
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
private:
    struct Layer {
        const Model* model = nullptr; // 일반 재료는 상호작용 배열이 소유한다.
        std::unique_ptr<Model> sauce; // 뿌려진 소스는 트레이가 소유한다.
        IngredientType type = IngredientType::Bottom_Bun;
        glm::mat4 localMatrix{1.0f};
    };
    void AddLayer(const Model* model, IngredientType type, std::unique_ptr<Model> owned) {
        const glm::vec3 min = model->GetBoundsMin();
        const glm::vec3 max = model->GetBoundsMax();
        float heightScale = 1.0f;
        float heightContribution = 1.0f;
        switch (type) {
        case IngredientType::Top_Bun:
            heightScale = 0.65f;
            break;
        case IngredientType::Lettuce:
            heightScale = 0.75f;
            heightContribution = 0.8f;
            break;
        case IngredientType::Cheese:
        case IngredientType::Pickle:
        case IngredientType::Tomato:
        case IngredientType::Onion:
            heightContribution = 0.8f;
            break;
        default:
            break;
        }
        const bool sauce = type == IngredientType::Ketchup || type == IngredientType::Mustard;
        const float gap = sauce ? 0.0008f : 0.0005f;
        const float y = stackTop + gap - min.y * heightScale;
        const glm::vec3 center((min.x + max.x) * 0.5f, 0.0f, (min.z + max.z) * 0.5f);
        Layer layer;
        layer.model = model;
        layer.sauce = std::move(owned);
        layer.type = type;
        layer.localMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(-center.x, y, -center.z)) *
            glm::scale(glm::mat4(1.0f), glm::vec3(1.0f, heightScale, 1.0f));
        // 채소는 다음 재료에 살짝 눌리고, 평면 소스는 높이를 거의 차지하지 않는다.
        stackTop += gap + (sauce ? 0.0f : (max.y - min.y) * heightScale * heightContribution);
        layers.push_back(std::move(layer));
    }
    std::vector<Layer> layers;
    float stackTop = 0.018f; // 쟁반 안쪽 바닥 높이
};

inline bool TrayPlacementSurface::CanFocus(const Player& player) const {
    if (!dynamic_cast<TrayPickup*>(player.heldObject)) return false;
    return !placed || placed == player.heldObject || !placed->GetWorldCollider().Intersects(bounds);
}
