#include "display/diagnostics/oled_diagnostic_renderer.h"

namespace {

constexpr int16_t kSceneBottom = 102;
constexpr int16_t kTrackLeft = 12;
constexpr int16_t kTrackRight = 115;
constexpr int16_t kTrackY = 76;

int16_t diagnosticMarkerX(uint16_t frame) {
  const uint16_t travel = kTrackRight - kTrackLeft;
  const uint16_t cycle = travel * 2;
  const uint16_t phase = frame % cycle;
  const uint16_t distance = phase <= travel ? phase : cycle - phase;
  return kTrackLeft + distance;
}

} // namespace

void OledDiagnosticRenderer::reset() {
  frame_ = 0;
}

void OledDiagnosticRenderer::render(Adafruit_SH1107& display, uint32_t now) {
  (void)now;
  display.clearDisplay();

  // Fixed geometry makes addressing or transport errors obvious. Only the
  // small marker moves, one pixel per deliberately slow display frame.
  display.drawRect(0, 0, 128, kSceneBottom + 1, SH110X_WHITE);
  display.drawLine(64, 17, 64, 61, SH110X_WHITE);
  display.drawLine(42, 39, 86, 39, SH110X_WHITE);
  display.drawCircle(64, 39, 12, SH110X_WHITE);
  display.drawLine(kTrackLeft, kTrackY, kTrackRight, kTrackY, SH110X_WHITE);
  display.fillRect(
      diagnosticMarkerX(frame_) - 2, kTrackY - 2, 5, 5, SH110X_WHITE);

  display.setTextSize(1);
  display.setTextWrap(false);
  display.setTextColor(SH110X_WHITE);
  display.setCursor(31, 5);
  display.print("OLED DEBUG");
  display.setCursor(31, 89);
  display.print("SLOW TEST");
  display.setTextWrap(true);

  ++frame_;
}
