#include "Scene/CutsceneScene.h"

#include "Core/GL.h"

void CutsceneScene::Enter() {
}

void CutsceneScene::Exit() {
}

void CutsceneScene::Update(float deltaTime) {
}

void CutsceneScene::Render() {
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}
