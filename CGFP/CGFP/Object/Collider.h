#pragma once
#include "Core/GL.h"

// 월드 좌표 AABB 충돌 박스. 플레이어·NPC·물체 충돌과 레이 판정에 같이 쓴다.
// 오브젝트를 그리는 transform에서 바로 만들어서(FromLocalBounds) 보이는 위치와 판정이 어긋나지 않게 한다.
class Collider {
public:
    glm::vec3 min{ 0.0f };
    glm::vec3 max{ 0.0f };

    Collider() = default;
    Collider(const glm::vec3& min, const glm::vec3& max) : min(min), max(max) {}

    // 로컬 바운딩 박스의 8개 꼭짓점을 model 행렬로 옮긴 뒤 감싸는 AABB
    static Collider FromLocalBounds(const glm::vec3& localMin, const glm::vec3& localMax, const glm::mat4& model);

    bool Intersects(const Collider& other) const;
    bool Contains(const glm::vec3& point) const;
    // 슬랩 방식 레이 교차. 맞으면 outDistance에 거리(0~maxDistance)를 넣고 true
    bool IntersectsRay(const glm::vec3& origin, const glm::vec3& direction, float maxDistance, float& outDistance) const;
};
