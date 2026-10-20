import unittest
from tools.sim_fdn_profile import extend,EXTENDED_LENGTH


class FdnProfileTests(unittest.TestCase):
    def test_retains_complete_base_and_initializes_only_tail(self):
        before=bytes([0x53])*3570
        data=extend(before)
        self.assertEqual(len(data),EXTENDED_LENGTH)
        self.assertEqual(data[:3570],before)
        self.assertEqual(data[3570:3578],b'FDN\x01\x01\x01\x00\x00')
        self.assertEqual(data[3578:],b'\xff'*320)
        readable=extend(before,True)
        self.assertEqual(readable,data[:3576]+b'\x01'+data[3577:])
        for length in (0,3569,3571,3898):
            with self.assertRaises(ValueError):extend(bytes(length))


if __name__=='__main__':unittest.main()
