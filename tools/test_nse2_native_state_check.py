import hashlib
import re
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
    def test_duplicate_snapshot_and_dsp_fields_cannot_overwrite_evidence(self):
        card = bytes(3570)
        with patch('tools.nse2_native_state_check.MENU_FRAME', hashlib.sha256(b'menu').hexdigest()):
            for bad in (trace().replace(' pc=', ' pc=00231056 pc='),
                        trace().replace('dsp=A:0,', 'dsp=A:1,A:0,'),
                        trace().replace('ILLEGAL:0', 'ILLEGAL:1,ILLEGAL:0')):
                with self.subTest(snapshot=bad[:100]), self.assertRaises(ValueError):
                    check_state(bad, card, card, b'menu', b'menu')

    def test_control_records_are_complete_not_substring_matches(self):
        card = bytes(3570)
        with patch('tools.nse2_native_state_check.MENU_FRAME', hashlib.sha256(b'menu').hexdigest()):
            for bad in (trace().replace('completed=1', 'completed=10'),
                        trace().replace('completed=1', 'completed=1 trailing'),
                        trace() + '\nnse2_state: phase=unknown malformed',
                        trace() + '\nnse2_state_replay: malformed',
                        trace() + '\nnse2_state_contact: malformed',
                        trace().replace('window=2.7 exact=1', 'window=2.7 exact=1 trailing')):
                with self.subTest(tail=bad[-100:]), self.assertRaises(ValueError):
                    check_state(bad, card, card, b'menu', b'menu')

    def test_model_owned_periodic_samples_are_not_save_load_control_records(self):
        text = trace() + '\n[luaengine] nse2_state: t=23.000000000 pc=00256d08 cpsr=0000003f sp=0010d4f8 observer_writes=0'
        with patch('tools.nse2_native_state_check.MENU_FRAME', hashlib.sha256(b'menu').hexdigest()):
            check_state(text, bytes(3570), bytes(3570), b'menu', b'menu')

    def test_dsp_values_fit_the_exported_register_interface(self):
        card = bytes(3570)
        with patch('tools.nse2_native_state_check.MENU_FRAME', hashlib.sha256(b'menu').hexdigest()):
            for name, maximum in (('PC',0xffff), ('XPC',0x7f), ('A',0xffffffffff),
                                  ('B',0xffffffffff), ('RC',0xffff), ('IDLE',1),
                                  ('GENPC',0x7fffff), ('CURPC',0x7fffff), ('SWCR',1)):
                check_state(re.sub(rf'\b{name}:0\b', f'{name}:{maximum:x}', trace()),
                            card, card, b'menu', b'menu')
                with self.subTest(register=name), self.assertRaises(ValueError):
                    check_state(re.sub(rf'\b{name}:0\b', f'{name}:{maximum+1:x}', trace()),
                                card, card, b'menu', b'menu')
            with self.assertRaises(ValueError):
                check_state(trace().replace('BSCR:0', 'BSCR:1'), card, card, b'menu', b'menu')

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
