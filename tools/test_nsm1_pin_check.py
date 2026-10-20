import unittest
from tools.nsm1_pin_check import check_cards,check_authenticated
from tools.nsk3_pin_check import check_trace
from tools.make_sim_card_profile import make_profile
from tools.sim_security_trace_check import ATTEMPTS_OFFSET,AOC_NVRAM_SIZE


class PinTest(unittest.TestCase):
    def test_whole_card_changes_only_retry(self):
        before=make_profile(True).ljust(AOC_NVRAM_SIZE,b'\xff')
        wrong=bytearray(before);wrong[ATTEMPTS_OFFSET]=2
        check_cards(before,wrong,wrong,before)
        for offset in (0,ATTEMPTS_OFFSET+1,len(before)-1):
            bad=bytearray(wrong);bad[offset]^=1
            with self.assertRaises(ValueError):check_cards(before,bad,bad,before)
        with self.assertRaises(ValueError):check_cards(before,wrong,before,before)

    def pin_trace(self):
        command=bytes.fromhex('a020000108')+b'1234'+b'\xff'*4
        trace='\n'.join(f'nsm1_pin_contact: correct=1 key={key:02x} down={down} t=1' for key in (1,2,3,4,0x19) for down in (1,0))
        trace+='\n'+'\n'.join(f'nsm1_sim_tx: index={i} value={value:02x}' for i,value in enumerate(command,1))
        trace+='\nSIM status ins=20 sw=9000 chv=3/3 puk=10/10 enabled=1'
        return trace

    def test_shared_protocol_checker_uses_explicit_observer_prefix(self):
        trace=self.pin_trace()
        check_trace(trace,True,prefix='nsm1',tx_limit=4096)
        for bad in (trace.replace('index=2 ','index=3 ',1),trace.replace('nsm1_pin_contact','nsk3_pin_contact'),trace.replace('value=a0','value=00')):
            with self.assertRaises(ValueError):check_trace(bad,True,prefix='nsm1',tx_limit=4096)

    def test_pin_phone_code_then_application_input(self):
        phone='\n'.join(f'nsm1_security_contact: key={key:02x} down={down} t=15 firmware_writes=0' for key in (1,2,3,4,5,0x19) for down in (1,0))
        phone+='\nnsm1_security_submit: text=3132333435000000 caller=0029faf3\nnsm1_security_gate: match=00000001 retry_pointer=00112824 retry=00\nnsm1_security_accepted: result=00000001'
        menu='\nnsm1_security_contact: key=19 down=1 t=21 firmware_writes=0\nnsm1_security_contact: key=19 down=0 t=21.2 firmware_writes=0'
        check_authenticated(self.pin_trace()+'\n'+phone+menu)
        with self.assertRaises(ValueError):check_authenticated(phone+'\n'+self.pin_trace()+menu)


if __name__=='__main__':unittest.main()
