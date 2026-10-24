import unittest

from tools.nsb3_nominal_input import checksum_control
from tools.nsb3_selftest_check import check_traces


class Nsb3SelftestTest(unittest.TestCase):
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
