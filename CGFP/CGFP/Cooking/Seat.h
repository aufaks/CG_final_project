#pragma once
#include "Object/InteractableObject.h"

class Customer;

// 좌석
class Seat : public InteractableObject {
public:
    bool IsEmpty() const { return customer == nullptr; }

    Customer* customer = nullptr; // 앉은 손님
};
