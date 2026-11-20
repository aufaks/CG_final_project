#include "Render/Button.h"

#include "Core/InputManager.h"
#include "Render/UI.h"

namespace {
constexpr float kHoverScale = 1.05f;
const glm::vec4 kPressedColor(0.8f, 0.8f, 0.8f, 1.0f);
const glm::vec4 kDisabledColor(0.5f, 0.5f, 0.5f, 0.7f);
} // namespace

Button::Button(const std::string& imagePath, const glm::vec4& rect)
    : imagePath(imagePath), rect(rect) {}

void Button::Update() {
    clicked = false;
    if (!enabled) return;

    hovered = Contains(InputManager::GetMousePosition());
    if (hovered && InputManager::GetMouseButtonDown(GLFW_MOUSE_BUTTON_LEFT)) {
        pressed = true;
    }
    if (pressed && !InputManager::GetMouseButton(GLFW_MOUSE_BUTTON_LEFT)) {
        clicked = hovered;
        pressed = false;
    }
}

void Button::Draw(UI& ui) const {
    glm::vec4 drawRect = rect;
    glm::vec4 color(1.0f);

    if (!enabled) {
        color = kDisabledColor;
    } else if (pressed && hovered) {
        color = kPressedColor;
    } else if (hovered) {
        // 가운데를 기준으로 살짝 키운다
        const glm::vec2 grow = glm::vec2(rect.z, rect.w) * (kHoverScale - 1.0f);
        drawRect = glm::vec4(rect.x - grow.x * 0.5f, rect.y - grow.y * 0.5f, rect.z + grow.x, rect.w + grow.y);
    }

    ui.DrawImage(imagePath, drawRect, baseColor * color);
}

void Button::SetEnabled(bool value) {
    enabled = value;
    if (!enabled) {
        hovered = false;
        pressed = false;
        clicked = false;
    }
}

bool Button::Contains(const glm::vec2& point) const {
    return point.x >= rect.x && point.x <= rect.x + rect.z &&
           point.y >= rect.y && point.y <= rect.y + rect.w;
}
