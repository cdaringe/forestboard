#include "settings/settings_menu.h"
#include <Adafruit_SH110X.h>
#include <cstdio>

namespace {
const char* const rootLabels[] = {"Display", "Keyboard", "Statistics",
    "Animations", "Save & exit", "Cancel changes"};
void textAt(Adafruit_SH1107& display, int16_t x, int16_t y, const char* text) {
  display.setCursor(x, y);
  display.print(text);
}
void arrow(
    Adafruit_SH1107& display, int16_t x, int16_t y, int8_t dx, int8_t dy) {
  display.drawLine(x, y, x + dx * 6, y + dy * 6, SH110X_WHITE);
  display.drawLine(x + dx * 6, y + dy * 6, x + dx * 3 + dy * 3,
      y + dy * 3 - dx * 3, SH110X_WHITE);
  display.drawLine(x + dx * 6, y + dy * 6, x + dx * 3 - dy * 3,
      y + dy * 3 + dx * 3, SH110X_WHITE);
}
void row(Adafruit_SH1107& display, size_t index, size_t selected, int16_t y,
    const char* label) {
  if (index == selected) {
    arrow(display, 1, y + 3, 1, 0);
  }
  textAt(display, 11, y, label);
}
} // namespace

void SettingsMenu::open() {
  draft_ = configuration();
  selected_ = 0;
  page_ = Page::Root;
  open_ = true;
  typing_ = saveRequested_ = resetRequested_ = false;
  message_ = "";
}
bool SettingsMenu::isGroupPage() const {
  return page_ == Page::Display || page_ == Page::Keyboard ||
      page_ == Page::MountainBike || page_ == Page::WarpTunnel ||
      page_ == Page::CurvedTunnel;
}
bool SettingsMenu::isBindingEditor() const {
  return page_ == Page::Edit &&
      settingDefinitions[static_cast<size_t>(setting_)].kind ==
      SettingKind::PhysicalKey;
}
SettingGroup SettingsMenu::group() const {
  switch (page_) {
  case Page::Keyboard:
    return SettingGroup::Keyboard;
  case Page::MountainBike:
    return SettingGroup::MountainBike;
  case Page::WarpTunnel:
    return SettingGroup::WarpTunnel;
  case Page::CurvedTunnel:
    return SettingGroup::CurvedTunnel;
  default:
    return SettingGroup::Display;
  }
}
Setting SettingsMenu::groupSetting(size_t index) const {
  for (size_t i = 0; i < kSettingCount; ++i) {
    if (settingDefinitions[i].group == group()) {
      if (index == 0) {
        return static_cast<Setting>(i);
      }
      --index;
    }
  }
  return Setting::Count;
}
size_t SettingsMenu::itemCount() const {
  if (page_ == Page::Root) {
    return 6;
  }
  if (page_ == Page::Animations) {
    return 4;
  }
  if (page_ == Page::Statistics || page_ == Page::ConfirmReset) {
    return 2;
  }
  size_t count = 1; // Back row.
  for (const auto& definition : settingDefinitions) {
    if (definition.group == group()) {
      ++count;
    }
  }
  return count;
}
void SettingsMenu::rotate(int8_t direction) {
  if (!open_ || saveRequested_ || resetRequested_ || direction == 0) {
    return;
  }
  message_ = "";
  if (page_ != Page::Edit) {
    const int count = itemCount();
    selected_ =
        (static_cast<int>(selected_) + (direction > 0 ? 1 : -1) + count) %
        count;
    return;
  }
  if (isBindingEditor()) {
    draft_.set(setting_, stepPhysicalKey(draft_.get(setting_), direction));
    return;
  }
  const auto& definition = settingDefinitions[static_cast<size_t>(setting_)];
  const uint32_t current = typing_ ? number_ : draft_.get(setting_);
  int64_t next = static_cast<int64_t>(current) +
      (direction > 0 ? definition.step
                     : -static_cast<int64_t>(definition.step));
  if (next < definition.minimum) {
    next = definition.minimum;
  }
  if (next > definition.maximum) {
    next = definition.maximum;
  }
  draft_.set(setting_, static_cast<uint32_t>(next));
  typing_ = false;
}
void SettingsMenu::startEdit() {
  setting_ = groupSetting(selected_);
  if (setting_ == Setting::Count) {
    back();
    return;
  }
  originalValue_ = draft_.get(setting_);
  parent_ = page_;
  parentSelection_ = selected_;
  page_ = Page::Edit;
  typing_ = false;
}
void SettingsMenu::select() {
  if (!open_ || saveRequested_ || resetRequested_) {
    return;
  }
  message_ = "";
  switch (page_) {
  case Page::Root:
    if (selected_ == 4) {
      if (!draft_.isValid()) {
        message_ = "Keys must be unique";
        return;
      }
      saveRequested_ = true;
      return;
    }
    if (selected_ == 5) {
      open_ = false;
      return;
    }
    page_ = selected_ == 0 ? Page::Display
        : selected_ == 1   ? Page::Keyboard
        : selected_ == 2   ? Page::Statistics
                           : Page::Animations;
    selected_ = 0;
    break;
  case Page::Animations:
    if (selected_ == 3) {
      back();
      return;
    }
    page_ = selected_ == 0 ? Page::MountainBike
        : selected_ == 1   ? Page::WarpTunnel
                           : Page::CurvedTunnel;
    selected_ = 0;
    break;
  case Page::Display:
  case Page::Keyboard:
  case Page::MountainBike:
  case Page::WarpTunnel:
  case Page::CurvedTunnel:
    startEdit();
    break;
  case Page::Statistics:
    if (selected_ == 1) {
      back();
      return;
    }
    page_ = Page::ConfirmReset;
    selected_ = 0; // Safe default: Cancel.
    break;
  case Page::ConfirmReset:
    if (selected_ == 0) {
      back();
    } else {
      resetRequested_ = true;
    }
    break;
  case Page::Edit:
    if (typing_ && !draft_.set(setting_, number_)) {
      message_ = "Outside range";
      return;
    }
    page_ = parent_;
    selected_ = parentSelection_;
    typing_ = false;
    break;
  }
}
void SettingsMenu::back() {
  message_ = "";
  switch (page_) {
  case Page::Edit:
    draft_.set(setting_, originalValue_);
    page_ = parent_;
    selected_ = parentSelection_;
    typing_ = false;
    break;
  case Page::Root:
    open_ = false;
    break;
  case Page::ConfirmReset:
    page_ = Page::Statistics;
    selected_ = 0;
    break;
  case Page::MountainBike:
  case Page::WarpTunnel:
  case Page::CurvedTunnel:
    selected_ = page_ == Page::MountainBike ? 0
        : page_ == Page::WarpTunnel         ? 1
                                            : 2;
    page_ = Page::Animations;
    break;
  default:
    selected_ = page_ == Page::Display ? 0
        : page_ == Page::Keyboard      ? 1
        : page_ == Page::Statistics    ? 2
                                       : 3;
    page_ = Page::Root;
    break;
  }
}
void SettingsMenu::key(uint8_t usage, uint8_t physicalUsage) {
  if (!open_ || saveRequested_ || resetRequested_) {
    return;
  }
  if (isBindingEditor()) {
    if (usage == 0x29) {
      back();
      return;
    }
    if (usage == 0x28 || usage == 0x58) {
      select();
      return;
    }
    if (draft_.set(setting_, physicalUsage == 0 ? usage : physicalUsage)) {
      message_ = "Enter to accept";
    } else {
      message_ = "Choose another key";
    }
    return;
  }
  if (usage == 0x28 || usage == 0x58 || usage == 0x4f) {
    select();
    return;
  }
  if (usage == 0x29 || usage == 0x50) {
    back();
    return;
  }
  if (usage == 0x52) {
    rotate(page_ == Page::Edit ? 1 : -1);
    return;
  }
  if (usage == 0x51) {
    rotate(page_ == Page::Edit ? -1 : 1);
    return;
  }
  if (usage == 0x2a && page_ == Page::Edit && typing_) {
    number_ /= 10;
    message_ = "";
    return;
  }
  int digit = -1;
  if (usage >= 0x1e && usage <= 0x26) {
    digit = usage - 0x1e + 1;
  }
  if (usage == 0x27 || usage == 0x62) {
    digit = 0;
  }
  if (usage >= 0x59 && usage <= 0x61) {
    digit = usage - 0x59 + 1;
  }
  if (digit < 0) {
    return;
  }
  if (isGroupPage()) {
    if (groupSetting(selected_) == Setting::Count) {
      return;
    }
    startEdit();
    if (isBindingEditor()) {
      draft_.set(setting_, physicalUsage == 0 ? usage : physicalUsage);
      return;
    }
  }
  if (page_ != Page::Edit) {
    return;
  }
  if (!typing_) {
    number_ = 0;
  }
  typing_ = true;
  if (number_ <= (UINT32_MAX - digit) / 10) {
    number_ = number_ * 10 + digit;
    message_ = "";
  } else {
    message_ = "Number too large";
  }
}
void SettingsMenu::finishSave(bool success) {
  saveRequested_ = false;
  if (success) {
    open_ = false;
  } else {
    message_ = "Save failed. Retry.";
  }
}
void SettingsMenu::finishReset(bool success) {
  resetRequested_ = false;
  page_ = Page::Statistics;
  selected_ = 0;
  message_ = success ? "Stats reset" : "Reset failed. Retry.";
}
void SettingsMenu::render(Adafruit_SH1107& display) const {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  display.setTextWrap(false);
  const char* title = "SETTINGS";
  switch (page_) {
  case Page::Display:
    title = "DISPLAY";
    break;
  case Page::Keyboard:
    title = "KEYBOARD";
    break;
  case Page::Statistics:
  case Page::ConfirmReset:
    title = "STATISTICS";
    break;
  case Page::Edit:
    title = "EDIT SETTING";
    break;
  case Page::Animations:
    title = "ANIMATIONS";
    break;
  case Page::MountainBike:
    title = "MOUNTAIN BIKE";
    break;
  case Page::WarpTunnel:
    title = "WARP TUNNEL";
    break;
  case Page::CurvedTunnel:
    title = "CURVED TUNNEL";
    break;
  default:
    break;
  }
  textAt(display, 0, 0, title);
  char text[32];
  switch (page_) {
  case Page::Root:
    for (size_t i = 0; i < 6; ++i) {
      row(display, i, selected_, 16 + i * 13, rootLabels[i]);
    }
    break;
  case Page::Display:
  case Page::Keyboard:
  case Page::MountainBike:
  case Page::WarpTunnel:
  case Page::CurvedTunnel:
    // Keep the current row visible as schema-driven submenus grow.
    {
      constexpr size_t visibleRows = 6;
      const size_t first =
          selected_ >= visibleRows ? selected_ - visibleRows + 1 : 0;
      if (first > 0) {
        arrow(display, 115, 7, 0, -1);
      }
      if (first + visibleRows < itemCount()) {
        arrow(display, 123, 1, 0, 1);
      }
      for (size_t i = first; i < itemCount() && i < first + visibleRows; ++i) {
        const Setting setting = groupSetting(i);
        row(display, i, selected_, 14 + (i - first) * 12,
            setting == Setting::Count
                ? "Back"
                : settingDefinitions[static_cast<size_t>(setting)].label);
      }
    }
    break;
  case Page::Animations: {
    const char* names[] = {
        "Mountain bike", "Warp tunnel", "Curved tunnel", "Back"};
    for (size_t i = 0; i < 4; ++i) {
      row(display, i, selected_, 16 + i * 13, names[i]);
    }
    break;
  }
  case Page::Edit: {
    const auto& definition = settingDefinitions[static_cast<size_t>(setting_)];
    textAt(display, 0, 18, definition.label);
    if (isBindingEditor()) {
      physicalKeyLabel(draft_.get(setting_), text, sizeof(text));
      textAt(display, 0, 36, text);
      textAt(display, 0, 54, "Press physical key");
      textAt(display, 0, 72, "Knob also chooses");
      break;
    }
    snprintf(text, sizeof(text), "%lu %s",
        static_cast<unsigned long>(typing_ ? number_ : draft_.get(setting_)),
        definition.unit);
    textAt(display, 0, 36, text);
    snprintf(text, sizeof(text), "Range %lu-%lu",
        static_cast<unsigned long>(definition.minimum),
        static_cast<unsigned long>(definition.maximum));
    textAt(display, 0, 54, text);
    textAt(display, 0, 72, "Knob or type digits");
    break;
  }
  case Page::Statistics:
    snprintf(
        text, sizeof(text), "Keys: %lu", static_cast<unsigned long>(count_));
    textAt(display, 0, 20, text);
    row(display, 0, selected_, 44, "Reset stats");
    row(display, 1, selected_, 60, "Back");
    break;
  case Page::ConfirmReset:
    textAt(display, 0, 18, "Reset count to zero?");
    textAt(display, 0, 32, "Cannot be undone.");
    row(display, 0, selected_, 52, "Cancel");
    row(display, 1, selected_, 68, "Yes, reset stats");
    break;
  }
  textAt(display, 0, 88, message_);
  if (isBindingEditor()) {
    textAt(display, 0, 100, "Enter/click: accept");
    textAt(display, 0, 114, "Esc: cancel edit");
    display.setTextWrap(true);
    return;
  }
  arrow(display, 3, 106, 0, -1);
  arrow(display, 12, 100, 0, 1);
  textAt(display, 20, 100, page_ == Page::Edit ? "Adjust" : "Move");
  arrow(display, 70, 103, 1, 0);
  textAt(display, 81, 100, "Select");
  arrow(display, 9, 117, -1, 0);
  textAt(display, 15, 114, "Back  Click=Enter");
  display.setTextWrap(true);
}
