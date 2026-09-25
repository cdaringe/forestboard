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
static bool failSettingsSave = false;
bool KeystrokeCounter::resetStats() {
  if (failSettingsSave) {
    return false;
  }
  count_ = 0;
  pendingMilestone_ = 0;
  return true;
}
bool KeystrokeCounter::saveConfiguration(const Configuration& value) {
  if (failSettingsSave) {
    return false;
  }
  configuration_ = value;
  return true;
}
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
  for (bool qwerty : {false, true}) {
    applyConfiguration(Configuration{});
    InputController input;
    input.setInteractiveAnimation(true);
    if (qwerty) {
      input.toggleKeyboardLayout();
    }
    Configuration config;
    config.set(Setting::gameMode, 1);
    applyConfiguration(config);
    const uint8_t actions[] = {bikeControls::backflip, bikeControls::cancan,
        bikeControls::spin, bikeControls::wheelie, bikeControls::jump};
    for (uint8_t i = 0; i < 5; ++i) {
      // Physical ASDFG positions are Colemak ARSTD, independent of layout.
      press(input, 3, 2 + i, 10 + i);
      assert(!isReportContaining(input.activeUsageAt(3, 2 + i)));
      uint8_t action;
      bool game;
      assert(
          input.takeAnimationKey(action, game) && game && action == actions[i]);
      release(input, 3, 2 + i);
    }
    press(input, 3, 9, 30); // Old J shortcut is no longer captured.
    assert(isReportContaining(input.activeUsageAt(3, 9)));
    uint8_t action;
    bool game;
    assert(!input.takeAnimationKey(
        action, game)); // Unbound keys cannot trigger a trick by coincidence.
    release(input, 3, 9);
    press(input, 3, 2, 40);
    config.set(Setting::gameMode, 0);
    applyConfiguration(config);
    input.sendReport();
    assert(!isReportContaining(
        input.activeUsageAt(3, 2))); // Capture lasts through release.
    release(input, 3, 2);
    press(input, 3, 2, 50);
    assert(isReportContaining(input.activeUsageAt(3, 2)));
    release(input, 3, 2);
  }
  applyConfiguration(Configuration{});
}

static void chooseSetting(SettingsMenu& menu, Setting setting) {
  assert(menu.page_ == SettingsMenu::Page::Root);
  const auto group = settingDefinitions[static_cast<size_t>(setting)].group;
  const size_t groupIndex = group == SettingGroup::Display ? 0
      : group == SettingGroup::Keyboard                    ? 1
                                                           : 3;
  while (menu.selected_ != groupIndex) {
    menu.key(hid::DOWN);
  }
  menu.key(hid::RIGHT);
  if (groupIndex == 3) {
    const size_t animationIndex = group == SettingGroup::MountainBike ? 0
        : group == SettingGroup::WarpTunnel                           ? 1
                                                                      : 2;
    while (menu.selected_ != animationIndex) {
      menu.key(hid::DOWN);
    }
    menu.key(hid::RIGHT);
  }
  while (menu.groupSetting(menu.selected_) != setting) {
    menu.key(hid::DOWN);
  }
  menu.key(hid::ENTER);
}
static void requestMenuSave(SettingsMenu& menu) {
  while (menu.page_ != SettingsMenu::Page::Root) {
    menu.key(hid::LEFT);
  }
  while (menu.selected_ != 4) {
    menu.key(hid::DOWN);
  }
  menu.select();
}
static void typeNumber(SettingsMenu& menu, const char* digits) {
  for (; *digits; ++digits) {
    menu.key(*digits == '0' ? 0x27 : 0x1e + *digits - '1');
  }
}

static void testSettingsMenu() {
  applyConfiguration(Configuration{});
  InputController k;
  press(k, 8, 4, 0);
  release(k, 8, 4);
  assert(strcmp(k.layoutBadgeLabel(), "QTY") == 0);
  press(k, 8, 4, 100);
  k.updateSettingsMenu(449);
  assert(!k.settingsMenu().isOpen());
  k.updateSettingsMenu(450);
  assert(k.settingsMenu().isOpen() && !k.isGameModeActive());
  k.updateSettingsMenu(900);
  release(k, 8, 4);
  assert(k.settingsMenu().isOpen() && strcmp(k.layoutBadgeLabel(), "QTY") == 0);
  const uint32_t count = k.keystrokeCount();
  press(k, 1, 1, 910);
  assert(!isReportContaining(k.activeUsageAt(1, 1)));
  assert(
      k.keystrokeCount() == count); // Menu navigation is not typing statistics.
  uint8_t usage;
  bool game;
  assert(!k.takeAnimationKey(usage, game));
  release(k, 1, 1);
  auto& menu = k.settingsMenu_;
  chooseSetting(menu, Setting::fps);
  typeNumber(menu, "0");
  menu.select();
  assert(menu.page_ == SettingsMenu::Page::Edit &&
      strcmp(menu.message_, "Outside range") == 0);
  menu.key(0x2a);
  typeNumber(menu, "121");
  menu.select();
  assert(menu.page_ == SettingsMenu::Page::Edit && menu.draft().fps() == 60);
  menu.key(0x2a);
  menu.key(0x2a);
  menu.key(0x2a);
  typeNumber(menu, "90");
  menu.select();
  assert(menu.draft().fps() == 90 && configuration().fps() == 60);
  k.queueEncoderStep(1); // Next display setting: animation delay.
  assert(menu.groupSetting(menu.selected_) == Setting::animationSeconds &&
      k.pendingEncoderSteps_ == 0);
  press(k, kMuteKeyPosition.row, kMuteKeyPosition.col, 1000);
  assert(menu.page_ == SettingsMenu::Page::Edit && k.consumerButtons(0) == 0);
  release(k, kMuteKeyPosition.row, kMuteKeyPosition.col);
  k.queueEncoderStep(1);
  assert(menu.draft().animationSeconds() == 310);
  menu.select();
  requestMenuSave(menu);
  failSettingsSave = true;
  k.serviceSettings();
  assert(menu.isOpen() && configuration().fps() == 60);
  failSettingsSave = false;
  menu.select();
  k.serviceSettings();
  assert(!menu.isOpen() && configuration().fps() == 90 &&
      configuration().animationSeconds() == 310);
  menu.open();
  chooseSetting(menu, Setting::fps);
  menu.rotate(1);
  menu.key(hid::ESC); // Back cancels this value's knob edits.
  assert(menu.draft().fps() == 90);
  menu.key(hid::ENTER);
  typeNumber(menu, "30");
  menu.select();
  menu.key(hid::LEFT);
  menu.key(hid::ESC); // Cancel draft at root.
  assert(!menu.isOpen() && configuration().fps() == 90);
  menu.open();
  press(k, 0, 1, 1100); // Escape at root closes menu.
  assert(!menu.isOpen() && !isReportContaining(hid::ESC));
  release(k, 0, 1);
  press(k, 0, 1, 1110);
  assert(isReportContaining(hid::ESC));
  release(k, 0, 1);
  press(k, 11, 10, 1200);
  press(k, 8, 4, 1210);
  k.updateSettingsMenu(1600);
  assert(k.takeDisplayRecoveryRequest() && !menu.isOpen());
  release(k, 8, 4);
  release(k, 11, 10);
  applyConfiguration(Configuration{});
}

static void testMenuOwnsGameMode() {
  applyConfiguration(Configuration{});
  InputController input;
  press(input, 8, 4, 0);
  input.updateSettingsMenu(350);
  assert(input.settingsMenu().isOpen() && !input.isGameModeActive());
  release(input, 8, 4);
  auto& menu = input.settingsMenu_;
  chooseSetting(menu, Setting::gameMode);
  typeNumber(menu, "1");
  menu.select();
  assert(!input.isGameModeActive()); // Draft not applied until saved.
  requestMenuSave(menu);
  input.serviceSettings();
  assert(input.isGameModeActive() && !menu.isOpen());
  press(input, 8, 4, 1000);
  input.updateSettingsMenu(1350);
  release(input, 8, 4);
  assert(input.isGameModeActive()); // Layer hold never toggles Game mode.
  chooseSetting(menu, Setting::gameMode);
  typeNumber(menu, "0");
  menu.select();
  requestMenuSave(menu);
  input.serviceSettings();
  assert(!input.isGameModeActive() && !menu.isOpen());
  applyConfiguration(Configuration{});
}

static void testResetStatsConfirmation() {
  InputController input;
  input.keystrokeCounter_.count_ = 12345;
  auto& menu = input.settingsMenu_;
  menu.open();
  menu.key(hid::DOWN);
  menu.key(hid::DOWN);
  menu.key(hid::RIGHT);
  assert(menu.page_ == SettingsMenu::Page::Statistics);
  menu.select();
  input.serviceSettings();
  assert(menu.page_ == SettingsMenu::Page::ConfirmReset && menu.selected_ == 0);
  assert(input.keystrokeCount() == 12345 && !menu.resetRequested());
  menu.select(); // Default Cancel must never erase statistics.
  assert(menu.page_ == SettingsMenu::Page::Statistics &&
      input.keystrokeCount() == 12345);
  menu.select();
  menu.key(hid::DOWN);
  menu.select();
  failSettingsSave = true;
  input.serviceSettings();
  assert(input.keystrokeCount() == 12345 && menu.isOpen());
  failSettingsSave = false;
  menu.select();
  menu.key(hid::DOWN);
  menu.select();
  input.serviceSettings();
  assert(input.keystrokeCount() == 0 && menu.count_ == 0);
  assert(menu.page_ == SettingsMenu::Page::Statistics);
}

static void testConfigurationLimits() {
  Configuration config;
  for (size_t i = 0; i < kSettingCount; ++i) {
    auto key = static_cast<Setting>(i);
    const auto& def = settingDefinitions[i];
    assert(config.get(key) == def.initial);
    assert(!config.set(key, def.maximum + 1));
    if (def.minimum) {
      assert(!config.set(key, def.minimum - 1));
    }
    assert(config.set(key, def.minimum));
    assert(config.set(key, def.maximum));
  }
  assert(!config.set(Setting::Count, 1));
  assert(config.frameIntervalMs() == 9);
  config.set(Setting::fps, 60);
  assert(config.frameIntervalMs() == 17);
  SettingsMenu menu;
  menu.open();
  chooseSetting(menu, Setting::fps);
  for (int i = 0; i < 200; ++i) {
    menu.rotate(1);
  }
  assert(menu.draft().fps() == 120);
  for (int i = 0; i < 200; ++i) {
    menu.rotate(-1);
  }
  assert(menu.draft().fps() == 20);
  typeNumber(menu, "999999999999999999999999");
  menu.select();
  assert(menu.page_ == SettingsMenu::Page::Edit && menu.draft().fps() == 20);
  menu.key(hid::ESC);
  assert(menu.draft().fps() == configuration().fps());
}

static void testAnimationSettingsAndPhysicalBindingEditor() {
  applyConfiguration(Configuration{});
  InputController input;
  auto& menu = input.settingsMenu_;
  menu.open();
  chooseSetting(menu, Setting::mtbSpeed);
  typeNumber(menu, "95");
  menu.select();
  menu.key(hid::LEFT);
  menu.key(hid::LEFT);
  assert(menu.page_ == SettingsMenu::Page::Root);
  chooseSetting(menu, Setting::warpSpeed);
  typeNumber(menu, "200");
  menu.select();
  menu.key(hid::LEFT);
  menu.key(hid::LEFT);
  chooseSetting(menu, Setting::curvedRings);
  typeNumber(menu, "12");
  menu.select();
  menu.key(hid::LEFT);
  menu.key(hid::LEFT);
  chooseSetting(menu, Setting::mtbWheelieKey);
  assert(menu.isBindingEditor());
  press(input, 2, 7, 100); // Physical QWERTY Y is logical J in Colemak.
  assert(menu.draft().mtbWheelieKey() == hid::Y);
  assert(!isReportContaining(hid::Y));
  menu.select();
  requestMenuSave(menu);
  input.serviceSettings();
  assert(
      configuration().mtbSpeed() == 95 && configuration().warpSpeed() == 200);
  assert(configuration().curvedRings() == 12 &&
      configuration().mtbWheelieKey() == hid::Y);
  assert(!isReportContaining(
      hid::Y)); // The binding key stays suppressed through exit.
  release(input, 2, 7);
  Configuration config = configuration();
  config.set(Setting::gameMode, 1);
  applyConfiguration(config);
  input.setInteractiveAnimation(true);
  for (unsigned layout = 0; layout < 2; ++layout) {
    press(input, 2, 7, 200 + layout);
    uint8_t action;
    bool game;
    assert(input.takeAnimationKey(action, game) &&
        action == bikeControls::wheelie && game);
    assert(!isReportContaining(input.activeUsageAt(2, 7)));
    release(input, 2, 7);
    press(input, 3, 5, 300 + layout); // Former wheelie physical key now types.
    assert(isReportContaining(input.activeUsageAt(3, 5)));
    assert(!input.takeAnimationKey(action, game));
    release(input, 3, 5);
    input.toggleKeyboardLayout();
  }
  menu.open();
  chooseSetting(menu, Setting::mtbWheelieKey);
  menu.key(hid::A, hid::A);
  menu.select(); // Duplicate backflip binding.
  requestMenuSave(menu);
  assert(!menu.saveRequested() && menu.isOpen());
  assert(strcmp(menu.message_, "Keys must be unique") == 0);
  menu.key(hid::ESC);
  assert(configuration().mtbWheelieKey() == hid::Y);
  menu.open();
  chooseSetting(menu, Setting::mtbWheelieKey);
  menu.key(hid::LEFT, hid::LEFT); // Arrow keys can themselves be bound.
  assert(menu.draft().mtbWheelieKey() == hid::LEFT && menu.isBindingEditor());
  menu.key(hid::ESC); // Only Escape cancels a key-binding edit.
  assert(menu.draft().mtbWheelieKey() == hid::Y);
  applyConfiguration(Configuration{});
}

static void testBikeRandomJumpsAndSpeed() {
  Adafruit_SH1107 screen;
  Configuration config;
  MountainBikeAnimation bike;
  applyConfiguration(config);
  bike.reset();
  bike.render(screen, 0);
  for (uint32_t now = 20; now <= 10000; now += 20) {
    bike.render(screen, now);
  }
  for (const auto& ramp : bike.jumps_) {
    assert(!ramp.isActive); // No automatic game ramps in ambient mode.
  }
  config.set(Setting::gameMode, 1);
  applyConfiguration(config);
  unsigned spawns = 0, launches = 0;
  bool laneSeen[3] = {};
  float firstDelay = 0;
  bool variedDelay = false;
  for (uint32_t now = 10020; now <= 30000; now += 20) {
    const bool airborne = bike.isAirborne_;
    bike.render(screen, now);
    if (!airborne && bike.isAirborne_) {
      ++launches;
    }
    for (const auto& ramp : bike.jumps_) {
      if (ramp.isActive && ramp.x == 124) {
        ++spawns;
        laneSeen[ramp.lane] = true;
        if (firstDelay == 0) {
          firstDelay = bike.randomJumpRemainingMs_;
        } else {
          variedDelay |= bike.randomJumpRemainingMs_ != firstDelay;
        }
        assert(bike.randomJumpRemainingMs_ >= 1200 &&
            bike.randomJumpRemainingMs_ <= 2000);
      }
    }
  }
  assert(spawns >= 9 && launches >= 3 && variedDelay);
  assert(laneSeen[1] && (laneSeen[0] || laneSeen[2]));
  auto countSpawns = [&](uint32_t gap) {
    config.set(Setting::mtbJumpIntervalMs, gap);
    applyConfiguration(config);
    bike.reset();
    unsigned total = 0;
    for (uint32_t now = 0; now <= 20000; now += 20) {
      bike.render(screen, now);
      for (const auto& ramp : bike.jumps_) {
        if (ramp.isActive && ramp.x == 124) {
          ++total;
        }
      }
    }
    return total;
  };
  assert(countSpawns(800) > countSpawns(4000));
  config.set(Setting::gameMode, 0);
  config.set(Setting::mtbSpeed, 100);
  applyConfiguration(config);
  bike.reset();
  bike.render(screen, 0);
  bike.onKeyPress(kJ, true);
  bike.render(screen, 50);
  assert(fabsf(bike.scroll_ - 5) < 0.001f &&
      fabsf(bike.jumps_[0].x - 119) < 0.001f);
  applyConfiguration(Configuration{});
}

static void testBikeWheelies() {
  applyConfiguration(Configuration{});
  Adafruit_SH1107 screen;
  MountainBikeAnimation bike;
  bike.reset();
  bike.render(screen, 0);
  bike.onKeyPress(kWheelie, true);
  float previous = 0;
  for (unsigned now = 20; now <= 600; now += 20) {
    bike.render(screen, now);
    const float rise = bike.frontWheelRise();
    assert(rise >= previous && !bike.isAirborne_);
    const RiderPose pose(75, Trick::None, 0, rise);
    const Point rear = pose.project({-8, 0});
    assert(rear.x == 24 && rear.y == 75);
    previous = rise;
  }
  assert(bike.frontWheelRise() > 8.9f && bike.isWheelieActive_);
  bike.onKeyPress(kWheelie, true);
  assert(bike.wheelieStartedAt_ ==
      0); // Repeated presses do not postpone completion.
  for (unsigned now = 620; now <= 1200; now += 20) {
    bike.render(screen, now);
  }
  assert(!bike.isWheelieActive_ && bike.frontWheelRise() == 0 &&
      bike.landedTricks_ == 1);
  bike.onKeyPress(kWheelie, true);
  bike.render(screen, 1800);
  bike.jumps_[0] = {38, 1, true};
  bike.advanceJumps(0.01f);
  assert(bike.isAirborne_ && !bike.isWheelieActive_);
  assert(bike.frontWheelRise() >
      8.9f); // Wheelie pitch is carried into ramp takeoff.
  bike.onKeyPress(kWheelie, true);
  assert(!bike.isWheelieActive_); // Wheelies only begin on the ground.
  bike.reset();
  bike.onKeyPress(kWheelie, true);
  bike.render(screen, 50000);
  assert(bike.isWheelieActive_ && bike.wheelieStartedAt_ == 50000);
}

static void testBikeRampApproach() {
  MountainBikeAnimation bike;
  bike.reset();
  bike.jumps_[0] = {60, 1, true};
  assert(bike.frontWheelRise() == 0);
  float previous = 0;
  for (float x : {52.0f, 49.0f, 46.0f, 43.0f, 40.0f, 38.0f}) {
    bike.jumps_[0].x = x;
    const float rise = bike.frontWheelRise();
    assert(rise >= previous && rise <= 8);
    const RiderPose pose(75, Trick::None, 0, rise);
    const Point rear = pose.project({-8, 0});
    const Point front = pose.project({8, 0});
    assert(rear.x == 24 && rear.y == 75); // Rear tire stays on flat ground.
    const float surfaceY = 79 - (front.x - (x - 12)) * 8 / 12;
    assert(fabsf(front.y + 4 - surfaceY) <= 1); // Front tire climbs slope.
    assert(front.y <= rear.y);
    previous = rise;
  }
  bike.jumps_[0].lane = 0;
  assert(bike.frontWheelRise() == 0); // No pitching for another lane's ramp.
  bike.jumps_[0].lane = 1;
  bike.advanceJumps(0.01f);
  assert(bike.isAirborne_ && bike.frontWheelRise() == 8);
  bike.now_ += 250;
  assert(bike.frontWheelRise() == 0);
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
  if (const char* path = getenv("FORESTBOARD_BIKE_PREVIEW")) {
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

static void testRuntimeDisplaySettings() {
  Configuration config;
  applyConfiguration(config);
  DisplayController display;
  display.isReady_ = true;
  display.configuredContrast_ = config.contrast();
  assert(!display.isFrameDue(16) && display.isFrameDue(17));
  config.set(Setting::fps, 20);
  config.set(Setting::animationSeconds, 10);
  config.set(Setting::refreshSeconds, 120);
  applyConfiguration(config);
  assert(!display.isFrameDue(49) && display.isFrameDue(50));
  assert(!display.isConfigurationRefreshDue(119999));
  assert(display.isConfigurationRefreshDue(120000));
  const auto index = display.animationManager_.currentIndex();
  display.rotateAnimationWhenDue(9999);
  assert(display.animationManager_.currentIndex() == index);
  display.rotateAnimationWhenDue(10000);
  assert(display.animationManager_.currentIndex() != index);
  config.set(Setting::contrast, 120);
  applyConfiguration(config);
  assert(display.isConfigurationRefreshDue(1));
  oledCommands.clear();
  display.refreshConfiguration(1);
  bool foundContrast = false;
  for (const auto& command : oledCommands) {
    for (size_t i = 0; i + 1 < command.size(); ++i) {
      if (command[i] == 0x81 && command[i + 1] == 120) {
        foundContrast = true;
      }
    }
  }
  assert(foundContrast && !display.isConfigurationRefreshDue(2));
  SettingsMenu menu;
  menu.open();
  DisplayStatus status = {
      "CMK", "", 0, false, false, false, false, false, &menu};
  display.renderScene(10, status);
  assert(display.display_.getBuffer()[3] != 0 ||
      display.display_.getBuffer()[1 + 2 * 128] != 0);
  applyConfiguration(Configuration{});
}

static void finishStartupRetry(DisplayController& display,
    const DisplayStatus& status, uint32_t start = 0) {
  for (uint32_t elapsed : {2000U, 2350U, 2450U}) {
    fakeNow = start + elapsed;
    display.render(fakeNow, status);
  }
}

static void testSilentStartupRetry() {
  applyConfiguration(Configuration{});
  DisplayStatus status = {"CMK", "", 0, false, false, false, false, false};
  for (uint32_t start : {0U, UINT32_MAX - 1000U}) {
    DisplayController display;
    fakeNow = start;
    display.begin(serviceInput);
    auto renderAt = [&](uint32_t elapsed) {
      fakeNow = start + elapsed;
      display.render(fakeNow, status);
      assert(fakeNow == start + elapsed); // No blocking millisecond delays.
    };
    renderAt(350);
    renderAt(450);
    assert(display.isReady_ && !display.isRecovering_);
    // All writes report success even if the panel silently ignored them.
    // No failure injection or manual request may be needed for this retry.
    assert(!failOled && !display.isRecoveryRequested_);
    renderAt(1999);
    const unsigned frames = pageWrites, callbacks = inputCalls;
    oledCommands.clear();
    display.animationManager_.select(9);
    renderAt(2000);
    assert(display.isResetHeld_ && !display.isReady_);
    assert(!display.isStartupRetryPending_ && pinValues[PC9] == LOW);
    assert(display.animationManager_.currentIndex() == 9);
    renderAt(2349);
    assert(pageWrites == frames && oledCommands.empty());
    renderAt(2350);
    assert(!display.isResetHeld_ && pinValues[PC9] == HIGH);
    assert(oledCommands.front() == std::vector<uint8_t>({0xAE, 0xAD, 0x8A}));
    renderAt(2449);
    assert(pageWrites == frames && !display.isReady_);
    renderAt(2450);
    assert(display.isReady_ && !display.isRecovering_);
    assert(pageWrites == frames + 64 && inputCalls == callbacks + 64);
    assert(oledCommands.back() == std::vector<uint8_t>{0xAF});
    for (uint32_t elapsed : {4000U, 6000U, 60000U, 120000U}) {
      renderAt(elapsed);
      assert(!display.isRecovering_ && pinValues[PC9] == HIGH);
      assert(display.lastRecoveryAttemptAt_ == start + 2000U);
    }
  }

  // Manual recovery before the scheduled retry consumes it, not a second blink.
  DisplayController manual;
  fakeNow = 0;
  manual.begin(serviceInput);
  fakeNow = 350;
  manual.render(fakeNow, status);
  fakeNow = 450;
  manual.render(fakeNow, status);
  manual.requestRecovery();
  for (uint32_t now : {700U, 1050U, 1150U, 2000U, 4000U}) {
    fakeNow = now;
    manual.render(fakeNow, status);
  }
  assert(!manual.isStartupRetryPending_ && !manual.isRecovering_);
  assert(manual.lastRecoveryAttemptAt_ == 700);

  // A late loop must not restart a reset already in progress.
  DisplayController late;
  fakeNow = 0;
  late.begin(serviceInput);
  fakeNow = 2000;
  late.render(fakeNow, status);
  assert(!late.isStartupRetryPending_ && !late.isResetHeld_);
  assert(late.lastRecoveryAttemptAt_ == 0);
  fakeNow = 2100;
  late.render(fakeNow, status);
  assert(late.isReady_ && !late.isRecovering_);

  // Sleep cancels a pending retry rather than running it later on wake-up.
  DisplayController sleeping;
  fakeNow = 0;
  sleeping.begin(serviceInput);
  fakeNow = 350;
  sleeping.render(fakeNow, status);
  fakeNow = 450;
  sleeping.render(fakeNow, status);
  fakeNow = 300000;
  sleeping.render(fakeNow, status);
  assert(!sleeping.isStartupRetryPending_ && sleeping.isPoweredOff_);
  sleeping.onActivity(++fakeNow);
  sleeping.render(fakeNow, status);
  assert(!sleeping.isPoweredOff_ && !sleeping.isRecovering_);
  assert(sleeping.lastRecoveryAttemptAt_ == 0);
}

static void testDisplayRecovery() {
  // Initialization is nonblocking, complete frame precedes ON, periodic repair
  // doesn't blank/reset, and explicit recovery retains the selected animation.
  DisplayController display;
  fakeNow = 0;
  display.begin(serviceInput);
  DisplayStatus status = {"CMK", "", 0, false, false, false, false, false};
  assert(fakeNow == 0 && display.isRecovering_);
  assert(pinValues[PC9] == LOW);
  oledCommands.clear();
  fakeNow = 280; // The breakout supervisor may still be holding reset.
  display.render(fakeNow, status);
  assert(pageWrites == 0 && oledCommands.empty() && pinValues[PC9] == LOW);
  fakeNow = 349;
  display.render(fakeNow, status);
  assert(oledCommands.empty());
  fakeNow = 350;
  display.render(fakeNow, status);
  assert(pageWrites == 0 && pinValues[PC9] == HIGH);
  fakeNow = 449;
  display.render(fakeNow, status);
  assert(pageWrites == 0);
  fakeNow = 450;
  display.render(fakeNow, status);
  assert(pageWrites == 64 && inputCalls == 64 && !display.isRecovering_);
  assert(oledCommands.back() == std::vector<uint8_t>{0xAF});
  oledCommands.clear();
  finishStartupRetry(display, status);
  assert(display.lastRefreshAt_ == 0);
  fakeNow = 59980;
  display.render(fakeNow, status);
  assert(display.lastRefreshAt_ == 0);
  oledCommands.clear();
  fakeNow = 60000;
  display.render(fakeNow, status);
  assert(display.lastRefreshAt_ == 60000);
  for (const auto& command : oledCommands) {
    for (auto byte : command) {
      assert(byte != 0xAE && byte != 0xA5);
    }
  }
  display.animationManager_.select(9);
  display.requestRecovery();
  fakeNow = 60010;
  display.render(fakeNow, status);
  assert(
      display.isRecovering_ && display.animationManager_.currentIndex() == 9);
  fakeNow = 60360;
  display.render(fakeNow, status);
  assert(display.isRecovering_);
  fakeNow = 60460;
  display.render(fakeNow, status);
  assert(!display.isRecovering_);
  failOled = true;
  fakeNow = 60480;
  display.render(fakeNow, status);
  assert(display.isRecoveryRequested_);
  failOled = false;
  fakeNow = 60490;
  display.render(fakeNow, status);
  assert(display.isRecovering_);
  // Configuration can fail after reset release; retry the complete sequence
  // without ever treating a partially initialized panel as ready.
  failOled = true;
  fakeNow = 60840;
  display.render(fakeNow, status);
  assert(!display.isRecovering_ && !display.isReady_ && !display.isResetHeld_);
  failOled = false;
  fakeNow = 61489;
  display.render(fakeNow, status);
  assert(!display.isRecovering_);
  fakeNow = 61490;
  display.render(fakeNow, status);
  assert(display.isResetHeld_ && pinValues[PC9] == LOW);
  fakeNow = 61840;
  display.render(fakeNow, status);
  assert(!display.isResetHeld_ && !display.isReady_);
  fakeNow = 61940;
  display.render(fakeNow, status);
  assert(display.isReady_ && !display.isRecovering_);
}

static void testScreenTimeout() {
  applyConfiguration(Configuration{});
  DisplayStatus status = {"CMK", "", 0, false, false, false, false, false};
  for (uint32_t start : {0U, UINT32_MAX - 1000U}) {
    DisplayController display;
    fakeNow = start;
    display.begin(serviceInput);
    auto renderAt = [&](uint32_t elapsed) {
      fakeNow = start + elapsed;
      display.render(fakeNow, status);
    };
    renderAt(350);
    renderAt(450);
    finishStartupRetry(display, status, start);
    renderAt(299999);
    assert(!display.isPoweredOff_);
    const auto frames = pageWrites;
    oledCommands.clear();
    renderAt(300000);
    assert(display.isPoweredOff_ && pageWrites == frames);
    assert(oledCommands == std::vector<std::vector<uint8_t>>{{0xAE}});
    display.requestRecovery();
    renderAt(600000);
    renderAt(0); // Even a full clock wrap must leave the panel asleep.
    assert(display.isPoweredOff_ && pageWrites == frames);
    assert(oledCommands.size() == 1);
    display.onActivity(start + 600001);
    renderAt(600001);
    renderAt(600351);
    renderAt(600451);
    assert(!display.isPoweredOff_ && pageWrites == frames + 64);
    assert(oledCommands.back() == std::vector<uint8_t>{0xAF});
    renderAt(900000);
    assert(!display.isPoweredOff_);
    renderAt(900001);
    assert(display.isPoweredOff_);
    display.onActivity(start + 900002);
    renderAt(900002);
    assert(!display.isPoweredOff_);
    assert(oledCommands.back() == std::vector<uint8_t>{0xAF});
  }

  // Editing and saving uses the same menu/persistence path as other settings.
  InputController input;
  auto& menu = input.settingsMenu_;
  menu.open();
  chooseSetting(menu, Setting::idleSeconds);
  typeNumber(menu, "60");
  menu.select();
  requestMenuSave(menu);
  input.serviceSettings();
  assert(configuration().idleSeconds() == 60);
  DisplayController display;
  fakeNow = 0;
  display.begin(serviceInput);
  fakeNow = 350;
  display.render(fakeNow, status);
  fakeNow = 450;
  display.render(fakeNow, status);
  finishStartupRetry(display, status);
  fakeNow = 60000;
  failOled = true;
  display.render(fakeNow, status);
  assert(!display.isPoweredOff_);
  failOled = false;
  display.render(fakeNow, status);
  assert(display.isPoweredOff_); // Failed off commands retry.

  Configuration config = configuration();
  config.set(Setting::idleSeconds, 0);
  applyConfiguration(config);
  display.render(fakeNow, status);
  assert(!display.isPoweredOff_);
  fakeNow = 3600000;
  display.render(fakeNow, status);
  assert(!display.isPoweredOff_); // Disabled timeout never sleeps.
  applyConfiguration(Configuration{});
}

static void testInputActivity() {
  InputController input;
  input.scanMatrix(1);
  assert(!input.takeActivity());
  digitalReadOverride = [](uint32_t pin) {
    return pin == kColPins[0] ? LOW : HIGH;
  };
  input.scanMatrix(10);
  assert(input.takeActivity() && !input.takeActivity());
  input.scanMatrix(20);
  assert(input.takeActivity());
  input.scanMatrix(300020); // Held keys keep the screen awake.
  assert(input.takeActivity());
  digitalReadOverride = nullptr;
  input.scanMatrix(300030);
  input.scanMatrix(300040);
  assert(input.takeActivity()); // Release restarts the full idle interval.
  input.scanMatrix(300050);
  assert(!input.takeActivity());
  input.queueEncoderStep(1);
  assert(input.takeActivity() && input.pendingEncoderSteps_ == 1);
  input.settingsMenu_.open();
  input.queueEncoderStep(-1);
  assert(input.takeActivity()); // Menu-only input also wakes the display.
}

static void testBootSplash() {
  applyConfiguration(Configuration{});
  for (uint32_t start : {0U, UINT32_MAX - 200U}) {
    DisplayController display;
    fakeNow = start;
    display.begin(serviceInput);
    DisplayStatus status = {"CMK", "", 0, false, false, false, false, false};
    assert(fakeNow == start && !display.isSplashVisible_);
    fakeNow = start + 350U;
    display.render(fakeNow, status);
    fakeNow = start + 450U;
    const unsigned previousInputCalls = inputCalls;
    display.render(fakeNow, status);
    assert(display.isSplashVisible_ && display.splashShownAt_ == fakeNow);
    assert(inputCalls ==
        previousInputCalls + 64); // Scanner serviced during splash transfer.
    assert(display.display_.lastText == "forestboard");
    assert(display.display_.cursorX == 40 && display.display_.cursorY == 60);
    const uint8_t* pixels = display.display_.getBuffer();
    int left = 128, right = -1, top = 128, bottom = -1;
    for (int y = 0; y < 128; ++y) {
      for (int x = 0; x < 128; ++x) {
        if (pixels[x + (y / 8) * 128] & (1 << (y % 8))) {
          left = min(left, x);
          right = max(right, x);
          top = min(top, y);
          bottom = max(bottom, y);
        }
      }
    }
    // Text is not rasterized by the mock; combine its real 5x7 font bounds
    // with the tree geometry to check centering within one pixel.
    assert(right == 33 && left == 23);
    assert(left == 127 - (display.display_.cursorX + 65 - 1));
    assert(abs(top - (127 - bottom)) <= 1);
    finishStartupRetry(display, status, start);
    fakeNow = start + 3449U;
    display.render(fakeNow, status);
    assert(
        display.isSplashPending_ && display.display_.lastText == "forestboard");
    fakeNow = start + 3450U;
    display.render(fakeNow, status);
    assert(!display.isSplashPending_ &&
        display.display_.lastText != "forestboard");
    display.requestRecovery();
    fakeNow = start + 3600U;
    display.render(fakeNow, status);
    fakeNow = start + 3950U;
    display.render(fakeNow, status);
    fakeNow = start + 4050U;
    display.render(fakeNow, status);
    assert(!display.isSplashPending_ &&
        display.display_.lastText != "forestboard");
  }
  DisplayController delayed;
  fakeNow = 0;
  delayed.begin(serviceInput);
  DisplayStatus status = {"CMK", "", 0, false, false, false, false, false};
  fakeNow = 350;
  delayed.render(fakeNow, status);
  failOled = true;
  fakeNow = 450;
  delayed.render(fakeNow, status);
  assert(!delayed
          .isSplashVisible_); // A failed transfer does not consume splash time.
  failOled = false;
  fakeNow = 460;
  delayed.render(fakeNow, status);
  fakeNow = 810;
  delayed.render(fakeNow, status);
  fakeNow = 910;
  delayed.render(fakeNow, status);
  assert(delayed.isSplashVisible_ && delayed.splashShownAt_ == 910);
  fakeNow = 3909;
  delayed.render(fakeNow, status);
  assert(delayed.isSplashPending_);
  fakeNow = 3910;
  delayed.render(fakeNow, status);
  assert(!delayed.isSplashPending_);
}

static void testPersistentSplashSetting() {
  applyConfiguration(Configuration{});
  InputController input;
  auto& menu = input.settingsMenu_;
  menu.open();
  chooseSetting(menu, Setting::showSplash);
  typeNumber(menu, "1");
  menu.select();
  // The added Display row leaves Back accessible through the scrolled list.
  menu.key(hid::DOWN);
  assert(menu.groupSetting(menu.selected_) == Setting::idleSeconds);
  menu.key(hid::DOWN);
  assert(menu.groupSetting(menu.selected_) == Setting::Count);
  Adafruit_SH1107 screen;
  menu.render(screen);
  assert(screen.getBuffer()[7 + 9 * 128] &
      (1 << 5)); // Selected Back arrow at y=77.
  menu.select();
  assert(menu.page_ == SettingsMenu::Page::Root);
  requestMenuSave(menu);
  input.serviceSettings();
  assert(configuration().showSplash() && !menu.isOpen());
  Configuration settings = configuration();
  settings.set(Setting::gameMode, 1);
  applyConfiguration(settings);
  DisplayController display;
  fakeNow = 0;
  display.begin(serviceInput);
  display.animationManager_.select(9);
  DisplayStatus status = {
      "CMK", "", 0, false, false, false, false, true, &menu};
  fakeNow = 350;
  display.render(fakeNow, status);
  fakeNow = 450;
  display.render(fakeNow, status);
  finishStartupRetry(display, status);
  fakeNow = 3450;
  display.render(fakeNow, status);
  assert(
      !display.isSplashPending_ && display.display_.lastText == "forestboard");
  assert(
      !display.isInteractiveAnimation()); // Hidden game cannot consume typing.
  const auto selected = display.animationManager_.currentIndex();
  display.stepAnimation(3450, 1);
  assert(display.animationManager_.currentIndex() == selected);
  status.isGameModeActive = false;
  fakeNow = 900000;
  display.onActivity(fakeNow); // Keep this splash/rotation check awake.
  display.render(fakeNow, status);
  assert(display.display_.lastText == "forestboard");
  assert(display.animationManager_.currentIndex() == selected);
  menu.open();
  fakeNow += 100;
  display.render(fakeNow, status);
  assert(display.display_.lastText != "forestboard"); // Menu takes priority.
  chooseSetting(menu, Setting::showSplash);
  typeNumber(menu, "0");
  menu.select();
  requestMenuSave(menu);
  input.serviceSettings();
  fakeNow += 100;
  display.render(fakeNow, status);
  assert(!configuration().showSplash() &&
      display.display_.lastText != "forestboard");
  assert(display.isInteractiveAnimation());
  applyConfiguration(Configuration{});
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
  testSettingsMenu();
  testConfigurationLimits();
  testMenuOwnsGameMode();
  testResetStatsConfirmation();
  testAnimationSettingsAndPhysicalBindingEditor();
  testBikeRandomJumpsAndSpeed();
  testBikeWheelies();
  testBikeRampApproach();
  testConsumerReports();
  testAnimationClock();
  testBikeTricks();
  testAnimationStress();
  exportBikePreview();
  testRuntimeDisplaySettings();
  testDisplayRecovery();
  testSilentStartupRetry();
  testBootSplash();
  testPersistentSplashSetting();
  testScreenTimeout();
  testInputActivity();
  puts("Firmware behavior tests passed");
}
