import unittest
from unittest.mock import patch

from tools.noki6250_state_check import verify
from tools.test_noki8210_state_check import GOOD as IDLE_TRACE


GOOD = IDLE_TRACE.replace('8210_', '6250_').replace(
    '6250_keypad_decoded: key=19', '6250_raw_matrix_key: value=06')


class Nokia6250StateTest(unittest.TestCase):
    def test_call_save_requires_established_connection(self):
        trace = GOOD + '6250_state_physical: key=End\n'
        with patch('tools.noki6250_state_check.verify_outgoing'):
            with self.assertRaisesRegex(ValueError, 'established active call'):
                verify(trace, call=True)
            verify('GSM service uplink sapi=0 pd=03 message=0f length=2 data=030f\n' + trace,
                   call=True)

    def test_call_requires_post_load_end(self):
        trace = 'GSM service uplink sapi=0 pd=03 message=0f length=2 data=030f\n' + GOOD
        with patch('tools.noki6250_state_check.verify_outgoing'):
            with self.assertRaisesRegex(ValueError, 'physical call release'):
                verify(trace, call=True)

    def test_exact_replay(self):
        verify(GOOD)

    def test_architectural_mismatch(self):
        with self.assertRaisesRegex(ValueError, 'architectural state'):
            verify(GOOD.replace('restored pc=0000001c', 'restored pc=00000020'))

    def test_empty_replay_is_not_evidence(self):
        with self.assertRaisesRegex(ValueError, 'no radio records'):
            verify('\n'.join(line for line in GOOD.splitlines() if 'TX packet' not in line))

    def test_wrong_physical_scan(self):
        with self.assertRaisesRegex(ValueError, 'physical Menu'):
            verify(GOOD.replace('value=06', 'value=07'))

    def test_incomplete_fixture(self):
        with self.assertRaisesRegex(ValueError, 'did not complete'):
            verify(GOOD + '6250_state: FAIL incomplete')
