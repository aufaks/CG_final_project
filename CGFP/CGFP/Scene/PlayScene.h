#pragma once
#include "Core/GL.h"
#include "Scene/Scene.h"

class PlayScene : public Scene {
public:
    void Enter() override {}
    void Exit() override {}
    void Update(float deltaTime) override {}
    void Render() override {
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }
};
