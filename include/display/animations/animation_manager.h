#pragma once

#include "display/animations/animation.h"

class AnimationManager {
public:
  AnimationManager();

  void begin();
  void render(Adafruit_SH1107& display, uint32_t now);
  void onKeystroke(uint32_t sequence);
  void onKeyPress(uint8_t usage, bool isGameMode);
  bool isInteractive() const;
  void next();
  void previous();
  bool select(uint8_t index);
  void resetCurrent();

  uint8_t count() const;
  uint8_t currentIndex() const;
  const char* currentName() const;
  const char* nameAt(uint8_t index) const;

private:
  uint8_t currentIndex_;
};
