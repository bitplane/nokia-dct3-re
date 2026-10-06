import struct
import unittest
from tools.dct3_startup_records import decode_startup_records, initialized_bytes


class StartupRecordsTest(unittest.TestCase):
    def test_byte_tail_alignment_and_short_terminator(self):
        image = struct.pack('>II', 3, 0x30000) + b'abc\xff' + bytes(4)
        records, end = decode_startup_records(image, 0x200000, 0x200000)
        self.assertEqual(records, [(0x200008, 0x30000, b'abc')])
        self.assertEqual(end, 0x200010)

    def test_overlap_preserves_last_writer(self):
        records = [(0, 10, b'abcd'), (8, 11, b'XY')]
        self.assertEqual(initialized_bytes(records, 10, 4), b'aXYd')
        with self.assertRaises(ValueError):
            initialized_bytes(records, 9, 4)

    def test_truncation_and_overflow(self):
        for image in (b'\x00', struct.pack('>II', 4, 10) + b'a',
                      struct.pack('>II', 4, 0xfffffffe) + bytes(4)):
            with self.assertRaises(ValueError):
                decode_startup_records(image, 0, 0)


if __name__ == '__main__':
    unittest.main()
