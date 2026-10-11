import unittest
from unittest.mock import patch
from tools.make_sim_card_profile import make_profile
from tools.noki5210_pin_registration_check import verify


def complete():
    return '\n'.join([
        *(f'5210_pin_physical: key=Keypad {digit}' for digit in '1234'),
        '5210_pin_physical: key=Left Softkey / Menu',
        'SIM status ins=20 sw=9000 chv=3/3 puk=10/10 enabled=1',
        'LAPDm Location Updating Accept acknowledged nr=1',
    ])


class PinRegistrationTest(unittest.TestCase):
    def test_complete(self):
        with patch('tools.noki5210_pin_registration_check.registration') as registration:
            verify(complete(), make_profile(pin_enabled=True))
        registration.assert_called_once_with(complete(), 'nsm5')

    def test_bad_pin_or_missing_physical_key(self):
        for text in (complete().replace('sw=9000', 'sw=9804'),
                     complete().replace('key=Keypad 3', 'key=Keypad 5'),
                     complete() + '\n[LUA ERROR]'):
            with self.subTest(text=text), self.assertRaises(ValueError):
                verify(text, make_profile(pin_enabled=True))

    def test_registration_must_follow_pin(self):
        text = complete()
        registration = 'LAPDm Location Updating Accept acknowledged nr=1'
        text = registration + '\n' + text.replace(registration, '')
        with self.assertRaises(ValueError):
            verify(text, make_profile(pin_enabled=True))

    def test_unlocked_card_cannot_pass(self):
        with self.assertRaises(ValueError):
            verify(complete(), make_profile(pin_enabled=False))


if __name__ == '__main__':
    unittest.main()
