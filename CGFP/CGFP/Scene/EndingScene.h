#pragma once
#include "Scene/Scene.h"

class EndingScene : public Scene {
public:
    void Enter() override;
    void Exit() override;
    void Update(float deltaTime) override;
    void Render() override;
};
