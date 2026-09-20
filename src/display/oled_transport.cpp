#include "display/oled_transport.h"
#include "config/firmware_config.h"
#include <SPI.h>

namespace {
constexpr uint32_t kReset = PC9;
constexpr uint32_t kMosi = PB15;
constexpr uint32_t kDc = PB14;
constexpr uint32_t kClock = PB13;
constexpr uint32_t kCs = PB12;
#if !FORESTBOARD_OLED_SOFTWARE_SPI
// Explicit SPI2 pins, with MISO disconnected: PB14 must remain OLED D/C.
SPIClass oledSpi(kMosi, PNUM_NOT_DEFINED, kClock);
#endif
} // namespace

#if FORESTBOARD_OLED_SOFTWARE_SPI
OledTransport::OledTransport()
    : Adafruit_SH1107(128, 128, kMosi, kClock, kDc, kReset, kCs) {}
#else
OledTransport::OledTransport()
    : Adafruit_SH1107(
          128, 128, &oledSpi, kDc, kReset, kCs, FORESTBOARD_OLED_SPI_HZ) {}
#endif

bool OledTransport::allocate() {
  return _init(0, false);
}

void OledTransport::reset() {
  pinMode(kReset, OUTPUT);
  digitalWrite(kReset, LOW);
  delayMicroseconds(20); // Datasheet: >=10 us low, <=2 us internal reset.
  digitalWrite(kReset, HIGH);
  delayMicroseconds(20);
}

bool OledTransport::configure(bool isAfterReset) {
  if (isAfterReset) {
    const uint8_t power[] = {0xAE, 0xAD, 0x8A};
    if (!oled_commandList(power, sizeof(power))) {
      return false;
    }
  }
  // Two NOPs consume a possible stray pending command parameter. Preserve
  // the vendor's orientation/contrast/precharge; select the fastest specified
  // oscillator and divide by 1. Live repair never turns the display off.
  const uint8_t settings[] = {
      0xE3,
      0xE3,
      0xD5,
      0xF0,
      0x20,
      0x81,
      static_cast<uint8_t>(configuration().contrast()),
      0xA0,
      0xC0,
      0xDC,
      0x00,
      0xD3,
      0x00,
      0xD9,
      0x22,
      0xDB,
      0x35,
      0xA8,
      0x7F,
      0xA4,
      0xA6,
  };
  return oled_commandList(settings, sizeof(settings));
}

bool OledTransport::powerOn() {
  const uint8_t command = SH110X_DISPLAYON;
  return oled_commandList(&command, 1);
}

bool OledTransport::powerOff() {
  const uint8_t command = SH110X_DISPLAYOFF;
  return oled_commandList(&command, 1);
}

bool OledTransport::transferFrame(void (*serviceInput)()) {
  // Always refresh every page, repairing silent display RAM corruption.
  // Yield to the keyboard between 128-byte pages (~0.35 ms at 3 MHz), so a
  // full-frame transfer does not stall matrix/encoder scanning for ~6 ms.
  for (uint8_t page = 0; page < 16; ++page) {
    const uint8_t address[] = {static_cast<uint8_t>(0xB0 | page), 0x10, 0x00};
    if (!oled_commandList(address, sizeof(address))) {
      return false;
    }
    digitalWrite(kDc, HIGH);
    if (!spi_dev->write(getBuffer() + page * 128, 128)) {
      return false;
    }
    if (serviceInput != nullptr) {
      serviceInput();
    }
  }
  return true;
}
