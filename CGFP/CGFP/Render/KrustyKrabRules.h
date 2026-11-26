#pragma once
#include "Core/GL.h"
#include <string>

// 집게리아 원본 OBJ의 문과 교체할 조리도구를 구분한다.
struct KrustyKrabRules {
    static bool HallDoor(const std::string& material, const glm::vec3& center) {
        return material == "Door1Mtl" && center.x >= -2593.2f && center.x <= -2586.9f &&
            center.z >= 2600.2f && center.z <= 2601.7f;
    }
    static bool StorageDoor(const std::string& material) { return material == "Freezerdoorrim1Mtl"; }
    static bool ReplacedFork(const std::string& material, const glm::vec3& center) {
        return material == "Panrack1Mtl" && center.x >= -2559.52f && center.x <= -2559.31f &&
            center.y >= -117.64f && center.y <= -113.87f && center.z >= 2616.39f && center.z <= 2617.55f;
    }
    static bool WarehouseClutter(const std::string& material, const glm::vec3& center) {
        // 창고 입구에서 바닥 위에 떠 있는 통과 벽을 뚫고 나온 얼음 장식을 숨긴다.
        const bool floatingBarrel = material == "Barrel1Mtl" &&
            center.x >= -2602.9f && center.x <= -2598.8f &&
            center.y >= -122.5f && center.y <= -118.6f &&
            center.z >= 2630.5f && center.z <= 2634.6f;
        const bool protrudingIce = material == "Cickcle1Mtl" &&
            center.x >= -2607.1f && center.x <= -2602.4f &&
            center.z >= 2632.0f && center.z <= 2638.1f;
        const bool inWarehouse = center.x >= -2607.5f && center.x <= -2547.0f &&
            center.z >= 2631.0f && center.z <= 2690.5f;
        // 패티가 상자 밖으로 뚫고 나온 원본 장식은 통째로 제외한다.
        const bool looseFoodBoxes = inWarehouse &&
            (material == "Freezerbox1Mtl" || material == "Pattystack1Mtl");
        const bool backIce = inWarehouse && material == "Cickcle1Mtl" && center.z >= 2686.0f;
        return floatingBarrel || protrudingIce || looseFoodBoxes || backIce;
    }
    static float WarehouseHeightAdjustment(const std::string& material) {
        // 각각 바닥 높이 -125.46에 맞추어 통의 아랫면을 내린다.
        if (material == "Icecreamtubvanilla1Mtl") return -1.07f;
        if (material == "Icecreamtubstraw1Mtl") return -0.55f;
        if (material == "Icecreamtubchoc1Mtl") return -0.76f;
        return 0.0f;
    }
};
