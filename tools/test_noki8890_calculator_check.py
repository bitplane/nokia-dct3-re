import unittest
from tools.noki8890_calculator_check import verify


class CalculatorTest(unittest.TestCase):
    def test_missing_input(self):
        with self.assertRaisesRegex(ValueError, 'missing physical'):
            verify('')

    def test_wrong_input_decode(self):
        with self.assertRaisesRegex(ValueError, 'did not decode'):
            verify('8890_application_physical: action=input_1\n8890_keypad_decoded: key=02')

    def test_fixture_error(self):
        with self.assertRaisesRegex(ValueError, 'fixture error'):
            verify('[LUA ERROR]')
