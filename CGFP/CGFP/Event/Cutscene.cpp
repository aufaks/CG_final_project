#include "Event/Cutscene.h"

#include "Character/Camera.h"

#include <algorithm>

void Cutscene::AddCameraKey(const CameraKeyframe& key) {
    cameraKeys.push_back(key);
    std::sort(cameraKeys.begin(), cameraKeys.end(), [](const CameraKeyframe& a, const CameraKeyframe& b) { return a.time < b.time; });
}

void Cutscene::AddLine(const std::string& speaker, const std::string& text) {
    lines.push_back({ speaker, text });
}

void Cutscene::Clear() {
    cameraKeys.clear();
    lines.clear();
    Start();
}

void Cutscene::Start() {
    lineIndex = 0;
    time = 0.0f;
    finished = lines.empty();
}

void Cutscene::Update(float deltaTime) {
    if (!finished) time += deltaTime;
}

void Cutscene::NextLine() {
    if (finished) return;
    if (++lineIndex >= lines.size()) finished = true;
}

void Cutscene::Skip() {
    lineIndex = lines.size();
    finished = true;
}

const DialogueLine* Cutscene::GetCurrentLine() const {
    return lineIndex < lines.size() ? &lines[lineIndex] : nullptr;
}

void Cutscene::ApplyCamera(Camera& camera) const {
    if (cameraKeys.empty()) return;
    // time이 들어 있는 구간 [a, b]를 찾는다. 처음/마지막 키 밖이면 그 키에 고정
    const CameraKeyframe* a = &cameraKeys.front();
    const CameraKeyframe* b = a;
    if (time >= cameraKeys.back().time) {
        a = b = &cameraKeys.back();
    } else {
        for (size_t i = 0; i + 1 < cameraKeys.size(); ++i) {
            if (time < cameraKeys[i + 1].time) {
                a = &cameraKeys[i];
                b = &cameraKeys[i + 1];
                break;
            }
        }
    }
    const float span = b->time - a->time;
    const float t = span > 0.0f ? glm::clamp((time - a->time) / span, 0.0f, 1.0f) : 0.0f;
    camera.position = glm::mix(a->position, b->position, t);
    camera.yaw = glm::mix(a->yaw, b->yaw, t);
    camera.pitch = glm::mix(a->pitch, b->pitch, t);
}
