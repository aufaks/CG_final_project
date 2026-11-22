#pragma once
#include "Character/Animator.h"
#include "Object/Object.h"
#include "Render/SkinnedModel.h"

#include <deque>
#include <string>
#include <vector>

struct Order;

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

// 손님 모델 한 종류. Mixamo에서 리깅·애니메이션한 glTF (tools/mixamo_to_gltf.py로 만든다).
// 클립 이름: Idle(서 있기), Walk(제자리 걷기), StandToSit(앉기), SitIdle(앉아 있기), SitEat(앉아서 먹기)
struct CustomerModelDesc {
    std::string gltfPath;
    float height = 1.2f; // 월드 단위 키
};

// 손님 모델 데이터. 메시·뼈대·클립은 여러 손님이 공유하고, 자세는 손님마다 Animator가 갖는다.
class CustomerModel {
public:
    bool Load(const CustomerModelDesc& desc);

    SkinnedModel skinned;
    float scale = 1.0f;       // glTF → 월드
    float width = 0.0f;       // 월드 단위 가로 폭 (T포즈 바운딩 박스 기준이라 팔 포함)
    float depth = 0.0f;       // 월드 단위 앞뒤 두께. 줄 설 때 간격 계산에 쓴다
    float walkSpeed = 1.0f;   // Walk 클립을 1배속으로 재생할 때 발이 미끄러지지 않는 이동 속도(월드/초)
    glm::vec3 sitHips{ 0.0f }; // SitIdle 첫 프레임의 Hips 위치 (glTF 모델 공간). 좌석 중심에 맞춘다
    float sitBottomY = 0.0f;   // SitIdle 첫 프레임 메시의 엉덩이·허벅지 아랫면 높이. 좌석 면에 맞춘다
    glm::vec3 sitContact{ 0.0f }; // 좌석에 닿는 면의 가운데 - Hips (SitIdle 첫 프레임, y는 0). 좌석 중심에 맞춘다
    int hipsNode = -1;
    bool hasSitHips = false;
    int kneeNodes[2] = { -1, -1 };  // LeftLeg, RightLeg
    int thighNodes[2] = { -1, -1 }; // LeftUpLeg, RightUpLeg
    float sitKneeAngle = 0.0f;      // 앉아 있을 때 무릎을 더 펴는 각도(도). 종아리가 통 옆면을 뚫지 않게 FitSeat이 정한다
    // 앉아 있을 때 허벅지에 더하는 회전. SitIdle에서 허벅지가 아래로 기운 모델(FishH)은
    // 무릎 쪽이 좌석 면 기준이 되어 몸이 통 속으로 내려가므로 허벅지를 거의 수평으로 든다 (Load가 정한다)
    glm::quat sitThigh[2] = { glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f) };

    // 좌석 크기(월드)에 맞춰 sitKneeAngle을 정한다. 앉은 자세 메시를 무릎 각도별로 스키닝해 보고
    // 좌석 면보다 아래로 내려간 종아리·발이 좌석 원기둥 밖으로 나오는 가장 작은 각도를 고른다.
    void FitSeat(float seatHeight, float seatRadius);

private:
    bool MeasureSitContact(const std::vector<glm::mat4>& globals, float modelHeight);
    void LevelThighs(const SkinnedModel::Clip& sit);
    // SitIdle 첫 프레임 자세 + 허벅지 보정 + 무릎 펴기(도)
    void SampleSitPose(const SkinnedModel::Clip& sit, float kneeAngle, std::vector<NodePose>& pose) const;
};

// 손님 NPC. 애니메이션은 Animator가 클립을 재생하고, 여기서는 상태에 맞는 클립만 고른다.
// transform.position은 발 위치, 정면은 rotation.y 방향 (yaw 0 = +Z).
class Customer : public Object {
public:
    enum class Anim { Idle, Walk, Sit };

    explicit Customer(const CustomerModel& model);

    void Update(float deltaTime) override;
    void Render(ShaderManager& shaders) const override; // "skinned" 셰이더가 Use된 상태에서 부른다

    // 이동: 경유점을 차례로 걸어간다. MoveTo는 경로를 목표 한 점으로 바꾼다.
    void SetPath(const std::vector<glm::vec3>& points);
    void MoveTo(const glm::vec3& point);
    bool IsMoving() const { return !path.empty(); }
    const glm::vec3& GetDestination() const { return destination; }
    void FaceYaw(float yawDegrees) { targetYaw = yawDegrees; } // 멈춰 있을 때 이 방향으로 돈다

    // 좌석에 앉는다. seatHeight = 바닥에서 앉는 면까지 높이(월드)
    // 엉덩이·허벅지가 좌석 면 가운데에 닿도록 몸을 옮긴다
    void SitDown(const glm::vec3& seatPosition, float yawDegrees, float seatHeight);
    // 앉아 있는 손님이 밥을 먹기 시작한다 (Eating 상태, SitEat 클립 반복). 앉아 있지 않으면 false
    bool StartEating();

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
    glm::vec3 GetSitOffset() const; // 앉는 동안 엉덩이가 좌석 위에 오도록 몸을 옮기는 양(월드)

    const CustomerModel& model;
    Animator animator;

    std::deque<glm::vec3> path;
    glm::vec3 destination{ 0.0f };
    float targetYaw = 0.0f;

    Anim anim = Anim::Idle;
    float seatHeight = 0.0f;
    float sitBlend = 0.0f; // 0 = 선 자리, 1 = 좌석에 맞춘 자리
};
