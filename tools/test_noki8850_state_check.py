import unittest
from unittest.mock import patch
from tools.noki8850_state_check import verify


GOOD = '''8850_state: event=saved pc=0000001c sp=00137000 ram=12345678 t=40.000000000
state_replay: phase=reference event=begin t=40.000000000
RX enqueue type=80 payload=34 data=1234 t=40.500000000
state_replay: phase=reference event=end t=41.000000000
8850_state: event=restored pc=0000001c sp=00137000 ram=12345678 t=40.000000000
state_roundtrip: result=pass scenario=8850_call requested_at=40.000000000 t=40.000000000
state_replay: phase=restored event=begin t=40.000000000
RX enqueue type=80 payload=34 data=1234 t=40.500000000
state_replay: phase=restored event=end t=41.000000000
8850_call_physical: action=end
'''


class StateTest(unittest.TestCase):
    def check(self, text):
        # The product-specific call grammar has its own executable checker tests.
        with patch('tools.noki8850_state_check.verify_call') as call:
            verify(text)
            call.assert_called_once_with(text)

    def test_exact_replay(self):
        self.check(GOOD)

    def test_architectural_mismatch(self):
        with self.assertRaisesRegex(ValueError, 'exactly'):
            self.check(GOOD.replace('event=restored pc=0000001c', 'event=restored pc=00000020'))

    def test_protocol_mismatch(self):
        with self.assertRaisesRegex(ValueError, 'diverged'):
            self.check(GOOD.replace('data=1234', 'data=5678', 1))

    def test_missing_postload_input(self):
        with self.assertRaisesRegex(ValueError, 'post-load'):
            self.check(GOOD.replace('8850_call_physical: action=end', ''))

    def test_incomplete(self):
        with self.assertRaisesRegex(ValueError, 'complete'):
            self.check(GOOD + '8850_state: FAIL incomplete')
