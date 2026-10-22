from pathlib import Path
import unittest
from unittest.mock import patch

from tools import nsk3_pin_exhaust as target
from tools.sim_security_trace_check import AOC_NVRAM_SIZE,ATTEMPTS_OFFSET


class PinExhaustTest(unittest.TestCase):
    def test_attempt_binds_complete_card_and_previous_retry(self):
        for count in (1,2):
            before=bytearray(AOC_NVRAM_SIZE)
            before[ATTEMPTS_OFFSET:ATTEMPTS_OFFSET+4]=bytes((count,3,10,10))
            after=bytearray(before)
            after[ATTEMPTS_OFFSET]=count-1
            with patch.object(target,'check_link') as link, \
                 patch.object(target,'stored',side_effect=[bytes(before),bytes(after)]) as stored, \
                 patch.object(target,'trace_text',return_value='trace'), \
                 patch.object(target,'check_trace') as trace, \
                 patch.object(target,'frame_check') as frame:
                target.check_attempt(Path('run'),Path('previous'),b'image',count)
                link.assert_called_once_with(Path('run'),Path('previous'),b'image')
                trace.assert_called_once_with('trace',False,attempts_before=count)
                frame.assert_called_once_with(Path('run/snap/nsk3_pin_5.png'),target.PIN)
                for offset in (0,ATTEMPTS_OFFSET,len(after)-1):
                    bad=bytearray(after)
                    bad[offset]^=1
                    stored.side_effect=[bytes(before),bytes(bad)]
                    with self.assertRaises(ValueError):
                        target.check_attempt(Path('run'),Path('previous'),b'image',count)
                stored.side_effect=[bytes(after)]
                with self.assertRaises(ValueError):
                    target.check_attempt(Path('run'),Path('previous'),b'image',count)


if __name__=='__main__':unittest.main()
