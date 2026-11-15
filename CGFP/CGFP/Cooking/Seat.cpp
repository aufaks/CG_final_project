#include "Cooking/Seat.h"

#include "Character/Customer.h"
#include "Character/Player.h"
#include "Cooking/Plate.h"
#include "Core/SoundManager.h"
#include "Order/OrderManager.h"
#include "Order/Recipe.h"
#include "Render/ResourceManager.h"

Seat::Seat(int id, OrderManager* orderManager) : id(id), orderManager(orderManager) {
    mesh = ResourceManager::Get().GetCube();
    transform.scale = glm::vec3(0.5f, 0.45f, 0.5f);
    color = glm::vec3(0.55f, 0.35f, 0.2f);
}

void Seat::OnInteract(Player& player) {
    if (!customer || customer->GetState() != CustomerState::WaitingForFood || !orderManager) return;
    auto* plate = dynamic_cast<Plate*>(player.GetHeldObject());
    if (!plate || plate->IsEmpty()) return;

    if (orderManager->TryServe(id, *plate)) {
        plate->ClearStack();
        SoundManager::PlaySFX("serve_ok");
    } else {
        // 틀린 음식은 가져가지 않고 피드백만 준다
        SoundManager::PlaySFX("serve_wrong");
    }
}

std::string Seat::GetPrompt(const Player& player) const {
    const std::string seatName = std::to_string(id + 1) + "번 자리";
    if (!customer || customer->GetState() != CustomerState::WaitingForFood) return seatName;

    const Order* order = orderManager ? orderManager->FindOrderBySeat(id) : nullptr;
    const std::string menu = (order && order->recipe) ? order->recipe->name : "?";
    if (dynamic_cast<Plate*>(player.GetHeldObject())) return "F: 서빙 (" + menu + ")";
    return seatName + " 주문: " + menu;
}
