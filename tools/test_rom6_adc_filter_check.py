import unittest
from unittest.mock import patch

from tools.rom6_adc_filter_check import COEFFICIENTS,check,check_filter_program,fir_word,quarter_rate_mix,check_mixer


class AdcFilterCheckTest(unittest.TestCase):
    def fixture(self,mixer_mode=0,mixer_pointer=0x80):
        lines=['rom6_adc_processing_initial: history='+','.join(['0000']*16)+
               ' coefficients='+','.join(f'{word:04x}' for word in COEFFICIENTS)+' observer_writes=0']
        output=0
        for read in range(1,1025):
            # Distinct constant I/Q lanes give an analytic unity-DC-gain oracle,
            # independent of fir_word: +1024 I, -1024 Q after the discard block.
            value='0400' if read&1 else 'fc00'
            lines.append(f'mfi_rx_word: sequence={read} value={value} source_tick={read*6} t=0.100000000')
            lines.append(f'rom6_adc_history: read={read} address={0x200+(read-1)%16:04x} value={value} observer_writes=0')
            if read>32 and read%4==0:
                for value,pc in (('fc00','002496'),('0400','002497')):
                    output+=1
                    if output<=128:
                        lines.append(f'rom6_adc_filter: write={output} read={read} address={0xc8+(output-1)%16:04x} value={value} cpu_pc={pc} st1=c900 observer_writes=0')
            if read%32==0 and 64<=read<=288:
                count=2 if mixer_mode else 4
                pointer=0x80+(mixer_pointer-0x80+count)%0x48
                values=','.join(['0000']*count)
                lines.append(f'rom6_adc_mixer: write={(read-32)//32} read={read} previous={mixer_pointer:04x} pointer={pointer:04x} mode={mixer_mode:04x} values={values} cpu_pc=0024d7 observer_writes=0')
                mixer_pointer=pointer
        lines.append('rom6_adc_processing_complete: history_writes=1024 filter_writes=496 input_cap=1024 output_cap=128 observer_writes=0')
        lines.append('rom6_adc_mixer_complete: writes=31 cap=8 observer_writes=0')
        return '\n'.join(lines)

    def checked(self,text):
        with patch('tools.rom6_adc_filter_check.check_filter_program'):
            return check(text,b'',b'',b'')

    def test_signed_lanes_and_scope(self):
        result=self.checked(self.fixture())
        self.assertEqual(result['checked_output_words'],128)
        self.assertEqual(result['nonzero_output_words'],128)
        self.assertEqual(result['output_order'],'Q,I')
        for name in ('physical_clock_qualified','rssi_qualified','fcch_qualified',
                     'sch_qualified','bcch_qualified','camping_qualified'):
            self.assertFalse(result[name])

    def test_rounding_and_signed_coefficients(self):
        history=[0]*16
        coefficients=[0]*7+[0x4000]
        history[2]=2
        self.assertEqual(fir_word(history,coefficients,3,0),1)
        history[2]=0xfffe
        self.assertEqual(fir_word(history,coefficients,3,0),0)
        coefficients[-1]=0xc000
        self.assertEqual(fir_word(history,coefficients,3,0),1)

    def test_reject_unordered_or_changed_pipeline(self):
        text=self.fixture()
        for old,new in (('read=1 address=0200','read=1 address=0201'),
                        ('address=0200 value=0400','address=0200 value=0401'),
                        ('read=36 address=00c8','read=32 address=00c8'),
                        ('address=00c8 value=fc00','address=00c8 value=0400'),
                        ('cpu_pc=002496','cpu_pc=002495'),('st1=c900','st1=c940'),
                        ('history_writes=1024','history_writes=1023'),
                        ('filter_writes=496','filter_writes=495'),
                        ('write=128 ','write=129 '),('coefficients=0387','coefficients=0388')):
            with self.subTest(old=old),self.assertRaises(ValueError):
                self.checked(text.replace(old,new,1))
        with self.assertRaises(ValueError): self.checked(text+'\nrom6_adc_filter: malformed')

    def test_requires_actual_code_provenance(self):
        with self.assertRaises(ValueError): check_filter_program(bytes(0x20000),b'',bytes(0x20000))

    def test_quarter_rate_tone_becomes_constant(self):
        # Analytic +Fs/4 phasor, in native Q,I order.
        tone=[0,1000,1000,0,0,(-1000)&0xffff,(-1000)&0xffff,0]*2
        self.assertEqual(quarter_rate_mix(tone,4),[4000,0,4000,0])
        self.assertEqual(quarter_rate_mix(tone,8),[8000,0])

    def test_mixer_accounting_modes_and_wrap(self):
        for mode in (0,1):
            result=check_mixer(self.fixture(mode,0xc4))
            self.assertEqual(result['checked_output_words'],16 if mode else 32)
            self.assertEqual(result['modes'],[mode])
            self.assertFalse(result['fcch_detection_qualified'])
        text=self.fixture()
        for old,new in (('previous=0080','previous=0081'),('pointer=0084','pointer=0085'),
                        ('mode=0000','mode=0001'),('values=0000','values=0001'),
                        ('writes=31','writes=30'),('cpu_pc=0024d7','cpu_pc=0024d6')):
            with self.subTest(old=old),self.assertRaises(ValueError):
                check_mixer(text.replace(old,new,1))


if __name__=='__main__': unittest.main()
