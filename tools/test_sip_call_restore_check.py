import json
from pathlib import Path
import tempfile
import unittest

from tools.run_sip_handset_gate import verify_restore


LOG = '''
incoming state id=1 epoch=1 phase=connected
sip_state: saved
sip_state: restored
termination id=1 cause=41 result=accepted
incoming state id=1 epoch=2 phase=ended
'''
BRIDGE = '''
SIP incoming identity=(1, 1) caller=5551234
SIP epoch changed old=1 new=2
SIP restored call cleared identity=(2, 1)
'''
REMOTE = 'state changed to CONFIRMED\nRequest msg BYE/\n'


class SipRestoreCheckTest(unittest.TestCase):
    def check(self, log=LOG, bridge=BRIDGE, remote=REMOTE, phase='connected'):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'error.log').write_text(log)
            (root / 'sip-bridge.log').write_text(bridge)
            verify_restore(root, remote, phase)
            self.assertTrue(json.loads((root / 'sip-result.json').read_text())['passed'])

    def test_connected_restore_clears_both_sides(self):
        self.check()

    def test_missing_external_clear_is_rejected(self):
        with self.assertRaises(RuntimeError):
            self.check(remote='state changed to CONFIRMED')

    def test_old_epoch_or_wrong_cause_is_rejected(self):
        for log in (LOG.replace('epoch=2', 'epoch=1'),
                    LOG.replace('cause=41', 'cause=16')):
            with self.assertRaises(RuntimeError):
                self.check(log=log)

    def test_replayed_incoming_dialog_is_rejected(self):
        with self.assertRaises(RuntimeError):
            self.check(bridge=BRIDGE + 'SIP incoming identity=(2, 1)\n')

    def test_alerting_restore_requires_rejected_invite(self):
        log = LOG.replace('phase=connected', 'phase=alerting')
        self.check(log=log, remote='Response msg 603/INVITE/\n', phase='alerting')
        with self.assertRaises(RuntimeError):
            self.check(log=log, remote='Response msg 180/INVITE/\n', phase='alerting')

    def test_alerting_restore_rejects_answer_or_connection(self):
        log = LOG.replace('phase=connected', 'phase=alerting')
        for remote in ('Response msg 603/INVITE/\nstate changed to CONFIRMED',):
            with self.assertRaises(RuntimeError):
                self.check(log=log, remote=remote, phase='alerting')
        with self.assertRaises(RuntimeError):
            self.check(log=log + 'GSM service uplink sapi=0 pd=03 message=07\n',
                remote='Response msg 603/INVITE/\n', phase='alerting')
        with self.assertRaises(RuntimeError):
            self.check(log=log, bridge=BRIDGE + 'SIP physical answer\n',
                remote='Response msg 603/INVITE/\n', phase='alerting')


if __name__ == '__main__':
    unittest.main()
