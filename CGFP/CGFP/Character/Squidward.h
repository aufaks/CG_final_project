#pragma once
#include "Object/Object.h"
#include "Render/Mesh.h"

#include <functional>
#include <memory>
#include <random>
#include <string>
#include <vector>

struct Order;

// 계산대의 징징이. 기본은 Idle, 5~10초마다 한숨(Sigh) 애니메이션을 한 번 재생하고 Idle로 돌아간다.
// 손님이 주문하면 TakeOrder(주문 받아 적기 → 영수증 뜯기) → SubmitOrder(뒤돌아 주방 창구에 영수증 넣기 → 제자리) 순서로 재생한다.
// 메시를 조각내지 않고, 정점마다 다리·몸통·머리·팔 가중치를 줘서 관절 근처를 부드럽게 섞는다
// (linear blend skinning). 그래서 어깨·목·허리가 굽어도 메시가 갈라지지 않는다.
class Squidward : public Object {
public:
    enum class State { Idle, Sigh, TakeOrder, SubmitOrder };

    Squidward();
    ~Squidward() override;

    bool LoadModel(const std::string& objPath, const std::string& texturePath, float height);
    void Place(const glm::vec3& feetPosition, float yawDegrees);

    void Update(float deltaTime) override;
    void Render(ShaderManager& shaders) const override;

    // 주문 받기. Idle일 때만 받는다 (한숨 쉬는 중이면 다 쉬고 나서 받는다).
    bool CanTakeOrder() const { return state == State::Idle; }
    bool TakeOrder(Order* order);

    State GetState() const { return state; }

    // 영수증을 주방 창구에 넣은 순간 한 번 불린다 (주문표 UI를 띄운다)
    std::function<void(Order&)> onReceiptSubmitted;

    float minIdleSeconds = 5.0f;
    float maxIdleSeconds = 10.0f;
    float sighSeconds = 4.8f;
    float takeOrderSeconds = 3.2f;
    float submitOrderSeconds = 3.8f;
    float windowStep = 0.55f; // 창구 쪽으로 가는 거리(월드). 팔 길이(약 0.5)로 창구에 닿을 만큼

private:
    enum Bone { Legs, Torso, Head, RightArm, LeftArm, BoneCount };

    struct Weights {
        float w[BoneCount]{};
    };

    // 한 프레임 자세. 각도는 도, lift는 키 대비.
    struct Pose {
        float lift = 0.0f;
        float lean = 0.0f;      // 허리에서 앞으로 숙임
        float headPitch = 0.0f; // + = 고개 숙임
        float headRoll = 0.0f;
        float armDrop[2]{};     // Z축: T포즈 팔을 내림
        float armForward[2]{};  // Y축: 팔을 앞으로
        float armPitch[2]{};    // X축: - = 위로 듦
    };

    void EnterState(State next);
    Pose ComputePose() const;
    void ComputeBoneMatrices(const Pose& pose, glm::mat4 (&out)[BoneCount]) const;
    void Skin();
    void UpdateTransform();

    std::unique_ptr<Mesh> skinnedMesh;
    std::vector<MeshVertex> restVertices; // OBJ 원래 자세(T포즈)
    std::vector<MeshVertex> posed;        // 이번 프레임 자세
    std::vector<Weights> weights;

    glm::vec3 waist{ 0.0f };
    glm::vec3 neck{ 0.0f };
    glm::vec3 shoulders[2]{}; // [0] = 오른쪽(-X), [1] = 왼쪽(+X)
    glm::vec3 rightHand{ 0.0f }; // 오른손 끝 (OBJ 좌표). 영수증을 여기에 붙인다
    float modelHeight = 1.0f;

    glm::mat4 modelOffset{ 1.0f }; // OBJ 좌표 → 발 원점·키 height
    glm::mat4 rightArmBone{ 1.0f }; // 마지막 Skin의 오른팔 행렬
    glm::vec3 basePosition{ 0.0f }; // 계산대 제자리
    float baseYaw = 0.0f;

    State state = State::Idle;
    float stateTime = 0.0f; // 현재 상태에 들어온 뒤 지난 시간
    float idleTime = 0.0f;  // Idle 흔들림 시계 (상태와 상관없이 계속 흐른다)
    float idleDuration = 0.0f;
    Order* handlingOrder = nullptr;
    bool receiptInHand = false;
    std::mt19937 random{ std::random_device{}() };
};
