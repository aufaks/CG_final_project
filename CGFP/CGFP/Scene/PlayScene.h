#pragma once
#include "Character/Player.h"
#include "Character/Squidward.h"
#include "Render/Model.h"
#include "Scene/Scene.h"

#include <memory>

class PlayScene : public Scene {
public:
    void Enter() override;
    void Exit() override;
    void Update(float deltaTime) override;
    void Render() override;

private:
    void LoadMap();

    std::unique_ptr<Model> map;
    glm::mat4 mapMatrix{ 1.0f };
    Player player;
    Squidward squidward;
    bool squidwardLoaded = false;
    bool loadRequested = false; // 첫 프레임은 화면을 먼저 그리고, 다음 Update에서 맵을 읽는다
    bool loadAttempted = false;
};
