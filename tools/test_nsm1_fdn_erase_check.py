import unittest
from tools.nsm1_fdn_erase_check import erased_card,RECORD
from tools.sim_fdn_profile import EXTENDED_LENGTH


class FdnEraseTests(unittest.TestCase):
    def test_only_record_is_freed(self):
        saved=bytearray([0x53])*EXTENDED_LENGTH;saved[3578:3610]=RECORD
        expected=bytearray(saved);expected[3578:3610]=b'\xff'*32
        self.assertEqual(erased_card(saved),expected)
        for bad in (expected,saved[:-1],saved+b'\x00'):
            with self.assertRaises(ValueError):erased_card(bad)


if __name__=='__main__':unittest.main()
