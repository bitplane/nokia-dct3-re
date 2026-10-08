import unittest
from tools.noki8890_state_check import verify


def sample():
    return '''8890_state: event=saved pc=0000001c sp=00137b6c ram=97bf9f87 t=42.000000000
state_replay: phase=reference event=begin t=42.000000000
RX enqueue type=80 payload=34 data=1234 t=42.500000000
state_replay: phase=reference event=end t=43.000000000
8890_state: event=restored pc=0000001c sp=00137b6c ram=97bf9f87 t=42.000000000
state_roundtrip: result=pass scenario=8890_idle requested_at=42.000000000 t=42.000000000
state_replay: phase=restored event=begin t=42.000000000
RX enqueue type=80 payload=34 data=1234 t=42.500000000
state_replay: phase=restored event=end t=43.000000000
8890_state_physical: key=Menu
8890_keypad_decoded: key=19
'''


class StateTest(unittest.TestCase):
    def test_sip_idle_continuation_requires_physical_exit(self):
        text = sample().replace('8890_state_physical: key=Menu', '8890_sip_cancel: physical Exit').replace('key=19', 'key=1a')
        verify(text, sip_cancel=True)
        with self.assertRaisesRegex(ValueError, 'dismissal'):
            verify(sample(), sip_cancel=True)
        with self.assertRaisesRegex(ValueError, 'idle restoration'):
            verify(text, sip_cancel=True, call=True)
        with self.assertRaisesRegex(ValueError, 'exactly'):
            verify(text.replace('event=restored pc=0000001c', 'event=restored pc=00000020'), sip_cancel=True)

    def test_pcs_requires_independent_registration(self):
        with self.assertRaisesRegex(ValueError, 'candidate window'):
            verify(sample(), pcs1900=True)

    def test_sms_requires_storage(self):
        with self.assertRaisesRegex(ValueError, 'persistent SIM storage'):
            verify(sample(), sms=True)

    def test_complete(self):
        verify(sample())

    def test_architectural_mismatch(self):
        with self.assertRaisesRegex(ValueError, 'exactly'):
            verify(sample().replace('event=restored pc=0000001c', 'event=restored pc=00000020'))

    def test_protocol_mismatch(self):
        with self.assertRaisesRegex(ValueError, 'diverged'):
            verify(sample().replace('data=1234', 'data=5678', 1))

    def test_missing_input(self):
        with self.assertRaisesRegex(ValueError, 'Menu decode'):
            verify(sample().replace('key=19', 'key=18'))

    def test_incomplete(self):
        with self.assertRaisesRegex(ValueError, 'complete'):
            verify(sample() + '8890_state: FAIL incomplete')
