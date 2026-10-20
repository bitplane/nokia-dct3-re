import unittest

from tools.nsk1_radio_contract import literal_references,analyze,native_completion_contract


class Nsk1RadioContractTest(unittest.TestCase):
    def test_big_endian_aligned_literal_candidates(self):
        image = bytearray(64)
        image[0:2] = bytes.fromhex('4803')
        image[2:4] = bytes.fromhex('4903')
        image[16:20] = (0x107158).to_bytes(4,'big')
        result = literal_references(image,{0x107158})
        self.assertEqual([{'address':'0x200010','value':'0x107158'}],result['aligned_pools'])
        self.assertEqual(['0x200000','0x200002'],[row['address'] for row in result['thumb16_literal_load_candidates']])
        self.assertEqual(31,result['aligned_halfwords_scanned'])
        image[16:20] = (0x107158).to_bytes(4,'little')
        self.assertFalse(literal_references(image,{0x107158})['aligned_pools'])

    def test_requires_own_exact_image(self):
        with self.assertRaisesRegex(ValueError,'own standard'):
            analyze(bytes(1024))

    def test_native_publisher_rejects_unqualified_program(self):
        with self.assertRaisesRegex(ValueError, 'acquired NSE-1'):
            native_completion_contract(bytes(1024))


if __name__ == '__main__':
    unittest.main()
