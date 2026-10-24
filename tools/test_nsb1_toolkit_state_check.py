import hashlib
import unittest
from unittest.mock import patch
from tools import nsb1_toolkit_state_check as checker
from tools.sim_toolkit_display_check import DISPLAY,RESPONSE
from tools.test_nsb1_native_state_check import trace as architecture
from tools.test_nsb1_toolkit_check import wire


def trace():
    startup=['SIM status ins=20 sw=9000 chv=3/3 puk=10/10 enabled=1']
    for phase,count in (('pin',5),('phone',6)):
        for index in range(1,count+1):
            for down in (1,0):
                startup.append(f'nsb1_pin_contact: phase={phase} index={index} down={down} t=20 physical_only=1')
    initial='\n'.join((wire('TX',bytes.fromhex('a010000005')+checker.TERMINAL_PROFILE),
        'SIM status ins=10 sw=9000 chv=3/3 puk=10/10 enabled=1',
        'SIM completion ins=f2 sw=9116',wire('TX',bytes.fromhex('a012000016')),
        wire('RX',DISPLAY+b'\x90\x00')))
    text='\n'.join(startup)+'\n'+initial+'\n'+architecture().replace('40.000000000','65.000000000')
    for phase in ('reference','restored'):
        marker=f'nsb1_state_contact: phase={phase} down=1'
        text=text.replace(marker,marker+'\n'+wire('TX',bytes.fromhex('a01400000c')+RESPONSE)+
            '\nSIM status ins=14 sw=9000 chv=3/3 puk=10/10 enabled=1')
    return text


class PendingToolkitStateTest(unittest.TestCase):
    def test_own_state_and_one_fetch_two_physical_responses(self):
        card,pixels=bytes(3570),b'own idle'
        with patch.object(checker,'IDLE',hashlib.sha256(pixels).hexdigest()):
            for prefix in ('','[luaengine] '):
                text='\n'.join(prefix+line for line in trace().splitlines())
                result=checker.check_trace(text,card,card,pixels,pixels)
                self.assertEqual(result['io_replay_events'],8180)
                self.assertFalse(result['native_registration_verified'])

    def test_authentication_pending_card_architecture_and_pixels_are_required(self):
        card,pixels=bytes(3570),b'own idle'
        text=trace()
        early=text.replace('nsb1_state_contact: phase=restored down=1','')
        early+='\nnsb1_state_contact: phase=restored down=1'
        for bad in (early,text.replace('data=0f ','data=0b ',1),
                    text.replace('ILLEGAL:0','ILLEGAL:1'),
                    text.replace('ins=14 sw=9000','ins=14 sw=6a80',1),
                    text.replace('ins=20 sw=9000','ins=20 sw=9804'),
                    text.replace('phase=phone index=6 down=0','phase=phone index=6 down=1'),
                    text.replace('completed=1','completed=10'),
                    text.replace('restored=8180','restored=8179'),
                    text+'\nnsb1_reset_reason:',text+'\nunsupported instruction',
                    text+'\nunimplemented opcode',
                    text+'\nnsb1_state: phase=saved malformed',
                    text+'\n'+wire('TX',bytes.fromhex('a012000016'))):
            with patch.object(checker,'IDLE',hashlib.sha256(pixels).hexdigest()), self.assertRaises(ValueError):
                checker.check_trace(bad,card,card,pixels,pixels)
        with patch.object(checker,'IDLE',hashlib.sha256(pixels).hexdigest()):
            with self.assertRaises(ValueError):
                checker.check_trace(text,card,card[:-1]+b'\x01',pixels,pixels)
            with self.assertRaises(ValueError):
                checker.check_trace(text,card,card,pixels,b'blank')


if __name__=='__main__':
    unittest.main()
