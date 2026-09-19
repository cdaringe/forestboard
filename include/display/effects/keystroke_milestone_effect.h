#pragma once

#include <Adafruit_SH110X.h>
#include <Arduino.h>

class KeystrokeMilestoneEffect {
public:
  void start(uint32_t now, uint32_t keystrokeCount);
  void render(Adafruit_SH1107& display, uint32_t now);

private:
  uint32_t startedAt_ = 0;
  uint32_t keystrokeCount_ = 0;
  bool isActive_ = false;
};
