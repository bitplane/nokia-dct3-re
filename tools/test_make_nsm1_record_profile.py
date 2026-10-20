import unittest
from tools.make_nsm1_record_profile import build
from tools.dct3_rom4_record_codec import decode_record,retained_record_transform
from tools.dct3_lock_record_example import EXAMPLE_RECORDS


class RecordInputTest(unittest.TestCase):
    def test_only_record_and_checksum_change(self):
        before=bytes(i%256 for i in range(0x4000))
        chip=bytes.fromhex('00160010')
        result=build(before,chip)
        changed={i for i,(a,b) in enumerate(zip(before,result)) if a!=b}
        self.assertTrue(changed<=set(range(0x20,0x38))|set(range(0x3c,0x40)))
        self.assertEqual(int.from_bytes(result[0x3c:0x40],'big'),sum(result[:0x3c])&65535)
        encoded=retained_record_transform(result[0x20:0x38],before[:12])
        self.assertEqual(b''.join(decode_record(encoded[i:i+12],chip) for i in (0,12)),EXAMPLE_RECORDS)
        for bad,key in ((before[:-1],chip),(before,b'\0'*4)):
            with self.assertRaises(ValueError):build(bad,key)


if __name__=='__main__':unittest.main()
