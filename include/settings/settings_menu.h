#pragma once
#include "config/configuration.h"
class Adafruit_SH1107;

class SettingsMenu {
public:
  void open();
  void rotate(int8_t direction);
  void key(uint8_t usage, uint8_t physicalUsage = 0);
  void select();
  void render(Adafruit_SH1107& display) const;
  bool isOpen() const {
    return open_;
  }
  bool saveRequested() const {
    return saveRequested_;
  }
  bool resetRequested() const {
    return resetRequested_;
  }
  const Configuration& draft() const {
    return draft_;
  }
  void finishSave(bool success);
  void finishReset(bool success);
  void setStats(uint32_t count) {
    count_ = count;
  }

private:
  enum class Page : uint8_t {
    Root,
    Display,
    Keyboard,
    Statistics,
    Edit,
    ConfirmReset,
    Animations,
    MountainBike,
    WarpTunnel,
    CurvedTunnel
  };
  void back();
  void startEdit();
  size_t itemCount() const;
  Setting groupSetting(size_t index) const;
  SettingGroup group() const;
  bool isGroupPage() const;
  bool isBindingEditor() const;
  Configuration draft_;
  Page page_ = Page::Root, parent_ = Page::Display;
  Setting setting_ = Setting::fps;
  size_t selected_ = 0, parentSelection_ = 0;
  bool open_ = false, typing_ = false, saveRequested_ = false,
       resetRequested_ = false;
  uint32_t number_ = 0, originalValue_ = 0, count_ = 0;
  const char* message_ = "";
};
