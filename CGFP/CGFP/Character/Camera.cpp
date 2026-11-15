#include "Character/Camera.h"

#include <cmath>

void Camera::AddRotation(float deltaYaw, float deltaPitch) {
    yaw = std::fmod(yaw + deltaYaw, 360.0f);
    pitch = glm::clamp(pitch + deltaPitch, -kMaxPitch, kMaxPitch);
}

glm::vec3 Camera::GetForward() const {
    const float y = glm::radians(yaw);
    const float p = glm::radians(pitch);
    return glm::normalize(glm::vec3(std::cos(y) * std::cos(p), std::sin(p), std::sin(y) * std::cos(p)));
}

glm::vec3 Camera::GetFlatForward() const {
    const float y = glm::radians(yaw);
    return glm::vec3(std::cos(y), 0.0f, std::sin(y));
}

glm::vec3 Camera::GetRight() const {
    return glm::normalize(glm::cross(GetFlatForward(), glm::vec3(0, 1, 0)));
}

glm::mat4 Camera::GetViewMatrix() const {
    return glm::lookAt(position, GetTarget(), glm::vec3(0, 1, 0));
}

glm::mat4 Camera::GetProjectionMatrix(float aspect) const {
    return glm::perspective(glm::radians(fovDegrees), aspect, nearPlane, farPlane);
}
