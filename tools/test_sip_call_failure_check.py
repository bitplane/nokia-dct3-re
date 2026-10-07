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
    def check(self, status=486, log=LOG, counts=None, bridge_extra='', remote_extra='', product='3210'):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'error.log').write_text(log)
            (root / 'sip-bridge.log').write_text(
                f'SIP disconnected status={status} identity=(1, 1)\n'
                + 'SIP bridge ended ' + json.dumps(COUNTS if counts is None else counts)
                + '\n' + bridge_extra)
            verify_failure(root, f'SIP/2.0 {status} response\nCall-ID: call-1\nCSeq: 1 INVITE\n\n'
                           + remote_extra, status, product)
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

    def test_3310_failure_requires_its_own_setup(self):
        with self.assertRaises(RuntimeError):
            self.check(product='3310')
        log = LOG.replace('length=15 data=03450401a05e0581551532f4150101',
                          'length=19 data=03450404600200815e0581551532f4a2150101')
        self.assertTrue(self.check(product='3310', log=log)['scope'].startswith('3310 HLE'))

    def check_redial(self, omit_second_release=False, wrong_identity=False, status=486,
                     omit_second_cause=False, duplicate_sip_dialog=False):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            log = bridge = ''
            for request_id in (1, 2):
                log += ('GSM service uplink sapi=0 pd=03 message=05 length=19 '
                        'data=03450404600200815e0581551532f4a2150101\n'
                        f'gsm_call_adapter: request id={request_id} epoch=1 digits=5551234\n'
                        f'outgoing decision consumed id={request_id} outcome={1 if status == 486 else 2}\n')
                if status == 480 and (request_id != 2 or not omit_second_cause):
                    log += f'outgoing termination consumed id={request_id} cause=18\n'
                log += ('GSM service downlink kind=13 sapi=0 pd=03 message=25 length=5\n'
                        'GSM service uplink sapi=0 pd=03 message=2d data=032d\n'
                        'GSM service downlink kind=26 sapi=0 pd=03 message=2a\n')
                if request_id != 2 or not omit_second_release:
                    log += 'LAPDm service Channel Release acknowledged\n'
                log += f'gsm_call_adapter: state id={request_id} epoch=1 phase=ended\n'
                identity = 99 if request_id == 2 and wrong_identity else request_id
                bridge += (f'SIP disconnected status={status} identity=(1, {identity})\n'
                           'SIP bridge ended ' + json.dumps(COUNTS) + '\n')
            (root / 'error.log').write_text(log)
            (root / 'sip-bridge.log').write_text(bridge)
            remote = ''.join(f'SIP/2.0 {status} failure\nCall-ID: call-{1 if duplicate_sip_dialog else i}\n'
                             'CSeq: 1 INVITE\n\n' for i in (1, 2))
            verify_failure(root, remote, status, '3310', calls=2)
            self.assertEqual(json.loads((root / 'sip-result.json').read_text())['completed_calls'], 2)

    def test_redial_requires_both_correlated_complete_releases(self):
        self.check_redial()
        with self.assertRaises(RuntimeError):
            self.check_redial(omit_second_release=True)
        with self.assertRaises(RuntimeError):
            self.check_redial(wrong_identity=True)
        with self.assertRaises(RuntimeError):
            self.check_redial(duplicate_sip_dialog=True)

    def test_unavailable_redial_requires_cause_18_for_each_attempt(self):
        self.check_redial(status=480)
        with self.assertRaises(RuntimeError):
            self.check_redial(status=480, omit_second_cause=True)


if __name__ == '__main__':
    unittest.main()
