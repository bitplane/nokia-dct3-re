import io
import unittest
from tools.rom6_radio_census import census, check_output_overlay


class RadioCensusTest(unittest.TestCase):
    def test_own_upload_writer_not_a_later_overlay(self):
        image=bytearray(0x117070+160)
        overlay=bytearray(0x4000)
        overlay[0xb54*2:0xb56*2]=bytes.fromhex('75920031')
        overlay[0xb58*2:0xb5a*2]=bytes.fromhex('75820032')
        image[0x117070:]=overlay[0xb20*2:0xb70*2]
        self.assertEqual(check_output_overlay(image,overlay)['bytes'],160)
        with self.assertRaises(ValueError): check_output_overlay(image,overlay[:-1])
        overlay[0xb54*2]^=1
        with self.assertRaises(ValueError): check_output_overlay(image,overlay)

    def test_complete_stream_and_issued_tuple(self):
        writes=b''.join(f'1.00000000{i}\t001700\t{port:04x}\t{value:04x}\tffff\n'.encode()
                        for i,(port,value) in enumerate(((0x30,0x180),(0x31,0x48c8),(0x32,0x4400),(4,0x17c))))
        text='\n'.join(f'rom6_radio_port: direction=W address={port:04x} count=1 first=1.000000000 last=1.000000003 values={value:04x} values_capped=0 cpu_pcs=1700 pcs_capped=0'
                       for port,value in ((0x30,0x180),(0x31,0x48c8),(0x32,0x4400),(4,0x17c)))
        text+='\nrom6_radio_capture: writes=4 overflow=0 observer_writes=0'
        report=census(text,io.BytesIO(writes))
        self.assertEqual(report['write_records'],4)
        self.assertEqual(report['issued_p30_p31_p32_p04'][0]['words'],['0180','48c8','4400','017c'])
        self.assertEqual(report['issued_p30_p31_p32_p04'][0]['fresh_payloads'],1)
        repeated=writes+b'1.000000004\t001700\t0004\t017c\tffff\n'
        repeated_text=text.replace('address=0004 count=1','address=0004 count=2').replace('writes=4','writes=5')
        repeated_report=census(repeated_text,io.BytesIO(repeated))
        self.assertEqual(repeated_report['issued_p30_p31_p32_p04'][0]['count'],2)
        self.assertEqual(repeated_report['issued_p30_p31_p32_p04'][0]['fresh_payloads'],1)
        for bad_text,bad_writes in ((text,writes[:-1]),(text,writes+writes),
                (text.replace('overflow=0','overflow=1'),writes),
                (text.replace('count=1','count=2',1),writes),
                (text+'\nrom6_radio_port: broken read summary',writes),
                (text.replace('writes=4','writes=0'),writes),
                (text,writes.replace(b'1.000000003',b'0.000000003'))):
            with self.subTest(text=bad_text[-65:]),self.assertRaises(ValueError):
                census(bad_text,io.BytesIO(bad_writes))


if __name__=='__main__': unittest.main()
