#include "config/configuration.h"
#include "display/animations/animation.h"
#include "display/animations/animation_clock.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#define private public
#if defined(TEST_CURVED_TUNNEL)
#include "../../src/display/animations/curved_contour_tunnel.cpp"
using Tunnel = CurvedContourTunnelAnimation;
constexpr Setting speedSetting = Setting::curvedSpeed;
constexpr Setting ringSetting = Setting::curvedRings;
constexpr float phaseSpeed = 4;
#else
#include "../../src/display/animations/hyperspace_tunnel.cpp"
using Tunnel = HyperspaceTunnelAnimation;
constexpr Setting speedSetting = Setting::warpSpeed;
constexpr Setting ringSetting = Setting::warpRings;
constexpr float phaseSpeed = 5;
#endif
#undef private

int main() {
  Configuration config;
  assert(config.get(speedSetting) == 150);
  Adafruit_SH1107 display;
  for (uint32_t speed : {25, 100, 150, 400}) {
    config.set(speedSetting, speed);
    applyConfiguration(config);
    Tunnel tunnel;
    tunnel.reset();
    tunnel.render(display, 0);
#if defined(TEST_CURVED_TUNNEL)
    tunnel.particles_[0].depth = 80;
#else
    tunnel.stars_[0] = {12, 0, 200};
#endif
    tunnel.render(display, 100);
    assert(
        fabsf(tunnel.ringPhase_ - phaseSpeed * 0.5f * speed / 100.0f) < 0.001f);
#if defined(TEST_CURVED_TUNNEL)
    assert(fabsf(tunnel.particles_[0].depth - (80 + 2.5f * speed / 100.0f)) <
        0.001f);
#else
    assert(fabsf(tunnel.stars_[0].z - (200 - 3.0f * speed / 100.0f)) < 0.001f);
#endif
    config.set(ringSetting, 16);
    applyConfiguration(config);
    for (uint32_t now = 120; now < 30000; now += 20) {
      tunnel.render(display, now);
      assert(tunnel.ringPhase_ >= 0 && tunnel.ringPhase_ < 256);
    }
    // Changing speed does not jump the phase back to an absolute-time value.
    const float phase = tunnel.ringPhase_;
    config.set(speedSetting, 100);
    applyConfiguration(config);
    tunnel.render(display, 30000);
    float expected = phase + phaseSpeed * 0.1f;
    if (expected >= 256) {
      expected -= 256;
    }
    assert(fabsf(tunnel.ringPhase_ - expected) < 0.001f);
  }
  Tunnel tunnel;
  tunnel.reset();
  tunnel.ringPhase_ = 128;
  unsigned sparse = 0;
  for (uint32_t count : {3, 16}) {
    config.set(ringSetting, count);
    applyConfiguration(config);
    display.ellipseCalls = display.rectCalls = 0;
#if defined(TEST_CURVED_TUNNEL)
    tunnel.drawContours(display);
    const unsigned calls = display.ellipseCalls;
    if (count == 3) {
      sparse = calls;
    } else {
      assert(calls > sparse);
    }
#else
    tunnel.drawTunnelRings(display, 64, 64);
    assert(display.rectCalls == count);
#endif
  }
  puts("Tunnel speed, density and phase tests passed");
}
