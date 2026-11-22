#include "Character/Squidward.h"

#include "Order/Order.h"
#include "Render/ResourceManager.h"
#include "Render/ShaderManager.h"
#include "Render/Texture.h"

#include <algorithm>
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

// TakeOrder (takeOrderSeconds 동안): 손님 쪽으로 몸을 기울이고 → 고개 끄덕이며 듣고 → 왼손 메모지에 오른손으로 적고 → 영수증을 뜯는다
constexpr float kOrderLean = 6.0f;
constexpr float kOrderNod = 9.0f;
constexpr float kPadArmForward = 62.0f; // 왼팔: 메모지를 가슴 앞에 든다
constexpr float kPadArmDrop = 52.0f;
constexpr float kPenArmForward = 52.0f; // 오른팔: 펜
constexpr float kPenArmDrop = 58.0f;
constexpr float kScribbleAngle = 6.0f;
constexpr float kLookAtPad = 14.0f;
constexpr float kTearUp = -30.0f; // 영수증을 뜯으며 오른팔을 휙 든다
constexpr float kReceiptAppear = 0.82f; // takeOrderSeconds 대비, 이때부터 영수증이 손에 있다

// SubmitOrder (submitOrderSeconds 동안, 초 단위 구간은 기본 3.8초 기준 비율): 뒤돌기 → 창구 쪽으로 뒷걸음 →
// 팔을 뻗어 창구에 영수증을 꽂기 → 팔 내리기 → 제자리로 → 다시 손님 쪽으로 돌기
constexpr float kReachForward = 88.0f;
constexpr float kReachPitch = -38.0f; // 창구(키 0.55~1.1 높이)가 어깨보다 높아서 위로 뻗는다
constexpr float kReachLean = 5.0f;
constexpr float kReceiptRelease = 0.46f; // submitOrderSeconds 대비, 이때 창구에 넣는다

// 손에 든 영수증 크기 (월드)
const glm::vec3 kReceiptSize(0.08f, 0.11f, 0.004f);


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

    // 오른손 끝: 오른팔(-X) 정점 중 가장 바깥쪽 근처의 중심. 영수증을 여기에 붙인다.
    float tipX = FLT_MAX;
    for (size_t i = 0; i < restVertices.size(); ++i) {
        if (weights[i].w[RightArm] > 0.9f) tipX = std::min(tipX, restVertices[i].position.x);
    }
    glm::vec3 handSum(0.0f);
    int handCount = 0;
    for (size_t i = 0; i < restVertices.size(); ++i) {
        if (weights[i].w[RightArm] > 0.9f && restVertices[i].position.x < tipX + h * 0.03f) {
            handSum += restVertices[i].position;
            ++handCount;
        }
    }
    rightHand = handCount ? handSum / static_cast<float>(handCount) : shoulders[0];

    posed = restVertices;
    skinnedMesh = std::make_unique<Mesh>();
    skinnedMesh->Upload(restVertices, indices);
    mesh = skinnedMesh.get();

    EnterState(State::Idle);
    Skin();
    return true;
}

void Squidward::Place(const glm::vec3& feetPosition, float yawDegrees) {
    basePosition = feetPosition;
    baseYaw = yawDegrees;
    UpdateTransform();
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
    case State::TakeOrder:
        if (stateTime >= takeOrderSeconds * kReceiptAppear) receiptInHand = true;
        if (stateTime >= takeOrderSeconds) {
            if (handlingOrder) handlingOrder->state = OrderState::Taken; // 손님은 이제 자리로 간다
            EnterState(State::SubmitOrder);
        }
        break;
    case State::SubmitOrder:
        if (receiptInHand && stateTime >= submitOrderSeconds * kReceiptRelease) {
            receiptInHand = false;
            if (handlingOrder && onReceiptSubmitted) onReceiptSubmitted(*handlingOrder);
        }
        if (stateTime >= submitOrderSeconds) {
            handlingOrder = nullptr;
            EnterState(State::Idle);
        }
        break;
    }
    UpdateTransform();
    Skin();
}

void Squidward::UpdateTransform() {
    // SubmitOrder: 뒤로 돌아 창구 쪽(원래 정면의 반대)으로 몇 걸음 갔다가 돌아온다
    float turn = 0.0f, step = 0.0f;
    if (state == State::SubmitOrder) {
        const float t = stateTime / submitOrderSeconds;
        turn = Smooth(0.0f, 0.18f, t) - Smooth(0.82f, 1.0f, t);
        step = Smooth(0.16f, 0.29f, t) - Smooth(0.68f, 0.82f, t);
    }
    const float yawRad = glm::radians(baseYaw);
    const glm::vec3 back(-std::sin(yawRad), 0.0f, -std::cos(yawRad));
    transform.position = basePosition + back * (windowStep * step);
    transform.rotation.y = baseYaw + 180.0f * turn;
}

Squidward::Pose Squidward::ComputePose() const {
    Pose pose;
    pose.lift = kIdleBreathHeight * (0.5f + 0.5f * std::sin(idleTime * kIdleBreathSpeed));
    pose.headRoll = kIdleHeadSwayAngle * std::sin(idleTime * kIdleHeadSwaySpeed);
    pose.armDrop[0] = pose.armDrop[1] = kArmDropAngle;

    switch (state) {
    case State::Idle:
        break;
    case State::Sigh: {
        // 한숨 곡선 (0~1). 시간 구간은 sighSeconds(기본 4.8초)에 비례한다.
        const float t = stateTime / sighSeconds;
        const float inhale = Smooth(0.0f, 0.29f, t) * (1.0f - Smooth(0.33f, 0.46f, t));
        const float sigh = Smooth(0.38f, 0.58f, t) * (1.0f - Smooth(0.71f, 1.0f, t));
        pose.lift += kInhaleLift * inhale - kSighDrop * sigh;
        pose.lean = kSighSlump * sigh;
        pose.headPitch = kInhaleHeadBack * inhale + kSighHeadDroop * sigh;
        pose.headRoll += kSighHeadTilt * sigh;
        for (float& drop : pose.armDrop) drop += kInhaleShrug * inhale + kSighArmHang * sigh;
        break;
    }
    case State::TakeOrder: {
        const float t = stateTime / takeOrderSeconds;
        const float ready = Smooth(0.0f, 0.15f, t) * (1.0f - Smooth(0.9f, 1.0f, t));
        // 듣는 동안(0.1~0.3) 고개를 두 번 끄덕인다
        const float listen = (t > 0.1f && t < 0.3f) ? 0.5f - 0.5f * std::cos((t - 0.1f) / 0.2f * 12.5663706f) : 0.0f;
        const float write = Smooth(0.28f, 0.34f, t) * (1.0f - Smooth(0.72f, 0.78f, t));
        const float tear = Smooth(0.78f, 0.84f, t) * (1.0f - Smooth(0.86f, 0.97f, t));
        const float scribble = write * std::sin(stateTime * 22.0f);
        const float scribble2 = write * std::sin(stateTime * 15.0f + 1.0f);

        pose.lean = kOrderLean * ready;
        pose.headPitch = kOrderNod * listen + kLookAtPad * write;
        // 왼팔: 메모지를 가슴 앞에 든다
        pose.armDrop[1] = kArmDropAngle + (kPadArmDrop - kArmDropAngle) * ready;
        pose.armForward[1] = kPadArmForward * ready;
        // 오른팔: 펜으로 끄적이다가 영수증을 뜯어 든다
        pose.armDrop[0] = kArmDropAngle + (kPenArmDrop - kArmDropAngle) * ready;
        pose.armForward[0] = kPenArmForward * ready + kScribbleAngle * scribble;
        pose.armPitch[0] = kScribbleAngle * 0.6f * scribble2 + kTearUp * tear;
        break;
    }
    case State::SubmitOrder: {
        const float t = stateTime / submitOrderSeconds;
        const float reach = Smooth(0.29f, 0.42f, t) * (1.0f - Smooth(0.52f, 0.66f, t));
        // 영수증을 넣는 순간 손을 살짝 밀어 넣는다
        const float push = Smooth(0.40f, 0.46f, t) * (1.0f - Smooth(0.46f, 0.52f, t));
        // 걸어가는 동안 통통
        const float walking = Smooth(0.16f, 0.2f, t) * (1.0f - Smooth(0.25f, 0.29f, t)) +
                              Smooth(0.68f, 0.72f, t) * (1.0f - Smooth(0.78f, 0.82f, t));
        pose.lift += 0.008f * walking * std::abs(std::sin(stateTime * 14.0f));
        pose.lean = kReachLean * reach + 3.0f * push;
        pose.headPitch = -6.0f * reach; // 창구를 올려다본다
        pose.armDrop[0] = kArmDropAngle * (1.0f - reach);
        pose.armForward[0] = kReachForward * reach;
        pose.armPitch[0] = kReachPitch * reach - 8.0f * push;
        break;
    }
    }
    return pose;
}

void Squidward::ComputeBoneMatrices(const Pose& pose, glm::mat4 (&out)[BoneCount]) const {
    const glm::vec3 X(1, 0, 0), Y(0, 1, 0), Z(0, 0, 1);
    const glm::mat4 torso = RotateAround(waist, pose.lean, X) *
                            glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, pose.lift * modelHeight, 0.0f));

    out[Legs] = glm::mat4(1.0f);
    out[Torso] = torso;
    out[Head] = torso * RotateAround(neck, pose.headPitch, X) * RotateAround(neck, pose.headRoll, Z);
    // -X 팔: Z축 +회전이 아래로, Y축 +회전이 앞(+Z)으로, X축 -회전이 위로. +X 팔은 Y·Z 부호가 반대.
    out[RightArm] = torso * RotateAround(shoulders[0], pose.armPitch[0], X) *
                    RotateAround(shoulders[0], pose.armForward[0], Y) * RotateAround(shoulders[0], pose.armDrop[0], Z);
    out[LeftArm] = torso * RotateAround(shoulders[1], pose.armPitch[1], X) *
                   RotateAround(shoulders[1], -pose.armForward[1], Y) * RotateAround(shoulders[1], -pose.armDrop[1], Z);
}

void Squidward::Skin() {
    if (!skinnedMesh) return;
    glm::mat4 bones[BoneCount];
    ComputeBoneMatrices(ComputePose(), bones);
    rightArmBone = bones[RightArm];

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
    const glm::mat4 model = transform.GetMatrix() * modelOffset;
    shaders.SetMat4("uModel", model);
    shaders.SetVec3("uColor", color);
    shaders.SetFloat("uOpacity", 1.0f);
    const bool useTexture = texture && texture->IsLoaded();
    shaders.SetBool("uUseTexture", useTexture);
    if (useTexture) texture->Bind(0);
    mesh->Draw();

    // 손에 든 영수증: 오른손 끝에 매달린 하얀 종이
    if (!receiptInHand) return;
    Mesh* paper = ResourceManager::Get().GetCube();
    if (!paper) return;
    const glm::vec3 hand = glm::vec3(model * rightArmBone * glm::vec4(rightHand, 1.0f));
    const glm::mat4 receipt = glm::translate(glm::mat4(1.0f), hand - glm::vec3(0.0f, kReceiptSize.y * 0.45f, 0.0f)) *
                              glm::rotate(glm::mat4(1.0f), glm::radians(transform.rotation.y), glm::vec3(0, 1, 0)) *
                              glm::rotate(glm::mat4(1.0f), glm::radians(-12.0f), glm::vec3(1, 0, 0)) *
                              glm::scale(glm::mat4(1.0f), kReceiptSize);
    shaders.SetMat4("uModel", receipt);
    shaders.SetVec3("uColor", glm::vec3(1.0f));
    shaders.SetBool("uUseTexture", false);
    paper->Draw();
}
