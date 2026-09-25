#pragma once

#include <Adafruit_SH110X.h>

// Keep Adafruit's framebuffer/GFX API, but control initialization and transfer
// timing here. No all-white startup scene or multi-second reset is needed.
class OledTransport : public Adafruit_SH1107 {
public:
  OledTransport();
  bool allocate();
  bool configure(bool isAfterReset);
  void holdReset();
  void releaseReset();
  bool transferFrame(void (*serviceInput)());
  bool powerOn();
  bool powerOff();

private:
  bool writeBytes(const uint8_t* bytes, size_t length, bool isData);
  uint32_t halfCycleTicks_ = 0;
};
