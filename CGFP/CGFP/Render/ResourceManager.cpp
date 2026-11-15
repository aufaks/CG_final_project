#include "Render/ResourceManager.h"

ResourceManager& ResourceManager::Get() {
    static ResourceManager instance;
    return instance;
}

Mesh* ResourceManager::GetMesh(const std::string& objPath) {
    auto it = meshes.find(objPath);
    if (it != meshes.end()) return it->second.get();

    auto mesh = std::make_unique<Mesh>();
    if (!mesh->LoadObj(objPath)) mesh.reset(); // 실패도 기록해서 다시 읽지 않게 한다
    Mesh* result = mesh.get();
    meshes.emplace(objPath, std::move(mesh));
    return result;
}

Mesh* ResourceManager::GetCube() {
    if (!cube) cube = Mesh::CreateCube();
    return cube.get();
}

Texture* ResourceManager::GetTexture(const std::string& path) {
    auto it = textures.find(path);
    if (it != textures.end()) return it->second.get();

    auto texture = std::make_unique<Texture>();
    if (!texture->Load(path)) texture.reset();
    Texture* result = texture.get();
    textures.emplace(path, std::move(texture));
    return result;
}

void ResourceManager::Clear() {
    meshes.clear();
    textures.clear();
    cube.reset();
    shaders.Clear();
}
