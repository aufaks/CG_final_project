// 집게리아 : 게살 버거 만들기 — 진입점
// 프레임 순서: 입력 수집 → 씬 전환 적용 → 업데이트 → 렌더 → 입력 상태 정리
#include <Windows.h> // GL 헤더보다 먼저 (APIENTRY 재정의 경고 방지)

#include "Core/GL.h"
#include "Core/InputManager.h"
#include "Render/ResourceManager.h"
#include "Scene/SceneManager.h"
#include "Scene/TitleScene.h"

#include <algorithm>
#include <iostream>
#include <memory>

namespace {
constexpr int kWindowWidth = 1280;
constexpr int kWindowHeight = 720;
constexpr double kMaxDeltaTime = 0.05; // 창을 끌다 놓았을 때 한 번에 크게 진행되지 않게 제한
} // namespace

int main() {
    // 소스가 UTF-8(/utf-8)이므로 콘솔도 UTF-8로 출력해야 한글이 깨지지 않는다
    SetConsoleOutputCP(CP_UTF8);

    // 텍스처 디코딩(WIC)에 필요
    const HRESULT com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    if (!glfwInit()) {
        std::cerr << "GLFW 초기화 실패\n";
        return 1;
    }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_STENCIL_BITS, 8);

    GLFWwindow* window = glfwCreateWindow(kWindowWidth, kWindowHeight, "집게리아", nullptr, nullptr);
    if (!window) {
        std::cerr << "창 생성 실패\n";
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
    glfwSetFramebufferSizeCallback(window, [](GLFWwindow*, int width, int height) {
        glViewport(0, 0, width, height);
    });

    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        std::cerr << "GLEW 초기화 실패\n";
        glfwTerminate();
        return 1;
    }
    glEnable(GL_DEPTH_TEST);

    InputManager::Init(window);
    if (!ResourceManager::Get().Shaders().Load("basic", "vertex.glsl", "fragment.glsl")) {
        std::cerr << "셰이더를 불러오지 못했습니다. 작업 디렉터리가 프로젝트 폴더(CGFP/CGFP)인지 확인하세요.\n";
    }
    if (!ResourceManager::Get().Shaders().Load("ui", "ui_vertex.glsl", "ui_fragment.glsl")) {
        std::cerr << "UI 셰이더를 불러오지 못했습니다.\n";
    }
    if (!ResourceManager::Get().Shaders().Load("map", "shaders/object_viewer.vert", "shaders/object_viewer.frag")) {
        std::cerr << "맵 셰이더를 불러오지 못했습니다.\n";
    }
    if (!ResourceManager::Get().Shaders().Load("outline", "shaders/outline.vert", "shaders/outline.frag")) {
        std::cerr << "외곽선 셰이더를 불러오지 못했습니다.\n";
    }
    if (!ResourceManager::Get().Shaders().Load("skinned", "shaders/skinned.vert", "shaders/object_viewer.frag")) {
        std::cerr << "스키닝 셰이더를 불러오지 못했습니다.\n";
    }

    SceneManager sceneManager;
    sceneManager.ChangeScene(std::make_unique<TitleScene>());

    double lastTime = glfwGetTime();
    while (!glfwWindowShouldClose(window) && !sceneManager.ShouldQuit()) {
        glfwPollEvents();

        const double now = glfwGetTime();
        const float deltaTime = static_cast<float>(std::min(now - lastTime, kMaxDeltaTime));
        lastTime = now;

        sceneManager.ApplyPendingChange();
        sceneManager.Update(deltaTime);
        sceneManager.Render();

        glfwSwapBuffers(window);
        InputManager::EndFrame();
    }

    // GL 리소스는 컨텍스트가 살아 있을 때 정리한다
    sceneManager.Shutdown();
    ResourceManager::Get().Clear();
    glfwDestroyWindow(window);
    glfwTerminate();
    if (SUCCEEDED(com)) CoUninitialize();
    return 0;
}
