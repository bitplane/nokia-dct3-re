import unittest

from make_5110_eeprom_profile import (
    DSP_FAULT_LATCH,
    FACTORY_KEY_FLASH_OFFSET,
    FAID_RECORD_704,
    FAID_RECORD_705,
    FAID_RECORD_706,
    FAID_RECORD_707,
    RECORD_CHECKSUM_OFFSET,
    SECURITY_LEVEL,
    TUNE_CHECKSUM_OFFSET,
    build_profile,
    decode_msid,
)


class Make5110EepromProfileTest(unittest.TestCase):
    def test_measured_msid_identity(self):
        self.assertEqual(
            (bytes.fromhex("054b7d89"), bytes.fromhex("00160010"), bytes.fromhex("a8a9aa60")),
            decode_msid(bytes.fromhex("8264b000eb8f457e168bd2d32a")))

    def test_factory_records_and_checksums(self):
        source = bytearray([0xFF]) * 0x800
        source[0x00C:0x014] = bytes.fromhex("49054410901987ff")
        flash = bytearray(FACTORY_KEY_FLASH_OFFSET + 8)
        flash[FACTORY_KEY_FLASH_OFFSET:] = bytes.fromhex("5d091658050c1d18")

        image = build_profile(source, flash)
        self.assertEqual(bytes.fromhex("e37069457c7457265d647bc9"), image[0x00:0x0C])
        self.assertEqual(bytes.fromhex("2fcd98412b9f39e55125afba"), image[0x20:0x2C])
        self.assertEqual(bytes.fromhex("a5e2e92b944a3855510ebb22"), image[0x2C:0x38])
        expected = bytes.fromhex("2d3c6371ad22abe7")
        self.assertEqual(expected, image[FAID_RECORD_704:FAID_RECORD_704 + 8])
        self.assertEqual(expected, image[FAID_RECORD_706:FAID_RECORD_706 + 8])
        self.assertEqual(1, image[FAID_RECORD_705])
        self.assertEqual(1, image[FAID_RECORD_707])
        self.assertEqual(0, image[DSP_FAULT_LATCH])
        self.assertEqual(0xFF, image[SECURITY_LEVEL])
        self.assertEqual(sum(image[:RECORD_CHECKSUM_OFFSET]) & 0xFFFF,
                         int.from_bytes(image[RECORD_CHECKSUM_OFFSET:RECORD_CHECKSUM_OFFSET + 2], "big"))
        tune = (sum(image[0x040:0x11E]) - image[0x074] - image[0x075]) & 0xFFFF
        self.assertEqual(tune, int.from_bytes(image[TUNE_CHECKSUM_OFFSET:TUNE_CHECKSUM_OFFSET + 2], "big"))


if __name__ == "__main__":
    unittest.main()
