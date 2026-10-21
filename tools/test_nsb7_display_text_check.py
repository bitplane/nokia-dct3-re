"""Source-text checks; the composed menu checker has its own input tests."""
import unittest
from unittest.mock import patch

from tools.nsb7_display_text_check import check


def fixture(value=' 600   0 xxx\n 0  0 x xxxx\n   0       0\n    CCCH', time=19.35):
    words = value.encode('utf-16-be').hex()
    return ('nsb7_display_text: font=00341420 pointer=001040e0 '
            f'words={words} t={time} observer_writes=0')


class DisplayTextTest(unittest.TestCase):
    @patch('tools.nsb7_display_text_check.check_trace')
    def test_source_text_and_menu_composition(self, menu):
        check(fixture())
        menu.assert_called_once_with(fixture())

    @patch('tools.nsb7_display_text_check.check_trace')
    def test_rejects_partial_or_wrong_source(self, menu):
        for text in [fixture('CCCH'), fixture().replace('001040e0', '001040e2'),
                     fixture().replace('00341420', '0036043c'), fixture(time=27),
                     fixture().replace('observer_writes=0', 'observer_writes=1')]:
            with self.subTest(text=text), self.assertRaises(ValueError):
                check(text)

    @patch('tools.nsb7_display_text_check.check_trace', side_effect=ValueError('menu failed'))
    def test_rejects_failed_menu_control(self, menu):
        with self.assertRaises(ValueError):
            check(fixture())


if __name__ == '__main__':
    unittest.main()
