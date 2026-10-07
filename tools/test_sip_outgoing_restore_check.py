from pathlib import Path
import tempfile
import unittest

from tools.run_sip_handset_gate import verify_outgoing_restore


LOG = '''
GSM service uplink sapi=0 pd=03 message=05 length=15 data=03450401a05e0581551532f4150101
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
    def check(self, log=LOG, bridge=BRIDGE, remote=REMOTE, connected=False, product='3210'):
        if product == '3310':
            log = log.replace('length=15 data=03450401a05e0581551532f4150101',
                              'length=19 data=03450404600200815e0581551532f4a2150101')
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'error.log').write_text(log)
            (root / 'sip-bridge.log').write_text(bridge)
            verify_outgoing_restore(root, remote, connected, product)
            return (root / 'sip-result.json').read_text()

    def test_product_scope(self):
        self.assertIn('3310 pending outgoing', self.check(product='3310'))
        self.assertIn('3410 pending outgoing', self.check(product='3410'))

    def test_missing_or_malformed_product_setup_is_rejected(self):
        for log in (LOG.replace('message=05', 'message=06'),
                    LOG.replace('03450401a05e0581551532f4150101',
                                '03450401a05e0581551532f415010100')):
            with self.assertRaises(RuntimeError):
                self.check(log=log, product='3410')

    def test_duplicate_accepted_clear_is_rejected(self):
        with self.assertRaises(RuntimeError):
            self.check(log=LOG + 'termination id=1 cause=41 result=accepted\n')

    def test_pending_call_clears_without_redial(self):
        self.check()

    def test_connected_call_requires_connection_before_save(self):
        log = LOG.replace('sip_state: saved',
            'gsm_call_adapter: state id=1 epoch=1 phase=connected\nsip_state: saved')
        log = log.replace('termination id=1 cause=41 result=accepted\noutgoing termination consumed id=1 cause=41',
            'outgoing termination consumed id=1 cause=41\ntermination id=1 cause=41 result=accepted')
        remote = 'state changed to CONFIRMED\nRequest msg BYE/\n'
        self.check(log=log, remote=remote, connected=True)
        with self.assertRaises(RuntimeError):
            self.check(remote=remote, connected=True)
        with self.assertRaises(RuntimeError):
            self.check(log=log, remote='state changed to CONFIRMED\n', connected=True)

    def test_missing_real_cancel_is_rejected(self):
        with self.assertRaises(RuntimeError):
            self.check(remote=REMOTE.replace('Request msg CANCEL/', ''))

    def test_reordered_sip_release_is_rejected(self):
        with self.assertRaises(RuntimeError):
            self.check(remote='\n'.join(reversed(REMOTE.splitlines())))

    def test_false_connection_or_redial_is_rejected(self):
        with self.assertRaises(RuntimeError):
            self.check(bridge=BRIDGE + 'SIP dial identity=(2, 1)\n')
        with self.assertRaises(RuntimeError):
            self.check(remote=REMOTE + 'state changed to CONFIRMED\n')
        with self.assertRaises(RuntimeError):
            self.check(log=LOG + 'termination id=1 cause=41 result=rejected\n')

    def test_missing_rr_release_or_wrong_epoch_is_rejected(self):
        for log in (LOG.replace('LAPDm service Channel Release acknowledged', ''),
                    LOG.replace('epoch=2', 'epoch=1')):
            with self.assertRaises(RuntimeError):
                self.check(log=log)


if __name__ == '__main__':
    unittest.main()
