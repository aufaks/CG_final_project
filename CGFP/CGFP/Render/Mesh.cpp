#include "Render/Mesh.h"

#include <cfloat>
#include <cstddef>
#include <fstream>
#include <iostream>
#include <sstream>

Mesh::~Mesh() {
    Release();
}

void Mesh::Release() {
    if (ebo) glDeleteBuffers(1, &ebo);
    if (vbo) glDeleteBuffers(1, &vbo);
    if (vao) glDeleteVertexArrays(1, &vao);
    vao = vbo = ebo = 0;
    indexCount = 0;
}

void Mesh::Upload(const std::vector<MeshVertex>& vertices, const std::vector<uint32_t>& indices) {
    Release();

    boundsMin = glm::vec3(FLT_MAX);
    boundsMax = glm::vec3(-FLT_MAX);
    for (const MeshVertex& v : vertices) {
        boundsMin = glm::min(boundsMin, v.position);
        boundsMax = glm::max(boundsMax, v.position);
    }

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(MeshVertex), vertices.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(uint32_t), indices.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(MeshVertex), reinterpret_cast<void*>(offsetof(MeshVertex, position)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(MeshVertex), reinterpret_cast<void*>(offsetof(MeshVertex, normal)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(MeshVertex), reinterpret_cast<void*>(offsetof(MeshVertex, uv)));
    glBindVertexArray(0);

    indexCount = static_cast<GLsizei>(indices.size());
}

void Mesh::Draw() const {
    if (!vao) return;
    glBindVertexArray(vao);
    glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
}

std::unique_ptr<Mesh> Mesh::CreateCube() {
    struct Face { glm::vec3 normal, u, v; }; // u x v = normal (바깥에서 봤을 때 반시계)
    const Face faces[6] = {
        { { 1, 0, 0 }, { 0, 0, -1 }, { 0, 1, 0 } },
        { { -1, 0, 0 }, { 0, 0, 1 }, { 0, 1, 0 } },
        { { 0, 1, 0 }, { 1, 0, 0 }, { 0, 0, -1 } },
        { { 0, -1, 0 }, { 1, 0, 0 }, { 0, 0, 1 } },
        { { 0, 0, 1 }, { 1, 0, 0 }, { 0, 1, 0 } },
        { { 0, 0, -1 }, { -1, 0, 0 }, { 0, 1, 0 } },
    };
    const glm::vec2 corners[4] = { { -1, -1 }, { 1, -1 }, { 1, 1 }, { -1, 1 } };

    std::vector<MeshVertex> vertices;
    std::vector<uint32_t> indices;
    for (const Face& face : faces) {
        const uint32_t base = static_cast<uint32_t>(vertices.size());
        for (const glm::vec2& c : corners) {
            MeshVertex v;
            v.position = 0.5f * (face.normal + c.x * face.u + c.y * face.v);
            v.normal = face.normal;
            v.uv = (c + 1.0f) * 0.5f;
            vertices.push_back(v);
        }
        indices.insert(indices.end(), { base, base + 1, base + 2, base, base + 2, base + 3 });
    }

    auto mesh = std::make_unique<Mesh>();
    mesh->Upload(vertices, indices);
    return mesh;
}

namespace {
// OBJ 인덱스(1부터, 음수는 뒤에서부터)를 0부터 시작하는 인덱스로 바꾼다. 실패하면 -1.
int ResolveIndex(const std::string& token, size_t count) {
    if (token.empty()) return -1;
    int index = 0;
    try {
        index = std::stoi(token);
    } catch (...) {
        return -1;
    }
    const int resolved = index > 0 ? index - 1 : static_cast<int>(count) + index;
    return (resolved >= 0 && resolved < static_cast<int>(count)) ? resolved : -1;
}
} // namespace

bool Mesh::LoadObj(const std::string& path) {
    std::ifstream file(path);
    if (!file) {
        std::cerr << "[Mesh] OBJ 파일을 열 수 없음: " << path << "\n";
        return false;
    }

    std::vector<glm::vec3> positions, normals;
    std::vector<glm::vec2> uvs;
    std::vector<MeshVertex> vertices;
    std::vector<uint32_t> indices;

    std::string line;
    while (std::getline(file, line)) {
        std::istringstream stream(line);
        std::string tag;
        stream >> tag;
        if (tag == "v") {
            glm::vec3 p;
            stream >> p.x >> p.y >> p.z;
            positions.push_back(p);
        } else if (tag == "vt") {
            glm::vec2 t;
            stream >> t.x >> t.y;
            uvs.push_back(t);
        } else if (tag == "vn") {
            glm::vec3 n;
            stream >> n.x >> n.y >> n.z;
            normals.push_back(n);
        } else if (tag == "f") {
            std::vector<MeshVertex> face;
            std::string token;
            while (stream >> token) {
                // v, v/vt, v//vn, v/vt/vn
                const size_t slash1 = token.find('/');
                const size_t slash2 = slash1 == std::string::npos ? std::string::npos : token.find('/', slash1 + 1);
                const int pi = ResolveIndex(token.substr(0, slash1), positions.size());
                if (pi < 0) continue;

                MeshVertex v;
                v.position = positions[pi];
                if (slash1 != std::string::npos) {
                    const size_t uvLength = slash2 == std::string::npos ? std::string::npos : slash2 - slash1 - 1;
                    const int ti = ResolveIndex(token.substr(slash1 + 1, uvLength), uvs.size());
                    if (ti >= 0) v.uv = uvs[ti];
                }
                if (slash2 != std::string::npos) {
                    const int ni = ResolveIndex(token.substr(slash2 + 1), normals.size());
                    if (ni >= 0) v.normal = normals[ni];
                }
                face.push_back(v);
            }

            for (size_t i = 1; i + 1 < face.size(); ++i) { // 다각형은 부채꼴로 삼각형 분할
                MeshVertex tri[3] = { face[0], face[i], face[i + 1] };
                if (glm::length(tri[0].normal) < 1e-6f) { // 노멀이 없으면 면 노멀 사용
                    glm::vec3 n = glm::cross(tri[1].position - tri[0].position, tri[2].position - tri[0].position);
                    if (glm::length(n) > 1e-12f) n = glm::normalize(n);
                    for (MeshVertex& v : tri) v.normal = n;
                }
                for (const MeshVertex& v : tri) {
                    indices.push_back(static_cast<uint32_t>(vertices.size()));
                    vertices.push_back(v);
                }
            }
        }
    }

    if (vertices.empty()) {
        std::cerr << "[Mesh] OBJ에 면이 없음: " << path << "\n";
        return false;
    }
    Upload(vertices, indices);
    return true;
}
