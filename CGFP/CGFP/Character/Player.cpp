#include "Character/Player.h"

#include "Core/InputManager.h"
#include "Object/InteractableObject.h"

Player::Player() {
    camera.position = transform.position + glm::vec3(0.0f, eyeHeight, 0.0f);
}

void Player::SetPosition(const glm::vec3& feet) {
    transform.position = feet;
    camera.position = feet + glm::vec3(0.0f, eyeHeight, 0.0f);
}

void Player::Update(float deltaTime) {
    if (InputManager::IsCursorCaptured()) {
        const glm::vec2 mouse = InputManager::GetMouseDelta();
        camera.AddRotation(mouse.x * mouseSensitivity, -mouse.y * mouseSensitivity);
    }

    glm::vec3 move(0.0f);
    if (InputManager::GetKey(GLFW_KEY_W)) move += camera.GetFlatForward();
    if (InputManager::GetKey(GLFW_KEY_S)) move -= camera.GetFlatForward();
    if (InputManager::GetKey(GLFW_KEY_D)) move += camera.GetRight();
    if (InputManager::GetKey(GLFW_KEY_A)) move -= camera.GetRight();
    if (glm::length(move) > 0.0f) {
        move = glm::normalize(move) * moveSpeed * deltaTime; // 대각선도 같은 속도
        TryMove(glm::vec3(move.x, 0.0f, 0.0f));
        TryMove(glm::vec3(0.0f, 0.0f, move.z));
    }

    camera.position = transform.position + glm::vec3(0.0f, eyeHeight, 0.0f);
    UpdateHeldObject();
}

Collider Player::ColliderAt(const glm::vec3& feet) const {
    // 발 바로 위부터 잡아서 바닥과는 겹치지 않게 한다
    return Collider(feet + glm::vec3(-radius, 0.05f, -radius), feet + glm::vec3(radius, height, radius));
}

Collider Player::GetWorldCollider() const {
    return ColliderAt(transform.position);
}

bool Player::CollidesAt(const glm::vec3& feet) const {
    if (!obstacles) return false;
    const Collider body = ColliderAt(feet);
    for (const Object* object : *obstacles) {
        if (object && object->solid && body.Intersects(object->GetWorldCollider())) return true;
    }
    return false;
}

void Player::TryMove(const glm::vec3& delta) {
    const glm::vec3 next = transform.position + delta;
    if (!CollidesAt(next)) transform.position = next;
}

void Player::PickUp(InteractableObject* object) {
    if (!object || heldObject) return;
    heldObject = object;
    heldObject->SetHeld(true);
    UpdateHeldObject();
}

InteractableObject* Player::Release() {
    InteractableObject* object = heldObject;
    if (object) object->SetHeld(false);
    heldObject = nullptr;
    return object;
}

void Player::UpdateHeldObject() {
    if (!heldObject) return;
    // 화면 오른쪽 아래 앞쪽에 들고 있는 것처럼 보이게 둔다
    heldObject->transform.position = camera.position + camera.GetForward() * 0.7f + camera.GetRight() * 0.25f - glm::vec3(0.0f, 0.25f, 0.0f);
    heldObject->transform.rotation = glm::vec3(0.0f, -camera.yaw - 90.0f, 0.0f);
}
