import unittest
from pathlib import Path
from tools.noki6210_installed_program import assess, BLOCKS


class InstalledProgramTest(unittest.TestCase):
    def test_rejects_other_image(self):
        with self.assertRaisesRegex(ValueError, 'acquired NPE-3'):
            assess(b'')

    def test_source_and_capture_contract(self):
        path = Path(__file__).resolve().parents[1] / 'roms/noki6210/6210_556c.fls'
        if not path.exists():
            self.skipTest('own acquired image unavailable')
        image = path.read_bytes()
        captures = {name: image[offset:offset + count * 2]
                    for name, (offset, _, count) in BLOCKS.items()}
        result = assess(image, captures)
        self.assertEqual(result['pairs_examined'], 211)
        self.assertEqual(result['return_nop_pairs'], 143)
        self.assertEqual(len(result['branch_pairs']), 64)
        self.assertEqual(result['unowned_branch_pairs'], 58)
        self.assertEqual(len(result['other_pairs']), 4)
        captures['loader2'] = captures['loader2'][:-2]
        with self.assertRaisesRegex(ValueError, 'loader2 capture differs'):
            assess(image, captures)
