import copy
import unittest

from tools.mu4_restart_tail_check import check_restart, check_eof_mute


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


class EofMuteTests(unittest.TestCase):
    def fixture(self):
        registers = [0x97,0x97,0x79,0x79,0x0a,8,7,1,0x20,0]
        lines = ['mu4_playback_lifecycle_write: address=373e value=0005 t=10 observer_writes=0',
                 'mu4_playback_lifecycle_write: address=373e value=0004 t=11 observer_writes=0']
        controls = [(1,0x0810),(2,0x0a01),(3,0x1201),
                    (10_100_000_000,0x0580),(10_200_000_000,0x0a09),
                    (11_100_000_000,0x05f0),(11_200_000_000,0x0a01)]
        for ns, word in controls:
            address, value = word >> 9, word & 0x1ff
            registers[address] = value
            if address == 2 and value & 0x100:
                registers[3] = (registers[3] & 0x180) | (value & 0x7f)
            values = dict(power=6,analog=4,digital=5,format=7,rate=8,active=9,hp_left=2,hp_right=3)
            fields = ' '.join(f'{key}={registers[index]:03x}' for key,index in values.items())
            lines.append(f'mu4_codec_control: word={word:04x} ns={ns} {fields} '
                         'observer_writes=0 audio_acceptance=0')
        snapshot = {'file_control': dict(descriptor_bytes=144192,counter=144192,policy=1,short_input=1),
                    'pending_refill': 186}
        return '\n'.join(lines), snapshot

    def test_software_eof_mutes_then_reenables(self):
        result = check_eof_mute(*self.fixture())
        self.assertEqual(result['mute_observed_ns'], 10_200_000_000)
        self.assertEqual(result['reenabled_ns'], 11_200_000_000)

    def test_reject_wrong_eof_state(self):
        text, snapshot = self.fixture()
        for key, value in [('descriptor_bytes',144193),('counter',72096),('policy',0),('short_input',0)]:
            changed = copy.deepcopy(snapshot)
            changed['file_control'][key] = value
            with self.subTest(key=key), self.assertRaises(ValueError):
                check_eof_mute(text, changed)

    def test_reject_missing_or_misordered_mute(self):
        text, snapshot = self.fixture()
        for before, after in [('value=0005 t=10', 'value=0005 t=10.5'),
                              ('value=0004 t=11', 'value=0004 t=12'),
                              ('value=0005 t=10', 'value=0005 t=NaN'),
                              ('hp_right=000', 'hp_right=079')]:
            with self.subTest(before=before), self.assertRaises(ValueError):
                check_eof_mute(text.replace(before, after), snapshot)


if __name__ == '__main__':
    unittest.main()
