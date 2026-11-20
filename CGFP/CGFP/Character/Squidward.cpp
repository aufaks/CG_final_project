#include "Character/Squidward.h"

#include "Render/ResourceManager.h"
#include "Render/ShaderManager.h"
#include "Render/Texture.h"

#include <cfloat>
#include <cmath>

namespace {
// 관절 위치 (키 대비 높이). OBJ는 T포즈: 팔(소매 포함)은 0.47~0.63 높이에서 좌우로 뻗어 있다.
constexpr float kLegTopRatio = 0.30f;  // 허리(셔츠 아랫단 근처)
constexpr float kNeckRatio = 0.615f;   // 목
constexpr float kArmBandLow = 0.47f;
constexpr float kArmBandHigh = 0.63f;
constexpr float kJointBlendRatio = 0.03f; // 허리·목에서 위아래 부위를 섞는 폭 (키 대비)

// 어깨: 몸통 옆면(중심에서 약 0.2). 소매는 팔과 같이 움직여야 해서 팔 쪽에 넣는다.
constexpr float kShoulderX = 0.20f;    // OBJ 좌표
constexpr float kShoulderBlend = 0.07f; // 어깨 안팎으로 몸통↔팔을 섞는 폭

constexpr float kArmDropAngle = 68.0f; // T포즈 팔을 내린 기본 자세 (90이면 완전히 수직)

// Idle: 심드렁하게 조금씩 흔들린다
constexpr float kIdleBreathSpeed = 1.6f;
constexpr float kIdleBreathHeight = 0.006f; // 키 대비
constexpr float kIdleHeadSwayAngle = 2.5f;
constexpr float kIdleHeadSwaySpeed = 0.7f;

// Sigh (sighSeconds 동안): 숨 들이쉬기 → 잠깐 멈춤 → 한숨 쉬며 축 처짐 → 원래대로
constexpr float kInhaleLift = 0.018f;    // 키 대비. 어깨·머리가 올라간다
constexpr float kInhaleHeadBack = -8.0f; // 고개를 살짝 뒤로
constexpr float kInhaleShrug = -10.0f;   // 어깨가 올라간다 (팔이 덜 내려감)
constexpr float kSighSlump = 7.0f;       // 상체가 앞으로 숙여진다
constexpr float kSighDrop = 0.025f;      // 키 대비. 어깨가 내려앉는다
constexpr float kSighHeadDroop = 20.0f;  // 고개를 푹 숙인다
constexpr float kSighHeadTilt = 6.0f;
constexpr float kSighArmHang = 8.0f;     // 팔이 몸 쪽으로 더 처진다


float Smooth(float edge0, float edge1, float x) {
    const float t = glm::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

glm::mat4 RotateAround(const glm::vec3& pivot, float degrees, const glm::vec3& axis) {
    return glm::translate(glm::mat4(1.0f), pivot) * glm::rotate(glm::mat4(1.0f), glm::radians(degrees), axis) *
           glm::translate(glm::mat4(1.0f), -pivot);
}
} // namespace

Squidward::Squidward() = default;
Squidward::~Squidward() = default;

bool Squidward::LoadModel(const std::string& objPath, const std::string& texturePath, float height) {
    std::vector<uint32_t> indices;
    if (!Mesh::ReadObj(objPath, restVertices, indices)) return false;
    texture = ResourceManager::Get().GetTexture(texturePath);
    color = glm::vec3(1.0f);

    glm::vec3 boundsMin(FLT_MAX), boundsMax(-FLT_MAX);
    for (const MeshVertex& v : restVertices) {
        boundsMin = glm::min(boundsMin, v.position);
        boundsMax = glm::max(boundsMax, v.position);
    }
    modelHeight = boundsMax.y - boundsMin.y;
    const float scale = modelHeight > 0.0f ? height / modelHeight : 1.0f;
    const glm::vec3 pivot((boundsMin.x + boundsMax.x) * 0.5f, boundsMin.y, (boundsMin.z + boundsMax.z) * 0.5f);
    modelOffset = glm::scale(glm::mat4(1.0f), glm::vec3(scale)) * glm::translate(glm::mat4(1.0f), -pivot);

    // 관절 위치
    const float h = modelHeight;
    const float legTop = boundsMin.y + h * kLegTopRatio;
    const float neckY = boundsMin.y + h * kNeckRatio;
    const float armLow = boundsMin.y + h * kArmBandLow;
    const float armHigh = boundsMin.y + h * kArmBandHigh;
    const float blend = h * kJointBlendRatio;
    const float cx = pivot.x;

    // 어깨 높이·앞뒤 = 팔 띠 안 바깥쪽 정점들의 중심
    glm::vec3 armSum(0.0f), torsoSum(0.0f);
    int armCount = 0, torsoCount = 0;
    for (const MeshVertex& v : restVertices) {
        const glm::vec3& p = v.position;
        if (p.y >= armLow && p.y <= armHigh && std::abs(p.x - cx) > kShoulderX + kShoulderBlend) { armSum += p; ++armCount; }
        if (p.y >= legTop && p.y <= armLow) { torsoSum += p; ++torsoCount; }
    }
    const float armY = armCount ? armSum.y / armCount : (armLow + armHigh) * 0.5f;
    const float armZ = armCount ? armSum.z / armCount : pivot.z;
    const float torsoZ = torsoCount ? torsoSum.z / torsoCount : pivot.z;
    shoulders[0] = glm::vec3(cx - kShoulderX, armY, armZ);
    shoulders[1] = glm::vec3(cx + kShoulderX, armY, armZ);
    waist = glm::vec3(cx, legTop, torsoZ);
    neck = glm::vec3(cx, neckY, torsoZ);

    // 정점 가중치. 같은 위치의 정점은 같은 가중치를 받으므로 삼각형 경계에서 메시가 벌어지지 않는다.
    weights.assign(restVertices.size(), Weights{});
    for (size_t i = 0; i < restVertices.size(); ++i) {
        const glm::vec3& p = restVertices[i].position;
        const float side = std::abs(p.x - cx);
        const float inBand = Smooth(armLow - blend, armLow + blend, p.y) * (1.0f - Smooth(armHigh - blend, armHigh + blend, p.y));
        const float arm = inBand * Smooth(kShoulderX - kShoulderBlend, kShoulderX + kShoulderBlend, side);
        const float head = Smooth(neckY - blend, neckY + blend, p.y);
        const float upper = Smooth(legTop - blend, legTop + blend, p.y);

        Weights& w = weights[i];
        w.w[p.x < cx ? RightArm : LeftArm] = arm;
        const float rest = 1.0f - arm;
        w.w[Head] = rest * head;
        w.w[Torso] = rest * (1.0f - head) * upper;
        w.w[Legs] = rest * (1.0f - head) * (1.0f - upper);
    }

    posed = restVertices;
    skinnedMesh = std::make_unique<Mesh>();
    skinnedMesh->Upload(restVertices, indices);
    mesh = skinnedMesh.get();

    EnterState(State::Idle);
    Skin();
    return true;
}

void Squidward::Place(const glm::vec3& feetPosition, float yawDegrees) {
    transform.position = feetPosition;
    transform.rotation.y = yawDegrees;
}

void Squidward::EnterState(State next) {
    state = next;
    stateTime = 0.0f;
    if (state == State::Idle) {
        std::uniform_real_distribution<float> wait(minIdleSeconds, maxIdleSeconds);
        idleDuration = wait(random);
    }
}

void Squidward::Update(float deltaTime) {
    stateTime += deltaTime;
    idleTime += deltaTime;
    switch (state) {
    case State::Idle:
        if (stateTime >= idleDuration) EnterState(State::Sigh);
        break;
    case State::Sigh:
        if (stateTime >= sighSeconds) EnterState(State::Idle);
        break;
    }
    Skin();
}

void Squidward::ComputeBoneMatrices(glm::mat4 (&out)[BoneCount]) const {
    const glm::vec3 X(1, 0, 0), Z(0, 0, 1);

    // 한숨 곡선 (0~1). 시간 구간은 sighSeconds(기본 4.8초)에 비례한다.
    float inhale = 0.0f, sigh = 0.0f;
    if (state == State::Sigh) {
        const float t = stateTime / sighSeconds;
        inhale = Smooth(0.0f, 0.29f, t) * (1.0f - Smooth(0.33f, 0.46f, t));
        sigh = Smooth(0.38f, 0.58f, t) * (1.0f - Smooth(0.71f, 1.0f, t));
    }

    const float breath = kIdleBreathHeight * modelHeight * (0.5f + 0.5f * std::sin(idleTime * kIdleBreathSpeed));
    const float lift = breath + (kInhaleLift * inhale - kSighDrop * sigh) * modelHeight;
    const glm::mat4 torso = RotateAround(waist, kSighSlump * sigh, X) * glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, lift, 0.0f));
    const float sway = kIdleHeadSwayAngle * std::sin(idleTime * kIdleHeadSwaySpeed);
    const float armAngle = kArmDropAngle + kInhaleShrug * inhale + kSighArmHang * sigh;

    out[Legs] = glm::mat4(1.0f);
    out[Torso] = torso;
    out[Head] = torso * RotateAround(neck, kInhaleHeadBack * inhale + kSighHeadDroop * sigh, X) *
                RotateAround(neck, sway + kSighHeadTilt * sigh, Z);
    out[RightArm] = torso * RotateAround(shoulders[0], armAngle, Z); // -X 팔: Z축 +회전이 아래로
    out[LeftArm] = torso * RotateAround(shoulders[1], -armAngle, Z);
}

void Squidward::Skin() {
    if (!skinnedMesh) return;
    glm::mat4 bones[BoneCount];
    ComputeBoneMatrices(bones);

    for (size_t i = 0; i < restVertices.size(); ++i) {
        const glm::vec4 p(restVertices[i].position, 1.0f);
        const glm::vec4 n(restVertices[i].normal, 0.0f);
        glm::vec4 position(0.0f), normal(0.0f);
        for (int b = 0; b < BoneCount; ++b) {
            const float w = weights[i].w[b];
            if (w <= 0.0f) continue;
            position += w * (bones[b] * p);
            normal += w * (bones[b] * n); // 회전·이동만 있으므로 노멀도 같은 행렬로 돌린다
        }
        posed[i].position = glm::vec3(position);
        posed[i].normal = glm::vec3(normal); // 셰이더에서 normalize
    }
    skinnedMesh->UpdateVertices(posed);
}

void Squidward::Render(ShaderManager& shaders) const {
    if (!visible || !mesh) return;
    shaders.SetMat4("uModel", transform.GetMatrix() * modelOffset);
    shaders.SetVec3("uColor", color);
    shaders.SetFloat("uOpacity", 1.0f);
    const bool useTexture = texture && texture->IsLoaded();
    shaders.SetBool("uUseTexture", useTexture);
    if (useTexture) texture->Bind(0);
    mesh->Draw();
}
