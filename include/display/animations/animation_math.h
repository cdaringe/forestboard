#pragma once

#include <Arduino.h>

namespace animationMath {

constexpr int8_t kSineTable[] = {
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

inline int16_t sine(uint8_t phase) {
  return kSineTable[phase >> 2];
}

inline uint32_t nextRandom(uint32_t& state) {
  state = state * 1664525UL + 1013904223UL;
  return state;
}

inline uint8_t randomByte(uint32_t& state) {
  return static_cast<uint8_t>(nextRandom(state) >> 24);
}

} // namespace animationMath
