#pragma once

class Customer;
struct Recipe;

// 주문서 한 장: 손님, 메뉴, 자리 번호.
// 서빙할 때 Plate 스택을 recipe와 비교한다(OrderManager::TryServe).
struct Order {
    int id = -1;
    Customer* customer = nullptr;
    const Recipe* recipe = nullptr; // OrderManager가 가진 레시피 목록을 가리킴
    int seatId = -1;
    float elapsed = 0.0f;           // 주문 후 지난 시간(인내심/팁 계산용)
};
