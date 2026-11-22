#pragma once
#include "Object/InteractableObject.h"

#include <vector>

class Customer;

// 좌석 = 통 의자 하나. 테이블마다 통 의자 2개가 있고 테이블 하나에 한 팀이 앉는다.
// transform.position은 앉는 자리 바닥 위치, rotation.y는 테이블을 보는 방향.
class Seat : public InteractableObject {
public:
    bool IsEmpty() const { return customer == nullptr; }

    int number = -1;                    // 테이블 번호 (같은 테이블의 의자는 번호가 같다). 주문표에 쓴다
    float seatHeight = 0.5f;            // 바닥에서 앉는 면까지 높이(월드)
    float seatRadius = 0.0f;            // 앉는 면(통 윗면) 반지름(월드). 종아리가 통을 뚫지 않게 무릎 펴는 각도를 정할 때 쓴다
    std::vector<glm::vec3> approachPath; // 계산대 앞에서 이 자리까지 가는 경유점 (자리 자체는 빼고)
    Customer* customer = nullptr;        // 앉은(또는 가는 중인) 손님
};
