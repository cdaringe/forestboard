#pragma once
#include "config/physical_keys.h"
#include <cstddef>
#include <cstdint>

enum class SettingGroup : uint8_t {
  Display,
  Keyboard,
  MountainBike,
  WarpTunnel,
  CurvedTunnel
};
enum class SettingKind : uint8_t { Number, PhysicalKey };

enum class Setting : uint8_t {
#define SETTING(name, label, unit, value, low, high, step, group, kind) name,
#include "config/settings.def"
#undef SETTING
  Count
};
constexpr size_t kSettingCount = static_cast<size_t>(Setting::Count);
struct SettingDefinition {
  const char* label;
  const char* unit;
  uint32_t initial, minimum, maximum, step;
  SettingGroup group;
  SettingKind kind;
};
inline constexpr SettingDefinition settingDefinitions[] = {
#define SETTING(name, label, unit, value, low, high, step, group, kind)        \
  {label, unit, value, low, high, step, SettingGroup::group, SettingKind::kind},
#include "config/settings.def"
#undef SETTING
};

#define SETTING(name, label, unit, value, low, high, step, group, kind)        \
  static_assert(                                                               \
      low <= value && value <= high && high <= UINT16_MAX && step > 0,         \
      "Invalid setting definition: " #name);
#include "config/settings.def"
#undef SETTING

class Configuration {
public:
  uint32_t get(Setting setting) const {
    return values_[static_cast<size_t>(setting)];
  }
  bool set(Setting setting, uint32_t value) {
    const size_t index = static_cast<size_t>(setting);
    if (index >= kSettingCount || value < settingDefinitions[index].minimum ||
        value > settingDefinitions[index].maximum) {
      return false;
    }
    if (settingDefinitions[index].kind == SettingKind::PhysicalKey &&
        !isBindablePhysicalKey(value)) {
      return false;
    }
    values_[index] = value;
    return true;
  }
#define SETTING(name, label, unit, value, low, high, step, group, kind)        \
  uint32_t name() const {                                                      \
    return get(Setting::name);                                                 \
  }
#include "config/settings.def"
#undef SETTING
  uint32_t frameIntervalMs() const {
    return (1000U + fps() - 1U) / fps();
  }
  uint32_t animationDurationMs() const {
    return animationSeconds() * 1000U;
  }
  uint32_t refreshIntervalMs() const {
    return refreshSeconds() * 1000U;
  }
  bool isValid() const {
    for (size_t i = 0; i < kSettingCount; ++i) {
      if (settingDefinitions[i].kind != SettingKind::PhysicalKey) {
        continue;
      }
      if (!isBindablePhysicalKey(values_[i])) {
        return false;
      }
      for (size_t j = i + 1; j < kSettingCount; ++j) {
        if (settingDefinitions[j].kind == SettingKind::PhysicalKey &&
            settingDefinitions[j].group == settingDefinitions[i].group &&
            values_[i] == values_[j]) {
          return false;
        }
      }
    }
    return true;
  }
  bool operator==(const Configuration& other) const {
    for (size_t i = 0; i < kSettingCount; ++i) {
      if (values_[i] != other.values_[i]) {
        return false;
      }
    }
    return true;
  }
  bool operator!=(const Configuration& other) const {
    return !(*this == other);
  }

private:
  uint32_t values_[kSettingCount] = {
#define SETTING(name, label, unit, value, low, high, step, group, kind) value,
#include "config/settings.def"
#undef SETTING
  };
};

// The one live configuration. Menu drafts are published only after a verified
// save.
inline Configuration activeConfiguration;
inline const Configuration& configuration() {
  return activeConfiguration;
}
inline void applyConfiguration(const Configuration& value) {
  activeConfiguration = value;
}
