#include "display/animations/animation.h"
#include "display/animations/animation_clock.h"

namespace {

struct MovingPoint {
  float x;
  float y;
  int8_t dx;
  int8_t dy;
};

class TriangleAnimation final : public Animation {
public:
  const char* name() const override {
    return "bouncing-triangle";
  }

  void reset() override {
    clock_.reset();
    points_[0] = {18, 31, 1, 2};
    points_[1] = {106, 40, -2, 1};
    points_[2] = {71, 103, 2, -1};
    frame_ = 0;
  }

  void render(Adafruit_SH1107& display, uint32_t now) override {
    const float delta = clock_.advance(now);
    (void)delta;
    for (MovingPoint& point : points_) {
      advance(point, delta);
    }

    const uint8_t phase = frame_ % 40;
    const uint8_t radius = 7 + (phase <= 20 ? phase : 40 - phase);

    display.clearDisplay();
    display.drawRect(0, 0, 128, 128, SH110X_WHITE);
    display.drawTriangle(points_[0].x, points_[0].y, points_[1].x, points_[1].y,
        points_[2].x, points_[2].y, SH110X_WHITE);
    for (const MovingPoint& point : points_) {
      display.drawCircle(point.x, point.y, 4, SH110X_WHITE);
    }
    display.drawCircle(64, 65, radius, SH110X_WHITE);
    display.setTextColor(SH110X_WHITE);
    display.setTextSize(1);
    display.setCursor(38, 5);
    display.print("ERGOWIDE");

    frame_ = clock_.frame();
  }

private:
  AnimationClock clock_;
  static void advance(MovingPoint& point, float delta) {
    point.x += point.dx * delta;
    point.y += point.dy * delta;
    if (point.x <= 5 || point.x >= 122) {
      point.dx = -point.dx;
      point.x = constrain(point.x, 5.0f, 122.0f);
    }
    if (point.y <= 22 || point.y >= 108) {
      point.dy = -point.dy;
      point.y = constrain(point.y, 22.0f, 108.0f);
    }
  }

  MovingPoint points_[3];
  uint32_t frame_ = 0;
};

} // namespace

Animation& triangleAnimation() {
  static TriangleAnimation animation;
  return animation;
}
