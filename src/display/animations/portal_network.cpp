#include "display/animations/animation.h"
#include "display/animations/animation_clock.h"

#include "display/animations/animation_math.h"

namespace {

struct Portal {
  int16_t x;
  int16_t y;
  int16_t radius;
};

class PortalNetworkAnimation final : public Animation {
public:
  const char* name() const override {
    return "portal-network";
  }
  void reset() override {
    clock_.reset();
    frame_ = 0;
  }

  void render(Adafruit_SH1107& display, uint32_t now) override {
    const float delta = clock_.advance(now);
    (void)delta;
    display.clearDisplay();

    const uint16_t cycle = frame_ % 260;
    if (cycle >= 205) {
      drawPortalTransit(display, cycle - 205);
    } else {
      drawNetwork(display);
    }
    frame_ = clock_.frame();
  }

private:
  AnimationClock clock_;
  Portal portalAt(uint8_t index) const {
    static constexpr int16_t baseX[] = {27, 94, 66, 105};
    static constexpr int16_t baseY[] = {34, 29, 79, 82};
    const uint8_t phase = frame_ + index * 61;
    return {
        static_cast<int16_t>(
            baseX[index] + animationMath::sine(phase) * 7 / 127),
        static_cast<int16_t>(
            baseY[index] + animationMath::sine(phase + 64) * 6 / 127),
        static_cast<int16_t>(7 + (frame_ / 3 + index * 2) % 5),
    };
  }

  void drawNetwork(Adafruit_SH1107& display) const {
    Portal portals[4];
    for (uint8_t index = 0; index < 4; ++index) {
      portals[index] = portalAt(index);
    }

    for (uint8_t index = 0; index < 4; ++index) {
      const Portal& from = portals[index];
      const Portal& to = portals[(index + 1) % 4];
      display.drawLine(from.x, from.y, to.x, to.y, SH110X_WHITE);
      const uint8_t energyPhase = clock_.phase(4, index * 64);
      const int16_t energyX =
          from.x + static_cast<int32_t>(to.x - from.x) * energyPhase / 255;
      const int16_t energyY =
          from.y + static_cast<int32_t>(to.y - from.y) * energyPhase / 255;
      display.fillCircle(energyX, energyY, 1, SH110X_WHITE);

      display.drawCircle(from.x, from.y, from.radius, SH110X_WHITE);
      display.drawCircle(from.x, from.y, from.radius - 3, SH110X_WHITE);
      display.drawPixel(from.x, from.y, SH110X_WHITE);
    }
  }

  void drawPortalTransit(Adafruit_SH1107& display, uint8_t transitFrame) const {
    const int16_t centerX = 64 + animationMath::sine(frame_ * 2) * 8 / 127;
    const int16_t centerY = 57 + animationMath::sine(frame_ + 64) * 6 / 127;
    for (uint8_t ring = 0; ring < 9; ++ring) {
      const uint8_t phase = transitFrame * 7 + ring * 29;
      const int16_t radius =
          3 + static_cast<uint16_t>(phase) * phase * 70 / 65025;
      display.drawCircle(centerX, centerY, radius, SH110X_WHITE);
    }
    display.drawLine(centerX, centerY, 0, 0, SH110X_WHITE);
    display.drawLine(centerX, centerY, 127, 0, SH110X_WHITE);
    display.drawLine(centerX, centerY, 0, 111, SH110X_WHITE);
    display.drawLine(centerX, centerY, 127, 111, SH110X_WHITE);
  }

  uint16_t frame_ = 0;
};

} // namespace

Animation& portalNetworkAnimation() {
  static PortalNetworkAnimation animation;
  return animation;
}
