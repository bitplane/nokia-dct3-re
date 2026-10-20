import unittest
from tools.nsm1_pin_check import check_cards
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

    def test_shared_protocol_checker_uses_explicit_observer_prefix(self):
        command=bytes.fromhex('a020000108')+b'1234'+b'\xff'*4
        trace='\n'.join(f'nsm1_pin_contact: correct=1 key={key:02x} down={down} t=1' for key in (1,2,3,4,0x19) for down in (1,0))
        trace+='\n'+'\n'.join(f'nsm1_sim_tx: index={i} value={value:02x}' for i,value in enumerate(command,1))
        trace+='\nSIM status ins=20 sw=9000 chv=3/3 puk=10/10 enabled=1'
        check_trace(trace,True,prefix='nsm1',tx_limit=4096)
        for bad in (trace.replace('index=2 ','index=3 ',1),trace.replace('nsm1_pin_contact','nsk3_pin_contact'),trace.replace('value=a0','value=00')):
            with self.assertRaises(ValueError):check_trace(bad,True,prefix='nsm1',tx_limit=4096)


if __name__=='__main__':unittest.main()
