#include "display/display_controller.h"

#include "config/firmware_config.h"
#include "display/widgets/boot_splash.h"
#include "settings/settings_menu.h"

namespace {
constexpr uint32_t kBootSplashDurationMs = 3000;
// Adafruit 5297's APX803 can hold panel reset for 280 ms after its supply
// becomes valid. Give it margin before issuing any initialization commands.
constexpr uint32_t kPanelResetHoldMs = 350;
// SPI cannot acknowledge initialization. Replay the manual recovery sequence
// once after startup, even if all initial writes appeared to succeed.
constexpr uint32_t kStartupRetryDelayMs = 2000;
} // namespace

DisplayController::DisplayController() = default;

void DisplayController::begin(void (*serviceInput)()) {
  serviceInput_ = serviceInput;
  isSplashPending_ = true;
  isSplashVisible_ = false;
  if (firmwareConfig::isOledDebugMode) {
    diagnosticRenderer_.reset();
  } else {
    animationManager_.begin();
  }
  lastAnimationChangeAt_ = millis();
  onActivity(millis());
  startupStartedAt_ = millis();
  isStartupRetryPending_ = true;
  startRecovery(millis());
}

void DisplayController::onActivity(uint32_t now) {
  lastActivityAt_ = now;
  isIdle_ = false;
}

void DisplayController::requestRecovery() {
  // Explicit recovery supersedes the one-shot startup safeguard.
  isStartupRetryPending_ = false;
  isRecoveryRequested_ = true;
}

void DisplayController::onKeyPress(uint8_t usage, bool isGameMode) {
  if (!firmwareConfig::isOledDebugMode && !configuration().showSplash()) {
    animationManager_.onKeyPress(usage, isGameMode);
  }
}

bool DisplayController::isInteractiveAnimation() const {
  return !isSplashPending_ && !configuration().showSplash() &&
      !firmwareConfig::isOledDebugMode && animationManager_.isInteractive();
}

void DisplayController::startRecovery(uint32_t now) {
  isReady_ = false;
  isRecovering_ = false;
  isResetHeld_ = false;
  isRecoveryRequested_ = false;
  lastRecoveryAttemptAt_ = now;
  if (!isAllocated_) {
    isAllocated_ = display_.allocate();
  }
  if (!isAllocated_) {
    return;
  }
  display_.holdReset();
  isResetHeld_ = true;
  recoveryStartedAt_ = millis();
  isRecovering_ = true;
}

bool DisplayController::isRecoveryDue(uint32_t now) const {
  const bool isRetryWaiting = !isReady_ && !isRecovering_;
  const bool isRetryDue = now - lastRecoveryAttemptAt_ >= 1000;
  return isRecoveryRequested_ || (isRetryWaiting && isRetryDue);
}

bool DisplayController::isPowerSettled(uint32_t now) const {
  return isRecovering_ && !isResetHeld_ && now - recoveryStartedAt_ >= 100;
}

bool DisplayController::isFrameDue(uint32_t now) const {
  return isReady_ &&
      now - lastFrameAt_ >= (firmwareConfig::isOledDebugMode
                                    ? firmwareConfig::oledDebugFrameIntervalMs
                                    : configuration().frameIntervalMs());
}

bool DisplayController::isConfigurationRefreshDue(uint32_t now) const {
  return configuredContrast_ != configuration().contrast() ||
      now - lastRefreshAt_ >= configuration().refreshIntervalMs();
}

void DisplayController::serviceRecovery(uint32_t now) {
  if (isStartupRetryPending_ &&
      now - startupStartedAt_ >= kStartupRetryDelayMs) {
    isStartupRetryPending_ = false;
    // Do not interrupt an ongoing reset or duplicate a reported-failure retry.
    if (isReady_ && !isRecovering_) {
      requestRecovery();
    }
  }
  if (isRecoveryDue(now)) {
    startRecovery(now);
    return; // The reset captured a newer millis(); avoid subtracting stale now.
  }
  if (isResetHeld_ && now - recoveryStartedAt_ >= kPanelResetHoldMs) {
    // Let the supply settle with reset asserted, then configure and allow a
    // separate 100 ms power-settling interval. Neither phase blocks typing.
    display_.releaseReset();
    isResetHeld_ = false;
    if (!display_.configure(true)) {
      isRecovering_ = false;
      return;
    }
    configuredContrast_ = configuration().contrast();
    recoveryStartedAt_ = millis();
    return;
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
  keyCaptureOverlay_.render(
      display_, status.isKeyCaptureActive, status.capturedKeys);
  milestoneEffect_.render(display_, now);
}

void DisplayController::renderScene(uint32_t now, const DisplayStatus& status) {
  if (isSplashPending_) {
    drawBootSplash(display_);
    return;
  }
  if (status.settingsMenu != nullptr && status.settingsMenu->isOpen()) {
    status.settingsMenu->render(display_);
    return;
  }
  if (!firmwareConfig::isOledDebugMode && configuration().showSplash()) {
    drawBootSplash(display_);
    return;
  }
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
  configuredContrast_ = configuration().contrast();
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
  isPoweredOff_ = false;
  isRecovering_ = false;
}

void DisplayController::render(uint32_t now, const DisplayStatus& status) {
  const uint32_t idleTimeoutMs = configuration().idleSeconds() * 1000U;
  // Latch sleep until activity so millis() wrapping cannot wake the panel.
  isIdle_ =
      idleTimeoutMs != 0 && (isIdle_ || now - lastActivityAt_ >= idleTimeoutMs);
  if (isIdle_) {
    isStartupRetryPending_ = false; // Never defer a boot reset until wake-up.
    if (isAllocated_ && !isPoweredOff_) {
      isPoweredOff_ = display_.powerOff();
    }
    return; // No animation, refresh, or recovery traffic while idle.
  }
  serviceRecovery(now);
  const bool isSplashFinished = isSplashPending_ && isSplashVisible_ &&
      now - splashShownAt_ >= kBootSplashDurationMs;
  if (isSplashFinished) {
    isSplashPending_ = false;
    lastAnimationChangeAt_ = now;
  }
  if (!isFrameDue(now) && !((isSplashFinished || isPoweredOff_) && isReady_)) {
    return;
  }
  lastFrameAt_ = now;
  const bool menuOpen =
      status.settingsMenu != nullptr && status.settingsMenu->isOpen();
  updateAnimationSelection(now,
      status.isGameModeActive || menuOpen || isSplashPending_ ||
          configuration().showSplash());
  if (menuOpen || isSplashPending_ || configuration().showSplash()) {
    lastObservedKeystrokeCount_ = status.keystrokeCount;
  }
  renderScene(now, status);
  const bool isRefreshDue = isConfigurationRefreshDue(now);
  if (isRefreshDue) {
    refreshConfiguration(now);
  }
  sendFrame(isRecovering_ || isRefreshDue || isPoweredOff_);
  if (isSplashPending_ && !isSplashVisible_ && !isRecoveryRequested_ &&
      !isRecovering_) {
    // Count three visible seconds after transfer and display-on, not during
    // setup.
    splashShownAt_ = millis();
    isSplashVisible_ = true;
  }
}

void DisplayController::stepAnimation(uint32_t now, int8_t direction) {
  if (!isReady_ || firmwareConfig::isOledDebugMode ||
      configuration().showSplash() || direction == 0) {
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
  if (firmwareConfig::isOledDebugMode || configuration().showSplash()) {
    return;
  }
  milestoneEffect_.start(now, keystrokeCount);
}

void DisplayController::rotateAnimationWhenDue(uint32_t now) {
  if (now - lastAnimationChangeAt_ < configuration().animationDurationMs()) {
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
