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
              Layout/Settings
```

`Fn` is now `SW105`, immediately right of Space. `LAYER_KEY` is `SW79`,
below Page Down. Tapping it switches Colemak (`CMK`) / QWERTY (`QTY`);
holding it for 350 ms opens Settings. It never toggles Game mode.
Enable or disable Game mode in **Settings → Keyboard → Game mode**, then
choose **Save & exit**. It stays active after release and across power cycles.
Set the host input source to **US QWERTY** for both firmware layouts.
The keyboard starts in Colemak (`CMK`) and translates physical positions into
Colemak HID usages. `QTY` sends the canonical QWERTY usages unchanged.
When upgrading from the Colemak-host firmware, switch the host from Colemak
to US QWERTY after flashing to avoid applying the layout twice.

Hold `Fn` and tap `Layout/Settings` to request an OLED hardware reset and redraw.
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
the configured animation timer (five minutes by default). Four rapid Num Lock taps toggle the OLED
key-capture overlay. Enabled screen meta-key badges include `NL`, `INS`, `DBG`,
and a live `FN` indicator while the function key is held.

The OLED advances to the next registered animation every five minutes. Every
debounced press on a populated matrix position increments a persistent
keystroke counter. The compact counter badge is stacked above the `CMK`/`QTY`
badge, and powers of ten (`1`, `10`, `100`, `1K`, and onward) trigger a short
milestone celebration. Automatic counter checkpoints occur at multiples of **10,000 keystrokes**.
Explicit settings saves and confirmed statistics resets also commit a snapshot;
milestone celebrations do not cause additional flash writes. Normal power loss can discard
up to 9,999 keystrokes since the last successful checkpoint.

The journal reserves flash sectors 6 and 7 (the final 256 KiB), leaving 256 KiB
for firmware. New CFG1 records are 64-byte, versioned snapshots containing both
the complete settings model and the keystroke count. Existing KEY2/KEY3 counter
records are read without erasing them; defaults are used until settings are saved.
Each sector holds 2,048 new snapshots. Knob turns and typing edit RAM only, and
saving unchanged settings does not write flash.

The payload and CRC-32 are programmed and read back before the final commit
marker. Boot accepts only committed snapshots with valid CRC, schema version,
and setting bounds. At rollover, the other sector is erased and verified before
writing; the previous valid sector is retained until the destination commits.
Failed append slots are skipped. Sequence exhaustion and recognized future
record versions disable writes instead of risking wraparound or erasing newer
settings. A failed save keeps the previous live settings and the menu open.
A failed reset keeps the previous live count. Interrupted saves/resets recover
either the entire old snapshot or the entire new one, never a mixture.

## Settings menu

Hold Layout/Settings for 350 ms, then release. The menu has **Display**,
**Keyboard**, **Animations**, and **Statistics** pages, plus **Save & exit** and **Cancel changes**.
Holding this key opens the menu only; Game mode is controlled inside the menu.

- Turn the knob or use Up/Down to move between rows. Click, Enter, or Right opens
  a page or setting. Left or Esc goes back; at the root it cancels changes.
- In a numeric editor, turn the knob or use Up/Down to adjust. Type digits from
  the number row or keypad to replace the value; Backspace removes a digit.
  Enter/click accepts the draft value; Left/Esc cancels that value's edit.
- Choose **Save & exit** to persist and apply the complete draft. Editing alone
  does not change the running configuration. Arrow icons indicate navigation.
- **Statistics → Reset stats** displays the keystroke total and opens a
  confirmation with **Cancel** selected. Choosing **Yes, reset stats** immediately
  commits zero while preserving saved settings. This action cannot be undone by
  later cancelling menu edits. Menu navigation does not increment the count.

While settings are open, keys and knob activity are captured locally rather
than sent to the host. Held menu keys remain suppressed until released. Automatic
animation rotation pauses while the menu is open.

The single schema, `include/config/settings.def`, generates the typed model,
defaults, validation limits, menu labels/groups/steps, and serialized field order.
Modules read the shared configuration rather than keeping independent copies.

| Page / setting | Default | Allowed range | Knob step |
|---|---:|---:|---:|
| Display / Screen FPS | 60 | 20–120 | 1 |
| Display / Animation delay | 300 s (5 min) | 10–3,600 s | 10 s |
| Display / Screen reset | 60 s | 60–3,600 s | 60 s |
| Display / Contrast | 79 | 1–255 | 1 |
| Display / Status badges | On | Off / On (0 / 1) | 1 |
| Display / Show splash | Off | Off / On (0 / 1) | 1 |
| Keyboard / Key capture | Off | Off / On (0 / 1) | 1 |
| Keyboard / Game mode | Off | Off / On (0 / 1) | 1 |

**Display → Show splash → 1**, followed by **Save & exit**, replaces animations
with the centered tree and `forestboard` indefinitely. The setting persists;
return to the menu and save 0 to resume animations. Menu rendering takes priority,
and hidden game controls do not consume typing. Diagnostic builds retain their
isolation scene. Version-1 snapshots load with Show splash off. Version-1/2
snapshots retain existing settings and count when upgrading to version 3; appended
animation fields take defaults. Version 3 packs each bounded setting into 16 bits,
retaining the 64-byte snapshot size and CRC coverage. Invalid or conflicting
physical bindings are rejected before a write and during boot validation.

FPS is a scheduling ceiling, rounded up to a whole-millisecond interval. The
20 FPS minimum matches the bike simulation's 50 ms maximum timestep. Actual
panel throughput depends on software SPI and scene rendering. Contrast changes
are applied on the next rendered frame after saving, without a hardware reset.

## OLED configuration and isolation mode

Only hardware and diagnostic build options remain in
`include/config/firmware_config.h`; runtime tuning uses the shared settings model:

| Build setting | Production default | Purpose |
|---|---:|---|
| `FORESTBOARD_OLED_SPI_HZ` | `2000000` | SPI ceiling; software supports up to 2 MHz, hardware up to 4 MHz |
| `FORESTBOARD_OLED_SOFTWARE_SPI` | `1` | Software SPI; required with the installed STM32 driver |
| `FORESTBOARD_OLED_DEBUG_FRAME_INTERVAL_MS` | `750` | Diagnostic update interval |
| `FORESTBOARD_OLED_DEBUG_MODE` | `0` | Replace all animations with the isolation scene |

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
60 FPS default and 120 FPS maximum are scheduling limits, not measured or
guaranteed panel refresh rates. The oscillator has no specified min/max in that table.
Actual frame throughput still needs verification on the assembled board.

Software SPI uses PB15 MOSI and PB13 clock; PB14 remains D/C. The installed
STM32 hardware-SPI driver requires a valid MISO pin: using `PNUM_NOT_DEFINED`
causes initialization to return without a peripheral, and the first transfer
faults during display startup. USB can enumerate before that fault while the
keyboard main loop never starts. Keep `FORESTBOARD_OLED_SOFTWARE_SPI=1` until a
compatible hardware transport is implemented and tested on the board.
Production software SPI uses explicit data-setup and clock-high delays (at
least 250 ns each), timed with the Cortex-M4 cycle counter, with atomic STM32
GPIO writes. This bypasses Adafruit BusIO's
software transfer loop, whose nominal 1 MHz setting rounds its half-cycle delay
to zero and sets data immediately before raising the clock. The build-time
`FORESTBOARD_OLED_SPI_HZ` ceiling now applies to both transports; software timing
rounds each half-cycle up to CPU cycles. Initialization verifies the counter is
running before using it, and elapsed-cycle subtraction handles counter wrap.

The display pins use MEDIUM output slew instead of STM32duino's VERY_HIGH
default. Output slew controls electrical edge speed independently of the SPI
bit rate. The [F411 datasheet, Table 55](https://www.st.com/resource/en/datasheet/stm32f411re.pdf)
specifies a maximum 10 ns rise/fall at 3.3 V and 50 pF for MEDIUM, within the
SH1107's 15 ns requirement. This reduces edge aggressiveness; it is not proof
that ringing caused the observed corruption. Actual loading and waveforms
still need to be checked on the assembled keyboard.

Each frame sends 64 separately addressed 32-byte bursts, servicing the keyboard
with chip select high between bursts. Every burst clears a pending command
parameter, exits read-modify-write mode, and restores page addressing plus the
absolute page/column. A transient address-mode error can therefore be repaired
on the next burst, without waiting for the periodic configuration refresh.
At the default speed, explicit delays total about 10.1 ms per frame (about
0.16 ms between input callbacks), plus GPIO, rendering, interrupt, and input
overhead. This removes the prior whole-microsecond writer's 40.2 ms delay
floor; the panel's internal scan rate is separate. These are calculated
delay budgets, not measurements on the assembled keyboard.
Animation motion uses elapsed time instead of speeding up with refresh rate.

Once per minute by default (adjustable in Settings), the firmware restores addressing, orientation, contrast,
and timing without sending display-off/reset commands. Every frame rewrites
all display RAM. This attempts to repair silent controller corruption; visual behavior still
needs verification on the physical panel. Hardware reset is reserved for the manual chord or reported transfer/
initialization failures. Reset is held low for 100 ms to allow the supply to
settle before initialization, then configuration is followed by a separate
100 ms settling interval and a complete frame before display-on. Both waits
are nonblocking, so keyboard scanning continues. The same sequence runs on a
manual recovery. Startup white tests and multi-second reset delays
are removed. SPI provides no panel acknowledgement: firmware cannot detect
all wiring noise, power faults, or a disconnected panel; persistent problems
still need an electrical check.

Game mode pauses automatic animation rotation. Select the mountain-bike scene
with `Fn` + encoder, then enable **Settings → Keyboard → Game mode** and save.
Ramps spawn automatically at randomized intervals in Game mode, even without
keypresses. Defaults are 60 px/s rider speed and a 1,600 ms average spawn gap
(randomized to 75–125%; most ramps are in the rider's lane).

**Settings → Animations → Mountain bike** exposes speed (20–140 px/s), jump gap
(500–5,000 ms), and all seven physical control bindings. The defaults are:

| Action | Colemak physical position | QWERTY physical position |
|---|---|---|
| Backflip | A | A |
| Cancan | R | S |
| 360 | S | D |
| Wheelie | T | F |
| Extra ramp | D | G |
| Lane up / down | Up / Down | Up / Down |

Bindings use the switch's canonical physical usage before layout translation;
they do not move when CMK/QTY changes. Select a binding, press a physical key or
use the knob, then Enter/click to accept and **Save & exit** to persist. Escape
cancels. Arrow keys can be bound; Enter/Escape, modifiers, Fn, Layout and encoder
push remain reserved. Duplicate assignments block saving. Only assigned controls
are consumed while the bike scene and Game mode are active; held captured keys
remain suppressed until release, even after changing modes. A ground wheelie
raises and lowers the front tire over 1.2 seconds, and completed wheelies count
toward the scene's trick score. Taking a ramp interrupts a wheelie smoothly.

**Settings → Animations → Warp tunnel / Curved tunnel** independently controls
warp speed (25–400%, default 150%) and ring density (3–16, default 8 / 7).
Speed changes preserve the travel phase, and ring spacing adjusts to the count.

As the bike approaches a ramp, the front wheel climbs its slope while the rear
tire stays grounded; takeoff carries the upward pitch smoothly into flight.
Outside Game mode, typing produces occasional random bike actions and all keys
retain normal host output.

Run host behavior tests with `test/host/run.sh`. They cover mapping, tap/hold,
control capture, Consumer HID descriptors/report retries, display recovery,
animation timing, and bike tricks under address/undefined-behavior sanitizers.
