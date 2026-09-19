#include "display/animations/animation.h"
#include "display/animations/animation_clock.h"

#include "display/animations/animation_math.h"

namespace {

constexpr uint8_t kParticleCount = 24;

struct OrbitalParticle {
  float angle;
  float radius;
  uint8_t speed;
};

class GravityWellAnimation final : public Animation {
public:
  const char* name() const override {
    return "gravity-well";
  }

  void reset() override {
    clock_.reset();
    frame_ = 0;
    randomState_ = 0x6A4B17E1;
    for (OrbitalParticle& particle : particles_) {
      particle.angle = animationMath::randomByte(randomState_);
      particle.radius = 12 + animationMath::randomByte(randomState_) % 50;
      particle.speed = 1 + animationMath::randomByte(randomState_) % 4;
    }
  }

  void render(Adafruit_SH1107& display, uint32_t now) override {
    const float delta = clock_.advance(now);
    (void)delta;
    display.clearDisplay();
    const int16_t centerX = 64 + animationMath::sine(frame_) * 5 / 127;
    const int16_t centerY = 57 + animationMath::sine(frame_ + 64) * 4 / 127;

    for (uint8_t ring = 0; ring < 5; ++ring) {
      const int16_t radius = 7 + ring * 9 + ((frame_ + ring * 3) % 5);
      display.drawEllipse(centerX, centerY, radius, radius / 2, SH110X_WHITE);
    }
    display.fillCircle(centerX, centerY, 4, SH110X_WHITE);
    display.drawCircle(centerX, centerY, 7, SH110X_WHITE);

    for (OrbitalParticle& particle : particles_) {
      const uint8_t previousAngle = particle.angle;
      particle.angle += particle.speed * delta;
      if (particle.angle >= 256) {
        particle.angle -= 256;
      }
      if (particle.radius > 9) {
        particle.radius -= delta / 8;
      }
      if (particle.radius <= 9) {
        particle.radius = 54;
        particle.angle = animationMath::randomByte(randomState_);
      }

      const int16_t x = centerX +
          animationMath::sine(static_cast<uint8_t>(
              static_cast<uint16_t>(particle.angle) + 64)) *
              particle.radius / 127;
      const int16_t y =
          centerY + animationMath::sine(particle.angle) * particle.radius / 254;
      const int16_t previousX = centerX +
          animationMath::sine(previousAngle + 64) * particle.radius / 127;
      const int16_t previousY =
          centerY + animationMath::sine(previousAngle) * particle.radius / 254;
      display.drawLine(previousX, previousY, x, y, SH110X_WHITE);
      display.drawPixel(x, y, SH110X_WHITE);
    }
    frame_ = clock_.frame();
  }

private:
  AnimationClock clock_;
  OrbitalParticle particles_[kParticleCount] = {};
  uint32_t randomState_ = 0;
  uint8_t frame_ = 0;
};

} // namespace

Animation& gravityWellAnimation() {
  static GravityWellAnimation animation;
  return animation;
}
