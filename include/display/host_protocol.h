#pragma once

#include <cstddef>
#include <cstdint>

namespace hostDisplay {

constexpr uint8_t protocolVersion = 1;
constexpr size_t packetSize = 64;
constexpr size_t payloadCapacity = 56;
constexpr size_t frameSize = 2048;
constexpr uint32_t frameTimeoutMs = 5000;

enum class Opcode : uint8_t {
  BeginFrame = 1,
  WritePixels = 2,
  PresentFrame = 3,
  ReleaseDisplay = 4,
};

namespace wireOffset {
constexpr size_t version = 0;
constexpr size_t opcode = 1;
constexpr size_t frameIdLow = 2;
constexpr size_t frameIdHigh = 3;
constexpr size_t offsetLow = 4;
constexpr size_t offsetHigh = 5;
constexpr size_t payloadSize = 6;
constexpr size_t reserved = 7;
constexpr size_t payload = 8;
} // namespace wireOffset

struct Packet {
  uint8_t version;
  Opcode opcode;
  uint16_t frameId;
  uint16_t offset;
  uint8_t payloadSize;
  uint8_t reserved;
  const uint8_t* payload;

  bool isValidHeader() const {
    return version == protocolVersion && reserved == 0;
  }
};

// Decode explicitly: the wire format does not depend on struct packing.
inline Packet decodePacket(const uint8_t* bytes) {
  return {
      bytes[wireOffset::version],
      static_cast<Opcode>(bytes[wireOffset::opcode]),
      static_cast<uint16_t>(bytes[wireOffset::frameIdLow] |
          (uint16_t(bytes[wireOffset::frameIdHigh]) << 8)),
      static_cast<uint16_t>(bytes[wireOffset::offsetLow] |
          (uint16_t(bytes[wireOffset::offsetHigh]) << 8)),
      bytes[wireOffset::payloadSize],
      bytes[wireOffset::reserved],
      bytes + wireOffset::payload,
  };
}

} // namespace hostDisplay
