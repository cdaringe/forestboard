#include "config/configuration.h"
#include "display/animations/animation.h"
#include "display/animations/animation_clock.h"

namespace {

constexpr int16_t kCenterX = 64;
constexpr int16_t kCenterY = 64;
constexpr int16_t kFocalLength = 96;
constexpr uint8_t kStarCount = 28;
constexpr int16_t kNearDepth = 10;
constexpr int16_t kFarDepth = 255;
constexpr int16_t kTravelSpeed = 6;

struct Star {
  int16_t x;
  int16_t y;
  float z;
};

class HyperspaceTunnelAnimation final : public Animation {
public:
  const char* name() const override {
    return "hyperspace-tunnel";
  }

  void reset() override {
    clock_.reset();
    ringPhase_ = 0;
    randomState_ = 0xC0FFEE42;
    frame_ = 0;
    for (uint8_t index = 0; index < kStarCount; ++index) {
      resetStar(stars_[index], 20 + index * 8);
    }
  }

  void render(Adafruit_SH1107& display, uint32_t now) override {
    const float delta = clock_.advance(now);
    const float speed = configuration().warpSpeed() / 100.0f;
    ringPhase_ += 5 * delta * speed;
    if (ringPhase_ >= 256) {
      ringPhase_ -= 256;
    }
    display.clearDisplay();

    const int16_t centerX = kCenterX + triangleWave(frame_, 160, 4);
    const int16_t centerY = kCenterY + triangleWave(frame_ + 40, 192, 3);

    drawTunnelRings(display, centerX, centerY);
    drawStars(display, centerX, centerY, delta);

    // Perspective rails keep the vanishing point legible even as rings wrap.
    display.drawLine(centerX, centerY, 0, 0, SH110X_WHITE);
    display.drawLine(centerX, centerY, 127, 0, SH110X_WHITE);
    display.drawLine(centerX, centerY, 0, 127, SH110X_WHITE);
    display.drawLine(centerX, centerY, 127, 127, SH110X_WHITE);

    frame_ = clock_.frame();
  }

private:
  AnimationClock clock_;
  float ringPhase_ = 0;
  static int16_t triangleWave(
      uint32_t frame, uint16_t period, int16_t amplitude) {
    const uint16_t phase = frame % period;
    const uint16_t half = period / 2;
    const int16_t rising = phase < half ? phase : period - phase;
    return ((rising * amplitude * 4) / period) - amplitude;
  }

  uint32_t nextRandom() {
    randomState_ = randomState_ * 1664525UL + 1013904223UL;
    return randomState_;
  }

  void resetStar(Star& star, int16_t depth = kFarDepth) {
    star.x = static_cast<int16_t>((nextRandom() >> 16) % 121) - 60;
    star.y = static_cast<int16_t>((nextRandom() >> 16) % 121) - 60;

    // Avoid stars directly on the vanishing point where motion is invisible.
    if (star.x > -10 && star.x < 10 && star.y > -10 && star.y < 10) {
      star.x += star.x < 0 ? -18 : 18;
    }
    star.z = depth;
  }

  static void project(const Star& star, float depth, int16_t centerX,
      int16_t centerY, int16_t& screenX, int16_t& screenY) {
    screenX = centerX + static_cast<int32_t>(star.x) * kFocalLength / depth;
    screenY = centerY + static_cast<int32_t>(star.y) * kFocalLength / depth;
  }

  void drawStars(
      Adafruit_SH1107& display, int16_t centerX, int16_t centerY, float delta) {
    for (Star& star : stars_) {
      star.z -= kTravelSpeed * delta * configuration().warpSpeed() / 100.0f;
      if (star.z <= kNearDepth) {
        resetStar(star);
        continue;
      }

      int16_t headX;
      int16_t headY;
      int16_t tailX;
      int16_t tailY;
      project(star, star.z, centerX, centerY, headX, headY);
      project(star, star.z + 18, centerX, centerY, tailX, tailY);

      if (headX < -12 || headX > 139 || headY < -12 || headY > 139) {
        resetStar(star);
        continue;
      }

      display.drawLine(tailX, tailY, headX, headY, SH110X_WHITE);
      if (star.z < 70) {
        display.drawPixel(headX + 1, headY, SH110X_WHITE);
      }
    }
  }

  void drawTunnelRings(
      Adafruit_SH1107& display, int16_t centerX, int16_t centerY) const {
    for (uint8_t index = 0; index < configuration().warpRings(); ++index) {
      const uint16_t phase =
          static_cast<uint8_t>(static_cast<uint32_t>(ringPhase_) +
              index * 256 / configuration().warpRings());
      const int16_t halfSize =
          2 + static_cast<uint32_t>(phase) * phase * 61 / 65025;

      display.drawRect(centerX - halfSize, centerY - halfSize, halfSize * 2,
          halfSize * 2, SH110X_WHITE);
    }
  }

  Star stars_[kStarCount];
  uint32_t randomState_ = 0;
  uint32_t frame_ = 0;
};

} // namespace

Animation& hyperspaceTunnelAnimation() {
  static HyperspaceTunnelAnimation animation;
  return animation;
}
