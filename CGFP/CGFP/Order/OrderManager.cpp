#include "Order/OrderManager.h"

#include "Character/Customer.h"
#include "Cooking/Plate.h"
#include "Order/DayManager.h"

#include <algorithm>

OrderManager::OrderManager() {
    using T = IngredientType;
    // TODO: 기획서 메뉴/가격으로 교체
    recipes.push_back({ "게살 버거", { T::BottomBun, T::Patty, T::Cheese, T::Lettuce, T::TopBun }, 10, 1 });
    recipes.push_back({ "토마토 게살 버거", { T::BottomBun, T::Patty, T::Cheese, T::Lettuce, T::Tomato, T::TopBun }, 13, 2 });
    recipes.push_back({ "더블 게살 버거", { T::BottomBun, T::Patty, T::Cheese, T::Patty, T::Cheese, T::Lettuce, T::TopBun }, 18, 3 });
}

void OrderManager::Update(float deltaTime) {
    for (Order& order : activeOrders) order.elapsed += deltaTime;
    // TODO(3단계): 너무 오래 기다리면 손님이 화나서 나가기
}

const Recipe* OrderManager::PickRandomRecipe() {
    std::vector<const Recipe*> available;
    for (const Recipe& recipe : recipes) {
        if (recipe.unlocked) available.push_back(&recipe);
    }
    if (available.empty()) return nullptr;
    std::uniform_int_distribution<size_t> pick(0, available.size() - 1);
    return available[pick(rng)];
}

int OrderManager::CreateOrder(Customer* customer, int seatId) {
    const Recipe* recipe = PickRandomRecipe();
    if (!recipe) return -1;
    Order order;
    order.id = nextOrderId++;
    order.customer = customer;
    order.recipe = recipe;
    order.seatId = seatId;
    activeOrders.push_back(order);
    return order.id;
}

bool OrderManager::TryServe(int seatId, const Plate& plate) {
    auto it = std::find_if(activeOrders.begin(), activeOrders.end(), [seatId](const Order& o) { return o.seatId == seatId; });
    if (it == activeOrders.end() || !it->recipe || !it->recipe->Matches(plate)) return false;

    if (dayManager) dayManager->AddSales(it->recipe->price);
    if (it->customer) it->customer->OnServed();
    activeOrders.erase(it);
    return true;
}

void OrderManager::CancelOrder(int orderId) {
    activeOrders.erase(std::remove_if(activeOrders.begin(), activeOrders.end(),
        [orderId](const Order& o) { return o.id == orderId; }), activeOrders.end());
}

void OrderManager::Clear() {
    activeOrders.clear();
}

const Order* OrderManager::FindOrderBySeat(int seatId) const {
    for (const Order& order : activeOrders) {
        if (order.seatId == seatId) return &order;
    }
    return nullptr;
}
