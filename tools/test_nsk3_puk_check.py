import unittest
from tools.nsk3_puk_check import check_recovery
from tools.make_sim_card_profile import make_profile
from tools.sim_security_trace_check import CHV_OFFSET,ATTEMPTS_OFFSET,AOC_NVRAM_SIZE


class PukCheckTest(unittest.TestCase):
    def fixture(self,wrong):
        digits=(1,2,3,4,5,6,7,9 if wrong else 8,0x19)+(4,3,2,1,0x19)*2
        command=bytes.fromhex('a02c000010')+(b'12345679' if wrong else b'12345678')+b'4321'+b'\xff'*4
        trace='\n'.join([
            *[f'nsk3_puk_contact: key={key:02x} down={down} t=0' for key in digits for down in (1,0)],
            *[f'nsk3_sim_tx: index={i} value={value:02x}' for i,value in enumerate(command,1)],
            f'SIM status ins=2c sw={"9804" if wrong else "9000"} chv={0 if wrong else 3}/3 puk={9 if wrong else 10}/10 enabled=1'])
        card=bytearray(make_profile(True).ljust(AOC_NVRAM_SIZE,b'\xff'))
        if wrong:
            card[ATTEMPTS_OFFSET]=0
            card[ATTEMPTS_OFFSET+2]=9
        else:card[CHV_OFFSET:CHV_OFFSET+8]=b'4321'+b'\xff'*4
        return trace,card

    def test_positive_and_negative_contracts(self):
        for wrong in (False,True):
            trace,card=self.fixture(wrong)
            check_recovery(trace,card,wrong)
            for bad in (trace.replace('value=2c','value=20'),trace.replace('key=19','key=17'),
                        trace.replace('ins=2c','ins=20'),trace+'\nunhandled opcode'):
                with self.assertRaises(ValueError):check_recovery(bad,card,wrong)
            mutated=bytearray(card)
            mutated[CHV_OFFSET]^=1
            with self.assertRaises(ValueError):check_recovery(trace,mutated,wrong)


if __name__=='__main__':unittest.main()
