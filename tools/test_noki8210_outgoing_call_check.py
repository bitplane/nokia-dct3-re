import unittest
from pathlib import Path
import tempfile
from PIL import Image
from tools.noki8210_outgoing_call_check import check_frames

try:
    from tools.noki8210_outgoing_call_check import verify
except ModuleNotFoundError:
    from noki8210_outgoing_call_check import verify


class OutgoingCheckTest(unittest.TestCase):
    def test_blank_or_wrong_geometry_cannot_prove_outgoing_ui(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)
            for size, color in (((84, 48), 255), ((84, 48), 0), ((96, 60), 255)):
                Image.new('L', size, color).save(path / '8210_dialed_number.png')
                with self.subTest(size=size, color=color), self.assertRaisesRegex(ValueError, 'presentation'):
                    check_frames(path)
    def test_requires_physical_send(self):
        with self.assertRaisesRegex(ValueError, 'physical Send'):
            verify('')

    def test_requires_own_send_decode(self):
        with self.assertRaisesRegex(ValueError, 'Send decode'):
            verify('8210_call_physical: action=send')

    def test_rejects_fixture_error(self):
        with self.assertRaisesRegex(ValueError, 'fixture error'):
            verify('[LUA ERROR]')


if __name__ == '__main__':
    unittest.main()
