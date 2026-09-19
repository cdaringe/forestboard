#pragma once
#include <stdint.h>
constexpr int HID_KEYBOARD = 1;
inline uint8_t keyboardReport[8] = {};
inline void HID_Composite_Init(int) {}
inline void HID_Composite_keyboard_sendReport(uint8_t* data, uint16_t n) {
  memcpy(keyboardReport, data, n);
}
