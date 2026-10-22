import unittest
import json
import tempfile
import uuid
import shutil
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
                    'machine': 'npe3hle', 'scenario': 'accessory', 'passed': True,
                    'launch': {'run_directory': str(run.resolve()), 'invocation_id': str(uuid.uuid4()),
                               'input_sha256': {key: 'a'*64 for key in ('mame', 'mcu', 'pmm', 'script')},
                               'initial_storage': {}}}))
                (run / 'error.log').write_text('radio peer: ordered fixture event\n')
                for name in ('ccont', 'flash', 'sim_card'):
                    (run / 'nvram/npe3hle' / name).write_bytes(b'fixture')
                for name in ('6210_before_menu.png', '6210_after_menu.png'):
                    Image.new('L', (96, 60), 255).save(run / 'snap' / name)
            self.assertEqual(len(verify(first, second)), 7)
            copied = Path(directory) / 'copied'
            shutil.copytree(first, copied)
            with self.assertRaisesRegex(ValueError, 'copied'):
                verify(first, copied)
            acceptance = json.loads((copied / 'acceptance.json').read_text())
            acceptance['launch']['run_directory'] = str(copied.resolve())
            (copied / 'acceptance.json').write_text(json.dumps(acceptance))
            with self.assertRaisesRegex(ValueError, 'independent'):
                verify(first, copied)
            Image.new('L', (96, 60), 0).save(second / 'snap/6210_after_menu.png')
            with self.assertRaisesRegex(ValueError, '6210_after_menu.png'):
                verify(first, second)
            (first / 'error.log').write_text('unrelated output\n')
            with self.assertRaisesRegex(ValueError, 'empty protocol'):
                fingerprint(first)

    def test_rejects_same_directory(self):
        with self.assertRaisesRegex(ValueError, 'distinct'):
            verify(Path('same'), Path('same'))

    def test_requires_valid_input_identity_and_no_preseeded_storage(self):
        with tempfile.TemporaryDirectory() as directory:
            run = Path(directory)
            valid = {'run_directory': str(run.resolve()), 'invocation_id': str(uuid.uuid4()),
                     'input_sha256': {key: 'a'*64 for key in ('mame', 'mcu', 'pmm', 'script')},
                     'initial_storage': {}}
            for launch in (None, {}, {**valid, 'invocation_id': None},
                           {**valid, 'invocation_id': 'not-a-uuid'},
                           {**valid, 'input_sha256': {'mame':'a'*64}},
                           {**valid, 'input_sha256': {**valid['input_sha256'], 'mcu':'invalid'}},
                           {**valid, 'initial_storage': None},
                           {**valid, 'initial_storage': {'npe3hle/flash':'a'*64}}):
                (run / 'acceptance.json').write_text(json.dumps({
                    'machine':'npe3hle', 'scenario':'accessory', 'passed':True, 'launch':launch}))
                with self.subTest(launch=launch), self.assertRaises(ValueError):
                    fingerprint(run)

    @patch('tools.noki6210_cold_repeat_check.fingerprint')
    def test_matches_all_artifacts(self, fingerprint):
        fingerprint.side_effect = [{'invocation_id': identifier, 'protocol': 'a', 'flash': 'b'}
                                  for identifier in ('first', 'second')]
        self.assertEqual(verify(Path('a'), Path('b')), {'protocol': 'a', 'flash': 'b'})

    @patch('tools.noki6210_cold_repeat_check.fingerprint')
    def test_rejects_storage_or_protocol_difference(self, fingerprint):
        fingerprint.side_effect = [
            {'invocation_id': 'first', 'protocol': 'a', 'flash': 'b'},
            {'invocation_id': 'second', 'protocol': 'c', 'flash': 'd'}]
        with self.assertRaisesRegex(ValueError, 'protocol, flash'):
            verify(Path('a'), Path('b'))

    @patch('tools.noki6210_cold_repeat_check.fingerprint')
    def test_same_launch_or_different_inputs_cannot_prove_repeatability(self, fingerprint):
        fingerprint.side_effect = [{'invocation_id': 'same', 'protocol': 'a'} for _ in range(2)]
        with self.assertRaisesRegex(ValueError, 'independent'):
            verify(Path('a'), Path('b'))
        fingerprint.side_effect = [
            {'invocation_id': 'first', 'launch_inputs': 'a'},
            {'invocation_id': 'second', 'launch_inputs': 'b'}]
        with self.assertRaisesRegex(ValueError, 'launch_inputs'):
            verify(Path('a'), Path('b'))
