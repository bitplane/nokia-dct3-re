import struct
import unittest
from unittest.mock import patch

from tools.nsm3d_loader_compare import compare


class LoaderComparisonTest(unittest.TestCase):
    def fixture(self):
        old = [0] * 638
        new = old.copy()
        for number in range(22):
            index = 2 * number + 1
            old[index - 1] = new[index - 1] = 0xf073
            old[index] = 0x1b00 + number
            new[index] = old[index] + (-11 if number < 19 else 10)
        entries = [{'header': [0xfd00, 0xff80, 638, 0x500, 0x78, 0],
                    'words': 638, 'payload_offset': 0,
                    'selector': 0, 'declared_destination': [0xfd00, 0xff7e]},
                   {'selector': 1, 'declared_destination': [0x1000, 0x2000]},
                   {'selector': 20, 'declared_destination': [0x1b00, 0x1c00]}]
        return old, new, entries

    def run_fixture(self, old, new, entries):
        with patch('tools.nsm3d_loader_compare.catalogue', return_value=entries):
            return compare(struct.pack('>638H', *old), struct.pack('>638H', *new))

    def test_exact_deltas_and_overlapping_destinations_retained(self):
        result = self.run_fixture(*self.fixture())
        self.assertEqual(len(result['changed_operands']), 22)
        self.assertEqual(result['unchanged_executable_tail_words'], 126)
        self.assertEqual(result['changed_operands'][0]['old_declared_coverage'], [1, 20])
        self.assertIn('native v6.00 boot', result['scope'])

    def test_executable_or_padding_changes_rejected(self):
        for index in (422, 511, 512, 637):
            old, new, entries = self.fixture()
            new[index] = 1
            with self.subTest(index=index), self.assertRaises(ValueError):
                self.run_fixture(old, new, entries)

    def test_non_branch_and_changed_opcode_rejected(self):
        for word in (0xf074, 0xf495):
            old, new, entries = self.fixture()
            old[0] = new[0] = word
            with self.subTest(opcode=word), self.assertRaises(ValueError):
                self.run_fixture(old, new, entries)
        old, new, entries = self.fixture()
        new[0] = 0xf074
        with self.assertRaises(ValueError):
            self.run_fixture(old, new, entries)

    def test_missing_or_wrong_delta_rejected(self):
        for delta in (0, -10, 11):
            old, new, entries = self.fixture()
            new[1] = old[1] + delta
            with self.subTest(delta=delta), self.assertRaises(ValueError):
                self.run_fixture(old, new, entries)

    def test_product_hash_validation_is_not_bypassed(self):
        with self.assertRaises(ValueError):
            compare(bytes(1276), bytes(1276))


if __name__ == '__main__':
    unittest.main()
