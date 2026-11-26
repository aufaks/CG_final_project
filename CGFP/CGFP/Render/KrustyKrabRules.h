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
};
