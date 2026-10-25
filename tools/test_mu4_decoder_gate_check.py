import unittest
import re

from tools.mu4_decoder_gate_check import (check, check_return_routes, check_activity_cycles,
                                         check_counter_producers, check_counter_pointers, check_context_callers,
                                         check_exchange_status, SITES)


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


def activity_fixture(held=0,count=1):
    text = return_fixture(0)
    after = count-int(not held)
    events = [
        f'mu4_decoder_gate: address=0293b5 opcode=f845 call=375 a={held|count:010x} '
        f'ar1={held:04x} activity={held:04x} context=d796 context_activity={count:04x} '
        'identity=Execute_entry operands=context_only observer_writes=0',
        'mu4_decoder_gate: address=0293b7 opcode=10f8 call=375 a=0000000001 '
        f'ar1={held:04x} activity={held:04x} context=d796 context_activity={count:04x} '
        'identity=Execute_entry operands=context_only observer_writes=0']
    for line in text.splitlines():
        if not line.startswith('mu4_decoder_gate:'):
            continue
        flag = 0 if 'address=0293fe' in line or 'address=02940b' in line else 1
        events.append(line+f' activity={flag:04x} context=d796 context_activity={after:04x}')
    for i,line in enumerate(events,1):
        line = re.sub(r' ordinal=\d+| t=[0-9.]+','',line)
        events[i-1] = line+f' ordinal={i} t={9.65+i*.001:.3f}'
    outer = [line for line in text.splitlines() if not line.startswith('mu4_decoder_gate:')]
    outer[2] = outer[2].replace('events=6',f'events={len(events)}')
    return '\n'.join(outer[:2]+events+outer[2:])


def counter_fixture():
    return activity_fixture()+'\n'+'\n'.join([
        'mu4_context_count_read: ordinal=1 address=d7ac instruction=0201ca opcode=6b83 word=0000 mask=ffff t=9.6505 identity=Access_bus observer_writes=0',
        'mu4_context_count_read: ordinal=2 address=d7ac instruction=020182 opcode=6b82 word=0001 mask=ffff t=9.6525 identity=Access_bus observer_writes=0',
        'mu4_context_count_terminal: reads=2 capped=0 address=d7ac first=9.64 last=9.86 t=10 identity=Access_bus observer_writes=0'])


def caller_fixture():
    return counter_fixture()+'\n'+'\n'.join([
            'mu4_context_pointer_read: ordinal=1 address=d79e instruction=0201c7 opcode=7192 word=d7ac mask=ffff t=9.6504 identity=Access_bus observer_writes=0',
            'mu4_context_pointer_terminal: reads=1 capped=0 first=9.64 last=9.86 t=10 identity=Access_bus observer_writes=0',
            'mu4_context_call: ordinal=1 address=0204bc opcode=f982 kind=call a=000000d796 t=9.6501 identity=Execute_entry operands=context_only observer_writes=0',
            'mu4_context_call: ordinal=2 address=02046b opcode=f6be kind=entry a=000000d796 t=9.6502 identity=Execute_entry operands=context_only observer_writes=0',
            'mu4_context_call: ordinal=3 address=020477 opcode=f4e4 kind=return a=0000000000 t=9.6507 identity=Execute_entry operands=context_only observer_writes=0',
            'mu4_context_call_terminal: events=3 bad=0 capped=0 first=9.64 last=9.86 t=10 identity=Execute_entry operands=context_only observer_writes=0'])


class DecoderGateTests(unittest.TestCase):
    def test_exchange_status_owned_read_and_both_successors(self):
        text = caller_fixture().replace('address=0204bc','address=020581')
        for ordinal,(when,value) in enumerate(((9.649,0),(9.65001,1)),1):
            text += f'\nmu4_exchange_status_read: ordinal={ordinal} address=13e9 instruction=02a310 opcode=10e1 word={value:04x} mask=ffff t={when} identity=Access_bus observer_writes=0'
        sequence = [(0x20553,9.64901,0),(0x20586,9.64902,0),
                    (0x20553,9.65002,1),(0x20555,9.65003,1),
                    (0x20580,9.65004,1),(0x20586,9.6508,1)]
        for ordinal,(address,when,value) in enumerate(sequence,1):
            opcode = 0xf845 if address==0x20553 else 0x4811 if address==0x20580 else 0x10f8
            text += f'\nmu4_exchange_branch: ordinal={ordinal} address={address:06x} opcode={opcode:04x} receiver=13e0 table=c6f0 field9={value:04x} field13=0001 context=d796 count22=0000 flag1310=0000 t={when} identity=Execute_entry operands=context_only observer_writes=0'
        text += '\nmu4_exchange_terminal: reads=2 events=6 bad=0 capped=0 first=9.64 last=9.86 t=10 identity=Access_bus_and_Execute_entry observer_writes=0'
        report = check_exchange_status(text)['exchange_status']
        self.assertEqual(report['outcomes'],{'skip':1,'publish':1})
        self.assertEqual(report['publish_flags'],{'0000':1})
        self.assertEqual(report['last_test']['word'],'0001')
        self.assertEqual(report['last_test']['context_snapshots'],dict(field13='0001',count22='0000',flag1310='0000'))
        for before,after in (('instruction=02a310','instruction=02a311'),('address=13e9','address=13ea'),
                             ('table=c6f0','table=c738'),('field9=0001','field9=0000'),
                             ('events=6','events=5'),('address=020555','address=020580'),
                             ('t=9.65004','t=9.65011'),('t=9.6508','t=9.6505'),
                             ('opcode=10e1','opcode=10e2'),('capped=0','capped=1')):
            with self.subTest(before=before),self.assertRaises(ValueError):
                check_exchange_status(text.replace(before,after))

    def test_context_caller_interval_and_pointer_agreement(self):
        text = caller_fixture()
        result = check_context_callers(text)['context_callers']
        self.assertEqual(result['matched_calls'],1)
        self.assertEqual(result['calls'],[dict(caller='0204bc',wrapper='02046b',context='d796',count=1)])
        self.assertEqual(result['target_owners'],[dict(caller='0204bc',wrapper='02046b',context='d796',reads=1)])
        for before,after in (('address=0204bc','address=0205f3'),('address=020477','address=020460'),
                             ('a=000000d796','a=000000d797'),('events=3','events=2'),
                             ('t=9.6507','t=9.6503'),('opcode=f6be','opcode=f6bf'),
                             ('bad=0','bad=1'),('capped=0','capped=1')):
            with self.subTest(before=before),self.assertRaises(ValueError):
                check_context_callers(text.replace(before,after))
        with self.assertRaises(ValueError):
            check_context_callers('\n'.join(line for line in text.splitlines() if 'kind=return' not in line).replace('events=3','events=2'))
        outer_call = 'mu4_context_call: ordinal=1 address=0293c1 opcode=f982 kind=call a=000000d796 t=9.65001 identity=Execute_entry operands=context_only observer_writes=0'
        outer_entry = outer_call.replace('ordinal=1 address=0293c1 opcode=f982 kind=call','ordinal=2 address=02044a opcode=f6be kind=entry').replace('t=9.65001','t=9.65002')
        outer_return = outer_call.replace('ordinal=1 address=0293c1 opcode=f982 kind=call','ordinal=6 address=020453 opcode=f4e4 kind=return').replace('t=9.65001','t=9.6508')
        nested = text
        for old,new in ((3,5),(2,4),(1,3)):
            nested = nested.replace(f'mu4_context_call: ordinal={old} ',f'mu4_context_call: ordinal={new} ')
        nested = '\n'.join([outer_call,outer_entry,nested.replace('events=3','events=6'),outer_return])
        self.assertEqual(check_context_callers(nested)['context_callers']['matched_calls'],2)

    def test_counter_pointer_access_identity_and_pairing(self):
        text = counter_fixture()+'\n'+'\n'.join([
            'mu4_context_pointer_read: ordinal=1 address=d79e instruction=0201c7 opcode=7192 word=d7ac mask=ffff t=9.6504 identity=Access_bus observer_writes=0',
            'mu4_context_pointer_terminal: reads=1 capped=0 first=9.64 last=9.86 t=10 identity=Access_bus observer_writes=0'])
        self.assertEqual(check_counter_pointers(text)['counter_pointers']['sources'],{'d79e':1})
        other = ('mu4_context_pointer_read: ordinal=2 address=d7c0 instruction=0201c7 opcode=7192 word=d7ce mask=ffff t=9.651 identity=Access_bus observer_writes=0')
        expanded = text.replace('reads=1 capped=0','reads=2 capped=0')+'\n'+other
        result = check_counter_pointers(expanded)['counter_pointers']
        self.assertEqual((result['reads'],result['target_reads']),(2,1))
        for before,after in (('opcode=7192','opcode=7193'),('word=d7ac','word=d7ab'),
                             ('address=d79e','address=0013'),('t=9.6504','t=9.6506'),
                             ('reads=1 capped=0','reads=2 capped=0'),('capped=0','capped=1'),
                             ('observer_writes=0','observer_writes=1')):
            with self.subTest(before=before),self.assertRaises(ValueError):
                check_counter_pointers(text.replace(before,after))
        with self.assertRaises(ValueError):
            check_counter_pointers(counter_fixture())

    def test_counter_producers_require_actual_read_ownership(self):
        result = check_counter_producers(counter_fixture())['counter_reads']
        self.assertEqual(result['total'],2)
        self.assertEqual(result['increment_reads'],1)
        self.assertEqual(result['increment_before_values'],{0:1})
        self.assertEqual(result['decrement_reads'],1)
        self.assertIn('not write ownership',result['scope'])

    def test_counter_producers_reject_censored_or_changed_owner(self):
        text = counter_fixture()
        for before,after in (('instruction=0201ca','instruction=0001ca'),
                             ('opcode=6b83','opcode=6b82'), ('word=0000','word=0001'),
                             ('word=0001','word=0002'), ('mask=ffff','mask=fffe'),
                             ('reads=2','reads=1'),('capped=0','capped=1'),
                             ('t=9.6525','t=9.63'),('identity=Access_bus','identity=Execute_entry'),
                             ('address=d7ac','address=d7ab')):
            with self.subTest(before=before),self.assertRaises(ValueError):
                check_counter_producers(text.replace(before,after))
        with self.assertRaises(ValueError):
            check_counter_producers(activity_fixture())

    def test_activity_cycles_follow_acquisition_and_zero_release(self):
        for held,count in ((0,1),(0,2),(1,0)):
            with self.subTest(held=held,count=count):
                cycles = check_activity_cycles(activity_fixture(held,count))['activity_cycles']
                self.assertEqual(cycles,[dict(ordinal=1,context='d796',before_flag=held,
                    before_count=count,call_flag=1,call_count=count-int(not held),
                    refresh_flag=0,refresh_count=count-int(not held))])

    def test_activity_cycles_reject_changed_context_and_state_agreement(self):
        text = activity_fixture()
        for address,before,after in (
                ('0293b5','context_activity=0001','context_activity=0000'),
                ('0293cd',' activity=0001',' activity=0000'),
                ('0293cd','context_activity=0000','context_activity=0001'),
                ('0293cd','context=d796','context=d797'),
                ('0293fe',' activity=0000',' activity=0001')):
            lines = text.splitlines()
            index = next(i for i,line in enumerate(lines) if f'address={address}' in line)
            lines[index] = lines[index].replace(before,after)
            with self.subTest(address=address,before=before),self.assertRaises(ValueError):
                check_activity_cycles('\n'.join(lines))
        with self.assertRaises(ValueError):
            check_activity_cycles(return_fixture(0))

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
