#include "display/animations/animation.h"
#include "display/animations/animation_math.h"
#include "display/animations/bike_controls.h"

namespace {
constexpr float kRiderX = 32;
constexpr float kWheelbase = 16;
constexpr float kRampLength = 12;
constexpr float kRampHeight = 8;
constexpr float kRearWheelX = kRiderX - kWheelbase / 2;
constexpr uint32_t kAirTimeMs = 1200;
constexpr uint32_t kTrickTimeMs = 650;
constexpr uint32_t kWheelieTimeMs = 1200;
constexpr uint8_t kUp = bikeControls::up, kDown = bikeControls::down;
constexpr uint8_t kJ = bikeControls::jump, kB = bikeControls::backflip;
constexpr uint8_t kC = bikeControls::cancan, kT = bikeControls::spin;
constexpr uint8_t kWheelie = bikeControls::wheelie;
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
  RiderPose(float y, Trick trick, uint8_t phase, float frontRise = 0)
      : y_(y), isSpinning_(trick == Trick::Spin) {
    pitchSine_ = -frontRise / kWheelbase;
    pitchCosine_ = sqrtf(1 - pitchSine_ * pitchSine_);
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
    const float rotatedX = x * cosine_ - y * sine_ + kWheelbase / 2;
    const float rotatedY = -7 + x * sine_ + y * cosine_;
    // Pitch the whole rider/bike about the rear axle, keeping its tire
    // grounded.
    return {static_cast<int16_t>(lroundf(
                kRearWheelX + rotatedX * pitchCosine_ - rotatedY * pitchSine_)),
        static_cast<int16_t>(
            lroundf(y_ + rotatedX * pitchSine_ + rotatedY * pitchCosine_))};
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
  float pitchSine_ = 0, pitchCosine_ = 1;
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
    isWheelieActive_ = wasGameMode_ = false;
    randomJumpRemainingMs_ = 0;
    launchFrontRise_ = kRampHeight;
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
    case kWheelie:
      if (!isAirborne_ && !isWheelieActive_) {
        isWheelieActive_ = true;
        wheelieStartedAt_ = now_;
      }
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
    advanceWheelie();
    advanceRandomJumps(seconds);
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
    if (!isStarted_ && isWheelieActive_) {
      wheelieStartedAt_ = now;
    }
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
    if (choice < 16) {
      return kWheelie;
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

  bool isJumpNearSpawn(uint8_t lane) const {
    for (const Jump& jump : jumps_) {
      const bool isSameLane = jump.lane == lane;
      if (jump.isActive && isSameLane && jump.x > 98) {
        return true;
      }
    }
    return false;
  }

  void addJump() {
    addJumpInLane(lane_);
  }

  void addJumpInLane(uint8_t lane) {
    if (isJumpNearSpawn(lane)) {
      return;
    }
    for (Jump& jump : jumps_) {
      if (!jump.isActive) {
        jump = {124, lane, true};
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
    scroll_ += configuration().mtbSpeed() * seconds;
    if (scroll_ >= 128) {
      scroll_ -= 128;
    }
    const float laneStep = 70 * seconds;
    riderY_ += constrain(laneY(lane_) - riderY_, -laneStep, laneStep);
  }

  bool isRampCrossed(const Jump& jump, float previousX) const {
    const float takeoffX = kRearWheelX +
        sqrtf(kWheelbase * kWheelbase - kRampHeight * kRampHeight);
    const bool isCrossingRider = previousX >= takeoffX && jump.x < takeoffX;
    const bool isLaneAligned = fabsf(riderY_ - laneY(jump.lane)) < 2;
    return !isAirborne_ && isCrossingRider && isLaneAligned;
  }

  void launch() {
    launchFrontRise_ = max(kRampHeight, wheelieRise());
    isWheelieActive_ = false;
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
      jump.x -= configuration().mtbSpeed() * seconds;
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

  void advanceRandomJumps(float seconds) {
    const bool gameMode = configuration().gameMode() != 0;
    if (!gameMode) {
      wasGameMode_ = false;
      return;
    }
    if (!wasGameMode_) {
      random_ ^=
          now_; // Vary play sessions without blocking on external entropy.
      randomJumpRemainingMs_ = 0;
      wasGameMode_ = true;
    }
    randomJumpRemainingMs_ -= seconds * 1000;
    if (randomJumpRemainingMs_ > 0) {
      return;
    }
    // Most ramps are reachable without steering; the rest invite lane changes.
    const uint8_t choice = animationMath::randomByte(random_);
    const uint8_t lane = choice < 192 ? lane_ : (lane_ + 1 + (choice & 1)) % 3;
    addJumpInLane(lane);
    randomJumpRemainingMs_ = configuration().mtbJumpIntervalMs() *
        (0.75f + animationMath::randomByte(random_) / 510.0f);
  }

  float wheelieRise() const {
    if (!isWheelieActive_ || isAirborne_) {
      return 0;
    }
    const float phase = min(
        (now_ - wheelieStartedAt_) / static_cast<float>(kWheelieTimeMs), 1.0f);
    // Smooth rise and return around the rear axle, with no airborne
    // translation.
    return 9.0f * sinf(phase * 3.14159265f);
  }

  void advanceWheelie() {
    if (isWheelieActive_ && now_ - wheelieStartedAt_ >= kWheelieTimeMs) {
      isWheelieActive_ = false;
      ++landedTricks_;
    }
  }

  float frontWheelRise() const {
    if (isAirborne_) {
      // Carry the ramp pitch into takeoff, then settle smoothly into flight.
      const float progress = min((now_ - launchedAt_) / 250.0f, 1.0f);
      return launchFrontRise_ * (1 - progress);
    }
    float rise = wheelieRise();
    for (const Jump& jump : jumps_) {
      if (!jump.isActive || jump.lane != lane_ ||
          fabsf(riderY_ - laneY(jump.lane)) >= 2) {
        continue;
      }
      const float takeoffX = kRearWheelX +
          sqrtf(kWheelbase * kWheelbase - kRampHeight * kRampHeight);
      if (jump.x < takeoffX || jump.x - kRampLength > kRiderX + 8) {
        continue;
      }
      // Solve a fixed-length wheelbase with rear tire on the flat and front
      // tire bottom on the ramp. As pitch rises the front axle moves backward.
      float low = 0, high = kRampHeight;
      for (uint8_t i = 0; i < 12; ++i) {
        const float height = (low + high) * 0.5f;
        const float frontX =
            kRearWheelX + sqrtf(kWheelbase * kWheelbase - height * height);
        const float slopeHeight =
            (frontX - (jump.x - kRampLength)) * kRampHeight / kRampLength;
        if (height < slopeHeight) {
          low = height;
        } else {
          high = height;
        }
      }
      rise = max(rise, (low + high) * 0.5f);
    }
    return rise;
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
      display.drawTriangle(
          x - kRampLength, y, x, y - kRampHeight, x + 5, y, SH110X_WHITE);
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
    const RiderPose pose(
        riderY_ - airborneHeight(), poseTrick, trickPhase(), frontWheelRise());
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
    display.print(isWheelieActive_ ? "WHEELIE" : trickLabel(trick_));
  }

  Jump jumps_[6] = {};
  uint8_t lane_ = 1;
  float riderY_ = 75;
  float scroll_ = 0;
  float randomJumpRemainingMs_ = 0, launchFrontRise_ = kRampHeight;
  uint32_t wheelieStartedAt_ = 0;
  bool isWheelieActive_ = false, wasGameMode_ = false;
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
