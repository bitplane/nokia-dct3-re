"""Protect the independent instruction census, not the CPU's predicates."""
from pathlib import Path
import re
import unittest


class OperandDomainInventoryTest(unittest.TestCase):
    def test_census_has_every_reviewed_read_rmw_class_and_extension(self):
        source = (Path(__file__).resolve().parents[1] /
                  'driver/tms320c54x_test_operand_domains.cpp').read_text()
        table = source.split('source_domains[] = {', 1)[1].split('};', 1)[0]
        actual = [(int(word, 16), int(operation, 16)) for word, operation in
                  re.findall(r'\{\s*(0x[0-9a-f]+),\s*(0x[0-9a-f]+|0)\s*\}', table)]
        # SPRU172C's implemented Smem source/RMW high-byte classes. MMR and
        # shifted-operation extension fields are separate operand grammars.
        scalar = list(range(0x00, 0x48))
        scalar += [0x4b, 0x4c, 0x4d] + list(range(0x50, 0x60))
        scalar += list(range(0x60, 0x6c)) + [0x71, 0x75, 0x78, 0x79, 0x7a, 0x7b, 0x7d, 0x7f]
        shifted = [0x0c40, 0x0d40, 0x0c00, 0x0d00, 0x0e00, 0x0f00,
                   0x0c20, 0x0d20, 0x0e20, 0x0f20]
        expected = [(value << 8, 0) for value in scalar]
        expected += [(0x6f00, operation) for operation in shifted]
        expected += [(value << 8, 0) for value in (0x48, 0x49, 0x4a, 0x73)]
        self.assertEqual(actual, expected)
        self.assertEqual(len(set(actual)), 125)
        self.assertEqual(len(scalar), 111)
        self.assertNotIn((0x6f00, 0x0c60), actual)  # STL's encoded operand is a destination.


if __name__ == '__main__':
    unittest.main()
