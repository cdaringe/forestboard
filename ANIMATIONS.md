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

The production refresh target is 125 FPS. `AnimationClock` expresses motion in
fractions of the old 200 ms frame duration, preserving pace while permitting
intermediate positions. Life generations and energy decay use timed ticks.
Animations with typing effects still receive bounded `onKeystroke()` batches.
`onKeyPress(usage, isGameMode)` additionally supplies logical HID key usages;
`isInteractive()` identifies scenes whose game controls should be captured.

Mountain bike replaces Breakout at index 9. Normal typing has occasional random
effects. Hold `Layout/Game` for the `GAME` badge and direct controls: arrows
change lanes, `J` adds a jump, and `B`, `C`, `T` trigger a backflip, cancan or
360 in the air. Release the mode key to resume ambient riding. A landed-trick
counter rewards completed airborne tricks. Game mode pauses auto-rotation.

A shared shading pass preserves bright outlines, adds sparse dithered halos,
and textures solid interiors. It uses a fixed spatial pattern on the one-bit
panel, not temporal grayscale, to avoid flicker. Status badges and capture
text are painted afterward and stay crisp. Set `ERGOBOARD_OLED_SHADING=0` to
compare with the original rendering; the diagnostic scene bypasses shading.

Shooting stars launch from varied positions along all four scene edges. Active
flights are never replaced by incoming keypresses. If all 18 flight slots are
occupied, extra launch requests are skipped until a slot becomes available.
