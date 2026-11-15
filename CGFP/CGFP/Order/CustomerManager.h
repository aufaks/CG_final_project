#pragma once
#include "Character/Customer.h"

#include <memory>
#include <vector>

// 손님 관리: 스폰, 빈자리 배정, 웨이포인트 경로
class CustomerManager {
public:
    void Update(float deltaTime) {}

    std::vector<std::unique_ptr<Customer>> customers;
};
