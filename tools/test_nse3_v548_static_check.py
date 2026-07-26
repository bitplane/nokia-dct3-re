import hashlib
import unittest

from tools import nse3_v548_static_check as check


class Nse3V548StaticCheckTests(unittest.TestCase):
    def test_bootstrap_stream_preserves_stride_and_terminators(self):
        image = bytearray(check.FLASH_SIZE)
        for index in range(check.STREAM_WORDS):
            offset = 0x40 + index * 0x20
            image[offset : offset + 2] = index.to_bytes(2, "big")
        stream = check.extract_bootstrap_stream(bytes(image))
        self.assertEqual(0x10000, len(stream))
        self.assertEqual(b"\x00\x00\x00\x01", stream[:4])
        self.assertEqual(b"\xff\xff\xff\xff", stream[-4:])

    def test_manifest_variants_remain_distinct(self):
        rom3 = check.VARIANTS["rom3"]
        rom4 = check.VARIANTS["rom4"]
        self.assertNotEqual(rom3["sha1"], rom4["sha1"])
        self.assertNotEqual(rom3["stream_sha1"], rom4["stream_sha1"])
        self.assertEqual(0x1C1C, rom4["loader"] - rom3["loader"])
        self.assertEqual(0x10, rom4["state"] - rom3["state"])

    def test_identity_rejects_short_image(self):
        with self.assertRaisesRegex(ValueError, "expected 0x100000 bytes"):
            check.verify_variant(
                type("ShortPath", (), {"read_bytes": lambda self: b"\xff" * 32})(),
                "rom3",
            )

    def test_recorded_stream_hashes_are_sha1(self):
        for profile in check.VARIANTS.values():
            self.assertEqual(20, len(bytes.fromhex(profile["stream_sha1"])))
            hashlib.sha1(bytes.fromhex(profile["stream_sha1"])).digest()


if __name__ == "__main__":
    unittest.main()
