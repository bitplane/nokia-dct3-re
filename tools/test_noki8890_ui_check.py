import unittest
from tools.noki8890_ui_check import verify
from tools.test_noki8890_staged_check import SELFTEST


def sample():
    sim = '\n'.join('read-binary fid=' + fid for fid in ('2fe2', '6fae', '6f38', '6f07'))
    sim += '\n' + '\n'.join(f'header cla=a0 ins=b2 p1={i:02x} p2=04 p3=20 selected=6f3a' for i in range(1, 51))
    keys = '\n'.join(f'8890_security_physical: key={name}\n8890_keypad_decoded: key={code}' for name, code in (
        ('Keypad 1', '01'), ('Keypad 2', '02'), ('Keypad 3', '03'),
        ('Keypad 4', '04'), ('Keypad 5', '05'), ('Menu', '19')))
    keys += '\n' + '\n'.join(f'8890_navigation_physical: press={i}\n8890_keypad_decoded: key=19' for i in range(1, 4))
    return SELFTEST + '\n' + sim + '\n' + keys


class UiTest(unittest.TestCase):
    def test_complete(self):
        verify(sample())

    def test_missing_adn(self):
        with self.assertRaisesRegex(ValueError, '50 ADN'):
            verify(sample().replace('p1=32', 'p1=00'))

    def test_wrong_decode(self):
        with self.assertRaisesRegex(ValueError, 'did not decode'):
            verify(sample().replace('key=03', 'key=04'))
