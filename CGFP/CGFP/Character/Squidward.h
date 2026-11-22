#pragma once
#include "Character/Animator.h"
#include "Object/Object.h"
#include "Render/SkinnedModel.h"

#include <functional>
#include <memory>
#include <random>
#include <string>

struct Order;

// 계산대의 징징이. 기본은 Idle, 5~10초마다 한숨(Sigh) 클립을 한 번 재생하고 Idle로 돌아간다.
// 손님이 주문하면 TakeOrder(주문 받아 적기 → 영수증 뜯기) → SubmitOrder(뒤돌아 주방 창구에 영수증 넣기 → 제자리) 순서로 재생한다.
// 동작은 전부 glTF 파일(obj/mixamo/anim/Squidward.gltf, tools/squidward_to_gltf.py가 만든다)의 클립이고,
// 여기서는 상태에 맞는 클립만 고르고 영수증 타이밍을 맞춘다. 뒤돌아 창구로 가는 움직임도 클립(Root 뼈)에 들어 있다.
class Squidward : public Object {
public:
    enum class State { Idle, Sigh, TakeOrder, SubmitOrder };

    Squidward();
    ~Squidward() override;

    bool LoadModel(const std::string& gltfPath, float height);
    void Unload(); // GL 버퍼를 지운다 (컨텍스트가 살아 있을 때)
    void Place(const glm::vec3& feetPosition, float yawDegrees);

    void Update(float deltaTime) override;
    void Render(ShaderManager& shaders) const override; // "skinned" 셰이더가 Use된 상태에서 부른다
    void RenderProps(ShaderManager& shaders) const;     // 손에 든 영수증. "map" 셰이더로 그린다

    // 주문 받기. Idle일 때만 받는다 (한숨 쉬는 중이면 다 쉬고 나서 받는다).
    bool CanTakeOrder() const { return state == State::Idle; }
    bool TakeOrder(Order* order);

    State GetState() const { return state; }

    // 영수증을 주방 창구에 넣은 순간 한 번 불린다 (주문표 UI를 띄운다)
    std::function<void(Order&)> onReceiptSubmitted;

    float minIdleSeconds = 5.0f;
    float maxIdleSeconds = 10.0f;
    float receiptAppear = 0.82f;  // TakeOrder 진행도, 이때부터 영수증이 손에 있다 (영수증을 뜯는 순간)
    float receiptRelease = 0.46f; // SubmitOrder 진행도, 이때 창구에 넣는다 (팔을 끝까지 뻗은 순간)

private:
    void EnterState(State next);
    glm::mat4 GetModelMatrix() const;

    std::unique_ptr<SkinnedModel> skinned;
    Animator animator;
    glm::mat4 modelOffset{ 1.0f }; // 모델 좌표 → 발 원점·키 height
    int rootNode = -1;
    int rightHandNode = -1;

    State state = State::Idle;
    float stateTime = 0.0f; // 현재 상태에 들어온 뒤 지난 시간
    float idleDuration = 0.0f;
    Order* handlingOrder = nullptr;
    bool receiptInHand = false;
    std::mt19937 random{ std::random_device{}() };
};
