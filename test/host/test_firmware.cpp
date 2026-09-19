#include "Adafruit_SH110X.h"
#include "Arduino.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#define private public
#include "../../src/display/animations/keystroke_comets.cpp"
#include "../../src/display/animations/mountain_bike.cpp"
#include "../../src/keyboard/input_controller.cpp"
#include "display/animations/animation_clock.h"
#include "display/display_controller.h"
#include "keyboard/input_controller.h"
#undef private
#include "display/effects/oled_shading.h"

static bool isConsumerBusy = false;
static std::vector<uint8_t> consumerReports;
void installUsbHidSupport() {}
bool isUsbHostNumLockActive() {
  return false;
}
bool sendUsbConsumerReport(uint8_t b) {
  if (isConsumerBusy) {
    return false;
  }
  consumerReports.push_back(b);
  return true;
}
void KeystrokeCounter::begin() {}
void KeystrokeCounter::recordKeystroke() {
  ++count_;
}
uint32_t KeystrokeCounter::count() const {
  return count_;
}
bool KeystrokeCounter::takePendingMilestone(uint32_t&) {
  return false;
}
static void press(InputController& k, int r, int c, uint32_t now) {
  k.isPressed_[r][c] = true;
  k.handleKeyPress(r, c, now);
  k.sendReport();
}
static void release(InputController& k, int r, int c) {
  k.isPressed_[r][c] = false;
  k.handleKeyRelease(r, c);
  k.sendReport();
}
static bool isReportContaining(uint8_t key) {
  for (int i = 2; i < 8; ++i) {
    if (keyboardReport[i] == key) {
      return true;
    }
  }
  return false;
}
static unsigned inputCalls;
static void serviceInput() {
  ++inputCalls;
}

static void testPhysicalLayout() {
  // Physical bottom row/navigation and existing Colemak-host semantics.
  assert(kKeymap[5][2] == hid::APPLICATION && kKeymap[5][4] == hid::LEFT_GUI);
  assert(kKeymap[11][10] == kFn && kKeymap[11][8] == hid::RIGHT_ALT);
  assert(kKeymap[6][6] == hid::INSERT && kKeymap[6][5] == hid::HOME &&
      kKeymap[6][4] == hid::PAGE_UP);
  assert(kKeymap[7][6] == hid::DELETE && kKeymap[7][5] == hid::END &&
      kKeymap[7][4] == hid::PAGE_DOWN);
  assert(kKeymap[8][4] == kLayerKey);
  InputController k;
  assert(strcmp(k.layoutBadgeLabel(), "CMK") == 0);
  press(k, 8, 4, 0);
  release(k, 8, 4);
  assert(strcmp(k.layoutBadgeLabel(), "QTY") == 0);
  assert(k.activeUsageAt(2, 4) == hid::K); // QWERTY E on a Colemak host.
  press(k, 11, 10, 10);
  k.queueEncoderStep(1);
  int8_t direction;
  assert(k.takeAnimationStep(direction) && direction == 1);
  assert(k.pendingEncoderSteps_ == 0);
  release(k, 11, 10);
  press(k, 6, 6, 20);
  assert(k.isInsertModeActive());
  release(k, 6, 6);
  press(k, 7, 6, 30);
  assert(k.isInsertModeActive());
  release(k, 7, 6);
}

static void testGameMode() {
  InputController k;
  press(k, 8, 4, 0);
  release(k, 8, 4);
  // Held toggle enters GAME, captures only controls, and does not toggle on up.
  uint8_t queuedUsage;
  bool isQueuedGameMode;
  while (k.takeAnimationKey(queuedUsage, isQueuedGameMode)) {
  }
  k.setInteractiveAnimation(true);
  press(k, 8, 4, 100);
  k.rawChangedAt_[8][4] = 451;
  k.service(451);
  assert(k.isGameModeActive());
  press(k, 3, 9, 460);
  assert(!isReportContaining(hid::Y)); // J maps to Y in QTY mode.
  release(k, 8, 4);
  assert(!k.isGameModeActive());
  // A mode release before display dispatch must preserve the captured action.
  assert(k.takeAnimationKey(queuedUsage, isQueuedGameMode));
  assert(queuedUsage == hid::J && isQueuedGameMode);
  assert(strcmp(k.layoutBadgeLabel(), "QTY") == 0);
  k.sendReport();
  assert(!isReportContaining(hid::Y)); // Held control stays captured.
  release(k, 3, 9);
  press(k, 3, 9, 470);
  assert(isReportContaining(hid::Y));
  release(k, 3, 9);
  press(k, 11, 10, 480);
  press(k, 8, 4, 490);
  assert(k.takeDisplayRecoveryRequest());
  release(k, 11, 10);
  release(k, 8, 4);
  assert(strcmp(k.layoutBadgeLabel(), "QTY") == 0);
}

static void testConsumerReports() {
  // Consumer press/release timing, endpoint backpressure and mute coexistence.
  InputController media;
  media.queueEncoderStep(1);
  isConsumerBusy = true;
  media.serviceMediaKey(100);
  assert(media.pendingEncoderSteps_ == 1 && consumerReports.empty());
  isConsumerBusy = false;
  media.serviceMediaKey(101);
  assert(consumerReports.back() == 2);
  media.isPressed_[8][6] = true;
  media.serviceMediaKey(102);
  assert(consumerReports.back() == 3);
  media.serviceMediaKey(116);
  assert(media.mediaKey_ == 2);
  isConsumerBusy = true;
  media.serviceMediaKey(117);
  assert(media.mediaKey_ == 2);
  isConsumerBusy = false;
  media.serviceMediaKey(118);
  assert(consumerReports.back() == 1);
  media.queueEncoderStep(-1);
  media.serviceMediaKey(130);
  assert(media.mediaKey_ == 0);
  media.serviceMediaKey(134);
  assert(consumerReports.back() == 5);
  media.sendReport();
  assert(
      !isReportContaining(hid::MUTE) && !isReportContaining(hid::VOLUME_DOWN));
}

static void testAnimationClock() {
  AnimationClock clock;
  clock.advance(0xfffffff0U);
  assert(fabs(clock.advance(0x10U) - 0.16f) < 0.001f); // timer wrap
  clock.reset();
  clock.advance(0);
  float travel = 0;
  for (unsigned t = 8; t <= 1000; t += 8) {
    travel += clock.advance(t);
  }
  assert(fabs(travel - 5) < 0.001f);
}

static void testBikeTricks() {
  // Real bike logic: key-controlled jump -> each airborne trick -> landing.
  Adafruit_SH1107 screen;
  for (uint8_t trickKey : {kB, kC, kT}) {
    MountainBikeAnimation bike;
    bike.reset();
    bike.render(screen, 0);
    bike.onKeyPress(kUp, true);
    assert(bike.lane_ == 0);
    bike.onKeyPress(kJ, true);
    assert(bike.jumps_[0].isActive);
    unsigned now = 0;
    while (!bike.isAirborne_ && now < 4000) {
      now += 8;
      bike.render(screen, now);
    }
    assert(bike.isAirborne_);
    bike.onKeyPress(trickKey, true);
    assert(bike.trick_ != Trick::None);
    now += 8;
    bike.render(screen, now);
    for (unsigned end = now + 1300; now < end; now += 8) {
      bike.render(screen, now);
    }
    assert(!bike.isAirborne_ && bike.landedTricks_ == 1);
  }
  MountainBikeAnimation ambient;
  ambient.reset();
  ambient.render(screen, 0);
  ambient.onKeyPress(kUp, false); // No fixed UP binding outside GAME.
  assert(ambient.lane_ == 1);
}

static void testAnimationStress() {
  Adafruit_SH1107 screen;
  // Exercise all registered animations beyond their 8-bit phase wrap, with
  // typing effects and sanitized framebuffer accesses at the faster cadence.
  AnimationManager animations;
  animations.begin();
  for (uint8_t index = 0; index < animations.count(); ++index) {
    assert(animations.select(index));
    for (unsigned now = 0; now < 53000; now += 8) {
      if (now % 200 == 0) {
        animations.onKeystroke(now / 200 + 1);
      }
      animations.render(screen, now);
    }
  }
}

static void exportBikePreview() {
  Adafruit_SH1107 screen;
  if (const char* path = getenv("ERGOBOARD_BIKE_PREVIEW")) {
    MountainBikeAnimation bike;
    bike.reset();
    bike.render(screen, 0);
    bike.onKeyPress(kJ, true);
    unsigned now = 0;
    while (!bike.isAirborne_) {
      now += 8;
      bike.render(screen, now);
    }
    bike.onKeyPress(kC, true);
    for (unsigned end = now + 250; now < end; now += 8) {
      bike.render(screen, now);
    }
    applyOledShading(screen);
    FILE* out = fopen(path, "wb");
    assert(out);
    fprintf(out, "P5\n128 128\n255\n");
    for (unsigned y = 0; y < 128; ++y) {
      for (unsigned x = 0; x < 128; ++x) {
        fputc((screen.getBuffer()[x + (y / 8) * 128] >> (y % 8) & 1) ? 255 : 0,
            out);
      }
    }
    fclose(out);
  }
}

static void testDisplayRecovery() {
  // Initialization is nonblocking, complete frame precedes ON, periodic repair
  // doesn't blank/reset, and explicit recovery retains the selected animation.
  DisplayController display;
  fakeNow = 0;
  display.begin(serviceInput);
  DisplayStatus status = {"CMK", "", 0, false, false, false, false, false};
  assert(fakeNow == 0 && display.isRecovering_);
  fakeNow = 99;
  display.render(fakeNow, status);
  assert(pageWrites == 0);
  fakeNow = 100;
  display.render(fakeNow, status);
  assert(pageWrites == 16 && inputCalls == 16 && !display.isRecovering_);
  assert(oledCommands.back() == std::vector<uint8_t>{0xAF});
  oledCommands.clear();
  fakeNow = 2100;
  display.render(fakeNow, status);
  for (const auto& command : oledCommands) {
    for (auto byte : command) {
      assert(byte != 0xAE && byte != 0xA5);
    }
  }
  display.animationManager_.select(9);
  display.requestRecovery();
  fakeNow = 2110;
  display.render(fakeNow, status);
  assert(
      display.isRecovering_ && display.animationManager_.currentIndex() == 9);
  fakeNow = 2210;
  display.render(fakeNow, status);
  assert(!display.isRecovering_);
  failOled = true;
  fakeNow = 2220;
  display.render(fakeNow, status);
  assert(display.isRecoveryRequested_);
  failOled = false;
  fakeNow = 2230;
  display.render(fakeNow, status);
  assert(display.isRecovering_);
}

static void testShading() {
  Adafruit_SH1107 screen;
  // Shading preserves isolated highlights and texture stays within framebuffer.
  screen.clearDisplay();
  screen.drawPixel(64, 64, 1);
  applyOledShading(screen);
  assert(screen.getBuffer()[64 + 8 * 128] & 1);
  for (unsigned i = 0; i < 2048; ++i) {
    screen.getBuffer()[i] = static_cast<uint8_t>(i * 31);
  }
  applyOledShading(screen);
}

static void testCometsFinishDuringTypingBursts() {
  KeystrokeCometsAnimation animation;
  animation.reset();
  for (unsigned i = 1; i <= kCometCount; ++i) {
    animation.onKeystroke(i);
  }
  Comet original[kCometCount];
  memcpy(original, animation.comets_, sizeof(original));
  for (unsigned i = 19; i < 1000; ++i) {
    animation.onKeystroke(i);
  }
  assert(memcmp(original, animation.comets_, sizeof(original)) == 0);
  Adafruit_SH1107 screen;
  for (unsigned now = 0; now <= 5000; now += 200) {
    animation.render(screen, now);
  }
  for (const Comet& comet : animation.comets_) {
    assert(!animation.isActive(comet));
  }
  animation.onKeystroke(1001);
  assert(animation.isActive(animation.comets_[0]));
}

static void testCometLaunchLocations() {
  KeystrokeCometsAnimation animation;
  animation.reset();
  bool isEdgeUsed[4] = {};
  bool isInteriorEdgeUsed = false;
  for (unsigned sequence = 1; sequence <= 256; ++sequence) {
    animation.comets_[0].life = 0;
    animation.onKeystroke(sequence);
    const Comet& comet = animation.comets_[0];
    isEdgeUsed[0] |= comet.x == 4 && comet.velocityX > 0;
    isEdgeUsed[1] |= comet.x == 123 && comet.velocityX < 0;
    isEdgeUsed[2] |= comet.y == 4 && comet.velocityY > 0;
    isEdgeUsed[3] |= comet.y == 100 && comet.velocityY < 0;
    isInteriorEdgeUsed |=
        (comet.x > 20 && comet.x < 107) || (comet.y > 20 && comet.y < 84);
  }
  for (bool isUsed : isEdgeUsed) {
    assert(isUsed);
  }
  assert(isInteriorEdgeUsed);
}

int main() {
  testCometsFinishDuringTypingBursts();
  testCometLaunchLocations();
  testPhysicalLayout();
  testGameMode();
  testConsumerReports();
  testAnimationClock();
  testBikeTricks();
  testAnimationStress();
  exportBikePreview();
  testDisplayRecovery();
  testShading();
  puts("Firmware behavior tests passed");
}
