#pragma once
#include "Scene/Scene.h"

#include <memory>

// 씬 전환. ChangeScene은 예약만 하고, 실제 교체는 다음 프레임 시작(ApplyPendingChange)에 한다.
// 씬의 Update 도중 자기 자신을 지워 버리는 문제를 막기 위해서다.
class SceneManager {
public:
    void ChangeScene(std::unique_ptr<Scene> next);
    void ApplyPendingChange();
    void Update(float deltaTime);
    void Render();
    void Shutdown();

    void RequestQuit() { quitRequested = true; }
    bool ShouldQuit() const { return quitRequested; }

private:
    std::unique_ptr<Scene> current;
    std::unique_ptr<Scene> pending;
    bool quitRequested = false;
};
