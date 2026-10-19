import struct
import unittest
from tools.noki5510_nv_check import assess, verify_erased_frontier


class NvCheckTest(unittest.TestCase):
    def test_erased_frontier_requires_actual_store_address(self):
        text = ('pc=0024c7dc address=0013fbf2 data=00000012\n'
                'pc=0024c85a address=0013fbec data=0000000c\n'
                '5510_selftest_return: command=0d flags=88 records=ffff00ff/00ffff00/00ffff00/0cff0000/00001200')
        verify_erased_frontier(text, b'\xff' * 0x3800)
        with self.assertRaises(ValueError):
            verify_erased_frontier(text.replace('24c85a', '24c858'), b'\xff' * 0x3800)

    def test_erased_checksums_fail(self):
        report = assess(b'\xff' * 0x3800)
        self.assertEqual(report['security']['sum'], 0x1ae4)
        self.assertEqual(report['configuration']['sum'], 0x32cc)
        self.assertFalse(report['security']['valid'])
        self.assertFalse(report['configuration']['valid'])

    def test_checksum_and_nonzero_guard_are_independent(self):
        cache = bytearray(0x3800)
        self.assertTrue(assess(cache)['security']['valid'])
        self.assertFalse(assess(cache)['configuration']['valid'])
        cache[0x120] = 1
        struct.pack_into('>H', cache, 0x256, 1)
        self.assertTrue(assess(cache)['configuration']['valid'])
        cache[0x154:0x156] = b'\xff\xff'
        self.assertTrue(assess(cache)['configuration']['valid'])
        cache[0x121] = 1
        self.assertFalse(assess(cache)['configuration']['valid'])

    def test_incomplete_cache_rejected(self):
        with self.assertRaises(ValueError):
            assess(bytes(0x258))


if __name__ == '__main__':
    unittest.main()
