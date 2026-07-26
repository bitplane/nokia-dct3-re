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

    def test_simi_boundary_keeps_card_profile_unassigned(self):
        image = bytearray(b"\xff" * check.FLASH_SIZE)
        encodings = {
            0x290510: "2020", 0x290512: "617b",
            0x29051C: "3d21", 0x29051E: "1820",
            0x29052A: "3e21", 0x29052C: "1a20",
            0x290538: "3920", 0x29053A: "3221",
            0x2900FA: "8020", 0x2901B0: "4b71",
            0x2901B4: "0b72", 0x2901C2: "0b70",
            0x2901D0: "0d72", 0x2903DE: "6879",
            0x2903E4: "2878", 0x290462: "0878",
            0x29046A: "0870",
        }
        for pc, encoded in encodings.items():
            physical = bytes.fromhex(encoded)
            offset = pc - check.FLASH_BASE
            image[offset : offset + 2] = physical[::-1]
        result = check.verify_simi_boundary(bytes(image))
        self.assertEqual(0x20036, result["tx_data"])
        self.assertEqual(0x20037, result["rx_data"])
        self.assertEqual("not_established", result["synthetic_card_profile"])

    def test_dspif_boundary_recovers_ring_geometry_without_reply_semantics(self):
        physical = bytearray(b"\xff" * check.FLASH_SIZE)
        encodings = {
            0x28564C: "ee49", 0x28564E: "0420", 0x285650: "0880",
            0x2856D2: "d34b", 0x2856D4: "a422",
            0x2856E6: "8d42", 0x2856EA: "1d1c", 0x28572A: "1080",
            0x28577A: "f048", 0x28577E: "f048", 0x28578C: "6430",
            0x2857C8: "de4c", 0x2857F6: "3152",
            0x2858A0: "a948", 0x2858A2: "0288", 0x2858A4: "4188",
            0x285A42: "4148", 0x285A44: "0180",
            0x285A46: "3e4b", 0x285A48: "8022",
            0x285A4A: "1a80", 0x285A4C: "5a80", 0x285A4E: "4180",
        }
        for pc, encoded in encodings.items():
            offset = pc - check.FLASH_BASE
            physical[offset : offset + 2] = bytes.fromhex(encoded)
        pools = {
            0x285A08: 0x30000,
            0x285A20: 0x10000,
            0x285B3C: 0x101CA,
            0x285B40: 0x101C8,
            0x285B44: 0x10100,
            0x285B48: 0x100A4,
        }
        for address, value in pools.items():
            raw = ((value << 16) | (value >> 16)) & 0xFFFFFFFF
            struct.pack_into("<I", physical, address - check.FLASH_BASE, raw)
        result = check.verify_dspif_boundary(check.swap16(bytes(physical)))
        self.assertEqual(0x0A4, result["mcu_to_dsp"]["producer_byte"])
        self.assertEqual(0x1CA, result["dsp_to_mcu"]["consumer_byte"])
        self.assertEqual("missing", result["internal_dsp_image"])
        self.assertEqual(
            "not_established", result["bootstrap_reply_semantics"]
        )

    def test_dspif_boundary_rejects_inherited_cursor_address(self):
        physical = bytearray(b"\xff" * check.FLASH_SIZE)
        # A syntactically valid LDR at the first literal anchor must not be
        # enough when its effective pointer is from another product.
        offset = 0x28564C - check.FLASH_BASE
        physical[offset : offset + 2] = bytes.fromhex("ee49")
        wrong = 0x101EC
        raw = ((wrong << 16) | (wrong >> 16)) & 0xFFFFFFFF
        struct.pack_into("<I", physical, 0x285A08 - check.FLASH_BASE, raw)
        with self.assertRaisesRegex(ValueError, "Thumb literal"):
            check.verify_thumb_literals(
                check.swap16(bytes(physical)), {0x28564C: 0x30000}
            )


if __name__ == "__main__":
    unittest.main()
