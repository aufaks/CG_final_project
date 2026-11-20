#include "Scene/EndingScene.h"

#include "Core/GL.h"

void EndingScene::Enter() {
}

void EndingScene::Exit() {
}

void EndingScene::Update(float deltaTime) {
}

void EndingScene::Render() {
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}
