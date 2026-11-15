#pragma once
#include "Object/Object.h"

// 손님 상태: 입장 → 계산대 → 주문 → 자리 이동 → 대기(말풍선) → 식사 → 퇴장
enum class CustomerState {
    Entering,
    ToCounter,
    Ordering,
    ToSeat,
    Waiting,
    Eating,
    Leaving,
};

// 손님 NPC
class Customer : public Object {
public:
    void Update(float deltaTime) override {}

    CustomerState state = CustomerState::Entering;
};
