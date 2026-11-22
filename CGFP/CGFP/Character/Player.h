#pragma once
#include "Character/Animator.h"
#include "Character/Camera.h"
#include "Object/Object.h"
#include "Render/SkinnedModel.h"

#include <memory>
#include <string>
#include <vector>

class InteractableObject;

// 플레이어(스폰지밥): WASD 이동, 마우스 시점. 카메라는 눈 위치에 붙어 있는 1인칭.
// transform.position은 발 위치. 모델은 발이 원점, 정면이 +Z가 되도록 LoadModel에서 맞춘다.
// 걷기 애니메이션: OBJ를 몸통·팔·다리로 나눠 읽고 어깨·골반을 축으로 돌린다(Mesh::LoadObjParts).
class Player : public Object {
public:
    Player();
    ~Player() override;

    bool LoadModel(const std::string& objPath, const std::string& texturePath, float height);
    // 뼈대 애니메이션 glTF(tools/mixamo_to_gltf.py, 클립 Idle·Walk)를 읽는다. 성공하면 OBJ 대신 이것을 그린다.
    bool LoadSkinnedModel(const std::string& gltfPath, float height);
    bool HasSkinnedModel() const { return skinned && skinned->IsLoaded(); }
    void Spawn(const glm::vec3& feetPosition, float yawDegrees);

    void Update(float deltaTime) override;
    void Render(ShaderManager& shaders) const override;        // OBJ 모델 ("map" 셰이더). glTF를 쓰면 아무것도 안 한다
    void RenderSkinned(ShaderManager& shaders) const;          // glTF 모델 ("skinned" 셰이더)

    enum class State { Idle, Walk }; // 기본 Idle, 움직이는 동안 Walk

    bool IsFirstPerson() const { return firstPerson; }
    State GetState() const { return state; }

    Camera camera;
    InteractableObject* heldObject = nullptr;

    float moveSpeed = 3.0f;
    float mouseSensitivity = 0.05f;

private:
    void UpdateLook();
    void UpdateMovement(float deltaTime);
    void SyncCamera();
    void UpdateWalkAnimation(float deltaTime, bool moving);
    glm::mat4 GetPartMatrix(int part) const; // OBJ 좌표계에서 부위의 관절 회전

    enum Part { Body, RightArm, LeftArm, RightLeg, LeftLeg, PartCount };
    std::vector<std::unique_ptr<Mesh>> parts; // 비어 있으면 통짜 mesh로 그린다
    glm::vec3 joints[PartCount]{};             // 부위별 회전 중심(OBJ 좌표). Body는 골반 중심
    float walkPhase = 0.0f; // 걸음 주기(라디안)
    float walkBlend = 0.0f; // 0 = 서 있음, 1 = 걷는 중. 멈추거나 출발할 때 부드럽게 바뀐다
    State state = State::Idle;
    float stateTime = 0.0f; // 현재 상태에 들어온 뒤 지난 시간
    float idleTime = 0.0f;  // Idle 동작(숨쉬기·팔 흔들기) 시계

    // glTF 뼈대 애니메이션 모델 (있으면 위의 OBJ 부위 애니메이션 대신 클립을 재생한다)
    std::unique_ptr<SkinnedModel> skinned;
    Animator animator;
    float walkClipSpeed = 1.0f; // Walk 클립 1배속일 때 발이 미끄러지지 않는 이동 속도(월드/초)

    glm::mat4 modelOffset{ 1.0f }; // 모델 좌표 → 발 원점·정면 +Z·키 height
    float eyeHeight = 1.3f;
    float eyeForward = 0.0f; // 발 중심에서 얼굴 앞까지 수평 거리
    float firstPersonNear = 0.12f;
    bool firstPerson = true;
};
