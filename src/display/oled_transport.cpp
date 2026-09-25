#include "display/oled_transport.h"
#include "config/firmware_config.h"
#include <SPI.h>
#if FORESTBOARD_OLED_SOFTWARE_SPI
#include <digital_io.h>
#include <dwt.h>
#endif

namespace {
constexpr uint32_t kReset = PC9;
constexpr uint32_t kMosi = PB15;
constexpr uint32_t kDc = PB14;
constexpr uint32_t kClock = PB13;
constexpr uint32_t kCs = PB12;
#if FORESTBOARD_OLED_SOFTWARE_SPI
inline void waitCycles(uint32_t ticks) {
  const uint32_t start = dwt_getCycles();
  while (static_cast<uint32_t>(dwt_getCycles() - start) < ticks) {
  }
}
#endif
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
#if FORESTBOARD_OLED_SOFTWARE_SPI
  // STM32duino starts DWT before setup(). Refuse an inactive timebase rather
  // than hanging the keyboard in a display delay if initialization failed.
  const uint32_t first = dwt_getCycles();
  if (dwt_getCycles() == first) {
    return false;
  }
  halfCycleTicks_ = (SystemCoreClock + 2 * FORESTBOARD_OLED_SPI_HZ - 1) /
      (2 * FORESTBOARD_OLED_SPI_HZ);
#endif
  if (!_init(0, false)) {
    return false;
  }
#if FORESTBOARD_OLED_SOFTWARE_SPI
  // pinMode defaults to VERY_HIGH slew. MEDIUM is specified for <=10 ns
  // rise/fall at 3.3 V / 50 pF on F411, within SH1107's 15 ns limit, while
  // reducing edge aggressiveness. Actual wiring still needs measurement.
  for (uint32_t pin :
      {LL_GPIO_PIN_12, LL_GPIO_PIN_13, LL_GPIO_PIN_14, LL_GPIO_PIN_15}) {
    LL_GPIO_SetPinSpeed(GPIOB, pin, LL_GPIO_SPEED_FREQ_MEDIUM);
  }
#endif
  return true;
}

bool OledTransport::writeBytes(
    const uint8_t* bytes, size_t length, bool isData) {
  digitalWrite(kDc, isData ? HIGH : LOW);
#if FORESTBOARD_OLED_SOFTWARE_SPI
  // Cycle timing preserves explicit setup/hold margins without imposing a
  // 40 ms full-frame delay. Fast pin writes use atomic STM32 BSRR accesses.
  // On Adafruit 5297, CS and D/C rise through diode/10k pull-up level
  // shifters. Allow 2 us for those signals, independently of the bit rate.
  waitCycles((SystemCoreClock + 499999UL) / 500000UL);
  digitalWrite(kCs, LOW);
  for (size_t index = 0; index < length; ++index) {
    for (uint8_t mask = 0x80; mask != 0; mask >>= 1) {
      digitalWriteFast(PB_15, (bytes[index] & mask) ? HIGH : LOW);
      waitCycles(halfCycleTicks_); // Data setup and clock low.
      digitalWriteFast(PB_13, HIGH);
      waitCycles(halfCycleTicks_); // Data hold and clock high.
      digitalWriteFast(PB_13, LOW);
    }
  }
  waitCycles(halfCycleTicks_); // CS hold after the final clock.
  digitalWrite(kCs, HIGH);
  return true; // Write-only bus: this is not a panel acknowledgement.
#else
  return spi_dev->write(bytes, length);
#endif
}

void OledTransport::holdReset() {
  digitalWrite(kCs, HIGH);
  digitalWrite(kReset, LOW); // Preload output latch before enabling the pin.
  pinMode(kReset, OUTPUT);
#if FORESTBOARD_OLED_SOFTWARE_SPI
  LL_GPIO_SetPinSpeed(GPIOC, LL_GPIO_PIN_9, LL_GPIO_SPEED_FREQ_MEDIUM);
#endif
  digitalWrite(kReset, LOW);
}

void OledTransport::releaseReset() {
  digitalWrite(kReset, HIGH);
  delayMicroseconds(20); // >=2 us internal reset completion.
}

bool OledTransport::configure(bool isAfterReset) {
  if (isAfterReset) {
    // Adafruit 5297 provides VPP with its own boost converter; disable the
    // SH1107's internal converter, matching Adafruit's driver.
    const uint8_t power[] = {0xAE, 0xAD, 0x8A};
    if (!writeBytes(power, sizeof(power), false)) {
      return false;
    }
  }
  // Two NOPs consume a possible stray pending command parameter. Preserve
  // Adafruit's panel clock/drive settings. Live repair never turns it off.
  const uint8_t settings[] = {
      0xE3,
      0xE3,
      0xEE, // Exit any stray read-modify-write mode before restoring addresses.
      0xD5,
      0x51, // Adafruit baseline: nominal oscillator, divide by 2.
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
  return writeBytes(settings, sizeof(settings), false);
}

bool OledTransport::powerOn() {
  const uint8_t command = SH110X_DISPLAYON;
  return writeBytes(&command, 1, false);
}

bool OledTransport::powerOff() {
  const uint8_t command = SH110X_DISPLAYOFF;
  return writeBytes(&command, 1, false);
}

bool OledTransport::transferFrame(void (*serviceInput)()) {
  // Rewrite all RAM in short independently addressed bursts. A lost command
  // or stray addressing mode must not poison subsequent bursts/frames.
  // At 2 MHz, 39 bytes need 156 us plus GPIO/callback overhead. Service
  // input with CS high after each burst; never mask keyboard/USB interrupts.
  constexpr uint16_t burstBytes = 32;
  for (uint16_t offset = 0; offset < 2048; offset += burstBytes) {
    const uint8_t column = offset % 128;
    const uint8_t address[] = {0xE3, 0xE3, 0xEE, 0x20,
        static_cast<uint8_t>(0xB0 | (offset / 128)),
        static_cast<uint8_t>(0x10 | (column >> 4)),
        static_cast<uint8_t>(column & 0x0F)};
    if (!writeBytes(address, sizeof(address), false)) {
      return false;
    }
    if (!writeBytes(getBuffer() + offset, burstBytes, true)) {
      return false;
    }
    if (serviceInput != nullptr) {
      serviceInput();
    }
  }
  return true;
}
