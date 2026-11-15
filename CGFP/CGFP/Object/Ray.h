#pragma once
#include "Core/GL.h"

#include <vector>

class Camera;
class InteractableObject;
class Object;

// 화면 중앙 조준점에서 쏘는 레이. E·F 상호작용과 내려놓기 위치 찾기에 같이 쓴다.
class Ray {
public:
    glm::vec3 origin{ 0.0f };
    glm::vec3 direction{ 0.0f, 0.0f, -1.0f }; // 정규화된 방향
    float maxDistance = 3.0f;

    static Ray FromCamera(const Camera& camera, float maxDistance = 3.0f);

    // 가장 가까운 상호작용 대상. 들고 있거나 비활성인 대상은 건너뛴다
    InteractableObject* Pick(const std::vector<InteractableObject*>& candidates, float* outDistance = nullptr) const;
    // 가장 가까운 일반 오브젝트(바닥·작업대 등 내려놓을 곳 찾기)
    Object* PickObject(const std::vector<Object*>& candidates, float* outDistance = nullptr) const;

    glm::vec3 GetPoint(float distance) const { return origin + direction * distance; }
};
