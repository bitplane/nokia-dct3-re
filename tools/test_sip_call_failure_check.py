import json
from pathlib import Path
import tempfile
import unittest

from tools.run_sip_handset_gate import verify_failure


LOG = '''
GSM service uplink sapi=0 pd=03 message=05 length=15 data=03450401a05e0581551532f4150101
gsm_call_adapter: request id=1 epoch=1 digits=5551234
outgoing decision consumed id=1 outcome=1
GSM service downlink kind=13 sapi=0 pd=03 message=25 length=5
GSM service uplink sapi=0 pd=03 message=2d length=2 data=036d
GSM service downlink kind=26 sapi=0 pd=03 message=2a length=2
LAPDm service Channel Release acknowledged nr=5
gsm_call_adapter: state id=1 epoch=1 phase=ended
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
            log=LOG.replace('outcome=1', 'outcome=2\noutgoing termination consumed id=1 cause=18'))['sip_status'], 480)

    def test_single_call_decision_must_follow_request_and_precede_release(self):
        decision = 'outgoing decision consumed id=1 outcome=1\n'
        without = LOG.replace(decision, '')
        for misplaced in (decision + without, without + decision):
            with self.assertRaises(RuntimeError):
                self.check(log=misplaced)
        with self.assertRaises(RuntimeError):
            self.check(log=LOG.replace('consumed id=1 outcome=1', 'consumed id=2 outcome=1'))

    def test_single_unavailable_requires_ordered_correlated_cause(self):
        cause = 'outgoing termination consumed id=1 cause=18\n'
        without = LOG.replace('outcome=1', 'outcome=2')
        for misplaced in (cause + without, without + cause):
            with self.assertRaises(RuntimeError):
                self.check(status=480, log=misplaced)
        ordered = without.replace('outcome=2\n', 'outcome=2\n' + cause)
        with self.assertRaises(RuntimeError):
            self.check(status=480, log=ordered.replace('termination consumed id=1', 'termination consumed id=2'))

    def test_unavailable_without_cause_is_rejected(self):
        with self.assertRaises(RuntimeError):
            self.check(status=480)

    def test_numeric_prefixes_do_not_satisfy_failure_contract(self):
        with self.assertRaises(RuntimeError):
            self.check(log=LOG.replace('outcome=1', 'outcome=10'))
        with self.assertRaises(RuntimeError):
            self.check(status=480, log='outgoing termination consumed id=1 cause=180\n' + LOG)

    def test_forbidden_requires_ordered_rejection_cause(self):
        log = LOG.replace('outgoing decision consumed id=1 outcome=1',
                          'outgoing decision consumed id=1 outcome=2\n'
                          'outgoing termination consumed id=1 cause=21')
        self.assertEqual(self.check(status=403, log=log)['sip_status'], 403)
        for invalid in (log.replace('cause=21', 'cause=18'),
                        log.replace('outcome=2', 'outcome=1'),
                        log.replace('outgoing termination consumed id=1 cause=21', '')):
            with self.assertRaises(RuntimeError):
                self.check(status=403, log=invalid)

    def test_missing_destination_and_service_failure_require_exact_cause(self):
        for status, cause in ((404, 1), (408, 102), (503, 41)):
            with self.subTest(status=status):
                log = LOG.replace('outgoing decision consumed id=1 outcome=1',
                                  'outgoing decision consumed id=1 outcome=2\n'
                                  f'outgoing termination consumed id=1 cause={cause}')
                self.assertEqual(self.check(status=status, log=log)['sip_status'], status)
                for wrong in (18, cause * 10):
                    with self.assertRaises(RuntimeError):
                        self.check(status=status, log=log.replace(f'cause={cause}', f'cause={wrong}'))

    def test_global_busy_requires_busy_decision(self):
        self.assertEqual(self.check(status=600)['sip_status'], 600)
        with self.assertRaises(RuntimeError):
            self.check(status=600, log=LOG.replace('outcome=1', 'outcome=2'))

    def test_unhandled_attempt_and_missing_ended_state_are_rejected(self):
        for log in (LOG + 'gsm_call_adapter: request id=2 epoch=1 digits=5551234\n',
                    LOG.replace('phase=ended', 'phase=disconnecting'),
                    LOG + 'gsm_call_adapter: media direction=downlink id=1 epoch=1 result=accepted\n'):
            with self.assertRaises(RuntimeError):
                self.check(log=log, product='3410')

    def test_3410_failure_scope(self):
        self.assertTrue(self.check(product='3410')['scope'].startswith('3410 HLE'))
        self.assertTrue(self.check(product='5210')['scope'].startswith('5210 HLE'))

    def test_unhandled_setup_is_rejected_even_without_host_request(self):
        # A valid first dialog must not hide another firmware-side attempt.
        extra = LOG.splitlines()[1].replace('551532f4', '214365f7')
        for product in ('3210', '3410', '5210'):
            with self.subTest(product=product), self.assertRaisesRegex(
                    RuntimeError, 'SETUP differs from physically dialed number'):
                self.check(product=product, log=LOG + extra + '\n')

    def test_6210_failure_requires_own_physical_number_and_setup(self):
        log = LOG.replace('digits=5551234', 'digits=1234567').replace('551532f4', '214365f7')
        self.assertTrue(self.check(product='6210', log=log)['scope'].startswith('6210 HLE'))
        for invalid in (LOG, log.replace('214365f7', '551532f4'),
                        log.replace('length=15', 'length=16')):
            with self.assertRaises(RuntimeError):
                self.check(product='6210', log=invalid)

    def test_3330_failure_requires_own_setup(self):
        with self.assertRaises(RuntimeError):
            self.check(product='3330')
        log = LOG.replace('length=15 data=03450401a05e0581551532f4150101',
                          'length=18 data=03450404600200815e0581551532f4150101')
        self.assertTrue(self.check(product='3330', log=log)['scope'].startswith('3330 HLE'))

    def test_6210_unavailable_requires_cause_18(self):
        log = LOG.replace('digits=5551234', 'digits=1234567').replace('551532f4', '214365f7')
        log = log.replace('outcome=1', 'outcome=2\noutgoing termination consumed id=1 cause=18')
        self.assertEqual(self.check(product='6210', status=480, log=log)['sip_status'], 480)
        with self.assertRaises(RuntimeError):
            self.check(product='6210', status=480, log=log.replace('cause=18', 'cause=180'))

    def test_6250_busy_requires_own_three_digit_setup(self):
        log = LOG.replace('digits=5551234', 'digits=123').replace(
            'length=15 data=03450401a05e0581551532f4150101',
            'length=13 data=03450401a05e038121f3150101')
        self.assertEqual(self.check(product='6250', log=log)['sip_status'], 486)
        unavailable = log.replace('outcome=1', 'outcome=2\noutgoing termination consumed id=1 cause=18')
        self.assertEqual(self.check(product='6250', status=480, log=unavailable)['sip_status'], 480)
        with self.assertRaises(RuntimeError):
            self.check(product='6250', status=480, log=unavailable.replace('cause=18', 'cause=21'))
        with self.assertRaises(RuntimeError):
            self.check(product='6250')

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
