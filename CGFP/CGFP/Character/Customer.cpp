#include "Character/Customer.h"

#include "Render/ShaderManager.h"

#include <algorithm>
#include <cmath>

namespace {
// 클립 이름 (tools/mixamo_to_gltf.py가 FBX 파일 이름을 그대로 쓴다)
const char* kIdleClip = "Idle";
const char* kWalkClip = "Walk";
const char* kStandToSitClip = "StandToSit";
const char* kSitIdleClip = "SitIdle";

constexpr float kClipFade = 0.25f;
// Mixamo Walk(제자리)를 1배속으로 틀 때 키 1 단위 모델이 실제로 나아가야 하는 속도. 발 미끄러짐을 맞추는 값.
constexpr float kWalkSpeedPerHeight = 0.8f;
// 앉은 자세에서 엉덩이 아래로 이 반경(키 대비, 수평) 안의 메시가 좌석 면 위에 오도록 한다
constexpr float kSitContactRadius = 0.12f;
constexpr float kSitClearance = 0.01f; // 좌석 면과 살짝 띄운다 (월드)
constexpr float kArriveDistance = 0.03f;

float WrapAngle(float degrees) {
    degrees = std::fmod(degrees + 180.0f, 360.0f);
    if (degrees < 0.0f) degrees += 360.0f;
    return degrees - 180.0f;
}

float YawOf(const glm::vec3& direction) {
    return glm::degrees(std::atan2(direction.x, direction.z));
}
} // namespace

// ---------------------------------------------------------------- CustomerModel

bool CustomerModel::Load(const CustomerModelDesc& desc) {
    if (!skinned.Load(desc.gltfPath)) return false;
    const glm::vec3 size = skinned.GetBoundsMax() - skinned.GetBoundsMin();
    scale = size.y > 0.0f ? desc.height / size.y : 1.0f;
    width = size.x * scale;
    depth = size.z * scale;
    walkSpeed = kWalkSpeedPerHeight * desc.height;

    // 앉은 자세의 엉덩이 위치
    hasSitHips = false;
    const int hips = skinned.FindNode("Hips");
    if (const SkinnedModel::Clip* sit = skinned.FindClip(kSitIdleClip); sit && hips >= 0) {
        std::vector<NodePose> pose = skinned.GetRestPose();
        skinned.SampleClip(*sit, 0.0f, pose);
        std::vector<glm::mat4> globals, bones;
        skinned.ComputeGlobalMatrices(pose, globals);
        skinned.ComputeBoneMatrices(globals, bones);
        sitHips = glm::vec3(globals[hips][3]);

        // 엉덩이 바로 아래(수평 반경 안)의 가장 낮은 정점 = 좌석에 닿는 곳. 앞으로 뻗은 다리·발은 반경 밖이라 빠진다.
        std::vector<glm::vec3> posed;
        skinned.SkinPositions(bones, posed);
        const float radius = kSitContactRadius * size.y;
        sitBottomY = sitHips.y;
        for (const glm::vec3& p : posed) {
            const glm::vec2 d(p.x - sitHips.x, p.z - sitHips.z);
            if (glm::dot(d, d) <= radius * radius) sitBottomY = std::min(sitBottomY, p.y);
        }
        hasSitHips = true;
    }
    return true;
}

// ---------------------------------------------------------------- Customer

Customer::Customer(const CustomerModel& model) : model(model) {
    animator.SetModel(&model.skinned);
    animator.Play(kIdleClip, true, 0.0f);
    animator.Update(0.0f);
}

void Customer::SetPath(const std::vector<glm::vec3>& points) {
    path.assign(points.begin(), points.end());
    if (!path.empty()) destination = path.back();
}

void Customer::MoveTo(const glm::vec3& point) {
    path.clear();
    path.push_back(point);
    destination = point;
}

void Customer::SitDown(const glm::vec3& seatPosition, float yawDegrees, float height) {
    path.clear();
    transform.position = seatPosition;
    destination = seatPosition;
    targetYaw = yawDegrees;
    transform.rotation.y = yawDegrees;
    seatHeight = height;
    anim = Anim::Sit;
    if (!animator.Play(kStandToSitClip, false, kClipFade)) animator.Play(kSitIdleClip, true, kClipFade);
}

void Customer::Update(float deltaTime) {
    stateTime += deltaTime;
    const glm::vec3 before = transform.position;
    UpdateMovement(deltaTime);
    UpdateAnimation(deltaTime, transform.position != before);
}

void Customer::UpdateMovement(float deltaTime) {
    float yaw = transform.rotation.y;
    if (!path.empty() && anim != Anim::Sit) {
        float budget = moveSpeed * deltaTime;
        while (!path.empty() && budget > 0.0f) {
            glm::vec3 toTarget = path.front() - transform.position;
            toTarget.y = 0.0f;
            const float distance = glm::length(toTarget);
            if (distance <= kArriveDistance) {
                transform.position.x = path.front().x;
                transform.position.z = path.front().z;
                path.pop_front();
                continue;
            }
            const float step = std::min(budget, distance);
            transform.position += toTarget / distance * step;
            budget -= step;
            targetYaw = YawOf(toTarget);
        }
    }

    // 목표 방향으로 천천히 돈다
    const float diff = WrapAngle(targetYaw - yaw);
    const float maxTurn = turnSpeed * deltaTime;
    yaw += glm::clamp(diff, -maxTurn, maxTurn);
    transform.rotation.y = WrapAngle(yaw);
}

void Customer::UpdateAnimation(float deltaTime, bool moving) {
    if (anim == Anim::Sit) {
        // 앉기 클립이 끝나면 앉아 있기 반복. 그동안 엉덩이를 좌석 위로 서서히 옮긴다.
        animator.speed = 1.0f;
        if (animator.GetClipName() == kStandToSitClip) {
            sitBlend = animator.GetProgress();
            if (animator.IsFinished()) animator.Play(kSitIdleClip, true, kClipFade);
        } else {
            sitBlend = 1.0f;
        }
    } else {
        anim = moving ? Anim::Walk : Anim::Idle;
        if (anim == Anim::Walk) {
            animator.Play(kWalkClip, true, kClipFade);
            animator.speed = model.walkSpeed > 0.0f ? moveSpeed / model.walkSpeed : 1.0f;
        } else {
            animator.Play(kIdleClip, true, kClipFade);
            animator.speed = 1.0f;
        }
    }
    animator.Update(deltaTime);
}

glm::vec3 Customer::GetSitOffset() const {
    if (anim != Anim::Sit || !model.hasSitHips) return glm::vec3(0.0f);
    // SitIdle의 엉덩이가 좌석 중심 바로 위에, 엉덩이 아래 가장 낮은 곳(엉덩이·치마 끝)이 좌석 면에 오도록
    const glm::vec3 hips = model.sitHips * model.scale;
    const float bottom = model.sitBottomY * model.scale;
    const glm::vec3 local(-hips.x, seatHeight + kSitClearance - bottom, -hips.z);
    const glm::mat4 yaw = glm::rotate(glm::mat4(1.0f), glm::radians(transform.rotation.y), glm::vec3(0, 1, 0));
    return glm::vec3(yaw * glm::vec4(local, 0.0f)) * sitBlend;
}

void Customer::Render(ShaderManager& shaders) const {
    if (!visible || !model.skinned.IsLoaded()) return;
    const glm::mat4 world = glm::translate(glm::mat4(1.0f), GetSitOffset()) * transform.GetMatrix() *
                            glm::scale(glm::mat4(1.0f), glm::vec3(model.scale));
    model.skinned.Draw(shaders, world, animator.GetBones());
}
