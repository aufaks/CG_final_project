#pragma once
#include "Core/GL.h"

#include <string>

// 텍스처(말풍선·재료 등). PNG/JPEG는 Windows 기본 WIC로 디코딩한다.
class Texture {
public:
    Texture() = default;
    ~Texture();
    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;

    bool Load(const std::string& path);
    void Bind(int unit = 0) const;

    bool IsLoaded() const { return id != 0; }
    GLuint GetId() const { return id; }
    int GetWidth() const { return width; }
    int GetHeight() const { return height; }

private:
    GLuint id = 0;
    int width = 0;
    int height = 0;
};
