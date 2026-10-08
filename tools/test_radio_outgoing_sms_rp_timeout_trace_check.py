import unittest
from tools.radio_outgoing_sms_rp_timeout_trace_check import verify, verify_state


LOG = '''
GSM service establish sapi=0 pd=05 message=24 length=16 data=052474
TX packet type=1b data=00800d3f01
TX packet type=1b data=00800d0053290119000100069121436587090e110007815515
GSM service uplink sapi=3 pd=09 message=01 length=28 data=290119000100069121436587090e11000781551532f40000a702c824
gsm_sms_submit: cp=29 rp=01 smsc=1234567890 destination=5551234 alphabet=0 user_length=2 outcome=3 status_report=0 t=34.7
GSM service downlink kind=17 sapi=3 pd=09 message=04 length=2
TX pending type=1b payload=25 data=00800153012b t=102.2
RX enqueue type=80 data=80120000568e00010000017301
TX packet type=02 radio_phase=release_channel_change data=041202000000001a600000010000000f00000000
PCH no-identity fill
'''


class RpTimeoutTest(unittest.TestCase):
    def test_observed_lifecycle(self):
        verify(LOG)

    def test_missing_or_wrong_order_clearing(self):
        for marker in ('0080015301', '017301', 'release_channel_change', 'PCH no-identity fill'):
            with self.subTest(marker=marker), self.assertRaises(ValueError):
                verify(LOG.replace(marker, 'missing'))
        with self.assertRaises(ValueError):
            verify('PCH no-identity fill\n' + LOG.replace('PCH no-identity fill', ''))

    def test_wrong_outcome_result_or_timing(self):
        for text in (LOG.replace('outcome=3', 'outcome=2'),
                     LOG + 'GSM service downlink kind=18 sapi=3',
                     LOG.replace('t=102.2', 't=40.0'),
                     LOG + 'gsm_sms_submit: duplicate',
                     LOG + 'GSM service downlink kind=17 sapi=3 pd=09 message=04 length=2'):
            with self.subTest(text=text), self.assertRaises(ValueError):
                verify(text)

    def test_missing_failure_pixels(self):
        import tempfile
        with tempfile.TemporaryDirectory() as directory, self.assertRaises(ValueError):
            verify(LOG, directory)

    def state_log(self):
        prefix, clearing = LOG.split('TX pending type=1b', 1)
        prefix = prefix.replace('message=04 length=2\n', 'message=04 length=2 t=34.71\n')
        clearing = 'TX pending type=1b' + clearing
        clearing = clearing.replace('RX enqueue', 'dspif_transport: RX enqueue')
        clearing = clearing.replace('TX packet type=02', 'dsp_hle: TX packet type=02')
        return (prefix + 'state_replay: phase=reference event=begin t=40.0\n' + clearing +
                'state_replay: phase=reference event=end t=105.0\n'
                'state_replay: phase=restored event=begin t=40.01\n'
                'state_roundtrip: result=pass timer_delta=0001 requested_at=40.0 t=40.01\n' +
                clearing + 'state_replay: phase=restored event=end t=105.01\n')

    def test_state_replays_timeout_clearing(self):
        verify_state(self.state_log())

    def test_state_requires_pending_wait_and_clearing_in_replay(self):
        for bad in (self.state_log().replace('requested_at=40.0', 'requested_at=30.0'),
                    self.state_log().replace('restored event=end t=105.01', 'restored event=end t=100.0'),
                    self.state_log().replace('result=pass', 'result=fail')):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                verify_state(bad)

    def test_state_rejects_changed_release_payload(self):
        text = self.state_log()
        offset = text.index('phase=restored event=begin')
        text = text[:offset] + text[offset:].replace('000f00000000', '000e00000000')
        with self.assertRaises(ValueError):
            verify_state(text)
