import tempfile
from pathlib import Path
import unittest

from PIL import Image

from tools.noki8890_clock_check import KEYS, check_frames, verify


def sample():
    return '\n'.join(
        f"8890_clock_physical: key={'Menu' if key == 'Menu' else f'Keypad {key}'}\n"
        f"8890_keypad_decoded: key={'19' if key == 'Menu' else f'{10 if key == 0 else key:02x}'}"
        for key in KEYS)


class ClockCheckTest(unittest.TestCase):
    def test_invalid_recovery(self):
        prefix = ('8890_clock_physical: key=Menu\n'
                  '8890_keypad_decoded: key=19\n') * 2
        verify(prefix + sample(), invalid_first=True)

    def test_invalid_recovery_requires_both_confirmations(self):
        with self.assertRaisesRegex(ValueError, 'sequence'):
            verify(sample(), invalid_first=True)

    def test_complete(self):
        verify(sample())

    def test_missing_event(self):
        with self.assertRaisesRegex(ValueError, 'sequence'):
            verify(sample().replace('8890_clock_physical: key=Keypad 6', 'missing'))

    def test_wrong_decode(self):
        with self.assertRaisesRegex(ValueError, 'did not decode'):
            verify(sample().replace('key=0a', 'key=09', 1))

    def test_decode_cannot_cross_event(self):
        with self.assertRaisesRegex(ValueError, 'did not decode'):
            verify(sample().replace('8890_keypad_decoded: key=01', '', 1))

    def test_blank_frame_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            Image.new('L', (84, 48), 255).save(Path(directory) / '8890_date_entered.png')
            with self.assertRaisesRegex(ValueError, 'frame mismatch'):
                check_frames(Path(directory))
