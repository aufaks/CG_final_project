#include "Object/Object.h"

#include "Render/Mesh.h"
#include "Render/ShaderManager.h"
#include "Render/Texture.h"

glm::mat4 Transform::GetMatrix() const {
    glm::mat4 m = glm::translate(glm::mat4(1.0f), position);
    m = glm::rotate(m, glm::radians(rotation.y), glm::vec3(0, 1, 0));
    m = glm::rotate(m, glm::radians(rotation.x), glm::vec3(1, 0, 0));
    m = glm::rotate(m, glm::radians(rotation.z), glm::vec3(0, 0, 1));
    return glm::scale(m, scale);
}

void Object::Render(ShaderManager& shaders) const {
    if (!visible || !mesh) return;
    shaders.SetMat4("uModel", transform.GetMatrix());
    shaders.SetVec3("uColor", color);
    const bool useTexture = texture && texture->IsLoaded();
    shaders.SetBool("uUseTexture", useTexture);
    if (useTexture) texture->Bind(0);
    mesh->Draw();
}

Collider Object::GetWorldCollider() const {
    if (!mesh) return Collider(transform.position, transform.position);
    return Collider::FromLocalBounds(mesh->GetBoundsMin(), mesh->GetBoundsMax(), transform.GetMatrix());
}
