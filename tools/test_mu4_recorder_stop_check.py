import unittest

from tools.mu4_recorder_stop_check import check


def fixture():
    lines = ['mu4_recorder_stop_scope: first=0.8 last=15 cap=32768 boundary=changed_data_write identical_full_writes=deduplicated context_pc=not_owner observer_writes=0 audio_acceptance=0',
             'mu4_native_recorder_overlay: PASS marker=aa88 records=4622 words=73766 checked_ram_words=73762 excluded_register_words=4 before_execution=1',
             'mu4_native_recorder_control_entry: pc=0285dc pin_request=1']
    writes = [(0x5ed,0x120),(0x5ee,0x120),(0x5f0,0)]
    writes += [(address,1 if address == 0x83 else 0) for address in range(0x8f,0x80,-1)]
    writes += [(0x80,3),(0x5eb,1),(0x5f3,1),(0x80,0x8003),
               (0x5ed,0),(0x5ee,0),(0x5eb,0),(0xfd85,0x8009),(0xfd86,1),(0x373e,2)]
    for ordinal,(address,value) in enumerate(writes,1):
        lines.append(f'mu4_recorder_stop_write: ordinal={ordinal} bus_ordinal={ordinal} '
                     f'address={address:04x} value={value:04x} mask=ffff t={1+ordinal/1000:.9f} '
                     'context_pc=039000 observer_writes=0')
    lines.append(f'mu4_recorder_stop_terminal: events={len(writes)} bus_writes={len(writes)} '
                 'closed=1 capped=0 illegal=0 t=15.116666667 observer_writes=0 audio_acceptance=0')
    return '\n'.join(lines)


class RecorderStopTest(unittest.TestCase):
    def test_ordered_reset_precedes_separate_completion(self):
        result = check(fixture())
        self.assertEqual(result['witnesses']['reset_05ed']['previous'],0x120)
        self.assertEqual(result['witnesses']['reset_05ee']['previous'],0x120)
        self.assertLess(result['witnesses']['ack']['ordinal'],result['witnesses']['completion']['ordinal'])

    def test_rejects_scope_domains_missing_links_and_faults(self):
        text = fixture()
        for before,after in (('closed=1','closed=0'),('capped=0','capped=1'),('illegal=0','illegal=1'),
                ('last=15','last=16'),('mask=ffff','mask=ff00'),('bus_ordinal=2 ','bus_ordinal=1 '),
                ('address=0083 value=0001','address=0083 value=0002'),
                ('value=8003','value=8009'),('address=05f3 value=0001','address=05f3 value=0000'),
                ('address=05f0 value=0000','address=05f0 value=0001'),
                ('address=fd86 value=0001','address=fd86 value=0000'),
                ('context_pc=039000','context_pc=800000'),('pin_request=1','pin_request=0'),
                ('context_pc=not_owner','context_pc=retired'),('t=15.116666667','t=14.0'),
                ('t=1.002000000','t=1.000000000')):
            self.assertIn(before,text)
            with self.subTest(before=before), self.assertRaises(ValueError):
                check(text.replace(before,after))
        with self.assertRaises(ValueError):
            check(text+'\0')
        with self.assertRaises(ValueError):
            check(text+'\nLUA ERROR')

    def test_rejects_terminal_before_events(self):
        lines = fixture().splitlines()
        with self.assertRaises(ValueError):
            check('\n'.join([lines[-1]]+lines[:-1]))


if __name__ == '__main__':
    unittest.main()
