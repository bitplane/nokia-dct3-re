import unittest
from unittest.mock import patch
from tools.nsm1_pin_change_check import check_change
from tools.sim_security_trace_check import CHV_OFFSET,ENABLED_OFFSET,padded_pin
from tools.sim_phonebook_check import CURRENT_NVRAM_LENGTH


class PinChangeTests(unittest.TestCase):
    def test_exact_card_and_command(self):
        before=bytearray(CURRENT_NVRAM_LENGTH);before[ENABLED_OFFSET]=1
        before[CHV_OFFSET:CHV_OFFSET+8]=padded_pin('1234')
        after=bytearray(before);after[CHV_OFFSET:CHV_OFFSET+8]=padded_pin('4321')
        command=bytes.fromhex('a024000110')+padded_pin('1234')+padded_pin('4321')
        trace='SIM status ins=24 sw=9000 chv=3/3 puk=10/10 enabled=1\n'
        with patch('tools.nsm1_pin_change_check.check_authenticated') as auth, patch(
                'tools.nsm1_pin_change_check.transmitted',return_value=command):
            check_change(trace,before,after)
            self.assertEqual(auth.call_args.kwargs,{'allowed_mutations':(0x24,)})
            after[0]^=1
            with self.assertRaisesRegex(ValueError,'outside PIN1'):check_change(trace,before,after)
            with self.assertRaises(ValueError):check_change(trace.replace('9000','9804'),before,after)


if __name__=='__main__':unittest.main()
