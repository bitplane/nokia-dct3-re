import unittest
from tools.nsm1_pin2_check import changed_card
from tools.sim_security_trace_check import CHV_OFFSET,ATTEMPTS_OFFSET,padded_pin
from tools.sim_phonebook_check import CURRENT_NVRAM_LENGTH


class Pin2Tests(unittest.TestCase):
    def test_changes_only_credential_and_own_retry(self):
        before=bytearray(CURRENT_NVRAM_LENGTH)
        before[CHV_OFFSET+8:CHV_OFFSET+16]=padded_pin('5678')
        before[ATTEMPTS_OFFSET+1]=3
        wrong=bytearray(before);wrong[ATTEMPTS_OFFSET+1]=2
        self.assertEqual(changed_card(before,'5675','8765',False),wrong)
        expected=bytearray(before);expected[CHV_OFFSET+8:CHV_OFFSET+16]=padded_pin('8765')
        self.assertEqual(changed_card(wrong,'5678','8765',True),expected)
        self.assertEqual(changed_card(expected,'8765','5678',True),before)
        with self.assertRaises(ValueError):changed_card(before,'5675','8765',True)
        with self.assertRaises(ValueError):changed_card(before,'5678','8765',False)
        before[ATTEMPTS_OFFSET+1]=0
        with self.assertRaises(ValueError):changed_card(before,'5678','8765',True)


if __name__=='__main__':unittest.main()
