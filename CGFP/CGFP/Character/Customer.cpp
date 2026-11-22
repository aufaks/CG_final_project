#include "Character/Customer.h"

#include "Render/ShaderManager.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <iterator>

namespace {
// 클립 이름 (tools/mixamo_to_gltf.py가 FBX 파일 이름을 그대로 쓴다)
const char* kIdleClip = "Idle";
const char* kWalkClip = "Walk";
const char* kStandToSitClip = "StandToSit";
const char* kSitIdleClip = "SitIdle";

constexpr float kClipFade = 0.25f;
// Mixamo Walk(제자리)를 1배속으로 틀 때 키 1 단위 모델이 실제로 나아가야 하는 속도. 발 미끄러짐을 맞추는 값.
constexpr float kWalkSpeedPerHeight = 0.8f;
constexpr float kSitClearance = -0.05f; // 엉덩이·허벅지 아랫면과 좌석 면 사이 간격 (월드). +면 뜨고 -면 파묻힌다
constexpr float kSitForward = 0.07f;    // 좌석 중심에서 손님 정면(테이블) 쪽으로 더 나가 앉는 거리 (월드)
// 무릎 펴기 각도 찾기: 무릎(Leg 뼈) 로컬 X축 + 방향이 펴는 쪽 (Mixamo 뼈대)
constexpr float kKneeStep = 5.0f;
constexpr float kKneeMax = 100.0f;
constexpr float kKneeSinkTolerance = 0.07f; // 좌석 면보다 이만큼(월드) 아래까지는 좌석 위에 얹힌 것으로 본다 (kSitClearance로 파묻힌 만큼 포함)
constexpr float kKneeAllowedRatio = 0.01f;  // 좌석 안에 남아도 되는 종아리·발 정점 비율
constexpr float kSitBottomPercentile = 0.02f; // 아랫면 높이: 엉덩이·허벅지 정점 중 아래에서 이 비율 (튀어나온 정점 몇 개는 무시)
constexpr float kSitContactBand = 0.06f;      // 아랫면에서 이 높이(키 대비)까지를 좌석에 닿는 면으로 본다
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

    // 앉은 자세의 엉덩이 위치와 좌석에 닿는 면
    hasSitHips = false;
    const int hips = skinned.FindNode("Hips");
    if (const SkinnedModel::Clip* sit = skinned.FindClip(kSitIdleClip); sit && hips >= 0) {
        std::vector<NodePose> pose = skinned.GetRestPose();
        skinned.SampleClip(*sit, 0.0f, pose);
        std::vector<glm::mat4> globals;
        skinned.ComputeGlobalMatrices(pose, globals);
        sitHips = glm::vec3(globals[hips][3]);
        hipsNode = hips;
        hasSitHips = MeasureSitContact(globals, size.y);
    }
    kneeNodes[0] = skinned.FindNode("LeftLeg");
    kneeNodes[1] = skinned.FindNode("RightLeg");
    return true;
}

void CustomerModel::FitSeat(float seatHeight, float seatRadius) {
    sitKneeAngle = 0.0f;
    const SkinnedModel::Clip* sit = skinned.FindClip(kSitIdleClip);
    if (!sit || !hasSitHips || kneeNodes[0] < 0 || kneeNodes[1] < 0 || scale <= 0.0f) return;

    // 종아리·발 정점 (무릎·발·발가락 뼈에 주로 붙은 것)
    const int lowerNodes[] = { kneeNodes[0], kneeNodes[1], skinned.FindNode("LeftFoot"), skinned.FindNode("RightFoot"),
                               skinned.FindNode("LeftToeBase"), skinned.FindNode("RightToeBase") };
    const std::vector<int>& jointNodes = skinned.GetJointNodes();
    const std::vector<glm::uvec4>& vertexJoints = skinned.GetCpuJoints();
    const std::vector<glm::vec4>& weights = skinned.GetCpuWeights();
    std::vector<size_t> lower;
    for (size_t v = 0; v < weights.size(); ++v) {
        int best = 0;
        for (int k = 1; k < 4; ++k)
            if (weights[v][k] > weights[v][best]) best = k;
        const unsigned joint = vertexJoints[v][best];
        if (joint < jointNodes.size() &&
            std::find(std::begin(lowerNodes), std::end(lowerNodes), jointNodes[joint]) != std::end(lowerNodes))
            lower.push_back(v);
    }
    if (lower.empty()) return;

    // GetSitOffset과 같은 자리 맞춤 (sitBlend = 1, 모델 공간). 무릎만 펴므로 엉덩이 쪽은 각도와 상관없다.
    const glm::vec3 contact = sitHips + sitContact;
    const glm::vec3 offset(-contact.x, (seatHeight + kSitClearance) / scale - sitBottomY, kSitForward / scale - contact.z);
    const float limitY = (seatHeight - kKneeSinkTolerance) / scale;
    const float radius = seatRadius / scale;
    const size_t allowed = static_cast<size_t>(lower.size() * kKneeAllowedRatio);

    std::vector<NodePose> pose;
    std::vector<glm::mat4> globals, bones;
    std::vector<glm::vec3> posed;
    for (float angle = 0.0f; angle <= kKneeMax; angle += kKneeStep) {
        pose = skinned.GetRestPose();
        skinned.SampleClip(*sit, 0.0f, pose);
        const glm::quat extend = glm::angleAxis(glm::radians(angle), glm::vec3(1.0f, 0.0f, 0.0f));
        for (int knee : kneeNodes) pose[knee].rotation = pose[knee].rotation * extend;
        skinned.ComputeGlobalMatrices(pose, globals);
        skinned.ComputeBoneMatrices(globals, bones);
        skinned.SkinPositions(bones, posed);
        size_t inside = 0;
        for (size_t v : lower) {
            const glm::vec3 p = posed[v] + offset;
            if (p.y < limitY && glm::length(glm::vec2(p.x, p.z)) < radius) ++inside;
        }
        sitKneeAngle = angle;
        if (inside <= allowed) break;
    }
}

bool CustomerModel::MeasureSitContact(const std::vector<glm::mat4>& globals, float modelHeight) {
    // 앉은 자세(SitIdle 첫 프레임) 메시를 직접 스키닝해서 좌석에 닿는 곳(엉덩이·허벅지 아랫면)을 잰다.
    // 뼈 위치로 어림하면 몸 생김새(뒤로 튀어나온 몸통, 두꺼운 허벅지)를 못 따라가서 몸이 뜨거나 좌석 밖으로 밀린다.
    std::vector<glm::mat4> bones;
    skinned.ComputeBoneMatrices(globals, bones);
    std::vector<glm::vec3> posed;
    skinned.SkinPositions(bones, posed);

    // 엉덩이·허리·허벅지 뼈에 주로 붙은 정점 (종아리·발은 좌석 앞으로 내려와 있어서 뺀다)
    const int contactNodes[] = { skinned.FindNode("Hips"), skinned.FindNode("Spine"), skinned.FindNode("LeftUpLeg"),
                                 skinned.FindNode("RightUpLeg") };
    const std::vector<int>& jointNodes = skinned.GetJointNodes();
    const std::vector<glm::uvec4>& vertexJoints = skinned.GetCpuJoints();
    const std::vector<glm::vec4>& weights = skinned.GetCpuWeights();
    std::vector<glm::vec3> contact;
    for (size_t v = 0; v < posed.size(); ++v) {
        int best = 0;
        for (int k = 1; k < 4; ++k)
            if (weights[v][k] > weights[v][best]) best = k;
        const unsigned joint = vertexJoints[v][best];
        if (joint >= jointNodes.size()) continue;
        if (std::find(std::begin(contactNodes), std::end(contactNodes), jointNodes[joint]) != std::end(contactNodes))
            contact.push_back(posed[v]);
    }
    if (contact.empty()) return false;

    std::vector<float> heights;
    for (const glm::vec3& p : contact) heights.push_back(p.y);
    const size_t k = static_cast<size_t>(heights.size() * kSitBottomPercentile);
    std::nth_element(heights.begin(), heights.begin() + k, heights.end());
    sitBottomY = heights[k];

    // 아랫면 근처 띠의 가로·앞뒤 범위 가운데를 좌석 중심에 맞춘다
    const float band = sitBottomY + modelHeight * kSitContactBand;
    glm::vec2 lo(FLT_MAX), hi(-FLT_MAX);
    for (const glm::vec3& p : contact) {
        if (p.y > band) continue;
        lo = glm::min(lo, glm::vec2(p.x, p.z));
        hi = glm::max(hi, glm::vec2(p.x, p.z));
    }
    const glm::vec2 center = (lo + hi) * 0.5f;
    sitContact = glm::vec3(center.x - sitHips.x, 0.0f, center.y - sitHips.z);
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
        // 종아리가 통 옆면을 뚫지 않게 앉는 동안 무릎을 편다
        const glm::quat extend = glm::angleAxis(glm::radians(model.sitKneeAngle * sitBlend), glm::vec3(1.0f, 0.0f, 0.0f));
        for (int knee : model.kneeNodes)
            if (knee >= 0) animator.SetExtraRotation(knee, extend);
    } else {
        animator.ClearExtraRotations();
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
    // 높이: 엉덩이·허벅지 아랫면(SitIdle 기준)이 좌석 면에 오도록.
    // 가로·앞뒤: 좌석에 닿는 면의 가운데가 좌석 중심에 오도록. 지금 재생 중인 자세의 엉덩이를 따라간다
    // (StandToSit은 엉덩이가 뒤로 빠지며 끝나고 SitIdle은 아니어서, 고정값을 쓰면 클립이 바뀔 때 몸이 튄다)
    const float bottom = model.sitBottomY * model.scale;
    const glm::vec3 hipsNow =
        (model.hipsNode >= 0 ? animator.GetNodePosition(model.hipsNode) : model.sitHips) * model.scale;
    const glm::vec3 contact = hipsNow + model.sitContact * model.scale;
    const glm::vec3 local(-contact.x * sitBlend, (seatHeight + kSitClearance - bottom) * sitBlend,
                          (kSitForward - contact.z) * sitBlend);
    const glm::mat4 yaw = glm::rotate(glm::mat4(1.0f), glm::radians(transform.rotation.y), glm::vec3(0, 1, 0));
    return glm::vec3(yaw * glm::vec4(local, 0.0f));
}

void Customer::Render(ShaderManager& shaders) const {
    if (!visible || !model.skinned.IsLoaded()) return;
    const glm::mat4 world = glm::translate(glm::mat4(1.0f), GetSitOffset()) * transform.GetMatrix() *
                            glm::scale(glm::mat4(1.0f), glm::vec3(model.scale));
    model.skinned.Draw(shaders, world, animator.GetBones());
}
