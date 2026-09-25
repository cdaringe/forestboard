#pragma once

#include <cstdint>
#include <cstring>

#include "display/host_protocol.h"

// Protocol v1: version, opcode, frame ID LE16, offset LE16, length, reserved,
// then up to 56 bytes. Pixels are row-major, MSB first, one = lit.
class HostFrame {
public:
  bool receive(const hostDisplay::Packet& packet, uint32_t now) {
    if (!packet.isValidHeader()) {
      return false;
    }
    const uint16_t id = packet.frameId;
    const uint16_t offset = packet.offset;
    const uint8_t length = packet.payloadSize;
    switch (packet.opcode) {
    case hostDisplay::Opcode::BeginFrame:
      // Replace an unfinished frame while preserving visible pixels.
      if (offset != 0 || length != 0) {
        return false;
      }
      receiving_ = true;
      id_ = id;
      received_ = 0;
      return true;
    case hostDisplay::Opcode::WritePixels:
      if (!receiving_ || id != id_ || offset != received_ || length == 0 ||
          length > hostDisplay::payloadCapacity ||
          offset + length > sizeof(staging_)) {
        return false;
      }
      memcpy(staging_ + offset, packet.payload, length);
      received_ += length;
      return true;
    case hostDisplay::Opcode::PresentFrame:
      if (!receiving_ || id != id_ || received_ != sizeof(staging_) ||
          offset != 0 || length != 0) {
        return false;
      }
      memcpy(visible_, staging_, sizeof(visible_));
      receiving_ = false;
      active_ = true;
      presentedAt_ = now;
      return true;
    case hostDisplay::Opcode::ReleaseDisplay:
      if (offset != 0 || length != 0) {
        return false;
      }
      receiving_ = active_ = false;
      return true;
    default:
      return false;
    }
  }

  void expire(uint32_t now) {
    if (active_ && now - presentedAt_ >= hostDisplay::frameTimeoutMs) {
      active_ = false;
    }
  }
  bool isActive() const {
    return active_;
  }
  const uint8_t* pixels() const {
    return visible_;
  }

private:
  uint8_t staging_[hostDisplay::frameSize] = {};
  uint8_t visible_[hostDisplay::frameSize] = {};
  uint16_t id_ = 0;
  uint16_t received_ = 0;
  uint32_t presentedAt_ = 0;
  bool receiving_ = false;
  bool active_ = false;
};
