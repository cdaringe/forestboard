#include "display/widgets/boot_splash.h"

namespace {
constexpr char kName[] = "forestboard";
constexpr int16_t kTreeWidth = 11;
constexpr int16_t kTreeHeight = 15;
constexpr int16_t kGap = 6;
// The built-in 1x font has 5-pixel glyphs and a 6-pixel advance.
constexpr int16_t kTextWidth = (sizeof(kName) - 1) * 6 - 1;
constexpr int16_t kTextHeight = 7;
} // namespace

void drawBootSplash(Adafruit_SH1107& display) {
  display.clearDisplay();
  const int16_t left = (display.width() - kTreeWidth - kGap - kTextWidth) / 2;
  const int16_t top = (display.height() - kTreeHeight) / 2;
  // A native one-bit evergreen icon; the built-in OLED font has no emoji.
  const int16_t center = left + kTreeWidth / 2;
  for (int16_t tier = 0; tier < 3; ++tier) {
    for (int16_t row = 0; row < 5; ++row) {
      const int16_t halfWidth = row + (tier == 2 ? 1 : 0);
      display.drawFastHLine(center - halfWidth, top + tier * 3 + row,
          halfWidth * 2 + 1, SH110X_WHITE);
    }
  }
  display.fillRect(center - 1, top + 11, 3, 4, SH110X_WHITE);
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  display.setTextWrap(false);
  display.setCursor(
      left + kTreeWidth + kGap, top + (kTreeHeight - kTextHeight) / 2);
  display.print(kName);
  display.setTextWrap(true);
}
