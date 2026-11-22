#include "Order/CustomerManager.h"

#include "Character/Squidward.h"
#include "Order/OrderManager.h"

#include <algorithm>
#include <cmath>
#include <iostream>

namespace {
// 손님 물고기 모델들. 비율 값은 OBJ를 키 대비로 재서 정했다 (다리·팔 높이, 몸통 폭).
// Fish_G·Fish_H는 T포즈 손 바깥에 세로 판 조각이 붙어 있어서 dropOutsideX로 잘라 낸다.
// Morty는 정면이 -Z라서 읽을 때 돌린다.
std::vector<CustomerModelDesc> MakeModelDescs() {
    std::vector<CustomerModelDesc> descs;

    CustomerModelDesc fishG;
    fishG.objPath = "obj/imported/sbfbbr-fish-g/Fish_G.obj";
    fishG.texturePath = "obj/imported/sbfbbr-fish-g/T_Fish_G_D.png";
    fishG.height = 1.45f;
    fishG.hipRatio = 0.22f;
    fishG.kneeRatio = 0.11f;
    fishG.armLowRatio = 0.445f;
    fishG.armHighRatio = 0.53f;
    fishG.shoulderX = 0.115f;
    fishG.dropOutsideX = 0.31f;
    descs.push_back(fishG);

    CustomerModelDesc fishH;
    fishH.objPath = "obj/imported/sbfbbr-fish-h/Fish_H.obj";
    fishH.texturePath = "obj/imported/sbfbbr-fish-h/T_Fish_H_D.png";
    fishH.height = 1.15f;
    fishH.hipRatio = 0.21f;
    fishH.kneeRatio = 0.08f;
    fishH.armLowRatio = 0.53f;
    fishH.armHighRatio = 0.65f;
    fishH.shoulderX = 0.14f;
    fishH.dropOutsideX = 0.38f;
    descs.push_back(fishH);

    CustomerModelDesc fred;
    fred.objPath = "obj/viewer_models/Fred.obj";
    fred.texturePath = "obj/viewer_models/fish.png";
    fred.height = 1.3f;
    fred.hipRatio = 0.21f;
    fred.kneeRatio = 0.10f;
    fred.armLowRatio = 0.63f;
    fred.armHighRatio = 0.71f;
    fred.shoulderX = 0.11f;
    descs.push_back(fred);

    CustomerModelDesc morty;
    morty.objPath = "obj/viewer_models/Morty.obj";
    morty.texturePath = "obj/viewer_models/Meshesmorty1_diff.png";
    morty.height = 1.0f;
    morty.facesNegativeZ = true;
    morty.hipRatio = 0.18f;
    morty.kneeRatio = 0.08f;
    morty.armLowRatio = 0.50f;
    morty.armHighRatio = 0.58f;
    morty.shoulderX = 0.18f;
    descs.push_back(morty);

    return descs;
}

float YawOf(const glm::vec3& direction) {
    return glm::degrees(std::atan2(direction.x, direction.z));
}
} // namespace

CustomerManager::CustomerManager() = default;
CustomerManager::~CustomerManager() = default;

bool CustomerManager::LoadModels() {
    models.clear();
    for (const CustomerModelDesc& desc : MakeModelDescs()) {
        auto model = std::make_unique<CustomerModel>();
        if (model->Load(desc)) {
            models.push_back(std::move(model));
        } else {
            std::cerr << "[CustomerManager] 손님 모델을 불러오지 못했습니다: " << desc.objPath << "\n";
        }
    }
    return !models.empty();
}

void CustomerManager::Setup(const CustomerLayout& value, std::vector<Seat> seatList, Squidward* squidwardPtr, OrderManager* orders) {
    layout = value;
    if (glm::length(layout.queueDirection) > 0.0f) layout.queueDirection = glm::normalize(layout.queueDirection);
    seats = std::move(seatList);
    squidward = squidwardPtr;
    orderManager = orders;
    customers.clear();
    queue.clear();
    spawnTimer = RandomRange(minSpawnSeconds, maxSpawnSeconds);
    ready = !models.empty() && orderManager != nullptr;
}

float CustomerManager::RandomRange(float minValue, float maxValue) {
    return std::uniform_real_distribution<float>(minValue, maxValue)(random);
}

void CustomerManager::Update(float deltaTime) {
    if (!ready) return;

    // 테이블의 팀 + 줄·주문 중인 팀이 maxTeams 미만일 때만 다음 손님 시계가 흐른다
    if (GetTeamCount() < maxTeams) {
        spawnTimer -= deltaTime;
        if (spawnTimer <= 0.0f) {
            Spawn();
            spawnTimer = RandomRange(minSpawnSeconds, maxSpawnSeconds);
        }
    }

    UpdateQueue();
    for (const auto& customer : customers) {
        if (customer->state == CustomerState::Ordering) UpdateOrdering(*customer);
        if (customer->state == CustomerState::ToSeat && !customer->IsMoving()) {
            const Seat& seat = seats[customer->seatIndex];
            customer->SitDown(seat.transform.position, seat.transform.rotation.y, seat.seatHeight);
            customer->state = CustomerState::Waiting;
            customer->stateTime = 0.0f;
        }
        customer->Update(deltaTime);
    }
}

void CustomerManager::Spawn() {
    const CustomerModel& model = *models[std::uniform_int_distribution<size_t>(0, models.size() - 1)(random)];
    auto customer = std::make_unique<Customer>(model);
    customer->transform.position = layout.entrance;
    customer->transform.rotation.y = YawOf(-layout.queueDirection);
    customer->FaceYaw(customer->transform.rotation.y);
    customer->state = CustomerState::Entering;
    queue.push_back(customer.get());
    customers.push_back(std::move(customer));
}

void CustomerManager::UpdateQueue() {
    // 맨 앞 손님의 앞면이 계산대 앞에 닿고, 뒷사람은 앞사람 두께 + 간격만큼 뒤에 선다
    const float faceCounter = YawOf(-layout.queueDirection);
    float distance = 0.0f;
    for (size_t i = 0; i < queue.size(); ++i) {
        Customer& customer = *queue[i];
        distance += (i == 0 ? 0.0f : queue[i - 1]->GetDepth() * 0.5f + layout.queueGap) + customer.GetDepth() * 0.5f;
        glm::vec3 slot = layout.counterFront + layout.queueDirection * distance;
        slot.y = layout.counterFront.y;

        if (glm::length(customer.GetDestination() - slot) > 0.01f) customer.MoveTo(slot);
        if (customer.IsMoving()) continue;

        customer.FaceYaw(faceCounter);
        if (customer.state == CustomerState::Entering) {
            customer.state = CustomerState::Queueing;
            customer.stateTime = 0.0f;
        }
        if (i == 0 && customer.state == CustomerState::Queueing) {
            customer.state = CustomerState::Ordering;
            customer.stateTime = 0.0f;
            customer.orderDelay = RandomRange(minOrderDelay, maxOrderDelay);
        }
    }
}

void CustomerManager::UpdateOrdering(Customer& customer) {
    if (!customer.order) {
        // 메뉴를 다 고르면 징징이에게 주문한다. 징징이가 한숨 쉬는 중이면 끝날 때까지 기다린다.
        if (customer.stateTime < customer.orderDelay) return;
        if (squidward) {
            if (!squidward->CanTakeOrder()) return;
            customer.order = orderManager->CreateOrder(&customer);
            squidward->TakeOrder(customer.order);
        } else { // 징징이 모델이 없으면 바로 주문 완료
            customer.order = orderManager->CreateOrder(&customer);
            customer.order->state = OrderState::Taken;
            orderManager->Submit(*customer.order);
        }
        return;
    }
    if (customer.order->state == OrderState::Taking) return;

    // 주문이 끝났다 → 빈 자리로 간다
    Seat* seat = PickEmptySeat();
    if (!seat) return; // 자리가 날 때까지 계산대 앞에서 기다린다
    seat->customer = &customer;
    customer.seatIndex = static_cast<int>(seat - seats.data());
    customer.order->seatNumber = seat->number;
    std::vector<glm::vec3> path = seat->approachPath;
    path.push_back(seat->transform.position);
    customer.SetPath(path);
    customer.state = CustomerState::ToSeat;
    customer.stateTime = 0.0f;
    queue.erase(std::remove(queue.begin(), queue.end(), &customer), queue.end());
}

Seat* CustomerManager::PickEmptySeat() {
    std::vector<Seat*> empty;
    for (Seat& seat : seats) {
        if (seat.IsEmpty()) empty.push_back(&seat);
    }
    if (empty.empty()) return nullptr;
    return empty[std::uniform_int_distribution<size_t>(0, empty.size() - 1)(random)];
}

void CustomerManager::Render(ShaderManager& shaders) const {
    for (const auto& customer : customers) customer->Render(shaders);
}
