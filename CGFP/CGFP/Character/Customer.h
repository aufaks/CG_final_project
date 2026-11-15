#pragma once
#include "Object/Object.h"

#include <vector>

class Seat;

// 입장 → 계산대 → 주문 → 자리 이동 → 대기(말풍선) → 식사 → 퇴장
enum class CustomerState {
    Entering,       // 입구에서 계산대로 이동
    Ordering,       // 계산대에서 주문 중
    GoingToSeat,    // 예약한 자리로 이동
    WaitingForFood, // 자리에서 음식 대기(말풍선)
    Eating,         // 식사
    Leaving,        // 출구로 이동
    Gone,           // 퇴장 완료, CustomerManager가 지운다
};

// 손님 NPC. 자기 몸 이동(경유점 따라가기)만 하고,
// 상태 전환·주문 생성·서빙 판정은 CustomerManager/OrderManager가 한다.
class Customer : public Object {
public:
    explicit Customer(int id);

    void Update(float deltaTime) override;

    void SetPath(std::vector<glm::vec3> waypoints); // y는 무시하고 xz 평면으로 이동
    bool HasArrived() const { return pathIndex >= path.size(); }

    CustomerState GetState() const { return state; }
    void SetState(CustomerState next);
    float GetStateTime() const { return stateTime; } // 현재 상태에 머문 시간(초)

    int GetId() const { return id; }
    Seat* GetSeat() const { return seat; }
    void SetSeat(Seat* value) { seat = value; }
    int GetOrderId() const { return orderId; }
    void SetOrderId(int value) { orderId = value; }

    void OnServed(); // 맞는 음식을 받음 → 식사

    float moveSpeed = 1.5f;

private:
    int id;
    CustomerState state = CustomerState::Entering;
    float stateTime = 0.0f;
    std::vector<glm::vec3> path;
    size_t pathIndex = 0;
    Seat* seat = nullptr;
    int orderId = -1;
};
