#pragma once
#include "config/configuration.h"

// Stable semantic action IDs travel through the animation queue. Physical
// key choices are resolved before layout translation in the input controller.
namespace bikeControls {
constexpr uint8_t up = 0x52, down = 0x51, jump = 0x0D;
constexpr uint8_t backflip = 0x05, cancan = 0x06, spin = 0x17, wheelie = 0x1A;
inline uint8_t actionForPhysicalKey(uint8_t usage) {
  const auto& config = configuration();
  if (usage == config.mtbBackflipKey()) {
    return backflip;
  }
  if (usage == config.mtbCancanKey()) {
    return cancan;
  }
  if (usage == config.mtbSpinKey()) {
    return spin;
  }
  if (usage == config.mtbWheelieKey()) {
    return wheelie;
  }
  if (usage == config.mtbJumpKey()) {
    return jump;
  }
  if (usage == config.mtbUpKey()) {
    return up;
  }
  if (usage == config.mtbDownKey()) {
    return down;
  }
  return 0;
}
} // namespace bikeControls
