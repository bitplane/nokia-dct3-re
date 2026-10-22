import pathlib
import unittest

from tools import ccont_static_census


ROOT = pathlib.Path(__file__).resolve().parents[1]


class CcontStaticCensusTest(unittest.TestCase):
    def test_byte_lane_and_descriptor_decode(self):
        logical = bytearray([0xff] * 0x80)
        logical[0x20:0x20 + len(ccont_static_census.DESCRIPTOR_SIGNATURE)] = (
            ccont_static_census.DESCRIPTOR_SIGNATURE
        )
        physical = bytearray(logical)
        physical[0::2], physical[1::2] = logical[1::2], logical[0::2]
        address = ccont_static_census.locate_descriptor_table(bytes(physical))
        self.assertEqual(ccont_static_census.FLASH_BASE + 0x20, address)
        entries = ccont_static_census.table_entries(bytes(physical), address)
        self.assertEqual(0x10, entries[2])
        self.assertEqual(2, ccont_static_census.physical_register(entries[2]))
        self.assertIsNone(ccont_static_census.physical_register(entries[0]))

    def test_supported_rom_contract(self):
        payload = ccont_static_census.build_payload(ccont_static_census.DEFAULT_ROMS)
        self.assertTrue(payload["cross_rom_descriptor_identity"])
        self.assertEqual(5, payload["coverage"]["roms_decoded"])
        for rom in payload["roms"]:
            descriptors = rom["descriptors"]
            self.assertEqual("0xff", descriptors[8]["command"])
            self.assertEqual("0x38", descriptors[8]["write_command"])
            self.assertEqual(7, descriptors[8]["write_register"])
            self.assertEqual("0x38", descriptors[9]["command"])
            self.assertIsNone(descriptors[9]["write_command"])
            self.assertEqual("0x50", descriptors[12]["command"])
            self.assertEqual("0x50", descriptors[12]["write_command"])
        self.assertEqual(
            [2, 3, 4, 7, 8, 9, 10, 11, 12, 13, 14, 15],
            payload["roms"][0]["mapped_registers"],
        )

    def test_nsb7_independent_read_write_alias(self):
        report = ccont_static_census.analyze(
            "8290-v5.22", ROOT / "roms/noki8290/8290f522a.fls"
        )
        self.assertEqual("0x00343ee0", report["descriptor_table"])
        self.assertEqual("0x38", report["descriptors"][8]["write_command"])
        self.assertIsNone(report["descriptors"][8]["register"])
        self.assertEqual(7, report["descriptors"][9]["register"])
        self.assertIsNone(report["descriptors"][9]["write_command"])


if __name__ == "__main__":
    unittest.main()
