#pragma once

// 컷신: 카메라·NPC 키프레임, 대사 큐
class Cutscene {
public:
    void Start() {}
    void Update(float deltaTime) {}
    bool IsFinished() const { return true; }
};
