#include "display/animations/animation_manager.h"

Animation& hyperspaceTunnelAnimation();
Animation& triangleAnimation();
Animation& curvedContourTunnelAnimation();
Animation& keystrokeCometsAnimation();
Animation& wireframeTerrainAnimation();
Animation& gravityWellAnimation();
Animation& livingConstellationAnimation();
Animation& cellularLifeAnimation();
Animation& typingWeatherAnimation();
Animation& mountainBikeAnimation();
Animation& portalNetworkAnimation();

namespace {

Animation* const animations[] = {
    &hyperspaceTunnelAnimation(),
    &triangleAnimation(),
    &curvedContourTunnelAnimation(),
    &keystrokeCometsAnimation(),
    &wireframeTerrainAnimation(),
    &gravityWellAnimation(),
    &livingConstellationAnimation(),
    &cellularLifeAnimation(),
    &typingWeatherAnimation(),
    &mountainBikeAnimation(),
    &portalNetworkAnimation(),
};

constexpr uint8_t animationCount = sizeof(animations) / sizeof(animations[0]);

} // namespace

AnimationManager::AnimationManager() : currentIndex_(0) {}

void AnimationManager::begin() {
  resetCurrent();
}

void AnimationManager::render(Adafruit_SH1107& display, uint32_t now) {
  animations[currentIndex_]->render(display, now);
}

void AnimationManager::onKeystroke(uint32_t sequence) {
  animations[currentIndex_]->onKeystroke(sequence);
}

void AnimationManager::onKeyPress(uint8_t usage, bool isGameMode) {
  animations[currentIndex_]->onKeyPress(usage, isGameMode);
}

bool AnimationManager::isInteractive() const {
  return animations[currentIndex_]->isInteractive();
}

void AnimationManager::next() {
  select((currentIndex_ + 1) % animationCount);
}

void AnimationManager::previous() {
  select((currentIndex_ + animationCount - 1) % animationCount);
}

bool AnimationManager::select(uint8_t index) {
  if (index >= animationCount) {
    return false;
  }

  currentIndex_ = index;
  resetCurrent();
  return true;
}

void AnimationManager::resetCurrent() {
  animations[currentIndex_]->reset();
}

uint8_t AnimationManager::count() const {
  return animationCount;
}

uint8_t AnimationManager::currentIndex() const {
  return currentIndex_;
}

const char* AnimationManager::currentName() const {
  return animations[currentIndex_]->name();
}

const char* AnimationManager::nameAt(uint8_t index) const {
  return index < animationCount ? animations[index]->name() : nullptr;
}
