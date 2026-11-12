import unittest

from tools.rom4_lab_registration_check import check, check_incoming_call

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


if __name__ == '__main__':
    unittest.main()
