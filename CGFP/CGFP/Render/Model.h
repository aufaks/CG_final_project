#pragma once
#include "Core/GL.h"
#include "Render/Mesh.h"

#include <memory>
#include <string>
#include <vector>

class ShaderManager;
class Texture;

// 재질(.mtl)이 여러 개인 OBJ 모델. 재질마다 Mesh 하나(Part)로 나눠 텍스처·색을 따로 그린다.
// 맵처럼 큰 모델용. 셰이더는 shaders/object_viewer.* (uOpacity, uReferenceLighting 사용)
class Model {
public:
    struct Part {
        std::string materialName;
        std::unique_ptr<Mesh> mesh;
        Texture* texture = nullptr; // ResourceManager 소유
        glm::vec3 color{ 0.8f };
        glm::vec3 center{ 0.0f };   // 모델 공간. 반투명 정렬용
        float opacity = 1.0f;
        bool transparent = false;
        bool cullBackFaces = false;
    };

    bool LoadObj(const std::string& path, bool krustyKrab = false);

    // 불투명 파트를 먼저 그리고, 반투명 파트는 카메라에서 먼 순서로 그린다.
    void Draw(ShaderManager& shaders, const glm::mat4& modelMatrix, const glm::vec3& cameraPosition) const;
    // 외곽선의 스텐실/확장 패스처럼 재질 없이 형상만 다시 그릴 때 사용한다.
    void DrawGeometry() const;

    const glm::vec3& GetBoundsMin() const { return boundsMin; }
    const glm::vec3& GetBoundsMax() const { return boundsMax; }
    bool IsLoaded() const { return !parts.empty(); }
    const Mesh* GetKitchenHallDoorMesh() const { return kitchenHallDoorMesh.get(); }
    const Mesh* GetKitchenStorageDoorMesh() const { return kitchenStorageDoorMesh.get(); }

private:
    std::vector<Part> parts;
    std::unique_ptr<Mesh> kitchenHallDoorMesh;
    std::unique_ptr<Mesh> kitchenStorageDoorMesh;
    glm::vec3 boundsMin{ 0.0f };
    glm::vec3 boundsMax{ 0.0f };
};
