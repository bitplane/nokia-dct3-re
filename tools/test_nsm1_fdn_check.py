import unittest
from tools.nsm1_fdn_check import validity_targets,rejected_card
from tools.sim_fdn_profile import EXTENDED_LENGTH
from tools.sim_security_trace_check import ATTEMPTS_OFFSET


class FdnCommandsTests(unittest.TestCase):
    def test_rejected_activation_only_changes_pin2_retry(self):
        before=bytearray(EXTENDED_LENGTH);before[3570:3578]=b'FDN\x01\x01\x01\x00\x00'
        before[ATTEMPTS_OFFSET+1]=3
        after=rejected_card(before)
        self.assertEqual([(i,a,b) for i,(a,b) in enumerate(zip(before,after)) if a!=b],[(ATTEMPTS_OFFSET+1,3,2)])
        with self.assertRaises(ValueError):rejected_card(after)
        before[3575]=0
        with self.assertRaises(ValueError):rejected_card(before)

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
