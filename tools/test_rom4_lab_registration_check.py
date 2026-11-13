import unittest

from tools.rom4_lab_registration_check import (check, check_host_call, check_host_lab, check_incoming_call, check_roundtrip,
                                                check_traffic_call)

# Dialogue lines from a native nse1ap5 run (frames 3028-3337).
LOG = '''native_dsp_reset_line: released=1
native_dsp_reset_line: released=0
native_dsp_reset_line: released=1
npe3_raw_agch: ra=0d access_frame=3028 start_frame=3066 octets=2d063f002000230d126c00002b2b2b2b2b2b2b2b2b2b2b raw_adc_only=1 tuned_air_source=0 physical_claim=0
npe3_bts_uplink: start_frame=3097 octets=013f4905087000f000fffe230809101010325476982b2b exact_code=1 port_bits_only=1 physical_claim=0
npe3_raw_ua: uplink_start_frame=3097 start_frame=3133 octets=01734905087000f000fffe230809101010325476982b2b raw_adc_only=1 physical_claim=0
npe3_raw_lu_accept: uplink_start_frame=3097 start_frame=3184 octets=030045050200f1100001170809101010325476982b2b2b policy=plain_test_cell raw_adc_only=1 physical_claim=0
npe3_bts_uplink: start_frame=3148 octets=0103012b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b exact_code=1 port_bits_only=1 physical_claim=0
npe3_bts_uplink: start_frame=3199 octets=0321012b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b exact_code=1 port_bits_only=1 physical_claim=0
npe3_raw_release: ack_start_frame=3199 start_frame=3235 octets=03020d060d002b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b raw_adc_only=1 physical_claim=0
npe3_bts_uplink: start_frame=3250 octets=0341012b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b exact_code=1 port_bits_only=1 physical_claim=0
npe3_bts_uplink: start_frame=3301 octets=0153012b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b exact_code=1 port_bits_only=1 physical_claim=0
npe3_raw_disc_ua: disc_start_frame=3301 start_frame=3337 octets=0173012b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b raw_adc_only=1 physical_claim=0
'''

# Incoming call after registration (page, answer, end) from a native nse1ap5 run.
CALL = '''npe3_bts_page_request: t=20.016666667 identity=0910101032547698 phase=1 offset=6 multiframes=2 after_frame=4337 octets=00
npe3_bts_page_access: t=20.567900365 ra=17 last_page_frame=4341 port_bits_only=1 physical_claim=0
npe3_raw_agch: ra=17 access_frame=4456 start_frame=4494 octets=2d063f00200023171a6a00002b2b2b2b2b2b2b2b2b2b2b raw_adc_only=1 tuned_air_source=0 physical_claim=0
npe3_bts_uplink: start_frame=4525 octets=013f41062707032318010809101010325476982b2b2b2b exact_code=1 port_bits_only=1 physical_claim=0
npe3_raw_ua: uplink_start_frame=4525 start_frame=4561 octets=017341062707032318010809101010325476982b2b2b2b raw_adc_only=1 physical_claim=0
npe3_raw_call_setup: uplink_start_frame=4525 start_frame=4612 octets=030045030504046002008134015c0581551532f42b2b2b policy=plain_test_cell raw_adc_only=1 physical_claim=0
npe3_bts_uplink: start_frame=4627 octets=0321012b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b exact_code=1 port_bits_only=1 physical_claim=0
npe3_bts_uplink: start_frame=4678 octets=01202d83080404600200811501012b2b2b2b2b2b2b2b2b exact_code=1 port_bits_only=1 physical_claim=0
npe3_raw_call_rr: uplink_start_frame=4678 start_frame=4714 octets=0321012b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b raw_adc_only=1 physical_claim=0
npe3_bts_uplink: start_frame=4729 octets=01220983412b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b exact_code=1 port_bits_only=1 physical_claim=0
npe3_raw_call_rr: uplink_start_frame=4729 start_frame=4765 octets=0341012b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b raw_adc_only=1 physical_claim=0
npe3_bts_uplink: start_frame=5035 octets=01240983072b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b exact_code=1 port_bits_only=1 physical_claim=0
npe3_raw_call_rr: uplink_start_frame=5035 start_frame=5071 octets=0361012b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b raw_adc_only=1 physical_claim=0
npe3_bts_uplink: start_frame=5953 octets=012615836502e0902b2b2b2b2b2b2b2b2b2b2b2b2b2b2b exact_code=1 port_bits_only=1 physical_claim=0
npe3_raw_cc_release: disconnect_start_frame=5953 start_frame=5989 octets=038209032d2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b raw_adc_only=1 physical_claim=0
npe3_bts_uplink: start_frame=6004 octets=0341012b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b exact_code=1 port_bits_only=1 physical_claim=0
npe3_bts_uplink: start_frame=6055 octets=014809832a2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b exact_code=1 port_bits_only=1 physical_claim=0
npe3_raw_call_channel_release: complete_start_frame=6055 start_frame=6091 octets=03a40d060d002b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b raw_adc_only=1 physical_claim=0
npe3_bts_uplink: start_frame=6106 octets=0361012b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b exact_code=1 port_bits_only=1 physical_claim=0
npe3_bts_uplink: start_frame=6157 octets=0153012b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b exact_code=1 port_bits_only=1 physical_claim=0
npe3_raw_disc_ua: disc_start_frame=6157 start_frame=6193 octets=0173012b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b raw_adc_only=1 physical_claim=0
'''

# The same call assigned to TCH/F timeslot 1 (FACCH/SACCH lines only).
TRAFFIC = '''npe3_raw_assignment: confirmed_start_frame=4678 start_frame=4714 octets=032221062e0940230063012b2b2b2b2b2b2b2b2b2b2b2b
npe3_tch_start: first_frame=4719 timeslot=1 tsc=2 sacch=si5,si6 speech=empty codec=gsm_06_10
npe3_tch_uplink: channel=facch start_frame=4732 octets=013f012b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b exact_code=1
npe3_tch_facch: kind=ua uplink_start_frame=4732 octets=0173012b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b
npe3_tch_uplink: channel=facch start_frame=4771 octets=013f012b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b exact_code=1
npe3_tch_facch: kind=ua uplink_start_frame=4771 octets=0173012b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b
npe3_tch_uplink: channel=facch start_frame=4792 octets=01000d0629002b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b exact_code=1
npe3_tch_facch: kind=rr uplink_start_frame=4792 octets=0121012b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b
npe3_tch_uplink: channel=facch start_frame=4827 octets=01100d0629002b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b exact_code=1
npe3_tch_facch: kind=duplicate_rr uplink_start_frame=4827 octets=0131012b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b
npe3_tch_uplink: channel=facch start_frame=4844 octets=01020983412b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b exact_code=1
npe3_tch_facch: kind=rr uplink_start_frame=4844 octets=0141012b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b
npe3_tch_uplink: channel=sacch start_frame=4809 octets=050001034906153f3f7fc0000000000000000000000000 exact_code=1
npe3_tch_uplink: channel=sacch start_frame=4913 octets=050001034906157f3f6bc0000000000000000000000000 exact_code=1
npe3_tch_uplink: channel=facch start_frame=5000 octets=01040983072b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b exact_code=1
npe3_tch_facch: kind=connect_acknowledge uplink_start_frame=5000 octets=036009030f2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b
npe3_tch_uplink: channel=facch start_frame=5039 octets=01140983072b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b exact_code=1
npe3_tch_facch: kind=duplicate_rr uplink_start_frame=5039 octets=0171012b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b
npe3_tch_uplink: channel=sacch start_frame=5017 octets=050001034906157f3f7fc0000000000000000000000000 exact_code=1
npe3_tch_uplink: channel=facch start_frame=5091 octets=0321012b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b exact_code=1
npe3_tch_uplink: channel=sacch start_frame=5121 octets=050001034906157f3f7bc0000000000000000000000000 exact_code=1
npe3_tch_uplink: channel=sacch start_frame=5225 octets=050001034906157f3f7fc0000000000000000000000000 exact_code=1
npe3_tch_uplink: channel=facch start_frame=5915 octets=012615836502e0902b2b2b2b2b2b2b2b2b2b2b2b2b2b2b exact_code=1
npe3_tch_facch: kind=cc_release uplink_start_frame=5915 octets=038209032d2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b
npe3_tch_uplink: channel=facch start_frame=5936 octets=014809832a2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b exact_code=1
npe3_tch_facch: kind=channel_release uplink_start_frame=5936 octets=03a40d060d002b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b
npe3_tch_uplink: channel=facch start_frame=5971 octets=015809832a2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b exact_code=1
npe3_tch_facch: kind=duplicate_rr uplink_start_frame=5971 octets=01b1012b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b
npe3_tch_uplink: channel=facch start_frame=5988 octets=0153012b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b exact_code=1
npe3_tch_facch: kind=disc_ua uplink_start_frame=5988 octets=0173012b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b
'''
# The same call carrying the lab's tones both ways.
SPEECH = TRAFFIC.replace('speech=empty', 'speech=tone_1000hz') + (
    'npe3_tch_audio: earpiece_hz=1000 earpiece_blocks=307 earpiece_tone_blocks=206 earpiece_tone_amplitude=325 '
    'microphone_hz=500 uplink_blocks=230 uplink_tone_blocks=202 uplink_tone_amplitude=23552 physical_claim=0\n')
ROUNDTRIP = ('[luaengine] state_roundtrip: result=pass timer_delta=0000 mode=0000 requested_at=24.500000 t=24.500000\n'
             '[luaengine] rom4_codec_replay: result=pass reference_events=4308 restored_events=4308 window=0.16\n')


class LabRegistrationCheckTest(unittest.TestCase):
    def test_native_dialogue_passes(self):
        result = check(LOG)
        self.assertEqual((result['access_frame'], result['accept_frame'], result['closing_ua_frame']),
                         (3028, 3184, 3337))
        self.assertEqual(result['uplinks'], 5)

    def test_missing_or_corrupt_steps_fail(self):
        for old, new in (
                ('npe3_raw_lu_accept:', 'dropped:'),
                ('ra=0d', 'ra=8d'),  # answered page, not location updating
                ('013f4905087000', '013f4905017000'),  # SABM without LU Request
                ('start_frame=3301 octets=0153', 'start_frame=3301 octets=0103'),
                ('exact_code=1', 'exact_code=0'),
                ('start_frame=3235', 'start_frame=3190'),
        ):
            with self.subTest(old=old):
                with self.assertRaises(ValueError):
                    check(LOG.replace(old, new, 1))

    def test_reset_fails(self):
        for extra in ('rom4_reset_request: reason=68\n', 'native_dsp_reset_line: released=0\n'):
            with self.subTest(extra=extra), self.assertRaises(ValueError):
                check(LOG + extra)

    def test_incoming_call_passes(self):
        result = check_incoming_call(LOG + CALL)
        self.assertEqual((result['response_frame'], result['connect_frame'], result['call_closing_ua_frame']),
                         (4525, 5035, 6193))

    def test_incoming_call_steps_are_required(self):
        for old, new in (
                ('ra=17 last', 'ra=0d last'),  # access cause not answering the page
                ('identity=0910101032547698', 'identity=0910101032547699'),
                ('01240983072b', '01240983012b'),  # CONNECT never sent
                ('npe3_raw_cc_release:', 'dropped:'),
                ('disconnect_start_frame=5953', 'disconnect_start_frame=5954'),
        ):
            with self.subTest(old=old), self.assertRaises(ValueError):
                check_incoming_call(LOG + CALL.replace(old, new, 1))
        with self.assertRaises(ValueError):
            check_incoming_call(LOG + CALL + 'native_dsp_reset_line: released=0\n')


    def test_traffic_call_passes(self):
        result = check_traffic_call(TRAFFIC)
        self.assertEqual((result['tch_sabm_frame'], result['tch_connect_frame'], result['tch_disc_frame']),
                         (4732, 5000, 5988))
        self.assertEqual(result['measurement_reports'], 5)

    def test_traffic_call_steps_are_required(self):
        for old, new in (
                ('first_frame=4719', 'first_frame=4716'),  # before the Assignment Command ends
                ('0d0629002b', '0d0628002b'),  # no Assignment Complete, nor its retransmission
                ('kind=connect_acknowledge uplink_start_frame=5000', 'kind=connect_acknowledge uplink_start_frame=5039'),
                ('kind=cc_release', 'kind=rr'),
                ('start_frame=5936 octets=014809832a', 'start_frame=5936 octets=014809832b'),
                ('channel=facch start_frame=5988 octets=0153012b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b exact_code=1',
                 'channel=facch start_frame=5988 octets=0153012b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b exact_code=0'),
                ('channel=sacch start_frame=5', 'channel=sacch start_frame=x'),  # two reports left
        ):
            with self.subTest(old=old), self.assertRaises(ValueError):
                check_traffic_call(TRAFFIC.replace(old, new))
        with self.assertRaises(ValueError):
            check_traffic_call(TRAFFIC + 'native_dsp_reset_line: released=0\n')

    def test_traffic_speech_passes(self):
        result = check_traffic_call(SPEECH)
        self.assertEqual((result['earpiece_tone_blocks'], result['uplink_tone_blocks']), (206, 202))

    def test_traffic_speech_needs_both_tones(self):
        for old, new in (('earpiece_tone_blocks=206', 'earpiece_tone_blocks=100'),
                         ('uplink_tone_blocks=202', 'uplink_tone_blocks=40'),
                         ('npe3_tch_audio:', 'dropped:')):
            with self.subTest(old=old), self.assertRaises(ValueError):
                check_traffic_call(SPEECH.replace(old, new))

    def test_roundtrip_needs_exact_replay(self):
        self.assertEqual(check_roundtrip(ROUNDTRIP), {'state_roundtrip': 'pass'})
        for old, new in (('state_roundtrip: result=pass', 'state_roundtrip: result=fail'),
                         ('rom4_codec_replay: result=pass', 'rom4_codec_replay: result=fail')):
            with self.subTest(old=old), self.assertRaises(ValueError):
                check_roundtrip(ROUNDTRIP.replace(old, new))


if __name__ == '__main__':
    unittest.main()


# A host-placed call cleared by the host (native nse1ap5 run, hangup after 150 frames).
HOST_LOG = """npe3_host_call: request=1 epoch=2 phase=paged digits=4 t=16.130000000
npe3_host_call: request=1 epoch=2 phase=alerting t=18.994631730
npe3_host_call: request=1 epoch=2 phase=connected t=23.116170192
npe3_host_call: request=1 epoch=2 phase=network_clearing cause=16 t=26.140000000
npe3_tch_facch: kind=cc_disconnect uplink_start_frame=5663 octets=036215032502e0902b2b2b2b2b2b2b2b2b2b2b2b2b2b2b raw_adc_only=1 physical_claim=0
npe3_tch_uplink: channel=facch start_frame=5681 octets=014609836d2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b exact_code=1 speech_blocks=366 bad_speech_blocks=4 unreadable_bursts=0 downlink_speech_blocks=372 port_bits_only=1 physical_claim=0
npe3_tch_facch: kind=cc_release_complete uplink_start_frame=5681 octets=038409032a2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b raw_adc_only=1 physical_claim=0
npe3_tch_uplink: channel=facch start_frame=5698 octets=0361012b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b exact_code=1 speech_blocks=369 bad_speech_blocks=4 unreadable_bursts=0 downlink_speech_blocks=375 port_bits_only=1 physical_claim=0
npe3_tch_facch: kind=channel_release uplink_start_frame=5698 octets=03860d060d002b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b raw_adc_only=1 physical_claim=0
npe3_tch_uplink: channel=facch start_frame=5720 octets=0153012b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b exact_code=1 speech_blocks=373 bad_speech_blocks=4 unreadable_bursts=0 downlink_speech_blocks=379 port_bits_only=1 physical_claim=0
npe3_tch_facch: kind=disc_ua uplink_start_frame=5720 octets=0173012b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b raw_adc_only=1 physical_claim=0
npe3_host_call: request=1 epoch=2 phase=ended t=26.677469538
npe3_tch_audio: earpiece_hz=1000 earpiece_blocks=390 earpiece_tone_blocks=151 earpiece_tone_amplitude=862 microphone_hz=500 uplink_blocks=378 uplink_tone_blocks=148 uplink_tone_amplitude=24049 physical_claim=0
"""
HOST_CLIENT = dict(phases=['alerting', 'connected', 'ended'], uplink_good=166, downlink=167,
                   uplink_microphone_fraction=0.801)


class HostCallTest(unittest.TestCase):
    def test_accepts_native_host_call(self):
        result = check_host_call(HOST_LOG, HOST_CLIENT)
        self.assertEqual(result['earpiece_tone_blocks'], 151)
        self.assertEqual(result['host_uplink_frames'], 166)

    def test_rejects_unfinished_clearing(self):
        with self.assertRaisesRegex(ValueError, 'phases out of order'):
            check_host_lab(HOST_LOG.replace('phase=ended', 'phase=stuck'))

    def test_rejects_missing_handset_release(self):
        broken = '\n'.join(line for line in HOST_LOG.splitlines() if '014609836d' not in line)
        with self.assertRaisesRegex(ValueError, 'clearing out of order'):
            check_host_lab(broken)

    def test_rejects_short_earpiece_tone(self):
        with self.assertRaisesRegex(ValueError, 'earpiece'):
            check_host_lab(HOST_LOG.replace('earpiece_tone_blocks=151', 'earpiece_tone_blocks=99'))

    def test_rejects_quiet_host_uplink(self):
        with self.assertRaisesRegex(ValueError, 'microphone'):
            check_host_call(HOST_LOG, dict(HOST_CLIENT, uplink_microphone_fraction=0.2))
