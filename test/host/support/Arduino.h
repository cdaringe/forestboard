#pragma once
#include <algorithm>
#include <initializer_list>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>
using std::max;
using std::min;
inline uint32_t fakeNow = 0;
inline uint32_t millis() {
  return fakeNow;
}
inline void delay(uint32_t ms) {
  fakeNow += ms;
}
inline void delayMicroseconds(uint32_t) {}
constexpr int HIGH = 1, LOW = 0, OUTPUT = 1, INPUT = 0, INPUT_PULLUP = 2;
enum {
  PA0,
  PA1,
  PA2,
  PA3,
  PA4,
  PA5,
  PA6,
  PA7,
  PA8,
  PA9,
  PA10,
  PA11,
  PA12,
  PA13,
  PA14,
  PA15,
  PB0,
  PB1,
  PB2,
  PB3,
  PB4,
  PB5,
  PB6,
  PB7,
  PB8,
  PB9,
  PB10,
  PB11,
  PB12,
  PB13,
  PB14,
  PB15,
  PC0,
  PC1,
  PC2,
  PC3,
  PC4,
  PC5,
  PC6,
  PC7,
  PC8,
  PC9,
  PC10,
  PC11,
  PC12,
  PC13,
  PC14,
  PC15,
  PD2
};
inline int pinValues[64] = {};
inline void pinMode(uint32_t, int) {}
inline void digitalWrite(uint32_t pin, int value) {
  pinValues[pin] = value;
}
inline int digitalRead(uint32_t) {
  return HIGH;
}
template <class T, class L, class H> T constrain(T v, L l, H h) {
  return v < l ? l : v > h ? h : v;
}
