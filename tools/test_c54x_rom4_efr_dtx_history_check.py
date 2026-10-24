import unittest

from tools.c54x_rom4_efr_dtx_history_check import check_trace, check_lost_sid_failure


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
    def failure_fixture(self):
        console = '\n'.join(f'ROM4 EFR lost SID decoder comparison: frame={i} mismatches={86 if i == 97 else 0}' for i in range(98))
        console += '\nFatal error: TMS320C54x core conformance: native EFR decoder matches independent ETSI PCM\n'
        trace = ''.join(fixture(0x401 if i == 97 else 1) for i in range(98))
        for i in range(98):
            pair = ('efr_dtx_recovery: pc=28b0 flags=02c0 history=0001 bfi=0000 sid=0000\n'
                    'efr_dtx_recovery: pc=28f7 flags=0240 history=0000 bfi=0000 sid=0000\n') if i == 97 else (
                    'efr_dtx_recovery: pc=28b0 flags=0001 history=0000 bfi=0000 sid=0000\n'
                    'efr_dtx_recovery: pc=28f7 flags=0001 history=0000 bfi=0000 sid=0000\n')
            trace += pair * 2
        return console, trace

    def test_failure_prefix_is_not_conformance(self):
        console, trace = self.failure_fixture()
        result = check_lost_sid_failure(console, trace)
        self.assertFalse(result['conformance_claim'])
        self.assertFalse(result['complete_fixture_claim'])
        for bad_console, bad_trace in ((console.replace('mismatches=86', 'mismatches=0'), trace),
                                      (console, trace.replace('flags=0240', 'flags=02c0')),
                                      (console, trace.replace('flags=0401', 'flags=0501')),
                                      (console + 'PASS\n', trace)):
            with self.assertRaises(ValueError):
                check_lost_sid_failure(bad_console, bad_trace)

    def test_clear_and_set_previous_muting(self):
        for flags in (0x401, 0x501):
            self.assertEqual(check_trace(fixture(flags), frames=1), 1)

    def test_skip(self):
        self.assertEqual(check_trace(fixture(1), frames=1), 0)

    def test_previous_muting_requires_both_executed_alternatives(self):
        text = fixture(0x401) + fixture(0x501)
        self.assertEqual(check_trace(text, frames=2, require_previous_muting=True), 2)
        with self.assertRaisesRegex(ValueError, 'alternatives'):
            check_trace(fixture(0x401), frames=1, require_previous_muting=True)

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
