#include "Object/Door.h"
#include "Character/Player.h"

Door::Door(const Collider& bounds, const glm::vec3& first, const glm::vec3& second)
    : pickingBounds(bounds), firstSide(first), secondSide(second) {}

void Door::OnInteract(Player& player) {
    const glm::vec3& position = player.transform.position;
    const float firstDistance = glm::length(position - firstSide);
    const float secondDistance = glm::length(position - secondSide);
    player.Teleport(firstDistance <= secondDistance ? secondSide : firstSide);
}
