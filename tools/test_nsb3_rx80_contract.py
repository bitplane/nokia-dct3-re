import unittest
from pathlib import Path

from tools.nsb3_rx80_contract import forwards_selector60, selector60_block_result, inspect


class Nsb3Rx80ContractTest(unittest.TestCase):
    def test_all_256_block_header_values(self):
        accepted = [value for value in range(256) if selector60_block_result(value) == 0x61a]
        self.assertEqual(accepted, [(length << 2) | 1 for length in range(1, 23)])
        self.assertEqual(selector60_block_result(1), 0x61b)
        self.assertEqual(selector60_block_result(93), 0x61b)

    def test_flag_bit_positions_and_branch_interactions(self):
        for bit22 in (0, 1):
            for bit29 in (0, 1):
                for bit31 in (0, 1):
                    for byte6 in (0, 1, 2):
                        for mode in (0, 0x1a):
                            flags = bit22 << 22 | bit29 << 29 | bit31 << 31
                            expected = not bit22 and byte6 != 1 and (not bit29 or mode == 0x1a)
                            self.assertEqual(forwards_selector60(flags, byte6, 0, 0, 1, 1, 0, mode), expected)

    def test_duplicate_gate_is_conditional_not_universal(self):
        self.assertFalse(forwards_selector60(0, 0, 0, 1, 10, 11, 2, 0))
        for flags, gate, reference, previous, state in (
                (1 << 31, 1, 10, 11, 2), (0, 2, 10, 11, 2),
                (0, 1, 10, 10, 2), (0, 1, 10, 11, 1)):
            self.assertTrue(forwards_selector60(flags, 0, 0, gate, reference, previous, state, 0))
        self.assertFalse(forwards_selector60(0, 0, 1, 0, 1, 1, 0, 0))

    def test_own_instruction_and_literal_mutations_fail(self):
        path = Path('roms/noki6190/6190f613a.fls')
        if not path.exists():
            self.skipTest('optional own NSB-3 ROM not installed')
        image = path.read_bytes()
        self.assertEqual(inspect(image)['forward_task'], 14)
        changed = bytearray(image)
        changed[0x8a260] ^= 1
        with self.assertRaises(ValueError):
            inspect(changed)


if __name__ == '__main__':
    unittest.main()
