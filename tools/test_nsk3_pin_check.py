import unittest
from tools.nsk3_pin_check import check_trace,check_cards
from tools.make_sim_card_profile import make_profile
from tools.sim_security_trace_check import ATTEMPTS_OFFSET,AOC_NVRAM_SIZE


class PinCheckTest(unittest.TestCase):
    def trace(self,correct):
        keys=(1,2,3,4,0x19) if correct else (4,3,2,1,0x19)
        command=bytes.fromhex('a020000108')+(b'1234' if correct else b'4321')+b'\xff'*4
        return '\n'.join([
            *[f'nsk3_pin_contact: correct={int(correct)} key={key:02x} down={down} t=0'
              for key in keys for down in (1,0)],
            *[f'nsk3_sim_tx: index={i} value={value:02x}' for i,value in enumerate(command,1)],
            f'SIM status ins=20 sw={"9000" if correct else "9804"} chv={3 if correct else 2}/3 puk=10/10 enabled=1'])

    def test_physical_apdu_and_status(self):
        for correct in (False,True):
            trace=self.trace(correct)
            check_trace(trace,correct)
            for bad in (trace.replace('key=19','key=17'),trace.replace('ins=20','ins=24'),
                        trace.replace('value=a0','value=00'),trace+'\nunhandled opcode'):
                with self.assertRaises(ValueError):check_trace(bad,correct)

    def test_cold_card_restores_only_retry(self):
        wrong=bytearray(make_profile(True).ljust(AOC_NVRAM_SIZE,b'\xff'))
        wrong[ATTEMPTS_OFFSET]=2
        correct=bytearray(wrong)
        correct[ATTEMPTS_OFFSET]=3
        check_cards(wrong,wrong,correct)
        for offset in (0,ATTEMPTS_OFFSET,len(correct)-1):
            bad=bytearray(correct)
            bad[offset]^=1
            with self.assertRaises(ValueError):check_cards(wrong,wrong,bad)
        with self.assertRaises(ValueError):check_cards(wrong,correct,correct)


if __name__=='__main__':unittest.main()
