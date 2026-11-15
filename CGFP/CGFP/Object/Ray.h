#pragma once
#include "Character/Camera.h"
#include "Core/GL.h"

// 피킹: 화면 중앙 조준점에서 쏘는 레이. 맞았는지는 Collider::IntersectsRay로 판정한다.
class Ray {
public:
    glm::vec3 origin{ 0.0f };
    glm::vec3 direction{ 0.0f, 0.0f, -1.0f }; // 정규화된 방향

    static Ray FromCamera(const Camera& camera) {
        Ray ray;
        ray.origin = camera.position;
        ray.direction = camera.GetForward();
        return ray;
    }
};
