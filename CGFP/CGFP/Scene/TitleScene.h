#pragma once
#include "Scene/Scene.h"

class TitleScene : public Scene {
public:
    void Enter() override;
    void Exit() override;
    void Update(float deltaTime) override;
    void Render() override;
};
