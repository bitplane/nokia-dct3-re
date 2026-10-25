import unittest

from tools.nsm1_click_format_check import samples,compare


class ClickFormatTest(unittest.TestCase):
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
