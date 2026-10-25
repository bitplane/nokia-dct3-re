import unittest

from tools.nsm1_click_format_check import samples,compare,rom_format,check_steps


class ClickFormatTest(unittest.TestCase):
    def test_intermediate_cases_and_mutations(self):
        lines=[]
        cases=[(0,0,0,0x55),(-13485,17,-6,0x3f),(37738,15,-8,0x57),
               (2063,19,-4,0x95),(2059,19,-4,0x95),
               (8254,17,-6,0xb5),(8237,17,-6,0xb5),(8217,17,-6,0xb5)]
        for i,(value,exponent,shift,output) in enumerate(cases,1):
            magnitude=abs(value)
            stages=[(0x9435,4,value&0xffffffffff),(0x943a,4,magnitude),
                    (0x943b,6,exponent),(0x943d,5,exponent),
                    (0x9442,6,shift&0xffff),(0x9443,4,magnitude),
                    (0x9444,5,-shift),(0x9445,4,magnitude>>(-shift)),
                    (0x9446,4,(magnitude>>(-shift))>>3),
                    (0x3587,4,(0xff00 if value>0 else 0)|output),
                    (0x358a,4,(0xff00 if value>0 else 0)|output)]
            for address,field,wanted in stages:
                fields=[i,value&0xffffffffff,address,0,0,0,0,
                        0xb3d7 if value>0 else 0xb3d8,0,0]
                fields[field]=wanted
                lines.append('nsm1_format_step: id=%d input=%010x address=%04x pc=%08x a=%010x b=%010x t_reg=%04x ar6=%04x st0=%04x st1=%04x time=84.100000000 observer_writes=0\n'%tuple(fields))
        text=''.join(lines)
        self.assertEqual(check_steps(text)['intermediate_cases'],8)
        for bad in ('',text.replace('t_reg=fffc','t_reg=0000',1),
                    text.replace('ar6=b3d8','ar6=b3d7'),text+'nsm1_format_step: malformed'):
            with self.subTest(prefix=bad[:60]),self.assertRaises(ValueError):
                check_steps(bad)

    def test_rom_arithmetic_explains_zero_negative_and_boundary_values(self):
        for value,expected in ((0,0x55),(-13485,0x3f),(2063,0x95),
                               (2059,0x95),(8254,0xb5),(8237,0xb5),(8217,0xb5),
                               (3019,0x92),(15099,0xb8),(37738,0x57)):
            with self.subTest(value=value):
                self.assertEqual(rom_format(value),expected)
        for value in (-65536,65536):
            with self.assertRaises(ValueError):
                rom_format(value)

    def test_discrepancies_remain_explicit_and_out_of_range_is_excluded(self):
        rows=[(0,0x55)]*833+[(i,0x20) for i in range(1,532)]
        expected=[0xd5]*833+[0x20]*531
        rows += [(2063,0x95),(2059,0x95),(8254,0xb5),(8237,0xb5),(8217,0xb5)]
        expected += [0xea,0xea,0x8a,0x8a,0x8a]
        rows += [(1,0x20)]*614+[(-13485,0x3f)]+[(40000,0x55)]*16
        expected += [0x20]*614+[0x3f]
        self.assertEqual(compare(rows,bytes(expected))['out_of_range_excluded'],16)
        rows[0]=(0,0xd5)
        with self.assertRaises(ValueError):
            compare(rows,bytes(expected))

    def test_missing_duplicate_or_malformed_observations_fail(self):
        marker='nsm1_click_summary: input=2000 result=2000 serial=2000 observer_writes=0\n'
        for text in ('',marker,marker*2,marker+'nsm1_click_format: malformed\n'):
            with self.subTest(text=text[:80]),self.assertRaises(ValueError):
                samples(text)

    def test_missing_reference_or_hiding_mismatches_fails(self):
        for rows,reference in (([],b''),([(0,0x55)]*2000,b'\x55'*2000),
                               ([(0,0x55)]*2000,b'\xd5'*1999)):
            with self.subTest(count=len(rows)),self.assertRaises(ValueError):
                compare(rows,reference)


if __name__=='__main__':
    unittest.main()
