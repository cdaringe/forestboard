#include <Arduino.h>

namespace {

// Onboard LED -> P1.21 / B2 -> PB2 (active high).
constexpr uint32_t kOnboardLedPin = PB2;

// Matrix net -> WeAct header pin / board label -> MCU GPIO.
// Mapping is from keyboard/a10/keyboard/keyboard.kicad_sch.
constexpr uint32_t kMatrixPins[] = {
    PC0,   // ROW0  -> P1.5  / C0  -> PC0
    PC2,   // ROW1  -> P1.7  / C2  -> PC2
    PC1,   // ROW2  -> P1.8  / C1  -> PC1
    PA0,   // ROW3  -> P1.9  / A0  -> PA0
    PC3,   // ROW4  -> P1.10 / C3  -> PC3
    PA2,   // ROW5  -> P1.11 / A2  -> PA2
    PA1,   // ROW6  -> P1.12 / A1  -> PA1
    PA4,   // ROW7  -> P1.13 / A4  -> PA4
    PA3,   // ROW8  -> P1.14 / A3  -> PA3
    PA6,   // ROW9  -> P1.15 / A6  -> PA6
    PA5,   // ROW10 -> P1.16 / A5  -> PA5
    PC4,   // ROW11 -> P1.17 / C4  -> PC4
    PB9,   // COL0  -> P2.1  / B9  -> PB9
    PB8,   // COL1  -> P2.4  / B8  -> PB8
    PB5,   // COL2  -> P2.5  / B5  -> PB5
    PB3,   // COL3  -> P2.7  / B3  -> PB3
    PB4,   // COL4  -> P2.8  / B4  -> PB4
    PC12,  // COL5  -> P2.9  / C12 -> PC12
    PD2,   // COL6  -> P2.10 / D2  -> PD2
    PC10,  // COL7  -> P2.11 / C10 -> PC10
    PC11,  // COL8  -> P2.12 / C11 -> PC11
    PA15,  // COL9  -> P2.14 / A15 -> PA15
    PA10,  // COL10 -> P2.15 / A10 -> PA10
    PA8,   // COL11 -> P2.17 / A8  -> PA8
};

constexpr uint32_t kStartupOnMs = 1000;
constexpr uint32_t kStartupOffMs = 250;
constexpr uint32_t kMarkerOnMs = 10;
constexpr uint32_t kMarkerOffMs = 10;
constexpr uint32_t kPinOnMs = 50;
constexpr uint32_t kPinOffMs = 50                                                                                                                                                                                                                                                  ;

void showStartupIndicator() {
  // A long, unmistakable pulse means the application has left the DFU
  // bootloader and started running.
  digitalWrite(kOnboardLedPin, HIGH);
  delay(kStartupOnMs);
  digitalWrite(kOnboardLedPin, LOW);
  delay(kStartupOffMs);
}

void flashOnboardLedTwice() {
  for (uint8_t flash = 0; flash < 2; ++flash) {
    digitalWrite(kOnboardLedPin, HIGH);
    delay(kMarkerOnMs);
    digitalWrite(kOnboardLedPin, LOW);
    delay(kMarkerOffMs);
  }
}

void flashMatrixPin(uint32_t pin) {
  // Preload HIGH before enabling the output to avoid a brief low-going glitch.
  digitalWrite(pin, HIGH);
  pinMode(pin, OUTPUT);
  delay(kPinOnMs);

  digitalWrite(pin, LOW);
  delay(kPinOffMs);

  // Leave inactive matrix nets high-impedance so they cannot fight each other.
  pinMode(pin, INPUT);
}

}  // namespace

void setup() {
  digitalWrite(kOnboardLedPin, LOW);
  pinMode(kOnboardLedPin, OUTPUT);

  for (uint32_t pin : kMatrixPins) {
    pinMode(pin, INPUT);
  }

  showStartupIndicator();
}

void loop() {
  for (uint32_t pin : kMatrixPins) {
    flashOnboardLedTwice();
    flashMatrixPin(pin);
  }
}
