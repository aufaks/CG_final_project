#pragma once
#include <string>

// BGM·효과음 재생.
// TODO(4단계): 사운드 라이브러리(miniaudio, irrKlang 등)를 정한 뒤 실제 재생으로 교체한다.
class SoundManager {
public:
    static void PlayBGM(const std::string& name, bool loop = true);
    static void StopBGM();
    static void PlaySFX(const std::string& name);

private:
    inline static std::string currentBGM;
};
