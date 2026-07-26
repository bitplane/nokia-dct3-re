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

    def test_keypad_boundary_distinguishes_side_keys(self):
        image = bytearray(b"\xff" * check.FLASH_SIZE)
        offset = check.KEYMAP_ADDRESS - check.FLASH_BASE
        special_offset = check.SPECIAL_KEYMAP_ADDRESS - check.FLASH_BASE
        image[offset : offset + len(check.KEYMAP)] = check.KEYMAP
        image[special_offset : special_offset + len(check.SPECIAL_KEYMAP)] = (
            check.SPECIAL_KEYMAP
        )
        result = check.verify_keypad_boundary(bytes(image))
        self.assertEqual(0x11, result["volume_down"]["keycode"])
        self.assertEqual(0x10, result["volume_up"]["keycode"])

    def test_keypad_boundary_rejects_reversed_volume_key(self):
        image = bytearray(b"\xff" * check.FLASH_SIZE)
        offset = check.KEYMAP_ADDRESS - check.FLASH_BASE
        image[offset : offset + len(check.KEYMAP)] = check.KEYMAP
        image[offset + 5] = 0x10
        with self.assertRaisesRegex(ValueError, "normal 5x5 keypad"):
            check.verify_keypad_boundary(bytes(image))

    def test_eeprom_boundary_recovers_sda_scl_and_address_width(self):
        image = bytearray(b"\xff" * check.FLASH_SIZE)
        # swap16 Thumb encodings at the identified routine anchors.
        encodings = {
            0x29CDD2: "380a",
            0x29CDE0: "3806",
            0x29CDE2: "000e",
            0x29E8C4: "8024",
            0x29E8C8: "2033",
            0x29E8CA: "0120",
            0x29E8CC: "0422",
            0x29E8D4: "1979",
            0x29E90E: "1543",
            0x29E936: "9543",
        }
        for pc, encoded in encodings.items():
            physical = bytes.fromhex(encoded)
            offset = pc - check.FLASH_BASE
            image[offset : offset + 2] = physical[::-1]
        result = check.verify_eeprom_boundary(bytes(image))
        self.assertEqual(0, result["sda_bit"])
        self.assertEqual(2, result["scl_bit"])
        self.assertEqual(2, result["word_address_bytes"])


if __name__ == "__main__":
    unittest.main()
