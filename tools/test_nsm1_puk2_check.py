import unittest
from tools.nsm1_puk2_check import recovered_card
from tools.sim_security_trace_check import ATTEMPTS_OFFSET,CHV_OFFSET,padded_pin
from tools.sim_phonebook_check import CURRENT_NVRAM_LENGTH


class Puk2Tests(unittest.TestCase):
    def test_only_pin2_and_own_retries_change(self):
        before=bytearray(CURRENT_NVRAM_LENGTH)
        before[ATTEMPTS_OFFSET:ATTEMPTS_OFFSET+4]=bytes((3,0,10,10))
        before[CHV_OFFSET+8:CHV_OFFSET+16]=padded_pin('5678')
        wrong=bytearray(before);wrong[ATTEMPTS_OFFSET+3]=9
        self.assertEqual(recovered_card(before,False),wrong)
        recovered=bytearray(before);recovered[ATTEMPTS_OFFSET+1]=3
        recovered[CHV_OFFSET+8:CHV_OFFSET+16]=padded_pin('8765')
        self.assertEqual(recovered_card(before,True),recovered)
        self.assertEqual(recovered_card(wrong,True),recovered)
        before[ATTEMPTS_OFFSET+1]=1
        with self.assertRaises(ValueError):recovered_card(before,True)


if __name__=='__main__':unittest.main()
