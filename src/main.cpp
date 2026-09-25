#include <Arduino.h>

#include "display/display_controller.h"
#include "keyboard/input_controller.h"
#include "usb/hid_device.h"

namespace {

constexpr uint32_t kOnboardLedPin = PB2; // P1.21 / B2 / MCU PB2.
constexpr uint32_t kHeartbeatPeriodMs = 1000;
constexpr uint32_t kHeartbeatOnMs = 60;

DisplayController displayController;
InputController inputController;

DisplayStatus currentDisplayStatus() {
  return {
      inputController.layoutBadgeLabel(),
      inputController.keyCaptureText(),
      inputController.keystrokeCount(),
      inputController.isNumLockActive(),
      inputController.isInsertModeActive(),
      inputController.isKeyCaptureActive(),
      inputController.isKeyActive(KeyboardKey::Function),
      inputController.isGameModeActive(),
      &inputController.settingsMenu(),
  };
}

void celebratePendingKeystrokeMilestone(uint32_t now) {
  uint32_t milestoneCount = 0;
  if (inputController.takeKeystrokeMilestone(milestoneCount)) {
    displayController.celebrateKeystrokeMilestone(now, milestoneCount);
  }
}

void applyPendingAnimationControl(uint32_t now) {
  int8_t direction = 0;
  if (inputController.takeAnimationStep(direction)) {
    displayController.stepAnimation(now, direction);
  }
}

void updateOnboardActivityLed(uint32_t now) {
  // A short pulse proves that the main loop is alive. Holding any electrically
  // detected matrix key keeps the LED on continuously.
  const bool isHeartbeatActive = now % kHeartbeatPeriodMs < kHeartbeatOnMs;
  const bool isMatrixKeyActive = inputController.isRawKeyActive();
  digitalWrite(
      kOnboardLedPin, isHeartbeatActive || isMatrixKeyActive ? HIGH : LOW);
}

} // namespace

void serviceHostDisplay(uint32_t now) {
  uint8_t displayPacket[hostDisplay::packetSize];
  if (takeUsbDisplayPacket(displayPacket)) {
    finishUsbDisplayPacket(
        displayController.receiveHostPacket(displayPacket, now));
  }
}

void serviceKeyboardDuringDisplay() {
  const uint32_t now = millis();
  inputController.service(now);
  // Drain one packet between OLED bursts, keeping USB progress independent of
  // panel refresh time. Host frames never mutate the panel's transfer buffer.
  serviceHostDisplay(now);
}

void setup() {
  pinMode(kOnboardLedPin, OUTPUT);
  digitalWrite(kOnboardLedPin, HIGH);

  // USB HID starts before the nonblocking OLED initialization.
  inputController.begin();
  displayController.begin(serviceKeyboardDuringDisplay);
  digitalWrite(kOnboardLedPin, LOW);
}

void loop() {
  const uint32_t now = millis();
  inputController.service(now);
  inputController.serviceSettings();
  serviceHostDisplay(now);
  if (inputController.takeActivity()) {
    displayController.onActivity(now);
  }
  if (inputController.takeDisplayRecoveryRequest()) {
    displayController.requestRecovery();
  }
  applyPendingAnimationControl(now);
  inputController.setInteractiveAnimation(
      displayController.isInteractiveAnimation());
  uint8_t animationKey;
  bool isGameMode;
  while (inputController.takeAnimationKey(animationKey, isGameMode)) {
    displayController.onKeyPress(animationKey, isGameMode);
  }
  celebratePendingKeystrokeMilestone(now);
  updateOnboardActivityLed(now);
  displayController.render(now, currentDisplayStatus());
}
