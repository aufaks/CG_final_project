#include "Scene/TitleScene.h"

#include "Core/GL.h"

namespace {
const char* kBackgroundPath = "ui/title_background.png";
} // namespace

void TitleScene::Enter() {
    ui.Init();
}

void TitleScene::Exit() {
    ui.Release();
}

void TitleScene::Update(float deltaTime) {
}

void TitleScene::Render() {
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    int width = 0;
    int height = 0;
    glfwGetFramebufferSize(glfwGetCurrentContext(), &width, &height);
    if (width <= 0 || height <= 0) return; // 최소화 상태

    ui.Begin(width, height);
    ui.DrawImageCover(kBackgroundPath);
    ui.End();
}
