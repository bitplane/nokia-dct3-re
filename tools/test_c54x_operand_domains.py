"""Protect the independent instruction census, not the CPU's predicates."""
from pathlib import Path
import re
import unittest


class OperandDomainInventoryTest(unittest.TestCase):
    def test_rejection_families_do_not_borrow_other_fixture_state(self):
        # Architectural boundary only; the MAME profile runs establish behavior.
        source = (Path(__file__).resolve().parents[1] /
                  'tests/c54x/tms320c54x_test_operand_domains.cpp').read_text()
        methods = list(re.finditer(r'^(?:void|bool) tms320c54x_test_state::(\w+)\(', source, re.M))
        owners = {
            'start_source_domains': 'm_source_domains',
            'check_source_domains': 'm_source_domains',
            'start_source_alias_case': 'm_source_aliases',
            'check_source_alias_results': 'm_source_aliases',
            'start_short_read_mod3_edge': 'm_short_sources',
            'check_short_source_results': 'm_short_sources',
        }
        self.assertEqual({method[1] for method in methods}, set(owners))
        for index, method in enumerate(methods):
            end = methods[index + 1].start() if index + 1 < len(methods) else len(source)
            body = re.sub(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"', '',
                          source[method.start():end], flags=re.S)
            members = set(re.findall(r'\bm_\w+\b', body))
            with self.subTest(method=method[1]):
                self.assertLessEqual(members, {'m_cpu', 'm_phase', 'm_check_timer', owners[method[1]]})
                self.assertIn(owners[method[1]], members)

    def test_census_has_every_reviewed_read_rmw_class_and_extension(self):
        source = (Path(__file__).resolve().parents[1] /
                  'tests/c54x/tms320c54x_test_operand_domains.cpp').read_text()
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
