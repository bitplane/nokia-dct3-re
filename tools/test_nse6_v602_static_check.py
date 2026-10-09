import unittest
from pathlib import Path

from tools import nse6_v602_static_check as check
from tools.nse6_integrity_fixture import fixture


class Nse6StaticTests(unittest.TestCase):
    def test_thumb_bl_census_signed_offsets_and_scope(self):
        forward = bytes.fromhex("f000f802")
        self.assertEqual(check.thumb_bl_callers(forward, check.BASE + 8, 4),
                         (1, [check.BASE]))
        backward = bytes.fromhex("f7fffffc")
        self.assertEqual(check.thumb_bl_callers(backward, check.BASE - 4, 4),
                         (1, [check.BASE]))
        self.assertEqual(check.thumb_bl_callers(bytes(4), check.BASE, 4), (0, []))
        for extent in (0, 3, 6):
            with self.assertRaises(ValueError):
                check.thumb_bl_callers(forward, check.BASE, extent)

    def test_integrity_fixture_changes_only_checksum(self):
        image = fixture()
        self.assertEqual(len(image), 0x8000)
        self.assertEqual([i for i, byte in enumerate(image) if byte != 0xFF],
                         [0x11E, 0x11F])
        self.assertEqual(image[0x11E:0x120], bytes.fromhex("db24"))
        updated = bytearray(image)
        updated[0x74:0x76] = b"\0\0"
        self.assertEqual(check.integrity_arithmetic(updated[0x40:0x11E], 0),
                         int.from_bytes(image[0x11E:0x120], "big"))

    def test_config_integrity_fixture_preserves_identity(self):
        image = fixture(config_integrity=True)
        self.assertEqual([i for i, byte in enumerate(image) if byte != 0xFF],
                         [0x3C, 0x3D, 0x3E, 0x3F, 0x11E, 0x11F])
        self.assertEqual(int.from_bytes(image[0x3C:0x40], "big"),
                         sum(image[:0x3C]))
        self.assertEqual(image[:0x3C], b"\xff" * 0x3C)

    def test_integrity_arithmetic_and_allocation_paths(self):
        block = bytearray(b"\xff" * 0xDE)
        block[0x74 - 0x40:0x76 - 0x40] = b"\0\0"
        self.assertEqual(check.integrity_arithmetic(block, 0), 0xDB24)
        self.assertEqual(check.integrity_arithmetic(b"\xff" * 0xDE, 0xFFFF),
                         0xDB24)
        self.assertEqual(check.integrity_arithmetic(b"\xff" * 0x9E, 0xFFFF),
                         0x9B64)
        self.assertEqual(check.integrity_arithmetic(bytes(0xDE), 1), 0xFFFF)
        for block, word in ((bytes(2), 0), (bytes(0xDE), -1),
                            (bytes(0xDE), 0x10000)):
            with self.assertRaises(ValueError):
                check.integrity_arithmetic(block, word)

    def test_verifier_stream_stride_lane_and_terminators(self):
        image = bytearray(0x200000)
        image[0x40:0x44] = bytes.fromhex("12345678")
        image[0x60:0x62] = bytes.fromhex("9abc")
        image[-32:-30] = bytes.fromhex("def0")
        stream = check.verifier_stream(image)
        self.assertEqual(len(stream), 131072)
        self.assertEqual(stream[:4], bytes.fromhex("12349abc"))
        self.assertEqual(stream[-6:], bytes.fromhex("def0ffffffff"))
        with self.assertRaises(ValueError):
            check.verifier_stream(bytes(32))

    def test_eeprom_descriptor_own_fields(self):
        self.assertEqual(check.eeprom_descriptor(0xF6), {
            "capacity_bytes": 32768, "page_bytes": 32, "address_bytes": 2})
        self.assertEqual(check.eeprom_descriptor(0xB6)["address_bytes"], 1)
        with self.assertRaises(ValueError):
            check.eeprom_descriptor(0)

    def test_unknown_package_rejected(self):
        with self.assertRaisesRegex(ValueError, "package"):
            check.normalize(b"not firmware")

    def test_unknown_image_rejected(self):
        with self.assertRaisesRegex(ValueError, "image"):
            check.check(bytes(0x200000))

    def test_big_endian_word_and_bounds(self):
        self.assertEqual(check.read32(bytes.fromhex("12345678"), check.BASE),
                         0x12345678)
        for address in (check.BASE - 1, check.BASE + 1):
            with self.assertRaises(ValueError):
                check.read32(bytes(4), address)

    def test_acquired_package_when_available(self):
        root = Path(__file__).resolve().parents[1]
        path = root / "roms/archive-dct3-packages/nse6_602.exe"
        if not path.exists():
            self.skipTest("acquired NSE-6 package absent")
        image = check.normalize(path.read_bytes())
        result = check.check(image)
        self.assertEqual(result["reset_literals"]["0x200068"], "0x125f30")
        self.assertFalse(result["runtime_acceptance"])
        self.assertEqual(result["acquisition"]["source7_selector"], 2)
        self.assertEqual(result["acquisition"]["source8_cache"], "0x12158c")
        self.assertEqual(result["acquisition"]["cache_writer"], "0x2e09d6")
        self.assertEqual(result["acquisition"]["cache_selectors"], [2, 1])
        self.assertFalse(result["acquisition"]["physical_units_proven"])
        self.assertEqual(result["eeprom_tx"]["scl_bit"], 2)
        self.assertFalse(result["dsp_verifier"]["resident_mask_proven"])
        self.assertEqual(result["dsp_verifier"]["full_blocks"], 127)
        self.assertEqual(result["dsp_verifier"]["stream_sha1"],
                         check.VERIFIER_STREAM_SHA1)
        self.assertEqual(result["dsp_verifier"]["result_registers"],
                         ["0x10000", "0x10002"])
        self.assertEqual(result["eeprom_descriptor"]["page_bytes"], 32)
        self.assertEqual(result["keypad"]["power_column_mask"], 16)
        self.assertFalse(result["keypad"]["runtime_input_proven"])
        self.assertEqual((result["display"]["width"], result["display"]["height"]),
                         (84, 48))
        self.assertFalse(result["display"]["runtime_frame_proven"])
        self.assertEqual(result["gensio"]["ccont_ready_bit"], 2)
        self.assertEqual(len(result["input_controller"]["targets"]), 17)
        self.assertEqual(result["input_controller"]["targets"][16], "0x288c3a")
        self.assertEqual(result["simi"]["initial_control"], "0x32")
        self.assertFalse(result["simi"]["runtime_card_exchange_proven"])
        mutated = bytearray(image)
        mutated[0x168] ^= 1
        with self.assertRaises(ValueError):
            check.check(mutated)
