#include "Core/SoundManager.h"

#include <iostream>

void SoundManager::PlayBGM(const std::string& name, bool loop) {
    if (currentBGM == name) return;
    currentBGM = name;
    std::cout << "[Sound] BGM: " << name << (loop ? " (loop)" : "") << "\n";
}

void SoundManager::StopBGM() {
    currentBGM.clear();
}

void SoundManager::PlaySFX(const std::string& name) {
    std::cout << "[Sound] SFX: " << name << "\n";
}
