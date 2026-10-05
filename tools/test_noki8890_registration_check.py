import unittest
from tools.noki8890_registration_check import verify


class RegistrationTest(unittest.TestCase):
    def test_missing_candidate(self):
        with self.assertRaisesRegex(ValueError, 'candidate window'):
            verify('')

    def test_candidate_alone_insufficient(self):
        with self.assertRaisesRegex(ValueError, 'candidate channel'):
            verify('TX packet type=56 payload=160 data=003c')

    def test_fixture_error(self):
        with self.assertRaisesRegex(ValueError, 'fixture error'):
            verify('[LUA ERROR]')
