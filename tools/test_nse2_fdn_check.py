import unittest

from tools.nse2_fdn_check import check_phase, check_record, FDN_RECORD
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
    def test_record_storage_protocol_and_physical_input(self):
        before=bytearray(extend(bytes(3570),True))
        before[3517]=3
        for mode,count in (('write',54),('read',30),('wrong',38)):
            initial=bytearray(before)
            if mode=='read':
                initial[3578:3610]=FDN_RECORD
            final=bytearray(initial)
            rows=[]
            if mode!='read':
                rows.extend(('sim_device: header cla=a0 ins=20 p1=00 p2=02 p3=08 selected=6f3b t=24.9',
                             'SIM status ins=20 sw='+('9804 chv=3/2' if mode=='wrong' else '9000 chv=3/3')+' puk=10/10 enabled=0'))
            if mode=='write':
                final[3578:3610]=FDN_RECORD
                rows.extend(('sim_device: header cla=a0 ins=dc p1=01 p2=04 p3=20 selected=6f3b t=27.7',
                             'SIM status ins=dc sw=9000 chv=3/3 puk=10/10 enabled=0'))
            elif mode=='read':
                rows.append('sim_device: header cla=a0 ins=b2 p1=01 p2=04 p3=20 selected=6f3b t=23.1')
            else:
                final[3517]=2
            for i in range(count):
                duration=4 if mode=='read' and i==count-1 else .15
                when=18.5+(i//2)*.35+(duration if i%2 else 0)
                rows.append(f'nse2_fdn_record_contact: event={i+1} down={1-i%2} t={when:.3f} physical_only=1')
            text='\n'.join(rows)
            self.assertEqual(check_record(text,initial,final,mode)['mode'],mode)
            corrupt=bytearray(final)
            corrupt[10]^=1
            for bad_text,bad_card in ((text,corrupt),(text.replace('event=1 ','event=2 ',1),final),
                                      (text.replace('selected=6f3b','selected=6f3a'),final)):
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
