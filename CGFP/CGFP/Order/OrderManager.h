#pragma once
#include "Order/Order.h"

#include <vector>

// 주문 관리: 진행 중인 주문 목록
class OrderManager {
public:
    void Update(float deltaTime) {}

    std::vector<Order> orders;
};
