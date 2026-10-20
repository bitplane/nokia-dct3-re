import unittest
from tools.nsk3_pin_request_check import check_enable,check_setting,KEYS
from tools.make_sim_card_profile import make_profile
from tools.sim_security_trace_check import ENABLED_OFFSET,AOC_NVRAM_SIZE


class PinRequestCheckTest(unittest.TestCase):
    def test_physical_enable_changes_only_flag(self):
        command=bytes.fromhex('a028000108')+b'1234'+b'\xff'*4
        trace='\n'.join([
            *[f'nsk3_security_contact: key={key:02x} down=1 column=1' for key in (1,2,3,4,5,0x19)+KEYS],
            'nsk3_security_submit: pointer=0010ba2c text=3132333435000000 caller=0028a1d7',
            'nsk3_security_gate: match=00000001 retry_pointer=0010c520 retry=00',
            'nsk3_security_accepted: result=00000001',
            *[f'nsk3_sim_tx: index={i} value={value:02x}' for i,value in enumerate(command,1)],
            'SIM status ins=28 sw=9000 chv=3/3 puk=10/10 enabled=1'])
        before=make_profile(False).ljust(AOC_NVRAM_SIZE,b'\xff')
        after=bytearray(before)
        after[ENABLED_OFFSET]=1
        check_enable(trace,before,after)
        disabled=trace.replace('ins=28','ins=26').replace('value=28','value=26').replace('key=17','key=18')
        check_setting(disabled,after,before,False)
        with self.assertRaises(ValueError):check_setting(trace,after,before,False)
        for bad in (trace.replace('ins=28','ins=26'),trace.replace('value=28','value=26'),
                    trace.replace('key=17','key=18')):
            with self.assertRaises(ValueError):check_enable(bad,before,after)
        for offset in (0,ENABLED_OFFSET,len(after)-1):
            bad=bytearray(after)
            bad[offset]^=1
            with self.assertRaises(ValueError):check_enable(trace,before,bad)
        with self.assertRaises(ValueError):check_enable(trace,before[:-1],after)


if __name__=='__main__':unittest.main()
