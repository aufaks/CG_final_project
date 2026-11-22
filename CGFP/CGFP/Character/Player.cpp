#include "Character/Player.h"

#include "Core/InputManager.h"
#include "Render/Mesh.h"
#include "Render/ResourceManager.h"
#include "Render/ShaderManager.h"
#include "Render/Texture.h"

#include <cmath>

namespace {
constexpr float kEyeHeightRatio = 0.88f;     // 키 대비 카메라 높이 (모델의 실제 눈 0.78보다 조금 높게)
constexpr float kEyeForwardMargin = 0.02f;   // 코끝보다 키 대비 이만큼 더 앞에 카메라를 둔다
constexpr float kThirdPersonDistance = 3.0f; // V로 바꾸는 3인칭 확인용 카메라 거리
constexpr float kThirdPersonHeight = 0.6f;
// 1인칭 near plane(키 대비). 코(키의 0.68~0.75)는 카메라 바로 아래·뒤에 있어서 내려다봐도 깊이가
// 이 값보다 작아 잘린다. 눈 높이를 올리면 코까지 거리가 늘어나므로 같이 키워야 한다.
// 몸통 앞면(0.55 아래)·바지·다리는 더 멀리 있어서 그대로 보인다.
constexpr float kFirstPersonNearRatio = 0.21f;
constexpr float kThirdPersonNear = 0.05f;

// 걷기 애니메이션 (각도는 도, 길이는 OBJ 좌표 = 모델 키 약 1)
constexpr float kWalkCycleSpeed = 9.0f;  // 걸음 주기 속도(라디안/초). 클수록 종종걸음
constexpr float kWalkBlendSpeed = 8.0f;  // 걷기↔서기 전환 속도
constexpr float kArmDropAngle = 70.0f;   // OBJ는 T포즈라 팔을 이만큼 내린 것이 기본 자세
constexpr float kArmSwingAngle = 40.0f;
constexpr float kLegSwingAngle = 35.0f;
constexpr float kBodyBobHeight = 0.03f;  // 한 걸음마다 통통 튀는 높이
constexpr float kBodySwayAngle = 4.0f;   // 좌우로 기우뚱
constexpr float kBodyTwistAngle = 6.0f;  // 걸을 때 몸통 비틀기
constexpr float kHipHeightRatio = 0.24f; // 바지 아랫단 높이(키 대비). 이보다 아래 삼각형은 다리

// Idle 애니메이션: 숨 쉬듯 몸이 살짝 오르내리고, 팔이 느리게 흔들리고, 몸이 조금 기우뚱한다
constexpr float kIdleBreathSpeed = 2.4f;  // 라디안/초 (약 2.6초에 한 번 숨)
constexpr float kIdleBreathHeight = 0.012f;
constexpr float kIdleArmSwayAngle = 6.0f;
constexpr float kIdleArmSwaySpeed = 1.3f;
constexpr float kIdleBodySwayAngle = 2.0f;
constexpr float kIdleBodySwaySpeed = 0.9f;

// spongebob_bind.obj의 그룹: 3D_Object_32 = -X 쪽 팔(오른팔), 3D_Object_42 = +X 쪽 팔(왼팔), 3D_Object_52 = 몸통+다리
constexpr const char* kRightArmGroup = "3D_Object_32";
constexpr const char* kLeftArmGroup = "3D_Object_42";

glm::mat4 RotateAround(const glm::vec3& pivot, float degrees, const glm::vec3& axis) {
    return glm::translate(glm::mat4(1.0f), pivot) * glm::rotate(glm::mat4(1.0f), glm::radians(degrees), axis) *
           glm::translate(glm::mat4(1.0f), -pivot);
}
} // namespace

Player::Player() = default;
Player::~Player() = default;

bool Player::LoadModel(const std::string& objPath, const std::string& texturePath, float height) {
    mesh = ResourceManager::Get().GetMesh(objPath);
    if (!mesh) return false;
    texture = ResourceManager::Get().GetTexture(texturePath);
    color = glm::vec3(1.0f);

    // 발이 원점, 가로·앞뒤 중심이 원점에 오게 옮기고 키를 height로 맞춘다
    const glm::vec3 boundsMin = mesh->GetBoundsMin();
    const glm::vec3 boundsMax = mesh->GetBoundsMax();
    const float modelHeight = boundsMax.y - boundsMin.y;
    const float scale = modelHeight > 0.0f ? height / modelHeight : 1.0f;
    const glm::vec3 pivot((boundsMin.x + boundsMax.x) * 0.5f, boundsMin.y, (boundsMin.z + boundsMax.z) * 0.5f);
    modelOffset = glm::scale(glm::mat4(1.0f), glm::vec3(scale)) * glm::translate(glm::mat4(1.0f), -pivot);

    // 관절 애니메이션용으로 부위를 나눠 읽는다. 다리는 몸통과 한 덩어리라 높이(골반 아래)와 좌우로 자른다.
    const float hipY = boundsMin.y + modelHeight * kHipHeightRatio;
    const float centerX = pivot.x;
    parts = Mesh::LoadObjParts(objPath, PartCount, [&](const std::string& group, const glm::vec3& c) {
        if (group == kRightArmGroup) return static_cast<int>(RightArm);
        if (group == kLeftArmGroup) return static_cast<int>(LeftArm);
        if (c.y < hipY) return static_cast<int>(c.x < centerX ? RightLeg : LeftLeg);
        return static_cast<int>(Body);
    });
    bool partsOk = parts.size() == PartCount;
    for (const auto& part : parts) partsOk = partsOk && !part->IsEmpty();
    if (partsOk) {
        // 어깨: 팔에서 몸통 쪽 끝. 골반: 다리 윗면 중앙.
        const Mesh& rArm = *parts[RightArm];
        const Mesh& lArm = *parts[LeftArm];
        const glm::vec3 rMid = (rArm.GetBoundsMin() + rArm.GetBoundsMax()) * 0.5f;
        const glm::vec3 lMid = (lArm.GetBoundsMin() + lArm.GetBoundsMax()) * 0.5f;
        joints[RightArm] = glm::vec3(rArm.GetBoundsMax().x, rMid.y, rMid.z);
        joints[LeftArm] = glm::vec3(lArm.GetBoundsMin().x, lMid.y, lMid.z);
        for (Part leg : { RightLeg, LeftLeg }) {
            const Mesh& m = *parts[leg];
            const glm::vec3 mid = (m.GetBoundsMin() + m.GetBoundsMax()) * 0.5f;
            joints[leg] = glm::vec3(mid.x, m.GetBoundsMax().y, mid.z);
        }
        joints[Body] = (joints[RightLeg] + joints[LeftLeg]) * 0.5f;
    } else {
        parts.clear(); // 나누기 실패 → 통짜 메시로 그린다
    }

    eyeHeight = height * kEyeHeightRatio;
    // 1인칭 카메라를 얼굴 앞(코끝 바로 앞)에 둔다. 몸 안에 두면 내려다볼 때 몸 안쪽 면이 보인다.
    eyeForward = (boundsMax.z - pivot.z) * scale + height * kEyeForwardMargin;
    firstPersonNear = height * kFirstPersonNearRatio;
    return true;
}

void Player::Spawn(const glm::vec3& feetPosition, float yawDegrees) {
    transform.position = feetPosition;
    camera.yaw = yawDegrees;
    camera.pitch = 0.0f;
    SyncCamera();
}

void Player::Teleport(const glm::vec3& feetPosition) {
    transform.position = feetPosition;
    SyncCamera();
}

void Player::Update(float deltaTime) {
    if (InputManager::GetKeyDown(GLFW_KEY_V)) firstPerson = !firstPerson;
    UpdateLook();
    const glm::vec3 before = transform.position;
    UpdateMovement(deltaTime);
    UpdateWalkAnimation(deltaTime, transform.position != before);
    SyncCamera();
}

void Player::UpdateLook() {
    if (!InputManager::IsCursorCaptured()) return;
    const glm::vec2 delta = InputManager::GetMouseDelta();
    camera.AddRotation(delta.x * mouseSensitivity, -delta.y * mouseSensitivity);
}

void Player::UpdateMovement(float deltaTime) {
    glm::vec3 move(0.0f);
    if (InputManager::GetKey(GLFW_KEY_W)) move += camera.GetFlatForward();
    if (InputManager::GetKey(GLFW_KEY_S)) move -= camera.GetFlatForward();
    if (InputManager::GetKey(GLFW_KEY_D)) move += camera.GetRight();
    if (InputManager::GetKey(GLFW_KEY_A)) move -= camera.GetRight();
    if (glm::length(move) > 0.0f) transform.position += glm::normalize(move) * moveSpeed * deltaTime;
}

void Player::UpdateWalkAnimation(float deltaTime, bool moving) {
    // 상태 머신: 움직이면 Walk, 멈추면 Idle. 자세는 walkBlend로 두 상태 사이를 부드럽게 섞는다.
    const State next = moving ? State::Walk : State::Idle;
    if (next != state) {
        state = next;
        stateTime = 0.0f;
    }
    stateTime += deltaTime;
    idleTime += deltaTime;

    const float target = moving ? 1.0f : 0.0f;
    walkBlend += (target - walkBlend) * glm::min(1.0f, deltaTime * kWalkBlendSpeed);
    if (moving) walkPhase = std::fmod(walkPhase + kWalkCycleSpeed * deltaTime, 6.28318531f);
}

glm::mat4 Player::GetPartMatrix(int part) const {
    const float s = std::sin(walkPhase) * walkBlend;
    const glm::vec3 X(1, 0, 0), Y(0, 1, 0), Z(0, 0, 1);

    // Idle 동작은 서 있을수록(1 - walkBlend) 크게 섞인다
    const float idle = 1.0f - walkBlend;
    const float breath = kIdleBreathHeight * idle * (0.5f + 0.5f * std::sin(idleTime * kIdleBreathSpeed));
    const float idleArm = kIdleArmSwayAngle * idle * std::sin(idleTime * kIdleArmSwaySpeed);
    const float idleSway = kIdleBodySwayAngle * idle * std::sin(idleTime * kIdleBodySwaySpeed);

    // 다리가 모일 때(phase 0, π) 가장 높이 뜬다 → 한 걸음에 두 번 통통
    const float bob = kBodyBobHeight * walkBlend * (0.5f + 0.5f * std::cos(2.0f * walkPhase));
    const glm::mat4 root = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, bob, 0.0f));
    const glm::mat4 upper = root * glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, breath, 0.0f)) *
                            RotateAround(joints[Body], kBodySwayAngle * s + idleSway, Z) *
                            RotateAround(joints[Body], kBodyTwistAngle * s, Y);

    // X축 +회전은 아래로 뻗은 팔다리를 뒤로 보낸다. 오른다리가 앞으로 갈 때 오른팔은 뒤로.
    // Idle에서는 두 팔이 같은 방향으로 살짝 앞뒤로 흔들린다.
    switch (part) {
    case RightLeg: return root * RotateAround(joints[RightLeg], -kLegSwingAngle * s, X);
    case LeftLeg: return root * RotateAround(joints[LeftLeg], kLegSwingAngle * s, X);
    case RightArm: // -X 쪽 팔: Z축 +회전이 아래로 내림
        return upper * RotateAround(joints[RightArm], kArmSwingAngle * s + idleArm, X) * RotateAround(joints[RightArm], kArmDropAngle, Z);
    case LeftArm:
        return upper * RotateAround(joints[LeftArm], -kArmSwingAngle * s + idleArm, X) * RotateAround(joints[LeftArm], -kArmDropAngle, Z);
    default: return upper;
    }
}

void Player::SyncCamera() {
    // 모델 정면(+Z)이 카메라의 수평 전방(cos yaw, 0, sin yaw)을 보도록 Y축 회전
    transform.rotation.y = 90.0f - camera.yaw;

    const glm::vec3 eye = transform.position + glm::vec3(0.0f, eyeHeight, 0.0f);
    if (firstPerson) {
        camera.position = eye + camera.GetFlatForward() * eyeForward;
        camera.nearPlane = firstPersonNear;
    } else {
        camera.nearPlane = kThirdPersonNear;
        camera.position = eye - camera.GetForward() * kThirdPersonDistance + glm::vec3(0.0f, kThirdPersonHeight, 0.0f);
    }
}

void Player::Render(ShaderManager& shaders) const {
    if (!visible || !mesh) return;
    shaders.SetMat4("uModel", transform.GetMatrix() * modelOffset);
    shaders.SetVec3("uColor", color);
    shaders.SetFloat("uOpacity", 1.0f);
    const bool useTexture = texture && texture->IsLoaded();
    shaders.SetBool("uUseTexture", useTexture);
    if (useTexture) texture->Bind(0);

    if (parts.empty()) {
        mesh->Draw();
        return;
    }
    const glm::mat4 base = transform.GetMatrix() * modelOffset;
    for (int i = 0; i < PartCount; ++i) {
        shaders.SetMat4("uModel", base * GetPartMatrix(i));
        parts[i]->Draw();
    }
}
