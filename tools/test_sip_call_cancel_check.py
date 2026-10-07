import json
from pathlib import Path
import tempfile
import unittest

from tools.run_sip_handset_gate import verify_cancel


LOG = '''
gsm_call_adapter: incoming state id=1 epoch=1 phase=paging
GSM service downlink kind=9 sapi=0 pd=03 message=05
gsm_call_adapter: incoming state id=1 epoch=1 phase=alerting
gsm_call_adapter: termination id=1 cause=16 result=accepted
GSM service downlink kind=13 sapi=0 pd=03 message=25
GSM service uplink sapi=0 pd=03 message=2a
LAPDm service Channel Release acknowledged nr=2
gsm_call_adapter: incoming state id=1 epoch=1 phase=ended
'''
REMOTE = 'Request msg CANCEL/\nResponse msg 487/INVITE/\n'
COUNTS = dict(uplink=0, downlink=0, pcm_transmitted=0, pcm_received=0)


class SipCancelCheckTest(unittest.TestCase):
    def check(self, log=LOG, remote=REMOTE, counts=None, extra='', product='3210'):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'error.log').write_text(log)
            (root / 'sip-bridge.log').write_text(
                'SIP disconnected status=487 identity=(1, 1)\n'
                + 'SIP bridge ended ' + json.dumps(COUNTS if counts is None else counts)
                + '\n' + extra)
            verify_cancel(root, remote, product)
            return json.loads((root / 'sip-result.json').read_text())

    def test_cancel_completes_without_answer(self):
        self.check()

    def test_3310_scope(self):
        self.assertTrue(self.check(product='3310')['scope'].startswith('3310 HLE'))

    def test_incomplete_radio_release_is_rejected(self):
        for line in ('GSM service downlink kind=13 sapi=0 pd=03 message=25',
                     'LAPDm service Channel Release acknowledged nr=2'):
            with self.assertRaises(RuntimeError):
                self.check(log=LOG.replace(line, ''))

    def test_duplicate_clear_is_rejected(self):
        with self.assertRaises(RuntimeError):
            self.check(log=LOG + 'gsm_call_adapter: termination id=1 cause=16 result=accepted\n')

    def test_missing_cancel_or_release_is_rejected(self):
        for remote in ('', 'Request msg CANCEL/\n'):
            with self.assertRaises(RuntimeError):
                self.check(remote=remote)
        with self.assertRaises(RuntimeError):
            self.check(log=LOG.replace('phase=ended', 'phase=alerting'))

    def test_answer_or_media_is_rejected(self):
        with self.assertRaises(RuntimeError):
            self.check(extra='SIP physical answer')
        with self.assertRaises(RuntimeError):
            self.check(counts={**COUNTS, 'pcm_received': 1})

    def test_reordered_release_is_rejected(self):
        with self.assertRaises(RuntimeError):
            self.check(log='\n'.join(reversed(LOG.splitlines())))


if __name__ == '__main__':
    unittest.main()
