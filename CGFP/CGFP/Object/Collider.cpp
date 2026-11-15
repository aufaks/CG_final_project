#include "Object/Collider.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <utility>

Collider Collider::FromLocalBounds(const glm::vec3& localMin, const glm::vec3& localMax, const glm::mat4& model) {
    Collider result(glm::vec3(FLT_MAX), glm::vec3(-FLT_MAX));
    for (int i = 0; i < 8; ++i) {
        const glm::vec3 corner((i & 1) ? localMax.x : localMin.x,
                               (i & 2) ? localMax.y : localMin.y,
                               (i & 4) ? localMax.z : localMin.z);
        const glm::vec3 world = glm::vec3(model * glm::vec4(corner, 1.0f));
        result.min = glm::min(result.min, world);
        result.max = glm::max(result.max, world);
    }
    return result;
}

bool Collider::Intersects(const Collider& other) const {
    return min.x <= other.max.x && max.x >= other.min.x &&
           min.y <= other.max.y && max.y >= other.min.y &&
           min.z <= other.max.z && max.z >= other.min.z;
}

bool Collider::Contains(const glm::vec3& p) const {
    return p.x >= min.x && p.x <= max.x && p.y >= min.y && p.y <= max.y && p.z >= min.z && p.z <= max.z;
}

bool Collider::IntersectsRay(const glm::vec3& origin, const glm::vec3& direction, float maxDistance, float& outDistance) const {
    float tMin = 0.0f;
    float tMax = maxDistance;
    for (int axis = 0; axis < 3; ++axis) {
        if (std::abs(direction[axis]) < 1e-6f) {
            if (origin[axis] < min[axis] || origin[axis] > max[axis]) return false;
            continue;
        }
        const float inv = 1.0f / direction[axis];
        float t1 = (min[axis] - origin[axis]) * inv;
        float t2 = (max[axis] - origin[axis]) * inv;
        if (t1 > t2) std::swap(t1, t2);
        tMin = std::max(tMin, t1);
        tMax = std::min(tMax, t2);
        if (tMin > tMax) return false;
    }
    outDistance = tMin;
    return true;
}
