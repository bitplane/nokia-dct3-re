import unittest

from tools import nsm1_factory_tone_check as checker


def trace():
    rows=['nsm1_tone_boundary_summary: start=85 end=91.5 mcu_early=70528 mcu=18 dsp=10 io=2854 compiler_early=20 compiler=18 observer_writes=0',
          'nsm1_factory_tone_constructor: mode=0 tone=a caller=2965a1 t=86.065000000 observer_writes=0',
          'nsm1_factory_tone_admission: descriptor=002db038 enabled=01 setting=04 task=06 t=86.066000000 observer_writes=0']
    rows.extend(f'nsm1_tone_boundary_early: address=00010000 value=00000001 mask=ffff0000 t={.01+i*.001:.9f} observer_writes=0' for i in range(8))
    for i,field in enumerate((8,9,0x28,0x2d,0x2e,8,9,0x25)):
        rows.append(f'nsm1_tone_boundary_compiler: field={field:08x} value=00000002 commit=00000000 caller=002a2c29 t={.1+i*.001:.9f} observer_writes=0')
    times=[86.02+i*.0001 for i in range(5)]+[86.123+i*.0001 for i in range(2)]+[
        86.124+i*.0001 for i in range(4)]+[86.363+i*.0001 for i in range(2)]+[86.417+i*.0001 for i in range(5)]
    for (field,value),t in zip(checker.FIELDS,times):
        commit=field==0x2e
        rows.append(f'nsm1_tone_boundary_compiler: field={field:08x} value={value:08x} commit={int(commit):08x} caller={0x2a2c51 if commit else 0x2a2c29:08x} t={t:.9f} observer_writes=0')
        address,shift={9:(0x100a8,16),0x25:(0x100ac,16),0x20:(0x100ac,0),0x24:(0x100b4,0),0x2e:(0x100e0,16)}[field]
        published=0x9200|(value&255) if field==9 else 1 if commit else value
        rows.append(f'nsm1_tone_boundary_mcu: address={address:08x} canonical={address:08x} value={published<<shift:08x} mask={65535<<shift:08x} pc={0x2a4d3a if commit else 0x2a4d2c:08x} lr=002a4a7f t={t+.00002:.9f} observer_writes=0')
        if commit:
            for j,(address,value,pc) in enumerate(((0x866,2,0x3775),(0x870,0,0x37a1))):
                rows.append(f'nsm1_tone_boundary_dsp: address={address:04x} value={value:04x} mask=ffff pc={pc:04x} t={t+.00003+j*.00001:.9f} observer_writes=0')
    for address,value,count in ((0x21,0xe82,1408),(0x21,0x682,1408),(0,0x9d,19),
                                (0x2c,9,5),(0x2c,0x18,3),(0x2d,0x626,3),
                                (0x2c,8,3),(0x2d,0x20c,2),(0x2d,0x20d,2),(0x2d,0x20f,1)):
        rows.extend(f'nsm1_tone_boundary_io: address={address:04x} value={value:04x} mask=ffff pc=47a1 t=87.000000000 observer_writes=0' for _ in range(count))
    return '\n'.join(rows)


class FactoryTonePublicationTest(unittest.TestCase):
    def test_serial_release_is_not_sample_output_or_sibling_word_format(self):
        rows=['nsm1_factory_wave_summary: early=17 late=6 control=c8cc extension=0000 observer_writes=0']
        early=[('0022','c008','0e22'),('0022','c0c8','0e24'),
               ('0021','0aaa','0e31'),('0021','ffd5','358b')]+[('0022','c008','3021')]*12
        rows.extend(f'nsm1_factory_wave_data: phase=early address={a} value={v} mask=ffff pc={p} t=0.250000000 observer_writes=0' for a,v,p in early)
        rows.extend(f'nsm1_factory_wave_data: phase=late address=0022 value={v} mask=ffff pc={p} t=86.123000000 observer_writes=0' for _ in range(3) for v,p in (('c00c','a4a1'),('c0cc','a4a3')))
        good='\n'.join(rows)
        checker.check_serial(good)
        for bad in (good.replace('c0cc','c0c8'),good.replace('control=c8cc','control=c8c8'),
                    good.replace('phase=late address=0022','phase=late address=0021'),
                    good.replace('value=0aaa','value=0000'),good.replace('late=6','late=0'),
                    good+'\nnsm1_factory_wave_data: malformed'):
            with self.subTest(tail=bad[-80:]),self.assertRaises(ValueError):
                checker.check_serial(bad)

    def test_complete_projected_publication_and_native_acknowledgements(self):
        checker.check_publication(trace())

    def test_click_alone_wrong_tone_missing_ack_or_bad_lane_are_rejected(self):
        good=trace()
        for bad in (good.replace('enabled=01','enabled=00'),
                    good.replace('tone=a','tone=0'),
                    good.replace('value=00001b58','value=00000e10'),
                    good.replace('mask=0000ffff','mask=ffff0000'),
                    good.replace('address=0866','address=0867'),
                    good.replace('value=020d','value=020c'),
                    good.replace('mcu=18','mcu=0'),
                    good.replace('address=00010000','address=00100000'),
                    good.replace('t=86.066000000','t=86.125000000'),
                    good+'\nnsm1_factory_tone_constructor: malformed',
                    good+'\nnsm1_tone_boundary_dsp: malformed'):
            with self.subTest(tail=bad[-80:]),self.assertRaises(ValueError):
                checker.check_publication(bad)


if __name__=='__main__':
    unittest.main()
