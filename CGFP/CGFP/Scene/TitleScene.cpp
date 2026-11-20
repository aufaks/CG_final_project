#include "Scene/TitleScene.h"

#include "Core/GL.h"

void TitleScene::Enter() {
}

void TitleScene::Exit() {
}

void TitleScene::Update(float deltaTime) {
}

void TitleScene::Render() {
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}
