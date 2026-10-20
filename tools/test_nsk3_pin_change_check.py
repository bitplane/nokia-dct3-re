import unittest
from tools.nsk3_pin_change_check import check_change,KEYS
from tools.make_sim_card_profile import make_profile
from tools.sim_security_trace_check import CHV_OFFSET,AOC_NVRAM_SIZE


class PinChangeTest(unittest.TestCase):
    def test_only_pin1_changes_after_physical_confirmation(self):
        command=bytes.fromhex('a024000110')+b'1234'+b'\xff'*4+b'4321'+b'\xff'*4
        trace='\n'.join([
            *[f'nsk3_security_contact: key={key:02x} down=1 column=1' for key in (1,2,3,4,5,0x19)+KEYS],
            'nsk3_security_submit: pointer=0010ba2c text=3132333435000000 caller=0028a1d7',
            'nsk3_security_gate: match=00000001 retry_pointer=0010c520 retry=00',
            'nsk3_security_accepted: result=00000001',
            *[f'nsk3_sim_tx: index={i} value={value:02x}' for i,value in enumerate(command,1)],
            'SIM status ins=20 sw=9000 chv=3/3 puk=10/10 enabled=1',
            'SIM status ins=24 sw=9000 chv=3/3 puk=10/10 enabled=1'])
        before=make_profile(True).ljust(AOC_NVRAM_SIZE,b'\xff')
        after=bytearray(before)
        after[CHV_OFFSET:CHV_OFFSET+8]=b'4321'+b'\xff'*4
        check_change(trace,before,after)
        for bad in (trace.replace('value=24','value=2c'),trace.replace('ins=24','ins=28'),trace.replace('key=18','key=17')):
            with self.assertRaises(ValueError):check_change(bad,before,after)
        for offset in (0,CHV_OFFSET,len(after)-1):
            bad=bytearray(after)
            bad[offset]^=1
            with self.assertRaises(ValueError):check_change(trace,before,bad)


if __name__=='__main__':unittest.main()
