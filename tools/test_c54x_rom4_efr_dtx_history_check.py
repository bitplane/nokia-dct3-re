import unittest

from tools.c54x_rom4_efr_dtx_history_check import check_trace


def fixture(flags=0x401):
    selected = bool(flags & 0x400)
    value = int(bool(flags & 0x100))
    rows = [(0xc78f, 1, 0, 7)]
    if selected:
        rows += [(0xc793, 1, 0, 7), (0xc79b, 1, 5, value)]
    rows += [(0xc79d, value if selected else 1, 5 if selected else 0, value)]
    group = ''.join(f'efr_dtx_history: pc={pc:04x} flags={flags:04x} history={history:04x} state={state:04x} st0=0013 a={a:08x}\n'
                    for pc, history, state, a in rows)
    return group * 2


class HistoryTests(unittest.TestCase):
    def test_clear_and_set_previous_muting(self):
        for flags in (0x401, 0x501):
            self.assertEqual(check_trace(fixture(flags), frames=1), 1)

    def test_skip(self):
        self.assertEqual(check_trace(fixture(1), frames=1), 0)

    def test_wrong_dp(self):
        with self.assertRaisesRegex(ValueError, '09c0'):
            check_trace(fixture().replace('st0=0013', 'st0=0014'), frames=1)

    def test_missing_replay(self):
        with self.assertRaisesRegex(ValueError, 'original/replay'):
            check_trace(fixture(), frames=2)

    def test_wrong_store(self):
        with self.assertRaisesRegex(ValueError, 'publication'):
            check_trace(fixture().replace('a=00000000', 'a=00000001'), frames=1)

    def test_malformed(self):
        with self.assertRaisesRegex(ValueError, 'malformed'):
            check_trace('efr_dtx_history: broken', frames=1)
