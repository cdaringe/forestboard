# Draw from the computer

Build and flash the production firmware (`make build`, then the normal ROM DFU
upload procedure in KEYBOARD.md). Reconnect USB so the host reads the updated
HID descriptor. Diagnostic builds reject display packets.

Install the sender dependencies in a virtual environment:

```sh
python3 -m venv .venv-display
.venv-display/bin/pip install -r scripts/display-requirements.txt
.venv-display/bin/python scripts/forestboard-display --list
.venv-display/bin/python scripts/forestboard-display image.png
```

The sender fits the image inside 128 × 128 with black margins, composites
transparency onto black, and dithers to one bit per pixel. Use `--no-dither` for a
threshold conversion or `--invert` to reverse black and white. It refreshes the
image every two seconds; Ctrl-C releases the screen. `--once` sends a single
frame, which expires after five seconds. Animated image files currently use their
first frame. If multiple boards are attached, select the displayed `--path`.

The boot splash finishes first; settings take priority over host images. Host
images occupy the full screen without the normal status bar. Successfully
presenting a frame wakes the screen and resets its configured idle timer. The
normal idle timeout still applies between updates. Five seconds without a new
complete frame restores local scene selection; nothing is written to flash.

Linux needs permission to open the selected HID device. For HIDAPI's hidraw
backend, a local udev rule can grant the active desktop user access:

```udev
SUBSYSTEM=="hidraw", ATTRS{product}=="forestboard", TAG+="uaccess"
```

For a libusb backend, the equivalent device rule is:

```udev
SUBSYSTEM=="usb", ENV{DEVTYPE}=="usb_device", ATTR{product}=="forestboard", TAG+="uaccess"
```

Install the applicable rule as `/etc/udev/rules.d/70-forestboard.rules`, reload
udev rules, and reconnect. Desktop access policy and backend selection vary by
system. `--list` can succeed even if opening the device is denied.

## Protocol v1

Interface 0 retains its one-byte Consumer Control input report and gains a
vendor usage page `0xFF00`, usage 1 collection with an **unnumbered 64-byte Feature
report**. The keyboard boot interface and LED report are unchanged. Use one
sender at a time. HIDAPI requires a synthetic zero report-ID byte before these
64 payload bytes when sending; it is not part of the wire payload.

| Bytes | Meaning |
| --- | --- |
| 0 | Version: 1 |
| 1 | Opcode: 1 begin, 2 data, 3 present, 4 release |
| 2–3 | Frame ID, little endian |
| 4–5 | Byte offset, little endian |
| 6 | Data length, 1–56 for data; zero for other commands |
| 7 | Reserved: zero |
| 8–63 | Data, zero-padded by sender |

Begin starts a 2,048-byte staging frame. Data must use the same ID and strictly
consecutive offsets starting at zero. Present requires all 2,048 bytes and the
same ID. Begin/present/release require offset zero. Pixels are row-major,
16 bytes per row, most significant bit first, one means lit. An incomplete or
invalid frame never replaces the last complete frame. Release cancels staging
and returns to local display content.

Read Feature report zero to query status (64 bytes): `F`, `B`, version 1, busy
(0/1), last result (0 accepted, 1 rejected), then reserved zeros. Poll until busy
clears after **every** write and check the result. A second write while busy is
rejected at USB setup. The mailbox uses atomic publication between USB callbacks
and the main loop; only main-loop code validates and assembles frames. The
OLED transfer callback also processes one pending packet between 32-byte panel
bursts, after servicing keyboard input. Publishing a host frame changes a
separate buffer, leaving an in-progress panel transfer untouched. Status
acknowledges processing, not that the physical panel has displayed the pixels.

Measure the accepted-frame rate with a moving test pattern:

```sh
.venv-display/bin/python scripts/forestboard-display --benchmark 10
```

The test runs for approximately the requested number of seconds, finishing its
last frame before reporting transfer latency and throughput. It releases the
screen afterward. This measures firmware acceptance, not physical panel refresh.
The initial whole-refresh mailbox scheduling measured about 0.75 fps on hardware
(roughly 3 ms writing each packet and 30 ms awaiting processing). After processing packets between panel bursts, the same connected keyboard
accepted 45 frames in 10.17 seconds: **4.42 fps**, with 224.8 ms mean transfer
latency and 229.8 ms maximum. That is approximately six times the original
throughput. These are host-to-firmware measurements on this setup, not a
guarantee of physical panel refresh rate on every host.

A frame takes 39 write/status exchanges. This initial transport favors bounded
work and explicit backpressure. Streaming throughput and typing latency need
measurement on hardware before choosing an animation frame rate. There is no
compression, partial-frame update, or interrupt OUT endpoint yet.

Run firmware checks with `make test`; test the host sender with:

```sh
.venv-display/bin/python test/test_display_sender.py
```

Host calls follow the [HIDAPI feature report API](https://libusb.info/hidapi/group__API.html).
