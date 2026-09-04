import pathlib
import tempfile
import unittest

from tools.dsp_rom_audit import (
    audit, classify, covered_words, descriptor_locations,
    descriptor_signature, parse_block_map,
)


class DspRomAuditTest(unittest.TestCase):
    def test_fill_regions_are_not_reported_as_firmware(self):
        self.assertEqual(classify(bytes([0xff]) * 32), "placeholder-fill-ff")
        self.assertEqual(classify(bytes([0x00]) * 32), "placeholder-fill-00")

    def test_nonuniform_region_is_distinguished(self):
        self.assertEqual(classify(bytes([0xff, 0xfe])), "nonuniform-data")

    def test_audit_reports_size_and_digest(self):
        with tempfile.TemporaryDirectory() as directory:
            path = pathlib.Path(directory) / "dsp_prom"
            path.write_bytes(b"\xff" * 7)
            kind, size, digest = audit(path)
        self.assertEqual((kind, size), ("placeholder-fill-ff", 7))
        self.assertEqual(len(digest), 64)

    def test_recovered_block_map_parser_preserves_unknown_fields(self):
        blocks = parse_block_map(
            " 1: DAT_294540 25B4 1F80 034E 0130 044C 0000\n"
            "18: DAT_2980F4 0D80 1000 0122 0580 01F4 0000 ; loader2\n"
        )
        self.assertEqual(len(blocks), 2)
        self.assertEqual(blocks[0].index, 1)
        self.assertEqual(blocks[0].source, 0x294540)
        self.assertEqual(blocks[0].destination, 0x25B4)
        self.assertEqual(blocks[0].end, 0x2902)
        self.assertEqual(blocks[0].auxiliary, 0x1F80)
        self.assertEqual(blocks[0].staging, 0x0130)
        self.assertEqual(blocks[0].chunk_length, 0x044C)

    def test_coverage_is_clipped_and_deduplicated(self):
        blocks = parse_block_map(
            " 1: DAT_100000 1FF0 0000 0030 0000 0000 0000\n"
            " 2: DAT_100100 2010 0000 0020 0000 0000 0000\n"
            " 3: DAT_100200 27F0 0000 0030 0000 0000 0000\n"
        )
        self.assertEqual(covered_words(blocks, 0x2000, 0x2800), 0x40)

    def test_descriptor_signature_and_locations_use_raw_big_endian_words(self):
        block = parse_block_map(
            " 1: DAT_294540 25B4 1F80 034E 0130 044C 0000\n"
        )[0]
        signature = bytes.fromhex("25b41f80034e0130044c0000")
        self.assertEqual(descriptor_signature(block), signature)
        self.assertEqual(
            descriptor_locations(b"xx" + signature + b"yy" + signature, block, 0x200000),
            [0x200002, 0x200010],
        )


if __name__ == "__main__":
    unittest.main()
