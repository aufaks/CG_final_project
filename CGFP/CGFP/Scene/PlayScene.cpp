#include "Scene/PlayScene.h"

#include "Core/GL.h"

void PlayScene::Enter() {
}

void PlayScene::Exit() {
}

void PlayScene::Update(float deltaTime) {
}

void PlayScene::Render() {
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}
