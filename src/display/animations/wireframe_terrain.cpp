#include "display/animations/animation.h"
#include "display/animations/animation_clock.h"

#include "display/animations/animation_math.h"

namespace {

class WireframeTerrainAnimation final : public Animation {
public:
  const char* name() const override {
    return "wireframe-terrain";
  }
  void reset() override {
    clock_.reset();
    frame_ = 0;
  }

  void render(Adafruit_SH1107& display, uint32_t now) override {
    const float delta = clock_.advance(now);
    (void)delta;
    display.clearDisplay();

    const int16_t vanishingX = 64 + animationMath::sine(frame_) * 10 / 127;
    constexpr int16_t horizonY = 38;

    for (int16_t bottomX = -64; bottomX <= 192; bottomX += 16) {
      display.drawLine(vanishingX, horizonY, bottomX, 116, SH110X_WHITE);
    }

    const uint8_t travelPhase = clock_.phase(5);
    for (uint8_t line = 0; line < 11; ++line) {
      const uint8_t depth = travelPhase + line * 24;
      const int16_t y =
          horizonY + static_cast<uint32_t>(depth) * depth * 78 / 65025;
      display.drawLine(0, y, 127, y, SH110X_WHITE);
    }

    drawMountainRidge(display, horizonY + 3, frame_, 7);
    drawMountainRidge(display, horizonY + 15, frame_ + 70, 12);
    frame_ = clock_.frame();
  }

private:
  AnimationClock clock_;
  static void drawMountainRidge(Adafruit_SH1107& display, int16_t baseline,
      uint8_t phase, int16_t amplitude) {
    int16_t previousX = 0;
    int16_t previousY = baseline + animationMath::sine(phase) * amplitude / 127;
    for (int16_t x = 8; x <= 128; x += 8) {
      const int16_t y =
          baseline + animationMath::sine(phase + x * 3) * amplitude / 127;
      display.drawLine(previousX, previousY, x, y, SH110X_WHITE);
      previousX = x;
      previousY = y;
    }
  }

  uint8_t frame_ = 0;
};

} // namespace

Animation& wireframeTerrainAnimation() {
  static WireframeTerrainAnimation animation;
  return animation;
}
