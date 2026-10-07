import json
from pathlib import Path
import tempfile
import unittest

from tools.run_sip_handset_gate import verify_failure


LOG = '''
outgoing decision consumed id=1 outcome=1
GSM service uplink sapi=0 pd=03 message=05 length=15 data=03450401a05e0581551532f4150101
GSM service downlink kind=13 sapi=0 pd=03 message=25 length=5
GSM service uplink sapi=0 pd=03 message=2d length=2 data=036d
GSM service downlink kind=26 sapi=0 pd=03 message=2a length=2
LAPDm service Channel Release acknowledged nr=5
'''
COUNTS = {'uplink': 0, 'downlink': 0, 'pcm_transmitted': 0, 'pcm_received': 0, 'dropped': 0}


class SipFailureCheckTest(unittest.TestCase):
    def check(self, status=486, log=LOG, counts=None, bridge_extra='', remote_extra=''):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'error.log').write_text(log)
            (root / 'sip-bridge.log').write_text(
                f'SIP disconnected status={status} identity=(1, 1)\n'
                + 'SIP bridge ended ' + json.dumps(COUNTS if counts is None else counts)
                + '\n' + bridge_extra)
            verify_failure(root, f'SIP/2.0 {status} response\n' + remote_extra, status)
            return json.loads((root / 'sip-result.json').read_text())

    def test_busy_and_unavailable_require_own_release(self):
        self.assertEqual(self.check()['sip_status'], 486)
        self.assertEqual(self.check(status=480,
            log='outgoing termination consumed id=1 cause=18\n' + LOG)['sip_status'], 480)

    def test_unavailable_without_cause_is_rejected(self):
        with self.assertRaises(RuntimeError):
            self.check(status=480)

    def test_false_connection_is_rejected(self):
        for extra in ('state changed to CONFIRMED',):
            with self.assertRaises(RuntimeError):
                self.check(remote_extra=extra)
        with self.assertRaises(RuntimeError):
            self.check(bridge_extra='SIP confirmed status=200')
        with self.assertRaises(RuntimeError):
            self.check(log=LOG + 'GSM service downlink kind=12 sapi=0 pd=03 message=07')

    def test_false_media_and_missing_release_are_rejected(self):
        with self.assertRaises(RuntimeError):
            self.check(counts={**COUNTS, 'pcm_received': 1})
        with self.assertRaises(RuntimeError):
            self.check(log=LOG.replace('LAPDm service Channel Release acknowledged', 'absent'))


if __name__ == '__main__':
    unittest.main()
