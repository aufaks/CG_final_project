#pragma once
#include "Render/Button.h"
#include "Render/UI.h"
#include "Scene/Scene.h"

class TitleScene : public Scene {
public:
    void Enter() override;
    void Exit() override;
    void Update(float deltaTime) override;
    void Render() override;

private:
    void LayoutButtons(int screenWidth, int screenHeight);

    UI ui;
    Button startButton;
};
