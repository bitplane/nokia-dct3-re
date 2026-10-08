import unittest

from tools.noki8850_speech_control_check import recover


class SpeechControlTests(unittest.TestCase):
    def test_wrong_rom_rejected(self):
        with self.assertRaisesRegex(ValueError, 'NSM-2'):
            recover(b'not the acquired image')


if __name__ == '__main__':
    unittest.main()
