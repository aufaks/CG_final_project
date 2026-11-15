#pragma once
#include <string>

// 사운드: BGM, 효과음 재생
// TODO: 사운드 라이브러리 정한 뒤 구현
class SoundManager {
public:
    static void PlayBGM(const std::string& name) {}
    static void StopBGM() {}
    static void PlaySFX(const std::string& name) {}
};
