import unittest
from tools.nsm1_fdn_record_check import changed_card,edited_card,RECORD
from tools.sim_fdn_profile import EXTENDED_LENGTH
from tools.sim_security_trace_check import ATTEMPTS_OFFSET


class FdnRecordTests(unittest.TestCase):
    def test_exact_success_and_rejection_changes(self):
        before=bytearray(EXTENDED_LENGTH);before[3578:3610]=b'\xff'*32
        before[ATTEMPTS_OFFSET+1]=3
        expected=bytearray(before);expected[3578:3610]=RECORD
        self.assertEqual(changed_card(before),expected)
        rejected=bytearray(before);rejected[ATTEMPTS_OFFSET+1]=2
        self.assertEqual(changed_card(before,True),rejected)
        with self.assertRaises(ValueError):changed_card(expected)
        with self.assertRaises(ValueError):changed_card(rejected)
        with self.assertRaises(ValueError):changed_card(before[:-1])

    def test_edit_changes_only_last_bcd_digit(self):
        before=bytearray(EXTENDED_LENGTH);before[3578:3610]=RECORD
        after=edited_card(before)
        self.assertEqual([(i,a,b) for i,(a,b) in enumerate(zip(before,after)) if a!=b],[(3600,0xf5,0xf6)])
        with self.assertRaises(ValueError):edited_card(after)
        with self.assertRaises(ValueError):edited_card(before[:-1])


if __name__=='__main__':unittest.main()
