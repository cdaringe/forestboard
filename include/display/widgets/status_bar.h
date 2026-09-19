#pragma once

#include <Adafruit_SH110X.h>

#include "display/display_status.h"

class StatusBar {
public:
  void render(Adafruit_SH1107& display, const DisplayStatus& status) const;

private:
  static void drawFilledBadge(
      Adafruit_SH1107& display, const char* label, int16_t& rightEdge);
  static void drawFilledBadgeAt(
      Adafruit_SH1107& display, const char* label, int16_t x, int16_t y);
};
