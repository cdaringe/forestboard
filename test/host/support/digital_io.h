#pragma once
#include "Arduino.h"

constexpr uint32_t PB_13 = PB13, PB_15 = PB15;
constexpr uint32_t LL_GPIO_PIN_9 = 1U << 9, LL_GPIO_PIN_12 = 1U << 12,
                   LL_GPIO_PIN_13 = 1U << 13, LL_GPIO_PIN_14 = 1U << 14,
                   LL_GPIO_PIN_15 = 1U << 15;
constexpr uint32_t LL_GPIO_SPEED_FREQ_MEDIUM = 1;
constexpr uint32_t GPIOB = 16, GPIOC = 32;
inline uint32_t pinSpeeds[64] = {};
inline void LL_GPIO_SetPinSpeed(uint32_t port, uint32_t mask, uint32_t speed) {
  pinSpeeds[port + __builtin_ctz(mask)] = speed;
}
inline void digitalWriteFast(uint32_t pin, uint32_t value) {
  digitalWrite(pin, value);
}
