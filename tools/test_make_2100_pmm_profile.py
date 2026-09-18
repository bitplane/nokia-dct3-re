import unittest

from tools.make_2100_pmm_profile import (
    IDENTITY_CHECKSUM_OFFSET,
    FLASH_SIZE,
    LOGICAL_DATA_OFFSET,
    PMM_OFFSET,
    VERSION_REFERENCE_OFFSET,
    make_profile,
)


class Make2100PmmProfileTest(unittest.TestCase):
    def fixture(self):
        base = bytes((index * 7) & 0xff for index in range(PMM_OFFSET))
        donor = bytearray(b"\xff" * FLASH_SIZE)
        catalog = PMM_OFFSET
        donor[catalog + 6:catalog + 12] = b"EEPROM"
        logical = catalog + LOGICAL_DATA_OFFSET
        donor[logical:logical + IDENTITY_CHECKSUM_OFFSET] = bytes(
            index & 0xff for index in range(IDENTITY_CHECKSUM_OFFSET))
        donor[logical + VERSION_REFERENCE_OFFSET:logical + VERSION_REFERENCE_OFFSET + 2] = (
            bytes.fromhex("933d"))
        return base, donor, logical

    def test_preserves_reference_and_updates_identity_checksum(self):
        base, donor, logical = self.fixture()

        result = make_profile(base, bytes(donor))

        self.assertEqual(base, result[:PMM_OFFSET])
        self.assertEqual(FLASH_SIZE, len(result))
        self.assertEqual(
            bytes.fromhex("933d"),
            result[logical + VERSION_REFERENCE_OFFSET:logical + VERSION_REFERENCE_OFFSET + 2])
        self.assertEqual(
            sum(result[logical:logical + IDENTITY_CHECKSUM_OFFSET]) & 0xffffffff,
            int.from_bytes(
                result[logical + IDENTITY_CHECKSUM_OFFSET:logical + IDENTITY_CHECKSUM_OFFSET + 4],
                "big"))

    def test_rejects_unexpected_donor(self):
        base = b"\xff" * PMM_OFFSET
        donor = b"\xff" * FLASH_SIZE

        with self.assertRaisesRegex(ValueError, "0x933d"):
            make_profile(base, donor)

    def test_selects_the_evidenced_catalog_among_multiple_magic_strings(self):
        base, donor, _ = self.fixture()
        donor[PMM_OFFSET + 6:PMM_OFFSET + 12] = b"NOTEEP"
        selected_catalog = PMM_OFFSET + 0x400
        donor[selected_catalog + 6:selected_catalog + 12] = b"EEPROM"
        logical = selected_catalog + LOGICAL_DATA_OFFSET
        donor[logical:logical + IDENTITY_CHECKSUM_OFFSET] = bytes(
            (index * 3) & 0xff for index in range(IDENTITY_CHECKSUM_OFFSET))
        donor[logical + VERSION_REFERENCE_OFFSET:logical + VERSION_REFERENCE_OFFSET + 2] = (
            bytes.fromhex("933d"))

        result = make_profile(base, bytes(donor))

        self.assertEqual(base, result[:PMM_OFFSET])
        self.assertEqual(
            sum(result[logical:logical + IDENTITY_CHECKSUM_OFFSET]) & 0xffffffff,
            int.from_bytes(
                result[logical + IDENTITY_CHECKSUM_OFFSET:logical + IDENTITY_CHECKSUM_OFFSET + 4],
                "big"))

    def test_rejects_wrong_input_sizes(self):
        base, donor, _ = self.fixture()
        with self.assertRaisesRegex(ValueError, "MCU/PPM base"):
            make_profile(base + b"\xff", bytes(donor))
        with self.assertRaisesRegex(ValueError, "full-flash donor"):
            make_profile(base, bytes(donor[:-1]))


if __name__ == "__main__":
    unittest.main()
