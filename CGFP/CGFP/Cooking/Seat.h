#pragma once
#include "Object/InteractableObject.h"

#include <vector>

class Customer;

// 좌석 (테이블 하나 = 한 팀). transform.position은 앉는 자리 바닥 위치, rotation.y는 테이블을 보는 방향.
class Seat : public InteractableObject {
public:
    bool IsEmpty() const { return customer == nullptr; }

    int number = -1;
    float seatHeight = 0.5f;            // 바닥에서 앉는 면까지 높이(월드)
    std::vector<glm::vec3> approachPath; // 계산대 앞에서 이 자리까지 가는 경유점 (자리 자체는 빼고)
    Customer* customer = nullptr;        // 앉은(또는 가는 중인) 손님
};
