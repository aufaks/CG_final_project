#pragma once

class Customer;
struct Recipe;

// 주문서 한 장: 손님, 메뉴, 자리 번호
struct Order {
    Customer* customer = nullptr;
    const Recipe* recipe = nullptr;
    int seatNumber = -1;
};
