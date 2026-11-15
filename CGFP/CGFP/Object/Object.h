#pragma once
#include "Core/GL.h"
#include "Object/Collider.h"

class Mesh;
class ShaderManager;
class Texture;

struct Transform {
    glm::vec3 position{ 0.0f };
    glm::vec3 rotation{ 0.0f }; // 오일러 각(도). Y → X → Z 순서로 적용
    glm::vec3 scale{ 1.0f };

    glm::mat4 GetMatrix() const;
};

// 모든 오브젝트의 부모. transform → model 행렬, Mesh, Update/Render.
class Object {
public:
    virtual ~Object() = default;

    virtual void Update(float deltaTime) {}
    virtual void Render(ShaderManager& shaders) const;
    virtual Collider GetWorldCollider() const; // Mesh 바운딩 박스 + transform

    // 다음 프레임 정리 단계에서 씬이 지운다. 바로 delete 하지 않는다.
    void Destroy() { destroyed = true; }
    bool IsDestroyed() const { return destroyed; }

    Transform transform;
    Mesh* mesh = nullptr;       // ResourceManager 소유
    Texture* texture = nullptr; // ResourceManager 소유. 없으면 color만 사용
    glm::vec3 color{ 0.8f };
    bool visible = true;
    bool solid = false; // true면 플레이어 이동을 막는다

protected:
    bool destroyed = false;
};
