import unittest

try:
    from tools.noki8210_outgoing_call_check import verify
except ModuleNotFoundError:
    from noki8210_outgoing_call_check import verify


class OutgoingCheckTest(unittest.TestCase):
    def test_requires_physical_send(self):
        with self.assertRaisesRegex(ValueError, 'physical Send'):
            verify('')

    def test_requires_own_send_decode(self):
        with self.assertRaisesRegex(ValueError, 'Send decode'):
            verify('8210_call_physical: action=send')

    def test_rejects_fixture_error(self):
        with self.assertRaisesRegex(ValueError, 'fixture error'):
            verify('[LUA ERROR]')


if __name__ == '__main__':
    unittest.main()
