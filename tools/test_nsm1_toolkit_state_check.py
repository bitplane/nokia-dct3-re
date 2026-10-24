import hashlib
import unittest
from unittest.mock import patch
from tools import nsm1_toolkit_state_check as checker
from tools.sim_toolkit_display_check import DISPLAY,RESPONSE
from tools.test_nsm1_native_state_check import trace as native
from tools.test_nsm1_toolkit_check import wire


def trace():
    text=native().replace('40.000000000','65.000000000')
    initial='\n'.join((wire('TX',bytes.fromhex('a010000005')+checker.PROFILE),
        'SIM status ins=10 sw=9000 chv=3/3 puk=10/10 enabled=1',
        'SIM completion ins=f2 sw=9116',wire('TX',bytes.fromhex('a012000016')),
        wire('RX',DISPLAY+b'\x90\x00')))
    text=text.replace('nsm1_state: phase=saved',initial+'\nnsm1_state: phase=saved')
    for phase in ('reference','restored'):
        marker=f'nsm1_state_contact: phase={phase} down=1'
        text=text.replace(marker,marker+'\n'+wire('TX',bytes.fromhex('a01400000c')+RESPONSE)+
            '\nSIM status ins=14 sw=9000 chv=3/3 puk=10/10 enabled=1')
    return text


class PendingToolkitStateTest(unittest.TestCase):
    def test_architecture_card_ui_and_two_physical_completions_compose(self):
        card,pixels=bytes(3570),b'own idle'
        with patch.object(checker,'IDLE',hashlib.sha256(pixels).hexdigest()):
            for prefix in ('','[luaengine] '):
                text='\n'.join(prefix+line for line in trace().splitlines())
                result=checker.check_trace(text,card,card,pixels,pixels)
                self.assertEqual(result['io_replay_events'],8180)
                self.assertFalse(result['native_registration_verified'])

    def test_refetch_early_response_corrupt_card_and_architecture_fail(self):
        card,pixels=bytes(3570),b'own idle'
        text=trace()
        early=text.replace('nsm1_state_contact: phase=restored down=1','')
        early+='\nnsm1_state_contact: phase=restored down=1'
        for bad in (early,text+wire('TX',bytes.fromhex('a012000016')),
                    text.replace('ins=14 sw=9000','ins=14 sw=6a80',1),
                    text.replace('ILLEGAL:0','ILLEGAL:1'),
                    text.replace('completed=1','completed=10'),
                    text.replace('restored=8180','restored=8179'),
                    text.replace('ins=20 sw=9000','ins=20 sw=9804'),
                    text.replace('data=0b ','data=03 ',1),
                    text+'\nnsm1_restart_request:',text+'\nLUA ERROR',
                    text+'\nnsm1_state: phase=saved malformed'):
            with patch.object(checker,'IDLE',hashlib.sha256(pixels).hexdigest()), self.assertRaises(ValueError):
                checker.check_trace(bad,card,card,pixels,pixels)
        with patch.object(checker,'IDLE',hashlib.sha256(pixels).hexdigest()):
            with self.assertRaises(ValueError):
                checker.check_trace(text,card,card[:-1]+b'\x01',pixels,pixels)
            with self.assertRaises(ValueError):
                checker.check_trace(text,card,card,pixels,b'blank')


if __name__=='__main__':
    unittest.main()
