#include "config/configuration.h"
#include "display/animations/animation.h"
#include "display/animations/animation_clock.h"

namespace {

constexpr uint8_t kParticleCount = 20;
constexpr uint8_t kTravelSpeed = 5;
constexpr int16_t kMinimumContourRadius = 9;

// One cycle sampled at 64 points. Phase lookup keeps the animation smooth
// without pulling floating-point trigonometry into every rendered frame.
constexpr int8_t sineTable[] = {
    0,
    12,
    25,
    37,
    49,
    60,
    71,
    81,
    90,
    98,
    106,
    112,
    117,
    122,
    125,
    126,
    127,
    126,
    125,
    122,
    117,
    112,
    106,
    98,
    90,
    81,
    71,
    60,
    49,
    37,
    25,
    12,
    0,
    -12,
    -25,
    -37,
    -49,
    -60,
    -71,
    -81,
    -90,
    -98,
    -106,
    -112,
    -117,
    -122,
    -125,
    -126,
    -127,
    -126,
    -125,
    -122,
    -117,
    -112,
    -106,
    -98,
    -90,
    -81,
    -71,
    -60,
    -49,
    -37,
    -25,
    -12,
};

struct TunnelSlice {
  int16_t centerX;
  int16_t centerY;
  int16_t radiusX;
  int16_t radiusY;
};

struct LightParticle {
  float depth;
  int8_t wallX;
  int8_t wallY;
};

class CurvedContourTunnelAnimation final : public Animation {
public:
  const char* name() const override {
    return "curved-contour-tunnel";
  }

  void reset() override {
    clock_.reset();
    ringPhase_ = 0;
    frame_ = 0;
    randomState_ = 0x7A11C0DE;
    for (uint8_t index = 0; index < kParticleCount; ++index) {
      particles_[index].depth = index * 13;
      resetParticlePosition(particles_[index]);
    }
  }

  void render(Adafruit_SH1107& display, uint32_t now) override {
    const float delta = clock_.advance(now);
    const float speed = configuration().curvedSpeed() / 100.0f;
    ringPhase_ += 4 * delta * speed;
    if (ringPhase_ >= 256) {
      ringPhase_ -= 256;
    }
    display.clearDisplay();

    drawTrajectoryRails(display);
    drawContours(display);
    drawPassingLights(display, delta);
    frame_ = clock_.frame();
  }

private:
  AnimationClock clock_;
  float ringPhase_ = 0;
  static int16_t sine(uint8_t phase) {
    return sineTable[phase >> 2];
  }

  uint32_t nextRandom() {
    randomState_ = randomState_ * 1664525UL + 1013904223UL;
    return randomState_;
  }

  TunnelSlice sliceAt(uint8_t depthPhase) const {
    const uint32_t eased = static_cast<uint32_t>(depthPhase) * depthPhase;
    const uint8_t pathPhaseX = frame_ * 2 + depthPhase * 3 / 4;
    const uint8_t pathPhaseY = frame_ + 64 + depthPhase / 2;

    return {
        static_cast<int16_t>(64 + sine(pathPhaseX) * 18 / 127),
        static_cast<int16_t>(64 + sine(pathPhaseY) * 13 / 127),
        static_cast<int16_t>(3 + eased * 72 / 65025),
        static_cast<int16_t>(2 + eased * 56 / 65025),
    };
  }

  void drawContours(Adafruit_SH1107& display) const {
    for (uint8_t index = 0; index < configuration().curvedRings(); ++index) {
      const uint8_t depthPhase =
          static_cast<uint8_t>(static_cast<uint32_t>(ringPhase_) +
              index * 256 / configuration().curvedRings());
      const TunnelSlice slice = sliceAt(depthPhase);

      // Tiny distant rings made the vanishing point noisy. The trajectory
      // rails provide depth context until a contour is large enough to read.
      if (slice.radiusX < kMinimumContourRadius) {
        continue;
      }

      display.drawEllipse(slice.centerX, slice.centerY, slice.radiusX,
          slice.radiusY, SH110X_WHITE);

      // Reserve double ribs for contours that are already near the viewer.
      if ((index & 1) == 0 && slice.radiusX > 34) {
        display.drawEllipse(slice.centerX, slice.centerY, slice.radiusX - 2,
            slice.radiusY - 2, SH110X_WHITE);
      }
    }
  }

  void drawTrajectoryRails(Adafruit_SH1107& display) const {
    bool isPreviousAvailable = false;
    TunnelSlice previous{};

    // Connect four points on successive tunnel slices. Unlike straight rays,
    // these rails visibly describe the bend ahead.
    for (uint8_t sample = 0; sample < 9; ++sample) {
      const uint8_t depthPhase = 38 + sample * 26;
      const TunnelSlice current = sliceAt(depthPhase);

      if (isPreviousAvailable) {
        display.drawLine(previous.centerX - previous.radiusX, previous.centerY,
            current.centerX - current.radiusX, current.centerY, SH110X_WHITE);
        display.drawLine(previous.centerX + previous.radiusX, previous.centerY,
            current.centerX + current.radiusX, current.centerY, SH110X_WHITE);
        display.drawLine(previous.centerX, previous.centerY - previous.radiusY,
            current.centerX, current.centerY - current.radiusY, SH110X_WHITE);
        display.drawLine(previous.centerX, previous.centerY + previous.radiusY,
            current.centerX, current.centerY + current.radiusY, SH110X_WHITE);
      }

      previous = current;
      isPreviousAvailable = true;
    }
  }

  void resetParticlePosition(LightParticle& particle) {
    const uint8_t angle = nextRandom() >> 24;
    const int16_t distanceFromCenter = 62 + ((nextRandom() >> 24) % 39);
    particle.wallX = sine(angle) * distanceFromCenter / 127;
    particle.wallY = sine(angle + 64) * distanceFromCenter / 127;
  }

  static void particlePosition(const LightParticle& particle,
      const TunnelSlice& slice, int16_t& x, int16_t& y) {
    x = slice.centerX +
        static_cast<int16_t>(particle.wallX) * slice.radiusX / 100;
    y = slice.centerY +
        static_cast<int16_t>(particle.wallY) * slice.radiusY / 100;
  }

  void drawPassingLights(Adafruit_SH1107& display, float delta) {
    for (LightParticle& particle : particles_) {
      particle.depth +=
          kTravelSpeed * delta * configuration().curvedSpeed() / 100.0f;

      if (particle.depth >= 256.0f) {
        particle.depth -= 256.0f;
        resetParticlePosition(particle);
        continue;
      }

      // Hide distant particles until their streak has enough length to read.
      if (particle.depth < 34) {
        continue;
      }

      const uint8_t tailDepth = particle.depth - 18;
      const TunnelSlice headSlice = sliceAt(particle.depth);
      const TunnelSlice tailSlice = sliceAt(tailDepth);
      int16_t headX;
      int16_t headY;
      int16_t tailX;
      int16_t tailY;
      particlePosition(particle, headSlice, headX, headY);
      particlePosition(particle, tailSlice, tailX, tailY);

      display.drawLine(tailX, tailY, headX, headY, SH110X_WHITE);
      if (particle.depth > 190) {
        display.drawPixel(headX + 1, headY, SH110X_WHITE);
      }
    }
  }

  LightParticle particles_[kParticleCount];
  uint32_t randomState_ = 0;
  uint32_t frame_ = 0;
};

} // namespace

Animation& curvedContourTunnelAnimation() {
  static CurvedContourTunnelAnimation animation;
  return animation;
}
