#pragma once

class Customer;
struct Recipe;

// 주문 진행 단계: 징징이가 받는 중 → 다 받음(영수증 들고 감) → 주방 창구에 넣음(플레이어 화면에 주문표 표시)
enum class OrderState {
    Taking,
    Taken,
    Submitted,
};

// 주문서 한 장: 손님, 메뉴, 자리 번호
struct Order {
    int id = 0;
    OrderState state = OrderState::Taking;
    Customer* customer = nullptr;
    const Recipe* recipe = nullptr; // 주문표 시스템이 정해지면 채운다
    int seatNumber = -1;
    float submittedTime = 0.0f; // 주문표가 뜬 시각 (UI 등장 애니메이션용)
};
