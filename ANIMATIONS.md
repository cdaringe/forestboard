# OLED animations

Each compiled animation implements the small `Animation` interface in
`include/display/animations/animation.h`:

```cpp
class Animation {
public:
  virtual const char* name() const = 0;
  virtual void reset() = 0;
  virtual void render(Adafruit_SH1107& display, uint32_t now) = 0;
};
```

Animations live in `src/display/animations/`. To add one:

1. Add a source file containing an `Animation` implementation and a factory
   function that returns its static instance.
2. Declare that factory and add its result to `animations[]` in
   `src/display/animations/animation_manager.cpp`.

The first registry entry is the boot default. Drawing functions update the
Adafruit framebuffer; `DisplayController` owns frame pacing, the single
page-by-page framebuffer transfer, and automatic rotation to the next registered
animation every five minutes. Hold `Fn` and turn the rotary encoder to move
forward or backward through the registry on demand; manual selection restarts
the five-minute timer.

The milestone celebration is a temporary screen effect rather than a regular
animation, so it lives in `keystroke_milestone_effect.cpp` and overlays the
current animation without occupying a registry slot.

The current registry is:

| Index | Animation |
|---:|---|
| `0` | Hyperspace tunnel with square depth rings and star streaks |
| `1` | Bouncing triangle and pulsing circle |
| `2` | Rounded contour tunnel with a smoothly turning centerline |
| `3` | Keystroke comets launched by live typing |
| `4` | Forward flight over a moving wireframe terrain grid |
| `5` | Particles spiraling through an animated gravity well |
| `6` | A constellation that grows and reconnects as you type |
| `7` | Conway's Life seeded by incoming keystrokes |
| `8` | Typing weather that intensifies from rain into lightning |
| `9` | Mountain-bike track: ambient typing reactions or held-key Game mode |
| `10` | Drifting portal nodes with periodic tunnel transit |

The `Animation::onKeystroke()` hook is optional. The display controller sends
new press sequence numbers to the current animation in bounded batches, so
typing-reactive visuals do not add work to the latency-sensitive matrix scan.

For electrical and rendering isolation, the `*_oled_debug` PlatformIO
environments bypass this registry entirely and render only the deterministic
slow test scene in `oled_diagnostic_renderer.cpp`.

The refresh scheduling default is 60 FPS (settings range 20–120 FPS); the current software-SPI transport
does not guarantee that throughput. `AnimationClock` expresses motion in
fractions of the old 200 ms frame duration, preserving pace while permitting
intermediate positions. Life generations and energy decay use timed ticks.
Animations with typing effects still receive bounded `onKeystroke()` batches.
`onKeyPress(usage, isGameMode)` supplies logical HID usages for ambient typing
and stable bike action IDs for captured physical game controls;
`isInteractive()` identifies scenes whose game controls should be captured.

Mountain bike replaces Breakout at index 9. Normal typing has occasional random
effects. Enable **Settings → Keyboard → Game mode** and save for direct controls:
ramps spawn automatically at randomized intervals. Physical Colemak A/R/S/T
positions trigger backflip/cancan/360/wheelie; adjacent D adds a ramp. These are
QWERTY ASDFG positions and do not change when switching layouts. Arrow keys change
lanes. All seven bindings, rider speed, and average ramp interval are persisted
through **Settings → Animations → Mountain bike**. Binding editors capture the
physical key, validate it, and reject conflicting assignments on save. Wheelies
raise the front tire smoothly around a grounded rear axle and end after 1.2 s;
a ramp interrupts them while preserving the takeoff pitch. Disable Game mode in the menu to resume ambient riding.
Holding the layout key opens Settings; it never toggles Game mode directly.
A landed-trick counter rewards completed airborne tricks. Game mode pauses
animation auto-rotation. The bike pitches around the rear axle as its front
wheel climbs the ramp, then carries that pitch into takeoff and levels smoothly.

Hyperspace and curved tunnels have independent **Warp speed** and **Ring density**
settings under **Settings → Animations**. Defaults are 150% speed and 8/7 rings;
limits are 25–400% and 3–16 rings. Particle motion and ring travel use the speed
multiplier. Accumulated phases preserve continuity across changes and wrap cleanly.

Scenes render directly without a global shading or halo pass.

Shooting stars launch from varied positions along all four scene edges. Active
flights are never replaced by incoming keypresses. If all 18 flight slots are
occupied, extra launch requests are skipped until a slot becomes available.
