#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
output=$(mktemp -d)
trap 'rm -rf "$output"' EXIT
sources=(
  src/display/display_controller.cpp
  src/display/oled_transport.cpp
  src/display/effects/oled_shading.cpp
  src/display/widgets/status_bar.cpp
  src/display/widgets/key_capture_overlay.cpp
  src/display/widgets/keystroke_count_format.cpp
  src/display/effects/keystroke_milestone_effect.cpp
  src/display/diagnostics/oled_diagnostic_renderer.cpp
)
for source in src/display/animations/*.cpp; do
  case "$source" in
    */mountain_bike.cpp | */keystroke_comets.cpp) ;;
    *) sources+=("$source") ;;
  esac
done
compiler_flags=(
  -std=c++17 -g -O1
  -fsanitize=address,undefined -fno-omit-frame-pointer
  -DERGOBOARD_KEYBOARD_MODE
  -Itest/host/support -Iinclude
)
g++ "${compiler_flags[@]}" test/host/test_storage.cpp -o "$output/storage-tests"
"$output/storage-tests"
g++ "${compiler_flags[@]}" test/host/test_firmware.cpp "${sources[@]}" \
  -o "$output/firmware-tests"
"$output/firmware-tests"
g++ "${compiler_flags[@]}" -Itest/host/inc test/host/test_usb.cpp \
  -o "$output/usb-tests"
"$output/usb-tests"
