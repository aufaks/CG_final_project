#include "Scene/PlayScene.h"

#include "Core/GL.h"
#include "Core/InputManager.h"
#include "Object/Door.h"
#include "Object/PickupObject.h"
#include "Object/Ray.h"
#include "Render/ResourceManager.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <vector>

namespace {
const char* kMapPath = "obj/imported/krusty-krab-interior/Krusty_Krab_Interior.obj";
const char* kSpatulaPath = "obj/imported/spatula-spongebob/SpatulaSpongebob.obj";
const char* kPlayerModelPath = "obj/imported/spongebob-squarepants/spongebob_bind.obj";
const char* kPlayerTexturePath = "obj/imported/spongebob-squarepants/textures/spongebob.png";
// 뼈대 애니메이션 모델 (Idle·Walk 클립). 없으면 위 OBJ로 그린다
const char* kPlayerGltfPath = "obj/mixamo/anim/SpongeBob.gltf";

// ObjectViewer와 같은 배치: 가장 긴 변을 2로 맞춘 뒤 600배
constexpr float kMapDisplayScale = 600.0f;
// 주방(계산대 뒤, x -2597~-2557, z 2601~2630) 그릴 앞에서 그릴과 주문 창구(-Z)를 바라보는 시작 위치 (OBJ 원본 좌표)
const glm::vec3 kSpawnFeetInObj(-2576.7f, -125.5f, 2609.0f);
constexpr float kSpawnYaw = -90.0f;
constexpr float kPlayerHeight = 1.0f;
constexpr float kInteractionDistance = 1.5f;

// 징징이: 계산대(배 모양 Boat1Mtl, "ORDER HERE" 간판 아래) 가운데에서 정문(-Z) 쪽 손님을 바라본다
// 모델·뼈대·애니메이션 클립은 tools/squidward_to_gltf.py가 squidwards-house/Mesh_0037.rip.obj에서 만든다
const char* kSquidwardGltfPath = "obj/mixamo/anim/Squidward.gltf";
const glm::vec3 kSquidwardFeetInObj(-2576.7f, -125.0f, 2595.4f);
constexpr float kSquidwardYaw = 180.0f; // 모델 정면(코) +Z → -Z
constexpr float kSquidwardHeight = 1.6f;

// 손님 동선 (OBJ 원본 좌표). 바닥(초록 판자) 높이 -125.5, 정문은 -Z 쪽(z 2552~2555), 계산대(배) 앞면 z 2590.
constexpr float kFloorY = -125.5f;
const glm::vec3 kEntranceInObj(-2576.7f, kFloorY, 2557.5f);     // 정문 바로 안쪽
const glm::vec3 kCounterFrontInObj(-2576.7f, kFloorY, 2589.6f); // 징징이 정면, 계산대 앞면 바로 앞
// 통 의자(Barrel1Mtl) 윗면. 바닥 -125.48 ~ 윗면 -122.45, 반지름 1.6.
// (-121.35는 테이블 윗면 높이라 쓰면 손님이 통 위로 떠 버린다)
constexpr float kSeatTopY = -122.45f;
constexpr float kSeatRadius = 1.6f; // 통 반지름 (OBJ)

// 테이블 6개 (Kktable1Mtl), 테이블마다 양쪽에 통 의자(Barrel1Mtl) 2개. 손님은 빈 테이블의 두 의자 중 하나에 랜덤으로 앉는다.
// 경유점은 계산대 줄(x -2576.7) 양옆으로 빠져서 다른 테이블을 피해 가고, 안쪽 의자는 테이블을 돌아서 간다.
struct SeatInObj {
    glm::vec2 seat;  // 통 의자 중심 (x, z)
    std::vector<glm::vec2> path;
};
struct TableInObj {
    glm::vec2 table; // 테이블 중심 (의자가 바라보는 방향)
    SeatInObj seats[2];
};
const glm::vec2 kLeftAisle(-2584.0f, 2587.0f);  // 계산대 왼쪽(-X)으로 빠지는 지점
const glm::vec2 kRightAisle(-2569.5f, 2587.0f); // 계산대 오른쪽(+X)으로 빠지는 지점
const std::vector<TableInObj> kTablesInObj = {
    { { -2606.3f, 2568.55f }, { { { -2601.6f, 2567.0f }, { kLeftAisle, { -2585.0f, 2566.0f } } },
                                { { -2610.7f, 2570.6f }, { kLeftAisle, { -2585.0f, 2563.0f }, { -2610.7f, 2563.5f } } } } },
    { { -2604.5f, 2586.9f }, { { { -2599.7f, 2587.9f }, { kLeftAisle } },
                               { { -2609.25f, 2586.55f }, { kLeftAisle, { -2600.0f, 2581.5f }, { -2611.5f, 2581.5f } } } } },
    { { -2592.8f, 2575.85f }, { { { -2592.05f, 2580.6f }, { kLeftAisle } },
                                { { -2593.1f, 2570.9f }, { kLeftAisle, { -2586.5f, 2571.0f } } } } },
    { { -2560.75f, 2575.1f }, { { { -2560.4f, 2580.1f }, { kRightAisle } },
                                { { -2561.45f, 2570.4f }, { kRightAisle, { -2566.5f, 2571.0f } } } } },
    { { -2549.1f, 2567.9f }, { { { -2553.9f, 2566.95f }, { kRightAisle, { -2568.0f, 2566.0f } } },
                               { { -2544.4f, 2568.35f }, { kRightAisle, { -2568.0f, 2563.0f }, { -2544.4f, 2562.5f } } } } },
    { { -2546.95f, 2585.8f }, { { { -2551.6f, 2587.4f }, { kRightAisle } },
                                { { -2542.5f, 2583.75f }, { kRightAisle, { -2550.0f, 2580.5f }, { -2542.0f, 2579.5f } } } } },
};

// 주문표 UI (1280x720 기준 픽셀, 화면 높이 비율로 늘린다): 화면 위쪽에 하얀 종이가 왼쪽부터 쌓인다
constexpr float kUIReferenceHeight = 720.0f;
const glm::vec4 kTicketRect(24.0f, 16.0f, 96.0f, 128.0f); // 첫 장 x, y, w, h
constexpr float kTicketSpacing = 12.0f;
constexpr float kTicketDropSeconds = 0.35f; // 위에서 내려오는 시간
const glm::vec4 kTicketColor(1.0f, 1.0f, 1.0f, 1.0f);
const glm::vec4 kTicketShadowColor(0.0f, 0.0f, 0.0f, 0.35f);

const glm::vec3 kClearColor(0.12f, 0.27f, 0.39f);
const glm::vec3 kLightDirection(-0.5f, -1.0f, -0.6f);
} // namespace

void PlayScene::Enter() {
    player.camera.fovDegrees = 55.0f;
    player.camera.farPlane = 250.0f;
    if (!player.LoadSkinnedModel(kPlayerGltfPath, kPlayerHeight) &&
        !player.LoadModel(kPlayerModelPath, kPlayerTexturePath, kPlayerHeight)) {
        std::cerr << "[PlayScene] 플레이어 모델을 불러오지 못했습니다.\n";
    }
    squidwardLoaded = squidward.LoadModel(kSquidwardGltfPath, kSquidwardHeight);
    if (!squidwardLoaded) std::cerr << "[PlayScene] 징징이 모델을 불러오지 못했습니다.\n";
    // 징징이가 영수증을 주방 창구에 넣으면 주문표가 화면에 뜬다
    squidward.onReceiptSubmitted = [this](Order& order) { orderManager.Submit(order); };
    customersLoaded = customerManager.LoadModels();
    if (!customersLoaded) std::cerr << "[PlayScene] 손님 모델을 하나도 불러오지 못했습니다.\n";
    ui.Init();
    InputManager::SetCursorCaptured(true);
}

void PlayScene::Exit() {
    InputManager::SetCursorCaptured(false);
    focusedObject = nullptr;
    spatulaPickup = nullptr;
    player.heldObject = nullptr;
    interactables.clear();
    customerManager.customers.clear(); // 손님 메시(GL 버퍼)는 컨텍스트가 살아 있을 때 지운다
    squidward.Unload();
    squidwardLoaded = false;
    ui.Release();
    spatula.reset();
    map.reset();
}

glm::vec3 PlayScene::MapToWorld(const glm::vec3& objPosition) const {
    return (objPosition - mapCenter) * mapScale;
}

void PlayScene::SetupCustomers() {
    CustomerLayout layout;
    layout.entrance = MapToWorld(kEntranceInObj);
    layout.counterFront = MapToWorld(kCounterFrontInObj);
    layout.queueDirection = glm::normalize(layout.entrance - layout.counterFront);

    std::vector<Seat> seats;
    for (size_t i = 0; i < kTablesInObj.size(); ++i) {
        const TableInObj& table = kTablesInObj[i];
        for (const SeatInObj& def : table.seats) {
            Seat seat;
            seat.number = static_cast<int>(i) + 1;
            seat.transform.position = MapToWorld(glm::vec3(def.seat.x, kFloorY, def.seat.y));
            const glm::vec2 toTable = table.table - def.seat;
            seat.transform.rotation.y = glm::degrees(std::atan2(toTable.x, toTable.y));
            seat.seatHeight = (kSeatTopY - kFloorY) * mapScale;
            seat.seatRadius = kSeatRadius * mapScale;
            for (const glm::vec2& point : def.path)
                seat.approachPath.push_back(MapToWorld(glm::vec3(point.x, kFloorY, point.y)));
            seats.push_back(std::move(seat));
        }
    }
    customerManager.Setup(layout, std::move(seats), squidwardLoaded ? &squidward : nullptr, &orderManager);
}

void PlayScene::LoadMap() {
    const double start = glfwGetTime();
    map = std::make_unique<Model>();
    if (!map->LoadObj(kMapPath)) {
        map.reset();
        return;
    }

    const glm::vec3 boundsMin = map->GetBoundsMin();
    const glm::vec3 boundsMax = map->GetBoundsMax();
    const glm::vec3 center = (boundsMin + boundsMax) * 0.5f;
    const glm::vec3 size = boundsMax - boundsMin;
    const float largest = std::max({ size.x, size.y, size.z });
    const float scale = (largest > 0.0f ? 2.0f / largest : 1.0f) * kMapDisplayScale;

    mapMatrix = glm::scale(glm::mat4(1.0f), glm::vec3(scale)) * glm::translate(glm::mat4(1.0f), -center);
    mapCenter = center;
    mapScale = scale;

    spatula = std::make_unique<Model>();
    if (spatula->LoadObj(kSpatulaPath)) {
        // 기존 포크 자리에서 벽면과 평행하게 놓고, 손잡이가 위로 오도록 뒤집어 건다.
        const glm::vec3 rackHookInObj(-2559.415f, -113.885f, 2616.975f);
        spatulaMatrix = mapMatrix *
            glm::translate(glm::mat4(1.0f), rackHookInObj) *
            glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f)) *
            glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(0.0f, 0.0f, 1.0f)) *
            glm::scale(glm::mat4(1.0f), glm::vec3(7.5f));

    } else {
        std::cerr << "[PlayScene] 뒤집개 모델을 불러오지 못했습니다.\n";
        spatula.reset();
    }

    player.Spawn(MapToWorld(kSpawnFeetInObj), kSpawnYaw);
    focusedObject = nullptr;
    interactables.clear();
    spatulaPickup = nullptr;
    if (spatula) {
        Collider pickupBounds = Collider::FromLocalBounds(
            spatula->GetBoundsMin(), spatula->GetBoundsMax(), spatulaMatrix);
        // 얇은 뒤집개 메시보다 넓은 판정 상자를 사용해 화면 중앙으로 쉽게 조준할 수 있게 한다.
        pickupBounds.min -= glm::vec3(0.01f, 0.06f, 0.01f);
        pickupBounds.max += glm::vec3(0.01f, 0.06f, 0.01f);
        auto pickup = std::make_unique<PickupObject>(pickupBounds);
        spatulaPickup = pickup.get();
        interactables.push_back(std::move(pickup));
    }
    // z=2601 벽에 있는 주방 출입문. 보이는 문짝은 맵에 포함되어 있다.
    // 이 충돌 박스는 출입문만 감싸고, 두 좌표는 양쪽의 안전한 도착 위치다.
    interactables.push_back(std::make_unique<Door>(
        Collider(MapToWorld(glm::vec3(-2593.2f, -125.6f, 2600.2f)),
                 MapToWorld(glm::vec3(-2586.9f, -113.8f, 2601.7f))),
        MapToWorld(glm::vec3(-2590.0f, kFloorY, 2597.8f)),
        MapToWorld(glm::vec3(-2590.0f, kFloorY, 2603.8f)),
        map->GetKitchenHallDoorMesh()));
    // z=2630 벽에 있는 주방과 창고 사이 출입문.
    interactables.push_back(std::make_unique<Door>(
        Collider(MapToWorld(glm::vec3(-2585.2f, -125.6f, 2628.6f)),
                 MapToWorld(glm::vec3(-2576.3f, -114.4f, 2631.1f))),
        MapToWorld(glm::vec3(-2580.8f, kFloorY, 2626.6f)),
        MapToWorld(glm::vec3(-2580.8f, kFloorY, 2633.2f)),
        map->GetKitchenStorageDoorMesh()));
    squidward.Place(MapToWorld(kSquidwardFeetInObj), kSquidwardYaw);
    if (customersLoaded) SetupCustomers();

    std::cout << "[PlayScene] 맵 로드 " << (glfwGetTime() - start) << "초" << std::endl;
}

void PlayScene::Update(float deltaTime) {
    if (!loadRequested) {
        loadRequested = true;
        return;
    }
    if (!loadAttempted) { // 실패해도 매 프레임 다시 읽지 않는다
        loadAttempted = true;
        LoadMap();
    }

    if (InputManager::GetKeyDown(GLFW_KEY_P)) {
        InputManager::SetCursorCaptured(!InputManager::IsCursorCaptured());
    }
    // 테스트: 음식 서빙이 생기기 전까지 F 키로 앉아 있는 손님에게 밥을 준다
    if (InputManager::GetKeyDown(GLFW_KEY_F)) customerManager.StartEatingAll();
    player.Update(deltaTime);
    focusedObject = map && InputManager::IsCursorCaptured()
        ? FindInteractionTarget(Ray::FromCamera(player.camera)) : nullptr;
    if (InputManager::GetKeyDown(GLFW_KEY_E)) {
        if (player.heldObject) {
            DropHeldSpatula();
            focusedObject = nullptr;
        } else if (focusedObject && focusedObject->CanPickUp()) {
            focusedObject->OnInteract(player);
        }
    }
    if (focusedObject && !focusedObject->CanPickUp() && InputManager::GetKeyDown(GLFW_KEY_F)) {
        focusedObject->OnInteract(player);
    }
    if (spatulaPickup && player.heldObject == spatulaPickup) {
        // 마인크래프트 도구처럼 화면 오른쪽 아래에서 중앙 위쪽으로 기울여 배치한다.
        const glm::vec3 forward = player.camera.GetForward();
        const glm::vec3 right = player.camera.GetRight();
        const glm::vec3 up = glm::normalize(glm::cross(right, forward));
        const glm::vec3 position = player.camera.position + forward * 0.55f + right * 0.4f - up * 0.42f;
        spatulaMatrix = glm::mat4(1.0f);
        spatulaMatrix[0] = glm::vec4(right, 0.0f);
        spatulaMatrix[1] = glm::vec4(up, 0.0f);
        spatulaMatrix[2] = glm::vec4(-forward, 0.0f);
        spatulaMatrix[3] = glm::vec4(position, 1.0f);
        spatulaMatrix = spatulaMatrix *
            glm::rotate(glm::mat4(1.0f), glm::radians(25.0f), glm::vec3(0.0f, 0.0f, 1.0f)) *
            glm::rotate(glm::mat4(1.0f), glm::radians(-50.0f), glm::vec3(1.0f, 0.0f, 0.0f)) *
            glm::scale(glm::mat4(1.0f), glm::vec3(1.0f));
    }
    orderManager.Update(deltaTime);
    if (map) customerManager.Update(deltaTime);
    if (squidwardLoaded) squidward.Update(deltaTime);
}

void PlayScene::DropHeldSpatula() {
    if (!spatula || !spatulaPickup || player.heldObject != spatulaPickup) return;

    // 플레이어 앞 바닥에 뒤집개를 눕히고, 새 위치로 피킹 판정을 옮긴다.
    const glm::vec3 forward = player.camera.GetFlatForward();
    const glm::vec3 right = player.camera.GetRight();
    const glm::vec3 up(0.0f, 1.0f, 0.0f);
    const glm::vec3 position = player.transform.position + forward * 0.85f + up * 0.02f;
    spatulaMatrix = glm::mat4(1.0f);
    spatulaMatrix[0] = glm::vec4(right, 0.0f);
    spatulaMatrix[1] = glm::vec4(forward, 0.0f);
    spatulaMatrix[2] = glm::vec4(up, 0.0f);
    spatulaMatrix[3] = glm::vec4(position, 1.0f);

    Collider pickupBounds = Collider::FromLocalBounds(
        spatula->GetBoundsMin(), spatula->GetBoundsMax(), spatulaMatrix);
    pickupBounds.min -= glm::vec3(0.30f, 0.18f, 0.25f);
    pickupBounds.max += glm::vec3(0.30f, 0.18f, 0.25f);
    spatulaPickup->SetPickingBounds(pickupBounds);
    player.heldObject = nullptr;
}

InteractableObject* PlayScene::FindInteractionTarget(const Ray& ray) const {
    float nearest = kInteractionDistance;
    InteractableObject* target = nullptr;
    for (const auto& object : interactables) {
        if (object->IsDestroyed()) continue;
        if (object.get() == player.heldObject) continue;
        float distance = 0.0f;
        if (object->GetWorldCollider().IntersectsRay(ray.origin, ray.direction, nearest, distance)) {
            nearest = distance;
            target = object.get();
        }
    }
    return target;
}

void PlayScene::Render() {
    glClearColor(kClearColor.r, kClearColor.g, kClearColor.b, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    if (!map) return;

    int width = 0;
    int height = 0;
    glfwGetFramebufferSize(glfwGetCurrentContext(), &width, &height);
    if (width <= 0 || height <= 0) return;

    const Camera& camera = player.camera;
    ShaderManager& shaders = ResourceManager::Get().Shaders();
    const glm::mat4 projection = camera.GetProjectionMatrix(static_cast<float>(width) / static_cast<float>(height));
    const glm::mat4 view = camera.GetViewMatrix();
    auto useShader = [&](const char* name) {
        shaders.Use(name);
        shaders.SetMat4("uProjection", projection);
        shaders.SetMat4("uView", view);
        shaders.SetVec3("uLightDirection", glm::normalize(kLightDirection));
        shaders.SetBool("uReferenceLighting", true);
        shaders.SetInt("uTexture", 0);
    };

    // 뼈대 애니메이션 캐릭터(손님·플레이어·징징이)는 GPU 스키닝 셰이더로 그린다
    useShader("skinned");
    customerManager.Render(shaders);
    player.RenderSkinned(shaders);
    if (squidwardLoaded) squidward.Render(shaders);

    useShader("map");
    player.Render(shaders); // glTF 모델을 쓰면 아무것도 안 그린다
    if (squidwardLoaded) squidward.RenderProps(shaders); // 손에 든 영수증
    map->Draw(shaders, mapMatrix, camera.position);
    const bool holdingSpatula = spatula && spatulaPickup && player.heldObject == spatulaPickup;
    if (spatula && !holdingSpatula) spatula->Draw(shaders, spatulaMatrix, camera.position);

    const Door* focusedDoor = dynamic_cast<Door*>(focusedObject);
    const Mesh* doorMesh = focusedDoor ? focusedDoor->GetOutlineMesh() : nullptr;
    const bool outlineSpatula = spatula && spatulaPickup && focusedObject == spatulaPickup;
    if (doorMesh || outlineSpatula) {
        shaders.Use("outline");
        shaders.SetMat4("uProjection", projection);
        shaders.SetMat4("uView", view);
        const glm::mat4& outlinedMatrix = outlineSpatula ? spatulaMatrix : mapMatrix;
        shaders.SetMat4("uModel", outlinedMatrix);

        glEnable(GL_STENCIL_TEST);
        glStencilMask(0xFF);
        glStencilFunc(GL_ALWAYS, 1, 0xFF);
        glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
        glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
        glDepthMask(GL_FALSE);
        glDepthFunc(GL_LEQUAL);
        if (outlineSpatula) spatula->DrawGeometry();
        else doorMesh->Draw(); // 원래 형상에서 화면에 보이는 픽셀만 스텐실에 표시

        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glStencilFunc(GL_NOTEQUAL, 1, 0xFF);
        glStencilMask(0x00);
        glDisable(GL_DEPTH_TEST);
        const glm::vec3 boundsMin = outlineSpatula ? spatula->GetBoundsMin() : doorMesh->GetBoundsMin();
        const glm::vec3 boundsMax = outlineSpatula ? spatula->GetBoundsMax() : doorMesh->GetBoundsMax();
        const glm::vec3 center = (boundsMin + boundsMax) * 0.5f;
        const glm::mat4 expanded = outlinedMatrix * glm::translate(glm::mat4(1.0f), center) *
            glm::scale(glm::mat4(1.0f), glm::vec3(1.08f, 1.05f, 1.08f)) *
            glm::translate(glm::mat4(1.0f), -center);
        shaders.SetMat4("uModel", expanded);
        if (outlineSpatula) spatula->DrawGeometry();
        else doorMesh->Draw();

        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);
        glDepthMask(GL_TRUE);
        glStencilMask(0xFF);
        glDisable(GL_STENCIL_TEST);
    }

    if (holdingSpatula) {
        // 월드 깊이와 분리해 벽 가까이에서도 1인칭 도구가 가려지지 않게 한다.
        glClear(GL_DEPTH_BUFFER_BIT);
        useShader("map");
        spatula->Draw(shaders, spatulaMatrix, camera.position);
    }

    DrawOrderTickets(width, height);
    if (player.IsFirstPerson()) DrawCrosshair(width, height);
}

void PlayScene::DrawCrosshair(int width, int height) {
    const float scale = static_cast<float>(height) / kUIReferenceHeight;
    const float centerX = static_cast<float>(width) * 0.5f;
    const float centerY = static_cast<float>(height) * 0.5f;
    const float outerLength = 18.0f * scale;
    const float outerThickness = 4.0f * scale;
    const float innerLength = 14.0f * scale;
    const float innerThickness = 2.0f * scale;

    ui.Begin(width, height);
    // 밝은 벽에서도 보이도록 검은 테두리 위에 흰 십자를 겹쳐 그린다.
    ui.DrawRect(glm::vec4(centerX - outerLength * 0.5f, centerY - outerThickness * 0.5f,
                          outerLength, outerThickness), glm::vec4(0.0f, 0.0f, 0.0f, 0.75f));
    ui.DrawRect(glm::vec4(centerX - outerThickness * 0.5f, centerY - outerLength * 0.5f,
                          outerThickness, outerLength), glm::vec4(0.0f, 0.0f, 0.0f, 0.75f));
    ui.DrawRect(glm::vec4(centerX - innerLength * 0.5f, centerY - innerThickness * 0.5f,
                          innerLength, innerThickness), glm::vec4(1.0f));
    ui.DrawRect(glm::vec4(centerX - innerThickness * 0.5f, centerY - innerLength * 0.5f,
                          innerThickness, innerLength), glm::vec4(1.0f));
    ui.End();
}

void PlayScene::DrawOrderTickets(int width, int height) {
    // 주문표 시스템은 아직 미정: 창구에 들어간 주문마다 하얀 종이 한 장을 화면 위에 띄운다
    const float scale = static_cast<float>(height) / kUIReferenceHeight;
    const float now = orderManager.GetTime();
    ui.Begin(width, height);
    int index = 0;
    for (const auto& order : orderManager.orders) {
        if (order->state != OrderState::Submitted) continue;
        const float t = glm::clamp((now - order->submittedTime) / kTicketDropSeconds, 0.0f, 1.0f);
        const float ease = 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t);
        glm::vec4 rect = kTicketRect * scale;
        rect.x += (kTicketRect.z + kTicketSpacing) * scale * static_cast<float>(index);
        rect.y = -rect.w + (rect.y + rect.w) * ease; // 화면 위에서 내려온다
        ui.DrawRect(rect + glm::vec4(4.0f, 4.0f, 0.0f, 0.0f) * scale, kTicketShadowColor);
        ui.DrawRect(rect, kTicketColor);
        ++index;
    }
    ui.End();
}
