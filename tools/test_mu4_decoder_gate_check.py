import unittest

from tools.mu4_decoder_gate_check import check, check_return_routes, SITES


def fixture():
    return '\n'.join([
        'mu4_playback_execute_entry: address=026d62 opcode=fd00 t=0.2',
        'mu4_playback_execute_entry: address=026d62 opcode=fd00 t=9.9',
        'mu4_decoder_gate: ordinal=1 address=0293ae opcode=f830 call=375 t=9.65 '
        'identity=Execute_entry operands=context_only observer_writes=0',
        'mu4_decoder_gate: ordinal=2 address=0293b0 opcode=7211 call=375 t=9.66 '
        'identity=Execute_entry operands=context_only observer_writes=0',
        'mu4_decoder_gate_terminal: events=2 bad=0 capped=0 first=9.64 last=9.86 t=10 '
        'identity=Execute_entry operands=context_only observer_writes=0',
        'mu4_playback_parent_terminal: first_session_complete=1 overlay_verified=1 illegal=0 '
        'opcode_mismatch=0 selection=first_verified_aa99_session identity=Execute_entry '
        'audio_acceptance=0 t=10 observer_writes=0'])


def return_fixture(value, prefix=False):
    addresses = [0x293cd, 0x293cf, 0x293df]
    addresses += [0x293e1] if value == 0 else [0x293f1]
    if value == 2:
        addresses += [0x293f4]
    addresses += [0x293fe, 0x2940b]
    if prefix:
        addresses = [0x293cf,0x293df,0x293e1,0x293fe,0x2940b]+addresses
    lines = [line for line in fixture().splitlines() if not line.startswith('mu4_decoder_gate:')]
    events = []
    for ordinal,address in enumerate(addresses, 1):
        accumulator = 0xffffffffff if value == 0xffff else value
        if address not in (0x293cd,0x293cf):
            accumulator = 0x1234
        events.append(f'mu4_decoder_gate: ordinal={ordinal} address={address:06x} '
                      f'opcode={SITES[address]:04x} call={375 if address == 0x293cd else 376} '
                      f'a={accumulator:010x} ar1={value:04x} t={9.65+ordinal*.001:.3f} '
                      'identity=Execute_entry operands=context_only observer_writes=0')
    lines[2] = lines[2].replace('events=2',f'events={len(events)}')
    return '\n'.join(lines[:2]+events+lines[2:])


class DecoderGateTests(unittest.TestCase):
    def test_return_routes_use_retained_result_and_actual_successors(self):
        for value,route in ((0,'result_zero_group'),(2,'result_two_group'),
                            (1,'refresh_only'),(0xffff,'refresh_only')):
            with self.subTest(value=value):
                report = check_return_routes(return_fixture(value))
                self.assertEqual(report['return_routes'], [dict(ordinal=1,result=f'{value:04x}',
                    route=route,processing_before=375,processing_after=376)])
                self.assertEqual(report['partial_prefix_route_events'],0)
        report = check_return_routes(return_fixture(0,prefix=True))
        self.assertEqual(len(report['return_routes']),1)
        self.assertEqual(report['partial_prefix_route_events'],4)

    def test_return_routes_reject_missing_reordered_and_wrong_retention(self):
        text = return_fixture(0xffff)
        for before,after in (('ar1=ffff','ar1=0000'),('a=ffffffffff','a=10000000000'),
                             ('address=0293f1 opcode=6ce1','address=0293e1 opcode=76f8'),
                             ('address=0293fe opcode=10f8','address=0293f4 opcode=10f8')):
            with self.subTest(before=before),self.assertRaises(ValueError):
                check_return_routes(text.replace(before,after))
        for address in (0x293cf,0x293df,0x293f1,0x293fe,0x2940b):
            lines = [line for line in text.splitlines() if f'address={address:06x}' not in line]
            ordinal = 0
            for index,line in enumerate(lines):
                if line.startswith('mu4_decoder_gate:'):
                    ordinal += 1
                    lines[index] = line.replace(line.split()[1],f'ordinal={ordinal}',1)
            text_missing = '\n'.join(lines).replace('events=6','events=5')
            with self.subTest(missing=hex(address)),self.assertRaises(ValueError):
                check_return_routes(text_missing)
        with self.assertRaises(ValueError):
            check_return_routes(fixture())

    def test_successor_not_live_operands_defines_outcome(self):
        self.assertEqual(check(fixture())['branches'][0]['outcome'], 'pass')
        self.assertEqual(check(fixture().replace('address=0293b0 opcode=7211',
                                                'address=02940b opcode=10f8'))['branches'][0]['outcome'], 'skip')

    def test_rejects_mutated_or_incomplete_evidence(self):
        for before, after in [('bad=0', 'bad=1'), ('capped=0', 'capped=1'),
                              ('events=2', 'events=3'), ('opcode=f830', 'opcode=0000'),
                              ('ordinal=2', 'ordinal=3'), ('t=9.66', 't=9.63'),
                              ('t=9.9', 't=9.8'), ('illegal=0', 'illegal=1'),
                              ('operands=context_only', 'operands=retired'),
                              ('observer_writes=0', 'observer_writes=1'),
                              ('address=0293b0 opcode=7211', 'address=0293cd opcode=f982')]:
            with self.subTest(before=before, after=after), self.assertRaises(ValueError):
                check(fixture().replace(before, after))

    def test_rejects_censored_branch(self):
        text = fixture().replace('events=2', 'events=1')
        text = '\n'.join(line for line in text.splitlines() if 'ordinal=2' not in line)
        with self.assertRaisesRegex(ValueError, 'censored'):
            check(text)


if __name__ == '__main__':
    unittest.main()
