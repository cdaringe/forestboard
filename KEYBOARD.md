# Keyboard firmware

The default PlatformIO environment is the STM32F411RE keyboard bring-up build.
It scans the 12-row by 12-column `COL2ROW` matrix, sends standard USB HID
keyboard reports, and runs the OLED animation manager alongside the scanner.

PB2 emits one short pulse each second while the main loop is alive. It stays
continuously lit while any raw switch closure is detected. This distinguishes
the electrical matrix from the USB report path without requiring serial USB.

| Environment | MCU | USB behavior | Matrix behavior |
|---|---|---|---|
| `genericSTM32F411RE` | STM32F411RET6 | Keyboard HID | Safe keyboard-only scan |
| `genericSTM32F446RE` | STM32F446RET6 | Keyboard HID | Safe keyboard-only scan |

Build and upload the currently installed F411 production firmware:

```sh
pio run -e genericSTM32F411RE -t upload
```

The STM32 must first be in its ROM DFU bootloader. Hold BOOT0, tap RESET,
release BOOT0, then upload. After upload, tap RESET once (or power-cycle) to run
the application. No monitor port is expected in keyboard mode; the host should
instead enumerate a USB keyboard.

The bottom modifier row, in physical left-to-right order, is:

```text
LCtrl  Menu  LAlt  Win/Meta  Space  Fn  RAlt  RCtrl
```

The navigation cluster is:

```text
Insert  Home  Page Up
Delete  End   Page Down
              Layout/Game
```

`Fn` is now `SW105`, immediately right of Space. `LAYER_KEY` is `SW79`,
below Page Down. Tapping it switches Colemak (`CMK`) / QWERTY (`QTY`);
holding it for 350 ms enters momentary Game mode, shown by a `GAME` badge.
Releasing a held layout key exits Game mode without changing the layout.
This remains a Colemak-first keyboard: leave the host input source on Colemak.
`CMK` sends canonical matrix usages; `QTY` applies the existing inverse Colemak
mapping. The physical remap does not change that translation.

Hold `Fn` and tap `Layout/Game` to request an OLED hardware reset and redraw.
This chord does not switch layouts or enter Game mode.

The encoder rotates through volume up/down and its push switch sends Mute.
These use USB HID Consumer Control usages (`0xE9`, `0xEA`, `0xE2`) on their
own interface; ordinary typing retains its boot keyboard interface and LED
output report. Encoder presses and releases each have a 16 ms interval, and
busy endpoints are retried without discarding detents. This replaces the
keyboard-page volume usages that failed on macOS. Reconnect USB after flashing
so the host reads the new descriptors; macOS/Bluetooth behavior needs a device test.
Hold `Fn` while turning the encoder to choose an animation immediately:
clockwise advances and counter-clockwise goes back. A manual change restarts
the five-minute animation timer. Four rapid Num Lock taps toggle the OLED
key-capture overlay. Enabled screen meta-key badges include `NL`, `INS`, `DBG`,
and a live `FN` indicator while the function key is held.

The OLED advances to the next registered animation every five minutes. Every
debounced press on a populated matrix position increments a persistent
keystroke counter. The compact counter badge is stacked above the `CMK`/`QTY`
badge, and powers of ten (`1`, `10`, `100`, `1K`, and onward) trigger a short
milestone celebration. Counter checkpoints occur only at multiples of **10,000 keystrokes**; milestone
celebrations do not cause additional flash writes. Normal power loss can discard
up to 9,999 keystrokes since the last successful checkpoint.

The journal reserves flash sectors 6 and 7 (the final 256 KiB), leaving 256 KiB
for firmware. It appends 16-byte records within the active sector to reduce wear.
When that sector fills, it erases the other sector, writes and reads back the new
payload, then writes a final commit marker. Only a verified committed record
promotes the destination. The previous sector remains intact until the next
rollover needs that space. Boot scans both sectors for the newest valid committed
checkpoint; CRC-32 rejects torn or corrupted records. Failed append attempts skip
their slots rather than rewriting partially programmed flash. Valid KEY2 records
from the previous 100-key journal are retained and new writes use KEY3 records.
Flash write failures retain the previous checkpoint and are retried at the next
10,000-key boundary.

## OLED configuration and isolation mode

OLED behavior is parameterized in `include/config/firmware_config.h`. Every default
can be overridden with a PlatformIO `-D` build flag:

| Build setting | Production default | Purpose |
|---|---:|---|
| `ERGOBOARD_OLED_FRAME_INTERVAL_MS` | `8` | Target display update interval (125 FPS) |
| `ERGOBOARD_OLED_SPI_HZ` | `3000000` | Hardware SPI clock request, capped at 4 MHz |
| `ERGOBOARD_OLED_SOFTWARE_SPI` | `0` | Set to `1` for the previous software-SPI transport (slower) |
| `ERGOBOARD_OLED_REFRESH_MS` | `2000` | Reassert controller configuration without blanking |
| `ERGOBOARD_OLED_SHADING` | `1` | Spatial dither shading on animations |
| `ERGOBOARD_OLED_DEBUG_FRAME_INTERVAL_MS` | `750` | Diagnostic update interval |
| `ERGOBOARD_OLED_ANIMATION_DURATION_MS` | `300000` | Time before automatic animation rotation |
| `ERGOBOARD_OLED_META_KEY_BADGES` | `1` | Enable the `NL/INS/DBG/FN` badges |
| `ERGOBOARD_OLED_DEBUG_MODE` | `0` | Replace all animations with the isolation scene |

The OLED-debug environments keep the normal 1 ms matrix scanner and USB HID,
but bypass regular animations, reactive effects, key-capture painting, and
milestone painting. They draw fixed reference geometry plus one slowly moving
marker. Upload the current-board diagnostic build with:

```sh
pio run -e genericSTM32F411RE_oled_debug -t upload
```

## Display timing and recovery

The [SH1107 datasheet](https://cdn-shop.adafruit.com/product-files/5297/SH1107V2.1.pdf)
(pp. 35, 47, 52, 55) gives a nominal 720 kHz oscillator, up to +50% adjustment,
54 clocks per common, and 128 commons: `720000 * 1.5 / (54 * 128) = 156.25 Hz`.
Clock setting `0xF0` selects that oscillator setting with divide-by-one. The
125 FPS target is 80% of this calculated rate, not a measured or guaranteed
panel refresh rate. The oscillator has no specified min/max in that table.
Actual frame throughput still needs verification on the assembled board.

SPI2 explicitly uses PB15 MOSI and PB13 clock with **no MISO**; PB14 remains
D/C. At the configured MCU clocks, a 3 MHz request gives 3 MHz on F411 and
2.8125 MHz on F446, below the display's 4 MHz limit at 3.3 V. Each frame sends
16 separately addressed 128-byte pages, servicing the keyboard between pages.
The software-SPI fallback retains the exact wiring but cannot promise 125 FPS.
Animation motion uses elapsed time instead of speeding up with refresh rate.

Every two seconds the firmware restores addressing, orientation, contrast,
and timing without sending display-off/reset commands. Every frame rewrites
all display RAM. This repairs common silent corruption without a periodic
flash. Hardware reset is reserved for the manual chord or reported transfer/
initialization failures, with nonblocking 100 ms power settling and a complete
frame before display-on. Startup white tests and multi-second reset delays
are removed. SPI provides no panel acknowledgement: firmware cannot detect
all wiring noise, power faults, or a disconnected panel; persistent problems
still need an electrical check.

Game mode pauses automatic animation rotation. Select the mountain-bike scene
with `Fn` + encoder, then hold `Layout/Game` to play. In that scene, `↑`/`↓`
change lanes, `J` places a ramp ahead, and `B`/`C`/`T` perform backflip/cancan/360
while airborne with enough time to finish. Those six controls are consumed
only when Game mode and the bike scene are both active; other keys type
normally. Consumed keys remain suppressed until released, even if the mode
key is released first. Bindings follow the logical letters in both CMK/QTY.
Outside Game mode, typing produces occasional random bike actions and all keys
retain normal host output.

Run host behavior tests with `test/host/run.sh`. They cover mapping, tap/hold,
control capture, Consumer HID descriptors/report retries, display recovery,
animation timing, and bike tricks under address/undefined-behavior sanitizers.
