import copy
import unittest

from tools.mu4_restart_tail_check import check_restart


def fixture(mode=0, track=1, count=1):
    increment = (track+1) & 0xffff
    wrap = increment == (count+1) & 0xffff
    sites = ([('03a98f', track), ('03a9a2', increment)] +
             ([('03a9a5', increment)] if wrap else []) +
             [('03a9af', 1 if wrap else increment)] if mode != 3 else
             [('03a98f', track), ('03a9aa', track), ('03a9af', track)])
    lines = ['mu4_restart_scope: identity=debugger_Execute_hook operands=context_only '
             'observer_writes=0 audio_acceptance=0']
    for ordinal, (address, value) in enumerate(sites, 1):
        lines.append(f'mu4_restart_context: ordinal={ordinal} address={address} cycles={100+ordinal} '
                     f'mode={mode:04x} track={value:04x} count={count:04x} lifecycle=0005 '
                     f'continuation=0006 ar2={(increment-count)&0xffff:04x} '
                     'identity=debugger_Execute_hook operands=context_only observer_writes=0')
    lines.append(f'mu4_restart_terminal: events={len(sites)} cap=32 observer_writes=0 audio_acceptance=0')
    snapshot = {'resident_selection': dict(mode=mode, track=track, count=count, continuation=2),
                'cycles': 100, 'lifecycle': '0004'}
    return '\n'.join(lines), snapshot


class RestartTailTests(unittest.TestCase):
    def test_one_track_wrap(self):
        result = check_restart(*fixture())
        self.assertTrue(result['wrap_observed'])
        self.assertEqual(result['source_model']['track'], 1)

    def test_nonwrap_and_nonadvance(self):
        self.assertEqual(check_restart(*fixture(track=1, count=3))['source_model']['track'], 2)
        held = check_restart(*fixture(mode=3, track=2, count=3))
        self.assertFalse(held['source_model']['index_advanced'])
        self.assertFalse(held['wrap_observed'])

    def test_reject_capture_state_and_order_changes(self):
        text, snapshot = fixture()
        for before, after in [('cap=32', 'cap=4'), ('events=4', 'events=3'),
                              ('address=03a9a5', 'address=03a9aa'),
                              ('cycles=102', 'cycles=100'),
                              ('continuation=0006', 'continuation=0004'),
                              ('ar2=0001', 'ar2=0000'), ('track=0002', 'track=0001'),
                              ('observer_writes=0', 'observer_writes=1')]:
            with self.subTest(before=before), self.assertRaises(ValueError):
                check_restart(text.replace(before, after), snapshot)
        changed = copy.deepcopy(snapshot)
        changed['resident_selection']['count'] = 0
        with self.assertRaises(ValueError):
            check_restart(text, changed)


if __name__ == '__main__':
    unittest.main()
