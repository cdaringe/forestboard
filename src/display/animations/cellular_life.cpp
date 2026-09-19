#include "display/animations/animation.h"
#include "display/animations/animation_clock.h"

#include "display/animations/animation_math.h"

namespace {

constexpr uint8_t kGridWidth = 32;
constexpr uint8_t kGridHeight = 24;
constexpr uint8_t kCellSize = 4;

class CellularLifeAnimation final : public Animation {
public:
  const char* name() const override {
    return "cellular-life";
  }

  void reset() override {
    clock_.reset();
    randomState_ = 0x11FECAFE;
    frame_ = 0;
    for (uint8_t y = 0; y < kGridHeight; ++y) {
      cells_[y] = 0;
      for (uint8_t x = 0; x < kGridWidth; ++x) {
        if ((animationMath::randomByte(randomState_) & 7U) == 0) {
          setCell(cells_, x, y, true);
        }
      }
    }
  }

  void onKeystroke(uint32_t sequence) override {
    randomState_ ^= sequence * 3266489917UL;
    const uint8_t x = animationMath::randomByte(randomState_) % kGridWidth;
    const uint8_t y = animationMath::randomByte(randomState_) % kGridHeight;
    setCell(cells_, x, y, true);
    setCell(cells_, (x + 1) % kGridWidth, y, true);
    setCell(cells_, x, (y + 1) % kGridHeight, true);
  }

  void render(Adafruit_SH1107& display, uint32_t now) override {
    const float delta = clock_.advance(now);
    (void)delta;
    if (clock_.isTickDue() && (clock_.frame() & 1U) == 0) {
      evolve();
    }
    display.clearDisplay();
    for (uint8_t y = 0; y < kGridHeight; ++y) {
      for (uint8_t x = 0; x < kGridWidth; ++x) {
        if (isCellAlive(cells_, x, y)) {
          display.fillRect(x * kCellSize, y * kCellSize, kCellSize - 1,
              kCellSize - 1, SH110X_WHITE);
        }
      }
    }
    frame_ = clock_.frame();
  }

private:
  AnimationClock clock_;
  static bool isCellAlive(const uint32_t* grid, uint8_t x, uint8_t y) {
    return (grid[y] & (1UL << x)) != 0;
  }

  static void setCell(uint32_t* grid, uint8_t x, uint8_t y, bool isAlive) {
    if (isAlive) {
      grid[y] |= 1UL << x;
    } else {
      grid[y] &= ~(1UL << x);
    }
  }

  uint8_t neighborCount(uint8_t x, uint8_t y) const {
    uint8_t neighbors = 0;
    for (int8_t offsetY = -1; offsetY <= 1; ++offsetY) {
      for (int8_t offsetX = -1; offsetX <= 1; ++offsetX) {
        if (offsetX == 0 && offsetY == 0) {
          continue;
        }
        const uint8_t neighborX = (x + kGridWidth + offsetX) % kGridWidth;
        const uint8_t neighborY = (y + kGridHeight + offsetY) % kGridHeight;
        neighbors += isCellAlive(cells_, neighborX, neighborY) ? 1 : 0;
      }
    }
    return neighbors;
  }

  void evolve() {
    for (uint8_t y = 0; y < kGridHeight; ++y) {
      nextCells_[y] = 0;
      for (uint8_t x = 0; x < kGridWidth; ++x) {
        const uint8_t neighbors = neighborCount(x, y);
        const bool isAlive = isCellAlive(cells_, x, y);
        setCell(
            nextCells_, x, y, neighbors == 3 || (isAlive && neighbors == 2));
      }
    }
    for (uint8_t y = 0; y < kGridHeight; ++y) {
      cells_[y] = nextCells_[y];
    }
  }

  uint32_t cells_[kGridHeight] = {};
  uint32_t nextCells_[kGridHeight] = {};
  uint32_t randomState_ = 0;
  uint8_t frame_ = 0;
};

} // namespace

Animation& cellularLifeAnimation() {
  static CellularLifeAnimation animation;
  return animation;
}
