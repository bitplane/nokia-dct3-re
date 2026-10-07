import unittest
from unittest.mock import patch

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
    def test_sms_requires_storage_and_delivered_save_boundary(self):
        with self.assertRaisesRegex(ValueError, 'persistent SIM storage'):
            verify(GOOD, sms=True)
        with patch('tools.noki8210_state_check.verify_sms'):
            with self.assertRaisesRegex(ValueError, 'after delivered SMS'):
                verify(GOOD, sms=True, storage=b'fixture')

    def test_sms_requires_read_after_load(self):
        delivered = ('sim_device: update fid=6f3c record=1 length=176\n'
                     'LAPDm service Channel Release acknowledged\n')
        with patch('tools.noki8210_state_check.verify_sms'):
            with self.assertRaisesRegex(ValueError, 'post-load physical SMS read'):
                verify(delivered + GOOD, sms=True, storage=b'fixture')
            verify(delivered + GOOD + '8210_sms_physical: action=read_2\n',
                   sms=True, storage=b'fixture')

    def test_call_save_must_follow_connect(self):
        call = GOOD + '8210_call_physical: action=end\n'
        with patch('tools.noki8210_state_check.verify_call'):
            with self.assertRaisesRegex(ValueError, 'established active call'):
                verify(call, call=True)
            verify('GSM service uplink sapi=0 pd=03 message=0f length=2 data=030f\n' + call,
                   call=True)

    def test_call_requires_post_load_release(self):
        call = 'GSM service uplink sapi=0 pd=03 message=0f length=2 data=030f\n' + GOOD
        with patch('tools.noki8210_state_check.verify_call'):
            with self.assertRaisesRegex(ValueError, 'physical call release'):
                verify(call, call=True)

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
