"""Raw-token rejection inventory must retain unlogged and conditional sites."""
import unittest

from tools.c54x_dispatch_audit import clang_library
from tools.c54x_rejection_audit import inventory


try:
    clang_library()
    HAS_CLANG = True
except (FileNotFoundError, ValueError):
    HAS_CLANG = False


@unittest.skipUnless(HAS_CLANG, 'manual audit requires libclang')
class RejectionInventoryTests(unittest.TestCase):
    def test_conditional_and_unlogged_sites_are_distinct(self):
        result = inventory(b'''void tms320c54x_device::step() {
            if (conflict) { logerror("bus arbitration unsupported\\n"); m_illegal = true; return; }
            else if (missing) { m_illegal = true; return; }
            if (invalid) { logerror("unsupported form %04x\\n", op); m_illegal = true; }
        }''')
        self.assertEqual(result['sites'], 3)
        self.assertEqual(result['categories'], {'bus-arbitration': 1, 'unlogged': 1, 'encoding': 1})
        self.assertIsNone(result['rows'][1]['message'])
        self.assertEqual(result['rows'][1]['guards'], ['!(conflict)', 'missing'])
        self.assertFalse(result['reachability_claim'])

    def test_comments_strings_and_clearing_illegal_are_not_rejections(self):
        result = inventory(b'''void tms320c54x_device::reset() {
            // m_illegal = true;
            logerror("m_illegal = true;"); m_illegal = false;
        }''')
        self.assertEqual(result['sites'], 0)

    def test_unowned_rejection_fails_closed(self):
        with self.assertRaisesRegex(ValueError, 'owner'):
            inventory(b'void unrelated() { m_illegal = true; }')

    def test_format_arguments_do_not_become_part_of_the_diagnostic(self):
        result = inventory(b'''void tms320c54x_device::step() {
            logerror("operand " "%s", "nested"); m_illegal = true;
        }''')
        self.assertEqual(result['rows'][0]['message'], 'operand %s')
        self.assertEqual(result['rows'][0]['category'], 'other-diagnostic')


if __name__ == '__main__':
    unittest.main()
