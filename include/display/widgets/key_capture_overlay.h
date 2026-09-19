#pragma once

#include <Adafruit_SH110X.h>

class KeyCaptureOverlay {
public:
  void render(
      Adafruit_SH1107& display, bool isActive, const char* captureText) const;
};
