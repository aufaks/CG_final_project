#include "Character/Customer.h"

#include "Render/ResourceManager.h"
#include "Render/ShaderManager.h"
#include "Render/Texture.h"

#include <cfloat>
#include <cmath>

namespace {
// 관절 주변에서 위아래 부위를 섞는 폭 (키 대비)
constexpr float kArmBandBlend = 0.02f;
constexpr float kShoulderBlend = 0.02f;
constexpr float kHipBlend = 0.025f;
constexpr float kKneeBlend = 0.03f;
constexpr float kLegSideBlend = 0.01f; // 가랑이 가운데에서 좌우 다리를 섞는 폭

constexpr float kArmDropAngle = 72.0f; // T포즈 팔을 내린 기본 자세

// 걷기 (Player와 비슷하게, 손님은 조금 느긋하게)
constexpr float kWalkCycleSpeed = 8.0f;
constexpr float kWalkBlendSpeed = 8.0f;
constexpr float kArmSwingAngle = 30.0f;
constexpr float kLegSwingAngle = 30.0f;
constexpr float kBodyBob = 0.02f; // 키 대비
constexpr float kBodySwayAngle = 3.0f;

// 서 있기
constexpr float kIdleBreathSpeed = 2.2f;
constexpr float kIdleBreath = 0.008f; // 키 대비
constexpr float kIdleArmSway = 5.0f;
constexpr float kIdleBodySway = 1.5f;

// 앉기: 허벅지를 앞으로 들고 정강이를 아래로 내린다. 앉은 동안 발을 번갈아 달랑거린다.
constexpr float kSitBlendSpeed = 3.0f;
constexpr float kSitThighAngle = -85.0f; // X축 -회전 = 다리를 앞으로
constexpr float kSitShinAngle = 80.0f;
constexpr float kSitArmDrop = 78.0f;
constexpr float kSitArmForward = 28.0f; // 손을 무릎 쪽으로
constexpr float kSitKickAngle = 12.0f;
constexpr float kSitKickSpeed = 2.6f;
constexpr float kSitSwayAngle = 2.5f;
constexpr float kSitSwaySpeed = 0.8f;

constexpr float kArriveDistance = 0.03f;

float Smooth(float edge0, float edge1, float x) {
    const float t = glm::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

glm::mat4 RotateAround(const glm::vec3& pivot, float degrees, const glm::vec3& axis) {
    return glm::translate(glm::mat4(1.0f), pivot) * glm::rotate(glm::mat4(1.0f), glm::radians(degrees), axis) *
           glm::translate(glm::mat4(1.0f), -pivot);
}

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
    std::vector<MeshVertex> vertices;
    std::vector<uint32_t> rawIndices;
    if (!Mesh::ReadObj(desc.objPath, vertices, rawIndices)) return false;
    texture = ResourceManager::Get().GetTexture(desc.texturePath);

    if (desc.facesNegativeZ) { // Y축 180도: x, z 부호를 뒤집는다
        for (MeshVertex& v : vertices) {
            v.position.x = -v.position.x;
            v.position.z = -v.position.z;
            v.normal.x = -v.normal.x;
            v.normal.z = -v.normal.z;
        }
    }

    glm::vec3 boundsMin(FLT_MAX), boundsMax(-FLT_MAX);
    for (const MeshVertex& v : vertices) {
        boundsMin = glm::min(boundsMin, v.position);
        boundsMax = glm::max(boundsMax, v.position);
    }
    const float rawHeight = boundsMax.y - boundsMin.y;
    const float centerX = (boundsMin.x + boundsMax.x) * 0.5f;

    // 쓸 삼각형만 남긴다. ReadObj는 삼각형마다 정점 3개를 따로 만들므로 인덱스는 0,1,2,3...
    restVertices.clear();
    indices.clear();
    for (size_t i = 0; i + 2 < rawIndices.size(); i += 3) {
        bool keep = true;
        if (desc.dropOutsideX > 0.0f) {
            keep = false;
            for (int k = 0; k < 3; ++k) {
                if (std::abs(vertices[rawIndices[i + k]].position.x - centerX) <= desc.dropOutsideX * rawHeight) keep = true;
            }
        }
        if (!keep) continue;
        for (int k = 0; k < 3; ++k) {
            indices.push_back(static_cast<uint32_t>(restVertices.size()));
            restVertices.push_back(vertices[rawIndices[i + k]]);
        }
    }
    if (restVertices.empty()) return false;

    // 발 중심을 원점으로 옮긴다
    boundsMin = glm::vec3(FLT_MAX);
    boundsMax = glm::vec3(-FLT_MAX);
    for (const MeshVertex& v : restVertices) {
        boundsMin = glm::min(boundsMin, v.position);
        boundsMax = glm::max(boundsMax, v.position);
    }
    const glm::vec3 pivot((boundsMin.x + boundsMax.x) * 0.5f, boundsMin.y, (boundsMin.z + boundsMax.z) * 0.5f);
    for (MeshVertex& v : restVertices) v.position -= pivot;
    modelHeight = boundsMax.y - boundsMin.y;
    scale = modelHeight > 0.0f ? desc.height / modelHeight : 1.0f;

    const float h = modelHeight;
    const float hipY = desc.hipRatio * h;
    const float kneeY = desc.kneeRatio * h;
    const float armLow = desc.armLowRatio * h;
    const float armHigh = desc.armHighRatio * h;

    // 가중치
    weights.assign(restVertices.size(), Weights{});
    glm::vec3 armSum[2]{}, legSum[2]{}, kneeSum[2]{};
    int armCount[2]{}, legCount[2]{}, kneeCount[2]{};
    glm::vec2 bodyMin(FLT_MAX), bodyMax(-FLT_MAX); // 팔 뺀 몸의 x, z 범위
    for (size_t i = 0; i < restVertices.size(); ++i) {
        const glm::vec3& p = restVertices[i].position;
        const float y = p.y / h;
        const float side = std::abs(p.x) / h;
        const float inBand = Smooth(desc.armLowRatio - kArmBandBlend, desc.armLowRatio + kArmBandBlend, y) *
                             (1.0f - Smooth(desc.armHighRatio - kArmBandBlend, desc.armHighRatio + kArmBandBlend, y));
        const float arm = inBand * Smooth(desc.shoulderX - kShoulderBlend, desc.shoulderX + kShoulderBlend, side);
        const float leg = 1.0f - Smooth(desc.hipRatio - kHipBlend, desc.hipRatio + kHipBlend, y);
        const float shin = 1.0f - Smooth(desc.kneeRatio - kKneeBlend, desc.kneeRatio + kKneeBlend, y);
        const float right = Smooth(-kLegSideBlend, kLegSideBlend, -p.x / h); // -X = 오른쪽

        Weights& w = weights[i];
        w.w[p.x < 0.0f ? RightArm : LeftArm] = arm;
        const float rest = 1.0f - arm;
        w.w[Body] = rest * (1.0f - leg);
        w.w[RightThigh] = rest * leg * right * (1.0f - shin);
        w.w[RightShin] = rest * leg * right * shin;
        w.w[LeftThigh] = rest * leg * (1.0f - right) * (1.0f - shin);
        w.w[LeftShin] = rest * leg * (1.0f - right) * shin;

        const int s = p.x < 0.0f ? 0 : 1;
        if (arm > 0.5f) { armSum[s] += p; ++armCount[s]; }
        if (p.y < hipY && p.y > kneeY) { legSum[s] += p; ++legCount[s]; }
        if (std::abs(p.y - kneeY) < 0.03f * h) { kneeSum[s] += p; ++kneeCount[s]; }
        if (arm < 0.5f) {
            bodyMin = glm::min(bodyMin, glm::vec2(p.x, p.z));
            bodyMax = glm::max(bodyMax, glm::vec2(p.x, p.z));
        }
    }

    // 관절 위치
    for (int s = 0; s < 2; ++s) {
        const float sign = s == 0 ? -1.0f : 1.0f;
        const float armZ = armCount[s] ? armSum[s].z / armCount[s] : 0.0f;
        shoulders[s] = glm::vec3(sign * desc.shoulderX * h, (armLow + armHigh) * 0.5f, armZ);
        const glm::vec3 legMid = legCount[s] ? legSum[s] / static_cast<float>(legCount[s]) : glm::vec3(sign * 0.05f * h, 0.0f, 0.0f);
        hips[s] = glm::vec3(legMid.x, hipY, legMid.z);
        const glm::vec3 kneeMid = kneeCount[s] ? kneeSum[s] / static_cast<float>(kneeCount[s]) : legMid;
        knees[s] = glm::vec3(kneeMid.x, kneeY, kneeMid.z);
    }
    pelvis = (hips[0] + hips[1]) * 0.5f;

    hipHeight = hipY * scale;
    width = (bodyMax.x - bodyMin.x) * scale;
    depth = (bodyMax.y - bodyMin.y) * scale;
    return true;
}

// ---------------------------------------------------------------- Customer

Customer::Customer(const CustomerModel& model) : model(model) {
    texture = model.texture;
    color = glm::vec3(1.0f);
    modelOffset = glm::scale(glm::mat4(1.0f), glm::vec3(model.scale));
    posed = model.restVertices;
    skinnedMesh = std::make_unique<Mesh>();
    skinnedMesh->Upload(model.restVertices, model.indices);
    mesh = skinnedMesh.get();

    static std::mt19937 random{ std::random_device{}() };
    idleSeed = std::uniform_real_distribution<float>(0.0f, 100.0f)(random);
    Skin();
}

Customer::~Customer() = default;

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
    seatHeight = height;
    anim = Anim::Sit;
}

void Customer::Update(float deltaTime) {
    stateTime += deltaTime;
    const glm::vec3 before = transform.position;
    UpdateMovement(deltaTime);
    UpdateAnimation(deltaTime, transform.position != before);
    Skin();
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
    animTime += deltaTime;
    if (anim != Anim::Sit) anim = moving ? Anim::Walk : Anim::Idle;

    const float walkTarget = anim == Anim::Walk ? 1.0f : 0.0f;
    walkBlend += (walkTarget - walkBlend) * glm::min(1.0f, deltaTime * kWalkBlendSpeed);
    if (anim == Anim::Walk) walkPhase = std::fmod(walkPhase + kWalkCycleSpeed * deltaTime, 6.28318531f);
    const float sitTarget = anim == Anim::Sit ? 1.0f : 0.0f;
    sitBlend += glm::clamp(sitTarget - sitBlend, -deltaTime * kSitBlendSpeed, deltaTime * kSitBlendSpeed);
}

void Customer::ComputeBoneMatrices(glm::mat4 (&out)[CustomerModel::BoneCount]) const {
    using B = CustomerModel::Bone;
    const glm::vec3 X(1, 0, 0), Y(0, 1, 0), Z(0, 0, 1);
    const float h = model.modelHeight;
    const float t = animTime + idleSeed;
    const float sit = Smooth(0.0f, 1.0f, sitBlend);
    const float stand = 1.0f - sit;
    const float idle = stand * (1.0f - walkBlend);
    const float s = std::sin(walkPhase) * walkBlend * stand;

    // 몸통: 걸을 때 통통, 서 있을 때 숨쉬기, 앉으면 좌석 높이로 올라간다
    const float bob = kBodyBob * h * walkBlend * stand * (0.5f + 0.5f * std::cos(2.0f * walkPhase));
    const float breath = kIdleBreath * h * (idle + sit) * (0.5f + 0.5f * std::sin(t * kIdleBreathSpeed));
    const float seatLift = sit * (seatHeight - model.hipHeight) / model.scale;
    const glm::mat4 root = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, bob + seatLift, 0.0f));
    const float sway = kBodySwayAngle * s + kIdleBodySway * idle * std::sin(t * 0.9f) +
                       kSitSwayAngle * sit * std::sin(t * kSitSwaySpeed);
    const glm::mat4 body = root * glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, breath, 0.0f)) *
                           RotateAround(model.pelvis, sway, Z);
    out[B::Body] = body;

    // 팔: 기본은 내린 자세. 걸으면 다리와 반대로 흔들고, 앉으면 무릎 쪽으로 모은다.
    const float idleArm = kIdleArmSway * idle * std::sin(t * 1.3f);
    const float drop = kArmDropAngle * (1.0f - sit) + kSitArmDrop * sit;
    const float forward = kSitArmForward * sit;
    out[B::RightArm] = body * RotateAround(model.shoulders[0], kArmSwingAngle * s + idleArm, X) *
                       RotateAround(model.shoulders[0], forward, Y) * RotateAround(model.shoulders[0], drop, Z);
    out[B::LeftArm] = body * RotateAround(model.shoulders[1], -kArmSwingAngle * s + idleArm, X) *
                      RotateAround(model.shoulders[1], -forward, Y) * RotateAround(model.shoulders[1], -drop, Z);

    // 다리: 걷기 흔들기 + 앉기(허벅지 앞으로, 정강이 아래로). 앉아 있으면 발을 번갈아 달랑거린다.
    const float kick = kSitKickAngle * sit * std::sin(t * kSitKickSpeed);
    for (int side = 0; side < 2; ++side) {
        const float sign = side == 0 ? -1.0f : 1.0f; // 오른다리가 앞으로 갈 때 왼다리는 뒤로
        const glm::mat4 thigh = root * RotateAround(model.hips[side], sign * kLegSwingAngle * s + kSitThighAngle * sit, X);
        const glm::mat4 shin = thigh * RotateAround(model.knees[side], kSitShinAngle * sit + (side == 0 ? kick : -kick), X);
        out[side == 0 ? B::RightThigh : B::LeftThigh] = thigh;
        out[side == 0 ? B::RightShin : B::LeftShin] = shin;
    }
}

void Customer::Skin() {
    if (!skinnedMesh) return;
    glm::mat4 bones[CustomerModel::BoneCount];
    ComputeBoneMatrices(bones);

    const size_t count = model.restVertices.size();
    for (size_t i = 0; i < count; ++i) {
        const glm::vec4 p(model.restVertices[i].position, 1.0f);
        const glm::vec4 n(model.restVertices[i].normal, 0.0f);
        glm::vec4 position(0.0f), normal(0.0f);
        const CustomerModel::Weights& w = model.weights[i];
        for (int b = 0; b < CustomerModel::BoneCount; ++b) {
            if (w.w[b] <= 0.0f) continue;
            position += w.w[b] * (bones[b] * p);
            normal += w.w[b] * (bones[b] * n);
        }
        posed[i].position = glm::vec3(position);
        posed[i].normal = glm::vec3(normal);
    }
    skinnedMesh->UpdateVertices(posed);
}

void Customer::Render(ShaderManager& shaders) const {
    if (!visible || !mesh) return;
    shaders.SetMat4("uModel", transform.GetMatrix() * modelOffset);
    shaders.SetVec3("uColor", color);
    shaders.SetFloat("uOpacity", 1.0f);
    const bool useTexture = texture && texture->IsLoaded();
    shaders.SetBool("uUseTexture", useTexture);
    if (useTexture) texture->Bind(0);
    mesh->Draw();
}
