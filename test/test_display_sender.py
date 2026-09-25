"""Host protocol and image packing checks; no attached keyboard needed."""
import importlib.machinery
import importlib.util
from pathlib import Path
import tempfile
import unittest

loader = importlib.machinery.SourceFileLoader("sender", str(Path(__file__).resolve().parents[1] / "scripts/forestboard-display"))
spec = importlib.util.spec_from_loader(loader.name, loader)
sender = importlib.util.module_from_spec(spec)
loader.exec_module(sender)


class Device:
    def __init__(self, prefix=b""):
        self.packets = []
        self.prefix = prefix
        self.busy = False

    def send_feature_report(self, data):
        self.packets.append(data)
        self.busy = True
        return len(data)

    def get_feature_report(self, report_id, size):
        busy = self.busy
        self.busy = False
        return self.prefix + b"FB\x01" + bytes([busy, 0]) + bytes(59)


class SenderTests(unittest.TestCase):
    def test_frame_roundtrip(self):
        pixels = bytes(range(256)) * 8
        for prefix in (b"", b"\0"):
            device = Device(prefix)
            sender.send_frame(device, pixels, 257)
            self.assertEqual(len(device.packets), 39)
            self.assertTrue(all(len(p) == 65 and p[0] == 0 for p in device.packets))
            self.assertEqual(device.packets[0][1:9], b"\x01\x01\x01\x01\0\0\0\0")
            self.assertEqual(device.packets[-1][2], 3)
            rebuilt = b""
            for p in device.packets[1:-1]:
                self.assertEqual(int.from_bytes(p[5:7], "little"), len(rebuilt))
                rebuilt += p[9:9 + p[7]]
            self.assertEqual(rebuilt, pixels)

    def test_wrong_firmware(self):
        device = Device(b"bad")
        with self.assertRaises(RuntimeError):
            sender.status(device)

    def test_image_orientation(self):
        from PIL import Image
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "pixels.png"
            image = Image.new("1", (128, 128))
            image.putpixel((0, 0), 1)
            image.putpixel((1, 1), 1)
            image.putpixel((127, 127), 1)
            image.save(path)
            pixels = sender.convert_image(path)
            self.assertEqual(len(pixels), 2048)
            self.assertEqual(pixels[0], 0x80)
            self.assertEqual(pixels[16], 0x40)
            self.assertEqual(pixels[-1], 1)
            self.assertEqual(sum(p.bit_count() for p in pixels), 3)


if __name__ == "__main__":
    unittest.main()
