#include "Object/InteractableObject.h"

#include "Character/Player.h"

std::string InteractableObject::GetPrompt(const Player& player) const {
    if (CanPickUp() && !player.IsHolding()) return "E: 들기";
    return {};
}
