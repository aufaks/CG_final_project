#pragma once
#include "Core/GL.h"

#include <string>

// 화면 UI(주문서, 돈, 시간)와 월드 UI(손님 머리 위 말풍선 빌보드)
// 단위 사각형 하나를 "ui" 셰이더로 위치·크기만 바꿔 가며 그린다.
class UI {
public:
    UI() = default;
    ~UI();
    UI(const UI&) = delete;
    UI& operator=(const UI&) = delete;

    void Init();    // GL 컨텍스트가 있을 때 호출
    void Release(); // GL 컨텍스트가 살아 있을 때 호출

    // Begin과 End 사이에서 Draw* 호출. 깊이 테스트를 끄고 알파 블렌딩을 켠다.
    void Begin(int screenWidth, int screenHeight);
    void End();

    // rect = x, y, w, h (픽셀, 왼쪽 위 기준)
    void DrawImage(const std::string& path, const glm::vec4& rect, const glm::vec4& color = glm::vec4(1.0f));
    void DrawRect(const glm::vec4& rect, const glm::vec4& color);
    // 비율을 유지한 채 화면을 꽉 채운다. 넘치는 부분은 잘린다.
    void DrawImageCover(const std::string& path);

    void DrawScreenUI() {}
    void DrawWorldUI() {}

private:
    void DrawQuad(const glm::vec4& rect, const glm::vec4& uvRect, const glm::vec4& color, bool useTexture);

    GLuint vao = 0;
    GLuint vbo = 0;
    int width = 0;
    int height = 0;
};
