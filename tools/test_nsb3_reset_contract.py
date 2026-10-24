import unittest

from tools.nsb3_reset_contract import check_trace, check_windows, inspect


TRACE = '''nsb3_reset_reason_write: value=68 pc=002a8b08 lr=002a794f t=93.0 observer_writes=0
nsb3_reset_event_tail: reason=68 caller=002b16b5 observer_writes=0
nsb3_board_reset_request: data=05 pc=002b4b9e lr=0023ff2f reason=00000068 count=00 inhibit=00 t=94.0 observer_writes=0'''


class Nsb3ResetContractTest(unittest.TestCase):
    def test_ordered_publication(self):
        check_trace(TRACE)

    def test_wrong_owner_reason_order_or_duplicate_fails(self):
        for text in (TRACE.replace('lr=002a794f', 'lr=00240031'),
                     TRACE.replace('reason=68 caller=', 'reason=00 caller='),
                     TRACE.replace('t=93.0', 't=95.0'), TRACE + '\n' + TRACE):
            with self.assertRaises(ValueError):
                check_trace(text)

    def test_donor_or_truncated_image_rejected(self):
        with self.assertRaises(ValueError):
            inspect(bytes(100))

    def test_receive_windows_and_policy_owner(self):
        text = TRACE + '\nnsb3_receive_window: activity=0002 observer_writes=0\nnsb3_receive_window: activity=0000 observer_writes=0\nnsb3_reset_policy_entry: kind=00000001 caller=002b3a65 observer_writes=0'
        check_windows(text)
        for old, new in [('activity=0000', 'activity=0001'),
                         ('caller=002b3a65', 'caller=00207391')]:
            with self.assertRaises(ValueError):
                check_windows(text.replace(old, new))


if __name__ == '__main__':
    unittest.main()
