import unittest

try:
    from tools.noki8210_incoming_call_check import verify
except ModuleNotFoundError:
    from noki8210_incoming_call_check import verify


class IncomingCheckTest(unittest.TestCase):
    def test_requires_registration(self):
        with self.assertRaisesRegex(ValueError, 'registration release'):
            verify('')

    def test_rejects_fixture_error(self):
        with self.assertRaisesRegex(ValueError, 'fixture error'):
            verify('[LUA ERROR]')


if __name__ == '__main__':
    unittest.main()
