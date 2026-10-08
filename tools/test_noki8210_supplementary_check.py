import hashlib
from pathlib import Path
import tempfile
import unittest
from PIL import Image

from tools import radio_call_divert_trace_check as protocol
from tools.noki8210_supplementary_check import verify_transaction
from tools.test_radio_call_divert_trace_check import GOOD


KEYS = ('Keypad *', 'Keypad #', 'Keypad 2', 'Keypad 1', 'Keypad #', 'Call / Send')
INPUT = ''.join('8210_divert_physical: key=' + key + '\n' for key in KEYS)
BACK = '8210_divert_physical: key=Back\n'


class SupplementaryTest(unittest.TestCase):
    def check(self, text, size=(84, 48), wrong_hash=False):
        with tempfile.TemporaryDirectory() as directory:
            frames = Path(directory)
            frame = Image.new('L', size, 255)
            digest = hashlib.sha256(frame.tobytes()).hexdigest()
            for phase in ('result', 'after_back'):
                frame.save(frames / f'8210_divert_{phase}.png')
            verify_transaction(text, frames, 'divert', KEYS, protocol,
                               '0' * 64 if wrong_hash else digest, digest)

    def test_complete(self):
        self.check(INPUT + GOOD + BACK)

    def test_missing_or_early_back(self):
        for text in (INPUT + GOOD, INPUT + BACK + GOOD):
            with self.subTest(text=text), self.assertRaisesRegex(ValueError, 'physical Back'):
                self.check(text)

    def test_missing_physical_key(self):
        with self.assertRaisesRegex(ValueError, 'physical input'):
            self.check((INPUT + GOOD + BACK).replace('key=Keypad 2', 'key=Keypad 3'))

    def test_wrong_frame_or_geometry(self):
        for options in ({'wrong_hash': True}, {'size': (96, 60)}):
            with self.subTest(options=options), self.assertRaisesRegex(ValueError, 'firmware frame'):
                self.check(INPUT + GOOD + BACK, **options)

    def test_lua_failure(self):
        with self.assertRaisesRegex(ValueError, 'fixture failed'):
            self.check(INPUT + GOOD + BACK + '[LUA ERROR]')


if __name__ == '__main__':
    unittest.main()
