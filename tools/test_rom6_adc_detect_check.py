import unittest
from unittest.mock import patch

from tools.rom6_adc_detect_check import check,complex_products,normalized_metric,check_finite_search_acceptance,check_finite_peak_accumulation


class NativeComplexProductsTest(unittest.TestCase):
    def residual_fixture(self):
        program=bytearray(131072)
        for number in range(1,21):
            frames=(number//5)*51+(number%5)*10
            value=(frames*5000+16)//32
            address=0xd75+number
            program[2*address:2*address+2]=value.to_bytes(2,'big')
        text=('rom6_burst_fcch_accumulate: t=3.000000000 previous=0000 value=ffff a=0000001250 b=ffffffffff st1=6103 peak=193f base=06f0 index=ffef pc=000d0c observer_writes=0\n'
              'rom6_burst_fcch_accumulate: t=3.100000000 previous=ffff value=ffff a=000000186a b=ffffffffff st1=4103 peak=1f5a base=06f0 index=fff0 pc=000d0c observer_writes=0')
        return text,bytes(program)

    @patch('tools.rom6_adc_detect_check.check_consumer_program')
    def test_peak_accumulation_reads_table_contents_not_addresses(self,_):
        text,program=self.residual_fixture()
        result=check_finite_peak_accumulation(text,program,b'')
        self.assertEqual(result['last_residual'],-1)
        self.assertEqual(result['checked_updates'],2)
        self.assertFalse(result['physical_frame_phase_qualified'])

    @patch('tools.rom6_adc_detect_check.check_consumer_program')
    def test_peak_accumulation_rejects_stale_alias_and_arithmetic_drift(self,_):
        text,program=self.residual_fixture()
        for old,new in [('a=0000001250','a=0000000d78'),('b=ffffffffff','b=00000004d7'),
                        ('index=ffef','index=0000'),('value=ffff','value=04d7'),
                        ('t=3.100000000','t=2.900000000'),('pc=000d0c','pc=000c0c')]:
            with self.subTest(old=old),self.assertRaises(ValueError):
                check_finite_peak_accumulation(text.replace(old,new,1),program,b'')
        wrong=bytearray(program)
        wrong[2*0xd78:2*0xd78+2]=(4690).to_bytes(2,'big')
        with self.assertRaises(ValueError):
            check_finite_peak_accumulation(text,wrong,b'')

    def finite_fixture(self):
        lines=[]
        for index,frame in enumerate((642,652,663,673,683,693,703),1):
            tick=frame*60000+7800
            seconds,fraction=divmod(tick*1_000_000_000//13_000_000,1_000_000_000)
            lines.append(f'rom6_burst_fcch_state: sequence={index} t={seconds}.{fraction:09d} address=2f65 value={(index-7)&0xffff:04x} pc=000d27 reads={index*32} observer_writes=0')
        lines.append('rom6_burst_fcch_state: sequence=8 t=3.296010462 address=2627 value=0080 pc=000d60 reads=701312 observer_writes=0')
        lines.append('rom6_burst_fcch_complete: writes=8 cap=256 observer_writes=0')
        return '\n'.join(lines)

    @patch('tools.rom6_adc_detect_check.check_consumer_program')
    def test_finite_search_control_acceptance_is_not_synchronization(self,_):
        result=check_finite_search_acceptance(self.finite_fixture(),b'',b'')
        self.assertTrue(result['native_acceptance_observed'])
        self.assertFalse(result['fcch_timing_qualified'])
        self.assertFalse(result['sch_qualified'])

    @patch('tools.rom6_adc_detect_check.check_consumer_program')
    def test_finite_search_rejects_count_timing_flag_and_truncation_drift(self,_):
        text=self.finite_fixture()
        for old,new in [('value=fffa','value=fffb'),('t=2.963676923','t=2.960000000'),
                        ('value=0080','value=0000'),('pc=000d60','pc=000d61'),
                        ('writes=8','writes=9'),('value=0000 pc=000d27','value=0001 pc=000d27')]:
            self.assertIn(old,text)
            with self.subTest(old=old),self.assertRaises(ValueError):
                check_finite_search_acceptance(text.replace(old,new),b'',b'')

    def fixture(self):
        lines=[]
        for number in range(17):
            phase=2*number+1
            lines.extend((
                f'rom6_detect_product_input: phase={phase} mode=0000 values=0001,0002,0003,0004 st1=4103 observer_writes=0',
                f'rom6_detect_product_output: phase={phase} a=000000000b b=0000000002 st1=4103 observer_writes=0'))
        for number in range(1,17):
            lines.append(f'rom6_detect_metric: sequence={number} a=0001531d75 corr=0473,ef10,0000,0000 energy=012d,bf48 thresholds=0405,0405 mode=0000 st1=4103 observer_writes=0')
            lines.append(f'rom6_detect_threshold: sequence={number} t=2.000000000 value=0574 a=0000001d75 cpu_pc=000b9a observer_writes=0')
        for number in range(17,33):
            lines.append(f'rom6_detect_threshold: sequence={number} t=2.000000000 value=0574 a=0000001d75 cpu_pc=000b9a observer_writes=0')
        return '\n'.join(lines+['rom6_detect_product_complete: phases=34 cap=64 observer_writes=0',
                                'rom6_detect_threshold_complete: reads=500 cap=32 observer_writes=0',
                                'rom6_detect_metric_complete: calls=16 cap=16 observer_writes=0'])

    def checked(self,text):
        program=bytearray(0x20000)
        program[0x604*2:0x605*2]=bytes.fromhex('fc00')
        with patch('tools.rom6_adc_detect_check.check_filter_program'),patch('tools.rom6_adc_detect_check.check_consumer_program'):
            return check(text,program,b'',program)

    def test_independent_signed_vectors(self):
        self.assertEqual(complex_products((804,0xf870,804,0xf870)),(4394512,0))
        self.assertEqual(complex_products((0,0,0,0)),(0,0))
        self.assertEqual(complex_products((0xffff,2,3,4)),(5,10))
        self.assertEqual(normalized_metric((0x473ef10,0),0x12dbf48),0x1531d75)
        self.assertEqual(normalized_metric((0,0),0),0)
        with self.assertRaises(ValueError): normalized_metric((1,0),-1)

    def test_prefix_and_mutations(self):
        text=self.fixture()
        self.assertEqual(self.checked(text)['products'],17)
        self.assertFalse(self.checked(text)['fcch_qualified'])
        for old,new in (('000000000b','000000000a'),('0000000002','fffffffffe'),
                        ('phase=3 ','phase=1 '),('st1=4103','st1=4143'),
                        ('mode=0000','mode=0001'),('phases=34','phases=33'),
                        ('0001531d75','0001531d74'),('energy=012d','energy=013d'),
                        ('value=0574','value=0575'),('a=0000001d75','a=0000001d74'),
                        ('cpu_pc=000b9a','cpu_pc=000b9b'),('sequence=17 ','sequence=18 ')):
            with self.subTest(old=old),self.assertRaises(ValueError):
                self.checked(text.replace(old,new,1))


if __name__=='__main__': unittest.main()
