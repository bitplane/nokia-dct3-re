import unittest
from unittest.mock import patch
from tools.dct3_toolkit_check import display_text_events
from tools.nsb7_toolkit_check import check


def fixture():
    rows=list(display_text_events('nsb7'))
    rows.insert(rows.index('nsb7_toolkit_physical: action=dismiss')+1,
                'nsb7_key_decode: key=19 observer_writes=0')
    return '\n'.join(rows)


class ToolkitTest(unittest.TestCase):
    @patch('tools.nsb7_toolkit_check.check_authentication')
    def test_successful_physical_dismissal(self,authentication):
        text=fixture()
        check(text)
        authentication.assert_called_once_with(text.split('nsb7_toolkit_physical:',1)[0])

    @patch('tools.nsb7_toolkit_check.check_authentication')
    def test_timeout_wrong_key_and_missing_negotiation_rejected(self,authentication):
        for bad in (fixture().replace('810301218002028281030100','810301218002028281030112'),
                    fixture().replace('key=19','key=17'),
                    fixture().replace('sw=9116','sw=9000'),
                    fixture().replace('ins=10','ins=11'),
                    fixture()+'\nheader cla=a0 ins=12 p1=00 p2=00 p3=16'):
            with self.subTest(text=bad),self.assertRaises(ValueError):
                check(bad)

    @patch('tools.nsb7_toolkit_check.check_authentication')
    def test_response_before_physical_input_rejected(self,authentication):
        with self.assertRaises(ValueError):
            check(fixture().replace('nsb7_toolkit_physical: action=dismiss','')+
                  '\nnsb7_toolkit_physical: action=dismiss')

    def test_authentication_is_not_optional(self):
        with self.assertRaises(ValueError):
            check(fixture())


if __name__=='__main__':
    unittest.main()
