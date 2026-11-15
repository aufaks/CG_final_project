#include "Scene/EndingScene.h"

#include "Core/InputManager.h"
#include "Core/SoundManager.h"
#include "Scene/SceneManager.h"
#include "Scene/TitleScene.h"

#include <memory>
#include <string>

void EndingScene::Enter() {
    InputManager::SetCursorCaptured(false);
    const std::string title = success
        ? "성공! " + std::to_string(day) + "일 동안 할당량을 모두 채웠다 | Enter: 타이틀로"
        : "게임 오버: " + std::to_string(day) + "일차 할당량 미달 | Enter: 타이틀로";
    glfwSetWindowTitle(InputManager::GetWindow(), title.c_str());
    SoundManager::PlayBGM(success ? "ending_success" : "ending_fail", false);
}

void EndingScene::Update(float) {
    if (InputManager::GetKeyDown(GLFW_KEY_ENTER)) sceneManager->ChangeScene(std::make_unique<TitleScene>());
}

void EndingScene::Render() {
    if (success) glClearColor(0.3f, 0.75f, 0.4f, 1.0f);
    else glClearColor(0.6f, 0.15f, 0.15f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}
