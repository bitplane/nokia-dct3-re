"""Bounds and read-only contract for the cross-page execution observer."""
import unittest

from tools.make_c54x_entry_script import script


class EntryScriptTest(unittest.TestCase):
    def test_all_pages_and_bounded_observation(self):
        text = script(0x29d3)
        probes = [line for line in text.splitlines() if line.startswith('bpset ')]
        self.assertEqual(len(probes), 128)
        self.assertTrue(probes[0].startswith('bpset 29d3,'))
        self.assertTrue(probes[-1].startswith('bpset 7f29d3,'))
        for line in probes:
            self.assertIn('temp9<8&&', line)
            self.assertIn('observer_writes=0', line)
            self.assertNotIn('dw@29d3=', line)

    def test_next_word_wraps_inside_page(self):
        text = script(0xffff, 2)
        self.assertIn('pw@7fffff,pw@7f0000', text)
        self.assertIn('temp9<2&&', text)

    def test_invalid_bounds(self):
        for target, limit in ((-1, 8), (0x10000, 8), (0, 0), (0, 65)):
            with self.assertRaises(ValueError):
                script(target, limit)


if __name__ == '__main__':
    unittest.main()
