import unittest

try:
    from tools.noki8210_phonebook_check import check
except ModuleNotFoundError:
    from noki8210_phonebook_check import check


class PhonebookCheckTest(unittest.TestCase):
    def setUp(self):
        self.write = '\n'.join((
            '8210_phonebook_physical: action=save',
            'header cla=a0 ins=dc p1=01 p2=04 p3=20 selected=6f3a',
            'body ins=dc length=32 selected=6f3a',
            'SIM status ins=dc sw=9000'))
        self.read = '\n'.join((
            'header cla=a0 ins=b2 p1=01 p2=04 p3=20 selected=6f3a',
            '8210_phonebook_read_physical: action=contact'))
        self.storage = bytearray(b'\xff' * 1600)
        self.storage[0] = ord('A')
        self.storage[18:22] = bytes.fromhex('038121f3')

    def test_accept(self):
        check(self.write, self.read, self.storage)

    def test_reject_readback_write(self):
        with self.assertRaisesRegex(ValueError, 'unexpectedly wrote'):
            check(self.write, self.read + '\nins=dc', self.storage)

    def test_reject_missing_success(self):
        with self.assertRaisesRegex(ValueError, 'save evidence'):
            check(self.write.replace('sw=9000', 'sw=9404'), self.read, self.storage)

    def test_reject_wrong_storage(self):
        self.storage[0] = ord('B')
        with self.assertRaises(ValueError):
            check(self.write, self.read, self.storage)


if __name__ == '__main__':
    unittest.main()
