#include "Scene/TitleScene.h"

#include "Core/InputManager.h"
#include "Core/SoundManager.h"
#include "Scene/CutsceneScene.h"
#include "Scene/SceneManager.h"

#include <memory>

void TitleScene::Enter() {
    InputManager::SetCursorCaptured(false);
    glfwSetWindowTitle(InputManager::GetWindow(), "집게리아 : 게살 버거 만들기 | Enter: 시작  Esc: 종료");
    SoundManager::PlayBGM("title");
}

void TitleScene::Update(float) {
    if (InputManager::GetKeyDown(GLFW_KEY_ENTER)) sceneManager->ChangeScene(std::make_unique<CutsceneScene>());
    if (InputManager::GetKeyDown(GLFW_KEY_ESCAPE)) sceneManager->RequestQuit();
}

void TitleScene::Render() {
    glClearColor(0.95f, 0.75f, 0.3f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}
