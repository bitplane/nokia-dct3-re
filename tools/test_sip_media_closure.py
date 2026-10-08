import unittest

from tools.run_sip_handset_gate import verify_downlink_lifecycle


class SipMediaClosureTest(unittest.TestCase):
    def log(self, reason='session_closed', timestamp='37.040000'):
        return ''.join(
            f'gsm_call_adapter: media direction=downlink id=1 sequence={i} result=accepted\n'
            for i in range(100)) + (
            'LAPDm service Channel Release acknowledged\n'
            'gsm_call_adapter: media direction=downlink id=1 sequence=100 '
            f'result=rejected t=37.040000 reason={reason}\n'
            f'gsm_call_adapter: state id=1 epoch=1 phase=ended t={timestamp}\n')

    def test_ordered_terminal_packet_is_rejected_safely(self):
        verify_downlink_lifecycle(self.log())

    def test_active_and_wrong_request_failures_remain_failures(self):
        for reason in ('media_validation', 'wrong_request', 'unknown'):
            with self.subTest(reason=reason), self.assertRaises(RuntimeError):
                verify_downlink_lifecycle(self.log(reason=reason))

    def test_closure_requires_radio_release_and_exact_poll(self):
        for log in (self.log(timestamp='37.050000'),
                    self.log().replace('LAPDm service Channel Release acknowledged', ''),
                    self.log().replace('phase=ended', 'phase=connected')):
            with self.subTest(log=log), self.assertRaises(RuntimeError):
                verify_downlink_lifecycle(log)

    def test_sequence_and_post_closure_acceptance_remain_failures(self):
        for log in (self.log().replace('sequence=100', 'sequence=101'),
                    self.log() + 'gsm_call_adapter: media direction=downlink id=1 '
                    'sequence=101 result=accepted\n'):
            with self.subTest(log=log), self.assertRaises(RuntimeError):
                verify_downlink_lifecycle(log)


if __name__ == '__main__':
    unittest.main()
