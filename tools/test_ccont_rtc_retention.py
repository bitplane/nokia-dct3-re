import unittest
from tools.run_ccont_rtc_retention import verify


def sample():
    return ('ccont_rtc: event=counter_write reg=07 data=00\n'
            'ccont_rtc: event=read reg=0b data=01\n'
            'ccont_rtc: event=read reg=0c data=0c\n' + ''.join(
                f'ccont_rtc: event=second time=12:01:0{s} day=1\n'
                for s in range(1, 4)))


class RetentionTest(unittest.TestCase):
    def test_exact_retained_minute_and_alarm(self):
        verify(sample(), bytes((3, 1, 12, 1, 1, 12, 0, 0x50, 0)))

    def test_defaults_rewrites_stale_alarm_and_missing_ticks_fail(self):
        for text in (sample().replace('12:01:', '12:00:'),
                     sample().replace('day=1', 'day=0'),
                     sample().replace('reg=0c data=0c', 'reg=0c data=00'),
                     sample().replace('time=12:01:03', 'time=12:01:04'),
                     sample() + 'ccont_rtc: event=counter_write reg=08 data=01\n',
                     sample() + '[LUA ERROR]\n'):
            with self.subTest(text=text), self.assertRaises(ValueError):
                verify(text, bytes((3, 1, 12, 1, 1, 12, 0, 0x50, 0)))

    def test_truncated_or_default_seed_fails(self):
        for stored in (b'', b'\x03\x01\x0c', bytes((0, 0, 12, 1))):
            with self.subTest(stored=stored), self.assertRaises(ValueError):
                verify(sample(), stored)


if __name__ == '__main__':
    unittest.main()
