#pragma once
#include <stdint.h>

// Motion used to advance once per 200 ms display frame. Keep that speed while
// rendering more often; continuous positions use fractional steps, discrete
// events (Life generations, energy decay) use isTickDue(). Bound long stalls.
class AnimationClock {
public:
  void reset() {
    isStarted_ = false;
    isTickDue_ = false;
    elapsed_ = 0;
  }
  float advance(uint32_t now) {
    uint32_t delta = isStarted_ ? now - previous_ : 0;
    previous_ = now;
    isStarted_ = true;
    if (delta > 200) {
      delta = 200;
    }
    const uint32_t oldTick = elapsed_ / 200;
    elapsed_ += delta;
    isTickDue_ = elapsed_ / 200 != oldTick;
    return delta / 200.0f;
  }
  bool isTickDue() const {
    return isTickDue_;
  }
  uint32_t frame() const {
    return elapsed_ / 200;
  }
  uint8_t phase(uint8_t speed = 1, uint8_t offset = 0) const {
    return static_cast<uint8_t>(elapsed_ * speed / 200 + offset);
  }

private:
  uint32_t previous_ = 0;
  uint32_t elapsed_ = 0;
  bool isStarted_ = false;
  bool isTickDue_ = false;
};
