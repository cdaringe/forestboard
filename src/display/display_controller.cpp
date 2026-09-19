#include "display/display_controller.h"

#include "config/firmware_config.h"
#include "display/effects/oled_shading.h"

DisplayController::DisplayController() = default;

void DisplayController::begin(void (*serviceInput)()) {
  serviceInput_ = serviceInput;
  if (firmwareConfig::isOledDebugMode) {
    diagnosticRenderer_.reset();
  } else {
    animationManager_.begin();
  }
  lastAnimationChangeAt_ = millis();
  startRecovery(millis());
}

void DisplayController::requestRecovery() {
  isRecoveryRequested_ = true;
}

void DisplayController::onKeyPress(uint8_t usage, bool isGameMode) {
  if (!firmwareConfig::isOledDebugMode) {
    animationManager_.onKeyPress(usage, isGameMode);
  }
}

bool DisplayController::isInteractiveAnimation() const {
  return !firmwareConfig::isOledDebugMode && animationManager_.isInteractive();
}

void DisplayController::startRecovery(uint32_t now) {
  isReady_ = false;
  isRecovering_ = false;
  isRecoveryRequested_ = false;
  lastRecoveryAttemptAt_ = now;
  if (!isAllocated_) {
    isAllocated_ = display_.allocate();
  }
  if (!isAllocated_) {
    return;
  }
  display_.reset();
  if (!display_.configure(true)) {
    return;
  }
  recoveryStartedAt_ = millis();
  isRecovering_ = true;
}

bool DisplayController::isRecoveryDue(uint32_t now) const {
  const bool isRetryWaiting = !isReady_ && !isRecovering_;
  const bool isRetryDue = now - lastRecoveryAttemptAt_ >= 1000;
  return isRecoveryRequested_ || (isRetryWaiting && isRetryDue);
}

bool DisplayController::isPowerSettled(uint32_t now) const {
  return isRecovering_ && now - recoveryStartedAt_ >= 100;
}

bool DisplayController::isFrameDue(uint32_t now) const {
  return isReady_ && now - lastFrameAt_ >= firmwareConfig::oledFrameIntervalMs;
}

bool DisplayController::isConfigurationRefreshDue(uint32_t now) const {
  return now - lastRefreshAt_ >= firmwareConfig::oledRefreshMs;
}

void DisplayController::serviceRecovery(uint32_t now) {
  if (isRecoveryDue(now)) {
    startRecovery(now);
    return; // The reset captured a newer millis(); avoid subtracting stale now.
  }
  if (isPowerSettled(now)) {
    isReady_ = true;
  }
}

void DisplayController::updateAnimationSelection(
    uint32_t now, bool isGameMode) {
  if (firmwareConfig::isOledDebugMode) {
    return;
  }
  if (isGameMode) {
    lastAnimationChangeAt_ = now;
  } else {
    rotateAnimationWhenDue(now);
  }
}

void DisplayController::renderAnimationFrame(
    uint32_t now, const DisplayStatus& status) {
  forwardNewKeystrokes(status.keystrokeCount);
  animationManager_.render(display_, now);
  if (firmwareConfig::isOledShading) {
    applyOledShading(display_);
  }
  keyCaptureOverlay_.render(
      display_, status.isKeyCaptureActive, status.capturedKeys);
  milestoneEffect_.render(display_, now);
}

void DisplayController::renderScene(uint32_t now, const DisplayStatus& status) {
  if (firmwareConfig::isOledDebugMode) {
    diagnosticRenderer_.render(display_, now);
  } else {
    renderAnimationFrame(now, status);
  }
  statusBar_.render(display_, status);
}

void DisplayController::refreshConfiguration(uint32_t now) {
  if (!display_.configure(false)) {
    requestRecovery();
    return;
  }
  lastRefreshAt_ = now;
}

void DisplayController::sendFrame(bool isPowerOnNeeded) {
  if (isRecoveryRequested_) {
    return;
  }
  if (!display_.transferFrame(serviceInput_)) {
    requestRecovery();
    return;
  }
  if (!isPowerOnNeeded) {
    return;
  }
  // Reveal a complete frame; never a blank/white test or restarted animation.
  if (!display_.powerOn()) {
    requestRecovery();
    return;
  }
  isRecovering_ = false;
}

void DisplayController::render(uint32_t now, const DisplayStatus& status) {
  serviceRecovery(now);
  if (!isFrameDue(now)) {
    return;
  }
  lastFrameAt_ = now;
  updateAnimationSelection(now, status.isGameModeActive);
  renderScene(now, status);
  const bool isRefreshDue = isConfigurationRefreshDue(now);
  if (isRefreshDue) {
    refreshConfiguration(now);
  }
  sendFrame(isRecovering_ || isRefreshDue);
}

void DisplayController::stepAnimation(uint32_t now, int8_t direction) {
  if (!isReady_ || firmwareConfig::isOledDebugMode || direction == 0) {
    return;
  }

  if (direction > 0) {
    animationManager_.next();
  } else {
    animationManager_.previous();
  }
  lastAnimationChangeAt_ = now;
}

void DisplayController::celebrateKeystrokeMilestone(
    uint32_t now, uint32_t keystrokeCount) {
  if (firmwareConfig::isOledDebugMode) {
    return;
  }
  milestoneEffect_.start(now, keystrokeCount);
}

void DisplayController::rotateAnimationWhenDue(uint32_t now) {
  if (now - lastAnimationChangeAt_ < firmwareConfig::oledAnimationDurationMs) {
    return;
  }
  animationManager_.next();
  lastAnimationChangeAt_ = now;
}

void DisplayController::forwardNewKeystrokes(uint32_t keystrokeCount) {
  if (!isKeystrokeBaselineKnown_ ||
      keystrokeCount < lastObservedKeystrokeCount_) {
    lastObservedKeystrokeCount_ = keystrokeCount;
    isKeystrokeBaselineKnown_ = true;
    return;
  }
  if (keystrokeCount == lastObservedKeystrokeCount_) {
    return;
  }

  // A display frame normally contains only a few presses. Bound the
  // replay work after an unusual stall while preserving the newest events.
  constexpr uint32_t kMaximumEventsPerFrame = 32;
  const uint32_t newEventCount = keystrokeCount - lastObservedKeystrokeCount_;
  const uint32_t eventsToForward = newEventCount > kMaximumEventsPerFrame
      ? kMaximumEventsPerFrame
      : newEventCount;
  const uint32_t firstSequence = keystrokeCount - eventsToForward + 1;
  for (uint32_t offset = 0; offset < eventsToForward; ++offset) {
    animationManager_.onKeystroke(firstSequence + offset);
  }
  lastObservedKeystrokeCount_ = keystrokeCount;
}
