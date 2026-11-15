#pragma once
#include "Core/GL.h"

#include <string>
#include <vector>

class Camera;

struct CameraKeyframe {
    float time = 0.0f; // 컷신 시작부터 초
    glm::vec3 position{ 0.0f };
    float yaw = -90.0f;
    float pitch = 0.0f;
};

struct DialogueLine {
    std::string speaker;
    std::string text;
};

// 컷신: 카메라 키프레임 + 대사 큐(Enter로 넘기기). 집게사장 정산 장면에도 재사용한다.
// TODO(4단계): NPC 키프레임(위치·애니메이션) 추가
class Cutscene {
public:
    void AddCameraKey(const CameraKeyframe& key);
    void AddLine(const std::string& speaker, const std::string& text);
    void Clear();

    void Start();
    void Update(float deltaTime);
    void NextLine(); // 대사를 다 넘기면 끝
    void Skip();

    bool IsFinished() const { return finished; }
    const DialogueLine* GetCurrentLine() const;
    void ApplyCamera(Camera& camera) const; // 현재 시간의 키프레임 보간

private:
    std::vector<CameraKeyframe> cameraKeys; // time 오름차순
    std::vector<DialogueLine> lines;
    size_t lineIndex = 0;
    float time = 0.0f;
    bool finished = false;
};
