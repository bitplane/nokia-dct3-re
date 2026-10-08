import unittest
import json
import tempfile
from pathlib import Path
from unittest.mock import patch

from PIL import Image
from tools.noki6210_cold_repeat_check import fingerprint, verify


class ColdRepeatCheckTest(unittest.TestCase):
    def test_reads_artifacts_and_detects_pixel_change(self):
        with tempfile.TemporaryDirectory() as directory:
            first, second = Path(directory) / 'a', Path(directory) / 'b'
            for run in (first, second):
                (run / 'nvram/npe3hle').mkdir(parents=True)
                (run / 'snap').mkdir()
                (run / 'acceptance.json').write_text(json.dumps({
                    'machine': 'npe3hle', 'scenario': 'accessory', 'passed': True}))
                (run / 'error.log').write_text('radio peer: ordered fixture event\n')
                for name in ('ccont', 'flash', 'sim_card'):
                    (run / 'nvram/npe3hle' / name).write_bytes(b'fixture')
                for name in ('6210_before_menu.png', '6210_after_menu.png'):
                    Image.new('L', (96, 60), 255).save(run / 'snap' / name)
            self.assertEqual(len(verify(first, second)), 6)
            Image.new('L', (96, 60), 0).save(second / 'snap/6210_after_menu.png')
            with self.assertRaisesRegex(ValueError, '6210_after_menu.png'):
                verify(first, second)
            (first / 'error.log').write_text('unrelated output\n')
            with self.assertRaisesRegex(ValueError, 'empty protocol'):
                fingerprint(first)

    def test_rejects_same_directory(self):
        with self.assertRaisesRegex(ValueError, 'distinct'):
            verify(Path('same'), Path('same'))

    @patch('tools.noki6210_cold_repeat_check.fingerprint')
    def test_matches_all_artifacts(self, fingerprint):
        fingerprint.side_effect = [{'protocol': 'a', 'flash': 'b'}] * 2
        self.assertEqual(verify(Path('a'), Path('b')), {'protocol': 'a', 'flash': 'b'})

    @patch('tools.noki6210_cold_repeat_check.fingerprint')
    def test_rejects_storage_or_protocol_difference(self, fingerprint):
        fingerprint.side_effect = [
            {'protocol': 'a', 'flash': 'b'}, {'protocol': 'c', 'flash': 'd'}]
        with self.assertRaisesRegex(ValueError, 'protocol, flash'):
            verify(Path('a'), Path('b'))
