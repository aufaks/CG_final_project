#include "Scene/PlayScene.h"

#include "Core/GL.h"
#include "Core/InputManager.h"
#include "Render/ResourceManager.h"

#include <algorithm>
#include <iostream>

namespace {
const char* kMapPath = "obj/imported/krusty-krab-interior/Krusty_Krab_Interior.obj";
const char* kPlayerModelPath = "obj/imported/spongebob-squarepants/spongebob_bind.obj";
const char* kPlayerTexturePath = "obj/imported/spongebob-squarepants/textures/spongebob.png";

// ObjectViewer와 같은 배치: 가장 긴 변을 2로 맞춘 뒤 600배
constexpr float kMapDisplayScale = 600.0f;
// 집게리아 안쪽 바닥(초록 판자) 위, 정문(-Z)을 바라보는 시작 위치 (OBJ 원본 좌표)
const glm::vec3 kSpawnFeetInObj(-2575.0f, -125.0f, 2588.0f);
constexpr float kSpawnYaw = -90.0f;
constexpr float kPlayerHeight = 1.0f;

// 징징이: 계산대(배 모양 Boat1Mtl, "ORDER HERE" 간판 아래) 가운데에서 정문(-Z) 쪽 손님을 바라본다
// (폴더 이름은 squidwards-house지만 안에 든 Mesh_0037.rip.obj는 징징이 캐릭터 모델이다)
const char* kSquidwardModelPath = "obj/imported/squidwards-house/Mesh_0037.rip.obj";
const char* kSquidwardTexturePath = "obj/imported/squidwards-house/t0077_0.png";
const glm::vec3 kSquidwardFeetInObj(-2576.7f, -125.0f, 2595.4f);
constexpr float kSquidwardYaw = 180.0f; // 모델 정면(코) +Z → -Z
constexpr float kSquidwardHeight = 1.6f;

const glm::vec3 kClearColor(0.12f, 0.27f, 0.39f);
const glm::vec3 kLightDirection(-0.5f, -1.0f, -0.6f);
} // namespace

void PlayScene::Enter() {
    player.camera.fovDegrees = 55.0f;
    player.camera.farPlane = 250.0f;
    if (!player.LoadModel(kPlayerModelPath, kPlayerTexturePath, kPlayerHeight)) {
        std::cerr << "[PlayScene] 플레이어 모델을 불러오지 못했습니다.\n";
    }
    squidwardLoaded = squidward.LoadModel(kSquidwardModelPath, kSquidwardTexturePath, kSquidwardHeight);
    if (!squidwardLoaded) std::cerr << "[PlayScene] 징징이 모델을 불러오지 못했습니다.\n";
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
    player.Spawn((kSpawnFeetInObj - center) * scale, kSpawnYaw);
    squidward.Place((kSquidwardFeetInObj - center) * scale, kSquidwardYaw);

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
    player.Update(deltaTime);
    if (squidwardLoaded) squidward.Update(deltaTime);
}

void PlayScene::Render() {
    glClearColor(kClearColor.r, kClearColor.g, kClearColor.b, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    if (!map) return;

    int width = 0;
    int height = 0;
    glfwGetFramebufferSize(glfwGetCurrentContext(), &width, &height);
    if (width <= 0 || height <= 0) return;

    const Camera& camera = player.camera;
    ShaderManager& shaders = ResourceManager::Get().Shaders();
    shaders.Use("map");
    shaders.SetMat4("uProjection", camera.GetProjectionMatrix(static_cast<float>(width) / static_cast<float>(height)));
    shaders.SetMat4("uView", camera.GetViewMatrix());
    shaders.SetVec3("uLightDirection", glm::normalize(kLightDirection));
    shaders.SetBool("uReferenceLighting", true);
    shaders.SetInt("uTexture", 0);

    player.Render(shaders);
    if (squidwardLoaded) squidward.Render(shaders);
    map->Draw(shaders, mapMatrix, camera.position);
}
