#include "display/effects/keystroke_milestone_effect.h"

#include "display/widgets/keystroke_count_format.h"

namespace {

constexpr uint32_t kEffectDurationMs = 2600;
constexpr int8_t kRayDirections[][2] = {
    {0, -10},
    {5, -9},
    {9, -5},
    {10, 0},
    {9, 5},
    {5, 9},
    {0, 10},
    {-5, 9},
    {-9, 5},
    {-10, 0},
    {-9, -5},
    {-5, -9},
};
constexpr int8_t kShakePattern[][2] = {
    {0, 0},
    {2, -1},
    {-2, 1},
    {1, 2},
    {-1, -2},
    {2, 1},
    {-2, -1},
};

} // namespace

void KeystrokeMilestoneEffect::start(uint32_t now, uint32_t keystrokeCount) {
  startedAt_ = now;
  keystrokeCount_ = keystrokeCount;
  isActive_ = true;
}

void KeystrokeMilestoneEffect::render(Adafruit_SH1107& display, uint32_t now) {
  if (!isActive_) {
    return;
  }

  const uint32_t elapsed = now - startedAt_;
  if (elapsed >= kEffectDurationMs) {
    isActive_ = false;
    return;
  }

  const uint8_t shakeFrame =
      (elapsed / 70) % (sizeof(kShakePattern) / sizeof(kShakePattern[0]));
  const int16_t shakeX = kShakePattern[shakeFrame][0];
  const int16_t shakeY = kShakePattern[shakeFrame][1];
  const int16_t centerX = 64 + shakeX;
  const int16_t centerY = 51 + shakeY;

  display.clearDisplay();
  display.drawRoundRect(1 + shakeX, 1 + shakeY, 126, 114, 5, SH110X_WHITE);

  // Expand rapidly, then pulse until the message clears.
  const uint8_t expansion =
      elapsed < 650 ? 3 + elapsed * 21 / 650 : 20 + (elapsed / 120) % 5;
  for (const auto& direction : kRayDirections) {
    const int16_t innerX = centerX + direction[0] * 2 / 3;
    const int16_t innerY = centerY + direction[1] * 2 / 3;
    const int16_t outerX = centerX + direction[0] * expansion / 10;
    const int16_t outerY = centerY + direction[1] * expansion / 10;
    display.drawLine(innerX, innerY, outerX, outerY, SH110X_WHITE);
    display.fillCircle(outerX, outerY, 1, SH110X_WHITE);
  }
  display.drawCircle(centerX, centerY, 6 + (elapsed / 90) % 5, SH110X_WHITE);

  char compactCount[8];
  formatCompactKeystrokeCount(
      keystrokeCount_, compactCount, sizeof(compactCount));

  const int16_t messageX = 24 + shakeX;
  const int16_t messageY = 75 + shakeY;
  display.fillRoundRect(messageX, messageY, 80, 31, 4, SH110X_WHITE);
  display.setTextSize(1);
  display.setTextWrap(false);
  display.setTextColor(SH110X_BLACK);
  display.setCursor(messageX + 10, messageY + 5);
  display.print("MILESTONE!");
  display.setCursor(messageX + 13, messageY + 17);
  display.print(compactCount);
  display.print(" KEYS");
  display.setTextColor(SH110X_WHITE);
  display.setTextWrap(true);
}
