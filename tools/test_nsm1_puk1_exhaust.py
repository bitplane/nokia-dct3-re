from pathlib import Path
import unittest
from unittest.mock import patch

from tools.nsm1_puk1_exhaust import check_permanent,chain
from tools.sim_security_trace_check import ATTEMPTS_OFFSET,AOC_NVRAM_SIZE


class PermanentPuk1Test(unittest.TestCase):
    def test_requires_unchanged_terminal_card_and_physical_refusal(self):
        card=bytearray(AOC_NVRAM_SIZE)
        card[ATTEMPTS_OFFSET:ATTEMPTS_OFFSET+4]=bytes((0,3,0,10))
        keys=(1,2,3,4,5,6,7,9,0x19)+(4,3,2,1,0x19)*2
        trace='\n'.join(f'nsm1_puk_contact: key={key:02x} down={edge} '
                        for key in keys for edge in (1,0))
        with patch('tools.nsm1_puk1_exhaust.check_link'), \
             patch('tools.nsm1_puk1_exhaust.stored',return_value=bytes(card)) as stored, \
             patch('tools.nsm1_puk1_exhaust.trace_text',return_value=trace) as text, \
             patch('tools.nsm1_puk1_exhaust.frame_check') as frame:
            check_permanent(Path('run'),Path('previous'))
            self.assertEqual(frame.call_count,2)
            for bad in (trace.replace('key=19','key=17'),
                        trace+'\nSIM status ins=2c sw=9840 chv=0/3 puk=0/10 enabled=1'):
                text.return_value=bad
                with self.assertRaises(ValueError):check_permanent(Path('run'),Path('previous'))
            text.return_value=trace
            changed=bytearray(card);changed[-1]^=1
            stored.side_effect=[bytes(card),bytes(changed)]
            with self.assertRaises(ValueError):check_permanent(Path('run'),Path('previous'))

    def test_truncated_pin_seed_is_rejected_before_execution(self):
        with patch.object(Path,'read_bytes',return_value=b''):
            with self.assertRaisesRegex(ValueError,'extent'):
                chain([Path('a'),Path('b'),Path('c')],Path('wrong'),Path('output'),check_only=True)


if __name__=='__main__':unittest.main()
