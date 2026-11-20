#include "Render/UI.h"

#include "Render/ResourceManager.h"

UI::~UI() {
    Release();
}

void UI::Init() {
    if (vao) return;
    const float quad[] = {
        0.0f, 0.0f,  1.0f, 0.0f,  1.0f, 1.0f,
        0.0f, 0.0f,  1.0f, 1.0f,  0.0f, 1.0f,
    };
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
    glBindVertexArray(0);
}

void UI::Release() {
    if (vbo) glDeleteBuffers(1, &vbo);
    if (vao) glDeleteVertexArrays(1, &vao);
    vbo = 0;
    vao = 0;
}

void UI::Begin(int screenWidth, int screenHeight) {
    width = screenWidth;
    height = screenHeight;

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    ShaderManager& shaders = ResourceManager::Get().Shaders();
    shaders.Use("ui");
    shaders.SetMat4("uProjection", glm::ortho(0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f));
    shaders.SetInt("uTexture", 0);
    glBindVertexArray(vao);
}

void UI::End() {
    glBindVertexArray(0);
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
}

void UI::DrawImage(const std::string& path, const glm::vec4& rect, const glm::vec4& color) {
    Texture* texture = ResourceManager::Get().GetTexture(path);
    if (texture) texture->Bind(0);
    DrawQuad(rect, glm::vec4(0.0f, 0.0f, 1.0f, 1.0f), color, texture != nullptr);
}

void UI::DrawRect(const glm::vec4& rect, const glm::vec4& color) {
    DrawQuad(rect, glm::vec4(0.0f, 0.0f, 1.0f, 1.0f), color, false);
}

void UI::DrawImageCover(const std::string& path) {
    const glm::vec4 screen(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height));
    Texture* texture = ResourceManager::Get().GetTexture(path);
    if (!texture || width <= 0 || height <= 0) {
        DrawQuad(screen, glm::vec4(0.0f, 0.0f, 1.0f, 1.0f), glm::vec4(0.0f, 0.0f, 0.0f, 1.0f), false);
        return;
    }

    // 화면보다 넓은 쪽을 잘라 내도록 UV 범위를 줄인다
    const float imageAspect = static_cast<float>(texture->GetWidth()) / static_cast<float>(texture->GetHeight());
    const float screenAspect = static_cast<float>(width) / static_cast<float>(height);
    glm::vec4 uv(0.0f, 0.0f, 1.0f, 1.0f);
    if (imageAspect > screenAspect) {
        uv.z = screenAspect / imageAspect;
        uv.x = (1.0f - uv.z) * 0.5f;
    } else {
        uv.w = imageAspect / screenAspect;
        uv.y = (1.0f - uv.w) * 0.5f;
    }

    texture->Bind(0);
    DrawQuad(screen, uv, glm::vec4(1.0f), true);
}

void UI::DrawQuad(const glm::vec4& rect, const glm::vec4& uvRect, const glm::vec4& color, bool useTexture) {
    ShaderManager& shaders = ResourceManager::Get().Shaders();
    shaders.SetVec4("uRect", rect);
    shaders.SetVec4("uUVRect", uvRect);
    shaders.SetVec4("uColor", color);
    shaders.SetBool("uUseTexture", useTexture);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}
