import unittest

try:
    from tools.noki8210_incoming_sms_check import verify
except ModuleNotFoundError:
    from noki8210_incoming_sms_check import verify


class IncomingSmsTest(unittest.TestCase):
    def test_rejects_empty_evidence(self):
        with self.assertRaises(ValueError):
            verify('', b'')

    def test_rejects_fixture_error(self):
        with self.assertRaisesRegex(ValueError, 'fixture error'):
            verify('[LUA ERROR]', b'')


if __name__ == '__main__':
    unittest.main()
