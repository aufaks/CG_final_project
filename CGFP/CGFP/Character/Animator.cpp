#include "Character/Animator.h"

#include <cmath>

namespace {
const std::string kNoClip;
}

void Animator::SetModel(const SkinnedModel* value) {
    model = value;
    current = Layer{};
    previous = Layer{};
    fade = fadeDuration = 0.0f;
    Update(0.0f);
}

bool Animator::Play(const std::string& clipName, bool loop, float fadeSeconds, bool restart) {
    if (!model) return false;
    const SkinnedModel::Clip* clip = model->FindClip(clipName);
    if (!clip) return false;
    if (clip == current.clip && !restart) {
        current.loop = loop;
        return true;
    }
    previous = current;
    current = Layer{ clip, 0.0f, loop };
    fadeDuration = previous.clip ? fadeSeconds : 0.0f;
    fade = 0.0f;
    return true;
}

void Animator::Advance(Layer& layer, float deltaTime) const {
    if (!layer.clip) return;
    layer.time += deltaTime * speed;
    const float duration = layer.clip->duration;
    if (duration <= 0.0f) {
        layer.time = 0.0f;
    } else if (layer.loop) {
        layer.time = std::fmod(layer.time, duration);
    } else if (layer.time > duration) {
        layer.time = duration;
    }
}

void Animator::Update(float deltaTime) {
    if (!model) return;
    Advance(current, deltaTime);
    Advance(previous, deltaTime);
    if (fade < fadeDuration) fade += deltaTime;

    pose = model->GetRestPose();
    if (current.clip) model->SampleClip(*current.clip, current.time, pose);

    // 이전 클립에서 넘어오는 중이면 섞는다
    if (previous.clip && fade < fadeDuration) {
        previousPose = model->GetRestPose();
        model->SampleClip(*previous.clip, previous.time, previousPose);
        const float w = glm::clamp(fade / fadeDuration, 0.0f, 1.0f);
        for (size_t i = 0; i < pose.size(); ++i) {
            pose[i].translation = glm::mix(previousPose[i].translation, pose[i].translation, w);
            pose[i].rotation = glm::slerp(previousPose[i].rotation, pose[i].rotation, w);
            pose[i].scale = glm::mix(previousPose[i].scale, pose[i].scale, w);
        }
    } else {
        previous = Layer{};
    }

    model->ComputeGlobalMatrices(pose, globals);
    model->ComputeBoneMatrices(globals, bones);
}

const std::string& Animator::GetClipName() const {
    return current.clip ? current.clip->name : kNoClip;
}

float Animator::GetDuration() const {
    return current.clip ? current.clip->duration : 0.0f;
}

float Animator::GetProgress() const {
    const float duration = GetDuration();
    return duration > 0.0f ? glm::clamp(current.time / duration, 0.0f, 1.0f) : 1.0f;
}

bool Animator::IsFinished() const {
    return current.clip && !current.loop && current.time >= current.clip->duration;
}

glm::vec3 Animator::GetNodePosition(int node) const {
    if (node < 0 || node >= static_cast<int>(globals.size())) return glm::vec3(0.0f);
    return glm::vec3(globals[node][3]);
}
