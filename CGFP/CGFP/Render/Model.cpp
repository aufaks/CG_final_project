#include "Render/Model.h"

#include "Render/ResourceManager.h"
#include "Render/ShaderManager.h"
#include "Render/Texture.h"

#include <algorithm>
#include <cfloat>
#include <charconv>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <unordered_map>

namespace fs = std::filesystem;

namespace {
struct Material {
    glm::vec3 color{ 0.8f };
    float opacity = 1.0f;
    bool glass = false; // 유리 재질만 반투명으로 그린다
    std::string texturePath;
};

using MaterialMap = std::unordered_map<std::string, Material>;

std::string Trim(const std::string& text) {
    const size_t begin = text.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) return {};
    const size_t end = text.find_last_not_of(" \t\r\n");
    return text.substr(begin, end - begin + 1);
}

// ObjectViewer.cpp의 ReadMaterialLibrary와 같은 규칙.
// 이 맵의 MTL은 불투명한 벽에도 낮은 d 값이 붙어 있어서, 이름에 Glass가 들어간 재질만 투명도를 적용한다.
void ReadMaterialLibrary(const fs::path& path, MaterialMap& materials) {
    std::ifstream file(path);
    if (!file) return;
    Material* current = nullptr;
    std::string line;
    while (std::getline(file, line)) {
        std::istringstream stream(line);
        std::string tag;
        stream >> tag;
        if (tag == "newmtl") {
            std::string name;
            std::getline(stream, name);
            name = Trim(name);
            current = &materials[name];
            current->glass = name.find("Glass") != std::string::npos || name.find("glass") != std::string::npos;
        } else if (!current) {
            continue;
        } else if (tag == "Kd") {
            stream >> current->color.r >> current->color.g >> current->color.b;
        } else if (tag == "d" && current->glass) {
            stream >> current->opacity;
        } else if (tag == "Tr" && current->glass) {
            float transparency = 0.0f;
            stream >> transparency;
            current->opacity = 1.0f - transparency;
        } else if (tag == "map_d" && current->glass) {
            current->opacity = std::min(current->opacity, 0.65f);
        } else if (tag == "map_Kd" || tag == "map_kd") {
            std::string filename;
            std::getline(stream, filename);
            current->texturePath = (path.parent_path() / Trim(filename)).string();
        }
    }
}

bool IsBlank(char c) { return c == ' ' || c == '\t'; }

const char* SkipBlanks(const char* p, const char* end) {
    while (p < end && IsBlank(*p)) ++p;
    return p;
}

const char* SkipToken(const char* p, const char* end) {
    while (p < end && !IsBlank(*p)) ++p;
    return p;
}

bool ParseFloats(const char* p, const char* end, float* out, int count) {
    for (int i = 0; i < count; ++i) {
        p = SkipBlanks(p, end);
        const auto result = std::from_chars(p, end, out[i]);
        if (result.ec != std::errc()) return false;
        p = result.ptr;
    }
    return true;
}

// OBJ 인덱스(1부터, 음수는 뒤에서부터) → 0부터. 비었거나 범위 밖이면 -1
int ParseIndex(const char* begin, const char* end, size_t count) {
    if (begin >= end) return -1;
    int raw = 0;
    if (std::from_chars(begin, end, raw).ec != std::errc() || raw == 0) return -1;
    const long long resolved = raw > 0 ? raw - 1 : static_cast<long long>(count) + raw;
    return (resolved >= 0 && resolved < static_cast<long long>(count)) ? static_cast<int>(resolved) : -1;
}
} // namespace

bool Model::LoadObj(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        std::cerr << "[Model] OBJ 파일을 열 수 없음: " << path << "\n";
        return false;
    }
    std::string contents(static_cast<size_t>(file.tellg()), '\0');
    file.seekg(0);
    file.read(contents.data(), static_cast<std::streamsize>(contents.size()));

    const fs::path objPath(path);
    MaterialMap materials;
    // 바운드는 면에 쓰이지 않은 정점까지 포함한다 (ObjectViewer의 정규화와 같은 기준)
    boundsMin = glm::vec3(FLT_MAX);
    boundsMax = glm::vec3(-FLT_MAX);

    std::vector<glm::vec3> positions, normals;
    std::vector<glm::vec2> uvs;
    std::unordered_map<std::string, std::vector<MeshVertex>> buckets;
    std::vector<std::string> materialOrder;
    std::vector<MeshVertex>* bucket = nullptr;
    std::string currentMaterial = "__default";

    struct FaceIndex { int position, uv, normal; };
    std::vector<FaceIndex> face;
    std::vector<MeshVertex> kitchenHallDoorVertices;
    std::vector<MeshVertex> kitchenStorageDoorVertices;

    const char* cursor = contents.data();
    const char* const fileEnd = cursor + contents.size();
    while (cursor < fileEnd) {
        const char* lineEnd = static_cast<const char*>(std::memchr(cursor, '\n', static_cast<size_t>(fileEnd - cursor)));
        if (!lineEnd) lineEnd = fileEnd;
        const char* p = SkipBlanks(cursor, lineEnd);
        const char* end = lineEnd;
        while (end > p && end[-1] == '\r') --end;
        cursor = lineEnd < fileEnd ? lineEnd + 1 : fileEnd;

        const char* tagEnd = SkipToken(p, end);
        const size_t tagLength = static_cast<size_t>(tagEnd - p);
        if (tagLength == 0 || *p == '#') continue;

        if (tagLength == 1 && p[0] == 'v') {
            glm::vec3 v;
            if (ParseFloats(tagEnd, end, &v.x, 3)) {
                positions.push_back(v);
                boundsMin = glm::min(boundsMin, v);
                boundsMax = glm::max(boundsMax, v);
            }
        } else if (tagLength == 2 && p[0] == 'v' && p[1] == 't') {
            glm::vec2 t;
            if (ParseFloats(tagEnd, end, &t.x, 2)) uvs.push_back(t);
        } else if (tagLength == 2 && p[0] == 'v' && p[1] == 'n') {
            glm::vec3 n;
            if (ParseFloats(tagEnd, end, &n.x, 3)) normals.push_back(glm::length(n) > 0.0f ? glm::normalize(n) : n);
        } else if (tagLength == 6 && std::memcmp(p, "mtllib", 6) == 0) {
            ReadMaterialLibrary(objPath.parent_path() / Trim(std::string(tagEnd, end)), materials);
        } else if (tagLength == 6 && std::memcmp(p, "usemtl", 6) == 0) {
            currentMaterial = Trim(std::string(tagEnd, end));
            bucket = nullptr;
        } else if (tagLength == 1 && p[0] == 'f') {
            face.clear();
            const char* item = SkipBlanks(tagEnd, end);
            while (item < end) {
                const char* itemEnd = SkipToken(item, end);
                const char* slash1 = std::find(item, itemEnd, '/');
                const char* slash2 = slash1 < itemEnd ? std::find(slash1 + 1, itemEnd, '/') : itemEnd;
                FaceIndex index;
                index.position = ParseIndex(item, slash1, positions.size());
                index.uv = slash1 < itemEnd ? ParseIndex(slash1 + 1, slash2, uvs.size()) : -1;
                index.normal = slash2 < itemEnd ? ParseIndex(slash2 + 1, itemEnd, normals.size()) : -1;
                if (index.position >= 0) face.push_back(index);
                item = SkipBlanks(itemEnd, end);
            }
            if (face.size() < 3) continue;

            if (!bucket) {
                auto inserted = buckets.try_emplace(currentMaterial);
                if (inserted.second) materialOrder.push_back(currentMaterial);
                bucket = &inserted.first->second;
            }

            for (size_t i = 1; i + 1 < face.size(); ++i) { // 다각형은 부채꼴로 삼각형 분할
                const FaceIndex tri[3] = { face[0], face[i], face[i + 1] };
                const glm::vec3& a = positions[tri[0].position];
                const glm::vec3 centroid = (a + positions[tri[1].position] + positions[tri[2].position]) / 3.0f;
                const bool kitchenHallDoor = currentMaterial == "Door1Mtl" &&
                    centroid.x >= -2593.2f && centroid.x <= -2586.9f &&
                    centroid.z >= 2600.2f && centroid.z <= 2601.7f;
                const bool kitchenStorageDoor = currentMaterial == "Freezerdoorrim1Mtl";
                // 주방 오른쪽 벽 조리도구 걸이의 두 번째 포크는 별도 뒤집개로 교체한다.
                const bool replacedRackFork = currentMaterial == "Panrack1Mtl" &&
                    centroid.x >= -2559.52f && centroid.x <= -2559.31f &&
                    centroid.y >= -117.64f && centroid.y <= -113.87f &&
                    centroid.z >= 2616.39f && centroid.z <= 2617.55f;
                glm::vec3 faceNormal = glm::cross(positions[tri[1].position] - a, positions[tri[2].position] - a);
                faceNormal = glm::length(faceNormal) > 1e-12f ? glm::normalize(faceNormal) : glm::vec3(0, 1, 0);
                for (const FaceIndex& index : tri) {
                    MeshVertex v;
                    v.position = positions[index.position];
                    v.normal = index.normal >= 0 ? normals[index.normal] : faceNormal;
                    if (index.uv >= 0) v.uv = uvs[index.uv];
                    if (!replacedRackFork) bucket->push_back(v);
                    if (kitchenHallDoor) kitchenHallDoorVertices.push_back(v);
                    if (kitchenStorageDoor) kitchenStorageDoorVertices.push_back(v);
                }
            }
        }
    }

    if (buckets.empty()) {
        std::cerr << "[Model] OBJ에 면이 없음: " << path << "\n";
        return false;
    }

    kitchenHallDoorMesh.reset();
    if (!kitchenHallDoorVertices.empty()) {
        std::vector<uint32_t> indices(kitchenHallDoorVertices.size());
        for (uint32_t i = 0; i < indices.size(); ++i) indices[i] = i;
        kitchenHallDoorMesh = std::make_unique<Mesh>();
        kitchenHallDoorMesh->Upload(kitchenHallDoorVertices, indices);
    }
    kitchenStorageDoorMesh.reset();
    if (!kitchenStorageDoorVertices.empty()) {
        std::vector<uint32_t> indices(kitchenStorageDoorVertices.size());
        for (uint32_t i = 0; i < indices.size(); ++i) indices[i] = i;
        kitchenStorageDoorMesh = std::make_unique<Mesh>();
        kitchenStorageDoorMesh->Upload(kitchenStorageDoorVertices, indices);
    }

    parts.clear();
    for (const std::string& name : materialOrder) {
        std::vector<MeshVertex>& vertices = buckets[name];
        std::vector<uint32_t> indices(vertices.size());
        for (uint32_t i = 0; i < indices.size(); ++i) indices[i] = i;

        Part part;
        part.materialName = name;
        part.mesh = std::make_unique<Mesh>();
        part.mesh->Upload(vertices, indices);
        part.center = (part.mesh->GetBoundsMin() + part.mesh->GetBoundsMax()) * 0.5f;

        const auto material = materials.find(name);
        if (material != materials.end()) {
            part.color = material->second.color;
            part.opacity = glm::clamp(material->second.opacity, 0.0f, 1.0f);
            if (!material->second.texturePath.empty()) {
                part.texture = ResourceManager::Get().GetTexture(material->second.texturePath);
                // 내보내기 프로그램이 넣은 기본 회색 Kd(0.3736, 0.3686, 0.3838)가 텍스처에 곱해지면
                // 원래 어두운 텍스처(튀김기·선반 등)가 거의 까맣게 된다. 이 값이면 텍스처 색을 그대로 쓴다.
                const glm::vec3 exporterGray(0.373615f, 0.368591f, 0.383775f);
                if (glm::all(glm::lessThan(glm::abs(part.color - exporterGray), glm::vec3(1e-4f))))
                    part.color = glm::vec3(1.0f);
            }
        }
        // 튀김기 텍스처 자체가 어두운 회색이므로 MTL의 낮은 Kd를 다시 곱하지 않는다.
        if (name == "Frier1Mtl" && part.texture) part.color = glm::vec3(1.0f);
        part.transparent = part.opacity < 0.999f;
        part.cullBackFaces = name == "Kktable1Mtl"; // 테이블 윗면이 겹쳐 깜빡이는 것을 막는다 (ObjectViewer와 같음)
        parts.push_back(std::move(part));

        std::vector<MeshVertex>().swap(vertices); // 업로드한 정점은 바로 해제
    }
    return true;
}

void Model::Draw(ShaderManager& shaders, const glm::mat4& modelMatrix, const glm::vec3& cameraPosition) const {
    shaders.SetMat4("uModel", modelMatrix);
    shaders.SetInt("uTexture", 0);

    auto drawPart = [&](const Part& part) {
        if (part.cullBackFaces) {
            glEnable(GL_CULL_FACE);
            glCullFace(GL_BACK);
        }
        shaders.SetVec3("uColor", part.color);
        shaders.SetFloat("uOpacity", part.opacity);
        const bool useTexture = part.texture && part.texture->IsLoaded();
        shaders.SetBool("uUseTexture", useTexture);
        if (useTexture) part.texture->Bind(0);
        part.mesh->Draw();
        if (part.cullBackFaces) glDisable(GL_CULL_FACE);
    };

    std::vector<const Part*> transparentParts;
    for (const Part& part : parts) {
        if (part.transparent) transparentParts.push_back(&part);
        else drawPart(part);
    }
    if (transparentParts.empty()) return;

    std::sort(transparentParts.begin(), transparentParts.end(), [&](const Part* a, const Part* b) {
        const glm::vec3 da = glm::vec3(modelMatrix * glm::vec4(a->center, 1.0f)) - cameraPosition;
        const glm::vec3 db = glm::vec3(modelMatrix * glm::vec4(b->center, 1.0f)) - cameraPosition;
        return glm::dot(da, da) > glm::dot(db, db);
    });
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    for (const Part* part : transparentParts) drawPart(*part);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}

void Model::DrawGeometry() const {
    for (const Part& part : parts) part.mesh->Draw();
}
