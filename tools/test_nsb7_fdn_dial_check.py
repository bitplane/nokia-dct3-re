"""FDN local denial and full-card isolation controls."""
import unittest
from unittest.mock import patch
from tools.nsb7_fdn_dial_check import check_trace,check_cards
from tools.nsb7_fdn_record_check import edited_record
from tools.nsb7_pin_check import serialized_profile
from tools.sim_fdn_profile import extend


def denied_trace():
    rows=[]
    steps=[(28+i,'Keypad '+digit,int(digit)) for i,digit in enumerate('12347')]
    steps += [(36,'Send',0x0e),(48,'End',0x0f)]
    for second,key,code in steps:
        rows += [f'nsb7_outgoing_call_input: key={key} state=1 t={second}.000000000',
                 f'nsb7_key_decode: key={code:02x} observer_writes=0',
                 f'nsb7_outgoing_call_input: key={key} state=0 t={second}.200000000']
    return '\n'.join(rows+['nsb7_outgoing_call_done: t=58.000000000'])


class DialTest(unittest.TestCase):
    @patch('tools.nsb7_fdn_dial_check.check_authentication')
    def test_denied_and_no_network_establishment(self, authentication):
        text=denied_trace()
        check_trace(text,True)
        authentication.assert_called_once_with('')
        with self.assertRaises(ValueError):
            check_trace(text+'\nGSM service establish sapi=0 pd=05 message=24 length=16 data=05247103205881080910101032547698',True)
        with self.assertRaises(ValueError):
            check_trace(text.replace('key=0e observer_writes=0','key=0f observer_writes=0'),True)

    def test_authentication_required(self):
        with self.assertRaises(ValueError):
            check_trace(denied_trace(),True)

    def test_card_validity_only_off_transition(self):
        source=bytearray(extend(serialized_profile()))
        source[3575]=0
        source[3578:3610]=edited_record()
        off=bytearray(source)
        off[3575]=1
        check_cards(bytes(source),bytes(source),bytes(source),bytes(off),bytes(off))
        for index in (3517,3578,1600):
            changed=bytearray(source)
            changed[index]^=1
            with self.subTest(index=index),self.assertRaises(ValueError):
                check_cards(bytes(source),bytes(changed),bytes(source),bytes(off),bytes(off))


if __name__=='__main__':
    unittest.main()
