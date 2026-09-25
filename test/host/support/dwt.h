#pragma once
#include <cstdint>

inline uint32_t SystemCoreClock = 100000000;
inline uint64_t fakeCycles = 0;
inline bool isDwtRunning = true;
inline uint32_t dwt_getCycles() {
  if (isDwtRunning) {
    ++fakeCycles;
  }
  return static_cast<uint32_t>(fakeCycles);
}
