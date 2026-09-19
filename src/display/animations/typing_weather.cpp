#include "display/animations/animation.h"
#include "display/animations/animation_clock.h"

#include "display/animations/animation_math.h"

namespace {

constexpr uint8_t kMaximumRaindrops = 24;

struct Raindrop {
  uint8_t x;
  float y;
  uint8_t speed;
};

class TypingWeatherAnimation final : public Animation {
public:
  const char* name() const override {
    return "typing-weather";
  }

  void reset() override {
    clock_.reset();
    energy_ = 0;
    frame_ = 0;
    randomState_ = 0xC10D5EED;
    isLightningActive_ = false;
    for (Raindrop& drop : drops_) {
      resetDrop(drop);
    }
  }

  void onKeystroke(uint32_t sequence) override {
    randomState_ ^= sequence * 2654435761UL;
    energy_ = energy_ > 231 ? 255 : energy_ + 24;
  }

  void render(Adafruit_SH1107& display, uint32_t now) override {
    const float delta = clock_.advance(now);
    display.clearDisplay();
    drawClouds(display);
    drawRain(display, delta);
    updateLightning();
    drawLightning(display);
    decayEnergy();
    frame_ = clock_.frame();
  }

private:
  void drawRain(Adafruit_SH1107& display, float delta) {
    const uint8_t activeDrops = 5 + energy_ * (kMaximumRaindrops - 5) / 255;
    for (uint8_t index = 0; index < activeDrops; ++index) {
      Raindrop& drop = drops_[index];
      drop.y += (drop.speed + energy_ / 96) * delta;
      if (drop.y > 108) {
        resetDrop(drop);
        drop.y = 25;
      }
      display.drawLine(drop.x, drop.y, drop.x - 2, drop.y + 5, SH110X_WHITE);
    }
  }

  bool isLightningDue() const {
    return energy_ > 170 && (frame_ % 17U) < 2;
  }

  void updateLightning() {
    const bool isVisible = isLightningDue();
    // Preserve the old 200 ms strike cadence at the higher display FPS.
    // Choosing a new bolt every display frame would produce rapid flicker.
    if (isVisible && (!isLightningActive_ || clock_.isTickDue())) {
      strikeX_ = 28 + animationMath::randomByte(randomState_) % 72;
    }
    isLightningActive_ = isVisible;
  }

  void drawLightning(Adafruit_SH1107& display) const {
    if (!isLightningActive_) {
      return;
    }
    display.drawLine(strikeX_, 27, strikeX_ - 7, 55, SH110X_WHITE);
    display.drawLine(strikeX_ - 7, 55, strikeX_ + 1, 54, SH110X_WHITE);
    display.drawLine(strikeX_ + 1, 54, strikeX_ - 11, 91, SH110X_WHITE);
  }

  void decayEnergy() {
    if (clock_.isTickDue()) {
      energy_ = energy_ > 2 ? energy_ - 3 : 0;
    }
  }

  AnimationClock clock_;
  bool isLightningActive_ = false;
  int16_t strikeX_ = 28;
  void resetDrop(Raindrop& drop) {
    drop.x = animationMath::randomByte(randomState_) & 0x7F;
    drop.y = animationMath::randomByte(randomState_) % 100;
    drop.speed = 2 + animationMath::randomByte(randomState_) % 4;
  }

  void drawClouds(Adafruit_SH1107& display) const {
    const int16_t drift = animationMath::sine(frame_) * 5 / 127;
    display.fillCircle(35 + drift, 19, 11, SH110X_WHITE);
    display.fillCircle(51 + drift, 15, 15, SH110X_WHITE);
    display.fillCircle(69 + drift, 20, 11, SH110X_WHITE);
    display.fillRect(30 + drift, 19, 47, 10, SH110X_WHITE);
    display.fillCircle(91 - drift, 14, 9, SH110X_WHITE);
    display.fillCircle(105 - drift, 18, 12, SH110X_WHITE);
    display.fillRect(87 - drift, 17, 29, 8, SH110X_WHITE);
  }

  Raindrop drops_[kMaximumRaindrops] = {};
  uint32_t randomState_ = 0;
  uint8_t energy_ = 0;
  uint8_t frame_ = 0;
};

} // namespace

Animation& typingWeatherAnimation() {
  static TypingWeatherAnimation animation;
  return animation;
}
