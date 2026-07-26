import hashlib
import unittest
from pathlib import Path

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
        self.assertEqual(
            0x10,
            (rom4["state"] + 8) - (rom3["state"] + 8),
        )
        self.assertEqual(0x1C28, rom4["formatter"] - rom3["formatter"])
        self.assertEqual(0x1C2C, rom4["acceptance"] - rom3["acceptance"])

    def test_result_contract_is_not_named_as_physical_cobba(self):
        source = Path(check.__file__).read_text(encoding="utf-8")
        self.assertIn('"first_required_value": 0x0B06', source)
        self.assertIn('"physical_cobba_revision_semantic_proven": False', source)
        self.assertNotIn('"role": "COBBA identification"', source)

    def test_pre_upload_role_keeps_value_and_timing_units_separate(self):
        source = Path(check.__file__).read_text(encoding="utf-8")
        self.assertIn('"diagnostic_label": "DSP ROM"', source)
        self.assertIn('"dsp_publication_value": "not_static"', source)
        self.assertIn('"retry_delay_raw": 10', source)
        self.assertNotIn('"retry_delay_ms"', source)

    def test_second_result_census_is_capture_only_not_a_guessed_value(self):
        source = Path(check.__file__).read_text(encoding="utf-8")
        for profile in check.VARIANTS.values():
            self.assertEqual(14, len(profile["state_literal_roots"]))
            self.assertEqual(
                len(profile["state_literal_roots"]),
                len(set(profile["state_literal_roots"])),
            )
        self.assertIn('"second_state_root_reads": []', source)
        self.assertIn('"second_pointer_escapes": []', source)
        self.assertIn('"second_mcu_access": "capture_write_only"', source)
        self.assertIn('"second_value": "unknown"', source)

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
