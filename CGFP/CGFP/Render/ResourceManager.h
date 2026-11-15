#pragma once
#include "Render/Mesh.h"
#include "Render/ShaderManager.h"
#include "Render/Texture.h"

#include <memory>
#include <string>
#include <unordered_map>

// 같은 Mesh·Texture를 한 번만 로드해서 공유한다. 셰이더도 여기서 소유한다.
// 앱 전체에서 하나만 쓰므로 Get()으로 접근한다.
class ResourceManager {
public:
    static ResourceManager& Get();

    ShaderManager& Shaders() { return shaders; }
    Mesh* GetMesh(const std::string& objPath);    // 실패하면 nullptr
    Mesh* GetCube();
    Texture* GetTexture(const std::string& path); // 실패하면 nullptr

    void Clear(); // GL 컨텍스트를 없애기 전에 호출

private:
    ResourceManager() = default;

    ShaderManager shaders;
    std::unique_ptr<Mesh> cube;
    std::unordered_map<std::string, std::unique_ptr<Mesh>> meshes;
    std::unordered_map<std::string, std::unique_ptr<Texture>> textures;
};
