#include "Scene/PlayScene.h"

#include "Core/GL.h"
#include "Core/InputManager.h"
#include "Render/ResourceManager.h"

#include <algorithm>
#include <iostream>

namespace {
const char* kMapPath = "obj/imported/krusty-krab-interior/Krusty_Krab_Interior.obj";

// ObjectViewer와 같은 배치: 가장 긴 변을 2로 맞춘 뒤 600배
constexpr float kMapDisplayScale = 600.0f;
// 집게리아 안쪽, 정문(-Z)을 바라보는 시작 위치 (OBJ 원본 좌표)
const glm::vec3 kStartPositionInObj(-2575.0f, -117.0f, 2588.0f);

constexpr float kMoveSpeed = 2.5f;
constexpr float kMouseSensitivity = 0.05f;
const glm::vec3 kClearColor(0.12f, 0.27f, 0.39f);
const glm::vec3 kLightDirection(-0.5f, -1.0f, -0.6f);
} // namespace

void PlayScene::Enter() {
    camera.fovDegrees = 55.0f;
    camera.nearPlane = 0.05f;
    camera.farPlane = 250.0f;
    InputManager::SetCursorCaptured(true);
}

void PlayScene::Exit() {
    InputManager::SetCursorCaptured(false);
    map.reset();
}

void PlayScene::LoadMap() {
    const double start = glfwGetTime();
    map = std::make_unique<Model>();
    if (!map->LoadObj(kMapPath)) {
        map.reset();
        return;
    }

    const glm::vec3 boundsMin = map->GetBoundsMin();
    const glm::vec3 boundsMax = map->GetBoundsMax();
    const glm::vec3 center = (boundsMin + boundsMax) * 0.5f;
    const glm::vec3 size = boundsMax - boundsMin;
    const float largest = std::max({ size.x, size.y, size.z });
    const float scale = (largest > 0.0f ? 2.0f / largest : 1.0f) * kMapDisplayScale;

    mapMatrix = glm::scale(glm::mat4(1.0f), glm::vec3(scale)) * glm::translate(glm::mat4(1.0f), -center);
    camera.position = (kStartPositionInObj - center) * scale;
    camera.yaw = -90.0f;
    camera.pitch = 0.0f;

    std::cout << "[PlayScene] 맵 로드 " << (glfwGetTime() - start) << "초" << std::endl;
}

void PlayScene::Update(float deltaTime) {
    if (!loadRequested) {
        loadRequested = true;
        return;
    }
    if (!loadAttempted) { // 실패해도 매 프레임 다시 읽지 않는다
        loadAttempted = true;
        LoadMap();
    }

    if (InputManager::GetKeyDown(GLFW_KEY_P)) {
        InputManager::SetCursorCaptured(!InputManager::IsCursorCaptured());
    }
    UpdateCamera(deltaTime);
}

void PlayScene::UpdateCamera(float deltaTime) {
    if (InputManager::IsCursorCaptured()) {
        const glm::vec2 delta = InputManager::GetMouseDelta();
        camera.AddRotation(delta.x * kMouseSensitivity, -delta.y * kMouseSensitivity);
    }

    glm::vec3 move(0.0f);
    if (InputManager::GetKey(GLFW_KEY_W)) move += camera.GetFlatForward();
    if (InputManager::GetKey(GLFW_KEY_S)) move -= camera.GetFlatForward();
    if (InputManager::GetKey(GLFW_KEY_D)) move += camera.GetRight();
    if (InputManager::GetKey(GLFW_KEY_A)) move -= camera.GetRight();
    if (InputManager::GetKey(GLFW_KEY_E)) move.y += 1.0f;
    if (InputManager::GetKey(GLFW_KEY_Q)) move.y -= 1.0f;
    if (glm::length(move) > 0.0f) camera.position += glm::normalize(move) * kMoveSpeed * deltaTime;
}

void PlayScene::Render() {
    glClearColor(kClearColor.r, kClearColor.g, kClearColor.b, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    if (!map) return;

    int width = 0;
    int height = 0;
    glfwGetFramebufferSize(glfwGetCurrentContext(), &width, &height);
    if (width <= 0 || height <= 0) return;

    ShaderManager& shaders = ResourceManager::Get().Shaders();
    shaders.Use("map");
    shaders.SetMat4("uProjection", camera.GetProjectionMatrix(static_cast<float>(width) / static_cast<float>(height)));
    shaders.SetMat4("uView", camera.GetViewMatrix());
    shaders.SetVec3("uLightDirection", glm::normalize(kLightDirection));
    shaders.SetBool("uReferenceLighting", true);
    map->Draw(shaders, mapMatrix, camera.position);
}
