import unittest
from unittest.mock import patch

from tools.rom6_adc_detect_check import check,complex_products,normalized_metric


class NativeComplexProductsTest(unittest.TestCase):
    def fixture(self):
        lines=[]
        for number in range(17):
            phase=2*number+1
            lines.extend((
                f'rom6_detect_product_input: phase={phase} mode=0000 values=0001,0002,0003,0004 st1=4103 observer_writes=0',
                f'rom6_detect_product_output: phase={phase} a=000000000b b=0000000002 st1=4103 observer_writes=0'))
        for number in range(1,17):
            lines.append(f'rom6_detect_metric: sequence={number} a=0001531d75 corr=0473,ef10,0000,0000 energy=012d,bf48 thresholds=0405,0405 mode=0000 st1=4103 observer_writes=0')
        return '\n'.join(lines+['rom6_detect_product_complete: phases=34 cap=64 observer_writes=0',
                                'rom6_detect_metric_complete: calls=16 cap=16 observer_writes=0'])

    def checked(self,text):
        program=bytearray(0x20000)
        program[0x604*2:0x605*2]=bytes.fromhex('fc00')
        with patch('tools.rom6_adc_detect_check.check_filter_program'):
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
                        ('0001531d75','0001531d74'),('energy=012d','energy=013d')):
            with self.subTest(old=old),self.assertRaises(ValueError):
                self.checked(text.replace(old,new,1))


if __name__=='__main__': unittest.main()
