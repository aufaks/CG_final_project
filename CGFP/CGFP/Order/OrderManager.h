#pragma once
#include "Order/Order.h"

#include <memory>
#include <vector>

// 주문 관리: 진행 중인 주문 목록. Order는 unique_ptr로 들고 있어서 손님·징징이가 가진 포인터가 바뀌지 않는다.
class OrderManager {
public:
    void Update(float deltaTime) { time += deltaTime; }

    Order* CreateOrder(Customer* customer) {
        auto order = std::make_unique<Order>();
        order->id = nextId++;
        order->customer = customer;
        orders.push_back(std::move(order));
        return orders.back().get();
    }

    // 징징이가 주방 창구에 영수증을 넣으면 부른다 → 주문표가 화면에 뜬다
    void Submit(Order& order) {
        order.state = OrderState::Submitted;
        order.submittedTime = time;
    }

    float GetTime() const { return time; }

    std::vector<std::unique_ptr<Order>> orders;

private:
    int nextId = 1;
    float time = 0.0f;
};
