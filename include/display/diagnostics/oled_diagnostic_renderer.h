#pragma once

#include <Adafruit_SH110X.h>
#include <Arduino.h>

class OledDiagnosticRenderer {
public:
  void reset();
  void render(Adafruit_SH1107& display, uint32_t now);

private:
  uint16_t frame_ = 0;
};
