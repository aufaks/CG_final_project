#include "Order/CustomerManager.h"

#include "Cooking/Seat.h"
#include "Order/OrderManager.h"

#include <algorithm>
#include <utility>

void CustomerManager::Init(std::vector<Seat*> seatList, const CustomerWaypoints& points, OrderManager* orders) {
    seats = std::move(seatList);
    waypoints = points;
    orderManager = orders;
    Clear();
}

void CustomerManager::Update(float deltaTime, bool spawning) {
    if (spawning) {
        spawnTimer -= deltaTime;
        if (spawnTimer <= 0.0f) {
            // 빈자리가 있을 때만 들어온다 → 동시 손님 수는 좌석 수 이하
            if (Seat* seat = FindEmptySeat()) Spawn(seat);
            spawnTimer = NextSpawnDelay();
        }
    }

    for (auto& customer : customers) {
        customer->Update(deltaTime);
        UpdateState(*customer);
    }

    customers.erase(std::remove_if(customers.begin(), customers.end(),
        [](const std::unique_ptr<Customer>& c) { return c->GetState() == CustomerState::Gone; }), customers.end());
}

void CustomerManager::UpdateState(Customer& customer) {
    switch (customer.GetState()) {
    case CustomerState::Entering:
        if (customer.HasArrived()) customer.SetState(CustomerState::Ordering);
        break;

    case CustomerState::Ordering:
        // TODO(3단계): 계산대 대기열. 지금은 여러 손님이 계산대 위치에 겹칠 수 있다
        if (customer.GetStateTime() >= orderTime) {
            Seat* seat = customer.GetSeat();
            customer.SetOrderId(orderManager ? orderManager->CreateOrder(&customer, seat->GetId()) : -1);
            const glm::vec3 seatPosition = seat->GetSitPosition();
            customer.SetPath({ glm::vec3(waypoints.aisle.x, 0.0f, waypoints.counter.z),
                               glm::vec3(waypoints.aisle.x, 0.0f, seatPosition.z),
                               seatPosition });
            customer.SetState(CustomerState::GoingToSeat);
        }
        break;

    case CustomerState::GoingToSeat:
        if (customer.HasArrived()) customer.SetState(CustomerState::WaitingForFood);
        break;

    case CustomerState::WaitingForFood:
        break; // Seat::OnInteract → OrderManager::TryServe → Customer::OnServed가 Eating으로 바꾼다

    case CustomerState::Eating:
        if (customer.GetStateTime() >= eatTime) StartLeaving(customer);
        break;

    case CustomerState::Leaving:
        if (customer.HasArrived()) customer.SetState(CustomerState::Gone);
        break;

    case CustomerState::Gone:
        break;
    }
}

void CustomerManager::StartLeaving(Customer& customer) {
    if (Seat* seat = customer.GetSeat()) {
        seat->Release();
        customer.SetSeat(nullptr);
    }
    const glm::vec3 from = customer.transform.position;
    customer.SetPath({ glm::vec3(waypoints.aisle.x, 0.0f, from.z), waypoints.exit });
    customer.SetState(CustomerState::Leaving);
}

void CustomerManager::SendEveryoneHome() {
    for (auto& customer : customers) {
        const CustomerState state = customer->GetState();
        if (state == CustomerState::Eating || state == CustomerState::Leaving || state == CustomerState::Gone) continue;
        if (orderManager && customer->GetOrderId() >= 0) orderManager->CancelOrder(customer->GetOrderId());
        StartLeaving(*customer);
    }
}

void CustomerManager::Render(ShaderManager& shaders) const {
    for (const auto& customer : customers) customer->Render(shaders);
}

void CustomerManager::Clear() {
    for (Seat* seat : seats) seat->Release();
    customers.clear();
    spawnTimer = 1.0f;
}

Seat* CustomerManager::FindEmptySeat() const {
    for (Seat* seat : seats) {
        if (seat->IsEmpty()) return seat;
    }
    return nullptr;
}

void CustomerManager::Spawn(Seat* seat) {
    auto customer = std::make_unique<Customer>(nextCustomerId++);
    customer->transform.position.x = waypoints.entrance.x;
    customer->transform.position.z = waypoints.entrance.z;
    seat->Reserve(customer.get()); // 움직이기 전에 자리부터 예약
    customer->SetSeat(seat);
    customer->SetPath({ glm::vec3(waypoints.aisle.x, 0.0f, waypoints.entrance.z),
                        glm::vec3(waypoints.aisle.x, 0.0f, waypoints.counter.z),
                        waypoints.counter });
    customer->SetState(CustomerState::Entering);
    customers.push_back(std::move(customer));
}

float CustomerManager::NextSpawnDelay() {
    randomState = randomState * 1664525u + 1013904223u;
    const float t = static_cast<float>(randomState >> 8) / static_cast<float>(1u << 24);
    return spawnIntervalMin + (spawnIntervalMax - spawnIntervalMin) * t;
}
