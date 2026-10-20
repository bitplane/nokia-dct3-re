import unittest
from tools.nsm1_fdn_check import validity_targets


class FdnCommandsTests(unittest.TestCase):
    def test_selected_file_and_bodies_not_byte_pattern_search(self):
        tx=bytes.fromhex('ff1010 a0a40000027f10 a0a40000026f3a a004000000')
        self.assertEqual(validity_targets(tx,4),[0x6f3a])
        self.assertEqual(validity_targets(tx,0x44),[])
        payload=bytes.fromhex('a044000000')+bytes(27)
        tx+=bytes.fromhex('a0dc010420')+payload
        self.assertEqual(validity_targets(tx,0x44),[])
        tx+=bytes.fromhex('a0a40000026f7e a044000000')
        self.assertEqual(validity_targets(tx,0x44),[0x6f7e])

    def test_malformed_commands_rejected(self):
        for tx in (b'\xff',b'\xa0\xa4',bytes.fromhex('a0a40000026f'),
                   bytes.fromhex('a044010000'),bytes.fromhex('a004000001')):
            with self.subTest(tx=tx),self.assertRaises(ValueError):validity_targets(tx,tx[1] if len(tx)>1 else 4)


if __name__=='__main__':unittest.main()
