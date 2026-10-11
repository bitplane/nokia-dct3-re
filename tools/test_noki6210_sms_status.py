import unittest

from tools.noki6210_outgoing_sms_check import verify_status


class SmsStatusTest(unittest.TestCase):
    def fixture(self):
        text = ('read-binary fid=6f43 offset=0 length=2\n'
                'GSM service uplink sapi=3 pd=09 message=01 length=27 '
                'data=390118000100069121436587090d11010781551532f40000ff0141\n'
                'update-binary fid=6f43 offset=0 length=2\n')
        storage = bytes(3560) + b'\x01\xff'
        return text, storage

    def test_reference_matches_persistent_file(self):
        self.assertEqual(verify_status(*self.fixture()), 1)

    def test_aoc_extension_preserves_sms_status_offsets(self):
        text, storage = self.fixture()
        self.assertEqual(verify_status(text, storage + bytes(range(8))), 1)
        for size in range(1, 8):
            with self.subTest(size=size), self.assertRaises(ValueError):
                verify_status(text, storage + bytes(size))

    def test_missing_events(self):
        text, storage = self.fixture()
        for line in text.splitlines():
            with self.subTest(line=line), self.assertRaises(ValueError):
                verify_status(text.replace(line, ''), storage)

    def test_read_and_update_order(self):
        text, storage = self.fixture()
        lines = text.splitlines()
        for reordered in ((lines[1], lines[0], lines[2]),
                          (lines[0], lines[2], lines[1])):
            with self.subTest(order=reordered), self.assertRaises(ValueError):
                verify_status('\n'.join(reordered), storage)

    def test_corrupt_or_partial_status(self):
        text, storage = self.fixture()
        for changed in (storage[:-1], storage + b'\x00',
                        storage[:-2] + b'\x02\xff', storage[:-1] + b'\xfe'):
            with self.subTest(storage=changed[-3:]), self.assertRaises(ValueError):
                verify_status(text, changed)

    def test_cold_reference_is_not_fixed(self):
        text, storage = self.fixture()
        text = text.replace('0d11010781', '0d11020781')
        self.assertEqual(verify_status(text, storage[:-2] + b'\x02\xff'), 2)

    def test_product_validity_period_and_zero_reference(self):
        from tools.sim_sms_status_check import verify
        text, storage = self.fixture()
        text = text.replace('0d11010781', '0d11000781').replace('0000ff0141', '0000a70141')
        storage = storage[:-2] + b'\x00\xff'
        self.assertEqual(verify(text, storage, validity_period=0xa7), 0)
        with self.assertRaises(ValueError):
            verify_status(text, storage)
        for invalid in (-1, 256, 'a7'):
            with self.subTest(value=invalid), self.assertRaises(ValueError):
                verify(text, storage, validity_period=invalid)


if __name__ == '__main__':
    unittest.main()
