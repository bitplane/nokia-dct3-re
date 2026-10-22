import unittest
from unittest.mock import patch
from tools.nsm1_puk2_check import recovered_card,recovery
from tools.sim_security_trace_check import ATTEMPTS_OFFSET,CHV_OFFSET,padded_pin
from tools.sim_phonebook_check import CURRENT_NVRAM_LENGTH


class Puk2Tests(unittest.TestCase):
    def test_terminal_rejection_requires_9840_not_another_retry(self):
        before=bytearray(CURRENT_NVRAM_LENGTH)
        before[ATTEMPTS_OFFSET:ATTEMPTS_OFFSET+4]=bytes((3,0,10,1))
        after=recovered_card(before,False)
        trace='\n'.join(f'nsm1_pin_contact: correct=1 key={key:02x} down={edge} '
                        for key in (1,2,3,4,0x19) for edge in (1,0))
        trace+='\nSIM status ins=20\nnsm1_security_submit:\nnsm1_security_accepted:\nSIM status ins=2c'
        tx=bytes.fromhex('a020000108')+padded_pin('1234')+bytes.fromhex('a02c000210')+b'87654322'+padded_pin('8765')
        with patch('tools.nsm1_puk2_check.check_phone'), \
             patch('tools.nsm1_puk2_check.transmitted',return_value=tx), \
             patch('tools.nsm1_puk2_check.statuses',side_effect=lambda text,ins:
                   [(0x9000,3,10)] if ins==0x20 else [(0x9840,3,10)] if ins==0x2c else []) as status:
            recovery(trace,before,after,False)
            status.side_effect=lambda text,ins: [(0x9000,3,10)] if ins==0x20 else [(0x9804,3,10)] if ins==0x2c else []
            with self.assertRaises(ValueError):recovery(trace,before,after,False)

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
        for count in range(1,11):
            before[ATTEMPTS_OFFSET+3]=count
            rejected=bytearray(before)
            rejected[ATTEMPTS_OFFSET+3]=count-1
            self.assertEqual(recovered_card(before,False),rejected)
        before[ATTEMPTS_OFFSET+3]=0
        with self.assertRaises(ValueError):recovered_card(before,False)
        before[ATTEMPTS_OFFSET+1]=1
        with self.assertRaises(ValueError):recovered_card(before,True)


if __name__=='__main__':unittest.main()
