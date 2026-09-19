#pragma once

#include <Adafruit_SH110X.h>

// Keep Adafruit's framebuffer/GFX API, but control initialization and transfer
// timing here. No all-white startup scene or multi-second reset is needed.
class OledTransport : public Adafruit_SH1107 {
public:
  OledTransport();
  bool allocate();
  bool configure(bool isAfterReset);
  void reset();
  bool transferFrame(void (*serviceInput)());
  bool powerOn();
};
