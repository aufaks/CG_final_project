#pragma once
#include <vector>

struct Recipe;

// 하루·할당량: Day 번호, 영업 시간 타이머, 오늘 매출, 할당량, 메뉴 해금.
// 흐름도의 두 판정을 담당한다:
//   1) 영업 시간이 끝났는가 → IsBusinessTimeOver()
//   2) 할당량을 채웠는가     → IsQuotaMet()
class DayManager {
public:
    static constexpr int kLastDay = 5;

    void StartDay(std::vector<Recipe>& recipes); // 타이머·매출 초기화, 오늘 해금 메뉴 반영
    void Update(float deltaTime);
    void NextDay() { ++day; }

    bool IsOpen() const { return open; } // 새 손님을 받는 중인가
    bool IsBusinessTimeOver() const { return timeLeft <= 0.0f; }
    bool IsQuotaMet() const { return sales >= GetQuota(); }
    bool IsLastDay() const { return day >= kLastDay; }

    void AddSales(int amount) { sales += amount; }

    int GetDay() const { return day; }
    int GetSales() const { return sales; }
    int GetQuota() const;
    float GetTimeLeft() const { return timeLeft; }

    float businessDuration = 180.0f; // 하루 영업 시간(초)

private:
    int day = 1;
    int sales = 0;
    float timeLeft = 0.0f;
    bool open = false;
};
