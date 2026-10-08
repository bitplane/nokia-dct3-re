import unittest

from tools.radio_outgoing_host_sms_trace_check import verify


GOOD = """
gsm_call_adapter: sms request id=1 epoch=1 recipient=5551234 alphabet=gsm7 octets=2
gsm_call_adapter: sms decision id=2 outcome=1 result=rejected
gsm_call_adapter: sms decision id=1 outcome=0 result=accepted
gsm_call_adapter: sms decision id=1 outcome=0 result=rejected
GSM service downlink kind=18 sapi=3 pd=09 message=01 length=5
gsm_call_adapter: sms state id=1 epoch=1 phase=ended
"""


class HostSmsTraceCheckTest(unittest.TestCase):
    def failure_log(self, outcome):
        code = 1 if outcome == 'rp_error' else 3
        text = GOOD.replace('id=1 outcome=0', f'id=1 outcome={code}')
        if outcome == 'rp_error':
            return text.replace('kind=18', 'kind=19').replace('length=5', 'length=7')
        return text.replace('GSM service downlink kind=18 sapi=3 pd=09 message=01 length=5',
            'TX packet type=1b data=0080015301\n'
            'RX enqueue type=80 data=80000000000000000000017301\n'
            'TX packet type=02 radio_phase=release_channel_change')

    def test_distinct_failure_outcomes(self):
        for outcome in ('rp_error', 'rp_silence'):
            with self.subTest(outcome=outcome):
                verify(self.failure_log(outcome), outcome=outcome)
                with self.assertRaises(ValueError):
                    verify(GOOD, outcome=outcome)
                with self.assertRaises(ValueError):
                    verify(self.failure_log(outcome))

    def test_failure_cannot_include_success_or_wrong_result(self):
        for outcome in ('rp_error', 'rp_silence'):
            with self.subTest(outcome=outcome), self.assertRaises(ValueError):
                verify(self.failure_log(outcome) + '\nGSM service downlink kind=18 sapi=3', outcome=outcome)
        with self.assertRaises(ValueError):
            verify(self.failure_log('rp_silence') + '\nGSM service downlink kind=19 sapi=3', outcome='rp_silence')

    def test_silence_requires_timeout_closure(self):
        for marker in ('data=0080015301', 'data=80000000000000000000017301',
                       'radio_phase=release_channel_change', 'phase=ended'):
            with self.subTest(marker=marker), self.assertRaises(ValueError):
                verify(self.failure_log('rp_silence').replace(marker, 'missing'), outcome='rp_silence')

    def test_unknown_outcome(self):
        with self.assertRaisesRegex(ValueError, 'unknown'):
            verify(GOOD, outcome='guessed')

    def test_single_octet_submission(self):
        verify(GOOD.replace('octets=2', 'octets=1'), octets=1)

    def test_rejects_octet_prefix_match(self):
        with self.assertRaises(ValueError):
            verify(GOOD.replace('octets=2', 'octets=20'))

    def test_rejects_oversize_submission(self):
        with self.assertRaises(ValueError):
            verify(GOOD, octets=141)

    def test_complete(self):
        verify(GOOD)

    def test_missing_decision(self):
        with self.assertRaises(ValueError):
            verify(GOOD.replace("result=accepted", "result=rejected", 1))


if __name__ == "__main__":
    unittest.main()
