#pragma once

// 특별 이벤트(시간 남으면 5단계): 뚱이·멸치 손님, 플랑크톤 비법 도둑.
// TODO(5단계): 각 이벤트의 실제 동작(특별 주문, 도둑 쫓아내기 등) 구현
enum class SpecialEventType {
    SpecialCustomer, // 뚱이
    AnchovyGroup,    // 멸치 떼 손님
    PlanktonThief,   // 플랑크톤 비법 도둑
};

class SpecialEvent {
public:
    explicit SpecialEvent(SpecialEventType type) : type(type) {}

    void Trigger();
    void Update(float deltaTime);
    void Finish();

    SpecialEventType GetType() const { return type; }
    bool IsActive() const { return active; }
    bool IsFinished() const { return finished; }

    float duration = 30.0f; // 이벤트가 이어지는 시간(초)

private:
    SpecialEventType type;
    bool active = false;
    bool finished = false;
    float timer = 0.0f;
};
