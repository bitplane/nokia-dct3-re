import unittest

from tools.run_sip_handset_gate import verify_downlink_lifecycle


class SipMediaClosureTest(unittest.TestCase):
    def log(self, reason='session_closed', timestamp='37.040000'):
        return ''.join(
            f'gsm_call_adapter: media direction=downlink id=1 sequence={i} result=accepted\n'
            for i in range(100)) + (
            'gsm_call_adapter: state id=1 epoch=1 phase=media_closed t=37.030000\n'
            'LAPDm service Channel Release acknowledged\n'
            'gsm_call_adapter: media direction=downlink id=1 sequence=100 '
            f'result=rejected t=37.040000 reason={reason}\n'
            f'gsm_call_adapter: state id=1 epoch=1 phase=ended t={timestamp}\n')

    def test_ordered_terminal_packet_is_rejected_safely(self):
        verify_downlink_lifecycle(self.log())
        verify_downlink_lifecycle(self.log(timestamp='37.050000'))

    def test_active_and_wrong_request_failures_remain_failures(self):
        for reason in ('media_validation', 'wrong_request', 'unknown'):
            with self.subTest(reason=reason), self.assertRaises(RuntimeError):
                verify_downlink_lifecycle(self.log(reason=reason))

    def test_closure_requires_explicit_boundary_and_completion(self):
        for log in (self.log().replace('phase=media_closed', 'phase=alerting'),
                    self.log().replace('LAPDm service Channel Release acknowledged', ''),
                    self.log().replace('phase=ended', 'phase=connected'),
                    self.log().replace('epoch=1 phase=ended', 'epoch=2 phase=ended')):
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
