import unittest
from tools.noki6210_outgoing_call_check import verify as outgoing_call
from tools.noki6210_incoming_call_check import verify as incoming_call
from tools.noki6210_outgoing_sms_check import verify as outgoing_sms
from tools.noki6210_incoming_sms_check import verify as incoming_sms


class ServicesTest(unittest.TestCase):
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
