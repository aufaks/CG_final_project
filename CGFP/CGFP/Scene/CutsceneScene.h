#pragma once
#include "Character/Camera.h"
#include "Event/Cutscene.h"
#include "Scene/Scene.h"

// 오프닝 컷신: Enter로 대사 넘기기, Space로 전체 건너뛰기. 끝나면 PlayScene.
class CutsceneScene : public Scene {
public:
    void Enter() override;
    void Update(float deltaTime) override;
    void Render() override;

private:
    void ShowCurrentLine();

    Cutscene cutscene;
    Camera camera;
};
