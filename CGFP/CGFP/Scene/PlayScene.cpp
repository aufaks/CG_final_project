#include "Scene/PlayScene.h"

#include "Core/GL.h"
#include "Core/InputManager.h"
#include "Render/ResourceManager.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <vector>

namespace {
const char* kMapPath = "obj/imported/krusty-krab-interior/Krusty_Krab_Interior.obj";
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

// 테이블 6개 (Kktable1Mtl), 테이블마다 통 의자 2개 중 통로 쪽 하나에 앉는다.
// 경유점은 계산대 줄(x -2576.7) 양옆으로 빠져서 다른 테이블을 피해 간다.
struct SeatInObj {
    glm::vec2 seat;  // 통 의자 중심 (x, z)
    glm::vec2 table; // 테이블 중심 (바라보는 방향)
    std::vector<glm::vec2> path;
};
const glm::vec2 kLeftAisle(-2584.0f, 2587.0f);  // 계산대 왼쪽(-X)으로 빠지는 지점
const glm::vec2 kRightAisle(-2569.5f, 2587.0f); // 계산대 오른쪽(+X)으로 빠지는 지점
const std::vector<SeatInObj> kSeatsInObj = {
    { { -2601.6f, 2567.0f }, { -2606.3f, 2568.55f }, { kLeftAisle, { -2585.0f, 2566.0f } } },
    { { -2599.7f, 2587.9f }, { -2604.5f, 2586.9f }, { kLeftAisle } },
    { { -2592.05f, 2580.6f }, { -2592.8f, 2575.85f }, { kLeftAisle } },
    { { -2560.4f, 2580.1f }, { -2560.75f, 2575.1f }, { kRightAisle } },
    { { -2553.9f, 2566.95f }, { -2549.1f, 2567.9f }, { kRightAisle, { -2568.0f, 2566.0f } } },
    { { -2551.6f, 2587.4f }, { -2546.95f, 2585.8f }, { kRightAisle } },
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
    customerManager.customers.clear(); // 손님 메시(GL 버퍼)는 컨텍스트가 살아 있을 때 지운다
    squidward.Unload();
    squidwardLoaded = false;
    ui.Release();
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
    for (size_t i = 0; i < kSeatsInObj.size(); ++i) {
        const SeatInObj& def = kSeatsInObj[i];
        Seat seat;
        seat.number = static_cast<int>(i) + 1;
        seat.transform.position = MapToWorld(glm::vec3(def.seat.x, kFloorY, def.seat.y));
        const glm::vec2 toTable = def.table - def.seat;
        seat.transform.rotation.y = glm::degrees(std::atan2(toTable.x, toTable.y));
        seat.seatHeight = (kSeatTopY - kFloorY) * mapScale;
        seat.seatRadius = kSeatRadius * mapScale;
        for (const glm::vec2& point : def.path) seat.approachPath.push_back(MapToWorld(glm::vec3(point.x, kFloorY, point.y)));
        seats.push_back(std::move(seat));
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
    player.Spawn(MapToWorld(kSpawnFeetInObj), kSpawnYaw);
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
    player.Update(deltaTime);
    orderManager.Update(deltaTime);
    if (map) customerManager.Update(deltaTime);
    if (squidwardLoaded) squidward.Update(deltaTime);
}

void PlayScene::Render() {
    glClearColor(kClearColor.r, kClearColor.g, kClearColor.b, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
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

    DrawOrderTickets(width, height);
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
