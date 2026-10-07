from pathlib import Path
import tempfile
import unittest

from tools.run_sip_handset_gate import verify_outgoing_restore


LOG = '''
gsm_call_adapter: request id=1 epoch=1 digits=5551234
sip_state: saved
sip_state: restored
gsm_call_adapter: request id=1 epoch=2 digits=5551234
termination id=1 cause=41 result=accepted
outgoing termination consumed id=1 cause=41
GSM service downlink kind=13 sapi=0 pd=03 message=25
GSM service uplink sapi=0 pd=03 message=2d
GSM service downlink kind=26 sapi=0 pd=03 message=2a
LAPDm service Channel Release acknowledged
gsm_call_adapter: state id=1 epoch=2 phase=ended
'''
BRIDGE = '''
SIP dial identity=(1, 1) digits=5551234
SIP epoch changed old=1 new=2
SIP restored call cleared identity=(2, 1)
'''
REMOTE = 'Response msg 180/INVITE/\nRequest msg CANCEL/\nResponse msg 487/INVITE/\n'


class SipOutgoingRestoreCheckTest(unittest.TestCase):
    def check(self, log=LOG, bridge=BRIDGE, remote=REMOTE):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'error.log').write_text(log)
            (root / 'sip-bridge.log').write_text(bridge)
            verify_outgoing_restore(root, remote)

    def test_pending_call_clears_without_redial(self):
        self.check()

    def test_missing_real_cancel_is_rejected(self):
        with self.assertRaises(RuntimeError):
            self.check(remote=REMOTE.replace('Request msg CANCEL/', ''))

    def test_false_connection_or_redial_is_rejected(self):
        with self.assertRaises(RuntimeError):
            self.check(bridge=BRIDGE + 'SIP dial identity=(2, 1)\n')
        with self.assertRaises(RuntimeError):
            self.check(remote=REMOTE + 'state changed to CONFIRMED\n')

    def test_missing_rr_release_or_wrong_epoch_is_rejected(self):
        for log in (LOG.replace('LAPDm service Channel Release acknowledged', ''),
                    LOG.replace('epoch=2', 'epoch=1')):
            with self.assertRaises(RuntimeError):
                self.check(log=log)


if __name__ == '__main__':
    unittest.main()
