#pragma once
#include "Core/GL.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

struct MeshVertex {
    glm::vec3 position{ 0.0f };
    glm::vec3 normal{ 0.0f };
    glm::vec2 uv{ 0.0f };
};

// 모델 데이터(VAO/VBO/EBO). OBJ 로딩과 회색 박스용 큐브 생성을 지원한다.
class Mesh {
public:
    Mesh() = default;
    ~Mesh();
    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;

    // 간단한 OBJ 로더(v/vt/vn/f). 재질(.mtl)은 아직 읽지 않는다.
    // TODO(2단계): 큰 모델은 ObjectViewer.cpp의 ModelLoader(캐시·멀티스레드)를 옮겨와 쓴다.
    bool LoadObj(const std::string& path);
    // OBJ를 여러 부위 메시로 나눠 읽는다. classify(그룹 이름, 삼각형 중심)가 부위 번호(0..partCount-1)를 돌려준다.
    // 범위 밖 번호를 돌려주면 그 삼각형은 버린다. 관절 애니메이션용.
    using PartClassifier = std::function<int(const std::string& group, const glm::vec3& centroid)>;
    static std::vector<std::unique_ptr<Mesh>> LoadObjParts(const std::string& path, int partCount, const PartClassifier& classify);
    // OBJ를 GPU에 올리지 않고 정점·인덱스만 읽는다 (CPU에서 정점을 변형하는 캐릭터용)
    static bool ReadObj(const std::string& path, std::vector<MeshVertex>& vertices, std::vector<uint32_t>& indices);
    void Upload(const std::vector<MeshVertex>& vertices, const std::vector<uint32_t>& indices);
    // 같은 개수의 정점으로 VBO 내용만 바꾼다 (Upload한 뒤 매 프레임 변형할 때)
    void UpdateVertices(const std::vector<MeshVertex>& vertices);
    void Draw() const;

    // 중심이 원점인 1x1x1 큐브. 모델이 준비되기 전 회색 박스로 쓴다.
    static std::unique_ptr<Mesh> CreateCube();

    bool IsEmpty() const { return indexCount == 0; }
    const glm::vec3& GetBoundsMin() const { return boundsMin; }
    const glm::vec3& GetBoundsMax() const { return boundsMax; }

private:
    void Release();

    GLuint vao = 0;
    GLuint vbo = 0;
    GLuint ebo = 0;
    GLsizei indexCount = 0;
    glm::vec3 boundsMin{ -0.5f };
    glm::vec3 boundsMax{ 0.5f };
};
