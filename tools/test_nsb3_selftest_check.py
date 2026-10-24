import unittest

from tools.nsb3_nominal_input import checksum_control, fresh_phone_retry
from tools.nsb3_selftest_check import check_traces


class Nsb3SelftestTest(unittest.TestCase):
    def test_fresh_retry_changes_only_failure_counter_and_checks_own_contract(self):
        image = bytearray(0x100000)
        image[0xd03a8:0xd03b0] = bytes.fromhex('0000070904ac0001')
        image[0xaf48c:0xaf4a4] = bytes.fromhex('2900d0072000712020041900492f2200f7fcfa3d2001bd10')
        source = bytes([255]) * 8192
        result = fresh_phone_retry(image, source)
        self.assertEqual([i for i, (a, b) in enumerate(zip(source, result)) if a != b], [0x4ac])
        self.assertEqual(result[0x4ac], 0)
        self.assertEqual(result[0x4a8:0x4ac], source[0x4a8:0x4ac])
        for offset in (0xd03a8, 0xaf48c):
            altered = bytearray(image)
            altered[offset] ^= 1
            with self.assertRaises(ValueError):
                fresh_phone_retry(altered, source)

    def test_integrity_edits_preserve_every_content_byte(self):
        source = bytes([255]) * 8192
        calibration = checksum_control(source)
        complete = checksum_control(source, True)
        self.assertEqual(calibration[:0x1fe], source[:0x1fe])
        self.assertEqual(complete[:0x11c], source[:0x11c])
        self.assertEqual(complete[0x120:0x1fe], source[0x120:0x1fe])
        self.assertEqual(complete[0x200:], source[0x200:])
        self.assertEqual(int.from_bytes(complete[0x11c:0x120], 'big'), sum(source[:0x11c]) & 65535)
        self.assertEqual(int.from_bytes(complete[0x1fe:0x200], 'big'),
                         (sum(source[0x120:0x1fe]) - sum(source[0x154:0x156])) & 65535)
        with self.assertRaises(ValueError):
            checksum_control(source[:-1])

    def test_calibration_pass_is_not_all_selftests_pass(self):
        comparison = 'nsb3_checksum_compare: stored=db24 computed=db24 secondary=ffff observer_writes=0'
        partial = comparison + '\nnsb3_selftest_error: index=12 code=12 pointer=0010fcb2 observer_writes=0'
        complete = comparison + '\nnsb3_selftest_final: flags=40 remaining=00 observer_writes=0'
        check_traces(partial, complete)
        for bad in (partial, complete.replace('flags=40', 'flags=00'),
                    complete.replace('stored=db24', 'stored=ffff')):
            with self.assertRaises(ValueError):
                check_traces(partial, bad)


if __name__ == '__main__':
    unittest.main()
