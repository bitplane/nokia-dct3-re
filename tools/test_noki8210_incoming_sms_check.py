import unittest
from pathlib import Path
from tempfile import TemporaryDirectory
from PIL import Image

try:
    from tools.noki8210_incoming_sms_check import verify, verify_frame
except ModuleNotFoundError:
    from noki8210_incoming_sms_check import verify, verify_frame


class IncomingSmsTest(unittest.TestCase):
    def test_blank_message_body_is_not_acceptance(self):
        with TemporaryDirectory() as directory:
            path = Path(directory) / 'frame.png'
            Image.new('L', (84, 48)).save(path)
            with self.assertRaisesRegex(ValueError, 'hello message body'):
                verify_frame(path)

    def test_rejects_empty_evidence(self):
        with self.assertRaises(ValueError):
            verify('', b'')

    def test_rejects_fixture_error(self):
        with self.assertRaisesRegex(ValueError, 'fixture error'):
            verify('[LUA ERROR]', b'')


if __name__ == '__main__':
    unittest.main()
