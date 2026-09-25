#pragma once

#include <Adafruit_SH110X.h>
#include <Arduino.h>

#include "display/animations/animation_manager.h"
#include "display/diagnostics/oled_diagnostic_renderer.h"
#include "display/display_status.h"
#include "display/effects/keystroke_milestone_effect.h"
#include "display/oled_transport.h"
#include "display/widgets/key_capture_overlay.h"
#include "display/widgets/status_bar.h"

class DisplayController {
public:
  DisplayController();

  void begin(void (*serviceInput)() = nullptr);
  void requestRecovery();
  void onActivity(uint32_t now);
  void onKeyPress(uint8_t usage, bool isGameMode);
  bool isInteractiveAnimation() const;
  void render(uint32_t now, const DisplayStatus& status);
  void stepAnimation(uint32_t now, int8_t direction);
  void celebrateKeystrokeMilestone(uint32_t now, uint32_t keystrokeCount);

private:
  void startRecovery(uint32_t now);
  bool isRecoveryDue(uint32_t now) const;
  bool isPowerSettled(uint32_t now) const;
  bool isFrameDue(uint32_t now) const;
  bool isConfigurationRefreshDue(uint32_t now) const;
  void serviceRecovery(uint32_t now);
  void updateAnimationSelection(uint32_t now, bool isGameMode);
  void renderScene(uint32_t now, const DisplayStatus& status);
  void renderAnimationFrame(uint32_t now, const DisplayStatus& status);
  void refreshConfiguration(uint32_t now);
  void sendFrame(bool isPowerOnNeeded);
  void rotateAnimationWhenDue(uint32_t now);
  void forwardNewKeystrokes(uint32_t keystrokeCount);

  OledTransport display_;
  AnimationManager animationManager_;
  KeyCaptureOverlay keyCaptureOverlay_;
  KeystrokeMilestoneEffect milestoneEffect_;
  OledDiagnosticRenderer diagnosticRenderer_;
  StatusBar statusBar_;
  uint32_t splashShownAt_ = 0;
  bool isSplashPending_ = false;
  bool isSplashVisible_ = false;
  uint32_t lastFrameAt_ = 0;
  uint32_t lastActivityAt_ = 0;
  bool isIdle_ = false;
  bool isPoweredOff_ = false;
  uint32_t lastAnimationChangeAt_ = 0;
  uint32_t lastObservedKeystrokeCount_ = 0;
  bool isReady_ = false;
  bool isKeystrokeBaselineKnown_ = false;
  bool isAllocated_ = false;
  bool isRecovering_ = false;
  bool isResetHeld_ = false;
  bool isRecoveryRequested_ = false;
  bool isStartupRetryPending_ = false;
  uint32_t startupStartedAt_ = 0;
  uint32_t recoveryStartedAt_ = 0;
  uint32_t lastRecoveryAttemptAt_ = 0;
  uint32_t lastRefreshAt_ = 0;
  uint32_t configuredContrast_ = 0;
  void (*serviceInput_)() = nullptr;
};
