#pragma once
#include "Object/Object.h"
#include "Render/Mesh.h"

#include <memory>
#include <random>
#include <string>
#include <vector>

// 계산대의 징징이. 기본은 Idle, 5~10초마다 한숨(Sigh) 애니메이션을 한 번 재생하고 Idle로 돌아간다.
// 메시를 조각내지 않고, 정점마다 다리·몸통·머리·팔 가중치를 줘서 관절 근처를 부드럽게 섞는다
// (linear blend skinning). 그래서 어깨·목·허리가 굽어도 메시가 갈라지지 않는다.
class Squidward : public Object {
public:
    enum class State { Idle, Sigh };

    Squidward();
    ~Squidward() override;

    bool LoadModel(const std::string& objPath, const std::string& texturePath, float height);
    void Place(const glm::vec3& feetPosition, float yawDegrees);

    void Update(float deltaTime) override;
    void Render(ShaderManager& shaders) const override;

    State GetState() const { return state; }

    float minIdleSeconds = 5.0f;
    float maxIdleSeconds = 10.0f;
    float sighSeconds = 4.8f;

private:
    enum Bone { Legs, Torso, Head, RightArm, LeftArm, BoneCount };

    struct Weights {
        float w[BoneCount]{};
    };

    void EnterState(State next);
    void ComputeBoneMatrices(glm::mat4 (&out)[BoneCount]) const;
    void Skin();

    std::unique_ptr<Mesh> skinnedMesh;
    std::vector<MeshVertex> restVertices; // OBJ 원래 자세(T포즈)
    std::vector<MeshVertex> posed;        // 이번 프레임 자세
    std::vector<Weights> weights;

    glm::vec3 waist{ 0.0f };
    glm::vec3 neck{ 0.0f };
    glm::vec3 shoulders[2]{}; // [0] = 오른쪽(-X), [1] = 왼쪽(+X)
    float modelHeight = 1.0f;

    glm::mat4 modelOffset{ 1.0f }; // OBJ 좌표 → 발 원점·키 height
    State state = State::Idle;
    float stateTime = 0.0f; // 현재 상태에 들어온 뒤 지난 시간
    float idleTime = 0.0f;  // Idle 흔들림 시계 (상태와 상관없이 계속 흐른다)
    float idleDuration = 0.0f;
    std::mt19937 random{ std::random_device{}() };
};
