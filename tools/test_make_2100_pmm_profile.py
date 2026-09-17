import unittest

from tools.make_2100_pmm_profile import (
    IDENTITY_CHECKSUM_OFFSET,
    LOGICAL_DATA_OFFSET,
    VERSION_REFERENCE_OFFSET,
    make_profile,
)


class Make2100PmmProfileTest(unittest.TestCase):
    def test_preserves_reference_and_updates_identity_checksum(self):
        image = bytearray(b"\xff" * 0x400)
        image[6:12] = b"EEPROM"
        logical = LOGICAL_DATA_OFFSET
        image[logical:logical + IDENTITY_CHECKSUM_OFFSET] = bytes(
            index & 0xff for index in range(IDENTITY_CHECKSUM_OFFSET))
        image[logical + VERSION_REFERENCE_OFFSET:logical + VERSION_REFERENCE_OFFSET + 2] = (
            bytes.fromhex("933d"))

        result = make_profile(bytes(image))

        self.assertEqual(
            bytes.fromhex("933d"),
            result[logical + VERSION_REFERENCE_OFFSET:logical + VERSION_REFERENCE_OFFSET + 2])
        self.assertEqual(
            sum(result[logical:logical + IDENTITY_CHECKSUM_OFFSET]) & 0xffffffff,
            int.from_bytes(
                result[logical + IDENTITY_CHECKSUM_OFFSET:logical + IDENTITY_CHECKSUM_OFFSET + 4],
                "big"))

    def test_rejects_unexpected_donor(self):
        image = bytearray(b"\xff" * 0x400)
        image[6:12] = b"EEPROM"

        with self.assertRaisesRegex(ValueError, "0x933d"):
            make_profile(bytes(image))

    def test_selects_the_evidenced_catalog_among_multiple_magic_strings(self):
        image = bytearray(b"\xff" * 0x900)
        image[6:12] = b"EEPROM"
        selected_catalog = 0x400
        image[selected_catalog + 6:selected_catalog + 12] = b"EEPROM"
        logical = selected_catalog + LOGICAL_DATA_OFFSET
        image[logical:logical + IDENTITY_CHECKSUM_OFFSET] = bytes(
            (index * 3) & 0xff for index in range(IDENTITY_CHECKSUM_OFFSET))
        image[logical + VERSION_REFERENCE_OFFSET:logical + VERSION_REFERENCE_OFFSET + 2] = (
            bytes.fromhex("933d"))

        result = make_profile(bytes(image))

        self.assertEqual(b"\xff\xff\xff\xff", result[IDENTITY_CHECKSUM_OFFSET:IDENTITY_CHECKSUM_OFFSET + 4])
        self.assertEqual(
            sum(result[logical:logical + IDENTITY_CHECKSUM_OFFSET]) & 0xffffffff,
            int.from_bytes(
                result[logical + IDENTITY_CHECKSUM_OFFSET:logical + IDENTITY_CHECKSUM_OFFSET + 4],
                "big"))


if __name__ == "__main__":
    unittest.main()
