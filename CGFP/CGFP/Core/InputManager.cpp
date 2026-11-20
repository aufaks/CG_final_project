#include "Core/InputManager.h"

#include <algorithm>
#include <iterator>

void InputManager::Init(GLFWwindow* w) {
    window = w;
    glfwSetKeyCallback(w, KeyCallback);
    glfwSetMouseButtonCallback(w, MouseButtonCallback);
    glfwSetCursorPosCallback(w, CursorPosCallback);
}

void InputManager::EndFrame() {
    std::fill(std::begin(keysPressed), std::end(keysPressed), false);
    std::fill(std::begin(buttonsPressed), std::end(buttonsPressed), false);
    mouseDelta = glm::vec2(0.0f);
}

bool InputManager::GetKey(int key) { return key >= 0 && key < kKeyCount && keys[key]; }
bool InputManager::GetKeyDown(int key) { return key >= 0 && key < kKeyCount && keysPressed[key]; }
bool InputManager::GetMouseButton(int button) { return button >= 0 && button < kButtonCount && buttons[button]; }
bool InputManager::GetMouseButtonDown(int button) { return button >= 0 && button < kButtonCount && buttonsPressed[button]; }
glm::vec2 InputManager::GetMouseDelta() { return mouseDelta; }

glm::vec2 InputManager::GetMousePosition() {
    if (!window) return glm::vec2(0.0f);
    double x = 0.0, y = 0.0;
    glfwGetCursorPos(window, &x, &y);

    // 커서 좌표는 창 좌표라서, 화면 배율(DPI)이 100%가 아니면 프레임버퍼 픽셀과 다르다
    int windowW = 0, windowH = 0, framebufferW = 0, framebufferH = 0;
    glfwGetWindowSize(window, &windowW, &windowH);
    glfwGetFramebufferSize(window, &framebufferW, &framebufferH);
    if (windowW <= 0 || windowH <= 0) return glm::vec2(0.0f);
    return glm::vec2(static_cast<float>(x) * framebufferW / windowW, static_cast<float>(y) * framebufferH / windowH);
}

void InputManager::SetCursorCaptured(bool captured) {
    cursorCaptured = captured;
    firstMouse = true; // 캡처 전환 직후 큰 이동량이 튀지 않게 기준점을 다시 잡는다
    if (window) glfwSetInputMode(window, GLFW_CURSOR, captured ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
}

bool InputManager::IsCursorCaptured() { return cursorCaptured; }
GLFWwindow* InputManager::GetWindow() { return window; }

void InputManager::KeyCallback(GLFWwindow*, int key, int, int action, int) {
    if (key < 0 || key >= kKeyCount) return;
    if (action == GLFW_PRESS) {
        keys[key] = true;
        keysPressed[key] = true;
    } else if (action == GLFW_RELEASE) {
        keys[key] = false;
    }
}

void InputManager::MouseButtonCallback(GLFWwindow*, int button, int action, int) {
    if (button < 0 || button >= kButtonCount) return;
    if (action == GLFW_PRESS) {
        buttons[button] = true;
        buttonsPressed[button] = true;
    } else if (action == GLFW_RELEASE) {
        buttons[button] = false;
    }
}

void InputManager::CursorPosCallback(GLFWwindow*, double x, double y) {
    if (firstMouse) { // 첫 이벤트는 기준 좌표 설정에만 사용
        lastMouse = { x, y };
        firstMouse = false;
        return;
    }
    mouseDelta += glm::vec2(static_cast<float>(x - lastMouse.x), static_cast<float>(y - lastMouse.y));
    lastMouse = { x, y };
}
