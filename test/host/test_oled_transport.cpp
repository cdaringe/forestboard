#include "Arduino.h"
#include "digital_io.h"
#include "display/oled_transport.h"
#include "dwt.h"
#include <array>
#include <cassert>
#include <cstdio>

namespace {
// Decode the actual GPIO stream with a small SH1107 addressing model. This
// checks the production software writer, not a mocked successful SPI write.
std::array<uint8_t, 2048> panelRam;
uint8_t page = 11, column = 73, savedColumn = 0;
bool isVertical = true, isParameterPending = false;
bool isReadModifyWrite = false;
uint8_t receivedByte = 0, receivedBits = 0;
uint64_t clockChangedAt = 0, dataChangedAt = 0, dcChangedAt = 0;
uint64_t csChangedAt = 0, lastRisingAt = 0, lastServiceAt = 0;
unsigned burstLength = 0, serviced = 0;
bool injectAddressFault = false;
bool injectPixelFault = false;

uint64_t elapsedNs(uint64_t start) {
  return (fakeCycles - start) * 1000000000ULL / SystemCoreClock;
}

void command(uint8_t value) {
  if (isParameterPending) {
    isParameterPending = false;
  } else if (value == 0x20 || value == 0x21) {
    isVertical = value == 0x21;
  } else if ((value & 0xF0) == 0xB0) {
    page = value & 0x0F;
  } else if ((value & 0xF8) == 0x10) {
    column = (column & 0x0F) | ((value & 7) << 4);
  } else if ((value & 0xF0) == 0x00) {
    column = (column & 0x70) | value;
  } else if (value == 0xE0) {
    isReadModifyWrite = true;
    savedColumn = column;
  } else if (value == 0xEE && isReadModifyWrite) {
    isReadModifyWrite = false;
    column = savedColumn;
  } else if (value == 0x81 || value == 0xAD || value == 0xD5 || value == 0xDC ||
      value == 0xD3 || value == 0xD9 || value == 0xDB || value == 0xA8) {
    isParameterPending = true;
  }
}

void observeWrite(uint32_t pin, int value) {
  if (pin == PB14) {
    assert(pinValues[PB12] == HIGH); // D/C only changes outside a transaction.
    dcChangedAt = fakeCycles;
  } else if (pin == PB15) {
    assert(pinValues[PB13] == LOW);
    assert(elapsedNs(lastRisingAt) >= 250); // Data hold.
    dataChangedAt = fakeCycles;
  } else if (pin == PB12 && value != pinValues[pin]) {
    assert(pinValues[PB13] == LOW);
    if (value == LOW) {
      assert(elapsedNs(csChangedAt) >= 2000); // Breakout's passive CS pull-up.
      burstLength = 0;
    } else {
      assert(receivedBits == 0);
      assert(elapsedNs(lastRisingAt) >= 250); // CS hold.
      if (pinValues[PB14] == HIGH) {
        assert(burstLength <= 32);
      }
    }
    csChangedAt = fakeCycles;
  } else if (pin == PB13 && value != pinValues[pin]) {
    assert(pinValues[PB12] == LOW);
    assert(elapsedNs(clockChangedAt) >= 250); // High and low pulse widths.
    clockChangedAt = fakeCycles;
    if (value == HIGH) {
      assert(elapsedNs(dataChangedAt) >= 250);
      assert(elapsedNs(dcChangedAt) >= 2000); // Passive D/C pull-up.
      assert(elapsedNs(csChangedAt) >= 250);
      lastRisingAt = fakeCycles;
      receivedByte = (receivedByte << 1) | pinValues[PB15];
      if (++receivedBits == 8) {
        if (pinValues[PB14] == HIGH) {
          panelRam[page * 128 + column] = receivedByte;
          if (isVertical) {
            page = (page + 1) % 16;
          } else {
            column = (column + 1) % 128;
          }
        } else {
          command(receivedByte);
        }
        receivedBits = receivedByte = 0;
        ++burstLength;
      }
    }
  }
}

void serviceInput() {
  assert(pinValues[PB12] == HIGH && pinValues[PB13] == LOW);
  // Delay-only budget; real GPIO, interrupts and input work add overhead.
  assert(elapsedNs(lastServiceAt) <= 200000);
  lastServiceAt = fakeCycles;
  ++serviced;
  if (injectAddressFault) {
    // Simulate corruption between bursts, including a pending parameter that
    // would otherwise consume the next page-address command.
    page = 14;
    column = 99;
    isVertical = true;
    isParameterPending = true;
    isReadModifyWrite = true;
    savedColumn = 47;
  }
  if (injectPixelFault && serviced == 8) {
    panelRam[3] ^= 0x80;
  }
}

void transfer(OledTransport& display) {
  serviced = 0;
  lastServiceAt = fakeCycles;
  const uint64_t startedAt = fakeCycles;
  assert(display.transferFrame(serviceInput));
  assert(serviced == 64);
  assert(elapsedNs(startedAt) < 14000000); // Deliberate delays, not wall time.
}
} // namespace

void testTransport(uint32_t cpuHz) {
  SystemCoreClock = cpuHz;
  OledTransport display;
  isDwtRunning = false;
  assert(!display.allocate()); // No stopped-counter hang during startup.
  isDwtRunning = true;
  assert(display.allocate());
  for (uint32_t pin : {PB12, PB13, PB14, PB15}) {
    assert(pinSpeeds[pin] == LL_GPIO_SPEED_FREQ_MEDIUM);
  }
  assert(pinSpeeds[PB2] == 0); // Other GPIOs are untouched.
  display.holdReset();
  assert(pinValues[PC9] == LOW && pinValues[PB12] == HIGH);
  display.releaseReset();
  assert(pinValues[PC9] == HIGH);
  // The GFX allocation stub doesn't initialize pins like the real library.
  pinValues[PB12] = HIGH;
  pinValues[PB13] = LOW;
  fakeCycles += cpuHz / 1000;
  digitalWriteObserver = observeWrite;
  assert(display.configure(true));
  assert(display.powerOn());
  for (unsigned i = 0; i < panelRam.size(); ++i) {
    display.getBuffer()[i] = static_cast<uint8_t>((i * 37) ^ (i >> 7));
  }
  injectAddressFault = true;
  transfer(display);
  assert(memcmp(panelRam.data(), display.getBuffer(), panelRam.size()) == 0);
  injectAddressFault = false;
  injectPixelFault = true;
  transfer(display);
  assert(memcmp(panelRam.data(), display.getBuffer(), panelRam.size()) != 0);
  injectPixelFault = false;
  // Force the hardware's 32-bit cycle counter to wrap during a frame.
  fakeCycles = ((fakeCycles >> 32) + 1) * (1ULL << 32) - 10;
  transfer(display);
  assert(memcmp(panelRam.data(), display.getBuffer(), panelRam.size()) == 0);
  assert(display.configure(false));
  assert(display.powerOff());
  digitalWriteObserver = nullptr;
}

int main() {
  testTransport(100000000); // F411
  testTransport(180000000); // F446
  puts("OLED waveform, address fault recovery, and RAM refresh tests passed");
}
