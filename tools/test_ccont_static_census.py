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
        self.assertEqual(
            [2, 3, 4, 7, 8, 9, 10, 11, 12, 13, 14, 15],
            payload["roms"][0]["mapped_registers"],
        )


if __name__ == "__main__":
    unittest.main()
