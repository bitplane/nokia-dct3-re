import unittest

from tools.noki8210_state_check import verify


GOOD = '''8210_state: event=saved pc=0000001c sp=001384b4 ram=8662f12b t=32.000000000
state_replay: phase=reference event=begin t=32.000000000
TX packet type=20 payload=2 data=abcd t=32.5
state_replay: phase=reference event=end t=33.000000000
8210_state: event=restored pc=0000001c sp=001384b4 ram=8662f12b t=32.000000000
state_roundtrip: result=pass scenario=8210_idle requested_at=32.000000000 t=32.000000000
state_replay: phase=restored event=begin t=32.000000000
TX packet type=20 payload=2 data=abcd t=32.5
state_replay: phase=restored event=end t=33.000000000
8210_state_physical: key=Menu
8210_keypad_decoded: key=19
'''


class Nokia8210StateTest(unittest.TestCase):
    def test_exact_replay(self):
        verify(GOOD)

    def test_architectural_mismatch(self):
        with self.assertRaisesRegex(ValueError, 'architectural state'):
            verify(GOOD.replace('restored pc=0000001c', 'restored pc=00000020'))

    def test_empty_replay_is_not_evidence(self):
        with self.assertRaisesRegex(ValueError, 'no radio records'):
            verify('\n'.join(line for line in GOOD.splitlines() if 'TX packet' not in line))

    def test_changed_payload(self):
        with self.assertRaisesRegex(ValueError, 'diverged'):
            verify(GOOD.replace('data=abcd', 'data=abce', 1))

    def test_missing_physical_continuation(self):
        with self.assertRaisesRegex(ValueError, 'physical Menu'):
            verify(GOOD.replace('key=19', 'key=18'))

    def test_incomplete_fixture(self):
        with self.assertRaisesRegex(ValueError, 'did not complete'):
            verify(GOOD + '8210_state: FAIL incomplete')
