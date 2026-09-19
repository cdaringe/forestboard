#include "display/animations/animation.h"
#include "display/animations/animation_clock.h"

#include "display/animations/animation_math.h"

namespace {

constexpr uint8_t kCometCount = 18;
constexpr uint8_t kBackgroundStarCount = 20;

struct Comet {
  float x;
  float y;
  int8_t velocityX;
  int8_t velocityY;
  float life;
};

class KeystrokeCometsAnimation final : public Animation {
public:
  const char* name() const override {
    return "keystroke-comets";
  }

  void reset() override {
    clock_.reset();
    randomState_ = 0xC011E75U;
    for (Comet& comet : comets_) {
      comet.life = 0;
    }
    for (uint8_t index = 0; index < kBackgroundStarCount; ++index) {
      starX_[index] = animationMath::randomByte(randomState_) & 0x7F;
      starY_[index] = animationMath::randomByte(randomState_) % 108;
    }
  }

  void onKeystroke(uint32_t sequence) override {
    randomState_ ^= sequence * 2654435761UL;
    Comet* comet = availableComet();
    if (comet != nullptr) {
      launchComet(*comet);
    }
  }

  void render(Adafruit_SH1107& display, uint32_t now) override {
    const float delta = clock_.advance(now);
    display.clearDisplay();
    drawBackgroundStars(display);
    for (Comet& comet : comets_) {
      if (isActive(comet)) {
        advanceComet(comet, delta);
        drawComet(display, comet);
      }
    }
  }

private:
  static bool isActive(const Comet& comet) {
    return comet.life > 0;
  }

  Comet* availableComet() {
    for (Comet& comet : comets_) {
      if (!isActive(comet)) {
        return &comet;
      }
    }
    // A typing burst may fill the pool. Let existing flights finish.
    return nullptr;
  }

  int8_t randomSpeed() {
    return 2 + animationMath::randomByte(randomState_) % 3;
  }

  int8_t randomDrift() {
    return static_cast<int>(animationMath::randomByte(randomState_) % 7) - 3;
  }

  void launchComet(Comet& comet) {
    comet.x = 4 + animationMath::randomByte(randomState_) % 120;
    comet.y = 4 + animationMath::randomByte(randomState_) % 97;
    comet.velocityX = randomDrift();
    comet.velocityY = randomDrift();
    comet.life = 24;
    // Vary the launch position along every edge, directed into the scene.
    switch (animationMath::randomByte(randomState_) % 4) {
    case 0:
      comet.x = 4;
      comet.velocityX = randomSpeed();
      break;
    case 1:
      comet.x = 123;
      comet.velocityX = -randomSpeed();
      break;
    case 2:
      comet.y = 4;
      comet.velocityY = randomSpeed();
      break;
    case 3:
      comet.y = 100;
      comet.velocityY = -randomSpeed();
      break;
    }
  }

  void drawBackgroundStars(Adafruit_SH1107& display) const {
    for (uint8_t index = 0; index < kBackgroundStarCount; ++index) {
      display.drawPixel(starX_[index], starY_[index], SH110X_WHITE);
    }
  }

  static void advanceComet(Comet& comet, float delta) {
    comet.x += comet.velocityX * delta;
    comet.y += comet.velocityY * delta;
    comet.life -= delta;
  }

  static void drawComet(Adafruit_SH1107& display, const Comet& comet) {
    display.drawLine(comet.x - comet.velocityX * 3,
        comet.y - comet.velocityY * 3, comet.x, comet.y, SH110X_WHITE);
    display.fillCircle(comet.x, comet.y, comet.life > 16 ? 2 : 1, SH110X_WHITE);
  }

  AnimationClock clock_;
  Comet comets_[kCometCount] = {};
  uint8_t starX_[kBackgroundStarCount] = {};
  uint8_t starY_[kBackgroundStarCount] = {};
  uint32_t randomState_ = 0;
};

} // namespace

Animation& keystrokeCometsAnimation() {
  static KeystrokeCometsAnimation animation;
  return animation;
}
