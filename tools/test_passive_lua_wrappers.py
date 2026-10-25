"""Source-convention guard, not a behavioral emulator acceptance test."""
from pathlib import Path
import re
import unittest


class PassiveWrapperTests(unittest.TestCase):
    def test_observation_wrappers_have_no_direct_guest_memory_writes(self):
        root = Path(__file__).resolve().parents[1]
        for name in ('ram_probe', 'coverage', 'snapshot'):
            with self.subTest(wrapper=name):
                text = (root / f'mame_nokia_dct3_{name}.lua').read_text()
                self.assertIsNone(re.search(r':write_(?:u|s|direct|range)\w*\s*\(', text))


if __name__ == '__main__':
    unittest.main()
