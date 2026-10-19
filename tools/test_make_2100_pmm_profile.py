import unittest
from pathlib import Path

from tools.make_2100_pmm_profile import (
    IDENTITY_CHECKSUM_OFFSET,
    FLASH_SIZE,
    LOGICAL_DATA_OFFSET,
    PMM_OFFSET,
    VERSION_REFERENCE_OFFSET,
    make_profile,
    make_own_v521_profile,
)


class Make2100PmmProfileTest(unittest.TestCase):
    def test_own_v521_rejects_unidentified_image(self):
        with self.assertRaisesRegex(ValueError, "unchanged acquired"):
            make_own_v521_profile(bytes(FLASH_SIZE))

    def test_own_v521_changes_only_its_checksum_when_available(self):
        source = Path(__file__).resolve().parents[1] / "roms/noki2100/2100f521sharp.fls"
        if not source.exists():
            self.skipTest("acquired NAM-2 v5.21 input not present")
        original = source.read_bytes()
        repaired = make_own_v521_profile(original)
        self.assertEqual([0x1FC144, 0x1FC145],
                         [index for index, (a, b) in enumerate(zip(original, repaired)) if a != b])
        self.assertEqual(original[:PMM_OFFSET], repaired[:PMM_OFFSET])
        self.assertEqual(bytes.fromhex("0000a0aa"), repaired[0x1FC142:0x1FC146])
        with self.assertRaisesRegex(ValueError, "unchanged acquired"):
            make_own_v521_profile(repaired)

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
