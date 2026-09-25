#include "keyboard/input_controller.h"
#include "display/animations/bike_controls.h"

#if defined(FORESTBOARD_KEYBOARD_MODE)

#include <cstring>
#include <usbd_hid_composite_if.h>

#include "usb/hid_device.h"

namespace {

constexpr uint8_t kRows = 12;
constexpr uint8_t kCols = 12;
constexpr uint32_t kScanIntervalMs = 1;
constexpr uint32_t kDebounceMs = 5;
constexpr uint32_t kReportRefreshMs = 20;
constexpr uint32_t kLineSettleUs = 3;
constexpr uint32_t kMediaTapMs = 16;
constexpr uint32_t kSettingsHoldMs = 350;
constexpr uint32_t kRapidClickGapMs = 750;
constexpr size_t kCaptureCharacterCount = 21;

struct MatrixPosition {
  uint8_t row;
  uint8_t col;

  constexpr bool isMatch(uint8_t candidateRow, uint8_t candidateCol) const {
    return row == candidateRow && col == candidateCol;
  }
};

constexpr MatrixPosition kNumLockKeyPosition = {7, 3};
constexpr MatrixPosition kInsertKeyPosition = {6, 6}; // SW61
constexpr MatrixPosition kLayerKeyPosition = {8, 4};  // SW79, below Page Down
constexpr MatrixPosition kFnKeyPosition = {11, 10};   // SW105, right of Space
constexpr MatrixPosition kMuteKeyPosition = {8, 6};   // SW80 encoder push

const MatrixPosition& matrixPositionFor(KeyboardKey key) {
  switch (key) {
  case KeyboardKey::Function:
    return kFnKeyPosition;
  case KeyboardKey::Insert:
    return kInsertKeyPosition;
  case KeyboardKey::LayoutSwitch:
    return kLayerKeyPosition;
  case KeyboardKey::NumLock:
    return kNumLockKeyPosition;
  }

  // All enum values are handled above. Retain a safe position for compilers
  // that require a return after an exhaustive switch.
  return kFnKeyPosition;
}

// Rotary encoder SW80 rotation pins. Board C6/C7 are MCU PC6/PC7; the
// encoder's common pin is grounded, so the GPIO pull-ups hold both channels
// high while open.
constexpr uint32_t kEncoderPinA = PC6; // Board label C6 / MCU PC6.
constexpr uint32_t kEncoderPinB = PC7; // Board label C7 / MCU PC7.

// Board labels: C0, C2, C1, A0, C3, A2, A1, A4, A3, A6, A5, C4.
constexpr uint32_t kRowPins[kRows] = {
    PC0,
    PC2,
    PC1,
    PA0,
    PC3,
    PA2,
    PA1,
    PA4,
    PA3,
    PA6,
    PA5,
    PC4,
};

// Board labels: B9, B8, B5, B3, B4, C12, D2, C10, C11, A15, A10, A8.
constexpr uint32_t kColPins[kCols] = {
    PB9,
    PB8,
    PB5,
    PB3,
    PB4,
    PC12,
    PD2,
    PC10,
    PC11,
    PA15,
    PA10,
    PA8,
};

// USB HID Keyboard/Keypad usage IDs. Modifiers E0-E7 are packed into the
// report's modifier byte; all other keys occupy one of the six boot-keyboard
// slots. 0 means that the electrical matrix position is unused.
namespace hid {
constexpr uint8_t NONE = 0x00;
constexpr uint8_t A = 0x04;
constexpr uint8_t B = 0x05;
constexpr uint8_t C = 0x06;
constexpr uint8_t D = 0x07;
constexpr uint8_t E = 0x08;
constexpr uint8_t F = 0x09;
constexpr uint8_t G = 0x0A;
constexpr uint8_t H = 0x0B;
constexpr uint8_t I = 0x0C;
constexpr uint8_t J = 0x0D;
constexpr uint8_t K = 0x0E;
constexpr uint8_t L = 0x0F;
constexpr uint8_t M = 0x10;
constexpr uint8_t N = 0x11;
constexpr uint8_t O = 0x12;
constexpr uint8_t P = 0x13;
constexpr uint8_t Q = 0x14;
constexpr uint8_t R = 0x15;
constexpr uint8_t S = 0x16;
constexpr uint8_t T = 0x17;
constexpr uint8_t U = 0x18;
constexpr uint8_t V = 0x19;
constexpr uint8_t W = 0x1A;
constexpr uint8_t X = 0x1B;
constexpr uint8_t Y = 0x1C;
constexpr uint8_t Z = 0x1D;
constexpr uint8_t N1 = 0x1E;
constexpr uint8_t N2 = 0x1F;
constexpr uint8_t N3 = 0x20;
constexpr uint8_t N4 = 0x21;
constexpr uint8_t N5 = 0x22;
constexpr uint8_t N6 = 0x23;
constexpr uint8_t N7 = 0x24;
constexpr uint8_t N8 = 0x25;
constexpr uint8_t N9 = 0x26;
constexpr uint8_t N0 = 0x27;
constexpr uint8_t ENTER = 0x28;
constexpr uint8_t ESC = 0x29;
constexpr uint8_t BACKSPACE = 0x2A;
constexpr uint8_t TAB = 0x2B;
constexpr uint8_t SPACE = 0x2C;
constexpr uint8_t MINUS = 0x2D;
constexpr uint8_t EQUAL = 0x2E;
constexpr uint8_t LBRACKET = 0x2F;
constexpr uint8_t RBRACKET = 0x30;
constexpr uint8_t BACKSLASH = 0x31;
constexpr uint8_t SEMICOLON = 0x33;
constexpr uint8_t QUOTE = 0x34;
constexpr uint8_t GRAVE = 0x35;
constexpr uint8_t COMMA = 0x36;
constexpr uint8_t DOT = 0x37;
constexpr uint8_t SLASH = 0x38;
constexpr uint8_t CAPS = 0x39;
constexpr uint8_t F1 = 0x3A;
constexpr uint8_t F2 = 0x3B;
constexpr uint8_t F3 = 0x3C;
constexpr uint8_t F4 = 0x3D;
constexpr uint8_t F5 = 0x3E;
constexpr uint8_t F6 = 0x3F;
constexpr uint8_t F7 = 0x40;
constexpr uint8_t F8 = 0x41;
constexpr uint8_t F9 = 0x42;
constexpr uint8_t F10 = 0x43;
constexpr uint8_t F11 = 0x44;
constexpr uint8_t F12 = 0x45;
constexpr uint8_t PRINT_SCREEN = 0x46;
constexpr uint8_t SCROLL_LOCK = 0x47;
constexpr uint8_t PAUSE = 0x48;
constexpr uint8_t INSERT = 0x49;
constexpr uint8_t HOME = 0x4A;
constexpr uint8_t PAGE_UP = 0x4B;
constexpr uint8_t DELETE = 0x4C;
constexpr uint8_t END = 0x4D;
constexpr uint8_t PAGE_DOWN = 0x4E;
constexpr uint8_t RIGHT = 0x4F;
constexpr uint8_t LEFT = 0x50;
constexpr uint8_t DOWN = 0x51;
constexpr uint8_t UP = 0x52;
constexpr uint8_t NUM_LOCK = 0x53;
constexpr uint8_t KP_SLASH = 0x54;
constexpr uint8_t KP_ASTERISK = 0x55;
constexpr uint8_t KP_MINUS = 0x56;
constexpr uint8_t KP_PLUS = 0x57;
constexpr uint8_t KP_ENTER = 0x58;
constexpr uint8_t KP_1 = 0x59;
constexpr uint8_t KP_2 = 0x5A;
constexpr uint8_t KP_3 = 0x5B;
constexpr uint8_t KP_4 = 0x5C;
constexpr uint8_t KP_5 = 0x5D;
constexpr uint8_t KP_6 = 0x5E;
constexpr uint8_t KP_7 = 0x5F;
constexpr uint8_t KP_8 = 0x60;
constexpr uint8_t KP_9 = 0x61;
constexpr uint8_t KP_0 = 0x62;
constexpr uint8_t KP_DOT = 0x63;
constexpr uint8_t APPLICATION = 0x65;
constexpr uint8_t F13 = 0x68;
constexpr uint8_t MUTE = 0x7F;
constexpr uint8_t VOLUME_UP = 0x80;
constexpr uint8_t VOLUME_DOWN = 0x81;
constexpr uint8_t LEFT_CTRL = 0xE0;
constexpr uint8_t LEFT_SHIFT = 0xE1;
constexpr uint8_t LEFT_ALT = 0xE2;
constexpr uint8_t LEFT_GUI = 0xE3;
constexpr uint8_t RIGHT_CTRL = 0xE4;
constexpr uint8_t RIGHT_SHIFT = 0xE5;
constexpr uint8_t RIGHT_ALT = 0xE6;
constexpr uint8_t RIGHT_GUI = 0xE7;
} // namespace hid

// 0xFF is the local momentary Fn key and is never sent to the host. The matrix
// below follows the PCB's switch coordinates.
constexpr uint8_t kFn = 0xFF;
constexpr uint8_t kLayerKey = 0xFE;
constexpr uint8_t kKeymap[kRows][kCols] = {
    {hid::NONE, hid::ESC, hid::F1, hid::F2, hid::F3, hid::F4, hid::F5, hid::F6,
        hid::F7, hid::F8, hid::F9, hid::F10},
    {hid::GRAVE, hid::N1, hid::NONE, hid::N2, hid::N3, hid::N4, hid::N5,
        hid::N6, hid::N7, hid::N8, hid::N9, hid::N0},
    {hid::NONE, hid::TAB, hid::Q, hid::W, hid::E, hid::R, hid::T, hid::Y,
        hid::U, hid::I, hid::O, hid::P},
    {hid::CAPS, hid::NONE, hid::A, hid::S, hid::D, hid::F, hid::G, hid::NONE,
        hid::H, hid::J, hid::K, hid::L},
    {hid::LEFT_SHIFT, hid::NONE, hid::Z, hid::X, hid::C, hid::V, hid::B,
        hid::NONE, hid::N, hid::M, hid::COMMA, hid::DOT},
    {hid::NONE, hid::LEFT_CTRL, hid::APPLICATION, hid::LEFT_ALT, hid::LEFT_GUI,
        hid::NONE, hid::NONE, hid::SPACE, hid::NONE, hid::NONE, hid::NONE,
        hid::NONE},
    {hid::NONE, hid::NONE, hid::NONE, hid::NONE, hid::PAGE_UP, hid::HOME,
        hid::INSERT, hid::SCROLL_LOCK, hid::PRINT_SCREEN, hid::F12, hid::F11,
        hid::NONE},
    {hid::KP_MINUS, hid::KP_ASTERISK, hid::KP_SLASH, hid::NUM_LOCK,
        hid::PAGE_DOWN, hid::END, hid::DELETE, hid::NONE, hid::BACKSPACE,
        hid::EQUAL, hid::MINUS, hid::NONE},
    {hid::NONE, hid::KP_9, hid::KP_8, hid::KP_7, kLayerKey, hid::NONE,
        hid::MUTE, hid::BACKSLASH, hid::NONE, hid::RBRACKET, hid::LBRACKET,
        hid::NONE},
    {hid::KP_PLUS, hid::KP_6, hid::KP_5, hid::KP_4, hid::NONE, hid::NONE,
        hid::NONE, hid::NONE, hid::ENTER, hid::QUOTE, hid::SEMICOLON,
        hid::NONE},
    {hid::NONE, hid::KP_3, hid::KP_2, hid::KP_1, hid::NONE, hid::UP, hid::NONE,
        hid::NONE, hid::RIGHT_SHIFT, hid::NONE, hid::SLASH, hid::NONE},
    {hid::KP_ENTER, hid::KP_DOT, hid::KP_0, hid::NONE, hid::RIGHT, hid::DOWN,
        hid::LEFT, hid::RIGHT_CTRL, hid::RIGHT_ALT, hid::NONE, kFn, hid::NONE},
};

// The host uses US QWERTY. Translate canonical physical positions into
// Colemak usages here; QWERTY mode sends the physical usages unchanged.
uint8_t colemakUsageForPhysicalKey(uint8_t physicalUsage) {
  switch (physicalUsage) {
  case hid::E:
    return hid::F;
  case hid::R:
    return hid::P;
  case hid::T:
    return hid::G;
  case hid::Y:
    return hid::J;
  case hid::U:
    return hid::L;
  case hid::I:
    return hid::U;
  case hid::O:
    return hid::Y;
  case hid::P:
    return hid::SEMICOLON;
  case hid::S:
    return hid::R;
  case hid::D:
    return hid::S;
  case hid::F:
    return hid::T;
  case hid::G:
    return hid::D;
  case hid::J:
    return hid::N;
  case hid::K:
    return hid::E;
  case hid::L:
    return hid::I;
  case hid::SEMICOLON:
    return hid::O;
  case hid::N:
    return hid::K;
  default:
    return physicalUsage;
  }
}

const char* captureTokenForUsage(uint8_t usage) {
  static const char* const kLetters[] = {
      "A",
      "B",
      "C",
      "D",
      "E",
      "F",
      "G",
      "H",
      "I",
      "J",
      "K",
      "L",
      "M",
      "N",
      "O",
      "P",
      "Q",
      "R",
      "S",
      "T",
      "U",
      "V",
      "W",
      "X",
      "Y",
      "Z",
  };
  static const char* const kNumberRow[] = {
      "1",
      "2",
      "3",
      "4",
      "5",
      "6",
      "7",
      "8",
      "9",
      "0",
  };
  static const char* const kFunctionKeys[] = {
      "F1",
      "F2",
      "F3",
      "F4",
      "F5",
      "F6",
      "F7",
      "F8",
      "F9",
      "F10",
      "F11",
      "F12",
  };

  if (usage >= hid::A && usage <= hid::Z) {
    return kLetters[usage - hid::A];
  }
  if (usage >= hid::N1 && usage <= hid::N0) {
    return kNumberRow[usage - hid::N1];
  }
  if (usage >= hid::F1 && usage <= hid::F12) {
    return kFunctionKeys[usage - hid::F1];
  }
  if (usage >= hid::KP_1 && usage <= hid::KP_9) {
    static const char* const kKeypadDigits[] = {
        "K1",
        "K2",
        "K3",
        "K4",
        "K5",
        "K6",
        "K7",
        "K8",
        "K9",
    };
    return kKeypadDigits[usage - hid::KP_1];
  }

  switch (usage) {
  case hid::NONE:
    return "?";
  case hid::ENTER:
    return "ENT";
  case hid::ESC:
    return "ESC";
  case hid::BACKSPACE:
    return "BSP";
  case hid::TAB:
    return "TAB";
  case hid::SPACE:
    return "SPC";
  case hid::MINUS:
    return "-";
  case hid::EQUAL:
    return "=";
  case hid::LBRACKET:
    return "[";
  case hid::RBRACKET:
    return "]";
  case hid::BACKSLASH:
    return "BSL";
  case hid::SEMICOLON:
    return ";";
  case hid::QUOTE:
    return "'";
  case hid::GRAVE:
    return "`";
  case hid::COMMA:
    return ",";
  case hid::DOT:
    return ".";
  case hid::SLASH:
    return "/";
  case hid::CAPS:
    return "CAP";
  case hid::INSERT:
    return "INS";
  case hid::HOME:
    return "HOM";
  case hid::PAGE_UP:
    return "PGU";
  case hid::DELETE:
    return "DEL";
  case hid::END:
    return "END";
  case hid::PAGE_DOWN:
    return "PGD";
  case hid::RIGHT:
    return "RGT";
  case hid::LEFT:
    return "LFT";
  case hid::DOWN:
    return "DWN";
  case hid::UP:
    return "UP";
  case hid::NUM_LOCK:
    return "NL";
  case hid::KP_SLASH:
    return "K/";
  case hid::KP_ASTERISK:
    return "K*";
  case hid::KP_MINUS:
    return "K-";
  case hid::KP_PLUS:
    return "K+";
  case hid::KP_ENTER:
    return "KE";
  case hid::KP_0:
    return "K0";
  case hid::KP_DOT:
    return "K.";
  case hid::APPLICATION:
    return "MENU";
  case hid::LEFT_CTRL:
    return "LC";
  case hid::LEFT_SHIFT:
    return "LS";
  case hid::LEFT_ALT:
    return "LA";
  case hid::LEFT_GUI:
    return "LG";
  case hid::RIGHT_CTRL:
    return "RC";
  case hid::RIGHT_SHIFT:
    return "RS";
  case hid::RIGHT_ALT:
    return "RA";
  case hid::RIGHT_GUI:
    return "RG";
  default:
    return "?";
  }
}

struct KeyboardReport {
  uint8_t modifiers;
  uint8_t reserved;
  uint8_t keys[6];
};

bool isUsageInReport(const KeyboardReport& report, uint8_t usage) {
  for (uint8_t key : report.keys) {
    if (key == usage) {
      return true;
    }
  }
  return false;
}

} // namespace

void InputController::begin() {
  keystrokeCounter_.begin();
  applyConfiguration(keystrokeCounter_.savedConfiguration());

  // STM32duino's USB setup configures every entry in PinMap_USB_OTG_FS,
  // including PA8 as the optional USB SOF output. PA8 is COL11 on this board,
  // so start HID first and then reclaim all matrix GPIOs below.
  installUsbHidSupport();
  HID_Composite_Init(HID_KEYBOARD);
  delay(250);

  // The schematic has each diode's cathode on ROW and anode toward COL through
  // the switch. Therefore COL2ROW scanning pulls one row low and reads columns
  // with pull-ups; current can then flow from COL to ROW through a closed key.
  for (uint32_t pin : kRowPins) {
    pinMode(pin, INPUT);
  }
  for (uint32_t pin : kColPins) {
    pinMode(pin, INPUT_PULLUP);
  }
  pinMode(kEncoderPinA, INPUT_PULLUP);
  pinMode(kEncoderPinB, INPUT_PULLUP);
  encoderState_ = (digitalRead(kEncoderPinA) == HIGH ? 2U : 0U) |
      (digitalRead(kEncoderPinB) == HIGH ? 1U : 0U);

  sendReport();
}

void InputController::serviceSettings() {
  settingsMenu_.setStats(keystrokeCounter_.count());
  if (settingsMenu_.resetRequested()) {
    const bool success = keystrokeCounter_.resetStats();
    settingsMenu_.setStats(keystrokeCounter_.count());
    settingsMenu_.finishReset(success);
  }
  if (!settingsMenu_.saveRequested()) {
    return;
  }
  const bool success =
      keystrokeCounter_.saveConfiguration(settingsMenu_.draft());
  if (success) {
    applyConfiguration(settingsMenu_.draft());
  }
  settingsMenu_.finishSave(success);
}

void InputController::service(uint32_t now) {
  if (now - lastScanAt_ < kScanIntervalMs) {
    return;
  }
  lastScanAt_ = now;
  updateSettingsMenu(now);
  // Matrix input is the keyboard's primary latency-sensitive work. Encoder
  // service follows immediately and is delayed by only one matrix scan.
  scanMatrix(now);
  scanEncoder();
  serviceMediaKey(now);
}

bool InputController::takeActivity() {
  const bool active = isActivityPending_;
  isActivityPending_ = false;
  return active;
}

bool InputController::takeDisplayRecoveryRequest() {
  const bool isRequested = isDisplayRecoveryPending_;
  isDisplayRecoveryPending_ = false;
  return isRequested;
}

bool InputController::isGameModeActive() const {
  return configuration().gameMode();
}
void InputController::setInteractiveAnimation(bool isActive) {
  isInteractiveAnimation_ = isActive;
}

bool InputController::takeAnimationKey(uint8_t& usage, bool& isGameMode) {
  if (animationKeyHead_ == animationKeyTail_) {
    return false;
  }
  const AnimationKey& key = animationKeys_[animationKeyTail_];
  usage = key.usage;
  isGameMode = key.isGameMode;
  animationKeyTail_ = (animationKeyTail_ + 1) % kAnimationKeyCapacity;
  return true;
}

bool InputController::isRawKeyActive() const {
  return isRawKeyActive_;
}

bool InputController::isNumLockActive() const {
  return isUsbHostNumLockActive();
}

bool InputController::isInsertModeActive() const {
  return isInsertModeActive_;
}

bool InputController::isKeyCaptureActive() const {
  return configuration().keyCapture();
}

bool InputController::isKeyActive(KeyboardKey key) const {
  const MatrixPosition& position = matrixPositionFor(key);
  return isPressed_[position.row][position.col];
}

const char* InputController::keyCaptureText() const {
  return keyCaptureText_;
}

const char* InputController::layoutBadgeLabel() const {
  return activeLayout_ == KeyboardLayout::Colemak ? "CMK" : "QTY";
}

uint32_t InputController::keystrokeCount() const {
  return keystrokeCounter_.count();
}

bool InputController::takeKeystrokeMilestone(uint32_t& milestoneCount) {
  return keystrokeCounter_.takePendingMilestone(milestoneCount);
}

bool InputController::takeAnimationStep(int8_t& direction) {
  if (pendingAnimationSteps_ == 0) {
    return false;
  }

  direction = pendingAnimationSteps_ > 0 ? 1 : -1;
  pendingAnimationSteps_ -= direction;
  return true;
}

uint8_t InputController::activeUsageAt(uint8_t row, uint8_t col) const {
  const uint8_t physicalUsage = kKeymap[row][col];
  return activeLayout_ == KeyboardLayout::Colemak
      ? colemakUsageForPhysicalKey(physicalUsage)
      : physicalUsage;
}

void InputController::toggleKeyboardLayout() {
  activeLayout_ = activeLayout_ == KeyboardLayout::Colemak
      ? KeyboardLayout::Qwerty
      : KeyboardLayout::Colemak;
}

bool InputController::isSettingsHoldDue(uint32_t now) const {
  const bool isLayoutKeyDown = isKeyActive(KeyboardKey::LayoutSwitch);
  const bool isHoldElapsed = now - layoutKeyPressedAt_ >= kSettingsHoldMs;
  return !settingsMenu_.isOpen() && isLayoutKeyDown && isHoldElapsed &&
      !isLayoutKeyUsedForRecovery_ && !isLayoutKeyHeldForSettings_;
}

void InputController::updateSettingsMenu(uint32_t now) {
  if (!isSettingsHoldDue(now)) {
    return;
  }
  settingsMenu_.open();
  // Suppress already-held keys until release, including modifiers and mute.
  for (uint8_t row = 0; row < kRows; ++row) {
    for (uint8_t col = 0; col < kCols; ++col) {
      if (isPressed_[row][col]) {
        isCapturedForGame_[row][col] = true;
      }
    }
  }
  animationKeyHead_ = animationKeyTail_ = 0;
  pendingEncoderSteps_ = pendingAnimationSteps_ = 0;
  sendReport();
  isLayoutKeyHeldForSettings_ = true;
}

void InputController::beginLayoutKeyPress(uint32_t now) {
  layoutKeyPressedAt_ = now;
  isLayoutKeyHeldForSettings_ = false;
  isLayoutKeyUsedForRecovery_ = isKeyActive(KeyboardKey::Function);
  isDisplayRecoveryPending_ |= isLayoutKeyUsedForRecovery_;
}

void InputController::finishLayoutKeyPress() {
  const bool isTap = !settingsMenu_.isOpen() && !isLayoutKeyUsedForRecovery_ &&
      !isLayoutKeyHeldForSettings_;
  isLayoutKeyUsedForRecovery_ = false;
  isLayoutKeyHeldForSettings_ = false;
  if (!isTap) {
    return;
  }
  toggleKeyboardLayout();
  if (configuration().keyCapture()) {
    appendCaptureToken("LYR");
  }
}

void InputController::queueAnimationKey(uint8_t usage, bool isGameInput) {
  const uint8_t nextHead = (animationKeyHead_ + 1) % kAnimationKeyCapacity;
  const bool isQueueFull = nextHead == animationKeyTail_;
  if (isQueueFull) {
    return;
  }
  animationKeys_[animationKeyHead_] = {usage, isGameInput};
  animationKeyHead_ = nextHead;
}

void InputController::captureAnimationKey(uint8_t row, uint8_t col) {
  const bool isGameInputActive =
      configuration().gameMode() && isInteractiveAnimation_;
  const uint8_t action = bikeControls::actionForPhysicalKey(kKeymap[row][col]);
  isCapturedForGame_[row][col] = isGameInputActive && action != 0;
  // Never send arbitrary game-mode key usages as action IDs: only bindings.
  if (isGameInputActive) {
    if (action != 0) {
      queueAnimationKey(action, true);
    }
  } else {
    queueAnimationKey(activeUsageAt(row, col), false);
  }
}

void InputController::updateNumLockGesture(
    uint8_t row, uint8_t col, uint32_t now) {
  if (!kNumLockKeyPosition.isMatch(row, col)) {
    rapidNumLockClicks_ = 0;
    return;
  }
  const bool isRapidClick = now - lastNumLockClickAt_ <= kRapidClickGapMs;
  rapidNumLockClicks_ = isRapidClick ? rapidNumLockClicks_ + 1 : 1;
  lastNumLockClickAt_ = now;
}

bool InputController::isNumLockGestureComplete() const {
  return rapidNumLockClicks_ == 4;
}

void InputController::toggleCaptureMode() {
  Configuration next = configuration();
  next.set(Setting::keyCapture, !next.keyCapture());
  if (keystrokeCounter_.saveConfiguration(next)) {
    applyConfiguration(next);
  }
  rapidNumLockClicks_ = 0;
  keyCaptureText_[0] = '\0';
}

void InputController::handleKeyPress(uint8_t row, uint8_t col, uint32_t now) {
  if (kKeymap[row][col] == hid::NONE) {
    return;
  }
  if (settingsMenu_.isOpen()) {
    isCapturedForGame_[row][col] = true;
    if (kLayerKeyPosition.isMatch(row, col)) {
      isLayoutKeyHeldForSettings_ = true;
    }
    if (kMuteKeyPosition.isMatch(row, col)) {
      settingsMenu_.select();
    } else {
      settingsMenu_.key(activeUsageAt(row, col), kKeymap[row][col]);
    }
    return;
  }
  keystrokeCounter_.recordKeystroke();
  if (kLayerKeyPosition.isMatch(row, col)) {
    beginLayoutKeyPress(now);
    return;
  }
  captureAnimationKey(row, col);
  if (isCapturedForGame_[row][col]) {
    return;
  }
  updateNumLockGesture(row, col, now);
  if (isNumLockGestureComplete()) {
    toggleCaptureMode();
    return;
  }
  if (kInsertKeyPosition.isMatch(row, col)) {
    isInsertModeActive_ = !isInsertModeActive_;
  }
  if (!configuration().keyCapture()) {
    return;
  }
  const uint8_t usage = activeUsageAt(row, col);
  appendCaptureToken(usage == kFn ? "FN" : captureTokenForUsage(usage));
}

void InputController::handleKeyRelease(uint8_t row, uint8_t col) {
  isCapturedForGame_[row][col] = false;
  if (kLayerKeyPosition.isMatch(row, col)) {
    finishLayoutKeyPress();
  }
}

void InputController::appendCaptureCharacter(char character) {
  size_t length = strlen(keyCaptureText_);
  if (length == kCaptureCharacterCount) {
    memmove(keyCaptureText_, keyCaptureText_ + 1, kCaptureCharacterCount - 1);
    length = kCaptureCharacterCount - 1;
  }
  keyCaptureText_[length] = character;
  keyCaptureText_[length + 1] = '\0';
}

void InputController::appendCaptureToken(const char* token) {
  if (keyCaptureText_[0] != '\0') {
    appendCaptureCharacter(' ');
  }
  while (*token != '\0') {
    appendCaptureCharacter(*token++);
  }
}

void InputController::scanEncoder() {
  // Each entry describes one valid edge of a quadrature cycle. Invalid
  // two-bit jumps contribute nothing, which rejects most contact bounce.
  static constexpr int8_t kTransition[16] = {
      0,
      -1,
      1,
      0,
      1,
      0,
      0,
      -1,
      -1,
      0,
      0,
      1,
      0,
      1,
      -1,
      0,
  };

  const uint8_t nextState = (digitalRead(kEncoderPinA) == HIGH ? 2U : 0U) |
      (digitalRead(kEncoderPinB) == HIGH ? 1U : 0U);
  encoderDelta_ += kTransition[(encoderState_ << 2U) | nextState];
  encoderState_ = nextState;

  if (encoderDelta_ >= 4) {
    queueEncoderStep(1);
    encoderDelta_ = 0;
  } else if (encoderDelta_ <= -4) {
    queueEncoderStep(-1);
    encoderDelta_ = 0;
  }
}

void InputController::queueEncoderStep(int8_t direction) {
  isActivityPending_ = true;
  if (settingsMenu_.isOpen()) {
    settingsMenu_.rotate(direction);
    return;
  }
  if (isPressed_[kFnKeyPosition.row][kFnKeyPosition.col]) {
    if ((direction > 0 && pendingAnimationSteps_ < 8) ||
        (direction < 0 && pendingAnimationSteps_ > -8)) {
      pendingAnimationSteps_ += direction;
    }
    return;
  }

  if ((direction > 0 && pendingEncoderSteps_ < 8) ||
      (direction < 0 && pendingEncoderSteps_ > -8)) {
    pendingEncoderSteps_ += direction;
  }
}

bool InputController::isMediaReleaseDue(uint32_t now) const {
  return mediaKey_ != 0 && now - mediaKeyPressedAt_ >= kMediaTapMs;
}

bool InputController::isMediaPressDue(uint32_t now) const {
  return mediaKey_ == 0 && now - mediaKeyReleasedAt_ >= kMediaTapMs;
}

uint8_t InputController::pendingMediaKey() const {
  if (pendingEncoderSteps_ > 0) {
    return usbConsumer::volumeUp;
  }
  if (pendingEncoderSteps_ < 0) {
    return usbConsumer::volumeDown;
  }
  return 0;
}

uint8_t InputController::nextMediaKey(uint32_t now) const {
  if (isMediaReleaseDue(now)) {
    return 0;
  }
  if (isMediaPressDue(now)) {
    return pendingMediaKey();
  }
  return mediaKey_;
}

uint8_t InputController::consumerButtons(uint8_t mediaKey) const {
  const bool isMutePressed = !settingsMenu_.isOpen() &&
      !isCapturedForGame_[kMuteKeyPosition.row][kMuteKeyPosition.col] &&
      isPressed_[kMuteKeyPosition.row][kMuteKeyPosition.col];
  return mediaKey | (isMutePressed ? usbConsumer::mute : 0);
}

void InputController::applyMediaTransition(uint8_t nextKey, uint32_t now) {
  if (nextKey == mediaKey_) {
    return;
  }
  mediaKey_ = nextKey;
  if (nextKey == 0) {
    mediaKeyReleasedAt_ = now;
    return;
  }
  pendingEncoderSteps_ += nextKey == usbConsumer::volumeUp ? -1 : 1;
  mediaKeyPressedAt_ = now;
}

void InputController::serviceMediaKey(uint32_t now) {
  const uint8_t nextKey = settingsMenu_.isOpen() ? 0 : nextMediaKey(now);
  const uint8_t report = consumerButtons(nextKey);
  const bool isReportChanged = report != lastConsumerReport_;
  // Endpoint backpressure must not consume detents or start the hold timer.
  if (isReportChanged && !sendUsbConsumerReport(report)) {
    return;
  }
  lastConsumerReport_ = report;
  applyMediaTransition(nextKey, now);
}

bool InputController::isDebounceComplete(
    uint8_t row, uint8_t col, bool isPressed, uint32_t now) const {
  const bool isStateChanged = isPressed_[row][col] != isPressed;
  const bool isStableLongEnough = now - rawChangedAt_[row][col] >= kDebounceMs;
  return isStateChanged && isStableLongEnough;
}

InputController::ScanResult InputController::scanKey(
    uint8_t row, uint8_t col, uint32_t now) {
  const bool isPressed = digitalRead(kColPins[col]) == LOW;
  if (isPressed != isRawPressed_[row][col]) {
    isRawPressed_[row][col] = isPressed;
    rawChangedAt_[row][col] = now;
    return {isPressed, false};
  }
  if (!isDebounceComplete(row, col, isPressed, now)) {
    return {isPressed, false};
  }
  isPressed_[row][col] = isPressed;
  if (isPressed) {
    handleKeyPress(row, col, now);
  } else {
    handleKeyRelease(row, col);
  }
  return {isPressed, true};
}

InputController::ScanResult InputController::scanRow(
    uint8_t row, uint32_t now) {
  // Latch low before enabling the row driver; all other rows remain high-Z.
  digitalWrite(kRowPins[row], LOW);
  pinMode(kRowPins[row], OUTPUT);
  delayMicroseconds(kLineSettleUs);
  ScanResult result;
  for (uint8_t col = 0; col < kCols; ++col) {
    const ScanResult key = scanKey(row, col, now);
    result.isAnyPressed |= key.isAnyPressed;
    result.isReportChanged |= key.isReportChanged;
  }
  pinMode(kRowPins[row], INPUT);
  return result;
}

void InputController::scanMatrix(uint32_t now) {
  ScanResult result;
  for (uint8_t row = 0; row < kRows; ++row) {
    const ScanResult scanned = scanRow(row, now);
    result.isAnyPressed |= scanned.isAnyPressed;
    result.isReportChanged |= scanned.isReportChanged;
  }
  isRawKeyActive_ = result.isAnyPressed;
  // Include held keys and releases, even when input is captured locally.
  isActivityPending_ |= result.isAnyPressed || result.isReportChanged;
  const bool isRefreshDue = now - lastReportAt_ >= kReportRefreshMs;
  if (result.isReportChanged || isRefreshDue) {
    sendReport();
    lastReportAt_ = now;
  }
}

bool isLocalUsage(uint8_t usage) {
  return usage == hid::NONE || usage == kFn || usage == kLayerKey ||
      usage == hid::MUTE;
}

void appendReportUsage(
    KeyboardReport& report, uint8_t& keyCount, uint8_t usage) {
  if (isLocalUsage(usage)) {
    return;
  }
  const bool isModifier = usage >= hid::LEFT_CTRL && usage <= hid::RIGHT_GUI;
  if (isModifier) {
    report.modifiers |= 1U << (usage - hid::LEFT_CTRL);
    return;
  }
  if (isUsageInReport(report, usage)) {
    return;
  }
  if (keyCount < sizeof(report.keys)) {
    report.keys[keyCount++] = usage;
    return;
  }
  // Boot HID ErrorRollOver; modifier handling stays independent.
  memset(report.keys, 0x01, sizeof(report.keys));
}

void InputController::sendReport() {
  KeyboardReport report = {};
  uint8_t keyCount = 0;
  for (uint8_t row = 0; row < kRows; ++row) {
    for (uint8_t col = 0; col < kCols; ++col) {
      const bool isHostKey = !settingsMenu_.isOpen() && isPressed_[row][col] &&
          !isCapturedForGame_[row][col];
      if (isHostKey) {
        appendReportUsage(report, keyCount, activeUsageAt(row, col));
      }
    }
  }
  HID_Composite_keyboard_sendReport(
      reinterpret_cast<uint8_t*>(&report), sizeof(report));
}

#endif // FORESTBOARD_KEYBOARD_MODE
