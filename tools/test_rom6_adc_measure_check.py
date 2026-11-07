import math
import struct
import unittest
from unittest.mock import patch

from tools.rom6_adc_measure_check import MASK40,check,check_measure_program,log_power_q16,tone_power


class AdcMeasurementTest(unittest.TestCase):
    def fixture(self,tone=True,base=0x474):
        data=bytearray(0x20000)
        data[0xb09e*2:0xb0de*2]=struct.pack('>64H',0,*[math.floor(640*math.log10(i)) for i in range(1,64)])
        lines=['mfi_rx_epoch: first_tick=120 divider=12 physical_clock_claim=0']
        stores=0
        for number in range(1,33):
            rssi=number>=3 and number%2==1
            power=274657 if tone and rssi else 0
            # Literal log value for the analytic test vector; the fixture does
            # not call the converter under test to generate its expected output.
            logarithm=0x0d932000 if power else 0
            correction=0x0fba0000 if base else 3<<22
            result=(logarithm-correction)&MASK40
            address=base+3 if base else 0x022d
            calibration=0x0fba if base else 3
            flags=0x20 if base else 0
            band=4 if base==0x474 else 0
            lines.append(f'rom6_measure_input: sequence={number} a={power:010x} ar2=022d calibration=0003 flags3ec8={flags:04x} flags263b={band:04x} st1=4900 observer_writes=0')
            lines.append(f'rom6_measure_output: sequence={number} a={logarithm:010x} b={result:010x} ar2={address:04x} calibration={calibration:04x} st1=4900 observer_writes=0')
            if rssi:
                stores+=1
                lines.append(f'rom6_measure_rssi_return: sequence={number} b={result:010x} observer_writes=0')
                lines.append(f'rom6_adc_result_word: sequence={stores} t=1.000000000 value={(result>>16)&0xffff:04x} mode022c=0004 work022a=0000 cpu_pc=002432 mask=ffff observer_writes=0')
        lines.append('rom6_measure_complete: calls=32 pending=0 cap=32 observer_writes=0')
        return '\n'.join(lines),bytes(data)

    def checked(self,text,data,tone=True):
        with patch('tools.rom6_adc_measure_check.check_measure_program'):
            return check(text,b'',b'',b'',data,tone)

    def test_phased_power_and_scope(self):
        for tone in (False,True):
            result=self.checked(*self.fixture(tone),tone)
            self.assertEqual(result['raw_rssi_stores'],15)
            self.assertEqual(result['raw_signed_values'],[-551] if tone else [-4026])
            self.assertFalse(result['post_correction_qualified'])
            self.assertFalse(result['physical_dbm_qualified'])
            self.assertFalse(result['rssi_response_packet_qualified'])

    def test_calibration_branches_as_logical_vectors(self):
        for base,mode in ((0x49c,'table_049c'),(None,'direct_x64')):
            self.assertEqual(self.checked(*self.fixture(base=base))['calibration_modes'],[mode])

    def test_log_boundaries_and_tone_phase(self):
        self.assertEqual(log_power_q16(0),0)
        self.assertEqual(log_power_q16(MASK40),MASK40)
        self.assertEqual(log_power_q16(31),954<<16)
        self.assertEqual(log_power_q16(32),963<<16)
        self.assertEqual(log_power_q16(64),(963<<16)+(1541<<13))
        self.assertEqual(tone_power(0,True),274657)
        self.assertEqual(tone_power(12,True),274576)
        self.assertEqual(tone_power(36,True),275282)
        with self.assertRaises(ValueError): log_power_q16(-1)

    def test_reject_arithmetic_phase_and_store_drift(self):
        text,data=self.fixture()
        for old,new in (('a=000d932000','a=000d932001'),('ar2=0477','ar2=0478'),
                        ('calibration=0fba','calibration=0fbb'),('st1=4900','st1=4940'),
                        ('first_tick=120','first_tick=12'),('value=fdd9','value=fdda'),
                        ('cpu_pc=002432','cpu_pc=0071d7'),('calls=32 pending=0','calls=32 pending=1'),
                        ('rom6_measure_rssi_return: sequence=3 ','rom6_measure_rssi_return: sequence=2 ')):
            with self.subTest(old=old),self.assertRaises(ValueError):
                self.checked(text.replace(old,new,1),data)
        changed=bytearray(data); changed[0xb09e*2+7]^=1
        with self.assertRaises(ValueError): self.checked(text,bytes(changed))
        with self.assertRaises(ValueError): check_measure_program(b'',b'',b'')


if __name__=='__main__': unittest.main()
