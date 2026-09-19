#include "display/widgets/status_bar.h"

#include <cstring>

#include "config/firmware_config.h"
#include "display/widgets/keystroke_count_format.h"

namespace {

constexpr int16_t kBadgeHeight = 11;
constexpr int16_t kBadgeRadius = 2;
constexpr int16_t kBadgeGap = 1;
constexpr int16_t kScreenMargin = 1;

} // namespace

void StatusBar::render(
    Adafruit_SH1107& display, const DisplayStatus& status) const {
  const int16_t bottomBadgeY = display.height() - kScreenMargin - kBadgeHeight;

  // The layout is left-anchored, with the journey counter stacked above it.
  drawFilledBadgeAt(
      display, status.layoutBadgeLabel, kScreenMargin, bottomBadgeY);
  char compactKeystrokeCount[8];
  formatCompactKeystrokeCount(status.keystrokeCount, compactKeystrokeCount,
      sizeof(compactKeystrokeCount));
  drawFilledBadgeAt(display, compactKeystrokeCount, kScreenMargin,
      bottomBadgeY - kBadgeHeight - kBadgeGap);

  // Badges are placed from right to left. Future status icons can call the
  // same helper in priority order and inherit the one-pixel spacing.
  int16_t rightEdge = display.width() - kScreenMargin;
  // Game mode changes whether controls reach the host; always show it even
  // when optional diagnostic/meta badges are disabled.
  if (status.isGameModeActive) {
    drawFilledBadgeAt(display, "GAME", display.width() - 29,
        bottomBadgeY - kBadgeHeight - kBadgeGap);
  }
  if (!firmwareConfig::isOledMetaKeyBadgesEnabled) {
    return;
  }
  if (status.isNumLockActive) {
    drawFilledBadge(display, "NL", rightEdge);
  }
  if (status.isInsertModeActive) {
    drawFilledBadge(display, "INS", rightEdge);
  }
  if (status.isKeyCaptureActive || firmwareConfig::isOledDebugMode) {
    drawFilledBadge(display, "DBG", rightEdge);
  }
  if (status.isFunctionKeyActive) {
    drawFilledBadge(display, "FN", rightEdge);
  }
}

void StatusBar::drawFilledBadge(
    Adafruit_SH1107& display, const char* label, int16_t& rightEdge) {
  // The built-in 1x font advances six pixels per character. Three extra
  // pixels provide two pixels of left padding and one at the right edge.
  const int16_t badgeWidth = strlen(label) * 6 + 3;
  const int16_t x = rightEdge - badgeWidth;
  const int16_t y = display.height() - kScreenMargin - kBadgeHeight;
  drawFilledBadgeAt(display, label, x, y);
  rightEdge = x - kBadgeGap;
}

void StatusBar::drawFilledBadgeAt(
    Adafruit_SH1107& display, const char* label, int16_t x, int16_t y) {
  const int16_t badgeWidth = strlen(label) * 6 + 3;

  display.fillRoundRect(
      x, y, badgeWidth, kBadgeHeight, kBadgeRadius, SH110X_WHITE);
  display.setTextSize(1);
  display.setTextWrap(false);
  display.setTextColor(SH110X_BLACK);
  display.setCursor(x + 2, y + 2);
  display.print(label);

  // Leave the framebuffer drawing defaults friendly for animations that may
  // render text in a later frame.
  display.setTextColor(SH110X_WHITE);
  display.setTextWrap(true);
}
