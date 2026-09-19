#include "display/animations/animation.h"
#include "display/animations/animation_math.h"

namespace {
constexpr float kRiderX = 32;
constexpr float kTrackSpeed = 38;
constexpr uint32_t kAirTimeMs = 1200;
constexpr uint32_t kTrickTimeMs = 650;
constexpr uint8_t kUp = 0x52, kDown = 0x51;
constexpr uint8_t kJ = 0x0D, kB = 0x05, kC = 0x06, kT = 0x17;
enum class Trick : uint8_t { None, Backflip, Cancan, Spin };
struct Jump {
  float x;
  uint8_t lane;
  bool isActive;
};
struct Point {
  int16_t x;
  int16_t y;
};
struct Segment {
  Point from;
  Point to;
};

Trick trickForKey(uint8_t usage) {
  switch (usage) {
  case kB:
    return Trick::Backflip;
  case kC:
    return Trick::Cancan;
  case kT:
    return Trick::Spin;
  default:
    return Trick::None;
  }
}

const char* trickLabel(Trick trick) {
  switch (trick) {
  case Trick::Backflip:
    return "FLIP";
  case Trick::Cancan:
    return "CANCAN";
  case Trick::Spin:
    return "360";
  default:
    return "";
  }
}

// Transform the bicycle and rider as one pose during flips and spins.
class RiderPose {
public:
  RiderPose(float y, Trick trick, uint8_t phase)
      : y_(y), isSpinning_(trick == Trick::Spin) {
    if (trick == Trick::Backflip) {
      sine_ = -animationMath::sine(phase) / 127.0f;
      cosine_ = animationMath::sine(phase + 64) / 127.0f;
    }
    if (isSpinning_) {
      width_ = animationMath::sine(phase + 64) / 127.0f;
    }
  }

  Point project(Point point) const {
    const float x = point.x * width_;
    const float y = point.y + 7;
    return {static_cast<int16_t>(kRiderX + x * cosine_ - y * sine_),
        static_cast<int16_t>(y_ - 7 + x * sine_ + y * cosine_)};
  }

  void drawLine(Adafruit_SH1107& display, Point from, Point to) const {
    from = project(from);
    to = project(to);
    display.drawLine(from.x, from.y, to.x, to.y, SH110X_WHITE);
  }

  void drawWheels(Adafruit_SH1107& display) const {
    const int16_t radius =
        isSpinning_ ? 1 + abs(static_cast<int>(width_ * 3)) : 4;
    for (int16_t x : {-8, 8}) {
      const Point wheel = project({x, 0});
      display.drawEllipse(wheel.x, wheel.y, radius, 4, SH110X_WHITE);
    }
  }

private:
  float y_;
  float sine_ = 0;
  float cosine_ = 1;
  float width_ = 1;
  bool isSpinning_;
};

class MountainBikeAnimation final : public Animation {
public:
  const char* name() const override {
    return "mountain-bike";
  }
  bool isInteractive() const override {
    return true;
  }

  void reset() override {
    for (Jump& jump : jumps_) {
      jump.isActive = false;
    }
    lane_ = 1;
    riderY_ = laneY(lane_);
    isAirborne_ = false;
    trick_ = Trick::None;
    isStarted_ = false;
    scroll_ = 0;
    landedTricks_ = 0;
    isTrickFinished_ = false;
    random_ = 0xB1CE1234;
  }

  void onKeyPress(uint8_t usage, bool isGameMode) override {
    const uint8_t action = isGameMode ? usage : randomAmbientAction();
    switch (action) {
    case kUp:
      changeLane(-1);
      break;
    case kDown:
      changeLane(1);
      break;
    case kJ:
      addJump();
      break;
    default:
      startTrick(trickForKey(action));
      break;
    }
  }

  void render(Adafruit_SH1107& display, uint32_t now) override {
    const float seconds = advanceClock(now);
    advanceTrack(seconds);
    advanceJumps(seconds);
    advanceAirborneState();
    display.clearDisplay();
    drawTrack(display);
    drawJumps(display);
    drawRider(display);
    drawScore(display);
  }

private:
  static float laneY(uint8_t lane) {
    return 53 + lane * 22;
  }

  float advanceClock(uint32_t now) {
    const uint32_t elapsed = isStarted_ ? now - now_ : 0;
    now_ = now;
    isStarted_ = true;
    return min(elapsed, uint32_t{50}) / 1000.0f;
  }

  uint8_t randomAmbientAction() {
    const uint8_t choice = animationMath::randomByte(random_);
    if (choice > 79) {
      return 0;
    }
    if (isAirborne_) {
      const uint8_t tricks[] = {kB, kC, kT};
      return tricks[choice % 3];
    }
    if (choice < 48) {
      return kJ;
    }
    return choice < 64 ? kUp : kDown;
  }

  void changeLane(int8_t direction) {
    if (isAirborne_) {
      return;
    }
    lane_ = constrain(static_cast<int>(lane_) + direction, 0, 2);
  }

  bool isJumpNearSpawn() const {
    for (const Jump& jump : jumps_) {
      const bool isSameLane = jump.lane == lane_;
      if (jump.isActive && isSameLane && jump.x > 98) {
        return true;
      }
    }
    return false;
  }

  void addJump() {
    if (isJumpNearSpawn()) {
      return;
    }
    for (Jump& jump : jumps_) {
      if (!jump.isActive) {
        jump = {124, lane_, true};
        return;
      }
    }
  }

  bool isTrickAvailable() const {
    const bool isTrickUnused = trick_ == Trick::None;
    const bool isTimeRemaining =
        now_ - launchedAt_ <= kAirTimeMs - kTrickTimeMs;
    return isAirborne_ && isTrickUnused && isTimeRemaining;
  }

  void startTrick(Trick trick) {
    if (trick == Trick::None || !isTrickAvailable()) {
      return;
    }
    trick_ = trick;
    trickStartedAt_ = now_;
  }

  void advanceTrack(float seconds) {
    scroll_ += kTrackSpeed * seconds;
    if (scroll_ >= 128) {
      scroll_ -= 128;
    }
    const float laneStep = 70 * seconds;
    riderY_ += constrain(laneY(lane_) - riderY_, -laneStep, laneStep);
  }

  bool isRampCrossed(const Jump& jump, float previousX) const {
    const bool isCrossingRider = previousX >= kRiderX && jump.x < kRiderX;
    const bool isLaneAligned = fabsf(riderY_ - laneY(jump.lane)) < 2;
    return !isAirborne_ && isCrossingRider && isLaneAligned;
  }

  void launch() {
    isAirborne_ = true;
    launchedAt_ = now_;
    trick_ = Trick::None;
    isTrickFinished_ = false;
  }

  void advanceJumps(float seconds) {
    for (Jump& jump : jumps_) {
      if (!jump.isActive) {
        continue;
      }
      const float previousX = jump.x;
      jump.x -= kTrackSpeed * seconds;
      if (isRampCrossed(jump, previousX)) {
        launch();
      }
      if (jump.x < -14) {
        jump.isActive = false;
      }
    }
  }

  void advanceAirborneState() {
    if (!isAirborne_) {
      return;
    }
    if (trick_ != Trick::None) {
      isTrickFinished_ = now_ - trickStartedAt_ >= kTrickTimeMs;
    }
    if (now_ - launchedAt_ < kAirTimeMs) {
      return;
    }
    isAirborne_ = false;
    if (isTrickFinished_) {
      ++landedTricks_;
    }
    trick_ = Trick::None;
  }

  float airborneHeight() const {
    if (!isAirborne_) {
      return 0;
    }
    const float phase = (now_ - launchedAt_) / static_cast<float>(kAirTimeMs);
    return 96 * phase * (1 - phase);
  }

  uint8_t trickPhase() const {
    if (trick_ == Trick::None || isTrickFinished_) {
      return 0;
    }
    return (now_ - trickStartedAt_) * 255 / kTrickTimeMs;
  }

  bool isCancanExtended() const {
    const uint8_t phase = trickPhase();
    return trick_ == Trick::Cancan && phase > 35 && phase < 220;
  }

  void drawMountains(Adafruit_SH1107& display) const {
    for (int16_t peak = -1; peak < 6; ++peak) {
      const int16_t x = peak * 32 - static_cast<int16_t>(scroll_ / 4);
      display.drawLine(x, 32, x + 16, 15, SH110X_WHITE);
      display.drawLine(x + 16, 15, x + 32, 32, SH110X_WHITE);
    }
  }

  void drawLane(Adafruit_SH1107& display, uint8_t lane) const {
    const int16_t y = laneY(lane) + 7;
    for (int16_t x = -128; x < 256; x += 16) {
      const int16_t movingX = x - static_cast<int16_t>(scroll_);
      display.drawFastHLine(movingX, y, 9, SH110X_WHITE);
      display.drawPixel(movingX + 5, y - 3, SH110X_WHITE);
    }
  }

  void drawTrack(Adafruit_SH1107& display) const {
    drawMountains(display);
    for (uint8_t lane = 0; lane < 3; ++lane) {
      drawLane(display, lane);
    }
  }

  void drawJumps(Adafruit_SH1107& display) const {
    for (const Jump& jump : jumps_) {
      if (!jump.isActive) {
        continue;
      }
      const int16_t x = jump.x;
      const int16_t y = laneY(jump.lane) + 4;
      display.drawTriangle(x - 12, y, x, y - 8, x + 5, y, SH110X_WHITE);
      display.drawLine(x - 7, y - 1, x - 1, y - 5, SH110X_WHITE);
    }
  }

  void drawBicycle(Adafruit_SH1107& display, const RiderPose& pose) const {
    static constexpr Segment frame[] = {
        {{-8, 0}, {-2, -7}},
        {{-2, -7}, {3, 0}},
        {{3, 0}, {-8, 0}},
        {{-2, -7}, {5, -7}},
        {{5, -7}, {3, 0}},
        {{5, -7}, {8, 0}},
        {{5, -7}, {5, -10}},
        {{5, -10}, {9, -10}},
        {{-5, -8}, {-1, -8}},
    };
    pose.drawWheels(display);
    for (const Segment& segment : frame) {
      pose.drawLine(display, segment.from, segment.to);
    }
  }

  void drawBody(Adafruit_SH1107& display, const RiderPose& pose) const {
    pose.drawLine(display, {-3, -9}, {0, -15});
    pose.drawLine(display, {0, -15}, {6, -10});
    pose.drawLine(display, {-3, -9}, {0, -5});
    pose.drawLine(display, {0, -5}, {3, 0});
    const Point knee = isCancanExtended() ? Point{-11, -7} : Point{-5, -4};
    const Point foot = isCancanExtended() ? Point{-16, -10} : Point{0, 0};
    pose.drawLine(display, {-3, -9}, knee);
    pose.drawLine(display, knee, foot);
    const Point helmet = pose.project({1, -18});
    display.fillCircle(helmet.x, helmet.y, 2, SH110X_WHITE);
  }

  void drawRider(Adafruit_SH1107& display) const {
    const Trick poseTrick = isTrickFinished_ ? Trick::None : trick_;
    const RiderPose pose(riderY_ - airborneHeight(), poseTrick, trickPhase());
    drawBicycle(display, pose);
    drawBody(display, pose);
  }

  void drawScore(Adafruit_SH1107& display) const {
    display.setTextSize(1);
    display.setTextColor(SH110X_WHITE);
    display.setCursor(3, 2);
    display.print("MTB ");
    display.print(landedTricks_);
    display.setCursor(66, 2);
    display.print(trickLabel(trick_));
  }

  Jump jumps_[6] = {};
  uint8_t lane_ = 1;
  float riderY_ = 75;
  float scroll_ = 0;
  uint32_t now_ = 0, launchedAt_ = 0, trickStartedAt_ = 0;
  uint32_t random_ = 0xB1CE1234;
  uint16_t landedTricks_ = 0;
  bool isStarted_ = false, isAirborne_ = false, isTrickFinished_ = false;
  Trick trick_ = Trick::None;
};
} // namespace

Animation& mountainBikeAnimation() {
  static MountainBikeAnimation animation;
  return animation;
}
