#pragma once
#include "Object/InteractableObject.h"

class Customer;
class OrderManager;

// 좌석(테이블 6개). 비었는지, 앉은 손님이 누구인지 가진다.
// 손님을 기다리게 한 자리에 접시를 들고 F → 서빙(OrderManager가 판정).
// 손님이 움직이기 전에 Reserve로 자리를 먼저 잡아서 두 손님이 같은 자리로 가지 않게 한다.
class Seat : public InteractableObject {
public:
    Seat(int id, OrderManager* orderManager);

    int GetId() const { return id; }
    bool IsEmpty() const { return customer == nullptr; }
    Customer* GetCustomer() const { return customer; }
    void Reserve(Customer* who) { customer = who; }
    void Release() { customer = nullptr; }

    // 손님이 앉을 위치(좌석 위)
    glm::vec3 GetSitPosition() const { return transform.position; }

    void OnInteract(Player& player) override;
    std::string GetPrompt(const Player& player) const override;

private:
    int id;
    Customer* customer = nullptr;
    OrderManager* orderManager = nullptr;
};
