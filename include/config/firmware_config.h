#pragma once

#include <Arduino.h>

// Hardware/build-only options. Runtime settings are generated from
// settings.def.
#include "config/configuration.h"

#ifndef FORESTBOARD_OLED_SPI_HZ
#define FORESTBOARD_OLED_SPI_HZ 2000000UL
#endif
#ifndef FORESTBOARD_OLED_SOFTWARE_SPI
// The installed STM32 SPI driver requires MISO. Passing NC leaves its
// peripheral uninitialized; the first transfer then faults during setup.
// PB14 is OLED D/C, so keep the working write-only software transport.
#define FORESTBOARD_OLED_SOFTWARE_SPI 1
#endif
#ifndef FORESTBOARD_OLED_DEBUG_FRAME_INTERVAL_MS
#define FORESTBOARD_OLED_DEBUG_FRAME_INTERVAL_MS 750UL
#endif

#ifndef FORESTBOARD_OLED_DEBUG_MODE
#define FORESTBOARD_OLED_DEBUG_MODE 0
#endif

namespace firmwareConfig {

constexpr bool isOledDebugMode = FORESTBOARD_OLED_DEBUG_MODE != 0;
constexpr uint32_t oledDebugFrameIntervalMs =
    FORESTBOARD_OLED_DEBUG_FRAME_INTERVAL_MS;
static_assert(
    FORESTBOARD_OLED_SPI_HZ > 0 && FORESTBOARD_OLED_SPI_HZ <= 4000000UL,
    "SH1107 SPI must not exceed 4 MHz at 3.3 V");
static_assert(
    !FORESTBOARD_OLED_SOFTWARE_SPI || FORESTBOARD_OLED_SPI_HZ <= 2000000UL,
    "Software SPI needs at least 250 ns per half-cycle");
static_assert(
    oledDebugFrameIntervalMs > 0, "Debug frame interval must be positive");
} // namespace firmwareConfig
