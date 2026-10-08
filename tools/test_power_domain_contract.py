import unittest
from tools.power_domain_contract import require_endpoint_silence


class EndpointSilenceTest(unittest.TestCase):
    def test_all_known_powered_endpoint_events_rejected(self):
        for event in ('dspif_transport: RX enqueue', 'dspif_transport: FIQ0 notify',
                      'dspif_transport: peer RAM W', 'rom4_timing_port:',
                      'rom4_port_write:', 'staged_dsp: publication',
                      'radio_peer: LAPDm', 'dsp_hle: speech'):
            with self.subTest(event=event), self.assertRaisesRegex(ValueError, '^product-specific reason$'):
                require_endpoint_silence(event, 'product-specific reason')

    def test_retained_rtc_and_observation_are_not_endpoint_activity(self):
        require_endpoint_silence('ccont_rtc: event=second\nphysical: action=restart_press\n', 'error')

    def test_product_checker_owns_wake_and_empty_window_validation(self):
        # The helper must not silently claim these constitute a valid lifecycle.
        require_endpoint_silence('', 'error')
        require_endpoint_silence('ccont_power: event=wake cause=02', 'error')


if __name__ == '__main__':
    unittest.main()
