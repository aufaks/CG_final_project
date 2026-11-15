#pragma once
#include "Order/Order.h"
#include "Order/Recipe.h"

#include <random>
#include <vector>

class DayManager;
class Plate;

// 진행 중인 주문 목록과 메뉴(레시피) 목록. 주문서 UI가 여기를 읽는다.
class OrderManager {
public:
    OrderManager();

    void SetDayManager(DayManager* manager) { dayManager = manager; }
    void Update(float deltaTime);

    // 해금된 메뉴 중 하나를 골라 주문을 만든다. 주문 id를 돌려준다(메뉴가 없으면 -1)
    int CreateOrder(Customer* customer, int seatId);
    // 자리 seatId의 주문과 접시를 비교. 맞으면 매출 반영 + 손님 OnServed 후 true
    bool TryServe(int seatId, const Plate& plate);
    void CancelOrder(int orderId);
    void Clear();

    const Order* FindOrderBySeat(int seatId) const;
    const std::vector<Order>& GetActiveOrders() const { return activeOrders; }
    std::vector<Recipe>& GetRecipes() { return recipes; }

private:
    const Recipe* PickRandomRecipe();

    std::vector<Recipe> recipes; // 생성자에서 채우고 크기를 바꾸지 않는다(Order가 포인터로 가리킴)
    std::vector<Order> activeOrders;
    int nextOrderId = 1;
    DayManager* dayManager = nullptr;
    std::mt19937 rng{ std::random_device{}() };
};
