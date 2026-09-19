#pragma once

#include <Adafruit_SH110X.h>
#include <Arduino.h>

class Animation {
public:
  virtual ~Animation() = default;

  virtual const char* name() const = 0;
  virtual void reset() = 0;
  virtual void render(Adafruit_SH1107& display, uint32_t now) = 0;
  virtual void onKeystroke(uint32_t sequence) {
    (void)sequence;
  }
  virtual void onKeyPress(uint8_t usage, bool isGameMode) {
    (void)usage;
    (void)isGameMode;
  }
  virtual bool isInteractive() const {
    return false;
  }
};
