#pragma once
#include "Core/GL.h"

// 1인칭 시점. 플레이어 눈 위치와 yaw/pitch로 바라보는 방향(AT)을 계산한다.
// pitch를 ±89°로 제한해서 위아래로 넘어가 화면이 뒤집히는 문제를 막는다.
class Camera {
public:
    static constexpr float kMaxPitch = 89.0f;

    glm::vec3 position{ 0.0f, 1.6f, 0.0f };
    float yaw = -90.0f; // 0° = +X, 90° = +Z, -90° = -Z
    float pitch = 0.0f;
    float fovDegrees = 60.0f;
    float nearPlane = 0.05f;
    float farPlane = 200.0f;

    void AddRotation(float deltaYaw, float deltaPitch);

    glm::vec3 GetForward() const;     // 바라보는 방향(위아래 포함)
    glm::vec3 GetFlatForward() const; // 수평 전방(이동용)
    glm::vec3 GetRight() const;       // 수평 오른쪽(이동용)
    glm::vec3 GetTarget() const { return position + GetForward(); }

    glm::mat4 GetViewMatrix() const;
    glm::mat4 GetProjectionMatrix(float aspect) const;
};
