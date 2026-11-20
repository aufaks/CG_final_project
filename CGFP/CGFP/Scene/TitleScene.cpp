#include "Scene/TitleScene.h"

#include "Core/GL.h"
#include "Scene/PlayScene.h"
#include "Scene/SceneManager.h"

#include <memory>

namespace {
const char* kBackgroundPath = "ui/title_background.png";
const char* kStartButtonPath = "ui/start_button.png"; // 없으면 단색 사각형으로 그려진다

// 1280x720 기준 배치. 창 크기가 바뀌면 높이 비율로 같이 늘린다.
constexpr float kReferenceHeight = 720.0f;
const glm::vec4 kStartButtonRect(140.0f, 470.0f, 280.0f, 90.0f); // 이미지(560x180)와 같은 비율
const glm::vec4 kStartButtonColor(1.0f, 1.0f, 1.0f, 1.0f);

bool GetScreenSize(int& width, int& height) {
    glfwGetFramebufferSize(glfwGetCurrentContext(), &width, &height);
    return width > 0 && height > 0; // 최소화 상태면 false
}
} // namespace

void TitleScene::Enter() {
    ui.Init();
    startButton.SetImage(kStartButtonPath);
    startButton.SetColor(kStartButtonColor);
}

void TitleScene::Exit() {
    ui.Release();
}

void TitleScene::Update(float deltaTime) {
    int width = 0;
    int height = 0;
    if (!GetScreenSize(width, height)) return;
    LayoutButtons(width, height);

    startButton.Update();
    if (startButton.IsClicked() && sceneManager) {
        sceneManager->ChangeScene(std::make_unique<PlayScene>());
    }
}

void TitleScene::Render() {
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    int width = 0;
    int height = 0;
    if (!GetScreenSize(width, height)) return;

    ui.Begin(width, height);
    ui.DrawImageCover(kBackgroundPath);
    startButton.Draw(ui);
    ui.End();
}

void TitleScene::LayoutButtons(int screenWidth, int screenHeight) {
    const float scale = static_cast<float>(screenHeight) / kReferenceHeight;
    // 가로는 화면 비율이 달라도 왼쪽에서 같은 비율 위치에 오도록 맞춘다
    const float xScale = static_cast<float>(screenWidth) / (kReferenceHeight * 16.0f / 9.0f);
    startButton.SetRect(glm::vec4(kStartButtonRect.x * xScale, kStartButtonRect.y * scale,
                                  kStartButtonRect.z * scale, kStartButtonRect.w * scale));
}
