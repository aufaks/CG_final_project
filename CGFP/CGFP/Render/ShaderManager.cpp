#include "Render/ShaderManager.h"

#include <fstream>
#include <iostream>
#include <sstream>

std::string ShaderManager::ReadFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        std::cerr << "[Shader] 파일을 열 수 없음: " << path << "\n";
        return {};
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

GLuint ShaderManager::Compile(GLenum type, const std::string& source, const std::string& path) {
    GLuint shader = glCreateShader(type);
    const char* text = source.c_str();
    glShaderSource(shader, 1, &text, nullptr);
    glCompileShader(shader);

    GLint ok = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        std::cerr << "[Shader] 컴파일 실패 " << path << "\n" << log << "\n";
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

GLuint ShaderManager::Load(const std::string& name, const std::string& vertexPath, const std::string& fragmentPath) {
    const std::string vertexSource = ReadFile(vertexPath);
    const std::string fragmentSource = ReadFile(fragmentPath);
    if (vertexSource.empty() || fragmentSource.empty()) return 0;

    GLuint vertex = Compile(GL_VERTEX_SHADER, vertexSource, vertexPath);
    GLuint fragment = Compile(GL_FRAGMENT_SHADER, fragmentSource, fragmentPath);
    if (!vertex || !fragment) {
        if (vertex) glDeleteShader(vertex);
        if (fragment) glDeleteShader(fragment);
        return 0;
    }

    GLuint program = glCreateProgram();
    glAttachShader(program, vertex);
    glAttachShader(program, fragment);
    glLinkProgram(program);
    glDeleteShader(vertex);
    glDeleteShader(fragment);

    GLint ok = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetProgramInfoLog(program, sizeof(log), nullptr, log);
        std::cerr << "[Shader] 링크 실패 " << name << "\n" << log << "\n";
        glDeleteProgram(program);
        return 0;
    }

    auto existing = programs.find(name);
    if (existing != programs.end()) glDeleteProgram(existing->second);
    programs[name] = program;
    return program;
}

void ShaderManager::Use(const std::string& name) {
    auto it = programs.find(name);
    if (it == programs.end()) {
        std::cerr << "[Shader] 등록되지 않은 셰이더: " << name << "\n";
        return;
    }
    current = it->second;
    glUseProgram(current);
}

void ShaderManager::Clear() {
    for (auto& entry : programs) glDeleteProgram(entry.second);
    programs.clear();
    current = 0;
}

void ShaderManager::SetMat4(const char* uniform, const glm::mat4& value) const {
    glUniformMatrix4fv(glGetUniformLocation(current, uniform), 1, GL_FALSE, glm::value_ptr(value));
}

void ShaderManager::SetVec3(const char* uniform, const glm::vec3& value) const {
    glUniform3fv(glGetUniformLocation(current, uniform), 1, glm::value_ptr(value));
}

void ShaderManager::SetVec4(const char* uniform, const glm::vec4& value) const {
    glUniform4fv(glGetUniformLocation(current, uniform), 1, glm::value_ptr(value));
}

void ShaderManager::SetFloat(const char* uniform, float value) const {
    glUniform1f(glGetUniformLocation(current, uniform), value);
}

void ShaderManager::SetInt(const char* uniform, int value) const {
    glUniform1i(glGetUniformLocation(current, uniform), value);
}

void ShaderManager::SetBool(const char* uniform, bool value) const {
    glUniform1i(glGetUniformLocation(current, uniform), value ? 1 : 0);
}
