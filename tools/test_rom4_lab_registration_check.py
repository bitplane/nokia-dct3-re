import unittest

from tools.rom4_lab_registration_check import check

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

if __name__ == '__main__':
    unittest.main()
