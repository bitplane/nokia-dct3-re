import unittest
from tools.noki8890_incoming_call_check import verify


class IncomingTest(unittest.TestCase):
    def test_requires_registration(self):
        with self.assertRaisesRegex(ValueError, 'registration release'):
            verify('')

    def test_rejects_fixture_error(self):
        with self.assertRaisesRegex(ValueError, 'fixture error'):
            verify('[LUA ERROR]')

    def test_pcs_requires_pcs_registration(self):
        with self.assertRaisesRegex(ValueError, 'candidate window'):
            verify('', pcs1900=True)
