#pragma once

class SceneManager;

// 모든 씬의 공통 부모. TitleScene, CutsceneScene, PlayScene, EndingScene이 상속한다.
class Scene {
public:
    virtual ~Scene() = default;

    virtual void Enter() {}
    virtual void Exit() {}
    virtual void Update(float deltaTime) = 0;
    virtual void Render() = 0;

    void SetSceneManager(SceneManager* manager) { sceneManager = manager; }

protected:
    SceneManager* sceneManager = nullptr;
};
