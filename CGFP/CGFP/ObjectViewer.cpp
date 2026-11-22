// glfw3.h가 APIENTRY를 다시 정의하지 않도록 Windows.h를 먼저 포함한다(경고 C4005).
#define NOMINMAX
#include <Windows.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <GL/glew.h>
#include <GL/glfw3.h>
#include <GL/glm/glm.hpp>
#include <GL/glm/gtc/matrix_transform.hpp>
#include <GL/glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <charconv>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <deque>
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <limits>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
#include <cwctype>

namespace fs = std::filesystem;

namespace {
constexpr int kWindowWidth = 1440;
constexpr int kWindowHeight = 900;
constexpr float kMoveSpeed = 5.0f;
constexpr float kMapMoveSpeed = 2.5f;
constexpr float kInteriorMapDisplayScale = 600.0f;
constexpr float kMouseSensitivity = 0.05f;
constexpr float kPi = 3.14159265358979323846f;
constexpr size_t kNoModel = static_cast<size_t>(-1);

struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 uv;
};
static_assert(sizeof(Vertex) == 32, "Vertex must be tightly packed (it is hashed and cached as raw bytes)");

// ---------------------------------------------------------------------------------------------
// 로더 스레드와 메인 스레드 사이에 전달하는 데이터
// ---------------------------------------------------------------------------------------------

// 재질 하나의 메시 데이터. 동일한 정점은 한 번만 저장하고 인덱스로 참조한다.
struct CpuPart {
    std::string materialName;
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
};

// OBJ에서 읽은 전체 데이터. 디스크 캐시에도 같은 내용을 저장한다.
struct CpuMesh {
    std::vector<std::string> materialLibraries;
    std::vector<CpuPart> parts;
    float displayScale = 1.0f;
    float cameraMoveSpeed = kMoveSpeed;
    glm::vec3 cameraStartPosition{ 0.0f, 0.0f, 4.0f };
    float cameraPitch = 0.0f;
};

struct DecodedImage {
    UINT width = 0;
    UINT height = 0;
    std::vector<BYTE> pixels; // RGBA8, 맨 아래 행부터 저장(OpenGL 순서)
    std::string error;
};

struct DecodedTexture {
    std::string key;
    DecodedImage image;
};

// 컨텍스트 사이에 공유할 수 있는 OpenGL 객체(버퍼와 텍스처, VAO는 제외).
struct GpuPart {
    bool cullBackFaces = false;
    GLuint vbo = 0;
    GLuint ebo = 0;
    GLuint texture = 0;
    GLsizei indexCount = 0;
    glm::vec3 color{ 0.8f, 0.8f, 0.8f };
    glm::vec3 center{ 0.0f };
    float opacity = 1.0f;
    bool transparent = false;
};

struct LoadResult {
    int modelId = -1;
    bool ok = false;
    std::string error;
    std::vector<std::string> warnings;
    bool fromCache = false;
    double milliseconds = 0.0;
    size_t indexCount = 0;
    size_t uniqueVertexCount = 0;
    CpuMesh mesh;                            // GPU에 올리기 전까지 보관하는 메시 데이터
    std::vector<glm::vec3> partColors;        // mesh.parts와 같은 순서
    std::vector<float> partOpacities;         // mesh.parts와 같은 순서
    std::vector<uint8_t> partTransparencyFlags; // mesh.parts와 같은 순서
    std::vector<std::string> partTextureKeys; // mesh.parts와 같은 순서(빈 문자열은 텍스처 없음)
    std::vector<DecodedTexture> newTextures; // 디코딩했지만 아직 GPU에 올리지 않은 텍스처
    bool uploaded = false;
    std::vector<GpuPart> gpuParts;
};

// ---------------------------------------------------------------------------------------------
// 메인 스레드 상태
// ---------------------------------------------------------------------------------------------

struct MeshPart {
    bool cullBackFaces = false;
    GLuint vao = 0;
    GLuint vbo = 0;
    GLuint ebo = 0;
    GLuint texture = 0;
    GLsizei indexCount = 0;
    glm::vec3 color{ 0.8f, 0.8f, 0.8f };
    glm::vec3 center{ 0.0f };
    float opacity = 1.0f;
    bool transparent = false;
};

struct Model {
    int id = -1;
    fs::path path;
    std::vector<MeshPart> parts;
    float displayScale = 1.0f;
    float cameraMoveSpeed = kMoveSpeed;
    glm::vec3 cameraStartPosition{ 0.0f, 0.0f, 4.0f };
    float cameraPitch = 0.0f;
    bool loaded = false;
    bool requested = false; // 로더 스레드에 요청을 전달했는지 여부
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

struct Uniforms {
    GLint projection = -1;
    GLint view = -1;
    GLint model = -1;
    GLint lightDirection = -1;
    GLint referenceLighting = -1;
    GLint opacity = -1;
    GLint texture = -1;
    GLint color = -1;
    GLint useTexture = -1;
};

Camera gCamera;
bool gFirstMouse = true;
bool gCursorCaptured = true;
double gLastMouseX = kWindowWidth * 0.5;
double gLastMouseY = kWindowHeight * 0.5;
std::vector<Model> gModels;
int gTargetModelId = -1; // 사용자가 선택한 모델(아직 로딩 중일 수 있음)
int gShownModelId = -1;  // 현재 그리는 모델(선택 모델을 로딩하는 동안에도 표시)
int gTargetDirection = 1;
int gWindowWidth = kWindowWidth;
int gWindowHeight = kWindowHeight;
Microsoft::WRL::ComPtr<IWICImagingFactory> gWicFactory;
// 경로별 텍스처 객체. 공유 업로드 컨텍스트가 있으면 로더 스레드가, 없으면 메인 스레드가 생성한다.
// 메인 스레드는 로더 스레드가 멈춘 뒤에만 이 목록을 읽는다.
std::unordered_map<std::string, GLuint> gTextureCache;
std::atomic<bool> gQuit{ false };

// ---------------------------------------------------------------------------------------------
// 셰이더
// ---------------------------------------------------------------------------------------------

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

Uniforms FindUniforms(GLuint program) {
    Uniforms uniforms;
    uniforms.projection = glGetUniformLocation(program, "uProjection");
    uniforms.view = glGetUniformLocation(program, "uView");
    uniforms.model = glGetUniformLocation(program, "uModel");
    uniforms.lightDirection = glGetUniformLocation(program, "uLightDirection");
    uniforms.referenceLighting = glGetUniformLocation(program, "uReferenceLighting");
    uniforms.opacity = glGetUniformLocation(program, "uOpacity");
    uniforms.texture = glGetUniformLocation(program, "uTexture");
    uniforms.color = glGetUniformLocation(program, "uColor");
    uniforms.useTexture = glGetUniformLocation(program, "uUseTexture");
    return uniforms;
}

// ---------------------------------------------------------------------------------------------
// 빠른 OBJ 텍스트 처리 함수([begin, end) 포인터 범위에서 할당 없이 동작)
// ---------------------------------------------------------------------------------------------

inline bool IsBlank(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\v' || c == '\f';
}

inline const char* SkipBlanks(const char* p, const char* end) {
    while (p < end && IsBlank(*p)) ++p;
    return p;
}

inline const char* SkipToken(const char* p, const char* end) {
    while (p < end && !IsBlank(*p)) ++p;
    return p;
}

// stream 입력처럼 공백을 건너뛰고 앞의 '+'도 허용하며 실수 하나를 읽는다.
bool ParseFloat(const char*& p, const char* end, float& value) {
    p = SkipBlanks(p, end);
    const char* start = p;
    if (start < end && *start == '+') ++start;
    const auto result = std::from_chars(start, end, value, std::chars_format::general);
    if (result.ec == std::errc::result_out_of_range) value = 0.0f;
    else if (result.ec != std::errc()) return false;
    p = result.ptr;
    return true;
}

bool ParseVec(const char* p, const char* end, float* out, int count) {
    for (int i = 0; i < count; ++i) {
        if (!ParseFloat(p, end, out[i])) return false;
    }
    return true;
}

std::string LineError(const fs::path& path, int lineNumber, const char* message) {
    return path.string() + ":" + std::to_string(lineNumber) + " " + message;
}

// OBJ 인덱스(1부터 시작하거나 끝에서부터 세는 음수)를 0부터 시작하는 인덱스로 바꾼다.
int ParseIndex(const char* begin, const char* end, size_t count, const fs::path& path, int lineNumber) {
    if (begin == end) return -1;
    if (*begin == '+') ++begin;
    int raw = 0;
    const auto result = std::from_chars(begin, end, raw);
    if (result.ec != std::errc()) throw std::runtime_error(LineError(path, lineNumber, "잘못된 OBJ 인덱스입니다."));
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

bool ReadWholeFile(const fs::path& path, std::string& contents) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) return false;
    const std::streamoff size = file.tellg();
    if (size < 0) return false;
    contents.resize(static_cast<size_t>(size));
    file.seekg(0, std::ios::beg);
    return size == 0 || static_cast<bool>(file.read(contents.data(), size));
}

// ---------------------------------------------------------------------------------------------
// 재질
// ---------------------------------------------------------------------------------------------

struct Material {
    glm::vec3 color{ 0.8f, 0.8f, 0.8f };
    float opacity = 1.0f;
    bool supportsTransparency = false;
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
            const std::string materialName = Trim(name);
            current = &materials[materialName];
            // 원본 MTL은 불투명한 벽 덧씌우기 재질에도 낮은 d 값을 지정한다.
            // 벽 전체를 반투명으로 그리면 구멍처럼 보이므로 유리 재질만 혼합해서 그린다.
            current->supportsTransparency = materialName.find("Glass") != std::string::npos ||
                materialName.find("glass") != std::string::npos;
        }
        else if (current && tag == "Kd") {
            stream >> current->color.r >> current->color.g >> current->color.b;
        }
        else if (current && tag == "d") {
            float opacity = 1.0f;
            stream >> opacity;
            if (current->supportsTransparency) current->opacity = opacity;
        }
        else if (current && tag == "Tr") {
            float transparency = 0.0f;
            stream >> transparency;
            if (current->supportsTransparency) current->opacity = 1.0f - transparency;
        }
        else if (current && tag == "map_d" && current->supportsTransparency) {
            // Glass1_diff의 알파 채널은 전체가 반투명하다. 이 재질만 혼합 단계에서 그리고,
            // 일반 map_d 텍스처 때문에 벽·바닥·소품이 투명해지지 않게 한다.
            current->opacity = std::min(current->opacity, 0.65f);
        }
        else if (current && (tag == "map_Kd" || tag == "map_kd")) {
            std::string filename;
            std::getline(stream, filename);
            filename = Trim(filename);
            // 제공된 OBJ 재질은 별도 옵션 없이 map_Kd 경로를 지정한다.
            current->diffuseTexture = path.parent_path() / fs::path(filename);
        }
    }
}

// OBJ 전체를 파싱하지 않고 mtllib 줄만 찾는다. 앞쪽 공백을 허용하고,
// 태그를 독립된 단어로 확인한 뒤 나머지 내용을 공백 제거해 사용한다.
std::vector<std::string> FindMaterialLibraries(const std::string& contents) {
    std::vector<std::string> libraries;
    const char* const begin = contents.data();
    const char* const fileEnd = begin + contents.size();
    for (size_t at = contents.find("mtllib"); at != std::string::npos; at = contents.find("mtllib", at + 6)) {
        const char* tag = begin + at;
        const char* lineStart = tag;
        while (lineStart > begin && IsBlank(lineStart[-1])) --lineStart;
        if (lineStart != begin && lineStart[-1] != '\n') continue;
        const char* rest = tag + 6;
        if (rest < fileEnd && !IsBlank(*rest) && *rest != '\n') continue;
        const char* lineEnd = static_cast<const char*>(std::memchr(rest, '\n', static_cast<size_t>(fileEnd - rest)));
        if (!lineEnd) lineEnd = fileEnd;
        std::string library = Trim(std::string(rest, lineEnd));
        if (!library.empty()) libraries.push_back(std::move(library));
    }
    return libraries;
}

MaterialMap LoadMaterials(const fs::path& objPath, const std::vector<std::string>& materialLibraries) {
    MaterialMap materials;
    bool loadedLibrary = false;
    for (const std::string& library : materialLibraries) {
        const fs::path materialPath = objPath.parent_path() / fs::path(library);
        if (fs::exists(materialPath)) {
            ReadMaterialLibrary(materialPath, materials);
            loadedLibrary = true;
        }
    }
    // 일부 모델 묶음은 OBJ를 내보낸 뒤 MTL 파일 이름을 바꾼다.
    if (!loadedLibrary && fs::exists(objPath.parent_path())) {
        for (const auto& entry : fs::directory_iterator(objPath.parent_path())) {
            if (entry.is_regular_file() && entry.path().extension() == ".mtl") ReadMaterialLibrary(entry.path(), materials);
        }
    }
    return materials;
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

// ---------------------------------------------------------------------------------------------
// 텍스처(아무 스레드에서나 디코딩하고, OpenGL 컨텍스트가 있는 스레드에서 GPU에 올린다)
// ---------------------------------------------------------------------------------------------

std::string TextureCacheKey(const fs::path& path) {
    return path.lexically_normal().string();
}

// WIC로 PNG/JPEG를 CPU에서 디코딩한다. WIC 팩터리는 스레드 간 사용이 가능하다.
DecodedImage DecodeImage(const fs::path& path) {
    DecodedImage image;
    Microsoft::WRL::ComPtr<IWICBitmapDecoder> decoder;
    HRESULT result = gWicFactory->CreateDecoderFromFilename(
        path.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand, decoder.GetAddressOf());
    if (FAILED(result)) {
        std::ostringstream message;
        message << "Texture decode failed: " << path.string() << " (HRESULT 0x" << std::hex << result << ")";
        image.error = message.str();
        return image;
    }

    Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> frame;
    if (FAILED(decoder->GetFrame(0, frame.GetAddressOf()))) return image;
    UINT width = 0, height = 0;
    if (FAILED(frame->GetSize(&width, &height)) || width == 0 || height == 0) return image;

    Microsoft::WRL::ComPtr<IWICFormatConverter> converter;
    if (FAILED(gWicFactory->CreateFormatConverter(converter.GetAddressOf()))) return image;
    if (FAILED(converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppRGBA,
        WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom))) return image;

    // WIC의 첫 행은 이미지 위쪽이고 OpenGL UV의 원점은 아래쪽이다.
    // 이미지를 다시 복사하지 않도록 WIC가 디코딩하면서 상하를 뒤집게 한다.
    Microsoft::WRL::ComPtr<IWICBitmapFlipRotator> flipper;
    if (FAILED(gWicFactory->CreateBitmapFlipRotator(flipper.GetAddressOf()))) return image;
    if (FAILED(flipper->Initialize(converter.Get(), WICBitmapTransformFlipVertical))) return image;

    const UINT stride = width * 4;
    image.pixels.resize(static_cast<size_t>(stride) * height);
    if (FAILED(flipper->CopyPixels(nullptr, stride, static_cast<UINT>(image.pixels.size()), image.pixels.data()))) {
        image.pixels.clear();
        return image;
    }
    image.width = width;
    image.height = height;
    return image;
}

size_t HardwareThreads() {
    return std::max<size_t>(std::thread::hardware_concurrency(), 1);
}

// 호출 스레드를 포함해 최대 maxThreads개 스레드에서 경로의 이미지를 디코딩한다(OpenGL 사용 안 함).
std::vector<DecodedImage> DecodeImagesParallel(const std::vector<fs::path>& paths, size_t maxThreads) {
    std::vector<DecodedImage> images(paths.size());
    if (paths.empty()) return images;
    const size_t threadCount = std::min(paths.size(), std::max<size_t>(maxThreads, 1));
    std::atomic<size_t> next{ 0 };
    const auto work = [&]() {
        // WIC 객체를 이 스레드 안에서만 사용하도록 MTA를 명시적으로 초기화한다.
        const HRESULT com = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        for (size_t i = next++; i < paths.size(); i = next++) images[i] = DecodeImage(paths[i]);
        if (SUCCEEDED(com)) CoUninitialize();
    };
    std::vector<std::thread> workers;
    workers.reserve(threadCount - 1);
    for (size_t i = 1; i < threadCount; ++i) workers.emplace_back(work);
    work();
    for (std::thread& worker : workers) worker.join();
    return images;
}

// 존재하면서 아직 예약하지 않은 텍스처 경로를 pending에 넣고 예약한다.
// 여러 모델이 공유하는 텍스처도 한 번만 디코딩한다.
void ClaimTextures(const std::vector<fs::path>& paths, std::unordered_set<std::string>& claimed,
    std::vector<fs::path>& pending, std::vector<std::string>& keys) {
    for (const fs::path& path : paths) {
        if (path.empty() || !fs::exists(path)) continue;
        std::string key = TextureCacheKey(path);
        if (!claimed.insert(key).second) continue;
        pending.push_back(path);
        keys.push_back(std::move(key));
    }
}

GLuint UploadTexture(const DecodedImage& image) {
    if (image.pixels.empty()) return 0;
    GLuint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, static_cast<GLsizei>(image.width), static_cast<GLsizei>(image.height),
        0, GL_RGBA, GL_UNSIGNED_BYTE, image.pixels.data());
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glBindTexture(GL_TEXTURE_2D, 0);
    return texture;
}

// ---------------------------------------------------------------------------------------------
// 인덱싱(동일한 정점을 한 번만 저장하고 인덱스 버퍼로 참조)
// ---------------------------------------------------------------------------------------------

struct VertexHash {
    size_t operator()(const Vertex& vertex) const noexcept {
        uint32_t words[8];
        std::memcpy(words, &vertex, sizeof(words));
        uint64_t hash = 1469598103934665603ULL;
        for (uint32_t word : words) {
            hash ^= word;
            hash *= 1099511628211ULL;
            hash ^= hash >> 29;
        }
        return static_cast<size_t>(hash);
    }
};

struct VertexEqual {
    bool operator()(const Vertex& a, const Vertex& b) const noexcept {
        return std::memcmp(&a, &b, sizeof(Vertex)) == 0;
    }
};

void BuildIndexedPart(const std::vector<Vertex>& triangles, CpuPart& part) {
    std::unordered_map<Vertex, uint32_t, VertexHash, VertexEqual> lookup;
    lookup.reserve(triangles.size());
    part.vertices.reserve(triangles.size() / 2);
    part.indices.reserve(triangles.size());
    for (const Vertex& vertex : triangles) {
        const auto inserted = lookup.try_emplace(vertex, static_cast<uint32_t>(part.vertices.size()));
        if (inserted.second) part.vertices.push_back(vertex);
        part.indices.push_back(inserted.first->second);
    }
    part.vertices.shrink_to_fit();
}

// ---------------------------------------------------------------------------------------------
// OBJ 파싱
// ---------------------------------------------------------------------------------------------

void ParseObj(const fs::path& path, const std::string& contents, CpuMesh& mesh) {
    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> normals;
    std::vector<glm::vec2> texcoords;
    std::unordered_map<std::string, std::vector<Vertex>> materialVertices;
    std::string currentMaterial = "__default";
    // 현재 재질의 정점 목록은 usemtl에서 갱신하고 실제 면이 처음 나타날 때 생성한다.
    // 빈 __default 파트가 생기지 않으며 unordered_map은 재해시 후에도 값 주소를 유지한다.
    std::vector<Vertex>* currentBucket = nullptr;
    glm::vec3 minimum(std::numeric_limits<float>::max());
    glm::vec3 maximum(std::numeric_limits<float>::lowest());
    int lineNumber = 0;

    struct FaceIndex { int position; int texcoord; int normal; };
    std::vector<FaceIndex> face; // 모든 면을 파싱할 때 재사용
    face.reserve(8);

    const char* cursor = contents.data();
    const char* const fileEnd = cursor + contents.size();
    while (cursor < fileEnd) {
        const char* lineEnd = static_cast<const char*>(std::memchr(cursor, '\n', static_cast<size_t>(fileEnd - cursor)));
        if (!lineEnd) lineEnd = fileEnd;
        const char* p = cursor;
        cursor = lineEnd < fileEnd ? lineEnd + 1 : fileEnd;
        ++lineNumber;
        if ((lineNumber & 0xFFFF) == 0 && gQuit.load(std::memory_order_relaxed)) throw std::runtime_error("cancelled");
        const char* end = lineEnd;
        while (end > p && end[-1] == '\r') --end;

        p = SkipBlanks(p, end);
        const char* tagEnd = SkipToken(p, end);
        const size_t tagLength = static_cast<size_t>(tagEnd - p);
        if (tagLength == 0 || *p == '#') continue;
        const char* rest = tagEnd;

        if (tagLength == 1 && p[0] == 'v') {
            glm::vec3 position;
            if (!ParseVec(rest, end, &position.x, 3)) continue;
            positions.push_back(position);
            minimum = glm::min(minimum, position);
            maximum = glm::max(maximum, position);
        }
        else if (tagLength == 2 && p[0] == 'v' && p[1] == 't') {
            glm::vec2 uv;
            if (ParseVec(rest, end, &uv.x, 2)) texcoords.push_back(uv);
        }
        else if (tagLength == 2 && p[0] == 'v' && p[1] == 'n') {
            glm::vec3 normal;
            if (ParseVec(rest, end, &normal.x, 3)) normals.push_back(glm::normalize(normal));
        }
        // mtllib 줄은 FindMaterialLibraries에서 처리한다.
        else if (tagLength == 6 && std::memcmp(p, "usemtl", 6) == 0) {
            currentMaterial = Trim(std::string(rest, end));
            currentBucket = nullptr;
        }
        else if (tagLength == 1 && p[0] == 'f') {
            face.clear();
            const char* item = SkipBlanks(rest, end);
            while (item < end) {
                const char* itemEnd = SkipToken(item, end);
                // v/vt/vn을 나누고 세 번째 슬래시 이후 내용은 무시한다.
                const char* partBegin[3] = { item, item, item };
                const char* partEnd[3] = { item, item, item };
                int part = 0;
                const char* start = item;
                for (const char* c = item; ; ++c) {
                    if (c == itemEnd || *c == '/') {
                        if (part < 3) {
                            partBegin[part] = start;
                            partEnd[part] = c;
                            ++part;
                        }
                        start = c + 1;
                        if (c == itemEnd) break;
                    }
                }
                if (partBegin[0] != partEnd[0]) {
                    face.push_back({ ParseIndex(partBegin[0], partEnd[0], positions.size(), path, lineNumber),
                                     ParseIndex(partBegin[1], partEnd[1], texcoords.size(), path, lineNumber),
                                     ParseIndex(partBegin[2], partEnd[2], normals.size(), path, lineNumber) });
                }
                item = SkipBlanks(itemEnd, end);
            }
            if (face.size() < 3) continue;
            if (!currentBucket) currentBucket = &materialVertices[currentMaterial];

            // 다각형을 부채꼴 모양의 삼각형으로 나눈다. 노말이 없으면 평면 노말을 만든다.
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
                    currentBucket->push_back({ positions[index.position], normal, uv });
                }
            }
        }
    }

    size_t vertexCount = 0;
    for (const auto& pair : materialVertices) vertexCount += pair.second.size();
    if (vertexCount == 0) throw std::runtime_error("OBJ에 표시할 유효한 면(face)이 없습니다.");

    // 모델을 중앙에 놓고 가장 긴 축을 기준으로 크기를 맞춘다.
    const glm::vec3 center = (minimum + maximum) * 0.5f;
    const float largestDimension = std::max({ maximum.x - minimum.x, maximum.y - minimum.y, maximum.z - minimum.z });
    const float normalizeScale = largestDimension > 0.00001f ? 2.0f / largestDimension : 1.0f;
    const bool isOriginalRestaurantMap = path.filename() == "BFBBR - Krusty Krab.obj";
    const bool isInteriorMap = path.filename() == "Krusty_Krab_Interior.obj";
    const bool isRestaurantMap = isOriginalRestaurantMap || isInteriorMap;
    // 원본에는 넓은 주변 배경이 포함되어 있으므로 실내를 보기 좋게 식당 크기를 조정한다.
    mesh.displayScale = isInteriorMap ? kInteriorMapDisplayScale : (isOriginalRestaurantMap ? 96.0f : 1.0f);
    mesh.cameraMoveSpeed = isRestaurantMap ? kMapMoveSpeed : kMoveSpeed;
    mesh.cameraStartPosition = glm::vec3(0.0f, 0.0f, 4.0f);
    mesh.cameraPitch = 0.0f;
    if (isOriginalRestaurantMap) {
        // 식당 메시가 OBJ 원본 좌표계에서 이 지점 주변에 모여 있다.
        const glm::vec3 restaurantInterior(0.73f, 0.30f, -1.40f);
        mesh.cameraStartPosition = (restaurantInterior - center) * normalizeScale * mesh.displayScale;
    }
    else if (isInteriorMap) {
        // 식당 내부에서 정문(-Z)을 바라보며 시작한다.
        const glm::vec3 restaurantInterior(-2575.0f, -117.0f, 2588.0f);
        mesh.cameraStartPosition = (restaurantInterior - center) * normalizeScale * mesh.displayScale;
    }

    for (auto& pair : materialVertices) {
        CpuPart part;
        part.materialName = pair.first;
        BuildIndexedPart(pair.second, part);
        std::vector<Vertex>().swap(pair.second); // 삼각형 목록 메모리를 일찍 해제한다
        for (Vertex& vertex : part.vertices) vertex.position = (vertex.position - center) * normalizeScale * mesh.displayScale;
        mesh.parts.push_back(std::move(part));
    }
}

// ---------------------------------------------------------------------------------------------
// 바이너리 메시 캐시(%LOCALAPPDATA%\CGFP-ObjectViewer\cache).
// OBJ의 크기와 수정 시간이 같을 때만 사용하며, 다르면 OBJ를 다시 읽어 캐시를 갱신한다.
// ---------------------------------------------------------------------------------------------

constexpr uint32_t kCacheMagic = 0x3143564F; // "OVC1"
constexpr uint32_t kCacheVersion = 1;        // 파싱 또는 배율 규칙이 바뀌면 올린다

struct SourceStamp {
    uint64_t size = 0;
    int64_t modified = 0;
};

bool GetSourceStamp(const fs::path& path, SourceStamp& stamp) {
    std::error_code error;
    const uintmax_t size = fs::file_size(path, error);
    if (error) return false;
    const fs::file_time_type modified = fs::last_write_time(path, error);
    if (error) return false;
    stamp.size = static_cast<uint64_t>(size);
    stamp.modified = static_cast<int64_t>(modified.time_since_epoch().count());
    return true;
}

const fs::path& CacheDirectory() {
    static const fs::path directory = [] {
        wchar_t buffer[MAX_PATH];
        const DWORD length = GetEnvironmentVariableW(L"LOCALAPPDATA", buffer, MAX_PATH);
        fs::path base;
        if (length > 0 && length < MAX_PATH) base = fs::path(buffer);
        else {
            std::error_code error;
            base = fs::temp_directory_path(error);
        }
        return base / "CGFP-ObjectViewer" / "cache";
    }();
    return directory;
}

fs::path CachePathFor(const fs::path& objPath) {
    std::wstring key = fs::absolute(objPath).lexically_normal().wstring();
    if (objPath.filename() == L"Krusty_Krab_Interior.obj") {
        // 맵 표시 배율이 바뀌면 캐시의 정점 좌표도 새 배율에 맞춰 갱신한다.
        key += L"|viewer-scale-";
        key += std::to_wstring(static_cast<int>(kInteriorMapDisplayScale));
    }
    std::transform(key.begin(), key.end(), key.begin(), [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });
    uint64_t hash = 1469598103934665603ULL;
    for (wchar_t c : key) {
        hash ^= static_cast<uint64_t>(c);
        hash *= 1099511628211ULL;
    }
    wchar_t hex[17];
    swprintf_s(hex, L"%016llx", static_cast<unsigned long long>(hash));
    return CacheDirectory() / (objPath.stem().wstring() + L"-" + hex + L".ovcache");
}

class ByteWriter {
public:
    template <typename T> void Put(const T& value) { Bytes(&value, sizeof(T)); }
    void Bytes(const void* data, size_t size) {
        const char* bytes = static_cast<const char*>(data);
        buffer_.insert(buffer_.end(), bytes, bytes + size);
    }
    void String(const std::string& value) {
        Put(static_cast<uint32_t>(value.size()));
        Bytes(value.data(), value.size());
    }
    const std::string& Buffer() const { return buffer_; }
    void Reserve(size_t size) { buffer_.reserve(size); }
private:
    std::string buffer_;
};

class ByteReader {
public:
    explicit ByteReader(const std::string& data) : p_(data.data()), end_(data.data() + data.size()) {}
    template <typename T> bool Get(T& value) { return Bytes(&value, sizeof(T)); }
    bool Bytes(void* out, size_t size) {
        if (static_cast<size_t>(end_ - p_) < size) return false;
        std::memcpy(out, p_, size);
        p_ += size;
        return true;
    }
    bool String(std::string& value) {
        uint32_t size = 0;
        if (!Get(size) || static_cast<size_t>(end_ - p_) < size) return false;
        value.assign(p_, size);
        p_ += size;
        return true;
    }
    template <typename T> bool Array(std::vector<T>& values, uint32_t count) {
        if (static_cast<size_t>(end_ - p_) / sizeof(T) < count) return false;
        values.resize(count);
        return Bytes(values.data(), static_cast<size_t>(count) * sizeof(T));
    }
    bool AtEnd() const { return p_ == end_; }
private:
    const char* p_;
    const char* end_;
};

bool ReadMeshCache(const fs::path& cachePath, const SourceStamp& stamp, CpuMesh& mesh) {
    std::string data;
    if (!ReadWholeFile(cachePath, data)) return false;
    ByteReader reader(data);
    uint32_t magic = 0, version = 0;
    SourceStamp cached;
    if (!reader.Get(magic) || magic != kCacheMagic || !reader.Get(version) || version != kCacheVersion) return false;
    if (!reader.Get(cached.size) || !reader.Get(cached.modified)) return false;
    if (cached.size != stamp.size || cached.modified != stamp.modified) return false;

    CpuMesh loaded;
    uint32_t libraryCount = 0;
    if (!reader.Get(libraryCount)) return false;
    for (uint32_t i = 0; i < libraryCount; ++i) {
        std::string library;
        if (!reader.String(library)) return false;
        loaded.materialLibraries.push_back(std::move(library));
    }
    if (!reader.Get(loaded.displayScale) || !reader.Get(loaded.cameraMoveSpeed) ||
        !reader.Get(loaded.cameraStartPosition) || !reader.Get(loaded.cameraPitch)) return false;
    uint32_t partCount = 0;
    if (!reader.Get(partCount)) return false;
    for (uint32_t i = 0; i < partCount; ++i) {
        CpuPart part;
        uint32_t vertexCount = 0, indexCount = 0;
        if (!reader.String(part.materialName) || !reader.Get(vertexCount) || !reader.Get(indexCount)) return false;
        if (!reader.Array(part.vertices, vertexCount) || !reader.Array(part.indices, indexCount)) return false;
        for (uint32_t index : part.indices) {
            if (index >= vertexCount) return false;
        }
        loaded.parts.push_back(std::move(part));
    }
    if (!reader.AtEnd() || loaded.parts.empty()) return false;
    mesh = std::move(loaded);
    return true;
}

void WriteMeshCache(const fs::path& cachePath, const SourceStamp& stamp, const CpuMesh& mesh) {
    ByteWriter writer;
    size_t estimate = 256;
    for (const CpuPart& part : mesh.parts) estimate += part.vertices.size() * sizeof(Vertex) + part.indices.size() * 4 + 64;
    writer.Reserve(estimate);
    writer.Put(kCacheMagic);
    writer.Put(kCacheVersion);
    writer.Put(stamp.size);
    writer.Put(stamp.modified);
    writer.Put(static_cast<uint32_t>(mesh.materialLibraries.size()));
    for (const std::string& library : mesh.materialLibraries) writer.String(library);
    writer.Put(mesh.displayScale);
    writer.Put(mesh.cameraMoveSpeed);
    writer.Put(mesh.cameraStartPosition);
    writer.Put(mesh.cameraPitch);
    writer.Put(static_cast<uint32_t>(mesh.parts.size()));
    for (const CpuPart& part : mesh.parts) {
        writer.String(part.materialName);
        writer.Put(static_cast<uint32_t>(part.vertices.size()));
        writer.Put(static_cast<uint32_t>(part.indices.size()));
        writer.Bytes(part.vertices.data(), part.vertices.size() * sizeof(Vertex));
        writer.Bytes(part.indices.data(), part.indices.size() * sizeof(uint32_t));
    }

    // 중간에 종료되어도 불완전한 캐시가 남지 않도록 임시 파일에 먼저 쓴다.
    std::error_code error;
    fs::create_directories(cachePath.parent_path(), error);
    fs::path temporary = cachePath;
    temporary += L".tmp";
    {
        std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
        if (!file) return;
        file.write(writer.Buffer().data(), static_cast<std::streamsize>(writer.Buffer().size()));
        if (!file) {
            file.close();
            fs::remove(temporary, error);
            return;
        }
    }
    fs::rename(temporary, cachePath, error);
    if (error) fs::remove(temporary, error);
}

// ---------------------------------------------------------------------------------------------
// 로더 스레드에서 모델 하나를 불러온다(CPU 작업 후 가능하면 GPU에 업로드)
// ---------------------------------------------------------------------------------------------

void LoadModelCpu(const fs::path& path, std::unordered_set<std::string>& claimedTextures,
    std::vector<std::string>& claimedHere, LoadResult& result) {
    CpuMesh& mesh = result.mesh;
    SourceStamp stamp;
    const bool haveStamp = GetSourceStamp(path, stamp);
    const fs::path cachePath = haveStamp ? CachePathFor(path) : fs::path();
    result.fromCache = haveStamp && ReadMeshCache(cachePath, stamp, mesh);

    std::string contents;
    if (!result.fromCache) {
        // 파일 전체를 한 번에 읽고 getline이나 istringstream 대신 포인터로 순회한다.
        if (!ReadWholeFile(path, contents)) throw std::runtime_error("파일을 열 수 없습니다.");
        mesh.materialLibraries = FindMaterialLibraries(contents);
    }

    // 이 스레드가 OBJ를 파싱하는 동안 다른 스레드에서 텍스처를 디코딩하도록 MTL을 먼저 읽는다.
    // 사용할 수 있는 재질 라이브러리가 전혀 없을 때만 폴더의 기본 텍스처를 사용한다.
    // 텍스처가 지정되지 않은 모든 재질에 임의의 이미지를 입히면 다른 면까지 잘못 칠해지고,
    // 원본에서 의도적으로 텍스처가 없는 부분도 구분할 수 없게 된다.
    const MaterialMap materials = LoadMaterials(path, mesh.materialLibraries);
    bool fallbackResolved = false;
    fs::path fallbackTexture;
    std::vector<fs::path> earlyTextures;
    std::vector<std::string> earlyKeys;
    {
        std::vector<fs::path> diffuseTextures;
        bool needsFallback = materials.empty();
        for (const auto& pair : materials) {
            const fs::path& texturePath = pair.second.diffuseTexture;
            if (!texturePath.empty() && !fs::exists(texturePath)) {
                std::cerr << "Missing diffuse texture for material '" << pair.first << "': "
                    << texturePath.string() << '\n';
            }
            else diffuseTextures.push_back(texturePath);
        }
        if (needsFallback) {
            fallbackTexture = FindFallbackTexture(path.parent_path());
            fallbackResolved = true;
            diffuseTextures.push_back(fallbackTexture);
        }
        ClaimTextures(diffuseTextures, claimedTextures, earlyTextures, earlyKeys);
        claimedHere.insert(claimedHere.end(), earlyKeys.begin(), earlyKeys.end());
    }
    // 파서가 사용할 코어 하나를 남긴다. 아래에서 예외가 발생해도 future 소멸자가 디코딩 종료를
    // 기다리므로 earlyTextures가 사라지기 전에 작업이 끝난다.
    std::future<std::vector<DecodedImage>> earlyDecode = std::async(std::launch::async, [&earlyTextures]() {
        return DecodeImagesParallel(earlyTextures, std::max<size_t>(HardwareThreads() - 1, 1));
    });

    if (!result.fromCache) {
        ParseObj(path, contents, mesh);
        std::string().swap(contents);
        if (haveStamp) WriteMeshCache(cachePath, stamp, mesh);
    }

    // 각 파트의 색과 텍스처를 결정한다. 필요할 때만 폴더에서 기본 텍스처를 찾는다.
    std::vector<fs::path> partTextures;
    for (const CpuPart& part : mesh.parts) {
        Material material;
        const auto materialIt = materials.find(part.materialName);
        if (materialIt != materials.end()) material = materialIt->second;
        fs::path texturePath = material.diffuseTexture;
        // 내보내기 과정에서 map_Kd 연결이 빠져도 식탁의 원본 텍스처를 사용한다.
        if (part.materialName == "Kktable1Mtl" &&
            (texturePath.empty() || !fs::exists(texturePath))) {
            const fs::path tableAtlas = path.parent_path() / "Kktable1_diff.png";
            if (fs::exists(tableAtlas)) texturePath = tableAtlas;
        }
        if (materials.empty() && (texturePath.empty() || !fs::exists(texturePath))) {
            if (!fallbackResolved) {
                fallbackTexture = FindFallbackTexture(path.parent_path());
                fallbackResolved = true;
            }
            texturePath = fallbackTexture;
        }
        result.partColors.push_back(material.color);
        result.partOpacities.push_back(std::clamp(material.opacity, 0.0f, 1.0f));
        result.partTransparencyFlags.push_back(material.opacity < 0.999f ? 1 : 0);
        result.partTextureKeys.push_back(texturePath.empty() ? std::string() : TextureCacheKey(texturePath));
        partTextures.push_back(std::move(texturePath));
        result.indexCount += part.indices.size();
        result.uniqueVertexCount += part.vertices.size();
    }

    // 이 모델에서 쓰는 미리 디코딩한 텍스처만 남기고 나머지 예약은 해제한다.
    std::vector<DecodedImage> earlyImages = earlyDecode.get();
    for (size_t i = 0; i < earlyKeys.size(); ++i) {
        const bool used = std::find(result.partTextureKeys.begin(), result.partTextureKeys.end(), earlyKeys[i]) !=
            result.partTextureKeys.end();
        if (used) result.newTextures.push_back({ earlyKeys[i], std::move(earlyImages[i]) });
        else claimedTextures.erase(earlyKeys[i]);
    }
    // 파싱 전에는 알 수 없었던 텍스처를 디코딩한다.
    std::vector<fs::path> lateTextures;
    std::vector<std::string> lateKeys;
    ClaimTextures(partTextures, claimedTextures, lateTextures, lateKeys);
    claimedHere.insert(claimedHere.end(), lateKeys.begin(), lateKeys.end());
    std::vector<DecodedImage> lateImages = DecodeImagesParallel(lateTextures, HardwareThreads());
    for (size_t i = 0; i < lateKeys.size(); ++i) result.newTextures.push_back({ lateKeys[i], std::move(lateImages[i]) });

    for (const DecodedTexture& texture : result.newTextures) {
        if (!texture.image.error.empty()) result.warnings.push_back(texture.image.error);
    }
}

// 텍스처와 정점·인덱스 버퍼를 만든다. 로더의 공유 컨텍스트 또는 메인 OpenGL 컨텍스트가 필요하다.
void UploadResult(LoadResult& result) {
    for (const DecodedTexture& texture : result.newTextures) {
        if (!gTextureCache.count(texture.key)) gTextureCache.emplace(texture.key, UploadTexture(texture.image));
    }
    result.newTextures.clear();

    for (size_t i = 0; i < result.mesh.parts.size(); ++i) {
        const CpuPart& part = result.mesh.parts[i];
        GpuPart gpu;
        // 식탁에는 UV가 단색 영역에 모인 뒤집힌 외곽선 메시가 포함되어 있다.
        // 그 뒷면을 그리면 텍스처가 있는 식탁 윗면이 빨간 단색으로 덮인다.
        gpu.cullBackFaces = part.materialName == "Kktable1Mtl";
        gpu.color = result.partColors[i];
        gpu.opacity = result.partOpacities[i];
        gpu.transparent = result.partTransparencyFlags[i] != 0;
        glm::vec3 minimum(std::numeric_limits<float>::max());
        glm::vec3 maximum(std::numeric_limits<float>::lowest());
        for (const Vertex& vertex : part.vertices) {
            minimum = glm::min(minimum, vertex.position);
            maximum = glm::max(maximum, vertex.position);
        }
        gpu.center = (minimum + maximum) * 0.5f;
        const std::string& key = result.partTextureKeys[i];
        if (!key.empty()) {
            const auto found = gTextureCache.find(key);
            if (found != gTextureCache.end()) gpu.texture = found->second;
        }
        if (part.materialName == "Kktable1Mtl") {
            std::cerr << "Kktable1Mtl texture: " << (key.empty() ? "<none>" : key)
                << " (OpenGL id " << gpu.texture << ")\n";
        }
        gpu.indexCount = static_cast<GLsizei>(part.indices.size());
        // 두 버퍼 모두 GL_ARRAY_BUFFER로 채운다. 코어 프로파일에서 GL_ELEMENT_ARRAY_BUFFER를
        // 연결하려면 VAO가 필요하지만 VAO는 나중에 메인 컨텍스트에서 생성한다.
        glGenBuffers(1, &gpu.vbo);
        glBindBuffer(GL_ARRAY_BUFFER, gpu.vbo);
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(part.vertices.size() * sizeof(Vertex)), part.vertices.data(), GL_STATIC_DRAW);
        glGenBuffers(1, &gpu.ebo);
        glBindBuffer(GL_ARRAY_BUFFER, gpu.ebo);
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(part.indices.size() * sizeof(uint32_t)), part.indices.data(), GL_STATIC_DRAW);
        result.gpuParts.push_back(gpu);
    }
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    result.mesh.parts.clear();
    result.mesh.parts.shrink_to_fit();
    result.uploaded = true;
}

// 요청 순서대로 모델을 읽는 백그라운드 스레드 하나(긴급 요청은 대기열 맨 앞으로 보냄).
class ModelLoader {
public:
    void Start(GLFWwindow* uploadContext) {
        uploadContext_ = uploadContext;
        thread_ = std::thread([this]() { Run(); });
    }

    void Request(int modelId, const fs::path& path, bool urgent) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (urgent) queue_.push_front({ modelId, path });
            else queue_.push_back({ modelId, path });
        }
        wake_.notify_one();
    }

    // 대기 중인 요청을 맨 앞으로 옮긴다(실행 중이거나 완료된 요청은 그대로 둔다).
    void Prioritize(int modelId) {
        std::lock_guard<std::mutex> lock(mutex_);
        const auto it = std::find_if(queue_.begin(), queue_.end(), [modelId](const Job& job) { return job.modelId == modelId; });
        if (it == queue_.end() || it == queue_.begin()) return;
        const Job job = *it;
        queue_.erase(it);
        queue_.push_front(job);
    }

    bool PopResult(LoadResult& result) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (results_.empty()) return false;
        result = std::move(results_.front());
        results_.pop_front();
        return true;
    }

    void Stop() {
        gQuit = true;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            stopping_ = true;
            queue_.clear();
        }
        wake_.notify_all();
        if (thread_.joinable()) thread_.join();
    }

private:
    struct Job {
        int modelId;
        fs::path path;
    };

    void Run() {
        const HRESULT com = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        if (uploadContext_) {
            glfwMakeContextCurrent(uploadContext_);
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        }
        std::unordered_set<std::string> claimedTextures;
        for (;;) {
            Job job;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                wake_.wait(lock, [this]() { return stopping_ || !queue_.empty(); });
                if (stopping_) break;
                job = std::move(queue_.front());
                queue_.pop_front();
            }

            const auto start = std::chrono::steady_clock::now();
            LoadResult result;
            result.modelId = job.modelId;
            std::vector<std::string> claimedHere;
            try {
                LoadModelCpu(job.path, claimedTextures, claimedHere, result);
                result.ok = true;
            }
            catch (const std::exception& exception) {
                result = LoadResult();
                result.modelId = job.modelId;
                result.error = exception.what();
                for (const std::string& key : claimedHere) claimedTextures.erase(key);
            }
            if (result.ok && uploadContext_) {
                UploadResult(result);
                glFinish(); // 메인 컨텍스트에서 사용하기 전에 GPU 객체 생성이 끝나야 한다
            }
            result.milliseconds = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();

            std::lock_guard<std::mutex> lock(mutex_);
            if (stopping_) break;
            results_.push_back(std::move(result));
        }
        if (uploadContext_) glfwMakeContextCurrent(nullptr);
        if (SUCCEEDED(com)) CoUninitialize();
    }

    GLFWwindow* uploadContext_ = nullptr;
    std::thread thread_;
    std::mutex mutex_;
    std::condition_variable wake_;
    std::deque<Job> queue_;
    std::deque<LoadResult> results_;
    bool stopping_ = false;
};

ModelLoader gLoader;

// ---------------------------------------------------------------------------------------------
// 모델 목록과 선택(메인 스레드)
// ---------------------------------------------------------------------------------------------

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

size_t IndexOfModel(int modelId) {
    for (size_t i = 0; i < gModels.size(); ++i) {
        if (gModels[i].id == modelId) return i;
    }
    return kNoModel;
}

void UpdateWindowTitle(GLFWwindow* window) {
    if (gModels.empty()) {
        glfwSetWindowTitle(window, "Object Viewer | obj 폴더에 .obj 파일을 넣어주세요 | P: 마우스 해제/잠금");
        return;
    }
    size_t target = IndexOfModel(gTargetModelId);
    if (target == kNoModel) target = 0;
    const Model& model = gModels[target];
    const std::string position = "(" + std::to_string(target + 1) + "/" + std::to_string(gModels.size()) + ")";
    const std::string controls = " | ←/→: 이전/다음 | WASD: 이동 | E/Q: 높이 | P: 마우스 " +
        std::string(gCursorCaptured ? "해제" : "잠금") + " | Esc: 종료";
    const std::string title = model.loaded
        ? "Object Viewer " + position + " | " + model.path.filename().string() + controls
        : "Object Viewer " + position + " | 불러오는 중: " + model.path.filename().string() + controls;
    glfwSetWindowTitle(window, title.c_str());
}

void SetCursorCaptured(GLFWwindow* window, bool captured) {
    gCursorCaptured = captured;
    gFirstMouse = true;
    glfwSetInputMode(window, GLFW_CURSOR, captured ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
    UpdateWindowTitle(window);
}

void RequestLoad(Model& model, bool urgent) {
    if (model.loaded) return;
    if (!model.requested) {
        model.requested = true;
        gLoader.Request(model.id, model.path, urgent);
    }
    else if (urgent) {
        gLoader.Prioritize(model.id);
    }
}

// 방향키로 즉시 전환할 수 있도록 선택 모델의 양옆 모델을 백그라운드에서 불러온다.
void PrefetchNeighbors(size_t index) {
    const size_t count = gModels.size();
    if (count < 2) return;
    RequestLoad(gModels[(index + 1) % count], false);
    RequestLoad(gModels[(index + count - 1) % count], false);
}

void ShowModel(GLFWwindow* window, size_t index) {
    const Model& model = gModels[index];
    gShownModelId = model.id;
    // 카메라 시작 값은 모델을 읽을 때 결정되므로 로딩이 끝난 뒤에 적용한다.
    gCamera.position = model.cameraStartPosition;
    gCamera.yaw = -90.0f;
    gCamera.pitch = model.cameraPitch;
    UpdateWindowTitle(window);
    PrefetchNeighbors(index);
}

// gModels[index]를 선택한다. 이미 읽은 모델은 바로 보여주고,
// 아직 읽는 중이면 현재 모델을 계속 표시한다.
void RequestTarget(GLFWwindow* window, size_t index, int direction) {
    Model& model = gModels[index];
    gTargetModelId = model.id;
    gTargetDirection = direction;
    if (model.loaded) {
        ShowModel(window, index);
        return;
    }
    RequestLoad(model, true);
    UpdateWindowTitle(window);
}

void SelectModel(GLFWwindow* window, int direction) {
    if (gModels.empty()) return;
    const size_t count = gModels.size();
    size_t base = IndexOfModel(gTargetModelId);
    if (base == kNoModel) base = 0;
    const size_t next = (base + (direction > 0 ? 1 : count - 1)) % count;
    RequestTarget(window, next, direction);
}

void FinalizeModel(Model& model, LoadResult& result) {
    if (!result.uploaded) UploadResult(result); // 공유 컨텍스트가 없으면 메인 스레드에서 업로드
    for (const GpuPart& gpu : result.gpuParts) {
        MeshPart part;
        part.cullBackFaces = gpu.cullBackFaces;
        part.vbo = gpu.vbo;
        part.ebo = gpu.ebo;
        part.texture = gpu.texture;
        part.indexCount = gpu.indexCount;
        part.color = gpu.color;
        part.center = gpu.center;
        part.opacity = gpu.opacity;
        part.transparent = gpu.transparent;
        glGenVertexArrays(1, &part.vao);
        glBindVertexArray(part.vao);
        glBindBuffer(GL_ARRAY_BUFFER, part.vbo);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, position)));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, normal)));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, uv)));
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, part.ebo);
        glBindVertexArray(0);
        model.parts.push_back(part);
    }
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    model.displayScale = result.mesh.displayScale;
    model.cameraMoveSpeed = result.mesh.cameraMoveSpeed;
    model.cameraStartPosition = result.mesh.cameraStartPosition;
    model.cameraPitch = result.mesh.cameraPitch;
    model.loaded = true;
}

void HandleLoadResult(GLFWwindow* window, LoadResult& result) {
    const size_t index = IndexOfModel(result.modelId);
    if (index == kNoModel) return;
    for (const std::string& warning : result.warnings) std::cerr << warning << std::endl;

    if (!result.ok) {
        std::cerr << "Skipped: " << gModels[index].path.string() << " - " << result.error << std::endl;
        const bool wasTarget = result.modelId == gTargetModelId;
        gModels.erase(gModels.begin() + static_cast<std::ptrdiff_t>(index));
        if (gModels.empty()) {
            gTargetModelId = gShownModelId = -1;
            std::cerr << "No valid OBJ files found." << std::endl;
        }
        else if (wasTarget) {
            // 사용자가 탐색하던 방향으로 계속 이동한다.
            const size_t count = gModels.size();
            const size_t next = gTargetDirection > 0 ? (index >= count ? 0 : index) : (index == 0 ? count - 1 : index - 1);
            RequestTarget(window, next, gTargetDirection);
            return;
        }
        UpdateWindowTitle(window);
        return;
    }

    Model& model = gModels[index];
    FinalizeModel(model, result);
    std::cout << "Loaded: " << model.path.string() << " (" << result.indexCount << " vertices, "
        << result.uniqueVertexCount << " unique, " << static_cast<long long>(result.milliseconds + 0.5) << " ms"
        << (result.fromCache ? ", cache" : "") << ")" << std::endl;
    if (result.modelId == gTargetModelId) ShowModel(window, index);
    else UpdateWindowTitle(window);
}

void ProcessLoadResults(GLFWwindow* window) {
    LoadResult result;
    while (gLoader.PopResult(result)) HandleLoadResult(window, result);
}

// ---------------------------------------------------------------------------------------------
// 입력
// ---------------------------------------------------------------------------------------------

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
    const size_t shown = IndexOfModel(gShownModelId);
    if (shown == kNoModel) return;
    const glm::vec3 forward = gCamera.Forward();
    const glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0, 1, 0)));
    const float distance = gModels[shown].cameraMoveSpeed * deltaTime;
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) gCamera.position += forward * distance;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) gCamera.position -= forward * distance;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) gCamera.position += right * distance;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) gCamera.position -= right * distance;
    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) gCamera.position.y += distance;
    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) gCamera.position.y -= distance;
}

// gLoader.Stop() 이후에 호출한다. 그때부터 메인 스레드가 모든 OpenGL 객체를 소유한다.
void Cleanup() {
    for (Model& model : gModels) {
        for (MeshPart& part : model.parts) {
            if (part.vao) glDeleteVertexArrays(1, &part.vao);
            if (part.vbo) glDeleteBuffers(1, &part.vbo);
            if (part.ebo) glDeleteBuffers(1, &part.ebo);
        }
    }
    for (const auto& texture : gTextureCache) {
        if (texture.second) glDeleteTextures(1, &texture.second);
    }
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
        // 메인 창과 버퍼·텍스처를 공유하는 숨겨진 창을 만든다.
        // 로더 스레드가 화면 그리기를 멈추지 않고 GPU에 업로드할 수 있다.
        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
        GLFWwindow* uploadWindow = glfwCreateWindow(1, 1, "Object Viewer loader", nullptr, window);
        glfwWindowHint(GLFW_VISIBLE, GLFW_TRUE);
        if (!uploadWindow) std::cerr << "Shared OpenGL context unavailable; uploading on the main thread." << std::endl;

        glfwMakeContextCurrent(window);
        glfwSwapInterval(1);
        glewExperimental = GL_TRUE;
        const GLenum glewStatus = glewInit();
        if (glewStatus != GLEW_OK) {
            if (uploadWindow) glfwDestroyWindow(uploadWindow);
            glfwDestroyWindow(window);
            glfwTerminate();
            throw std::runtime_error("GLEW 초기화 실패: " + std::string(reinterpret_cast<const char*>(glewGetErrorString(glewStatus))));
        }
        // GLEW가 코어 컨텍스트를 확인하면서 INVALID_ENUM을 낼 수 있다.
        glGetError();

        glfwSetKeyCallback(window, KeyCallback);
        glfwSetCursorPosCallback(window, MouseCallback);
        glfwSetFramebufferSizeCallback(window, FramebufferCallback);
        glfwGetFramebufferSize(window, &gWindowWidth, &gWindowHeight);
        glViewport(0, 0, gWindowWidth, gWindowHeight);
        glEnable(GL_DEPTH_TEST);

        const GLuint program = LoadShaderProgram(shaderDirectory);
        const Uniforms uniforms = FindUniforms(program);

        // 여기서는 경로만 모은다. 모델은 표시할 때나 곧 표시할 차례일 때 백그라운드에서 읽는다.
        int nextModelId = 0;
        for (const fs::path& path : FindObjFiles(modelDirectory)) {
            Model model;
            model.id = nextModelId++;
            model.path = path;
            gModels.push_back(std::move(model));
        }
        SetCursorCaptured(window, true);
        gLoader.Start(uploadWindow);
        if (!gModels.empty()) RequestTarget(window, 0, 1);
        else std::cerr << "No valid OBJ files found under: " << modelDirectory.string() << '\n';

        float previousTime = static_cast<float>(glfwGetTime());
        while (!glfwWindowShouldClose(window)) {
            const float now = static_cast<float>(glfwGetTime());
            const float deltaTime = std::min(now - previousTime, 0.05f);
            previousTime = now;
            ProcessLoadResults(window);
            ProcessMovement(window, deltaTime);

            const size_t shown = IndexOfModel(gShownModelId);
            const bool isInteriorMap = shown != kNoModel &&
                gModels[shown].path.filename() == "Krusty_Krab_Interior.obj";
            if (isInteriorMap) glClearColor(0.12f, 0.27f, 0.39f, 1.0f);
            else glClearColor(0.055f, 0.070f, 0.10f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            if (shown != kNoModel) {
                glUseProgram(program);
                const float aspect = static_cast<float>(gWindowWidth) / static_cast<float>(gWindowHeight);
                const glm::mat4 projection = glm::perspective(glm::radians(55.0f), aspect, 0.05f, 250.0f);
                const glm::mat4 view = gCamera.View();
                const glm::mat4 modelMatrix(1.0f);
                glUniformMatrix4fv(uniforms.projection, 1, GL_FALSE, glm::value_ptr(projection));
                glUniformMatrix4fv(uniforms.view, 1, GL_FALSE, glm::value_ptr(view));
                glUniformMatrix4fv(uniforms.model, 1, GL_FALSE, glm::value_ptr(modelMatrix));
                glUniform3fv(uniforms.lightDirection, 1, glm::value_ptr(glm::normalize(glm::vec3(-0.5f, -1.0f, -0.6f))));
                glUniform1i(uniforms.referenceLighting, isInteriorMap ? GL_TRUE : GL_FALSE);
                glUniform1i(uniforms.texture, 0);
                glActiveTexture(GL_TEXTURE0);
                const std::vector<MeshPart>& parts = gModels[shown].parts;
                auto drawPart = [&](const MeshPart& part) {
                    if (part.cullBackFaces) {
                        glEnable(GL_CULL_FACE);
                        glFrontFace(GL_CCW);
                        glCullFace(GL_BACK);
                    }
                    else glDisable(GL_CULL_FACE);
                    glUniform3fv(uniforms.color, 1, glm::value_ptr(part.color));
                    glUniform1f(uniforms.opacity, part.opacity);
                    glUniform1i(uniforms.useTexture, part.texture != 0 ? GL_TRUE : GL_FALSE);
                    glBindTexture(GL_TEXTURE_2D, part.texture);
                    glBindVertexArray(part.vao);
                    glDrawElements(GL_TRIANGLES, part.indexCount, GL_UNSIGNED_INT, nullptr);
                    glDisable(GL_CULL_FACE);
                };
                for (const MeshPart& part : parts) {
                    if (!part.transparent) drawPart(part);
                }
                std::vector<const MeshPart*> transparentParts;
                for (const MeshPart& part : parts) {
                    if (part.transparent) transparentParts.push_back(&part);
                }
                std::sort(transparentParts.begin(), transparentParts.end(), [](const MeshPart* a, const MeshPart* b) {
                    const glm::vec3 da = a->center - gCamera.position;
                    const glm::vec3 db = b->center - gCamera.position;
                    return glm::dot(da, da) > glm::dot(db, db);
                });
                if (!transparentParts.empty()) {
                    glEnable(GL_BLEND);
                    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                    glDepthMask(GL_FALSE);
                    for (const MeshPart* part : transparentParts) drawPart(*part);
                    glDepthMask(GL_TRUE);
                    glDisable(GL_BLEND);
                }
                glBindVertexArray(0);
            }

            glfwSwapBuffers(window);
            glfwPollEvents();
        }

        gLoader.Stop();
        Cleanup();
        glDeleteProgram(program);
        if (uploadWindow) glfwDestroyWindow(uploadWindow);
        glfwDestroyWindow(window);
        glfwTerminate();
        gWicFactory.Reset();
        CoUninitialize();
        return 0;
    }
    catch (const std::exception& exception) {
        std::cerr << "Object Viewer error: " << exception.what() << '\n';
        gLoader.Stop();
        glfwTerminate();
        return 1;
    }
}
