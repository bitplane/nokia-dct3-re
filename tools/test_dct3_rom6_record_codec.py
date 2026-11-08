import unittest
from tools.dct3_rom6_record_codec import decode_record,encode_record,effective_key


class Rom6RecordArithmeticTest(unittest.TestCase):
    table=bytes.fromhex('b8338004b848851a2dcea128')
    chip=bytes.fromhex('00160010')

    def test_complete_native_faid_result_not_just_checksum_pair(self):
        encoded=bytes.fromhex('92af475b873dfecf1ec705d7')
        plain=bytes.fromhex('0dfd5edca196a316f99c5f96')
        self.assertEqual(effective_key(self.table,self.chip),
                         (0xd90e,0xaaa8,0x9c77,0xefa6,0xc12d,0x50fb))
        self.assertEqual(decode_record(encoded,self.chip,self.table),plain)
        self.assertEqual(encode_record(plain,self.chip,self.table),encoded)
        self.assertNotEqual(plain[:4],bytes.fromhex('f65a0d46'))

    def test_inverse_inputs_and_identity_are_explicit(self):
        for plain in (bytes(12),bytes([255])*12,bytes(range(12))):
            for chip in (self.chip,bytes(4),bytes([255])*4):
                self.assertEqual(decode_record(encode_record(plain,chip,self.table),chip,self.table),plain)
        for operation in (decode_record,encode_record):
            for record,chip,table in ((bytes(11),self.chip,self.table),
                                     (bytes(12),bytes(3),self.table),
                                     (bytes(12),self.chip,bytes(13))):
                with self.assertRaises(ValueError):operation(record,chip,table)

    def test_native_identity_and_both_lock_results(self):
        identity=decode_record(bytes.fromhex('2421a164fa26d9d3ed0a9f04'),self.chip,
                               bytes.fromhex('ca635ae719faa64c0d78fc16'))
        self.assertEqual(identity.hex(),'c21e6d4c90c54e5c649e096a')
        # The actual resident stores this xor-stamped six-word identity.
        stamped=bytes(value^0xa5 for value in identity)
        self.assertEqual(stamped.hex(),'67bbc8e93560ebf9c13baccf')
        table=bytes.fromhex('7bb4d0ef9eb20abe73dad335')
        for encoded,plain in (
                ('bbaf2c681fd3b4c98ef7b464','dcb244e67cbc67c570ca294d'),
                ('e73d2d65a48c94c190d83df6','e1d4f6f72aa44c88ae91e5cd')):
            result=decode_record(bytes.fromhex(encoded),self.chip,table)
            self.assertEqual(result.hex(),plain)
            self.assertNotEqual(result[-2:],bytes.fromhex('54c2'))


if __name__=='__main__':unittest.main()
