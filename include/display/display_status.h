#pragma once

#include <Arduino.h>

struct DisplayStatus {
  const char* layoutBadgeLabel;
  const char* capturedKeys;
  uint32_t keystrokeCount;
  bool isNumLockActive;
  bool isInsertModeActive;
  bool isKeyCaptureActive;
  bool isFunctionKeyActive;
  bool isGameModeActive;
};
