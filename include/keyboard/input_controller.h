#pragma once

#include <Arduino.h>

#include "settings/settings_menu.h"
#include "storage/keystroke_counter.h"

enum class KeyboardKey : uint8_t {
  Function,
  Insert,
  LayoutSwitch,
  NumLock,
};

class InputController {
public:
  void begin();
  void serviceSettings();
  const SettingsMenu& settingsMenu() const {
    return settingsMenu_;
  }
  void service(uint32_t now);
  bool takeActivity();
  bool isRawKeyActive() const;
  bool isNumLockActive() const;
  bool isInsertModeActive() const;
  bool isKeyCaptureActive() const;
  bool isKeyActive(KeyboardKey key) const;
  const char* keyCaptureText() const;
  const char* layoutBadgeLabel() const;
  uint32_t keystrokeCount() const;
  bool takeKeystrokeMilestone(uint32_t& milestoneCount);
  bool takeAnimationStep(int8_t& direction);
  bool takeDisplayRecoveryRequest();
  bool takeAnimationKey(uint8_t& usage, bool& isGameMode);
  bool isGameModeActive() const;
  void setInteractiveAnimation(bool isActive);

private:
  enum class KeyboardLayout : uint8_t {
    Colemak,
    Qwerty,
  };

  struct ScanResult {
    bool isAnyPressed = false;
    bool isReportChanged = false;
  };
  void scanMatrix(uint32_t now);
  ScanResult scanRow(uint8_t row, uint32_t now);
  ScanResult scanKey(uint8_t row, uint8_t col, uint32_t now);
  bool isDebounceComplete(
      uint8_t row, uint8_t col, bool isPressed, uint32_t now) const;
  bool isSettingsHoldDue(uint32_t now) const;
  void updateSettingsMenu(uint32_t now);
  void beginLayoutKeyPress(uint32_t now);
  void finishLayoutKeyPress();
  void queueAnimationKey(uint8_t usage, bool isGameInput);
  void captureAnimationKey(uint8_t row, uint8_t col);
  void updateNumLockGesture(uint8_t row, uint8_t col, uint32_t now);
  bool isNumLockGestureComplete() const;
  void toggleCaptureMode();
  bool isMediaReleaseDue(uint32_t now) const;
  bool isMediaPressDue(uint32_t now) const;
  uint8_t pendingMediaKey() const;
  uint8_t nextMediaKey(uint32_t now) const;
  uint8_t consumerButtons(uint8_t mediaKey) const;
  void applyMediaTransition(uint8_t nextKey, uint32_t now);
  void appendCaptureCharacter(char character);
  void scanEncoder();
  void queueEncoderStep(int8_t direction);
  void serviceMediaKey(uint32_t now);
  void handleKeyPress(uint8_t row, uint8_t col, uint32_t now);
  void handleKeyRelease(uint8_t row, uint8_t col);
  void toggleKeyboardLayout();
  uint8_t activeUsageAt(uint8_t row, uint8_t col) const;
  uint8_t logicalUsageAt(uint8_t row, uint8_t col) const;
  void appendCaptureToken(const char* token);
  void sendReport();

  bool isRawPressed_[12][12] = {};
  bool isPressed_[12][12] = {};
  uint32_t rawChangedAt_[12][12] = {};
  uint32_t lastScanAt_ = 0;
  uint32_t lastReportAt_ = 0;
  uint32_t mediaKeyPressedAt_ = 0;
  uint32_t mediaKeyReleasedAt_ = 0;
  uint8_t lastConsumerReport_ = 0;
  uint8_t encoderState_ = 0;
  uint8_t mediaKey_ = 0;
  int8_t encoderDelta_ = 0;
  int8_t pendingEncoderSteps_ = 0;
  int8_t pendingAnimationSteps_ = 0;
  bool isRawKeyActive_ = false;
  bool isActivityPending_ = false;
  bool isInsertModeActive_ = false;
  bool isLayoutKeyUsedForRecovery_ = false;
  bool isDisplayRecoveryPending_ = false;
  bool isLayoutKeyHeldForSettings_ = false;
  bool isInteractiveAnimation_ = false;
  uint32_t layoutKeyPressedAt_ = 0;
  bool isCapturedForGame_[12][12] = {};
  struct AnimationKey {
    uint8_t usage;
    bool isGameMode;
  };
  static constexpr uint8_t kAnimationKeyCapacity = 16;
  AnimationKey animationKeys_[kAnimationKeyCapacity] = {};
  uint8_t animationKeyHead_ = 0;
  uint8_t animationKeyTail_ = 0;
  uint8_t rapidNumLockClicks_ = 0;
  uint32_t lastNumLockClickAt_ = 0;
  char keyCaptureText_[22] = {};
  KeyboardLayout activeLayout_ = KeyboardLayout::Colemak;
  KeystrokeCounter keystrokeCounter_;
  SettingsMenu settingsMenu_;
};
