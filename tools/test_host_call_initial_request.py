import unittest

from tools.run_host_call_adapter_gate import verify_initial_request


REQUEST = dict(type='outgoing_call', request_id=1, epoch=1,
               digits='5551234', decision_pending=True)


class HostCallInitialRequestTest(unittest.TestCase):
    def test_pending_request_is_accepted(self):
        verify_initial_request(REQUEST, '5551234')

    def test_decided_or_missing_pending_flag_is_rejected(self):
        for event in ({**REQUEST, 'decision_pending': False},
                      {key: value for key, value in REQUEST.items() if key != 'decision_pending'}):
            with self.assertRaises(RuntimeError):
                verify_initial_request(event, '5551234')

    def test_wrong_digits_or_identity_is_rejected(self):
        for event in ({**REQUEST, 'digits': '123'}, {**REQUEST, 'request_id': 2}):
            with self.assertRaises(RuntimeError):
                verify_initial_request(event, '5551234')


if __name__ == '__main__':
    unittest.main()
