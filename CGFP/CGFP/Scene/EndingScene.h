#pragma once
#include "Scene/Scene.h"

// 엔딩: 마지막 날까지 할당량을 채우면 성공, 못 채우면 게임 오버. Enter → 타이틀.
// TODO(4단계): 엔딩 화면/컷신
class EndingScene : public Scene {
public:
    EndingScene(bool success, int day) : success(success), day(day) {}

    void Enter() override;
    void Update(float deltaTime) override;
    void Render() override;

private:
    bool success;
    int day;
};
