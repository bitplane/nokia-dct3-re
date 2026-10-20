import unittest
from tools.nsk3_puk2_check import recovery,KEYS
from tools.make_sim_card_profile import make_profile
from tools.sim_security_trace_check import CHV_OFFSET,ATTEMPTS_OFFSET,AOC_NVRAM_SIZE,padded_pin


class Puk2Test(unittest.TestCase):
    def test_only_pin2_and_its_retry_counter_recover(self):
        before=bytearray(make_profile(True).ljust(AOC_NVRAM_SIZE,b'\xff'))
        before[ATTEMPTS_OFFSET+1]=0
        after=bytearray(before);after[CHV_OFFSET+8:CHV_OFFSET+16]=padded_pin('8765');after[ATTEMPTS_OFFSET+1]=3
        command=(bytes.fromhex('a020000108')+padded_pin('1234')
                 +bytes.fromhex('a02c000210')+b'87654321'+padded_pin('8765')
                 +bytes.fromhex('a020000208')+padded_pin('8765'))
        trace='\n'.join((
            *[f'nsk3_security_contact: key={k:02x} down=1 column=1' for k in (1,2,3,4,5,0x19)+KEYS],
            'nsk3_security_submit: pointer=0010ba2c text=3132333435000000 caller=0028a1d7',
            'nsk3_security_gate: match=00000001 retry_pointer=0010c520 retry=00',
            'nsk3_security_accepted: result=00000001',
            *[f'nsk3_sim_tx: index={i} value={v:02x}' for i,v in enumerate(command,1)],
            'SIM status ins=20 sw=9000 chv=3/0 puk=10/10 enabled=1',
            'SIM status ins=2c sw=9000 chv=3/3 puk=10/10 enabled=1',
            'SIM status ins=20 sw=9000 chv=3/3 puk=10/10 enabled=1'))
        recovery(trace,before,after)
        for bad in (trace.replace('ins=2c sw=9000','ins=2c sw=9804'),trace.replace('value=2c','value=24'),trace.replace('key=18','key=17')):
            with self.assertRaises(ValueError):recovery(bad,before,after)
        for offset in (0,CHV_OFFSET,ATTEMPTS_OFFSET,ATTEMPTS_OFFSET+3):
            changed=bytearray(after);changed[offset]^=1
            with self.assertRaises(ValueError):recovery(trace,before,changed)


if __name__=='__main__':unittest.main()
