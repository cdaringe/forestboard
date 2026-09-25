#pragma once

#include <stdint.h>

namespace usbConsumer {
constexpr uint8_t mute = 1;
constexpr uint8_t volumeUp = 2;
constexpr uint8_t volumeDown = 4;
} // namespace usbConsumer

void installUsbHidSupport();
bool isUsbHostNumLockActive();

bool sendUsbConsumerReport(uint8_t buttons);

// Single-packet mailbox: finish only after processing the copied 64 bytes.
bool takeUsbDisplayPacket(uint8_t* packet);
void finishUsbDisplayPacket(bool accepted);
