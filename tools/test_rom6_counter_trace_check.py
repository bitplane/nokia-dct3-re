import unittest

from tools.rom6_counter_trace_check import check


def capture(reference=False):
    lines = []
    for i in range(64):
        if reference:
            time = (i // 4) * 0.001 + (0, 8, 22, 36)[i % 4] / 45_500_000
            value = (i // 4) * 100 + (0, 1, 1, 1)[i % 4]
            pc = 0x3eea if i % 4 == 0 else 0x3ef0
        else:
            time = i * 14 / 13_000_000
            value = i * 14 // 12
            pc = 0x3ef0
        lines.append(f'rom6_reset_policy_counter: sample={i+1} t={time:.12f} '
                     f'value={value:04x} pc={pc:06x} observer_writes=0')
    lines.append('native_dsp_terminal_timing: released=1 clock='
                 + str(45_500_000 if reference else 13_000_000))
    return '\n'.join(lines)


class CounterTimingTest(unittest.TestCase):
    def test_stalled_retry_and_reference_are_distinct_diagnostics(self):
        self.assertEqual(check(capture())['retry_pairs'], 63)
        self.assertGreater(check(capture(True), reference=True)['equal_adjacent_values'], 0)
        self.assertFalse(check(capture())['fitted_clock_claim'])
        with self.assertRaises(ValueError):
            check(capture(True))
        with self.assertRaises(ValueError):
            check(capture(), reference=True)

    def test_incomplete_duplicate_or_unknown_consumer_rejected(self):
        text = capture()
        for changed in (text.split('\n', 1)[1], text + '\n' + text,
                        text.replace('pc=003ef0', 'pc=003eee'),
                        text.replace('clock=13000000', 'clock=45500000')):
            with self.subTest(changed=changed[:80]), self.assertRaises(ValueError):
                check(changed)

    def test_wrong_timing_and_equal_retry_rejected(self):
        text = capture()
        for changed in (text.replace('t=0.000001076923', 't=0.000001176923'),
                        text.replace('value=0001', 'value=0000')):
            with self.subTest(changed=changed[:150]), self.assertRaises(ValueError):
                check(changed)


if __name__ == '__main__':
    unittest.main()
