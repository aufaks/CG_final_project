#pragma once
#include "Character/Customer.h"

#include <memory>
#include <vector>

class OrderManager;
class Seat;
class ShaderManager;

// 손님이 걷는 고정 경유점. 길찾기 대신 이 점들만 따라간다.
struct CustomerWaypoints {
    glm::vec3 entrance{ 0.0f }; // 생성 위치
    glm::vec3 aisle{ 0.0f };    // 입구~계산대~자리를 잇는 통로 위의 점(x만 사용)
    glm::vec3 counter{ 0.0f };  // 주문하는 위치
    glm::vec3 exit{ 0.0f };
};

// 손님 관리: 랜덤 스폰 타이머, 빈자리 배정, 웨이포인트 경로, 상태 전환.
class CustomerManager {
public:
    void Init(std::vector<Seat*> seats, const CustomerWaypoints& waypoints, OrderManager* orderManager);

    // spawning이 false면 새 손님을 만들지 않는다(영업 종료)
    void Update(float deltaTime, bool spawning);
    void Render(ShaderManager& shaders) const;

    void SendEveryoneHome(); // 영업 종료: 아직 음식을 못 받은 손님은 주문을 취소하고 퇴장
    void Clear();
    bool AllGone() const { return customers.empty(); }

    const std::vector<std::unique_ptr<Customer>>& GetCustomers() const { return customers; }

    float spawnIntervalMin = 6.0f;  // 다음 손님까지 최소 시간(초)
    float spawnIntervalMax = 12.0f; // TODO(4단계): 날짜가 지날수록 줄이기
    float orderTime = 2.0f;         // 계산대에서 주문하는 데 걸리는 시간
    float eatTime = 5.0f;           // 식사 시간

private:
    Seat* FindEmptySeat() const;
    void Spawn(Seat* seat);
    void UpdateState(Customer& customer);
    void StartLeaving(Customer& customer);
    float NextSpawnDelay();

    std::vector<std::unique_ptr<Customer>> customers;
    std::vector<Seat*> seats;
    CustomerWaypoints waypoints;
    OrderManager* orderManager = nullptr;
    float spawnTimer = 1.0f;
    int nextCustomerId = 1;
    unsigned randomState = 12345;
};
