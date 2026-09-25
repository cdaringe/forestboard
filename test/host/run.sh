#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
output=$(mktemp -d)
trap 'rm -rf "$output"' EXIT
sources=(
  src/settings/settings_menu.cpp
  src/display/display_controller.cpp
  src/display/oled_transport.cpp
  src/display/widgets/boot_splash.cpp
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
  -DFORESTBOARD_KEYBOARD_MODE
  -Itest/host/support -Iinclude
)
g++ "${compiler_flags[@]}" test/host/test_storage.cpp -o "$output/storage-tests"
"$output/storage-tests"
# Controller failure injection uses the mock SPI device; production software
# SPI is exercised separately at GPIO/clock level below.
g++ "${compiler_flags[@]}" -DFORESTBOARD_OLED_SOFTWARE_SPI=0 \
  test/host/test_firmware.cpp "${sources[@]}" \
  -o "$output/firmware-tests"
"$output/firmware-tests"
g++ "${compiler_flags[@]}" test/host/test_oled_transport.cpp \
  src/display/oled_transport.cpp -o "$output/oled-tests"
"$output/oled-tests"
g++ "${compiler_flags[@]}" -Itest/host/inc test/host/test_usb.cpp \
  -o "$output/usb-tests"
"$output/usb-tests"

g++ "${compiler_flags[@]}" test/host/test_tunnels.cpp -o "$output/warp-tests"
"$output/warp-tests"
g++ "${compiler_flags[@]}" -DTEST_CURVED_TUNNEL test/host/test_tunnels.cpp -o "$output/curved-tests"
"$output/curved-tests"
