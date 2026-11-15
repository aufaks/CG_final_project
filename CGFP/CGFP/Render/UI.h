#pragma once
#include <string>

class Camera;
class Customer;
class DayManager;
class OrderManager;
class ShaderManager;

// 화면 UI(주문서, 돈, 시간, 조준점)와 월드 UI(손님 머리 위 말풍선 빌보드).
// TODO(3단계): 글자 렌더링(비트맵 폰트 텍스처)을 붙이기 전까지 HUD는 창 제목에 표시한다.
class UI {
public:
    void SetPrompt(const std::string& text) { prompt = text; }

    // 화면 UI
    void DrawScreen(const OrderManager& orders, const DayManager& day);
    void DrawCrosshair(ShaderManager& shaders, const Camera& camera);

    // 월드 UI: 음식을 기다리는 손님 머리 위 말풍선
    // TODO(3단계): 말풍선 텍스처 + 카메라를 향하는 빌보드로 교체
    void DrawSpeechBubble(ShaderManager& shaders, const Customer& customer);

private:
    std::string prompt;
    std::string lastTitle;
};
