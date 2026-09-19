#include "display/effects/oled_shading.h"
#include <cstring>

void applyOledShading(Adafruit_SH1107& display) {
  // Work from an immutable copy so a glow cannot spread across the frame.
  // This is spatial dithering on a one-bit panel, never alternating frames.
  uint8_t source[2048];
  memcpy(source, display.getBuffer(), sizeof(source));
  uint8_t* destination = display.getBuffer();
  for (uint16_t page = 0; page < 16; ++page) {
    for (uint16_t x = 0; x < 128; ++x) {
      const uint16_t at = page * 128 + x;
      const uint8_t pixel = source[at];
      const uint8_t left = x > 0 ? source[at - 1] : 0;
      const uint8_t right = x < 127 ? source[at + 1] : 0;
      const uint8_t above =
          (pixel << 1) | (page > 0 ? source[at - 128] >> 7 : 0);
      const uint8_t below =
          (pixel >> 1) | (page < 15 ? source[at + 128] << 7 : 0);
      const uint8_t interior = pixel & left & right & above & below;
      const uint8_t glow = (left | right | above | below) & ~pixel;
      const uint8_t glowPattern = x % 4 == 0 ? 0x11 : x % 4 == 2 ? 0x44 : 0;
      const uint8_t fillPattern = x % 2 == 0 ? 0x55 : 0;
      // White contours, 75% solid interiors, sparse 12.5% halos.
      destination[at] =
          (pixel & ~(interior & fillPattern)) | (glow & glowPattern);
    }
  }
}
