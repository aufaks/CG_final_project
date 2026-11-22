#pragma once
#include "Render/SkinnedModel.h"

#include <string>
#include <vector>

// SkinnedModel의 애니메이션 클립을 재생한다. 캐릭터 인스턴스마다 하나씩 갖는다.
// Play로 클립을 바꾸면 fadeSeconds 동안 이전 클립과 섞어서 자세가 튀지 않게 넘어간다.
class Animator {
public:
    void SetModel(const SkinnedModel* value);

    // 같은 클립이 이미 재생 중이면 아무것도 안 한다 (restart면 처음부터). 클립이 없으면 false.
    bool Play(const std::string& clipName, bool loop, float fadeSeconds = 0.25f, bool restart = false);
    void Update(float deltaTime);

    const std::vector<glm::mat4>& GetBones() const { return bones; }
    const std::string& GetClipName() const;
    float GetTime() const { return current.time; }
    float GetDuration() const;
    float GetProgress() const; // 0~1
    bool IsFinished() const;   // 반복하지 않는 클립이 끝까지 재생됨
    glm::vec3 GetNodePosition(int node) const; // 마지막 Update 기준 모델 공간 위치

    float speed = 1.0f; // 재생 속도 배율

private:
    struct Layer {
        const SkinnedModel::Clip* clip = nullptr;
        float time = 0.0f;
        bool loop = true;
    };
    void Advance(Layer& layer, float deltaTime) const;

    const SkinnedModel* model = nullptr;
    Layer current;
    Layer previous;
    float fade = 0.0f;
    float fadeDuration = 0.0f;
    std::vector<NodePose> pose;
    std::vector<NodePose> previousPose;
    std::vector<glm::mat4> globals;
    std::vector<glm::mat4> bones;
};
