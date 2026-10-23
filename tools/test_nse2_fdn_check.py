import unittest

from tools.nse2_fdn_check import check_phase, check_record, check_dial, FDN_RECORD, FDN_EDITED_RECORD
from tools.sim_fdn_profile import extend, expected_validity_card


def fixture(phase):
    before = bytearray(extend(bytes(3570), True))
    before[3575] = int(phase in (0, 3))
    rows = []
    if phase in (1, 2):
        commands = [(0x44, 0, 0, fid, 4.4 + i * .1)
                    for i, fid in enumerate((0x6f7e, 0x6f07))]
    else:
        commands = []
    if phase in (0, 2):
        commands += [(0x20, 2, 8, 0x6f3b, 24.2)]
        commands += ([(0x04, 0, 0, 0x6f3a, 24.3),
                      (0x44, 0, 0, 0x6f7e, 24.4),
                      (0x44, 0, 0, 0x6f07, 24.5)] if phase == 0 else
                     [(0x44, 0, 0, 0x6f3a, 24.3)])
    for ins, p2, size, fid, when in commands:
        rows += [f'sim_device: header cla=a0 ins={ins:02x} p1=00 p2={p2:02x} p3={size:02x} selected={fid:04x} t={when}',
                 f'SIM status ins={ins:02x} sw=9000 chv=3/3 puk=10/10 enabled=0']
    for i in range(34 if phase in (0, 2) else 18):
        when = 18.5 + (i // 2) * .35 + (i % 2) * .15
        rows.append(f'nse2_fdn_contact: event={i+1} down={1-i%2} t={when:.3f} physical_only=1')
    return '\n'.join(rows), bytes(before), expected_validity_card(before, phase)


class FdnCheckTest(unittest.TestCase):
    def test_local_dial_card_and_security_contrast(self):
        for enabled in (False,True):
            before=bytearray(extend(bytes(3570),True))
            before[3575]=int(not enabled)
            before[3578:3610]=FDN_EDITED_RECORD
            rows=[]
            if enabled:
                for i,fid in enumerate((0x6f7e,0x6f07)):
                    rows.extend((f'sim_device: header cla=a0 ins=44 p1=00 p2=00 p3=00 selected={fid:04x} t={4.4+i*.1}',
                                 'SIM status ins=44 sw=9000 chv=3/3 puk=10/10 enabled=0'))
            for i in range(12):
                when=18.5+(i//2)*.35+(i%2)*.15
                rows.append(f'nse2_fdn_dial_contact: event={i+1} down={1-i%2} t={when:.3f} physical_only=1')
            text='\n'.join(rows)
            self.assertTrue(check_dial(text,before,before,enabled=enabled)['card_unchanged'])
            bad=bytearray(before)
            bad[3500]^=1
            for trace,after in ((text,bad),(text.replace('event=1 ','event=2 ',1),before),
                                 (text+'\nsim_device: header cla=a0 ins=20 p1=00 p2=02 p3=08 selected=6f3b t=20',before)):
                with self.assertRaises(ValueError):
                    check_dial(trace,before,after,enabled=enabled)
            with self.assertRaises(ValueError):
                check_dial(text,before,before,enabled=not enabled)

    def test_record_storage_protocol_and_physical_input(self):
        before=bytearray(extend(bytes(3570),True))
        before[3517]=3
        for mode,count in (('write',54),('read',30),('wrong',38),('edit',50),('read-edited',30),
                           ('erase',46),('cancel',46),('empty',28)):
            initial=bytearray(before)
            reading=mode in ('read','read-edited')
            if reading or mode in ('edit','erase','cancel'):
                initial[3578:3610]=FDN_EDITED_RECORD if mode in ('read-edited','erase','cancel') else FDN_RECORD
            final=bytearray(initial)
            rows=[]
            if not reading and mode!='empty':
                when=25.6 if mode in ('edit','erase','cancel') else 24.9
                rows.extend((f'sim_device: header cla=a0 ins=20 p1=00 p2=02 p3=08 selected=6f3b t={when}',
                             'SIM status ins=20 sw='+('9804 chv=3/2' if mode=='wrong' else '9000 chv=3/3')+' puk=10/10 enabled=0'))
            if mode in ('write','edit','erase'):
                final[3578:3610]=b'\xff'*32 if mode=='erase' else FDN_EDITED_RECORD if mode=='edit' else FDN_RECORD
                when=26.3 if mode=='erase' else 27.7
                rows.extend((f'sim_device: header cla=a0 ins=dc p1=01 p2=04 p3=20 selected=6f3b t={when}',
                             'SIM status ins=dc sw=9000 chv=3/3 puk=10/10 enabled=0'))
            elif reading:
                rows.append('sim_device: header cla=a0 ins=b2 p1=01 p2=04 p3=20 selected=6f3b t=23.1')
            elif mode=='wrong':
                final[3517]=2
            for i in range(count):
                duration=4 if reading and i==count-1 else .15
                when=18.5+(i//2)*.35+(duration if i%2 else 0)
                rows.append(f'nse2_fdn_record_contact: event={i+1} down={1-i%2} t={when:.3f} physical_only=1')
            text='\n'.join(rows)
            self.assertEqual(check_record(text,initial,final,mode)['mode'],mode)
            corrupt=bytearray(final)
            corrupt[10]^=1
            bad_cases=[(text,corrupt),(text.replace('event=1 ','event=2 ',1),final)]
            if mode!='empty':
                bad_cases.append((text.replace('selected=6f3b','selected=6f3a'),final))
            if mode=='erase':
                bad_cases.append((text.replace('t=25.6','t=26.4'),final))
            for bad_text,bad_card in bad_cases:
                with self.assertRaises(ValueError):
                    check_record(bad_text,initial,bad_card,mode)

    def test_all_four_phases(self):
        for phase in range(4):
            with self.subTest(phase=phase):
                self.assertEqual(check_phase(*fixture(phase), phase)['fdn_enabled'], phase < 2)

    def test_rejects_unrelated_mutation(self):
        text, before, after = fixture(0)
        bad = bytearray(after)
        bad[3500] ^= 1
        with self.assertRaises(ValueError):
            check_phase(text, before, bad, 0)

    def test_rejects_bad_protocol_and_contacts(self):
        text, before, after = fixture(0)
        for bad in (text.replace('p2=02', 'p2=01'),
                    text.replace('sw=9000', 'sw=9804', 1),
                    text.replace('selected=6f3a', 'selected=6f3b'),
                    text.replace('event=34', 'event=33'),
                    text.replace('t=24.2', 't=3.2', 1),
                    text.replace('physical_only=1', 'physical_only=0', 1),
                    text.replace('chv=3/3', 'chv=3/2', 1)):
            with self.subTest(bad=bad[-100:]), self.assertRaises(ValueError):
                check_phase(bad, before, after, 0)

    def test_cold_phase_rejects_extra_verify(self):
        text, before, after = fixture(3)
        extra = fixture(0)[0].split('nse2_fdn_contact:')[0]
        with self.assertRaises(ValueError):
            check_phase(text + '\n' + extra, before, after, 3)


if __name__ == '__main__':
    unittest.main()
