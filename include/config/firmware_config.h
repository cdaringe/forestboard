#pragma once

#include <Arduino.h>

// Every setting can be overridden with a PlatformIO -D build flag. Keeping
// the defaults here gives the display subsystem one explicit configuration
// surface instead of scattering timing and feature switches through it.
#ifndef ERGOBOARD_OLED_FRAME_INTERVAL_MS
// 720 kHz * 1.5 / (54 DCLKs * 128 commons) = 156.25 Hz nominal.
// An 8 ms frame interval targets 125 FPS, 80% of that theoretical rate.
#define ERGOBOARD_OLED_FRAME_INTERVAL_MS 8UL
#endif

#ifndef ERGOBOARD_OLED_SPI_HZ
#define ERGOBOARD_OLED_SPI_HZ 3000000UL
#endif
#ifndef ERGOBOARD_OLED_SOFTWARE_SPI
#define ERGOBOARD_OLED_SOFTWARE_SPI 0
#endif
#ifndef ERGOBOARD_OLED_REFRESH_MS
#define ERGOBOARD_OLED_REFRESH_MS 2000UL
#endif
#ifndef ERGOBOARD_OLED_SHADING
#define ERGOBOARD_OLED_SHADING 1
#endif

#ifndef ERGOBOARD_OLED_DEBUG_FRAME_INTERVAL_MS
#define ERGOBOARD_OLED_DEBUG_FRAME_INTERVAL_MS 750UL
#endif

#ifndef ERGOBOARD_OLED_ANIMATION_DURATION_MS
#define ERGOBOARD_OLED_ANIMATION_DURATION_MS (5UL * 60UL * 1000UL)
#endif

#ifndef ERGOBOARD_OLED_META_KEY_BADGES
#define ERGOBOARD_OLED_META_KEY_BADGES 1
#endif

#ifndef ERGOBOARD_OLED_DEBUG_MODE
#define ERGOBOARD_OLED_DEBUG_MODE 0
#endif

namespace firmwareConfig {

constexpr bool isOledDebugMode = ERGOBOARD_OLED_DEBUG_MODE != 0;
constexpr bool isOledMetaKeyBadgesEnabled = ERGOBOARD_OLED_META_KEY_BADGES != 0;
constexpr uint32_t oledFrameIntervalMs = isOledDebugMode
    ? ERGOBOARD_OLED_DEBUG_FRAME_INTERVAL_MS
    : ERGOBOARD_OLED_FRAME_INTERVAL_MS;
constexpr uint32_t oledAnimationDurationMs =
    ERGOBOARD_OLED_ANIMATION_DURATION_MS;
constexpr uint32_t oledRefreshMs = ERGOBOARD_OLED_REFRESH_MS;
constexpr bool isOledShading = ERGOBOARD_OLED_SHADING != 0;
static_assert(ERGOBOARD_OLED_SPI_HZ > 0 && ERGOBOARD_OLED_SPI_HZ <= 4000000UL,
    "SH1107 SPI must not exceed 4 MHz at 3.3 V");
static_assert(oledRefreshMs > 0, "OLED refresh interval must be positive");

static_assert(
    oledFrameIntervalMs > 0, "OLED frame interval must be greater than zero");
static_assert(oledAnimationDurationMs > 0,
    "OLED animation duration must be greater than zero");

} // namespace firmwareConfig
