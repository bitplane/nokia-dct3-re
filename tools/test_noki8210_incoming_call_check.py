import unittest
from pathlib import Path
from tempfile import TemporaryDirectory
from PIL import Image

try:
    from tools.noki8210_incoming_call_check import verify, check_frames
except ModuleNotFoundError:
    from noki8210_incoming_call_check import verify, check_frames


class IncomingCheckTest(unittest.TestCase):
    def test_rejects_blank_caller_presentation(self):
        with TemporaryDirectory() as directory:
            frames = Path(directory)
            Image.new('L', (84, 48)).save(frames / '8210_incoming_ringing.png')
            with self.assertRaisesRegex(ValueError, 'caller 5551234'):
                check_frames(frames)

    def test_requires_registration(self):
        with self.assertRaisesRegex(ValueError, 'registration release'):
            verify('')

    def test_rejects_fixture_error(self):
        with self.assertRaisesRegex(ValueError, 'fixture error'):
            verify('[LUA ERROR]')


if __name__ == '__main__':
    unittest.main()
