#pragma once
#include "Core/GL.h"

#include <string>

class UI;

// 화면 UI 버튼. 매 프레임 Update로 마우스 상태를 갱신하고, UI::Begin~End 사이에서 Draw한다.
// 버튼 안에서 눌렀다가 버튼 안에서 뗐을 때 클릭으로 친다.
class Button {
public:
    Button() = default;
    Button(const std::string& imagePath, const glm::vec4& rect);

    void Update();
    void Draw(UI& ui) const;

    bool IsClicked() const { return clicked; } // 클릭한 프레임에만 true
    bool IsHovered() const { return hovered; }
    bool IsPressed() const { return pressed; }

    void SetImage(const std::string& path) { imagePath = path; }
    void SetRect(const glm::vec4& value) { rect = value; }
    const glm::vec4& GetRect() const { return rect; }
    void SetColor(const glm::vec4& value) { baseColor = value; } // 이미지에 곱해지는 색. 이미지가 없으면 이 색 사각형
    void SetEnabled(bool value);
    bool IsEnabled() const { return enabled; }

private:
    bool Contains(const glm::vec2& point) const;

    std::string imagePath;
    glm::vec4 rect{ 0.0f }; // x, y, w, h (픽셀, 왼쪽 위 기준)
    glm::vec4 baseColor{ 1.0f };
    bool enabled = true;
    bool hovered = false;
    bool pressed = false; // 버튼 안에서 눌러서 아직 떼지 않음
    bool clicked = false;
};
