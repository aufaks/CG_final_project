#include <GL/glew.h>
#include <GL/glfw3.h>
#include <GL/glm/glm.hpp>
#include <GL/glm/gtc/matrix_transform.hpp>
#include <GL/glm/gtc/type_ptr.hpp>
#define NOMINMAX
#include <Windows.h>
#include <wincodec.h>
#include <wrl/client.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

namespace {
constexpr int kWindowWidth = 1280;
constexpr int kWindowHeight = 800;
constexpr float kMoveSpeed = 5.0f;
constexpr float kMapMoveSpeed = 2.5f;
constexpr float kMouseSensitivity = 0.05f;
constexpr float kPi = 3.14159265358979323846f;

struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 uv;
};

struct MeshPart {
    GLuint vao = 0;
    GLuint vbo = 0;
    GLuint texture = 0;
    GLsizei vertexCount = 0;
    glm::vec3 color{ 0.8f, 0.8f, 0.8f };
};

struct Model {
    fs::path path;
    std::vector<MeshPart> parts;
    float displayScale = 1.0f;
    float cameraMoveSpeed = kMoveSpeed;
    glm::vec3 cameraStartPosition{ 0.0f, 0.0f, 4.0f };
    float cameraPitch = 0.0f;
};

struct Camera {
    glm::vec3 position{ 0.0f, 0.0f, 4.0f };
    float yaw = -90.0f;
    float pitch = 0.0f;

    glm::vec3 Forward() const {
        const float yawRadians = glm::radians(yaw);
        const float pitchRadians = glm::radians(pitch);
        return glm::normalize(glm::vec3(
            std::cos(yawRadians) * std::cos(pitchRadians),
            std::sin(pitchRadians),
            std::sin(yawRadians) * std::cos(pitchRadians)));
    }

    glm::mat4 View() const {
        return glm::lookAt(position, position + Forward(), glm::vec3(0, 1, 0));
    }
};

Camera gCamera;
bool gFirstMouse = true;
bool gCursorCaptured = true;
double gLastMouseX = kWindowWidth * 0.5;
double gLastMouseY = kWindowHeight * 0.5;
std::vector<Model> gModels;
size_t gCurrentModel = 0;
int gWindowWidth = kWindowWidth;
int gWindowHeight = kWindowHeight;
Microsoft::WRL::ComPtr<IWICImagingFactory> gWicFactory;
std::unordered_map<std::string, GLuint> gTextureCache;

std::string ReadTextFile(const fs::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) throw std::runtime_error("셰이더 파일을 열 수 없습니다: " + path.string());
    std::ostringstream contents;
    contents << file.rdbuf();
    return contents.str();
}

GLuint CompileShader(GLenum type, const std::string& source, const fs::path& path) {
    GLuint shader = glCreateShader(type);
    const char* sourcePointer = source.c_str();
    glShaderSource(shader, 1, &sourcePointer, nullptr);
    glCompileShader(shader);
    GLint success = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (success == GL_FALSE) {
        GLint length = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
        std::string log(static_cast<size_t>(std::max(length, 1)), '\0');
        glGetShaderInfoLog(shader, length, nullptr, log.data());
        glDeleteShader(shader);
        throw std::runtime_error("셰이더 컴파일 실패 (" + path.string() + "): " + log);
    }
    return shader;
}

GLuint LoadShaderProgram(const fs::path& shaderDirectory) {
    const fs::path vertexPath = shaderDirectory / "object_viewer.vert";
    const fs::path fragmentPath = shaderDirectory / "object_viewer.frag";
    GLuint vertexShader = CompileShader(GL_VERTEX_SHADER, ReadTextFile(vertexPath), vertexPath);
    GLuint fragmentShader = CompileShader(GL_FRAGMENT_SHADER, ReadTextFile(fragmentPath), fragmentPath);
    GLuint program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    GLint success = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (success == GL_FALSE) {
        GLint length = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
        std::string log(static_cast<size_t>(std::max(length, 1)), '\0');
        glGetProgramInfoLog(program, length, nullptr, log.data());
        glDeleteProgram(program);
        throw std::runtime_error("셰이더 프로그램 링크 실패: " + log);
    }
    return program;
}

int ParseIndex(const std::string& token, size_t count, const fs::path& path, int lineNumber) {
    if (token.empty()) return -1;
    const int raw = std::stoi(token);
    if (raw == 0) throw std::runtime_error(path.string() + ":" + std::to_string(lineNumber) + " OBJ 인덱스는 0일 수 없습니다.");
    const long long index = raw > 0 ? static_cast<long long>(raw) - 1 : static_cast<long long>(count) + raw;
    if (index < 0 || index >= static_cast<long long>(count)) {
        throw std::runtime_error(path.string() + ":" + std::to_string(lineNumber) + " 범위를 벗어난 OBJ 인덱스입니다.");
    }
    return static_cast<int>(index);
}

std::string Trim(const std::string& value) {
    const size_t first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    const size_t last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

struct Material {
    glm::vec3 color{ 0.8f, 0.8f, 0.8f };
    fs::path diffuseTexture;
};

using MaterialMap = std::unordered_map<std::string, Material>;

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
            current = &materials[Trim(name)];
        }
        else if (current && tag == "Kd") {
            stream >> current->color.r >> current->color.g >> current->color.b;
        }
        else if (current && (tag == "map_Kd" || tag == "map_kd")) {
            std::string filename;
            std::getline(stream, filename);
            filename = Trim(filename);
            // The supplied OBJ assets use plain map_Kd paths (without map options).
            current->diffuseTexture = path.parent_path() / fs::path(filename);
        }
    }
}

GLuint LoadTexture(const fs::path& path) {
    if (!fs::exists(path)) return 0;
    const std::string cacheKey = path.lexically_normal().string();
    const auto found = gTextureCache.find(cacheKey);
    if (found != gTextureCache.end()) return found->second;

    Microsoft::WRL::ComPtr<IWICBitmapDecoder> decoder;
    HRESULT result = gWicFactory->CreateDecoderFromFilename(
        path.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad, decoder.GetAddressOf());
    if (FAILED(result)) {
        std::cerr << "Texture decode failed: " << path.string() << " (HRESULT 0x" << std::hex << result << std::dec << ")\n";
        return 0;
    }

    Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> frame;
    result = decoder->GetFrame(0, frame.GetAddressOf());
    if (FAILED(result)) return 0;
    UINT width = 0, height = 0;
    if (FAILED(frame->GetSize(&width, &height)) || width == 0 || height == 0) return 0;

    Microsoft::WRL::ComPtr<IWICFormatConverter> converter;
    result = gWicFactory->CreateFormatConverter(converter.GetAddressOf());
    if (FAILED(result)) return 0;
    result = converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppRGBA,
        WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom);
    if (FAILED(result)) return 0;

    const UINT stride = width * 4;
    std::vector<BYTE> pixels(static_cast<size_t>(stride) * height);
    if (FAILED(converter->CopyPixels(nullptr, stride, static_cast<UINT>(pixels.size()), pixels.data()))) return 0;

    // WIC row 0 is the image top; OpenGL UV origin is the bottom.
    std::vector<BYTE> flipped(pixels.size());
    for (UINT row = 0; row < height; ++row) {
        std::memcpy(flipped.data() + static_cast<size_t>(row) * stride,
            pixels.data() + static_cast<size_t>(height - 1 - row) * stride, stride);
    }

    GLuint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, static_cast<GLsizei>(width), static_cast<GLsizei>(height),
        0, GL_RGBA, GL_UNSIGNED_BYTE, flipped.data());
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    gTextureCache.emplace(cacheKey, texture);
    return texture;
}

fs::path FindFallbackTexture(const fs::path& directory) {
    std::vector<fs::path> candidates;
    if (fs::exists(directory)) {
        for (const auto& entry : fs::recursive_directory_iterator(directory)) {
            if (!entry.is_regular_file()) continue;
            std::string ext = entry.path().extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(),
                [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" || ext == ".tga")
                candidates.push_back(entry.path());
        }
    }
    if (candidates.empty()) return {};
    std::sort(candidates.begin(), candidates.end());
    const auto isColorMap = [](const fs::path& p) {
        std::string name = p.filename().string();
        std::transform(name.begin(), name.end(), name.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return name.find("basecolor") != std::string::npos || name.find("diff") != std::string::npos ||
            name.find("albedo") != std::string::npos || name.find("color") != std::string::npos;
    };
    const auto colorMap = std::find_if(candidates.begin(), candidates.end(), isColorMap);
    return colorMap != candidates.end() ? *colorMap : candidates.front();
}

bool LoadObj(const fs::path& path, Model& model, std::string& error) {
    std::ifstream file(path);
    if (!file) {
        error = "파일을 열 수 없습니다.";
        return false;
    }

    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> normals;
    std::vector<glm::vec2> texcoords;
    std::unordered_map<std::string, std::vector<Vertex>> materialVertices;
    std::vector<std::string> materialLibraries;
    std::string currentMaterial = "__default";
    glm::vec3 minimum(std::numeric_limits<float>::max());
    glm::vec3 maximum(std::numeric_limits<float>::lowest());
    std::string line;
    int lineNumber = 0;

    try {
        while (std::getline(file, line)) {
            ++lineNumber;
            std::istringstream stream(line);
            std::string tag;
            stream >> tag;
            if (tag.empty() || tag[0] == '#') continue;

            if (tag == "v") {
                glm::vec3 position;
                if (!(stream >> position.x >> position.y >> position.z)) continue;
                positions.push_back(position);
                minimum = glm::min(minimum, position);
                maximum = glm::max(maximum, position);
            }
            else if (tag == "vt") {
                glm::vec2 uv;
                if (stream >> uv.x >> uv.y) texcoords.push_back(uv);
            }
            else if (tag == "vn") {
                glm::vec3 normal;
                if (stream >> normal.x >> normal.y >> normal.z) normals.push_back(glm::normalize(normal));
            }
            else if (tag == "mtllib") {
                std::string library;
                std::getline(stream, library);
                library = Trim(library);
                if (!library.empty()) materialLibraries.push_back(library);
            }
            else if (tag == "usemtl") {
                std::getline(stream, currentMaterial);
                currentMaterial = Trim(currentMaterial);
            }
            else if (tag == "f") {
                struct FaceIndex { int position; int texcoord; int normal; };
                std::vector<FaceIndex> face;
                std::string item;
                while (stream >> item) {
                    std::string parts[3];
                    size_t part = 0;
                    size_t start = 0;
                    for (size_t i = 0; i <= item.size(); ++i) {
                        if (i == item.size() || item[i] == '/') {
                            if (part < 3) parts[part++] = item.substr(start, i - start);
                            start = i + 1;
                        }
                    }
                    if (parts[0].empty()) continue;
                    face.push_back({ ParseIndex(parts[0], positions.size(), path, lineNumber),
                                     parts[1].empty() ? -1 : ParseIndex(parts[1], texcoords.size(), path, lineNumber),
                                     parts[2].empty() ? -1 : ParseIndex(parts[2], normals.size(), path, lineNumber) });
                }
                if (face.size() < 3) continue;

                // Triangulate polygons as a fan. Generate flat normals when the OBJ has none.
                for (size_t i = 1; i + 1 < face.size(); ++i) {
                    const FaceIndex triangle[3] = { face[0], face[i], face[i + 1] };
                    const glm::vec3 a = positions[triangle[0].position];
                    const glm::vec3 b = positions[triangle[1].position];
                    const glm::vec3 c = positions[triangle[2].position];
                    glm::vec3 faceNormal = glm::cross(b - a, c - a);
                    faceNormal = glm::length(faceNormal) > 0.00001f ? glm::normalize(faceNormal) : glm::vec3(0, 1, 0);
                    for (const FaceIndex& index : triangle) {
                        const glm::vec3 normal = index.normal >= 0 ? normals[index.normal] : faceNormal;
                        const glm::vec2 uv = index.texcoord >= 0 ? texcoords[index.texcoord] : glm::vec2(0.0f);
                        materialVertices[currentMaterial].push_back({ positions[index.position], normal, uv });
                    }
                }
            }
        }
    }
    catch (const std::exception& exception) {
        error = exception.what();
        return false;
    }

    size_t vertexCount = 0;
    for (const auto& pair : materialVertices) vertexCount += pair.second.size();
    if (vertexCount == 0) {
        error = "OBJ에 표시할 유효한 면(face)이 없습니다.";
        return false;
    }

    // Center the model and normalize its largest dimension for predictable viewing.
    const glm::vec3 center = (minimum + maximum) * 0.5f;
    const float largestDimension = std::max({ maximum.x - minimum.x, maximum.y - minimum.y, maximum.z - minimum.z });
    const float normalizeScale = largestDimension > 0.00001f ? 2.0f / largestDimension : 1.0f;
    const bool isRestaurantMap = path.filename() == "BFBBR - Krusty Krab.obj";
    model.displayScale = isRestaurantMap ? 96.0f : 1.0f;
    model.cameraMoveSpeed = isRestaurantMap ? kMapMoveSpeed : kMoveSpeed;
    model.cameraStartPosition = glm::vec3(0.0f, 0.0f, 4.0f);
    model.cameraPitch = 0.0f;
    if (isRestaurantMap) {
        // The restaurant geometry is grouped around this source-space point in the OBJ.
        const glm::vec3 restaurantInterior(0.73f, 0.30f, -1.40f);
        model.cameraStartPosition = (restaurantInterior - center) * normalizeScale * model.displayScale;
    }
    model.path = path;
    MaterialMap materials;
    bool loadedLibrary = false;
    for (const std::string& library : materialLibraries) {
        const fs::path materialPath = path.parent_path() / fs::path(library);
        if (fs::exists(materialPath)) {
            ReadMaterialLibrary(materialPath, materials);
            loadedLibrary = true;
        }
    }
    // Some supplied model packages rename their MTL after the OBJ was exported.
    if (!loadedLibrary && fs::exists(path.parent_path())) {
        for (const auto& entry : fs::directory_iterator(path.parent_path())) {
            if (entry.is_regular_file() && entry.path().extension() == ".mtl") ReadMaterialLibrary(entry.path(), materials);
        }
    }

    const fs::path fallbackTexture = FindFallbackTexture(path.parent_path());
    for (auto& pair : materialVertices) {
        Material material;
        const auto materialIt = materials.find(pair.first);
        if (materialIt != materials.end()) material = materialIt->second;
        for (Vertex& vertex : pair.second) vertex.position = (vertex.position - center) * normalizeScale * model.displayScale;

        fs::path texturePath = material.diffuseTexture;
        if (texturePath.empty() || !fs::exists(texturePath)) texturePath = fallbackTexture;
        MeshPart part;
        part.color = material.color;
        if (!texturePath.empty()) part.texture = LoadTexture(texturePath);
        part.vertexCount = static_cast<GLsizei>(pair.second.size());
        glGenVertexArrays(1, &part.vao);
        glGenBuffers(1, &part.vbo);
        glBindVertexArray(part.vao);
        glBindBuffer(GL_ARRAY_BUFFER, part.vbo);
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(pair.second.size() * sizeof(Vertex)), pair.second.data(), GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, position)));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, normal)));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, uv)));
        glBindVertexArray(0);
        model.parts.push_back(part);
    }
    return true;
}

std::vector<fs::path> FindObjFiles(const fs::path& root) {
    std::vector<fs::path> paths;
    if (!fs::exists(root)) return paths;
    for (const auto& entry : fs::recursive_directory_iterator(root)) {
        if (!entry.is_regular_file()) continue;
        std::string extension = entry.path().extension().string();
        std::transform(extension.begin(), extension.end(), extension.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (extension == ".obj") paths.push_back(entry.path());
    }
    std::sort(paths.begin(), paths.end());
    return paths;
}

void UpdateWindowTitle(GLFWwindow* window) {
    if (gModels.empty()) {
        glfwSetWindowTitle(window, "Object Viewer | obj 폴더에 .obj 파일을 넣어주세요 | P: 마우스 해제/잠금");
        return;
    }
    const std::string title = "Object Viewer (" + std::to_string(gCurrentModel + 1) + "/" +
        std::to_string(gModels.size()) + ") | " + gModels[gCurrentModel].path.filename().string() +
        " | ←/→: 이전/다음 | WASD: 이동 | E/Q: 높이 | P: 마우스 " +
        (gCursorCaptured ? "해제" : "잠금") + " | Esc: 종료";
    glfwSetWindowTitle(window, title.c_str());
}

void SetCursorCaptured(GLFWwindow* window, bool captured) {
    gCursorCaptured = captured;
    gFirstMouse = true;
    glfwSetInputMode(window, GLFW_CURSOR, captured ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
    UpdateWindowTitle(window);
}

void SelectModel(GLFWwindow* window, int direction) {
    if (gModels.empty()) return;
    const size_t count = gModels.size();
    gCurrentModel = (gCurrentModel + count + static_cast<size_t>(direction > 0 ? 1 : count - 1)) % count;
    gCamera.position = gModels[gCurrentModel].cameraStartPosition;
    gCamera.yaw = -90.0f;
    gCamera.pitch = gModels[gCurrentModel].cameraPitch;
    UpdateWindowTitle(window);
}

void KeyCallback(GLFWwindow* window, int key, int, int action, int) {
    if (action != GLFW_PRESS) return;
    if (key == GLFW_KEY_ESCAPE) glfwSetWindowShouldClose(window, GLFW_TRUE);
    else if (key == GLFW_KEY_RIGHT) SelectModel(window, 1);
    else if (key == GLFW_KEY_LEFT) SelectModel(window, -1);
    else if (key == GLFW_KEY_P) SetCursorCaptured(window, !gCursorCaptured);
}

void MouseCallback(GLFWwindow*, double x, double y) {
    if (!gCursorCaptured) return;
    if (gFirstMouse) {
        gLastMouseX = x;
        gLastMouseY = y;
        gFirstMouse = false;
        return;
    }
    const float xOffset = static_cast<float>(x - gLastMouseX) * kMouseSensitivity;
    const float yOffset = static_cast<float>(gLastMouseY - y) * kMouseSensitivity;
    gLastMouseX = x;
    gLastMouseY = y;
    gCamera.yaw += xOffset;
    gCamera.pitch = std::clamp(gCamera.pitch + yOffset, -89.0f, 89.0f);
}

void FramebufferCallback(GLFWwindow*, int width, int height) {
    gWindowWidth = std::max(width, 1);
    gWindowHeight = std::max(height, 1);
    glViewport(0, 0, width, height);
}

void ProcessMovement(GLFWwindow* window, float deltaTime) {
    const glm::vec3 forward = gCamera.Forward();
    const glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0, 1, 0)));
    const float distance = gModels[gCurrentModel].cameraMoveSpeed * deltaTime;
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) gCamera.position += forward * distance;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) gCamera.position -= forward * distance;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) gCamera.position += right * distance;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) gCamera.position -= right * distance;
    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) gCamera.position.y += distance;
    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) gCamera.position.y -= distance;
}

void Cleanup() {
    for (Model& model : gModels) {
        for (MeshPart& part : model.parts) {
            if (part.vbo) glDeleteBuffers(1, &part.vbo);
            if (part.vao) glDeleteVertexArrays(1, &part.vao);
        }
    }
    for (const auto& texture : gTextureCache) glDeleteTextures(1, &texture.second);
}
}

int main() {
    try {
        fs::current_path(fs::absolute(fs::path(__FILE__)).parent_path());
        const fs::path modelDirectory = fs::current_path() / "obj";
        const fs::path shaderDirectory = fs::current_path() / "shaders";

        const HRESULT comStatus = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        if (FAILED(comStatus)) throw std::runtime_error("Windows 이미지 로더 초기화에 실패했습니다.");
        const HRESULT wicStatus = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
            IID_PPV_ARGS(gWicFactory.GetAddressOf()));
        if (FAILED(wicStatus)) {
            CoUninitialize();
            throw std::runtime_error("PNG/JPEG 텍스처 로더(WIC)를 초기화하지 못했습니다.");
        }

        if (!glfwInit()) throw std::runtime_error("GLFW 초기화에 실패했습니다.");
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef _DEBUG
        glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
#endif

        GLFWwindow* window = glfwCreateWindow(kWindowWidth, kWindowHeight, "Object Viewer", nullptr, nullptr);
        if (!window) {
            glfwTerminate();
            throw std::runtime_error("OpenGL 창을 만들 수 없습니다.");
        }
        glfwMakeContextCurrent(window);
        glfwSwapInterval(1);
        glewExperimental = GL_TRUE;
        const GLenum glewStatus = glewInit();
        if (glewStatus != GLEW_OK) {
            glfwDestroyWindow(window);
            glfwTerminate();
            throw std::runtime_error("GLEW 초기화 실패: " + std::string(reinterpret_cast<const char*>(glewGetErrorString(glewStatus))));
        }
        // GLEW may emit INVALID_ENUM while probing core contexts.
        glGetError();

        glfwSetKeyCallback(window, KeyCallback);
        glfwSetCursorPosCallback(window, MouseCallback);
        glfwSetFramebufferSizeCallback(window, FramebufferCallback);
        SetCursorCaptured(window, true);
        glfwGetFramebufferSize(window, &gWindowWidth, &gWindowHeight);
        glViewport(0, 0, gWindowWidth, gWindowHeight);
        glEnable(GL_DEPTH_TEST);

        const GLuint program = LoadShaderProgram(shaderDirectory);
        for (const fs::path& path : FindObjFiles(modelDirectory)) {
            Model model;
            std::string error;
            if (LoadObj(path, model, error)) {
                size_t loadedVertices = 0;
                for (const MeshPart& part : model.parts) loadedVertices += static_cast<size_t>(part.vertexCount);
                gModels.push_back(std::move(model));
                std::cout << "Loaded: " << path.string() << " (" << loadedVertices << " vertices)\n";
            }
            else {
                std::cerr << "Skipped: " << path.string() << " - " << error << '\n';
            }
        }
        UpdateWindowTitle(window);
        if (gModels.empty()) std::cerr << "No valid OBJ files found under: " << modelDirectory.string() << '\n';

        float previousTime = static_cast<float>(glfwGetTime());
        while (!glfwWindowShouldClose(window)) {
            const float now = static_cast<float>(glfwGetTime());
            const float deltaTime = std::min(now - previousTime, 0.05f);
            previousTime = now;
            ProcessMovement(window, deltaTime);

            glClearColor(0.055f, 0.070f, 0.10f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            if (!gModels.empty()) {
                glUseProgram(program);
                const float aspect = static_cast<float>(gWindowWidth) / static_cast<float>(gWindowHeight);
                const glm::mat4 projection = glm::perspective(glm::radians(55.0f), aspect, 0.05f, 250.0f);
                const glm::mat4 view = gCamera.View();
                const glm::mat4 modelMatrix(1.0f);
                glUniformMatrix4fv(glGetUniformLocation(program, "uProjection"), 1, GL_FALSE, glm::value_ptr(projection));
                glUniformMatrix4fv(glGetUniformLocation(program, "uView"), 1, GL_FALSE, glm::value_ptr(view));
                glUniformMatrix4fv(glGetUniformLocation(program, "uModel"), 1, GL_FALSE, glm::value_ptr(modelMatrix));
                glUniform3fv(glGetUniformLocation(program, "uLightDirection"), 1, glm::value_ptr(glm::normalize(glm::vec3(-0.5f, -1.0f, -0.6f))));
                glUniform1i(glGetUniformLocation(program, "uTexture"), 0);
                for (const MeshPart& part : gModels[gCurrentModel].parts) {
                    glUniform3fv(glGetUniformLocation(program, "uColor"), 1, glm::value_ptr(part.color));
                    glUniform1i(glGetUniformLocation(program, "uUseTexture"), part.texture != 0 ? GL_TRUE : GL_FALSE);
                    glActiveTexture(GL_TEXTURE0);
                    glBindTexture(GL_TEXTURE_2D, part.texture);
                    glBindVertexArray(part.vao);
                    glDrawArrays(GL_TRIANGLES, 0, part.vertexCount);
                }
                glBindVertexArray(0);
            }

            glfwSwapBuffers(window);
            glfwPollEvents();
        }

        Cleanup();
        glDeleteProgram(program);
        glfwDestroyWindow(window);
        glfwTerminate();
        gWicFactory.Reset();
        CoUninitialize();
        return 0;
    }
    catch (const std::exception& exception) {
        std::cerr << "Object Viewer error: " << exception.what() << '\n';
        glfwTerminate();
        return 1;
    }
}
