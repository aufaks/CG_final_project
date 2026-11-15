#pragma once

// 하루·할당량: Day 번호, 영업 시간 타이머, 오늘 매출, 할당량
class DayManager {
public:
    void Update(float deltaTime) {}

    int day = 1;
    float timeLeft = 0.0f;
    int sales = 0;
    int quota = 0;
};
