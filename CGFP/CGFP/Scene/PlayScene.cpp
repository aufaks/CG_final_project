#include "Scene/PlayScene.h"

#include "Core/InputManager.h"
#include "Core/SoundManager.h"
#include "Object/Ray.h"
#include "Render/ResourceManager.h"
#include "Scene/EndingScene.h"
#include "Scene/SceneManager.h"

#include <algorithm>

void PlayScene::Enter() {
    InputManager::SetCursorCaptured(true);
    BuildMap();

    orderManager.SetDayManager(&dayManager);
    CustomerWaypoints waypoints;
    waypoints.entrance = glm::vec3(1.0f, 0.0f, 7.0f);
    waypoints.aisle = glm::vec3(1.0f, 0.0f, 0.0f);
    waypoints.counter = glm::vec3(-1.0f, 0.0f, -1.2f);
    waypoints.exit = glm::vec3(1.0f, 0.0f, 7.5f);
    customerManager.Init(seats, waypoints, &orderManager);

    dayManager.StartDay(orderManager.GetRecipes());
    SoundManager::PlayBGM("day");
}

void PlayScene::Exit() {
    InputManager::SetCursorCaptured(false);
}

// ---------------------------------------------------------------------------------------------
// 맵 (단위: m). 주방은 z < -2, 홀은 z > -2. 카운터 오른쪽(x > 3)으로 홀에 나갈 수 있다.
// ---------------------------------------------------------------------------------------------

StaticObject* PlayScene::SpawnBox(const glm::vec3& center, const glm::vec3& size, const glm::vec3& color, bool solid) {
    StaticObject* box = Spawn<StaticObject>();
    box->mesh = ResourceManager::Get().GetCube();
    box->transform.position = center;
    box->transform.scale = size;
    box->color = color;
    box->solid = solid;
    return box;
}

void PlayScene::BuildMap() {
    const glm::vec3 wallColor(0.55f, 0.75f, 0.8f);
    SpawnBox({ 0.0f, -0.05f, 0.0f }, { 16.0f, 0.1f, 16.0f }, { 0.6f, 0.5f, 0.4f }, false); // 바닥
    SpawnBox({ 0.0f, 1.5f, -8.0f }, { 16.0f, 3.0f, 0.2f }, wallColor);
    SpawnBox({ 0.0f, 1.5f, 8.0f }, { 16.0f, 3.0f, 0.2f }, wallColor);
    SpawnBox({ -8.0f, 1.5f, 0.0f }, { 0.2f, 3.0f, 16.0f }, wallColor);
    SpawnBox({ 8.0f, 1.5f, 0.0f }, { 0.2f, 3.0f, 16.0f }, wallColor);

    SpawnBox({ -1.0f, 0.5f, -2.0f }, { 8.0f, 1.0f, 0.6f }, { 0.75f, 0.45f, 0.25f }); // 계산대 (x -5 ~ 3)
    SpawnBox({ 0.0f, 0.5f, -6.0f }, { 8.0f, 1.0f, 1.0f }, { 0.7f, 0.7f, 0.72f });   // 주방 작업대

    // 재료 상자: 작업대 왼쪽에 한 줄
    const IngredientType boxTypes[] = { IngredientType::BottomBun, IngredientType::Patty, IngredientType::Cheese,
                                        IngredientType::Lettuce, IngredientType::Tomato, IngredientType::TopBun };
    float x = -3.5f;
    for (IngredientType type : boxTypes) {
        IngredientBox* box = Spawn<IngredientBox>(type, [this](IngredientType t) { return SpawnIngredient(t); });
        box->transform.position = { x, 1.0f + box->transform.scale.y * 0.5f, -6.0f };
        x += 0.7f;
    }

    Grill* grill = Spawn<Grill>();
    grill->transform.position = { 2.0f, 1.0f + grill->transform.scale.y * 0.5f, -6.0f };

    for (int i = 0; i < 2; ++i) { // 접시 두 개: 계산대 위
        Plate* plate = Spawn<Plate>();
        plate->transform.position = { -2.0f + i * 0.8f, 1.0f + plate->transform.scale.y * 0.5f, -2.0f };
    }

    // 테이블 6개 + 좌석
    const float tableX[] = { -5.0f, -1.0f, 3.0f };
    const float tableZ[] = { 2.0f, 5.0f };
    int seatId = 0;
    for (float tz : tableZ) {
        for (float tx : tableX) {
            SpawnBox({ tx, 0.375f, tz }, { 1.0f, 0.75f, 1.0f }, { 0.85f, 0.85f, 0.8f });
            Seat* seat = Spawn<Seat>(seatId++, &orderManager);
            seat->transform.position = { tx + 0.9f, seat->transform.scale.y * 0.5f, tz };
            seats.push_back(seat);
        }
    }

    player.SetObstacles(&statics);
    player.SetPosition({ 0.0f, 0.0f, -4.0f });
    player.GetCamera().yaw = 90.0f; // 계산대(+z) 쪽을 본다
}

Ingredient* PlayScene::SpawnIngredient(IngredientType type) {
    return Spawn<Ingredient>(type);
}

// ---------------------------------------------------------------------------------------------
// 업데이트
// ---------------------------------------------------------------------------------------------

void PlayScene::Update(float deltaTime) {
    if (InputManager::GetKeyDown(GLFW_KEY_ESCAPE)) InputManager::SetCursorCaptured(!InputManager::IsCursorCaptured());
    // TODO: 일시정지 메뉴. 지금은 Esc로 마우스만 풀어 준다

    dayManager.Update(deltaTime);
    player.Update(deltaTime);
    HandleInteraction();
    for (size_t i = 0; i < objects.size(); ++i) objects[i]->Update(deltaTime);
    orderManager.Update(deltaTime);
    customerManager.Update(deltaTime, dayManager.IsOpen());
    CheckDayEnd();
    RemoveDestroyedObjects();
}

void PlayScene::HandleInteraction() {
    const Ray ray = Ray::FromCamera(player.GetCamera(), kReach);
    aimTarget = ray.Pick(interactables);

    // 프롬프트 표시와 E/F 판정이 같은 aimTarget을 쓰게 한다
    std::string prompt = aimTarget ? aimTarget->GetPrompt(player) : "";
    if (prompt.empty() && player.IsHolding()) prompt = "E: 내려놓기";
    ui.SetPrompt(prompt);

    if (InputManager::GetKeyDown(GLFW_KEY_E)) {
        if (player.IsHolding()) {
            InteractableObject* held = player.GetHeldObject();
            if (aimTarget && aimTarget->TryPlace(*held)) player.Release();
            else DropHeldObject();
        } else if (aimTarget && aimTarget->CanPickUp()) {
            player.PickUp(aimTarget);
        }
    }

    if (InputManager::GetKeyDown(GLFW_KEY_F) && aimTarget) aimTarget->OnInteract(player);
}

void PlayScene::DropHeldObject() {
    const Ray ray = Ray::FromCamera(player.GetCamera(), kReach);
    float distance = 0.0f;
    Object* surface = ray.PickObject(statics, &distance);
    InteractableObject* held = player.Release();
    if (!held) return;

    const float halfHeight = held->transform.scale.y * 0.5f;
    if (surface) {
        const glm::vec3 hit = ray.GetPoint(distance);
        const Collider box = surface->GetWorldCollider();
        if (hit.y >= box.max.y - 0.01f) { // 윗면을 조준했으면 그 위에 놓는다
            held->transform.position = { hit.x, box.max.y + halfHeight, hit.z };
            held->transform.rotation = glm::vec3(0.0f);
            return;
        }
    }
    // 벽 옆면 등: 발 앞 바닥에 놓는다
    // TODO(2단계): 놓을 자리가 다른 물체와 겹치면 놓지 못하게 하기
    const glm::vec3 front = player.transform.position + player.GetCamera().GetFlatForward() * 0.6f;
    held->transform.position = { front.x, halfHeight, front.z };
    held->transform.rotation = glm::vec3(0.0f);
}

void PlayScene::CheckDayEnd() {
    if (sceneFinished) return;

    // 판정 1: 영업 시간 종료 → 새 손님을 막고 남은 손님을 내보낸다
    if (dayManager.IsBusinessTimeOver() && !dayEnding) {
        dayEnding = true;
        customerManager.SendEveryoneHome();
    }
    if (!dayEnding || !customerManager.AllGone()) return;

    // 판정 2: 할당량
    if (!dayManager.IsQuotaMet() || dayManager.IsLastDay()) {
        sceneFinished = true;
        sceneManager->ChangeScene(std::make_unique<EndingScene>(dayManager.IsQuotaMet(), dayManager.GetDay()));
        return;
    }

    // TODO(4단계): 집게사장 정산 컷신(Cutscene 재사용) 후 다음 날로
    dayManager.NextDay();
    orderManager.Clear();
    dayManager.StartDay(orderManager.GetRecipes());
    dayEnding = false;
}

void PlayScene::RemoveDestroyedObjects() {
    auto destroyed = [](const Object* o) { return o->IsDestroyed(); };
    interactables.erase(std::remove_if(interactables.begin(), interactables.end(), destroyed), interactables.end());
    statics.erase(std::remove_if(statics.begin(), statics.end(), destroyed), statics.end());
    objects.erase(std::remove_if(objects.begin(), objects.end(),
        [](const std::unique_ptr<Object>& o) { return o->IsDestroyed(); }), objects.end());
}

// ---------------------------------------------------------------------------------------------
// 렌더
// ---------------------------------------------------------------------------------------------

void PlayScene::Render() {
    int width = 0, height = 0;
    glfwGetFramebufferSize(InputManager::GetWindow(), &width, &height);
    if (width == 0 || height == 0) return; // 최소화
    glViewport(0, 0, width, height);
    glClearColor(0.45f, 0.7f, 0.85f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    const Camera& camera = player.GetCamera();
    ShaderManager& shaders = ResourceManager::Get().Shaders();
    shaders.Use("basic");
    shaders.SetMat4("uView", camera.GetViewMatrix());
    shaders.SetMat4("uProjection", camera.GetProjectionMatrix(static_cast<float>(width) / height));
    shaders.SetVec3("uLightDirection", glm::normalize(glm::vec3(-0.4f, -1.0f, -0.3f)));
    shaders.SetInt("uTexture", 0);

    for (const auto& object : objects) object->Render(shaders);
    customerManager.Render(shaders);
    for (const auto& customer : customerManager.GetCustomers()) {
        if (customer->GetState() == CustomerState::WaitingForFood) ui.DrawSpeechBubble(shaders, *customer);
    }

    ui.DrawCrosshair(shaders, camera);
    ui.DrawScreen(orderManager, dayManager);
}
