#include "Character/Customer.h"

#include "Render/ResourceManager.h"

#include <cmath>
#include <utility>

Customer::Customer(int id) : id(id) {
    // TODO(3단계): 물고기 손님 OBJ(obj/imported/sbfbbr-fish-*)로 교체
    mesh = ResourceManager::Get().GetCube();
    transform.scale = glm::vec3(0.5f, 1.6f, 0.5f);
    transform.position.y = transform.scale.y * 0.5f;
    const float hue = static_cast<float>(id % 5) / 5.0f;
    color = glm::vec3(0.4f + 0.5f * hue, 0.6f, 0.9f - 0.5f * hue);
}

void Customer::SetState(CustomerState next) {
    state = next;
    stateTime = 0.0f;
}

void Customer::SetPath(std::vector<glm::vec3> waypoints) {
    path = std::move(waypoints);
    pathIndex = 0;
}

void Customer::OnServed() {
    SetState(CustomerState::Eating);
}

void Customer::Update(float deltaTime) {
    stateTime += deltaTime;
    if (HasArrived()) return;

    const glm::vec3 target(path[pathIndex].x, transform.position.y, path[pathIndex].z);
    const glm::vec3 toTarget = target - transform.position;
    const float distance = glm::length(toTarget);
    const float step = moveSpeed * deltaTime;
    if (distance <= step) {
        transform.position = target;
        ++pathIndex;
        return;
    }
    const glm::vec3 direction = toTarget / distance;
    transform.position += direction * step;
    transform.rotation.y = glm::degrees(std::atan2(direction.x, direction.z)); // 가는 방향을 바라보게
}
