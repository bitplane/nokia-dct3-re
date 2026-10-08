import unittest
from unittest.mock import patch
from tools.noki6210_outgoing_call_check import verify as outgoing_call
from tools.noki6210_incoming_call_check import verify as incoming_call
from tools.noki6210_outgoing_sms_check import verify as outgoing_sms
from tools.noki6210_incoming_sms_check import verify as incoming_sms


class ServicesTest(unittest.TestCase):
    def test_incoming_coherent_pin_keeps_exact_topology(self):
        from tools import noki6210_incoming_call_check as checker
        with patch.object(checker, 'require_ordered') as ordered, patch.object(checker, 'require_count'):
            checker.verify('', coherent_pin=True)
        patterns = dict(ordered.call_args.args[1])
        traffic = patterns['own traffic configuration']
        release = patterns['own release configuration']
        self.assertRegex('TX packet type=02 payload=24 data=041202000271012fc1000023000000040000000000000000', traffic)
        self.assertRegex('TX packet type=02 payload=24 data=041202001117001a60000023000000140000000100000000', release)
        self.assertNotRegex('TX packet type=02 payload=24 data=040002000271012fc1000001000000040000000000000000', traffic)
        self.assertEqual(len(patterns), len(checker.CHECKPOINTS))

    def test_outgoing_requires_physical_send(self):
        with self.assertRaisesRegex(ValueError, 'physical Send'):
            outgoing_call('')

    def test_outgoing_requires_decoded_send(self):
        with self.assertRaisesRegex(ValueError, 'Send decode'):
            outgoing_call('6210_call_physical: action=send')

    def test_incoming_requires_paging(self):
        with self.assertRaisesRegex(ValueError, 'registration release'):
            incoming_call('')

    def test_sms_requires_physical_text(self):
        with self.assertRaisesRegex(ValueError, 'physical A'):
            outgoing_sms('')

    def test_sms_requires_delivery(self):
        with self.assertRaisesRegex(ValueError, 'registration release'):
            incoming_sms('', b'')

    def test_fixture_error_is_not_success(self):
        for check in (outgoing_call, incoming_call, outgoing_sms):
            with self.assertRaisesRegex(ValueError, 'fixture error'):
                check('[LUA ERROR]')


if __name__ == '__main__':
    unittest.main()
