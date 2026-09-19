#include "display/animations/animation.h"
#include "display/animations/animation_clock.h"

#include "display/animations/animation_math.h"

namespace {

constexpr uint8_t kMaximumNodeCount = 24;

struct ConstellationNode {
  uint8_t x;
  uint8_t y;
  uint8_t phase;
};

class LivingConstellationAnimation final : public Animation {
public:
  const char* name() const override {
    return "living-constellation";
  }

  void reset() override {
    clock_.reset();
    frame_ = 0;
    nodeCount_ = 0;
    randomState_ = 0x57A2C0DE;
    for (uint8_t index = 0; index < 6; ++index) {
      addNode(animationMath::nextRandom(randomState_));
    }
  }

  void onKeystroke(uint32_t sequence) override {
    addNode(sequence);
  }

  void render(Adafruit_SH1107& display, uint32_t now) override {
    const float delta = clock_.advance(now);
    (void)delta;
    display.clearDisplay();

    for (uint8_t first = 0; first < nodeCount_; ++first) {
      for (uint8_t second = first + 1; second < nodeCount_; ++second) {
        const int16_t dx = nodes_[first].x - nodes_[second].x;
        const int16_t dy = nodes_[first].y - nodes_[second].y;
        const uint16_t distanceSquared = dx * dx + dy * dy;
        if (distanceSquared < 760 && ((first + second) & 1U) == 0) {
          display.drawLine(nodes_[first].x, nodes_[first].y, nodes_[second].x,
              nodes_[second].y, SH110X_WHITE);
        }
      }
    }

    for (uint8_t index = 0; index < nodeCount_; ++index) {
      const ConstellationNode& node = nodes_[index];
      const bool isBright = ((frame_ + node.phase) & 7U) < 3;
      display.fillCircle(node.x, node.y, isBright ? 2 : 1, SH110X_WHITE);
    }
    frame_ = clock_.frame();
  }

private:
  AnimationClock clock_;
  void addNode(uint32_t seed) {
    randomState_ ^= seed * 2246822519UL;
    const uint8_t target = nodeCount_ < kMaximumNodeCount
        ? nodeCount_++
        : static_cast<uint8_t>(seed % kMaximumNodeCount);
    nodes_[target] = {
        static_cast<uint8_t>(7 + animationMath::randomByte(randomState_) % 114),
        static_cast<uint8_t>(8 + animationMath::randomByte(randomState_) % 94),
        animationMath::randomByte(randomState_),
    };
  }

  ConstellationNode nodes_[kMaximumNodeCount] = {};
  uint32_t randomState_ = 0;
  uint8_t nodeCount_ = 0;
  uint8_t frame_ = 0;
};

} // namespace

Animation& livingConstellationAnimation() {
  static LivingConstellationAnimation animation;
  return animation;
}
