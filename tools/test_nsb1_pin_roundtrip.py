from pathlib import Path
import unittest
from unittest.mock import patch

from tools.make_sim_card_profile import make_profile
from tools.nsb1_pin_roundtrip import check_card, check_correct, check_wrong
from tools.sim_security_trace_check import AOC_NVRAM_SIZE, ATTEMPTS_OFFSET, CHV_OFFSET, ENABLED_OFFSET


class PinRoundtripTest(unittest.TestCase):
    def storage(self, retries):
        data = bytearray(make_profile(True))
        data.extend(bytes(AOC_NVRAM_SIZE - len(data)))
        data[ATTEMPTS_OFFSET] = retries
        return bytes(data)

    def contacts(self, phase, count):
        return '\n'.join(f'nsb1_pin_contact: phase={phase} index={index} down={down} t=12.0 physical_only=1'
                         for index in range(1, count + 1) for down in (1, 0))

    def wrong_trace(self):
        return self.contacts('wrong', 5) + '\nSIM status ins=20 sw=9804 chv=2/3 puk=10/10 enabled=1'

    def correct_trace(self):
        return (self.contacts('pin', 5) + '\n' + self.contacts('phone', 6)
                + '\nSIM status ins=20 sw=9000 chv=3/3 puk=10/10 enabled=1')

    def test_wrong_pin_requires_physical_failure_and_persisted_retry(self):
        with patch('tools.nsb1_pin_roundtrip.check_frame') as frame:
            check_wrong(self.wrong_trace(), self.storage(2), Path('.'))
            self.assertEqual(frame.call_count, 2)
            for trace, data in ((self.wrong_trace().replace('sw=9804', 'sw=9000'), self.storage(2)),
                                (self.wrong_trace(), self.storage(3)),
                                (self.wrong_trace().replace('index=5', 'index=4'), self.storage(2))):
                with self.subTest(trace=trace), self.assertRaises(ValueError):
                    check_wrong(trace, data, Path('.'))

    def test_correct_pin_requires_separate_phone_code_and_recovery(self):
        with patch('tools.nsb1_pin_roundtrip.check_frame') as frame:
            check_correct(self.correct_trace(), self.storage(3), Path('.'))
            self.assertEqual(frame.call_count, 4)
            for trace, data in ((self.contacts('pin', 5), self.storage(3)),
                                (self.correct_trace(), self.storage(2)),
                                (self.correct_trace().replace('ins=20', 'ins=24'), self.storage(3)),
                                (self.correct_trace() + '\nSIM status ins=20 sw=9000 chv=3/3 puk=10/10 enabled=1', self.storage(3))):
                with self.subTest(trace=trace), self.assertRaises(ValueError):
                    check_correct(trace, data, Path('.'))

    def test_disabled_changed_or_truncated_card_rejected(self):
        for offset, value in ((CHV_OFFSET, ord('9')), (ENABLED_OFFSET, 0), (ATTEMPTS_OFFSET + 2, 9)):
            data = bytearray(self.storage(2))
            data[offset] = value
            with self.subTest(offset=offset), self.assertRaises(ValueError):
                check_card(data, 2)
        with self.assertRaises(ValueError):
            check_card(self.storage(2)[:-1], 2)


if __name__ == '__main__':
    unittest.main()
