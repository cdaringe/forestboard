#include "display/widgets/key_capture_overlay.h"

namespace {

constexpr int16_t kCaptureRowHeight = 9;
constexpr int16_t kTextInset = 1;

} // namespace

void KeyCaptureOverlay::render(
    Adafruit_SH1107& display, bool isActive, const char* captureText) const {
  if (!isActive) {
    return;
  }

  // Cover the animation beneath the one-line debug readout so every glyph is
  // legible, then let later frames repaint it normally when capture is off.
  display.fillRect(0, 0, display.width(), kCaptureRowHeight, SH110X_BLACK);
  display.setTextSize(1);
  display.setTextWrap(false);
  display.setTextColor(SH110X_WHITE);
  display.setCursor(kTextInset, kTextInset);
  display.print(captureText[0] == '\0' ? "KEY CAPTURE" : captureText);
  display.setTextWrap(true);
}
