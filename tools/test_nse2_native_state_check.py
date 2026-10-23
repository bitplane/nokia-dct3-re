import hashlib
import unittest
from unittest.mock import patch

from tools.nse2_native_state_check import check_state, DSP_REGISTERS


def trace():
    registers=['00000000']*37
    registers[13]='0010d584'
    registers[15]='00231056'
    cpu=','.join(registers)
    dsp=','.join(f'{name}:0' for name in sorted(DSP_REGISTERS))
    snapshot=f't=18.500000000 pc=00231056 sp=0010d584 ram=9003be51 cpu={cpu} dsp={dsp} data=1faf2f5d program=cd8d0101'
    return '\n'.join((f'nse2_state: phase=saved {snapshot}',
                      'nse2_state_contact: phase=reference down=1',
                      'nse2_state_contact: phase=reference down=0',
                      f'nse2_state: phase=restored {snapshot}',
                      'nse2_state_contact: phase=restored down=1',
                      'nse2_state_contact: phase=restored down=0',
                      'nse2_state_replay: reference=8180 restored=8180 window=2.7 exact=1',
                      'nse2_state: completed=1'))


class NativeStateTest(unittest.TestCase):
    def test_restored_architecture_replay_and_physical_ui(self):
        card=bytes(3570)
        with patch('tools.nse2_native_state_check.MENU_FRAME',hashlib.sha256(b'menu').hexdigest()):
            result=check_state(trace(),card,card,b'menu',b'menu')
            self.assertEqual(result['io_replay_events'],8180)
            self.assertFalse(result['stable_long_run'])

    def test_rejects_missing_state_mutation_bad_replay_or_incomplete_input(self):
        card=bytes(3570)
        text=trace()
        bad_traces=[text.replace('restored=8180','restored=8179'),
                    text.replace('reference=8180 restored=8180','reference=0 restored=0'),
                    text.replace('window=2.7','window=2x7'),
                    text.replace('phase=restored t=','phase=missing t='),
                    text.replace('nse2_state: completed=1',''),
                    text.replace('phase=restored down=1','phase=restored down=0'),
                    text.replace('ILLEGAL:0','ILLEGAL:1'),
                    text.replace('pc=00231056','pc=00000000'),
                    text.replace('dsp=','dsp=BOGUS:0,'),
                    text+'\nnse2_state: FAIL incomplete']
        with patch('tools.nse2_native_state_check.MENU_FRAME',hashlib.sha256(b'menu').hexdigest()):
            for bad in bad_traces:
                with self.subTest(bad=bad[-100:]),self.assertRaises(ValueError):
                    check_state(bad,card,card,b'menu',b'menu')
            with self.assertRaises(ValueError):
                check_state(text,card,card[:-1]+b'\x01',b'menu',b'menu')
            with self.assertRaises(ValueError):
                check_state(text,card,card,b'menu',b'blank')


if __name__=='__main__':
    unittest.main()
