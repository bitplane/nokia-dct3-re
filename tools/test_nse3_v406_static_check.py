import struct
import unittest

from tools import nse3_v406_static_check as check


class Nse3V406StaticCheckTests(unittest.TestCase):
    def fixture(self):
        image = bytearray(0x180)
        words = {
            0x40: "e3a01602",
            0x44: "e5910000",
            0x58: "e3a01701",
            0x5C: "e5810000",
            0xB8: "e59f00a8",
            0xBC: "e3a01000",
            0xC0: "e89003fc",
            0xC4: "e88103fc",
            0xE4: "e28f0001",
            0xE8: "e12fff10",
        }
        for offset, word in words.items():
            image[offset : offset + 4] = bytes.fromhex(word)
        for offset, value in {
            0x168: check.VECTOR_SOURCE,
            0x16C: 0x10F9D8,
            0x170: 0x10F9D8,
            0x174: 0x100020,
            0x178: 0x10C508,
        }.items():
            struct.pack_into(">I", image, offset, value)
        return image

    def test_accepts_reset_vector_copy_and_sram_stacks(self):
        result = check.verify_reset_boundary(bytes(self.fixture()))
        self.assertEqual(0, result["vector_destination"])
        self.assertEqual(check.VECTOR_SOURCE, result["literals"]["vector_source"])

    def test_rejects_stack_outside_physical_sram(self):
        image = self.fixture()
        struct.pack_into(">I", image, 0x178, check.SRAM_BASE + check.SRAM_SIZE)
        with self.assertRaisesRegex(ValueError, "outside 64 KiB"):
            check.verify_reset_boundary(bytes(image))

    def test_swap16_is_involutive(self):
        value = bytes.fromhex("12345678")
        self.assertEqual(value, check.swap16(check.swap16(value)))

    def test_identity_rejects_unidentified_image(self):
        with self.assertRaisesRegex(ValueError, "expected 0x100000-byte"):
            check.verify_identity(b"\xff" * 32)


if __name__ == "__main__":
    unittest.main()
