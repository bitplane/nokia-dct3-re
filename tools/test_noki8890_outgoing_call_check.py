import unittest
from tools.noki8890_outgoing_call_check import verify


class OutgoingTest(unittest.TestCase):
    def test_requires_physical_send(self):
        with self.assertRaisesRegex(ValueError, 'physical Send'):
            verify('')

    def test_requires_send_decode(self):
        with self.assertRaisesRegex(ValueError, 'Send decode'):
            verify('8890_call_physical: action=send')

    def test_rejects_fixture_error(self):
        with self.assertRaisesRegex(ValueError, 'fixture error'):
            verify('[LUA ERROR]')

    def test_pcs_call_requires_pcs_registration(self):
        with self.assertRaisesRegex(ValueError, 'candidate window'):
            verify('8890_call_physical: action=send', pcs1900=True)
