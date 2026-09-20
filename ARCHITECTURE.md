# Firmware modules

`src/main.cpp` composes the keyboard and display. Headers in `include/` mirror
source categories, so includes identify the owning module explicitly.

```text
src/
  main.cpp
  keyboard/
    input_controller.cpp
  usb/
    hid_device.cpp
    usb_pinmap.cpp
  settings/
    settings_menu.cpp
  storage/
    keystroke_counter.cpp
  display/
    display_controller.cpp
    oled_transport.cpp
    animations/
      animation_manager.cpp
      mountain_bike.cpp
      ...
    diagnostics/
      oled_diagnostic_renderer.cpp
    effects/
      keystroke_milestone_effect.cpp
    widgets/
      key_capture_overlay.cpp
      keystroke_count_format.cpp
      status_bar.cpp
```

- **Keyboard** owns matrix scanning, debounce, physical mapping, Colemak/QWERTY
  translation, encoder input, and local gestures. It queues animation input and
  requests; it does not draw. Queued presses retain their original Game mode.
- **USB** adapts STM32duino's keyboard and Consumer Control interfaces, maintains
  report storage through transfer completion, and receives host LED state.
- **Configuration** is generated from `include/config/settings.def`: one typed
  model, defaults, limits, labels, groups, editor kinds, and storage ordering. All runtime
  consumers read `configuration()`; hardware build options stay separate.
- **Settings** owns the hierarchical menu and its RAM-only draft. It requests
  saves/resets; the main loop services those requests outside display transfers.
- **Bike controls** resolve configurable physical switch usages to stable action
  IDs before layout translation. Key-binding editors capture physical keys; only
  configured controls are suppressed from the host while playing.
- **Storage** owns the shared two-sector journal for configuration and keystrokes,
  versioned 64-byte snapshots (v4 packs settings in 16-bit slots), verified commit protocol, and 10,000-key automatic
  checkpoints. Explicit saves and confirmed resets are additional transactions.
  Milestone display events and menu edits do not write flash.
- **Display** owns frame scheduling, recovery, scene selection, and composition.
  Input activity resets its configurable idle timer; sleeping stops panel traffic
  until a key or encoder event wakes it. Keyboard scanning continues during sleep.
  `oled_transport` owns panel pins, SPI, controller commands, and RAM transfers.
- **Animations** draw scenes into the shared framebuffer. **Effects** temporarily
  decorate scenes. **Widgets** draw readable state and capture text.
  **Diagnostics** provide a deterministic scene for isolating display faults.

Hardware build options live in `include/config/firmware_config.h`. `DisplayStatus`
is the display's snapshot of keyboard state; the controller does not depend on
`InputController`. Main supplies a callback that services keyboard input
between OLED pages. Animation code does not send HID reports or transfer pixels
to the panel directly.

Use small named operations and `is*` names for boolean state and predicates.
Operations that consume events or attempt I/O use action verbs and return a
success flag. Keep hardware-specific behavior in its owning module.

## Formatting and verification

The repository's `.clang-format` defines two-space indentation, attached braces,
80-column wrapping, and consistent pointer/reference spacing. Format or check
project C++ (including the host fixtures) with:

```sh
rg --files src include test/host -g '*.cpp' -g '*.h' | xargs clang-format -i
rg --files src include test/host -g '*.cpp' -g '*.h' | xargs clang-format --dry-run --Werror
```

Run `test/host/run.sh` for sanitized host checks. Build both boards and diagnostic
variants with:

```sh
pio run -e genericSTM32F411RE -e genericSTM32F446RE -e genericSTM32F411RE_oled_debug -e genericSTM32F446RE_oled_debug
```

See [KEYBOARD.md](KEYBOARD.md) for controls, display timing, and hardware checks;
see [ANIMATIONS.md](ANIMATIONS.md) for the scene registry and extension interface.
