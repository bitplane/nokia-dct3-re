import unittest
from tools.nsm1_pin_request_check import card_transition
from tools.sim_security_trace_check import ENABLED_OFFSET
from tools.sim_phonebook_check import CURRENT_NVRAM_LENGTH


class RequestTests(unittest.TestCase):
    def test_only_enable_byte_changes(self):
        before=bytes(CURRENT_NVRAM_LENGTH)
        enabled=bytearray(before);enabled[ENABLED_OFFSET]=1
        self.assertEqual(card_transition(before,True),enabled)
        self.assertEqual(card_transition(enabled,False),before)
        with self.assertRaises(ValueError):card_transition(before,False)
        with self.assertRaises(ValueError):card_transition(before[:-1],True)


if __name__=='__main__':unittest.main()
