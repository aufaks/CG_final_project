#pragma once
#include "Core/GL.h"

#include <cstdint>
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
    void Upload(const std::vector<MeshVertex>& vertices, const std::vector<uint32_t>& indices);
    void Draw() const;

    // 중심이 원점인 1x1x1 큐브. 모델이 준비되기 전 회색 박스로 쓴다.
    static std::unique_ptr<Mesh> CreateCube();

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
