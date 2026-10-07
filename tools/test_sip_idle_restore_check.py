import json
from pathlib import Path
import tempfile
from types import SimpleNamespace
import unittest

from tools.run_sip_handset_gate import verify_success


LOG = '''
sip_state: saved
sip_state: restored
gsm_call_adapter: incoming state id=1 epoch=2 phase=paging
GSM service downlink kind=9 sapi=0 pd=03 message=05
sip_state: physical Answer after idle restoration
GSM service uplink sapi=0 pd=03 message=07 length=2 data=8347
gsm_call_adapter: incoming state id=1 epoch=2 phase=connected
gsm_call_adapter: termination id=1 cause=16 result=accepted
GSM service uplink sapi=0 pd=03 message=2a length=6 data=032a0802e0d1
gsm_call_adapter: incoming state id=1 epoch=2 phase=ended
'''
COUNTS = dict(uplink=100, downlink=100, pcm_transmitted=100, pcm_received=100)
BRIDGE = '''
SIP epoch changed old=1 new=2
SIP idle snapshot accepted epoch=2
SIP incoming identity=(2, 1) caller=5551234
SIP physical answer identity=(2, 1)
SIP confirmed status=200 identity=(2, 1)
'''
REMOTE = 'state changed to CONFIRMED\nDISCONNECTED [reason=200 (OK)]\n'


class SipIdleRestoreCheckTest(unittest.TestCase):
    def check(self, log=LOG, bridge=BRIDGE, counts=None, product='3210'):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'error.log').write_text(log + ''.join(
                f'gsm_call_adapter: media direction=downlink id=1 sequence={i} result=accepted\n'
                for i in range(100)))
            (root / 'sip-bridge.log').write_text(bridge + 'SIP bridge ended '
                + json.dumps(COUNTS if counts is None else counts) + '\n')
            verify_success(root, REMOTE, SimpleNamespace(incoming=True, restore_idle=True, product=product))

    def test_fresh_call_after_idle_restoration(self):
        self.check()

    def test_3410_fresh_call_uses_own_protocol(self):
        log = LOG.replace('data=8347', 'data=8307').replace('data=032a0802e0d1', 'data=036a0802e0d1')
        self.check(log=log, product='3410')

    def test_bridge_call_and_answer_must_use_new_epoch(self):
        for bridge in (BRIDGE.replace('identity=(2, 1)', 'identity=(1, 1)'),
                       BRIDGE.replace('SIP epoch changed old=1 new=2', '')):
            with self.assertRaises(RuntimeError):
                self.check(bridge=bridge)

    def test_missing_restore_or_old_epoch_is_rejected(self):
        for log in (LOG.replace('sip_state: restored', ''),
                    LOG.replace('epoch=2', 'epoch=1')):
            with self.assertRaises(RuntimeError):
                self.check(log=log)

    def test_call_before_restore_is_rejected(self):
        with self.assertRaises(RuntimeError):
            self.check(log=LOG.replace('sip_state: restored', '') + 'sip_state: restored\n')

    def test_duplicate_call_or_missing_media_is_rejected(self):
        with self.assertRaises(RuntimeError):
            self.check(bridge=BRIDGE + 'SIP incoming identity=(2, 2)\n')
        with self.assertRaises(RuntimeError):
            self.check(counts={**COUNTS, 'uplink': 99})


if __name__ == '__main__':
    unittest.main()
