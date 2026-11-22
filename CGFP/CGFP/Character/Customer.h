#pragma once
#include "Object/Object.h"
#include "Render/Mesh.h"

#include <deque>
#include <memory>
#include <random>
#include <string>
#include <vector>

struct Order;
class Texture;

// 손님 상태: 입장(입구 → 줄 끝) → 줄서기 → 주문 → 자리 이동 → 대기(착석) → 식사 → 퇴장
enum class CustomerState {
    Entering,
    Queueing,
    Ordering,
    ToSeat,
    Waiting,
    Eating,
    Leaving,
};

// 손님 모델 한 종류의 설정. 비율은 모두 모델 키 대비 (x는 몸 중심에서의 거리).
struct CustomerModelDesc {
    std::string objPath;
    std::string texturePath;
    float height = 1.2f;      // 월드 단위 키
    bool facesNegativeZ = false; // 모델 정면이 -Z면 읽을 때 180도 돌려 +Z로 맞춘다
    float hipRatio = 0.22f;   // 다리 위끝(골반 관절)
    float kneeRatio = 0.10f;
    float armLowRatio = 0.45f; // T포즈 팔이 있는 높이 띠
    float armHighRatio = 0.55f;
    float shoulderX = 0.10f;  // 이보다 바깥이 팔
    float dropOutsideX = 0.0f; // 0보다 크면 이보다 바깥 삼각형은 버린다 (손 옆에 떠 있는 판 조각)
};

// 손님 모델 데이터. 여러 손님이 같은 데이터를 공유하고, 정점 변형(스키닝)만 손님마다 따로 한다.
// Squidward와 같은 방식: 정점마다 몸통·팔·허벅지·정강이 가중치를 줘서 관절 근처를 부드럽게 섞는다.
class CustomerModel {
public:
    enum Bone { Body, RightArm, LeftArm, RightThigh, LeftThigh, RightShin, LeftShin, BoneCount };
    struct Weights {
        float w[BoneCount]{};
    };

    bool Load(const CustomerModelDesc& desc);

    std::vector<MeshVertex> restVertices; // 발 중심이 원점, 정면 +Z인 T포즈 (OBJ 단위)
    std::vector<uint32_t> indices;
    std::vector<Weights> weights;
    Texture* texture = nullptr;

    glm::vec3 pelvis{ 0.0f };
    glm::vec3 shoulders[2]{}; // [0] = 오른쪽(-X), [1] = 왼쪽(+X)
    glm::vec3 hips[2]{};
    glm::vec3 knees[2]{};
    float modelHeight = 1.0f; // OBJ 단위
    float scale = 1.0f;       // OBJ → 월드
    float hipHeight = 0.0f;   // 월드 단위 골반 높이
    float width = 0.0f;       // 월드 단위 몸 가로 폭 (팔 제외)
    float depth = 0.0f;       // 월드 단위 앞뒤 두께. 줄 설 때 간격 계산에 쓴다
};

// 손님 NPC. 걷기(Walk)·서 있기(Idle)·앉아 있기(Sit) 애니메이션을 직접 계산한다.
// transform.position은 발 위치, 정면은 rotation.y 방향 (yaw 0 = +Z).
class Customer : public Object {
public:
    enum class Anim { Idle, Walk, Sit };

    explicit Customer(const CustomerModel& model);
    ~Customer() override;

    void Update(float deltaTime) override;
    void Render(ShaderManager& shaders) const override;

    // 이동: 경유점을 차례로 걸어간다. MoveTo는 경로를 목표 한 점으로 바꾼다.
    void SetPath(const std::vector<glm::vec3>& points);
    void MoveTo(const glm::vec3& point);
    bool IsMoving() const { return !path.empty(); }
    const glm::vec3& GetDestination() const { return destination; }
    void FaceYaw(float yawDegrees) { targetYaw = yawDegrees; } // 멈춰 있을 때 이 방향으로 돈다

    // 좌석에 앉는다. seatHeight = 바닥에서 앉는 면까지 높이(월드)
    void SitDown(const glm::vec3& seatPosition, float yawDegrees, float seatHeight);

    float GetDepth() const { return model.depth; }
    float GetWidth() const { return model.width; }
    Anim GetAnim() const { return anim; }

    CustomerState state = CustomerState::Entering;
    float stateTime = 0.0f;   // 현재 상태에 들어온 뒤 지난 시간
    float orderDelay = 0.0f;  // 계산대 맨 앞에서 주문하기까지 고민하는 시간
    Order* order = nullptr;   // 이 손님의 주문표 (OrderManager 소유)
    int seatIndex = -1;

    float moveSpeed = 1.1f;   // 월드 단위/초
    float turnSpeed = 360.0f; // 도/초

private:
    void UpdateMovement(float deltaTime);
    void UpdateAnimation(float deltaTime, bool moving);
    void ComputeBoneMatrices(glm::mat4 (&out)[CustomerModel::BoneCount]) const;
    void Skin();

    const CustomerModel& model;
    std::unique_ptr<Mesh> skinnedMesh;
    std::vector<MeshVertex> posed;
    glm::mat4 modelOffset{ 1.0f };

    std::deque<glm::vec3> path;
    glm::vec3 destination{ 0.0f };
    float targetYaw = 0.0f;

    Anim anim = Anim::Idle;
    float walkPhase = 0.0f;
    float walkBlend = 0.0f;
    float sitBlend = 0.0f;
    float seatHeight = 0.0f;
    float animTime = 0.0f;
    float idleSeed = 0.0f; // 손님마다 흔들림 박자를 다르게
};
