from pathlib import Path
import tempfile
import unittest

from tools.noki8210_ussd_check import verify, verify_key_table


class UssdTest(unittest.TestCase):
    def test_key_table_and_pointer(self):
        image = bytearray(0x13ee91)
        image[0x107e40:0x107e44] = bytes.fromhex('0033ee78')
        image[0x13ee78:0x13ee91] = bytes.fromhex(
            '3e3e3e3e3e11190102030e170405060f18070809101a0c0a0b')
        verify_key_table(image)
        image[0x13ee8e], image[0x13ee90] = image[0x13ee90], image[0x13ee8e]
        with self.assertRaises(ValueError):
            verify_key_table(image)

    def test_short_or_wrong_lane_image(self):
        for image in (b'', bytes(0x13ee91)):
            with self.assertRaises(ValueError):
                verify_key_table(image)

    def test_physical_sequence_without_protocol_cannot_pass(self):
        text = ''.join('8210_ussd_physical: key=' + key + '\n' for key in
                       ('Keypad *', 'Keypad 1', 'Keypad 2', 'Keypad 3', 'Keypad #', 'Call / Send'))
        with tempfile.TemporaryDirectory() as directory, self.assertRaises(ValueError):
            verify(text, Path(directory))


if __name__ == '__main__':
    unittest.main()
