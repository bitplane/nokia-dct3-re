import unittest
from tools.nsk3_pin2_check import check_change,PREFIX
from tools.make_sim_card_profile import make_profile
from tools.sim_security_trace_check import CHV_OFFSET,ATTEMPTS_OFFSET,AOC_NVRAM_SIZE,padded_pin


class Pin2Test(unittest.TestCase):
    def test_pin2_only_and_physical_payload(self):
        keys=(1,2,3,4,5,0x19)+PREFIX+(5,6,7,8,0x19)+(8,7,6,5,0x19)*2
        command=bytes.fromhex('a024000210')+padded_pin('5678')+padded_pin('8765')
        trace='\n'.join((
            *[f'nsk3_security_contact: key={key:02x} down=1 column=1' for key in keys],
            'nsk3_security_submit: pointer=0010ba2c text=3132333435000000 caller=0028a1d7',
            'nsk3_security_gate: match=00000001 retry_pointer=0010c520 retry=00',
            'nsk3_security_accepted: result=00000001',
            *[f'nsk3_sim_tx: index={i} value={value:02x}' for i,value in enumerate(command,1)],
            'SIM status ins=20 sw=9000 chv=3/3 puk=10/10 enabled=1',
            'SIM status ins=24 sw=9000 chv=3/3 puk=10/10 enabled=1'))
        before=make_profile(True).ljust(AOC_NVRAM_SIZE,b'\xff')
        after=bytearray(before);after[CHV_OFFSET+8:CHV_OFFSET+16]=padded_pin('8765')
        check_change(trace,before,after,'5678','8765')
        wrong=trace.replace('key=08 down=1 column=1','key=05 down=1 column=1',1)
        wrong=wrong.replace('index=9 value=38','index=9 value=35')
        wrong=wrong.replace('ins=24 sw=9000 chv=3/3','ins=24 sw=9804 chv=3/2')
        rejected=bytearray(before);rejected[ATTEMPTS_OFFSET+1]=2
        check_change(wrong,before,rejected,'5675','8765',False)
        check_change(trace,rejected,after,'5678','8765')
        last=bytearray(before);last[ATTEMPTS_OFFSET+1]=1
        blocked=bytearray(last);blocked[ATTEMPTS_OFFSET+1]=0
        check_change(wrong.replace('sw=9804 chv=3/2','sw=9840 chv=3/0'),last,blocked,'5675','8765',False)
        with self.assertRaises(ValueError):check_change(wrong,before,after,'5675','8765',False)
        for bad in (trace.replace('value=24','value=2c'),trace.replace('ins=24 sw=9000','ins=24 sw=9804'),trace.replace('key=18','key=17')):
            with self.assertRaises(ValueError):check_change(bad,before,after,'5678','8765')
        for offset in (0,CHV_OFFSET,len(after)-1):
            bad=bytearray(after);bad[offset]^=1
            with self.assertRaises(ValueError):check_change(trace,before,bad,'5678','8765')


if __name__=='__main__':unittest.main()
