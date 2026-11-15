#include "Scene/SceneManager.h"

void SceneManager::ChangeScene(std::unique_ptr<Scene> next) {
    pending = std::move(next);
}

void SceneManager::ApplyPendingChange() {
    if (!pending) return;
    if (current) current->Exit();
    current = std::move(pending);
    current->SetSceneManager(this);
    current->Enter();
}

void SceneManager::Update(float deltaTime) {
    if (current) current->Update(deltaTime);
}

void SceneManager::Render() {
    if (current) current->Render();
}

void SceneManager::Shutdown() {
    if (current) current->Exit();
    current.reset();
    pending.reset();
}
