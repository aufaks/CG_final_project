#pragma once
#include "Core/GL.h"

// 키보드·마우스 상태를 한 곳에서 관리한다.
// GLFW 콜백은 상태만 기록하고, 게임 규칙은 각 씬의 Update에서 처리한다.
class InputManager {
public:
    static void Init(GLFWwindow* window);
    static void EndFrame(); // 프레임 끝에서 호출: 눌린 순간·마우스 이동량 초기화

    static bool GetKey(int key);      // 누르고 있는 동안 true
    static bool GetKeyDown(int key);  // 누른 순간 한 프레임만 true (E/F 상호작용용)
    static bool GetMouseButton(int button);
    static bool GetMouseButtonDown(int button);
    static glm::vec2 GetMouseDelta(); // 이번 프레임 마우스 이동량(픽셀)
    static glm::vec2 GetMousePosition(); // 창 왼쪽 위 기준, 프레임버퍼 픽셀 (UI 좌표와 같음)

    static void SetCursorCaptured(bool captured);
    static bool IsCursorCaptured();
    static GLFWwindow* GetWindow();

private:
    static void KeyCallback(GLFWwindow* w, int key, int scancode, int action, int mods);
    static void MouseButtonCallback(GLFWwindow* w, int button, int action, int mods);
    static void CursorPosCallback(GLFWwindow* w, double x, double y);

    static constexpr int kKeyCount = GLFW_KEY_LAST + 1;
    static constexpr int kButtonCount = GLFW_MOUSE_BUTTON_LAST + 1;

    inline static GLFWwindow* window = nullptr;
    inline static bool keys[kKeyCount] = {};
    inline static bool keysPressed[kKeyCount] = {};
    inline static bool buttons[kButtonCount] = {};
    inline static bool buttonsPressed[kButtonCount] = {};
    inline static glm::vec2 mouseDelta{ 0.0f };
    inline static glm::dvec2 lastMouse{ 0.0 };
    inline static bool firstMouse = true;
    inline static bool cursorCaptured = false;
};
