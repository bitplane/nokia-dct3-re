import unittest
from tools.noki8890_phonebook_check import verify
from tools.sim_phonebook_check import CURRENT_NVRAM_LENGTH


class PhonebookTest(unittest.TestCase):
    def setUp(self):
        self.data = bytearray([0xff] * CURRENT_NVRAM_LENGTH)
        self.data[0] = ord('A')
        self.data[18:22] = bytes((3, 0x81, 0x21, 0xf3))
        self.trace = ('ins=b2 p1=01 p2=04 p3=20 selected=6f3a\n'
                      '8890_phonebook_read_physical: action=contact\n'
                      '8890_keypad_decoded: key=19')

    def test_readback(self):
        verify('readback', self.trace, self.data)

    def test_readback_must_not_rewrite(self):
        with self.assertRaisesRegex(ValueError, 'rewrote'):
            verify('readback', self.trace + '\nins=dc', self.data)

    def test_requires_physical_decode(self):
        with self.assertRaisesRegex(ValueError, 'decode'):
            verify('readback', self.trace.replace('key=19', 'key=18'), self.data)

    def test_requires_wire_save(self):
        with self.assertRaisesRegex(ValueError, 'UPDATE RECORD'):
            verify('save', '', self.data)
