#pragma once
#include "Scene/Scene.h"

// 타이틀: Enter → 오프닝 컷신, Esc → 종료.
// TODO(1단계): 타이틀 이미지/버튼 렌더링
class TitleScene : public Scene {
public:
    void Enter() override;
    void Update(float deltaTime) override;
    void Render() override;
};
