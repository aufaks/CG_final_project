#pragma once
#include "Character/Customer.h"
#include "Cooking/Seat.h"

#include <memory>
#include <random>
#include <vector>

class OrderManager;
class ShaderManager;
class Squidward;

// 손님 동선 (월드 좌표). PlayScene이 맵 좌표에서 계산해 넘긴다.
struct CustomerLayout {
    glm::vec3 entrance{ 0.0f };      // 손님이 생기는 곳 (집게리아 입구 안쪽)
    glm::vec3 counterFront{ 0.0f };  // 계산대 앞. 맨 앞 손님의 앞면이 여기 닿는다
    glm::vec3 queueDirection{ 0.0f, 0.0f, -1.0f }; // 줄이 늘어나는 방향 (계산대 → 입구 쪽 단위 벡터)
    float queueGap = 0.15f;          // 앞사람과 뒷사람 사이 간격(월드)
};

// 손님 관리: 스폰, 줄서기, 주문, 빈자리 배정, 경유점 이동
// 임시 스폰 규칙: 테이블에 앉은 팀 + 줄 서거나 주문 중인 팀(일행이면 1팀)이 maxTeams 미만이면
// 10~20초마다 입구에서 한 팀(지금은 손님 한 명)을 만든다.
// TODO: 나중에는 밖에서 돌아다니다가 집게리아로 들어오는 연출로 바꾼다.
class CustomerManager {
public:
    CustomerManager();
    ~CustomerManager();

    bool LoadModels(); // 손님 모델들을 읽는다. 하나라도 읽으면 true
    void Setup(const CustomerLayout& layout, std::vector<Seat> seats, Squidward* squidward, OrderManager* orders);

    void Update(float deltaTime);
    void Render(ShaderManager& shaders) const;

    int GetTeamCount() const { return static_cast<int>(customers.size()); }
    const std::vector<Seat>& GetSeats() const { return seats; }

    int maxTeams = 6;
    float minSpawnSeconds = 10.0f;
    float maxSpawnSeconds = 20.0f;
    float minOrderDelay = 1.5f; // 계산대 맨 앞에 서서 메뉴 고민하는 시간
    float maxOrderDelay = 3.0f;

    std::vector<std::unique_ptr<Customer>> customers;

private:
    void Spawn();
    void UpdateQueue();
    void UpdateOrdering(Customer& customer);
    Seat* PickEmptySeat();
    float RandomRange(float minValue, float maxValue);

    std::vector<std::unique_ptr<CustomerModel>> models;
    std::vector<Seat> seats;
    std::vector<Customer*> queue; // 계산대 줄. [0]이 맨 앞(주문하는 손님)
    CustomerLayout layout;
    Squidward* squidward = nullptr;
    OrderManager* orderManager = nullptr;
    float spawnTimer = 0.0f;
    bool ready = false;
    std::mt19937 random{ std::random_device{}() };
};
