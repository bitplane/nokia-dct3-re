import hashlib
import unittest
from unittest.mock import patch
from PIL import Image
from tools import run_noki6210_acceptance as runner


class MenuAcceptanceTest(unittest.TestCase):
    def fixture(self):
        text = '\n'.join(f'sim_device: header cla=a0 ins=b2 p1={i:02x} p2=04 p3=20 selected=6f3a'
                         for i in range(1, 51))
        text += '\n6210_menu_physical: press=1\n6210_keypad_decoded: key=19'
        return text, Image.new('L', (96, 60), 255)

    def test_complete(self):
        text, frame = self.fixture()
        with patch.object(runner, 'MENU_SHA256', hashlib.sha256(frame.tobytes()).hexdigest()):
            runner.check_menu(text, frame)

    def test_incomplete_sim_initialization(self):
        text, frame = self.fixture()
        with self.assertRaisesRegex(ValueError, '50 ADN'):
            runner.check_menu(text.replace('p1=32', 'p1=31'), frame)

    def test_host_press_alone_is_not_decoded_input(self):
        text, frame = self.fixture()
        with self.assertRaises(ValueError):
            runner.check_menu(text.replace('key=19', 'key=5a'), frame)

    def test_wrong_frame_is_not_success(self):
        text, frame = self.fixture()
        with self.assertRaisesRegex(ValueError, 'Messages screen'):
            runner.check_menu(text, frame)


if __name__ == '__main__':
    unittest.main()
