"""Architectural fixture boundaries; executable MAME gates establish behavior."""
from pathlib import Path
import re
import unittest


ROOT = Path(__file__).resolve().parents[1] / 'tests/c54x'


def methods(filename):
    source = (ROOT / filename).read_text()
    declarations = list(re.finditer(r'^(?:void|bool) tms320c54x_test_state::(\w+)\(', source, re.M))
    bodies = {}
    for index, declaration in enumerate(declarations):
        end = declarations[index + 1].start() if index + 1 < len(declarations) else len(source)
        bodies[declaration[1]] = re.sub(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"', '',
                                      source[declaration.start():end], flags=re.S)
    return bodies


class FixtureOwnershipTests(unittest.TestCase):
    def check_owners(self, filename, owners, handoffs=()):
        bodies = methods(filename)
        self.assertEqual(set(bodies), set(owners))
        shared = {'m_cpu', 'm_phase', 'm_check_timer'}
        legacy_taps = {'m_vector_prefetch_tap', 'm_vector_stack_tap'}
        for name, body in bodies.items():
            with self.subTest(method=name):
                members = set(re.findall(r'\bm_\w+\b', body))
                allowed = shared | {owners[name]}
                if name in handoffs:
                    allowed |= legacy_taps
                    # Predecessor cleanup is not permission to observe or reuse its state.
                    remaining = re.sub(r'\bm_vector_(?:prefetch|stack)_tap\s*\.\s*remove\s*\(\s*\)', '', body)
                    self.assertFalse(set(re.findall(r'\bm_\w+\b', remaining)) & legacy_taps)
                self.assertLessEqual(members, allowed)
                self.assertIn(owners[name], members)

    def test_operand_rejection_families(self):
        self.check_owners('tms320c54x_test_operand_domains.cpp', {
            'start_source_domains': 'm_source_domains',
            'check_source_domains': 'm_source_domains',
            'start_source_alias_case': 'm_source_aliases',
            'check_source_alias_results': 'm_source_aliases',
            'start_short_read_mod3_edge': 'm_short_sources',
            'check_short_source_results': 'm_short_sources',
        })

    def test_interrupt_entry_family(self):
        names = ('start_interrupt_entry_spacing_case', 'start_interrupt_acceptance_checkpoint',
                 'start_entry_stage_restore_case', 'start_vector_prefetch_case',
                 'check_vector_prefetch_results', 'check_interrupt_entry_results',
                 'acknowledge_entry_interrupt')
        self.check_owners('tms320c54x_test_interrupt_entry.cpp', dict.fromkeys(names, 'm_entry'),
                          handoffs=('start_interrupt_entry_spacing_case',
                                    'start_entry_stage_restore_case', 'start_vector_prefetch_case'))


if __name__ == '__main__':
    unittest.main()
