#pragma once
#include "Core/GL.h"

#include <GL/glm/gtc/quaternion.hpp>

#include <cstdint>
#include <string>
#include <vector>

class ShaderManager;
class Texture;

// 노드 하나의 로컬 변환 (뼈 하나의 자세)
struct NodePose {
    glm::vec3 translation{ 0.0f };
    glm::quat rotation{ 1.0f, 0.0f, 0.0f, 0.0f };
    glm::vec3 scale{ 1.0f };
};

// 뼈대 애니메이션이 들어 있는 glTF(.gltf + .bin + 텍스처) 모델.
// Mixamo에서 리깅·애니메이션한 캐릭터를 Blender로 합쳐 내보낸 파일을 읽는다 (tools/mixamo_to_gltf.py).
// 메시·뼈대·클립은 여러 캐릭터 인스턴스가 공유하고, 자세(NodePose 배열)는 Animator가 인스턴스마다 따로 갖는다.
// 그리기는 "skinned" 셰이더(shaders/skinned.vert)로 GPU에서 스키닝한다.
class SkinnedModel {
public:
    static constexpr int kMaxBones = 64; // shaders/skinned.vert의 uBones 크기와 같아야 한다

    struct Channel {
        enum class Path { Translation, Rotation, Scale };
        int node = -1;
        Path path = Path::Translation;
        bool step = false;            // STEP 보간이면 true, 아니면 LINEAR
        std::vector<float> times;
        std::vector<glm::vec4> values; // 이동·크기는 xyz, 회전은 (x, y, z, w)
    };

    struct Clip {
        std::string name;
        float duration = 0.0f;
        std::vector<Channel> channels;
    };

    SkinnedModel() = default;
    ~SkinnedModel();
    SkinnedModel(const SkinnedModel&) = delete;
    SkinnedModel& operator=(const SkinnedModel&) = delete;

    bool Load(const std::string& gltfPath);

    const Clip* FindClip(const std::string& name) const;
    int FindNode(const std::string& name) const; // 이름이 name으로 끝나는 첫 노드 (Mixamo의 "mixamorig:Hips" 같은 접두어 무시)

    const std::vector<NodePose>& GetRestPose() const { return restPose; }
    // pose에 clip의 time 시점 값을 덮어쓴다 (채널이 없는 노드는 그대로)
    void SampleClip(const Clip& clip, float time, std::vector<NodePose>& pose) const;
    // 노드 로컬 자세 → 노드 전역 행렬 (모델 공간)
    void ComputeGlobalMatrices(const std::vector<NodePose>& pose, std::vector<glm::mat4>& globals) const;
    // 전역 행렬 → 셰이더로 보낼 뼈 행렬 (전역 × 역바인드)
    void ComputeBoneMatrices(const std::vector<glm::mat4>& globals, std::vector<glm::mat4>& bones) const;

    // 셰이더는 미리 Use("skinned")하고 uView·uProjection·조명을 설정해 둔다
    void Draw(ShaderManager& shaders, const glm::mat4& model, const std::vector<glm::mat4>& bones) const;

    // CPU에서 정점 위치를 스키닝한다 (자세별 메시 모양을 미리 재 볼 때 쓴다. 매 프레임 쓰지 않는다)
    void SkinPositions(const std::vector<glm::mat4>& bones, std::vector<glm::vec3>& out) const;

    // CPU 정점 사본과 뼈 번호 → 노드 번호 (자세별 메시 측정용)
    const std::vector<glm::vec3>& GetCpuPositions() const { return cpuPositions; }
    const std::vector<glm::uvec4>& GetCpuJoints() const { return cpuJoints; }
    const std::vector<glm::vec4>& GetCpuWeights() const { return cpuWeights; }
    const std::vector<int>& GetJointNodes() const { return joints; }

    const glm::vec3& GetBoundsMin() const { return boundsMin; } // 바인드 자세 기준
    const glm::vec3& GetBoundsMax() const { return boundsMax; }
    size_t GetNodeCount() const { return parents.size(); }
    bool IsLoaded() const { return !primitives.empty(); }

private:
    struct Primitive {
        GLuint vao = 0;
        GLuint vbo = 0;
        GLuint ebo = 0;
        GLsizei indexCount = 0;
        GLenum indexType = GL_UNSIGNED_INT;
        Texture* texture = nullptr;
        glm::vec4 color{ 1.0f };
    };

    void Release();

    std::vector<std::string> nodeNames;
    std::vector<int> parents;
    std::vector<int> order; // 부모가 자식보다 먼저 오는 순서
    std::vector<NodePose> restPose;
    std::vector<int> joints; // 뼈 번호 → 노드 번호
    std::vector<glm::mat4> inverseBind;
    std::vector<Primitive> primitives;
    std::vector<Clip> clips;
    // CPU 스키닝용 정점 사본 (모든 프리미티브)
    std::vector<glm::vec3> cpuPositions;
    std::vector<glm::uvec4> cpuJoints;
    std::vector<glm::vec4> cpuWeights;
    glm::vec3 boundsMin{ 0.0f };
    glm::vec3 boundsMax{ 0.0f };
};
