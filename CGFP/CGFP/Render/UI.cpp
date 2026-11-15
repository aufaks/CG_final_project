#include "Render/UI.h"

#include "Character/Camera.h"
#include "Character/Customer.h"
#include "Core/InputManager.h"
#include "Order/DayManager.h"
#include "Order/OrderManager.h"
#include "Order/Recipe.h"
#include "Render/ResourceManager.h"

#include <sstream>

void UI::DrawScreen(const OrderManager& orders, const DayManager& day) {
    std::ostringstream title;
    title << "집게리아 | Day " << day.GetDay()
          << " | 남은 시간 " << static_cast<int>(day.GetTimeLeft()) << "초"
          << " | 매출 " << day.GetSales() << "/" << day.GetQuota();
    for (const Order& order : orders.GetActiveOrders()) {
        title << " | " << order.seatId + 1 << "번 " << (order.recipe ? order.recipe->name : "?");
    }
    if (!prompt.empty()) title << " | " << prompt;

    const std::string text = title.str();
    if (text != lastTitle && InputManager::GetWindow()) { // 바뀔 때만 갱신
        glfwSetWindowTitle(InputManager::GetWindow(), text.c_str());
        lastTitle = text;
    }
}

void UI::DrawCrosshair(ShaderManager& shaders, const Camera& camera) {
    // 화면 중앙 조준점: 카메라 바로 앞에 아주 작은 큐브를 그린다(2D 렌더러가 생기면 교체).
    const glm::vec3 position = camera.position + camera.GetForward() * 0.15f;
    const glm::mat4 model = glm::scale(glm::translate(glm::mat4(1.0f), position), glm::vec3(0.0015f));
    shaders.SetMat4("uModel", model);
    shaders.SetVec3("uColor", glm::vec3(1.0f));
    shaders.SetBool("uUseTexture", false);
    ResourceManager::Get().GetCube()->Draw();
}

void UI::DrawSpeechBubble(ShaderManager& shaders, const Customer& customer) {
    const glm::vec3 position = customer.transform.position + glm::vec3(0.0f, customer.transform.scale.y * 0.5f + 0.35f, 0.0f);
    const glm::mat4 model = glm::scale(glm::translate(glm::mat4(1.0f), position), glm::vec3(0.35f, 0.25f, 0.05f));
    shaders.SetMat4("uModel", model);
    shaders.SetVec3("uColor", glm::vec3(1.0f));
    shaders.SetBool("uUseTexture", false);
    ResourceManager::Get().GetCube()->Draw();
}
