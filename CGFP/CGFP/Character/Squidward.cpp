#include "Character/Squidward.h"

#include "Order/Order.h"
#include "Render/Mesh.h"
#include "Render/ResourceManager.h"
#include "Render/ShaderManager.h"

#include <cmath>

namespace {
// glTF 클립 이름 (tools/squidward_to_gltf.py)
const char* kIdleClip = "Idle";
const char* kSighClip = "Sigh";
const char* kTakeOrderClip = "TakeOrder";
const char* kSubmitOrderClip = "SubmitOrder";
constexpr float kClipFade = 0.25f;

// 손에 든 영수증 크기 (월드)
const glm::vec3 kReceiptSize(0.08f, 0.11f, 0.004f);
} // namespace

Squidward::Squidward() = default;
Squidward::~Squidward() = default;

bool Squidward::LoadModel(const std::string& gltfPath, float height) {
    auto model = std::make_unique<SkinnedModel>();
    if (!model->Load(gltfPath) || !model->FindClip(kIdleClip)) return false;
    skinned = std::move(model);
    rootNode = skinned->FindNode("Root");
    rightHandNode = skinned->FindNode("RightHand");

    // 발이 원점, 가로·앞뒤 중심이 원점에 오게 옮기고 키를 height로 맞춘다 (바인드 자세 기준)
    const glm::vec3 boundsMin = skinned->GetBoundsMin();
    const glm::vec3 boundsMax = skinned->GetBoundsMax();
    const float modelHeight = boundsMax.y - boundsMin.y;
    const float scale = modelHeight > 0.0f ? height / modelHeight : 1.0f;
    const glm::vec3 pivot((boundsMin.x + boundsMax.x) * 0.5f, boundsMin.y, (boundsMin.z + boundsMax.z) * 0.5f);
    modelOffset = glm::scale(glm::mat4(1.0f), glm::vec3(scale)) * glm::translate(glm::mat4(1.0f), -pivot);

    animator.SetModel(skinned.get());
    handlingOrder = nullptr;
    receiptInHand = false;
    EnterState(State::Idle);
    animator.Update(0.0f);
    return true;
}

void Squidward::Unload() {
    animator.SetModel(nullptr);
    skinned.reset();
}

void Squidward::Place(const glm::vec3& feetPosition, float yawDegrees) {
    transform.position = feetPosition;
    transform.rotation.y = yawDegrees;
}

bool Squidward::TakeOrder(Order* order) {
    if (!order || !CanTakeOrder()) return false;
    handlingOrder = order;
    handlingOrder->state = OrderState::Taking;
    EnterState(State::TakeOrder);
    return true;
}

void Squidward::EnterState(State next) {
    state = next;
    stateTime = 0.0f;
    switch (state) {
    case State::Idle: {
        std::uniform_real_distribution<float> wait(minIdleSeconds, maxIdleSeconds);
        idleDuration = wait(random);
        animator.Play(kIdleClip, true, kClipFade);
        break;
    }
    case State::Sigh: animator.Play(kSighClip, false, kClipFade, true); break;
    case State::TakeOrder: animator.Play(kTakeOrderClip, false, kClipFade, true); break;
    case State::SubmitOrder: animator.Play(kSubmitOrderClip, false, kClipFade, true); break;
    }
}

void Squidward::Update(float deltaTime) {
    if (!skinned) return;
    stateTime += deltaTime;
    animator.Update(deltaTime);
    const float progress = animator.GetProgress();
    switch (state) {
    case State::Idle:
        if (stateTime >= idleDuration) EnterState(State::Sigh);
        break;
    case State::Sigh:
        if (animator.IsFinished()) EnterState(State::Idle);
        break;
    case State::TakeOrder:
        if (progress >= receiptAppear) receiptInHand = true;
        if (animator.IsFinished()) {
            if (handlingOrder) handlingOrder->state = OrderState::Taken; // 손님은 이제 자리로 간다
            EnterState(State::SubmitOrder);
        }
        break;
    case State::SubmitOrder:
        if (receiptInHand && progress >= receiptRelease) {
            receiptInHand = false;
            if (handlingOrder && onReceiptSubmitted) onReceiptSubmitted(*handlingOrder);
        }
        if (animator.IsFinished()) {
            handlingOrder = nullptr;
            EnterState(State::Idle);
        }
        break;
    }
}

glm::mat4 Squidward::GetModelMatrix() const {
    return transform.GetMatrix() * modelOffset;
}

void Squidward::Render(ShaderManager& shaders) const {
    if (!visible || !skinned) return;
    skinned->Draw(shaders, GetModelMatrix(), animator.GetBones());
}

void Squidward::RenderProps(ShaderManager& shaders) const {
    // 손에 든 영수증: 오른손 끝(RightHand 노드)에 매달린 하얀 종이
    if (!visible || !skinned || !receiptInHand || rightHandNode < 0) return;
    Mesh* paper = ResourceManager::Get().GetCube();
    if (!paper) return;
    const glm::vec3 hand = glm::vec3(GetModelMatrix() * glm::vec4(animator.GetNodePosition(rightHandNode), 1.0f));
    // 클립의 Root 회전(뒤돌기)까지 더한 몸 방향
    float yaw = transform.rotation.y;
    if (rootNode >= 0) {
        const glm::vec3 forward = glm::vec3(animator.GetNodeMatrix(rootNode)[2]);
        yaw += glm::degrees(std::atan2(forward.x, forward.z));
    }
    const glm::mat4 receipt = glm::translate(glm::mat4(1.0f), hand - glm::vec3(0.0f, kReceiptSize.y * 0.45f, 0.0f)) *
                              glm::rotate(glm::mat4(1.0f), glm::radians(yaw), glm::vec3(0, 1, 0)) *
                              glm::rotate(glm::mat4(1.0f), glm::radians(-12.0f), glm::vec3(1, 0, 0)) *
                              glm::scale(glm::mat4(1.0f), kReceiptSize);
    shaders.SetMat4("uModel", receipt);
    shaders.SetVec3("uColor", glm::vec3(1.0f));
    shaders.SetFloat("uOpacity", 1.0f);
    shaders.SetBool("uUseTexture", false);
    paper->Draw();
}
