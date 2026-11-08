import math
import struct
import unittest
from unittest.mock import patch

from tools.rom6_adc_measure_check import (MASK40,check,check_measure_program,
    check_frequency_correction,check_rx_publications,check_fcch_only_sch_failure,frequency_correction,log_power_q16,tone_power)


class AdcMeasurementTest(unittest.TestCase):
    def sch_negative_fixture(self):
        program=bytearray(0xa000)
        for address,words in {
                0x4bcd:(0x7715,0xe,0x7714,0x80,0xf274,0x2c88,0x7713,0x2f5b),
                0x4efb:(0x6ff8,0x2f20,0xc48,0x1a93),
                0x4f53:(0xf030,0x3ff,0x60f8,8,0x3ff,0x7711,0,0xfd20,0x6d91,0xfe00,0x7311,0x2f20)}.items():
            program[address*2:address*2+len(words)*2]=struct.pack('>'+str(len(words))+'H',*words)
        words=[0x4000,0x100,0xb,4,0,0,0]
        lines=['rom6_burst_crc: sequence=1 t=1.000000000 status=0001 remainder=0000 input=0000,0000,0000 pc=004bc8 observer_writes=0',
               'rom6_burst_block_sources: sequence=1 channel=0040 selection=0000 crc=0001 bsic=0000 frame=0000,000b arfcn=0004 shift=0000 decoded=0000,0000,0000 observer_writes=0']
        for index,word in enumerate(words):
            lines.append(f'rom6_burst_block_field: sequence={index+1} t=2.00000000{index} address={0x2f5b+index:04x} value={word:04x} pc=004f39 observer_writes=0')
        packed=','.join(f'{word:04x}' for word in [0xe80,*words])
        lines.extend([f'rom6_measure_rx_commit: sequence=1 t=3.000000000 previous=0080 producer=0088 consumer=0080 words={packed} mask=ffff cpu_pc=002d08 observer_writes=0',
                      'rom6_measure_rx_complete: commits=1 cap=64 observer_writes=0'])
        return '\n'.join(lines),program

    def test_sch_absent_input_joins_native_crc_to_wire_flag(self):
        text,program=self.sch_negative_fixture()
        report=check_fcch_only_sch_failure(text,program)
        self.assertEqual(report['error_flag'],1)
        self.assertEqual(report['payload'],'40000100000b0004000000000000')
        self.assertTrue(report['frame_fields_are_metadata'])
        self.assertFalse(report['decoded_frame_number_qualified'])
        self.assertFalse(report['sch_qualified'])

    def test_sch_negative_rejects_missing_status_and_serialization_drift(self):
        text,program=self.sch_negative_fixture()
        for old,new in [('remainder=0000','remainder=03ff'),('crc=0001','crc=0000'),
                        ('decoded=0000,0000,0000','decoded=0001,0000,0000'),
                        ('address=2f5c value=0100','address=2f5c value=0000'),
                        ('0e80,4000,0100','0e80,4000,0000'),
                        ('t=1.000000000','t=4.000000000')]:
            with self.subTest(old=old),self.assertRaises(ValueError):
                check_fcch_only_sch_failure(text.replace(old,new,1),program)
        program[0x4f53*2]^=1
        with self.assertRaises(ValueError): check_fcch_only_sch_failure(text,program)

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

    def test_frequency_descriptor_branches(self):
        main=(10,90,472,722)
        other=(575,783,364,160)
        for key,flags,expected,wrote in (
                (0,0x80,0xfdeb,True),(10,0x80,0xfdd9,True),
                (11,0x80,0xfdd9,False),(90,0x80,0xfdd9,True),
                (91,0x80,0xfddc,True),(512,0x80,0xfe33,True),
                (600,0x80,0xfdd9,False),(784,0x80,0xfdda,True),
                (1023,0x80,0xfded,True)):
            with self.subTest(key=key):
                self.assertEqual(frequency_correction(0xfdd9,key,flags,main,other),(expected,wrote))
        with self.assertRaises(ValueError): frequency_correction(0,1024,0,main,other)

    def correction_fixture(self):
        lines=[]
        for number in range(1,17):
            key=number-1
            value=0xfdd9+((max(0,10-key)*472+128)>>8)
            lines.extend((
                f'rom6_measure_rssi_return: sequence={number} b=fffdd90000 observer_writes=0',
                f'rom6_adc_result_word: sequence={number} t=1.000000000 value=fdd9 mode022c=0004 work022a=0000 cpu_pc=002432 mask=ffff observer_writes=0',
                f'rom6_measure_correct_input: sequence={number} key={key:04x} pointer=022b raw=fdd9 flags=0080 main=000a,005a,01d8,02d2 other=023f,030f,016c,00a0 st1=6103 observer_writes=0'))
            if key<=10:
                lines.append(f'rom6_adc_result_word: sequence={number} t=1.000000000 value={value:04x} mode022c=0004 work022a=0000 cpu_pc=0071d7 mask=ffff observer_writes=0')
            lines.append(f'rom6_measure_correct_output: sequence={number} pointer=022b value={value:04x} st1=6103 observer_writes=0')
        lines.append('rom6_measure_correct_complete: calls=16 pending=0 cap=16 observer_writes=0')
        return '\n'.join(lines)

    def test_frequency_correction_join_and_rejections(self):
        text=self.correction_fixture()
        result=check_frequency_correction(text)
        self.assertEqual((result['calls'],result['stores'],result['no_store_returns'],
                          result['joined_raw_measurements']),(16,11,5,16))
        for old,new in (('value=fdeb','value=fdea'),('pointer=022b','pointer=022c'),
                        ('st1=6103','st1=6143'),('pending=0 cap=16','pending=1 cap=16'),
                        ('cpu_pc=0071d7','cpu_pc=002432')):
            with self.subTest(old=old),self.assertRaises(ValueError):
                check_frequency_correction(text.replace(old,new,1))

    def test_committed_packet_wrap_and_continuity(self):
        text='\n'.join((
            'rom6_measure_rx_commit: sequence=1 t=1.000000000 previous=00e3 producer=0081 consumer=00e3 words=028a,0004 mask=ffff cpu_pc=002d08 observer_writes=0',
            'rom6_measure_rx_commit: sequence=2 t=2.000000000 previous=0081 producer=0084 consumer=0081 words=0383,1234,5600 mask=ffff cpu_pc=002d08 observer_writes=0',
            'rom6_measure_rx_complete: commits=2 cap=64 observer_writes=0'))
        result=check_rx_publications(text)
        self.assertEqual([packet['payload'] for packet in result['packets']],['0004','123456'])
        self.assertTrue(result['rssi_reply_observed'])
        self.assertFalse(result['mcu_consumption_qualified'])
        for old,new in (('commits=2','commits=3'),('previous=0081','previous=0080'),
                        ('0383','0883'),('mask=ffff','mask=00ff'),('t=2.000000000','t=0.500000000')):
            with self.subTest(old=old),self.assertRaises(ValueError):
                check_rx_publications(text.replace(old,new,1))

    def test_full_broadcast_capture_bound_keeps_completeness_requirement(self):
        rows=[]
        for i in range(150):
            start=0x80+(2*i)%100
            end=0x80+(2*i+2)%100
            rows.append(f'rom6_measure_rx_commit: sequence={i+1} t={i+1}.000000000 previous={start:04x} producer={end:04x} consumer={start:04x} words=028a,0004 mask=ffff cpu_pc=002d08 observer_writes=0')
        summary='rom6_measure_rx_complete: commits=150 cap=256 observer_writes=0'
        self.assertEqual(check_rx_publications('\n'.join(rows+[summary]))['commits'],150)
        for bad in ('\n'.join(rows[:64]+[summary]),
                    '\n'.join(rows+[summary.replace('cap=256','cap=64')]),
                    '\n'.join(rows+[summary.replace('cap=256','cap=512')]),
                    '\n'.join(rows+[summary,summary.replace('cap=256','cap=512')])):
            with self.assertRaises(ValueError):
                check_rx_publications(bad)


if __name__=='__main__': unittest.main()
