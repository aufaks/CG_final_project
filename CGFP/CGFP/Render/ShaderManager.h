#pragma once
#include "Core/GL.h"

#include <string>
#include <unordered_map>

// 셰이더 컴파일·링크와 uniform 설정. 이름으로 프로그램을 등록해 두고 Use로 고른다.
class ShaderManager {
public:
    GLuint Load(const std::string& name, const std::string& vertexPath, const std::string& fragmentPath);
    void Use(const std::string& name);
    GLuint GetCurrent() const { return current; }
    void Clear(); // GL 컨텍스트가 살아 있을 때 호출

    void SetMat4(const char* uniform, const glm::mat4& value) const;
    void SetVec3(const char* uniform, const glm::vec3& value) const;
    void SetVec4(const char* uniform, const glm::vec4& value) const;
    void SetFloat(const char* uniform, float value) const;
    void SetInt(const char* uniform, int value) const;
    void SetBool(const char* uniform, bool value) const;

private:
    static std::string ReadFile(const std::string& path);
    static GLuint Compile(GLenum type, const std::string& source, const std::string& path);

    std::unordered_map<std::string, GLuint> programs;
    GLuint current = 0;
};
